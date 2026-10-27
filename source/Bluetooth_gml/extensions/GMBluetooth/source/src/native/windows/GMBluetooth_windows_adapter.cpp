// Windows backend: the adapter and radio, and the LE scan.

#include "GMBluetooth_windows_internal.h"

#if defined(_WIN32)

namespace gmbluetooth::win
{
    namespace
    {
        bool advertisement_is_connectable(const WDBA::BluetoothLEAdvertisementReceivedEventArgs& args)
        {
            const auto type = args.AdvertisementType();

            // An extended advertisement carries its connectable bit apart
            // from its type (R2-158).
            if (type == WDBA::BluetoothLEAdvertisementType::Extended)
                return args.IsConnectable();

            return type == WDBA::BluetoothLEAdvertisementType::ConnectableUndirected ||
                   type == WDBA::BluetoothLEAdvertisementType::ConnectableDirected;
        }

        // Windows 10 2004 and later; the watcher receives legacy
        // advertisements only until it is set (R2-158).
        bool extended_advertisements_api_present() noexcept
        {
            try
            {
                return WFM::ApiInformation::IsPropertyPresent(
                    L"Windows.Devices.Bluetooth.Advertisement.BluetoothLEAdvertisementWatcher",
                    L"AllowExtendedAdvertisements");
            }
            catch (...)
            {
                return false;
            }
        }

        std::int32_t normalized_radio_state(WDR::RadioState state)
        {
            // BluetoothState raw values from spec.gmidl.
            switch (state)
            {
                case WDR::RadioState::On: return 5;       // PoweredOn
                case WDR::RadioState::Off: return 4;      // PoweredOff
                case WDR::RadioState::Disabled: return 4; // PoweredOff / policy-disabled
                default: return 0;                        // Unknown
            }
        }

        // What one advertising packet carried besides the name (R1-138). Windows
        // parses no service data, so it comes from the raw sections: types 0x16,
        // 0x20 and 0x21 start with a 16-, 32- or 128-bit UUID, little-endian, and
        // the rest is the data. Short UUIDs stay short; the core expands them.
        LeAdvertisement read_advertisement(const WDBA::BluetoothLEAdvertisementReceivedEventArgs& args)
        {
            static const char hex[] = "0123456789abcdef";

            LeAdvertisement out;
            const auto advertisement = args.Advertisement();

            for (const auto& uuid : advertisement.ServiceUuids())
                out.service_uuids.push_back(guid_to_uuid_string(uuid));

            for (const auto& entry : advertisement.ManufacturerData())
                out.manufacturer_data.push_back({ entry.CompanyId(), buffer_to_bytes(entry.Data()) });

            for (const auto& section : advertisement.DataSections())
            {
                std::size_t uuid_size = 0;
                switch (section.DataType())
                {
                    case 0x16: uuid_size = 2; break;
                    case 0x20: uuid_size = 4; break;
                    case 0x21: uuid_size = 16; break;
                    default: continue;
                }

                const std::vector<std::uint8_t> bytes = buffer_to_bytes(section.Data());
                if (bytes.size() < uuid_size)
                    continue;

                // Most significant byte first, with the 8-4-4-4-12 dashes for a
                // full UUID.
                std::string uuid;
                for (std::size_t i = uuid_size; i-- > 0;)
                {
                    uuid += hex[bytes[i] >> 4];
                    uuid += hex[bytes[i] & 0xF];
                    if (uuid_size == 16 && (i == 12 || i == 10 || i == 8 || i == 6))
                        uuid += '-';
                }

                out.service_data.push_back({
                    std::move(uuid),
                    std::vector<std::uint8_t>(bytes.begin() + static_cast<std::ptrdiff_t>(uuid_size), bytes.end()) });
            }

            // Missing before Windows 10 2004; a packet then just has no TX power.
            try
            {
                if (const auto tx_power = args.TransmitPowerLevelInDBm())
                    out.tx_power = static_cast<std::int32_t>(tx_power.Value());
            }
            catch (const winrt::hresult_error&)
            {
            }

            return out;
        }

        // BluetoothState raw values from spec.gmidl.
        constexpr std::int32_t k_state_unknown = 0;

        constexpr std::int32_t k_state_unsupported = 2;

