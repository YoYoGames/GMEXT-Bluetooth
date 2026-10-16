// Windows backend: the LE client - connections, GATT discovery, reads,
// writes and subscriptions - over WinRT.

#include "GMBluetooth_windows_internal.h"

#if defined(_WIN32)

namespace gmbluetooth::win
{
    namespace
    {
        // A GATT call's outcome as the core reports it: the BluetoothError and,
        // on a failure, the message saying why.
        struct GattOutcome
        {
            Error error = Error::Ok;
            std::string message;
        };

        // status is what the call returned; protocol_error is the ATT code
        // Windows attaches to a ProtocolError, when it has one.
        GattOutcome gatt_outcome(
            WDBG::GattCommunicationStatus status,
            const WF::IReference<std::uint8_t>& protocol_error = nullptr)
        {
            switch (status)
            {
                case WDBG::GattCommunicationStatus::Success:
                    return {};
                case WDBG::GattCommunicationStatus::Unreachable:
                    return { Error::Disconnected, "The GATT peer is unreachable" };
                case WDBG::GattCommunicationStatus::AccessDenied:
                    return { Error::PermissionDenied, "Windows denied access to the GATT operation" };
                case WDBG::GattCommunicationStatus::ProtocolError:
                    if (protocol_error)
                    {
                        const int att = static_cast<int>(protocol_error.Value());
                        const Error error = map_att_error(att);
                        if (error != Error::Ok)
                            return { error, att_error_message(att) };
                    }
                    return { Error::OperationFailed, "GATT protocol error with no ATT error code" };
                default:
                    return {
                        Error::OperationFailed,
                        "Windows GATT operation failed with status " + std::to_string(static_cast<int>(status)) };
            }
        }

        // The outcome of a GATT result object (services, characteristics,
        // descriptors, read or write result).
        template <typename Result>
        GattOutcome gatt_result_outcome(const Result& result)
        {
            return gatt_outcome(result.Status(), result.ProtocolError());
        }

        // An exception a GATT call threw. The device or session closing under
        // the call is a disconnect; anything else fails with Windows' text and
        // the HRESULT.
        GattOutcome gatt_exception_outcome(const winrt::hresult_error& error)
        {
            char code[24]{};
            std::snprintf(code, sizeof(code), "HRESULT 0x%08X", static_cast<unsigned>(error.code().value));

            std::string message = winrt::to_string(error.message());
            trim_trailing_space(message);
            message = message.empty() ? std::string(code) : message + " (" + code + ")";

            if (error.code() == winrt::hresult{RO_E_CLOSED})
                return { Error::Disconnected, std::move(message) };
            return { Error::OperationFailed, std::move(message) };
        }

        // What a job's catch (...) reports.
        constexpr const char* k_gatt_unexpected_failure =
            "Windows GATT operation failed with an unexpected exception";

        void push_le_client_event(
            const std::shared_ptr<SharedLeClientState>& shared,
            BackendEvent event)
        {
            if (!shared || !shared->alive->load() || !shared->hooks.push_event)
                return;

            event.transport = Transport::LowEnergy;
            shared->hooks.push_event(std::move(event));
        }

        // The completion of the LE call the core registered as op_id. Each
        // connection runs its ops on its own worker, so completions of different
        // connections arrive in any order; the id is what the core matches them by.
        void push_le_op_completion(
            const std::shared_ptr<SharedLeClientState>& shared,
            std::uint64_t op_id,
            Error error,
            std::string message,
            LeOpResult result = {})
        {
            if (!shared || !shared->alive->load() || !shared->hooks.push_event)
                return;

            BackendEvent event;
            event.type = BackendEventType::LeOpCompleted;
            event.transport = Transport::LowEnergy;
            event.op_id = op_id;
            event.error = error;
            event.message = std::move(message);
            event.result = std::move(result);
            shared->hooks.push_event(std::move(event));
        }

        void push_le_op_completion(
            const std::shared_ptr<SharedLeClientState>& shared,
            std::uint64_t op_id,
            GattOutcome outcome)
        {
            push_le_op_completion(shared, op_id, outcome.error, std::move(outcome.message));
        }

        // The end of an le_connect: Ok, or why the link never came up.
        void push_le_connect_result(
            const std::shared_ptr<SharedLeClientState>& shared,
            std::uint64_t connection,
            Error error,
            std::string message)
        {
            BackendEvent event;
            event.type = BackendEventType::LeConnectResult;
            event.connection = connection;
            event.error = error;
            event.message = std::move(message);
            push_le_client_event(shared, std::move(event));
        }

        constexpr const char* k_le_open_cancelled = "Connection cancelled by bluetooth_le_disconnect";

        constexpr const char* k_le_open_dropped = "The Bluetooth LE device disconnected while opening";

        constexpr const char* k_le_attribute_closed = "The GATT attribute was closed with its connection or a re-discovery";

        std::shared_ptr<RemoteGattConnectionState> find_le_client_connection(
            const std::shared_ptr<SharedLeClientState>& shared,
            std::uint64_t handle)
        {
            if (!shared)
                return {};

            std::scoped_lock lock(shared->connections_mutex);
            const auto it = shared->connections.find(handle);
            return it != shared->connections.end() ? it->second : nullptr;
        }

        void unregister_le_client_connection(
            const std::shared_ptr<SharedLeClientState>& shared,
            std::uint64_t handle)
        {
            if (!shared)
                return;

            std::scoped_lock lock(shared->connections_mutex);
            shared->connections.erase(handle);
        }

        // The attribute handle an instance from the core names; empty for one no
        // discovery here could have reported.
        std::optional<std::uint16_t> attribute_handle(std::uint64_t instance)
        {
            if (instance == 0 || instance > 0xFFFF)
                return std::nullopt;
            return static_cast<std::uint16_t>(instance);
        }

