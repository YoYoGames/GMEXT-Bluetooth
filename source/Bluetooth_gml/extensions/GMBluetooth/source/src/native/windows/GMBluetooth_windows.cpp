// The Windows backend: its lifecycle, feature and permission answers and
// the device queries. The roles live beside it: adapter and LE scan,
// Classic, LE client and GATT server with advertising.

#include "GMBluetooth_windows_internal.h"

#if defined(_WIN32)

namespace gmbluetooth::win
{
    namespace
    {
        // Worker threads can outlive a shutdown that stopped waiting for
        // them; pinning keeps the code they return into mapped until the
        // process exits, whatever the runner does with the DLL.
        void pin_module()
        {
            static const char anchor = 0;
            HMODULE module = nullptr;
            if (!GetModuleHandleExW(
                    GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                    reinterpret_cast<LPCWSTR>(&anchor),
                    &module))
            {
                GMBT_LOG("Could not pin the extension module: error %lu", GetLastError());
            }
        }
    }

    Error WindowsBackend::initialize(std::string& message)
    {
        if (initialized_)
            return Error::Ok;

        pin_module();

        // Nothing here touches the game thread's COM apartment (R1-104):
        // WinRT waits run on le_worker_, whose MTA also serves the game
        // thread's own non-blocking WinRT calls.
        WSADATA wsa{};
        const int wsa_result = WSAStartup(MAKEWORD(2, 2), &wsa);
        if (wsa_result != 0)
        {
            message = wsa_message("WSAStartup", wsa_result);
            return Error::OperationFailed;
        }

        le_worker_ = SerialWorker::start(workers_);
        if (!le_worker_)
        {
            WSACleanup();
            message = "Could not start the Bluetooth worker thread";
            return Error::OperationFailed;
        }
        winsock_initialized_ = true;
        le_client_->worker = le_worker_;

        // Classic is known from the Win32 stack at once; the adapter
        // query refines it.
        le_->classic_supported.store(has_classic_radio());

        // The adapter query waits on WinRT, so it runs on the worker
        // (R1-103). bluetooth_initialize is synchronous and games read
        // the capabilities right after it, so the query gets a few
        // seconds; an answer later than that still sets them and reports
        // the state.
        auto waiting = std::make_shared<std::atomic_bool>(true);
        auto answered = std::make_shared<std::promise<void>>();
        std::future<void> answer = answered->get_future();
        // The adapter watcher starts after the first answer, so a later
        // adapter or radio change reports too (R1-229).
        const auto le = le_;
        const std::weak_ptr<SerialWorker> worker = le_worker_;
        le_worker_->post([le, worker, waiting, answered]()
        {
            le->connection_parameters_api.store(connection_parameters_api_present());
            query_adapter(le);
            answered->set_value();
            if (!waiting->exchange(false))
                push_bluetooth_state(le);
            start_adapter_watch(le, worker);
        });

        if (answer.wait_for(std::chrono::seconds(3)) != std::future_status::ready && waiting->exchange(false))
            GMBT_LOG("The Windows Bluetooth adapter did not answer within 3 s; its capabilities follow when it does");

        initialized_ = true;
        message.clear();
        return Error::Ok;
    }

    void WindowsBackend::shutdown()
    {
        if (!initialized_)
            return;

        std::vector<std::shared_ptr<ClassicConnectionState>> connections;
        {
            std::scoped_lock lock(classic_->connections_mutex);
            classic_->alive->store(false);
            for (const auto& pair : classic_->connections)
                connections.push_back(pair.second);
        }

        std::vector<std::shared_ptr<RemoteGattConnectionState>> le_connections;
        {
            std::scoped_lock lock(le_client_->connections_mutex);
            le_client_->alive->store(false);
            for (const auto& pair : le_client_->connections)
                le_connections.push_back(pair.second);
            le_client_->connections.clear();
        }

        // From here no WinRT handler reports or touches anything.
        le_->alive.store(false);

        for (const auto& state : le_connections)
        {
            state->closing.store(true);
            close_le_client_connection_noexcept(state);
        }

        // With alive already false, none of these reports a stop.
        std::string ignored;
        le_scan_stop(ignored);
        le_advertise_stop(ignored);
        le_server_stop(ignored);
        classic_scan_stop(ignored);
        classic_server_stop(ignored);
        classic_discoverable_stop(ignored);

        // Only request a shutdown() here - never close the socket
        // directly. Each connection's own thread is the sole owner of
        // closesocket() (and of erasing itself from the map); a
        // connect still in progress gives up within one slice.
        for (const auto& state : connections)
            connection_request_close(state);

        release_watcher_noexcept();
        // Before the worker stops, so no watcher event posts to it after
        // (R1-229); a re-query already running sees alive false.
        release_adapter_watch_noexcept(*le_);
        release_radio_noexcept(*le_);

        // Last: its MTA served every WinRT call above.
        if (le_worker_)
            le_worker_->stop();

        // Everything is asked to stop; every thread the backend started
        // gets a bounded window to finish. Tearing down Winsock while a
        // worker still has a call in flight is undefined behavior, so a
        // worker still running past it leaves it up (the module is
        // pinned, so its code stays mapped).
        std::size_t remaining = 0;
        const bool drained = wait_for_workers(*workers_, std::chrono::seconds(5), remaining);

        if (classic_scan_thread_.joinable())
        {
            if (drained)
                classic_scan_thread_.join();
            else
                classic_scan_thread_.detach();
        }

        if (drained)
        {
            if (winsock_initialized_)
                WSACleanup();
        }
        else
        {
            GMBT_LOG(
                "Bluetooth shutdown: %zu worker thread(s) still running after 5 s; "
                "leaving Winsock initialized",
                remaining);
        }

        winsock_initialized_ = false;
        initialized_ = false;
    }