        // An adapter came, went or changed, on the backend worker (R1-229): the
        // adapter and radio are looked up again, and a state that moved reports.
        void requery_adapter(const std::shared_ptr<SharedLeState>& le) noexcept
        {
            // Cleared first, so a change during the query posts the next one.
            le->adapter_requery_pending.store(false);
            if (!le->alive.load())
                return;

            const std::int32_t before = le->bluetooth_state.load();
            release_radio_noexcept(*le);
            le->classic_supported.store(has_classic_radio());
            query_adapter(le);

            if (le->bluetooth_state.load() != before)
                push_bluetooth_state(le);
        }

        // An adapter watcher event, on a WinRT thread. The first enumeration's
        // Added events only list what query_adapter already saw and are skipped;
        // its completion and every change after it post the re-query unless one
        // is already waiting.
        void adapter_watch_event(
            const std::weak_ptr<SharedLeState>& weak,
            const std::weak_ptr<SerialWorker>& weak_worker)
        {
            const auto le = weak.lock();
            if (!le || !le->alive.load() || !le->adapter_enumerated.load())
                return;
            if (le->adapter_requery_pending.exchange(true))
                return;

            const auto worker = weak_worker.lock();
            if (!worker || !worker->post([le]() { requery_adapter(le); }))
                le->adapter_requery_pending.store(false);
        }

        // Revokes the handlers and stops the watcher. Neither call waits, so
        // shutdown makes them on the game thread.
        void stop_adapter_watch_noexcept(AdapterWatch& watch) noexcept
        {
            if (!watch.watcher)
                return;

            try
            {
                if (watch.added_token.value != 0)
                    watch.watcher.Added(watch.added_token);
                if (watch.removed_token.value != 0)
                    watch.watcher.Removed(watch.removed_token);
                if (watch.updated_token.value != 0)
                    watch.watcher.Updated(watch.updated_token);
                if (watch.enumerated_token.value != 0)
                    watch.watcher.EnumerationCompleted(watch.enumerated_token);
            }
            catch (...)
            {
            }

            try
            {
                const auto status = watch.watcher.Status();
                if (status == WDE::DeviceWatcherStatus::Started ||
                    status == WDE::DeviceWatcherStatus::EnumerationCompleted)
                {
                    watch.watcher.Stop();
                }
            }
            catch (...)
            {
            }

            watch = AdapterWatch{};
        }

        constexpr std::size_t k_max_address_types = 1024;

        void remember_address_type(SharedLeState& le, std::uint64_t address, WDB::BluetoothAddressType type)
        {
            std::scoped_lock lock(le.address_types_mutex);
            if (le.address_types.size() >= k_max_address_types &&
                le.address_types.find(address) == le.address_types.end())
            {
                le.address_types.clear();
            }
            le.address_types[address] = type;
        }
    }

    // A radio the Win32 Classic stack can open.
    bool has_classic_radio()
    {
        BLUETOOTH_FIND_RADIO_PARAMS params{};
        params.dwSize = sizeof(params);
        HANDLE radio = nullptr;
        HBLUETOOTH_RADIO_FIND find = BluetoothFindFirstRadio(&params, &radio);
        if (!find)
            return false;
        BluetoothFindRadioClose(find);
        if (radio)
            CloseHandle(radio);
        return true;
    }

    // The adapter and radio query, on the backend worker. A failed query
    // reports LE as unsupported, never as supported (R1-71); Classic keeps
    // what the Win32 stack said.
    void query_adapter(const std::shared_ptr<SharedLeState>& le) noexcept
    {
        const auto no_adapter = [&le]()
        {
            le->ble_supported.store(false);
            le->le_peripheral_supported.store(false);
            le->bluetooth_state.store(le->classic_supported.load() ? k_state_unknown : k_state_unsupported);
        };

        try
        {
            const auto adapter = WDB::BluetoothAdapter::GetDefaultAsync().get();
            if (!adapter)
            {
                no_adapter();
                return;
            }

            le->ble_supported.store(adapter.IsLowEnergySupported());
            le->le_peripheral_supported.store(adapter.IsPeripheralRoleSupported());
            le->classic_supported.store(adapter.IsClassicSupported());

            const auto radio = adapter.GetRadioAsync().get();
            if (!radio)
            {
                le->bluetooth_state.store(k_state_unknown);
                return;
            }
            le->bluetooth_state.store(normalized_radio_state(radio.State()));

            std::weak_ptr<SharedLeState> weak = le;
            std::scoped_lock lock(le->radio_mutex);
            if (!le->alive.load())
                return;
            le->radio = radio;
            le->radio_state_token = radio.StateChanged(
                [weak](const WDR::Radio& sender, const WF::IInspectable&)
                {
                    const auto current = weak.lock();
                    if (!current || !current->alive.load())
                        return;
                    try
                    {
                        current->bluetooth_state.store(normalized_radio_state(sender.State()));
                    }
                    catch (...)
                    {
                        return;
                    }
                    push_bluetooth_state(current);
                });
        }
        catch (const winrt::hresult_error& error)
        {
            GMBT_LOG("Windows Bluetooth adapter query failed: %s", winrt::to_string(error.message()).c_str());
            no_adapter();
        }
        catch (...)
        {
            GMBT_LOG("Windows Bluetooth adapter query failed");
            no_adapter();
        }
    }