        std::shared_ptr<RemoteGattServiceState> find_remote_service(
            const std::shared_ptr<RemoteGattConnectionState>& connection,
            std::uint64_t service_instance)
        {
            const auto key = attribute_handle(service_instance);
            if (!connection || !key)
                return {};

            std::scoped_lock lock(connection->mutex);
            const auto it = connection->services.find(*key);
            return it != connection->services.end() ? it->second : nullptr;
        }

        std::shared_ptr<RemoteGattCharacteristicState> find_remote_characteristic(
            const std::shared_ptr<RemoteGattConnectionState>& connection,
            std::uint64_t service_instance,
            std::uint64_t characteristic_instance)
        {
            auto service = find_remote_service(connection, service_instance);
            const auto key = attribute_handle(characteristic_instance);
            if (!service || !key)
                return {};

            std::scoped_lock lock(connection->mutex);
            const auto it = service->characteristics.find(*key);
            return it != service->characteristics.end() ? it->second : nullptr;
        }

        std::shared_ptr<RemoteGattDescriptorState> find_remote_descriptor(
            const std::shared_ptr<RemoteGattConnectionState>& connection,
            std::uint64_t service_instance,
            std::uint64_t characteristic_instance,
            std::uint64_t descriptor_instance)
        {
            auto characteristic = find_remote_characteristic(
                connection,
                service_instance,
                characteristic_instance);
            const auto key = attribute_handle(descriptor_instance);
            if (!characteristic || !key)
                return {};

            std::scoped_lock lock(connection->mutex);
            const auto it = characteristic->descriptors.find(*key);
            return it != characteristic->descriptors.end() ? it->second : nullptr;
        }

        // The WinRT object behind a cached attribute, read under the connection's
        // mutex; null once a close or a re-discovery dropped it.
        WDBG::GattDeviceService service_object(
            const RemoteGattConnectionState& state,
            const RemoteGattServiceState& service)
        {
            std::scoped_lock lock(state.mutex);
            return service.service;
        }

        WDBG::GattCharacteristic characteristic_object(
            const RemoteGattConnectionState& state,
            const RemoteGattCharacteristicState& characteristic)
        {
            std::scoped_lock lock(state.mutex);
            return characteristic.characteristic;
        }

        WDBG::GattDescriptor descriptor_object(
            const RemoteGattConnectionState& state,
            const RemoteGattDescriptorState& descriptor)
        {
            std::scoped_lock lock(state.mutex);
            return descriptor.descriptor;
        }

        // Caller holds the connection's mutex.
        bool service_has_subscription_locked(const RemoteGattServiceState& service)
        {
            for (const auto& pair : service.characteristics)
            {
                if (pair.second && pair.second->value_changed_registered)
                    return true;
            }
            return false;
        }

        void close_remote_characteristic_noexcept(
            const std::shared_ptr<RemoteGattConnectionState>& state,
            const std::shared_ptr<RemoteGattCharacteristicState>& characteristic)
        {
            if (!state || !characteristic)
                return;

            WDBG::GattCharacteristic object{nullptr};
            winrt::event_token token{};
            {
                std::scoped_lock lock(state->mutex);
                object = characteristic->characteristic;
                if (characteristic->value_changed_registered)
                    token = characteristic->value_changed_token;
                characteristic->value_changed_token = {};
                characteristic->value_changed_registered = false;
                characteristic->descriptors.clear();
                characteristic->characteristic = nullptr;
            }

            try
            {
                if (object && token.value != 0)
                    object.ValueChanged(token);
            }
            catch (...)
            {
            }
        }

        void close_remote_service_noexcept(
            const std::shared_ptr<RemoteGattConnectionState>& state,
            const std::shared_ptr<RemoteGattServiceState>& service)
        {
            if (!state || !service)
                return;

            std::unordered_map<std::uint16_t, std::shared_ptr<RemoteGattCharacteristicState>> characteristics;
            WDBG::GattDeviceService object{nullptr};
            {
                std::scoped_lock lock(state->mutex);
                characteristics.swap(service->characteristics);
                object = service->service;
                service->service = nullptr;
            }

            for (auto& pair : characteristics)
                close_remote_characteristic_noexcept(state, pair.second);

            try
            {
                if (object)
                    object.Close();
            }
            catch (...)
            {
            }
        }

        // The CCCD value a subscribe mode writes: 1 notify, 2 indicate, 0 none.
        WDBG::GattClientCharacteristicConfigurationDescriptorValue cccd_value_for_mode(std::int32_t mode)
        {
            return mode == 1
                ? WDBG::GattClientCharacteristicConfigurationDescriptorValue::Notify
            : mode == 2
                ? WDBG::GattClientCharacteristicConfigurationDescriptorValue::Indicate
                : WDBG::GattClientCharacteristicConfigurationDescriptorValue::None;
        }

        // Registers the ValueChanged handler that reports a subscribed
        // characteristic's notifications, naming it by the instances the core
        // asked with (R1-127). False when the connection closed first: the
        // handler is revoked again.
        bool register_value_changed(
            const std::shared_ptr<SharedLeClientState>& shared,
            const std::shared_ptr<RemoteGattConnectionState>& state,
            const std::shared_ptr<RemoteGattCharacteristicState>& characteristic,
            const WDBG::GattCharacteristic& object,
            std::uint64_t service_instance,
            std::uint64_t characteristic_instance)
        {
            std::weak_ptr<RemoteGattConnectionState>
                weak_connection = state;

            const auto token = object.ValueChanged(
                [shared,
                 weak_connection,
                 service_instance,
                 characteristic_instance](
                    const WDBG::GattCharacteristic&,
                    const WDBG::GattValueChangedEventArgs& args)
                {
                    auto current =
                        weak_connection.lock();
                    if (!current ||
                        !shared->alive->load())
                        return;

                    BackendEvent event;
                    event.type = BackendEventType::LeValueChanged;
                    event.connection = current->handle;
                    event.attribute.service = service_instance;
                    event.attribute.characteristic = characteristic_instance;
                    event.bytes = buffer_to_bytes(args.CharacteristicValue());
                    push_le_client_event(shared, std::move(event));
                });

            // Stored under the mutex close takes; a close that came first
            // leaves this handler to revoke now (R1-113).
            bool kept = false;
            {
                std::scoped_lock lock(state->mutex);
                kept = !state->closing.load() && characteristic->characteristic;
                if (kept)
                {
                    characteristic->value_changed_token = token;
                    characteristic->value_changed_registered = true;
                }
            }

            if (!kept)
            {
                try
                {
                    object.ValueChanged(token);
                }
                catch (...)
                {
                }
            }
            return kept;
        }