    bool WindowsBackend::pairing_is_supported(const DiscoveredDevice& device) const
    {
        return device.transport == Transport::Classic;
    }

    // BluetoothFeature raw values from spec.gmidl (R1-79). The name, service
    // UUIDs and service data go out through a registered GATT service, so
    // they are there whenever advertising is.
    bool WindowsBackend::feature_supported(std::int32_t feature) const
    {
        const bool le = le_->ble_supported.load();
        const bool advertise = supports_le_advertise();
        const bool server = supports_le_server();
        const bool classic = le_->classic_supported.load();
        switch (feature)
        {
            case 1: return le;              // LePassiveScan
            case 3:                         // LeAdvertiseName
            case 4:                         // LeAdvertiseServiceUuids
            case 5:                         // LeAdvertiseServiceData
            case 6:                         // LeAdvertiseManufacturerData
            case 7:                         // LeAdvertiseTxPower
            case 8:                         // LeAdvertiseIncludeTxPower
            case 9: return advertise;       // LeAdvertiseNonConnectable
            case 11: return server;         // LeServerDescriptorRequests
            case 17:                        // ClassicPairing
            case 18:                        // ClassicDiscoverable
            case 19: return classic;        // ClassicDiscoverableStop
            case 21:                        // LeMtuRequest: Windows negotiates it
            case 22: return false;          // LeReadRssi: WinRT has no connected RSSI
            case 23:                        // LeConnectionPriority
                return le && le_->connection_parameters_api.load();
            case 24:                        // RequestEnable
            {
                std::scoped_lock lock(le_->radio_mutex);
                return le_->radio != nullptr;
            }
            case 25: return le || classic;  // PairedDevicesQuery
            default: return false;          // signed writes, live server
                                            // connection events, LE pairing,
                                            // a permission prompt
        }
    }

    std::int32_t WindowsBackend::current_bluetooth_state() const
    {
        return le_->bluetooth_state.load();
    }

    PermissionStatus WindowsBackend::permission_status() const
    {
        return PermissionStatus::Granted;
    }

    // Windows asks a desktop app for no Bluetooth permission: the answer
    // is known, so it goes out at once.
    Error WindowsBackend::permission_request(std::string& message)
    {
        if (hooks_.push_event)
        {
            BackendEvent event;
            event.type = BackendEventType::PermissionResult;
            event.value = static_cast<std::int32_t>(PermissionStatus::Granted);
            hooks_.push_event(std::move(event));
        }

        message.clear();
        return Error::Ok;
    }

    // Asking for radio access and setting the state both wait on WinRT,
    // so they run on the worker, which answers (R1-143).
    Error WindowsBackend::request_enable(std::string& message)
    {
        if (!initialized_)
        {
            message = "Bluetooth backend is not initialized";
            return Error::NotInitialized;
        }

        WDR::Radio radio{nullptr};
        {
            std::scoped_lock lock(le_->radio_mutex);
            radio = le_->radio;
        }
        if (!radio)
        {
            message = "Windows found no Bluetooth radio to turn on";
            return Error::NotSupported;
        }

        const auto le = le_;
        const bool posted = le_worker_ && le_worker_->post([le, radio]()
        {
            request_radio_on(le, radio);
        });
        if (!posted)
        {
            message = "The Bluetooth worker thread is not running";
            return Error::OperationFailed;
        }

        message.clear();
        return Error::Ok;
    }