    // Lets the radio go with its StateChanged handler: in shutdown, and
    // before an adapter re-query looks for the radio again.
    void release_radio_noexcept(SharedLeState& le) noexcept
    {
        WDR::Radio radio{nullptr};
        winrt::event_token token{};
        {
            std::scoped_lock lock(le.radio_mutex);
            radio = le.radio;
            token = le.radio_state_token;
            le.radio = nullptr;
            le.radio_state_token = {};
        }

        try
        {
            if (radio && token.value != 0)
                radio.StateChanged(token);
        }
        catch (...)
        {
        }
    }

    void release_adapter_watch_noexcept(SharedLeState& le) noexcept
    {
        AdapterWatch watch;
        {
            std::scoped_lock lock(le.adapter_watch_mutex);
            std::swap(watch, le.adapter_watch);
        }
        stop_adapter_watch_noexcept(watch);
    }

    // Starts the adapter watcher, on the backend worker after the first
    // adapter query. It starts under the mutex shutdown takes to stop it,
    // so a shutdown either finds it or comes first and it never starts.
    void start_adapter_watch(
        const std::shared_ptr<SharedLeState>& le,
        const std::weak_ptr<SerialWorker>& worker) noexcept
    {
        AdapterWatch watch;
        try
        {
            watch.watcher = WDE::DeviceInformation::CreateWatcher(WDB::BluetoothAdapter::GetDeviceSelector());

            const std::weak_ptr<SharedLeState> weak = le;
            watch.added_token = watch.watcher.Added(
                [weak, worker](const WDE::DeviceWatcher&, const WDE::DeviceInformation&)
                {
                    adapter_watch_event(weak, worker);
                });
            watch.removed_token = watch.watcher.Removed(
                [weak, worker](const WDE::DeviceWatcher&, const WDE::DeviceInformationUpdate&)
                {
                    adapter_watch_event(weak, worker);
                });
            watch.updated_token = watch.watcher.Updated(
                [weak, worker](const WDE::DeviceWatcher&, const WDE::DeviceInformationUpdate&)
                {
                    adapter_watch_event(weak, worker);
                });
            // One re-query once the first enumeration is done covers an
            // adapter that came or went between the first query and Start().
            watch.enumerated_token = watch.watcher.EnumerationCompleted(
                [weak, worker](const WDE::DeviceWatcher&, const WF::IInspectable&)
                {
                    if (const auto current = weak.lock())
                        current->adapter_enumerated.store(true);
                    adapter_watch_event(weak, worker);
                });

            std::scoped_lock lock(le->adapter_watch_mutex);
            if (le->alive.load())
            {
                watch.watcher.Start();
                std::swap(watch, le->adapter_watch);
                return;
            }
        }
        catch (const winrt::hresult_error& error)
        {
            GMBT_LOG("Could not watch for Bluetooth adapters: %s", winrt::to_string(error.message()).c_str());
        }
        catch (...)
        {
            GMBT_LOG("Could not watch for Bluetooth adapters");
        }

        stop_adapter_watch_noexcept(watch);
    }

    std::optional<WDB::BluetoothAddressType> find_address_type(SharedLeState& le, std::uint64_t address)
    {
        std::scoped_lock lock(le.address_types_mutex);
        const auto it = le.address_types.find(address);
        if (it == le.address_types.end())
            return std::nullopt;
        return it->second;
    }