        // Revokes the ValueChanged handler of a characteristic the CCCD write
        // unsubscribed.
        void release_value_changed(
            RemoteGattConnectionState& state,
            RemoteGattCharacteristicState& characteristic,
            const WDBG::GattCharacteristic& object)
        {
            winrt::event_token token{};
            {
                std::scoped_lock lock(state.mutex);
                if (characteristic.value_changed_registered)
                    token = characteristic.value_changed_token;
                characteristic.value_changed_registered = false;
                characteristic.value_changed_token = {};
            }

            try
            {
                if (token.value != 0)
                    object.ValueChanged(token);
            }
            catch (...)
            {
            }
        }

        // A connection's status changed, on a WinRT thread.
        void le_connection_status_changed(
            const std::shared_ptr<SharedLeClientState>& shared,
            const std::shared_ptr<RemoteGattConnectionState>& current,
            const WDB::BluetoothLEDevice& sender)
        {
            if (!shared->alive->load())
                return;

            bool connected = false;
            try
            {
                connected = sender.ConnectionStatus() == WDB::BluetoothConnectionStatus::Connected;
            }
            catch (...)
            {
                return;
            }

            const bool was_connected = current->connected.exchange(connected);
            if (connected || !was_connected)
                return;

            // Only a link the game did not close reports; Windows gives no
            // reason for the loss. It is then closed like one the game closed,
            // so nothing on it reports or comes back later (R1-114).
            if (current->closing.exchange(true))
                return;
            current->peer_dropped.store(true);

            unregister_le_client_connection(shared, current->handle);
            BackendEvent event;
            event.type = BackendEventType::LeDisconnected;
            event.connection = current->handle;
            event.error = Error::Disconnected;
            event.message = "The Bluetooth LE device disconnected";
            push_le_client_event(shared, std::move(event));

            // Its queued ops are dropped now; the device is closed off this
            // handler, on the backend worker.
            if (current->worker)
                current->worker->stop();
            const auto worker = shared->worker;
            if (!worker || !worker->post([current]() { close_le_client_connection_noexcept(current); }))
                close_le_client_connection_noexcept(current);
        }