    // Ids are built from the address alone and connects resolve the
    // address, so an id makes a usable entry with no WinRT call (R1-144).
    Error WindowsBackend::device_from_id(const std::string& id, DiscoveredDevice& device, std::string& message)
    {
        static constexpr std::string_view ble_prefix = "win:ble:";
        static constexpr std::string_view classic_prefix = "win:classic:";

        Transport transport = Transport::Unknown;
        std::string address;
        if (id.compare(0, ble_prefix.size(), ble_prefix) == 0)
        {
            transport = Transport::LowEnergy;
            address = normalized_address(id.substr(ble_prefix.size()));
        }
        else if (id.compare(0, classic_prefix.size(), classic_prefix) == 0)
        {
            transport = Transport::Classic;
            address = normalized_address(id.substr(classic_prefix.size()));
        }

        BTH_ADDR parsed = 0;
        if (address.empty() || !parse_bluetooth_address(address, parsed))
        {
            message = "Expected a Windows device id: win:ble: or win:classic: and an address like AA:BB:CC:DD:EE:FF";
            return Error::InvalidArgument;
        }

        device = {};
        device.transport = transport;
        device.address = address;
        device.id = std::string(transport == Transport::LowEnergy ? ble_prefix : classic_prefix) + address;
        device.address_available = true;
        message.clear();
        return Error::Ok;
    }

    Error WindowsBackend::le_connected_devices_query(
        std::uint64_t query_id,
        const std::vector<std::string>& service_uuids,
        std::string& message)
    {
        // Windows cannot tell a device's services without connecting to
        // it, so every connected LE device is listed.
        (void)service_uuids;
        return post_devices_query(query_id, message,
            [](const std::shared_ptr<SharedLeState>& le, std::vector<DiscoveredDevice>& devices, std::string&)
            {
                find_le_devices(
                    le,
                    WDB::BluetoothLEDevice::GetDeviceSelectorFromConnectionStatus(WDB::BluetoothConnectionStatus::Connected),
                    devices);
                return Error::Ok;
            });
    }

    // Classic bonds from the Win32 stack, then paired LE devices (R1-145).
    Error WindowsBackend::paired_devices_query(std::uint64_t query_id, std::string& message)
    {
        const bool classic = le_->classic_supported.load();
        const bool ble = le_->ble_supported.load();
        return post_devices_query(query_id, message,
            [classic, ble](const std::shared_ptr<SharedLeState>& le, std::vector<DiscoveredDevice>& devices, std::string& failure)
            {
                if (classic)
                {
                    const Error error = find_paired_classic_devices(devices, failure);
                    if (error != Error::Ok)
                        return error;
                }
                if (ble)
                    find_le_devices(le, WDB::BluetoothLEDevice::GetDeviceSelectorFromPairingState(true), devices);
                return Error::Ok;
            });
    }

    // Runs find on the worker, which pushes query_id's one DevicesQueried
    // with what it found or why it failed.
    template <typename Find>
    Error WindowsBackend::post_devices_query(std::uint64_t query_id, std::string& message, Find find)
    {
        if (!initialized_)
        {
            message = "Bluetooth backend is not initialized";
            return Error::NotInitialized;
        }

        const auto le = le_;
        const bool posted = le_worker_ && le_worker_->post([le, query_id, find]()
        {
            BackendEvent event;
            event.type = BackendEventType::DevicesQueried;
            event.op_id = query_id;
            try
            {
                event.error = find(le, event.devices, event.message);
            }
            catch (const winrt::hresult_error& error)
            {
                event.error = Error::OperationFailed;
                event.message = winrt::to_string(error.message());
                trim_trailing_space(event.message);
            }
            catch (...)
            {
                event.error = Error::OperationFailed;
                event.message = "Listing Bluetooth devices failed with an unexpected exception";
            }
            if (event.error != Error::Ok)
                event.devices.clear();
            push_le_event(le, std::move(event));
        });

        if (!posted)
        {
            message = "The Bluetooth worker thread is not running";
            return Error::OperationFailed;
        }

        message.clear();
        return Error::Ok;
    }

    WindowsBackend::~WindowsBackend()
    {
        if (initialized_)
        {
            shutdown();
        }
        else
        {
            // Discoverability needs no initialize; its timer is joinable.
            std::string ignored;
            classic_discoverable_stop(ignored);
        }
    }
}

namespace gmbluetooth
{
    std::unique_ptr<Backend> create_platform_backend(CoreHooks hooks)
    {
        return std::make_unique<win::WindowsBackend>(std::move(hooks));
    }
}

#endif