    bool connection_parameters_api_present() noexcept
    {
        try
        {
            return WFM::ApiInformation::IsMethodPresent(
                L"Windows.Devices.Bluetooth.BluetoothLEDevice",
                L"RequestPreferredConnectionParameters");
        }
        catch (...)
        {
            return false;
        }
    }

    // request_enable's job on the backend worker: pushes its one
    // EnableResult whatever happens.
    void request_radio_on(const std::shared_ptr<SharedLeState>& le, const WDR::Radio& radio) noexcept
    {
        BackendEvent event;
        event.type = BackendEventType::EnableResult;

        try
        {
            if (WDR::Radio::RequestAccessAsync().get() != WDR::RadioAccessStatus::Allowed)
            {
                event.error = Error::PermissionDenied;
                event.message = "Windows denied access to the Bluetooth radio";
            }
            else
            {
                switch (radio.SetStateAsync(WDR::RadioState::On).get())
                {
                    case WDR::RadioAccessStatus::Allowed:
                        if (radio.State() != WDR::RadioState::On)
                        {
                            event.error = Error::BluetoothDisabled;
                            event.message = "The Bluetooth radio is still off";
                        }
                        break;
                    case WDR::RadioAccessStatus::DeniedByUser:
                    case WDR::RadioAccessStatus::DeniedBySystem:
                        event.error = Error::PermissionDenied;
                        event.message = "Windows denied turning the Bluetooth radio on";
                        break;
                    default:
                        event.error = Error::BluetoothDisabled;
                        event.message = "Windows could not turn the Bluetooth radio on";
                        break;
                }
            }
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
            event.message = "Turning the Bluetooth radio on failed with an unexpected exception";
        }

        push_le_event(le, std::move(event));
    }

    // Every LE device an AQS selector names, resolved to its address. One
    // Windows can no longer open is left out rather than failing the list.
    void find_le_devices(
        const std::shared_ptr<SharedLeState>& le,
        const winrt::hstring& selector,
        std::vector<DiscoveredDevice>& out)
    {
        for (const auto& info : WDE::DeviceInformation::FindAllAsync(selector).get())
        {
            try
            {
                const auto remote = WDB::BluetoothLEDevice::FromIdAsync(info.Id()).get();
                if (!remote)
                    continue;

                const std::uint64_t address = remote.BluetoothAddress();
                DiscoveredDevice device;
                device.transport = Transport::LowEnergy;
                device.address = format_bluetooth_address(address);
                device.id = le_device_id(device.address, remote.BluetoothAddressType());
                device.address_available = true;
                device.connectable = true;
                device.name = winrt::to_string(info.Name());
                if (device.name.empty())
                    device.name = winrt::to_string(remote.Name());

                // le_connect opens it with this type, as for a scanned one (R1-182).
                remember_address_type(*le, address, remote.BluetoothAddressType());
                remote.Close();
                out.push_back(std::move(device));
            }
            catch (const winrt::hresult_error& error)
            {
                GMBT_LOG("Skipping a Bluetooth LE device Windows could not open: %s", winrt::to_string(error.message()).c_str());
            }
        }
    }