        // The open of a connection: the first job on its worker.
        void open_le_connection(
            const std::shared_ptr<SharedLeClientState>& shared,
            const std::shared_ptr<RemoteGattConnectionState>& state,
            std::uint64_t address,
            std::optional<WDB::BluetoothAddressType> address_type)
        {
            // What it opened is closed and the core told why.
            const auto fail_open = [&shared, &state](Error failure, const std::string& why)
            {
                state->closing.store(true);
                close_le_client_connection_noexcept(state);
                unregister_le_client_connection(shared, state->handle);
                push_le_connect_result(shared, state->handle, failure, why);
            };

            const auto cancelled_message = [&state]()
            {
                return state->peer_dropped.load() ? k_le_open_dropped : k_le_open_cancelled;
            };

            try
            {
                if (!shared->alive->load() || state->closing.load())
                    return;

                // The advertised address type, when the scan saw one (R1-182).
                const auto remote = address_type
                    ? WDB::BluetoothLEDevice::FromBluetoothAddressAsync(address, *address_type).get()
                    : WDB::BluetoothLEDevice::FromBluetoothAddressAsync(address).get();

                if (!remote)
                {
                    fail_open(Error::ConnectionFailed, "Windows found no Bluetooth LE device at this address");
                    return;
                }

                // le_disconnect sets closing before it takes the state mutex to
                // close what is stored, so checking under that mutex leaves one
                // owner for the device: this job if the open was already
                // cancelled, close otherwise.
                bool cancelled = false;
                {
                    std::scoped_lock lock(state->mutex);
                    cancelled = state->closing.load();
                    if (!cancelled)
                    {
                        state->device = remote;

                        std::weak_ptr<RemoteGattConnectionState> weak_state = state;
                        state->connection_status_token =
                            remote.ConnectionStatusChanged(
                                [shared, weak_state](
                                    const WDB::BluetoothLEDevice& sender,
                                    const WF::IInspectable&)
                                {
                                    if (const auto current = weak_state.lock())
                                        le_connection_status_changed(shared, current, sender);
                                });
                        state->connection_status_registered = true;
                    }
                }

                if (cancelled)
                {
                    try
                    {
                        remote.Close();
                    }
                    catch (...)
                    {
                    }
                    push_le_connect_result(shared, state->handle, Error::ConnectionFailed, cancelled_message());
                    return;
                }

                // Windows may drop a link no session holds once it goes idle; the
                // session keeps it for the connection's life (R1-114).
                try
                {
                    auto session = WDBG::GattSession::FromDeviceIdAsync(remote.BluetoothDeviceId()).get();
                    if (session)
                    {
                        if (session.CanMaintainConnection())
                            session.MaintainConnection(true);

                        bool keep = false;
                        {
                            std::scoped_lock lock(state->mutex);
                            keep = !state->closing.load();
                            if (keep)
                                state->session = session;
                        }
                        if (!keep)
                            session.Close();
                    }
                }
                catch (const winrt::hresult_error& error)
                {
                    GMBT_LOG("No GATT session for the LE connection: %s", winrt::to_string(error.message()).c_str());
                }

                // An uncached GATT query is intentional here. Microsoft
                // documents that creating BluetoothLEDevice alone does not
                // necessarily initiate a physical connection; an uncached
                // GATT operation does. The probe is bounded by the 15 s every
                // platform documents for a connect (R1-66).
                const auto services_query =
                    remote.GetGattServicesAsync(WDB::BluetoothCacheMode::Uncached);
                if (services_query.wait_for(std::chrono::seconds(15)) == winrt::Windows::Foundation::AsyncStatus::Started)
                {
                    services_query.Cancel();
                    fail_open(Error::Timeout, "The Bluetooth LE device did not answer within 15 seconds");
                    return;
                }
                const auto services_result = services_query.GetResults();

                if (services_result.Status() !=
                    WDBG::GattCommunicationStatus::Success)
                {
                    // A peer that never answered is a timeout; a
                    // refusal keeps its ATT meaning.
                    const auto status = services_result.Status();
                    const bool unreachable = status == WDBG::GattCommunicationStatus::Unreachable;
                    GattOutcome outcome = gatt_result_outcome(services_result);
                    if (unreachable)
                    {
                        outcome.error = Error::Timeout;
                        outcome.message = "The Bluetooth LE device did not answer";
                    }
                    else if (outcome.error == Error::OperationFailed)
                    {
                        outcome.error = Error::ConnectionFailed;
                    }

                    fail_open(outcome.error, outcome.message);
                    return;
                }

                std::unordered_map<
                    std::uint16_t,
                    std::shared_ptr<RemoteGattServiceState>> services;

                for (const auto& service : services_result.Services())
                {
                    auto service_state =
                        std::make_shared<RemoteGattServiceState>();
                    service_state->uuid =
                        guid_to_uuid_string(service.Uuid());
                    service_state->handle = service.AttributeHandle();
                    service_state->service = service;
                    services.emplace(service_state->handle, service_state);
                }

                {
                    std::scoped_lock lock(state->mutex);
                    cancelled = state->closing.load();
                    if (!cancelled)
                    {
                        state->services = std::move(services);
                        state->connected.store(true);
                    }
                }

                if (cancelled)
                {
                    // The close already ran; the services found after it are
                    // this job's.
                    for (auto& pair : services)
                    {
                        try
                        {
                            if (pair.second->service)
                                pair.second->service.Close();
                        }
                        catch (...)
                        {
                        }
                    }
                    push_le_connect_result(shared, state->handle, Error::ConnectionFailed, cancelled_message());
                    return;
                }

                push_le_connect_result(shared, state->handle, Error::Ok, std::string());
            }
            catch (const winrt::hresult_error& error)
            {
                fail_open(Error::ConnectionFailed, gatt_exception_outcome(error).message);
            }
            catch (...)
            {
                fail_open(Error::ConnectionFailed, "Opening the Bluetooth LE device failed with an unexpected exception");
            }
        }

        const char* connection_parameters_status_name(WDB::BluetoothLEPreferredConnectionParametersRequestStatus status)
        {
            switch (status)
            {
                case WDB::BluetoothLEPreferredConnectionParametersRequestStatus::Success: return "Success";
                case WDB::BluetoothLEPreferredConnectionParametersRequestStatus::DeviceNotAvailable: return "DeviceNotAvailable";
                case WDB::BluetoothLEPreferredConnectionParametersRequestStatus::AccessDenied: return "AccessDenied";
                default: return "Unspecified";
            }
        }

        // A connection priority request, on the connection's worker. priority is
        // BluetoothLeConnectionPriority. Nothing reports back to the game, so a
        // refusal is only logged; a granted request replaces the previous one,
        // which is closed only after, so the link never falls back in between.
        void request_connection_parameters(
            const std::shared_ptr<RemoteGattConnectionState>& state,
            std::int32_t priority)
        {
            WDB::BluetoothLEDevice remote{nullptr};
            {
                std::scoped_lock lock(state->mutex);
                remote = state->device;
            }
            if (!remote || state->closing.load())
                return;

            try
            {
                const auto parameters =
                    priority == 1 ? WDB::BluetoothLEPreferredConnectionParameters::ThroughputOptimized() :
                    priority == 2 ? WDB::BluetoothLEPreferredConnectionParameters::PowerOptimized() :
                                    WDB::BluetoothLEPreferredConnectionParameters::Balanced();

                auto request = remote.RequestPreferredConnectionParameters(parameters);
                const auto status = request.Status();
                GMBT_LOG(
                    "Connection priority %d for LE connection %llu: %s",
                    static_cast<int>(priority),
                    static_cast<unsigned long long>(state->handle),
                    connection_parameters_status_name(status));

                // The close may have run meanwhile; it then took nothing of this.
                WDB::BluetoothLEPreferredConnectionParametersRequest stale = request;
                if (status == WDB::BluetoothLEPreferredConnectionParametersRequestStatus::Success)
                {
                    std::scoped_lock lock(state->mutex);
                    if (!state->closing.load())
                        std::swap(stale, state->connection_parameters);
                }
                if (stale)
                    stale.Close();
            }
            catch (const winrt::hresult_error& error)
            {
                GMBT_LOG(
                    "Connection priority request for LE connection %llu failed: %s",
                    static_cast<unsigned long long>(state->handle),
                    winrt::to_string(error.message()).c_str());
            }
        }
    }

    // Closes everything the connection holds and stops its worker; the job
    // running on it, if any, finishes and the queued ones are dropped.
    void close_le_client_connection_noexcept(
        const std::shared_ptr<RemoteGattConnectionState>& state)
    {
        if (!state)
            return;

        std::unordered_map<std::uint16_t, std::shared_ptr<RemoteGattServiceState>> services;
        WDB::BluetoothLEDevice device{nullptr};
        WDBG::GattSession session{nullptr};
        WDB::BluetoothLEPreferredConnectionParametersRequest parameters{nullptr};
        winrt::event_token connection_token{};
        bool remove_connection_token = false;

        {
            std::scoped_lock lock(state->mutex);
            services.swap(state->services);
            device = state->device;
            state->device = nullptr;
            session = state->session;
            state->session = nullptr;
            parameters = state->connection_parameters;
            state->connection_parameters = nullptr;
            connection_token = state->connection_status_token;
            remove_connection_token = state->connection_status_registered;
            state->connection_status_registered = false;
            state->connection_status_token = {};
        }

        if (state->worker)
            state->worker->stop();

        for (auto& pair : services)
            close_remote_service_noexcept(state, pair.second);

        try
        {
            if (device && remove_connection_token && connection_token.value != 0)
                device.ConnectionStatusChanged(connection_token);
        }
        catch (...)
        {
        }

        try
        {
            if (parameters)
                parameters.Close();
        }
        catch (...)
        {
        }

        try
        {
            if (session)
                session.Close();
        }
        catch (...)
        {
        }

        try
        {
            if (device)
                device.Close();
        }
        catch (...)
        {
        }

        state->connected.store(false);
    }

    Error WindowsBackend::le_connect(
        std::uint64_t connection,
        const DiscoveredDevice& device,
        std::string& message)
    {
        if (!initialized_)
        {
            message = "Bluetooth backend is not initialized";
            return Error::NotInitialized;
        }
        if (!le_->ble_supported.load())
        {
            message = "This Bluetooth adapter does not support Bluetooth LE";
            return Error::NotSupported;
        }

        // The core has checked the handle and the transport (R1-67).
        if (!device.address_available || device.address.empty())
        {
            message = "Bluetooth LE device has no usable address";
            return Error::InvalidArgument;
        }

        BTH_ADDR address = 0;
        if (!parse_bluetooth_address(device.address, address))
        {
            message = "Invalid Bluetooth LE address";
            return Error::InvalidArgument;
        }

        // One worker per connection keeps its ops in order without one
        // slow peer holding up another (R1-115).
        auto state = std::make_shared<RemoteGattConnectionState>();
        state->handle = connection;
        state->worker = SerialWorker::start(workers_);
        if (!state->worker)
        {
            message = "Could not start a thread for the Bluetooth LE connection";
            return Error::OperationFailed;
        }

        {
            std::scoped_lock lock(le_client_->connections_mutex);
            le_client_->connections[connection] = state;
        }

        const auto shared = le_client_;
        const auto address_type = find_address_type(*le_, static_cast<std::uint64_t>(address));
        const bool posted = state->worker->post(
            [shared, state, address, address_type]()
            {
                open_le_connection(shared, state, static_cast<std::uint64_t>(address), address_type);
            });

        if (!posted)
        {
            state->worker->stop();
            unregister_le_client_connection(le_client_, connection);
            message = "Could not start a thread for the Bluetooth LE connection";
            return Error::OperationFailed;
        }

        message.clear();
        return Error::Ok;
    }

    Error WindowsBackend::le_disconnect(
        std::uint64_t connection,
        std::string& message)
    {
        auto state = find_le_client_connection(le_client_, connection);
        if (!state)
        {
            message = "Invalid BLE connection handle";
            return Error::InvalidHandle;
        }

        state->closing.store(true);
        close_le_client_connection_noexcept(state);
        unregister_le_client_connection(le_client_, connection);
        message.clear();
        return Error::Ok;
    }

    bool WindowsBackend::le_connection_is_connected(
        std::uint64_t connection) const
    {
        auto state = find_le_client_connection(le_client_, connection);
        return state && state->connected.load() && !state->closing.load();
    }

    // Windows negotiates the MTU when the link opens; the session the
    // connection holds says what it came to (R1-140).
    std::int32_t WindowsBackend::le_connection_mtu(
        std::uint64_t connection) const
    {
        const auto state = find_le_client_connection(le_client_, connection);
        if (!state)
            return 23;

        WDBG::GattSession session{nullptr};
        {
            std::scoped_lock lock(state->mutex);
            session = state->session;
        }

        try
        {
            if (session)
                return static_cast<std::int32_t>(session.MaxPduSize());
        }
        catch (const winrt::hresult_error&)
        {
        }
        return 23;
    }

    // Windows 11 only; the request is made on the connection's worker
    // and kept there for the link's life (R1-142).
    Error WindowsBackend::le_request_connection_priority(
        std::uint64_t connection,
        std::int32_t priority,
        std::string& message)
    {
        if (!le_->connection_parameters_api.load())
        {
            message = "Connection priority needs Windows 11";
            return Error::NotSupported;
        }

        auto state = find_le_client_connection(le_client_, connection);
        if (!state)
        {
            message = "Invalid BLE connection handle";
            return Error::InvalidHandle;
        }

        bool open = false;
        {
            std::scoped_lock lock(state->mutex);
            open = static_cast<bool>(state->device);
        }
        if (!open)
        {
            message = "BLE connection is not open";
            return Error::Disconnected;
        }

        if (!state->worker || !state->worker->post([state, priority]() { request_connection_parameters(state, priority); }))
        {
            message = "The BLE connection is closing";
            return Error::Disconnected;
        }

        message.clear();
        return Error::Ok;
    }