    Error WindowsBackend::le_scan_start(const LeScanSettings& settings, const std::vector<LeScanFilter>& filters, std::string& message)
    {
        if (!initialized_)
        {
            message = "Bluetooth backend is not initialized";
            return Error::NotInitialized;
        }
        // A Classic-only adapter leaves the state Unknown, which the
        // core's radio check passes (R1-243).
        if (!le_->ble_supported.load())
        {
            message = "This Bluetooth adapter does not support Bluetooth LE";
            return Error::NotSupported;
        }

        // The core matches every result against the filters. A WinRT
        // AdvertisementFilter takes one pattern and ANDs its fields, so
        // it cannot say "any of these" (R1-139). Windows has no scan modes
        // (LeScanMode), so settings.mode is ignored.
        (void)filters;

        if (le_->le_scanning.load())
        {
            message.clear();
            return Error::Ok;
        }

        try
        {
            release_watcher_noexcept();
            watcher_ = WDBA::BluetoothLEAdvertisementWatcher{};
            watcher_.ScanningMode(
                settings.active
                    ? WDBA::BluetoothLEScanningMode::Active
                    : WDBA::BluetoothLEScanningMode::Passive);

            if (extended_advertisements_api_present())
                watcher_.AllowExtendedAdvertisements(true);

            // An event from an earlier watcher is stale once this
            // generation is current (R1-187).
            const std::uint64_t generation = ++le_->scan_generation;
            const std::weak_ptr<SharedLeState> weak = le_;

            received_token_ = watcher_.Received(
                [weak, generation](
                    const WDBA::BluetoothLEAdvertisementWatcher&,
                    const WDBA::BluetoothLEAdvertisementReceivedEventArgs& args)
                {
                    const auto le = weak.lock();
                    if (!le || !le->alive.load() || le->scan_generation.load() != generation)
                        return;

                    DiscoveredDevice device;
                    device.transport = Transport::LowEnergy;

                    const std::uint64_t raw_address = args.BluetoothAddress();
                    device.address = format_bluetooth_address(raw_address);
                    device.id = le_device_id(device.address, args.BluetoothAddressType());
                    device.address_available = true;
                    device.rssi = static_cast<std::int32_t>(args.RawSignalStrengthInDBm());
                    device.rssi_available = device.rssi != -127;
                    device.connectable = advertisement_is_connectable(args);

                    const auto local_name = args.Advertisement().LocalName();
                    if (!local_name.empty())
                        device.name = winrt::to_string(local_name);

                    // A payload that will not parse still reports the device.
                    try
                    {
                        device.advertisement = read_advertisement(args);
                    }
                    catch (const winrt::hresult_error& error)
                    {
                        GMBT_LOG("Could not read an LE advertisement: %s", winrt::to_string(error.message()).c_str());
                    }

                    // le_connect opens it with the type it advertised (R1-182).
                    remember_address_type(*le, raw_address, args.BluetoothAddressType());

                    if (le->hooks.upsert_device)
                        le->hooks.upsert_device(device);
                });

            stopped_token_ = watcher_.Stopped(
                [weak, generation](
                    const WDBA::BluetoothLEAdvertisementWatcher&,
                    const WDBA::BluetoothLEAdvertisementWatcherStoppedEventArgs& args)
                {
                    const auto le = weak.lock();
                    if (!le || !le->alive.load() || le->scan_generation.load() != generation)
                        return;

                    // A stop Windows made (radio off, an error); one the core
                    // asked for is never reported.
                    if (!le->le_scanning.exchange(false))
                        return;

                    BackendEvent event;
                    event.type = BackendEventType::ScanStopped;
                    event.transport = Transport::LowEnergy;
                    event.error = map_bluetooth_failure(args.Error());
                    event.message = bluetooth_failure_message(args.Error());
                    push_le_event(le, std::move(event));
                });

            le_->le_scanning.store(true);
            watcher_.Start();
            message.clear();
            return Error::Ok;
        }
        catch (const winrt::hresult_error& error)
        {
            le_->le_scanning.store(false);
            ++le_->scan_generation;
            release_watcher_noexcept();
            message = winrt::to_string(error.message());
            return Error::OperationFailed;
        }
    }

    // Reports nothing: the core has ended the scan's session, and the
    // watcher's handlers are gone with the generation bump.
    Error WindowsBackend::le_scan_stop(std::string& message)
    {
        if (!initialized_)
            return Error::NotInitialized;

        le_->le_scanning.store(false);
        ++le_->scan_generation;

        std::string failure;
        try
        {
            if (watcher_)
                watcher_.Stop();
        }
        catch (const winrt::hresult_error& error)
        {
            failure = winrt::to_string(error.message());
        }
        release_watcher_noexcept();

        if (!failure.empty())
        {
            message = failure;
            return Error::OperationFailed;
        }
        message.clear();
        return Error::Ok;
    }

    bool WindowsBackend::le_scan_is_running() const
    {
        return le_->le_scanning.load();
    }

    void WindowsBackend::release_watcher_noexcept() noexcept
    {
        try
        {
            if (watcher_)
            {
                if (received_token_.value != 0)
                {
                    watcher_.Received(received_token_);
                    received_token_ = {};
                }
                if (stopped_token_.value != 0)
                {
                    watcher_.Stopped(stopped_token_);
                    stopped_token_ = {};
                }
                watcher_ = nullptr;
            }
        }
        catch (...)
        {
        }
    }
}

#endif