    // Queues body as op_id's job on the connection's worker. A WinRT or
    // any other exception completes the op with why.
    template <typename Body>
    Error WindowsBackend::post_le_op(
        const std::shared_ptr<RemoteGattConnectionState>& state,
        std::uint64_t op_id,
        std::string& message,
        Body&& body)
    {
        const auto shared = le_client_;
        auto job = [shared, op_id, body = std::forward<Body>(body)]() mutable
        {
            try
            {
                body();
            }
            catch (const winrt::hresult_error& error)
            {
                push_le_op_completion(shared, op_id, gatt_exception_outcome(error));
            }
            catch (...)
            {
                push_le_op_completion(shared, op_id, Error::OperationFailed, k_gatt_unexpected_failure);
            }
        };

        if (!state->worker || !state->worker->post(std::move(job)))
        {
            message = "The BLE connection is closing";
            return Error::Disconnected;
        }

        message.clear();
        return Error::Ok;
    }

    Error WindowsBackend::le_services_discover(
        std::uint64_t op_id,
        std::uint64_t connection,
        std::string& message)
    {
        auto state = find_le_client_connection(le_client_, connection);
        if (!state)
        {
            message = "Invalid BLE connection handle";
            return Error::InvalidHandle;
        }

        WDB::BluetoothLEDevice remote{nullptr};
        {
            std::scoped_lock lock(state->mutex);
            remote = state->device;
        }

        if (!remote)
        {
            message = "BLE connection is not open";
            return Error::Disconnected;
        }

        const auto shared = le_client_;
        return post_le_op(state, op_id, message, [shared, op_id, state, remote]()
        {
            const auto result =
                remote.GetGattServicesAsync(
                    WDB::BluetoothCacheMode::Uncached).get();

            if (result.Status() !=
                WDBG::GattCommunicationStatus::Success)
            {
                push_le_op_completion(
                    shared,
                    op_id,
                    gatt_result_outcome(result));
                return;
            }

            LeOpResult found;
            std::vector<std::shared_ptr<RemoteGattServiceState>> gone;
            {
                std::scoped_lock lock(state->mutex);

                // A service a subscription lives under keeps its state, so
                // its notifications keep coming (R1-112); any other takes
                // the fresh object. An old entry is the same service only
                // when both its handle and its UUID match (R1-127).
                std::unordered_map<
                    std::uint16_t,
                    std::shared_ptr<RemoteGattServiceState>> services;
                for (const auto& service : result.Services())
                {
                    const std::string uuid = guid_to_uuid_string(service.Uuid());
                    const std::uint16_t handle = service.AttributeHandle();
                    if (services.find(handle) != services.end())
                        continue;
                    found.attributes.push_back(LeAttribute{ uuid, handle, 0 });

                    const auto old = state->services.find(handle);
                    if (old != state->services.end() &&
                        old->second->uuid == uuid &&
                        old->second->service &&
                        (old->second->service == service || service_has_subscription_locked(*old->second)))
                    {
                        services[handle] = old->second;
                        continue;
                    }

                    auto service_state = std::make_shared<RemoteGattServiceState>();
                    service_state->uuid = uuid;
                    service_state->handle = handle;
                    service_state->service = service;
                    services[handle] = service_state;
                }

                for (const auto& [handle, old] : state->services)
                {
                    const auto kept = services.find(handle);
                    if (kept == services.end() || kept->second != old)
                        gone.push_back(old);
                }
                state->services.swap(services);
            }

            for (const auto& service : gone)
                close_remote_service_noexcept(state, service);

            state->connected.store(true);
            push_le_op_completion(
                shared,
                op_id,
                Error::Ok,
                std::string(),
                std::move(found));
        });
    }

    Error WindowsBackend::le_characteristics_discover(
        std::uint64_t op_id,
        std::uint64_t connection,
        const LeAttributeRef& attribute,
        std::string& message)
    {
        auto state = find_le_client_connection(le_client_, connection);
        auto service = find_remote_service(state, attribute.service);
        if (!state || !service || !service_object(*state, *service))
        {
            message = "BLE GATT service was not found";
            return Error::InvalidHandle;
        }

        const auto shared = le_client_;
        return post_le_op(state, op_id, message, [shared, op_id, state, service]()
        {
            const auto object = service_object(*state, *service);
            if (!object)
            {
                push_le_op_completion(shared, op_id, Error::Disconnected, k_le_attribute_closed);
                return;
            }

            const auto result =
                object.GetCharacteristicsAsync(
                    WDB::BluetoothCacheMode::Uncached).get();

            if (result.Status() !=
                WDBG::GattCommunicationStatus::Success)
            {
                const auto protocol_error = result.ProtocolError();
                GMBT_LOG(
                    "GetCharacteristicsAsync failed: status=%d protocol_error=%d",
                    static_cast<int>(result.Status()),
                    protocol_error ? static_cast<int>(protocol_error.Value()) : -1);

                push_le_op_completion(
                    shared,
                    op_id,
                    gatt_result_outcome(result));
                return;
            }

            LeOpResult found;
            std::vector<std::shared_ptr<RemoteGattCharacteristicState>> gone;
            {
                std::scoped_lock lock(state->mutex);

                // A subscribed characteristic keeps its state and its
                // ValueChanged registration (R1-112). An old entry is the
                // same characteristic only when both its handle and its
                // UUID match (R1-127).
                std::unordered_map<
                    std::uint16_t,
                    std::shared_ptr<RemoteGattCharacteristicState>> characteristics;
                for (const auto& characteristic : result.Characteristics())
                {
                    const std::string uuid = guid_to_uuid_string(characteristic.Uuid());
                    const std::uint16_t handle = characteristic.AttributeHandle();
                    if (characteristics.find(handle) != characteristics.end())
                        continue;
                    found.attributes.push_back(LeAttribute{
                        uuid,
                        handle,
                        static_cast<std::int32_t>(characteristic.CharacteristicProperties()) });

                    const auto old = service->characteristics.find(handle);
                    if (old != service->characteristics.end() &&
                        old->second->uuid == uuid &&
                        old->second->characteristic &&
                        (old->second->value_changed_registered || old->second->characteristic == characteristic))
                    {
                        characteristics[handle] = old->second;
                        continue;
                    }

                    auto characteristic_state = std::make_shared<RemoteGattCharacteristicState>();
                    characteristic_state->uuid = uuid;
                    characteristic_state->handle = handle;
                    characteristic_state->characteristic = characteristic;
                    characteristics[handle] = characteristic_state;
                }

                for (const auto& [handle, old] : service->characteristics)
                {
                    const auto kept = characteristics.find(handle);
                    if (kept == characteristics.end() || kept->second != old)
                        gone.push_back(old);
                }
                service->characteristics.swap(characteristics);
            }

            for (const auto& characteristic : gone)
                close_remote_characteristic_noexcept(state, characteristic);

            push_le_op_completion(
                shared,
                op_id,
                Error::Ok,
                std::string(),
                std::move(found));
        });
    }

    Error WindowsBackend::le_descriptors_discover(
        std::uint64_t op_id,
        std::uint64_t connection,
        const LeAttributeRef& attribute,
        std::string& message)
    {
        auto state = find_le_client_connection(le_client_, connection);
        auto characteristic = find_remote_characteristic(
            state,
            attribute.service,
            attribute.characteristic);

        if (!state || !characteristic ||
            !characteristic_object(*state, *characteristic))
        {
            message = "BLE GATT characteristic was not found";
            return Error::InvalidHandle;
        }

        const auto shared = le_client_;
        return post_le_op(state, op_id, message, [shared, op_id, state, characteristic]()
        {
            const auto object = characteristic_object(*state, *characteristic);
            if (!object)
            {
                push_le_op_completion(shared, op_id, Error::Disconnected, k_le_attribute_closed);
                return;
            }

            const auto result =
                object.GetDescriptorsAsync(
                    WDB::BluetoothCacheMode::Uncached).get();

            if (result.Status() !=
                WDBG::GattCommunicationStatus::Success)
            {
                push_le_op_completion(
                    shared,
                    op_id,
                    gatt_result_outcome(result));
                return;
            }

            std::unordered_map<
                std::uint16_t,
                std::shared_ptr<RemoteGattDescriptorState>>
                descriptors;

            LeOpResult found;

            for (const auto& descriptor : result.Descriptors())
            {
                auto descriptor_state =
                    std::make_shared<RemoteGattDescriptorState>();
                descriptor_state->uuid =
                    guid_to_uuid_string(descriptor.Uuid());
                descriptor_state->handle = descriptor.AttributeHandle();
                descriptor_state->descriptor = descriptor;
                if (!descriptors.emplace(descriptor_state->handle, descriptor_state).second)
                    continue;

                found.attributes.push_back(
                    LeAttribute{ descriptor_state->uuid, descriptor_state->handle, 0 });
            }

            {
                std::scoped_lock lock(state->mutex);
                if (characteristic->characteristic)
                    characteristic->descriptors = std::move(descriptors);
            }

            push_le_op_completion(
                shared,
                op_id,
                Error::Ok,
                std::string(),
                std::move(found));
        });
    }

    Error WindowsBackend::le_characteristic_read(
        std::uint64_t op_id,
        std::uint64_t connection,
        const LeAttributeRef& attribute,
        std::string& message)
    {
        auto state = find_le_client_connection(le_client_, connection);
        auto characteristic = find_remote_characteristic(
            state,
            attribute.service,
            attribute.characteristic);

        if (!state || !characteristic ||
            !characteristic_object(*state, *characteristic))
        {
            message = "BLE GATT characteristic was not found";
            return Error::InvalidHandle;
        }

        const auto shared = le_client_;
        return post_le_op(state, op_id, message, [shared, op_id, state, characteristic]()
        {
            const auto object = characteristic_object(*state, *characteristic);
            if (!object)
            {
                push_le_op_completion(shared, op_id, Error::Disconnected, k_le_attribute_closed);
                return;
            }

            const auto result =
                object.ReadValueAsync(
                    WDB::BluetoothCacheMode::Uncached).get();

            if (result.Status() !=
                WDBG::GattCommunicationStatus::Success)
            {
                const auto protocol_error = result.ProtocolError();
                GMBT_LOG(
                    "ReadValueAsync failed: status=%d protocol_error=%d",
                    static_cast<int>(result.Status()),
                    protocol_error ? static_cast<int>(protocol_error.Value()) : -1);

                push_le_op_completion(
                    shared,
                    op_id,
                    gatt_result_outcome(result));
                return;
            }

            LeOpResult read;
            read.value = buffer_to_bytes(result.Value());
            push_le_op_completion(
                shared,
                op_id,
                Error::Ok,
                std::string(),
                std::move(read));
        });
    }

    Error WindowsBackend::le_characteristic_write(
        std::uint64_t op_id,
        std::uint64_t connection,
        const LeAttributeRef& attribute,
        const std::vector<std::uint8_t>& value,
        bool with_response,
        std::string& message)
    {
        auto state = find_le_client_connection(le_client_, connection);
        auto characteristic = find_remote_characteristic(
            state,
            attribute.service,
            attribute.characteristic);

        if (!state || !characteristic ||
            !characteristic_object(*state, *characteristic))
        {
            message = "BLE GATT characteristic was not found";
            return Error::InvalidHandle;
        }

        const auto payload = value;
        const auto shared = le_client_;
        return post_le_op(state, op_id, message, [shared, op_id, state, characteristic, payload, with_response]()
        {
            const auto object = characteristic_object(*state, *characteristic);
            if (!object)
            {
                push_le_op_completion(shared, op_id, Error::Disconnected, k_le_attribute_closed);
                return;
            }

            const auto option = with_response
                ? WDBG::GattWriteOption::WriteWithResponse
                : WDBG::GattWriteOption::WriteWithoutResponse;

            const auto result =
                object.WriteValueWithResultAsync(
                    bytes_to_buffer(payload),
                    option).get();

            const auto status = result.Status();
            if (status != WDBG::GattCommunicationStatus::Success)
            {
                const auto protocol_error = result.ProtocolError();
                GMBT_LOG(
                    "WriteValueWithResultAsync failed: status=%d protocol_error=%d with_response=%d bytes=%zu",
                    static_cast<int>(status),
                    protocol_error ? static_cast<int>(protocol_error.Value()) : -1,
                    with_response ? 1 : 0,
                    payload.size());
            }

            push_le_op_completion(
                shared,
                op_id,
                gatt_result_outcome(result));
        });
    }

    Error WindowsBackend::le_characteristic_subscribe(
        std::uint64_t op_id,
        std::uint64_t connection,
        const LeAttributeRef& attribute,
        std::int32_t mode,
        std::string& message)
    {
        auto state = find_le_client_connection(le_client_, connection);
        auto characteristic = find_remote_characteristic(
            state,
            attribute.service,
            attribute.characteristic);

        if (!state || !characteristic ||
            !characteristic_object(*state, *characteristic))
        {
            message = "BLE GATT characteristic was not found";
            return Error::InvalidHandle;
        }

        if (mode < 0 || mode > 2)
        {
            message = "Invalid BLE subscribe mode";
            return Error::InvalidArgument;
        }

        // The notification names its characteristic by the instances the
        // core asked with (R1-127).
        const auto shared = le_client_;
        const std::uint64_t service_instance = attribute.service;
        const std::uint64_t characteristic_instance = attribute.characteristic;

        return post_le_op(state, op_id, message,
            [shared, op_id,
             state,
             characteristic,
             service_instance,
             characteristic_instance,
             mode]()
        {
            WDBG::GattCharacteristic object{nullptr};
            bool registered = false;
            {
                std::scoped_lock lock(state->mutex);
                object = characteristic->characteristic;
                registered = characteristic->value_changed_registered;
            }

            if (!object || state->closing.load())
            {
                push_le_op_completion(shared, op_id, Error::Disconnected, k_le_attribute_closed);
                return;
            }

            if (mode != 0 && !registered &&
                !register_value_changed(shared, state, characteristic, object, service_instance, characteristic_instance))
            {
                push_le_op_completion(shared, op_id, Error::Disconnected, k_le_attribute_closed);
                return;
            }

            // The WithResult form carries the ATT code of a refusal.
            const auto result =
                object.WriteClientCharacteristicConfigurationDescriptorWithResultAsync(
                    cccd_value_for_mode(mode)).get();

            if (result.Status() ==
                    WDBG::GattCommunicationStatus::Success &&
                mode == 0)
            {
                release_value_changed(*state, *characteristic, object);
            }

            push_le_op_completion(
                shared,
                op_id,
                gatt_result_outcome(result));
        });
    }

    Error WindowsBackend::le_descriptor_read(
        std::uint64_t op_id,
        std::uint64_t connection,
        const LeAttributeRef& attribute,
        std::string& message)
    {
        auto state = find_le_client_connection(le_client_, connection);
        auto descriptor = find_remote_descriptor(
            state,
            attribute.service,
            attribute.characteristic,
            attribute.descriptor);

        if (!state || !descriptor || !descriptor_object(*state, *descriptor))
        {
            message = "BLE GATT descriptor was not found";
            return Error::InvalidHandle;
        }

        const auto shared = le_client_;
        return post_le_op(state, op_id, message, [shared, op_id, state, descriptor]()
        {
            const auto object = descriptor_object(*state, *descriptor);
            if (!object)
            {
                push_le_op_completion(shared, op_id, Error::Disconnected, k_le_attribute_closed);
                return;
            }

            const auto result =
                object.ReadValueAsync(
                    WDB::BluetoothCacheMode::Uncached).get();

            if (result.Status() !=
                WDBG::GattCommunicationStatus::Success)
            {
                push_le_op_completion(
                    shared,
                    op_id,
                    gatt_result_outcome(result));
                return;
            }

            LeOpResult read;
            read.value = buffer_to_bytes(result.Value());
            push_le_op_completion(
                shared,
                op_id,
                Error::Ok,
                std::string(),
                std::move(read));
        });
    }

    Error WindowsBackend::le_descriptor_write(
        std::uint64_t op_id,
        std::uint64_t connection,
        const LeAttributeRef& attribute,
        const std::vector<std::uint8_t>& value,
        std::string& message)
    {
        auto state = find_le_client_connection(le_client_, connection);
        auto descriptor = find_remote_descriptor(
            state,
            attribute.service,
            attribute.characteristic,
            attribute.descriptor);

        if (!state || !descriptor || !descriptor_object(*state, *descriptor))
        {
            message = "BLE GATT descriptor was not found";
            return Error::InvalidHandle;
        }

        const auto payload = value;
        const auto shared = le_client_;
        return post_le_op(state, op_id, message, [shared, op_id, state, descriptor, payload]()
        {
            const auto object = descriptor_object(*state, *descriptor);
            if (!object)
            {
                push_le_op_completion(shared, op_id, Error::Disconnected, k_le_attribute_closed);
                return;
            }

            // The WithResult form carries the ATT code of a refusal.
            const auto result =
                object.WriteValueWithResultAsync(
                    bytes_to_buffer(payload)).get();

            push_le_op_completion(
                shared,
                op_id,
                gatt_result_outcome(result));
        });
    }
}

#endif
