#include "GMBluetooth_backend.h"

#if defined(_WIN32)

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <winsock2.h>
#include <ws2bth.h>
#include <windows.h>
#include <objbase.h>
#include <bthsdpdef.h>
#include <bluetoothapis.h>

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace gmbluetooth
{
    namespace WDB = winrt::Windows::Devices::Bluetooth;
    namespace WDBA = winrt::Windows::Devices::Bluetooth::Advertisement;
    namespace WDBG = winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;

    namespace
    {
        std::string format_bluetooth_address(std::uint64_t address)
        {
            char buffer[18]{};
            std::snprintf(
                buffer,
                sizeof(buffer),
                "%02X:%02X:%02X:%02X:%02X:%02X",
                static_cast<unsigned>((address >> 40) & 0xFF),
                static_cast<unsigned>((address >> 32) & 0xFF),
                static_cast<unsigned>((address >> 24) & 0xFF),
                static_cast<unsigned>((address >> 16) & 0xFF),
                static_cast<unsigned>((address >> 8) & 0xFF),
                static_cast<unsigned>(address & 0xFF));
            return buffer;
        }

        std::string wide_to_utf8(const wchar_t* text)
        {
            if (!text || !*text)
                return {};

            const int count = WideCharToMultiByte(
                CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
            if (count <= 1)
                return {};

            std::string out(static_cast<std::size_t>(count), '\0');
            WideCharToMultiByte(
                CP_UTF8, 0, text, -1, out.data(), count, nullptr, nullptr);
            if (!out.empty() && out.back() == '\0') out.pop_back();
            return out;
        }

        std::wstring utf8_to_wide(const std::string& text)
        {
            if (text.empty())
                return {};

            const int count = MultiByteToWideChar(
                CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
            if (count <= 1)
                return {};

            std::wstring out(static_cast<std::size_t>(count), L'\0');
            MultiByteToWideChar(
                CP_UTF8, 0, text.c_str(), -1, out.data(), count);
            if (!out.empty() && out.back() == L'\0') out.pop_back();
            return out;
        }

        bool parse_guid(const std::string& text, GUID& out)
        {
            std::wstring wide = utf8_to_wide(text);
            if (wide.empty())
                return false;

            // CLSIDFromString accepts both braced and non-braced UUID strings.
            return SUCCEEDED(CLSIDFromString(wide.c_str(), &out));
        }

        bool parse_bluetooth_address(const std::string& text, BTH_ADDR& out)
        {
            unsigned values[6]{};
            if (std::sscanf(
                    text.c_str(),
                    "%02x:%02x:%02x:%02x:%02x:%02x",
                    &values[0], &values[1], &values[2],
                    &values[3], &values[4], &values[5]) != 6)
            {
                return false;
            }

            out = 0;
            for (int i = 0; i < 6; ++i)
                out = (out << 8) | static_cast<BTH_ADDR>(values[i] & 0xFFU);
            return true;
        }

        Error map_bluetooth_error(WDB::BluetoothError error)
        {
            switch (error)
            {
                case WDB::BluetoothError::Success:
                    return Error::Ok;
                case WDB::BluetoothError::RadioNotAvailable:
                    return Error::BluetoothDisabled;
                case WDB::BluetoothError::DeviceNotConnected:
                    return Error::Disconnected;
                case WDB::BluetoothError::DisabledByPolicy:
                case WDB::BluetoothError::DisabledByUser:
                case WDB::BluetoothError::ConsentRequired:
                    return Error::PermissionDenied;
                case WDB::BluetoothError::TransportNotSupported:
                    return Error::NotSupported;
                default:
                    return Error::OperationFailed;
            }
        }

        std::string bluetooth_error_message(WDB::BluetoothError error)
        {
            switch (error)
            {
                case WDB::BluetoothError::Success: return {};
                case WDB::BluetoothError::RadioNotAvailable: return "Bluetooth radio is not available";
                case WDB::BluetoothError::DeviceNotConnected: return "Bluetooth device is not connected";
                case WDB::BluetoothError::DisabledByPolicy: return "Bluetooth is disabled by policy";
                case WDB::BluetoothError::DisabledByUser: return "Bluetooth is disabled by the user";
                case WDB::BluetoothError::ConsentRequired: return "Bluetooth consent is required";
                case WDB::BluetoothError::TransportNotSupported: return "Bluetooth transport is not supported";
                default: return "Windows Bluetooth operation failed";
            }
        }

        bool advertisement_is_connectable(WDBA::BluetoothLEAdvertisementType type)
        {
            return type == WDBA::BluetoothLEAdvertisementType::ConnectableUndirected ||
                   type == WDBA::BluetoothLEAdvertisementType::ConnectableDirected;
        }

        Error map_wsa_error(int error)
        {
            switch (error)
            {
                case WSAEACCES: return Error::PermissionDenied;
                case WSAETIMEDOUT: return Error::Timeout;
                case WSAENETDOWN:
                case WSAENETUNREACH:
                case WSAEHOSTUNREACH:
                    return Error::ConnectionFailed;
                case WSAECONNRESET:
                case WSAECONNABORTED:
                case WSAESHUTDOWN:
                case WSAENOTCONN:
                    return Error::Disconnected;
                case WSAEADDRINUSE: return Error::Busy;
                default: return Error::OperationFailed;
            }
        }

        std::string wsa_message(const char* operation, int error)
        {
            char* system_message = nullptr;
            const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                FORMAT_MESSAGE_FROM_SYSTEM |
                                FORMAT_MESSAGE_IGNORE_INSERTS;
            FormatMessageA(
                flags,
                nullptr,
                static_cast<DWORD>(error),
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                reinterpret_cast<LPSTR>(&system_message),
                0,
                nullptr);

            std::string out(operation ? operation : "Bluetooth socket operation");
            out += " failed (" + std::to_string(error) + ")";
            if (system_message)
            {
                out += ": ";
                out += system_message;
                LocalFree(system_message);
            }
            return out;
        }

        struct ClassicConnectionState
        {
            explicit ClassicConnectionState(std::uint64_t h, std::uint64_t d)
                : handle(h), device(d)
            {
            }

            std::uint64_t handle = 0;
            std::uint64_t device = 0;
            SOCKET socket = INVALID_SOCKET;
            std::atomic_bool connected{false};
            std::atomic_bool closing{false};
            std::mutex receive_mutex;
            std::deque<std::uint8_t> received;
            std::mutex send_mutex;
        };

        struct SharedClassicState
        {
            CoreHooks hooks;
            std::shared_ptr<std::atomic_bool> alive =
                std::make_shared<std::atomic_bool>(true);

            mutable std::mutex connections_mutex;
            std::unordered_map<std::uint64_t, std::shared_ptr<ClassicConnectionState>> connections;
        };

        std::shared_ptr<ClassicConnectionState> find_connection(
            const std::shared_ptr<SharedClassicState>& shared,
            std::uint64_t handle)
        {
            std::scoped_lock lock(shared->connections_mutex);
            const auto it = shared->connections.find(handle);
            return it != shared->connections.end() ? it->second : nullptr;
        }

        void register_connection(
            const std::shared_ptr<SharedClassicState>& shared,
            const std::shared_ptr<ClassicConnectionState>& state)
        {
            std::scoped_lock lock(shared->connections_mutex);
            shared->connections[state->handle] = state;
        }

        void start_receive_loop(
            const std::shared_ptr<SharedClassicState>& shared,
            const std::shared_ptr<ClassicConnectionState>& state)
        {
            std::thread([shared, state]()
            {
                std::vector<std::uint8_t> buffer(4096);

                while (state->connected.load() && shared->alive->load())
                {
                    const int received = recv(
                        state->socket,
                        reinterpret_cast<char*>(buffer.data()),
                        static_cast<int>(buffer.size()),
                        0);

                    if (received > 0)
                    {
                        std::int32_t available = 0;
                        {
                            std::scoped_lock lock(state->receive_mutex);
                            state->received.insert(
                                state->received.end(),
                                buffer.begin(),
                                buffer.begin() + received);
                            available = static_cast<std::int32_t>(state->received.size());
                        }

                        if (shared->alive->load() && shared->hooks.push_event)
                        {
                            BackendEvent event;
                            event.type = BackendEventType::ClassicDataAvailable;
                            event.transport = Transport::Classic;
                            event.connection = state->handle;
                            event.device = state->device;
                            event.value = available;
                            shared->hooks.push_event(std::move(event));
                        }
                        continue;
                    }

                    const int error = received == 0 ? 0 : WSAGetLastError();
                    state->connected.store(false);

                    if (shared->alive->load() && shared->hooks.push_event)
                    {
                        BackendEvent event;
                        event.type = BackendEventType::ClassicDisconnected;
                        event.transport = Transport::Classic;
                        event.connection = state->handle;
                        event.device = state->device;

                        if (state->closing.load() || received == 0)
                        {
                            event.error = Error::Ok;
                            event.message = state->closing.load()
                                ? "Disconnected"
                                : "Remote device disconnected";
                        }
                        else
                        {
                            event.error = map_wsa_error(error);
                            event.message = wsa_message("recv", error);
                        }

                        shared->hooks.push_event(std::move(event));
                    }
                    break;
                }

                if (state->socket != INVALID_SOCKET)
                {
                    closesocket(state->socket);
                    state->socket = INVALID_SOCKET;
                }
            }).detach();
        }
    }

    class WindowsBackend final : public Backend
    {
    public:
        explicit WindowsBackend(CoreHooks hooks)
            : hooks_(std::move(hooks)),
              classic_(std::make_shared<SharedClassicState>())
        {
            classic_->hooks = hooks_;
        }

        Error initialize(std::string& message) override
        {
            if (initialized_)
                return Error::Ok;

            try
            {
                winrt::init_apartment(winrt::apartment_type::multi_threaded);
                owns_apartment_ = true;
            }
            catch (const winrt::hresult_error& error)
            {
                if (error.code() != winrt::hresult{RPC_E_CHANGED_MODE})
                {
                    message = winrt::to_string(error.message());
                    return Error::OperationFailed;
                }
            }

            WSADATA wsa{};
            const int wsa_result = WSAStartup(MAKEWORD(2, 2), &wsa);
            if (wsa_result != 0)
            {
                message = wsa_message("WSAStartup", wsa_result);
                return Error::OperationFailed;
            }
            winsock_initialized_ = true;

            initialized_ = true;
            message.clear();
            return Error::Ok;
        }

        void shutdown() override
        {
            if (!initialized_)
                return;

            classic_->alive->store(false);

            std::string ignored;
            le_scan_stop(ignored);
            classic_scan_stop(ignored);
            classic_server_stop(ignored);

            std::vector<std::shared_ptr<ClassicConnectionState>> connections;
            {
                std::scoped_lock lock(classic_->connections_mutex);
                for (const auto& pair : classic_->connections)
                    connections.push_back(pair.second);
                classic_->connections.clear();
            }

            for (const auto& state : connections)
            {
                state->closing.store(true);
                state->connected.store(false);
                if (state->socket != INVALID_SOCKET)
                {
                    ::shutdown(state->socket, SD_BOTH);
                    closesocket(state->socket);
                    state->socket = INVALID_SOCKET;
                }
            }

            release_watcher_noexcept();

            if (winsock_initialized_)
            {
                WSACleanup();
                winsock_initialized_ = false;
            }

            if (owns_apartment_)
            {
                winrt::uninit_apartment();
                owns_apartment_ = false;
            }

            initialized_ = false;
        }

        bool supports_ble() const override { return true; }
        bool supports_le_advertise() const override { return true; }
        bool supports_le_server() const override { return true; }
        bool supports_classic() const override { return true; }
        bool supports_classic_server() const override { return true; }

        PermissionStatus permission_status() const override
        {
            return PermissionStatus::Granted;
        }

        Error permission_request(std::string& message) override
        {
            message.clear();
            return Error::Ok;
        }

        Error le_scan_start(bool active, std::string& message) override
        {
            if (!initialized_)
            {
                message = "Bluetooth backend is not initialized";
                return Error::NotInitialized;
            }

            if (le_scanning_.load())
            {
                message.clear();
                return Error::Ok;
            }

            try
            {
                release_watcher_noexcept();
                watcher_ = WDBA::BluetoothLEAdvertisementWatcher{};
                watcher_.ScanningMode(
                    active
                        ? WDBA::BluetoothLEScanningMode::Active
                        : WDBA::BluetoothLEScanningMode::Passive);

                received_token_ = watcher_.Received(
                    [this](
                        const WDBA::BluetoothLEAdvertisementWatcher&,
                        const WDBA::BluetoothLEAdvertisementReceivedEventArgs& args)
                    {
                        DiscoveredDevice device;
                        device.transport = Transport::LowEnergy;

                        const std::uint64_t raw_address = args.BluetoothAddress();
                        device.address = format_bluetooth_address(raw_address);
                        device.id = "win:ble:" + device.address;
                        device.address_available = true;
                        device.rssi = static_cast<std::int32_t>(args.RawSignalStrengthInDBm());
                        device.rssi_available = device.rssi != -127;
                        device.connectable = advertisement_is_connectable(args.AdvertisementType());

                        const auto local_name = args.Advertisement().LocalName();
                        if (!local_name.empty())
                            device.name = winrt::to_string(local_name);

                        if (hooks_.upsert_device)
                            hooks_.upsert_device(device);
                    });

                stopped_token_ = watcher_.Stopped(
                    [this](
                        const WDBA::BluetoothLEAdvertisementWatcher&,
                        const WDBA::BluetoothLEAdvertisementWatcherStoppedEventArgs& args)
                    {
                        le_scanning_.store(false);

                        if (hooks_.push_event)
                        {
                            BackendEvent event;
                            event.type = BackendEventType::ScanStopped;
                            event.transport = Transport::LowEnergy;
                            event.error = map_bluetooth_error(args.Error());
                            event.message = bluetooth_error_message(args.Error());
                            hooks_.push_event(std::move(event));
                        }
                    });

                le_scanning_.store(true);
                watcher_.Start();
                message.clear();
                return Error::Ok;
            }
            catch (const winrt::hresult_error& error)
            {
                le_scanning_.store(false);
                release_watcher_noexcept();
                message = winrt::to_string(error.message());
                return Error::OperationFailed;
            }
        }

        Error le_scan_stop(std::string& message) override
        {
            if (!initialized_)
                return Error::NotInitialized;

            if (!watcher_)
            {
                le_scanning_.store(false);
                message.clear();
                return Error::Ok;
            }

            try
            {
                watcher_.Stop();
                le_scanning_.store(false);
                message.clear();
                return Error::Ok;
            }
            catch (const winrt::hresult_error& error)
            {
                le_scanning_.store(false);
                message = winrt::to_string(error.message());
                return Error::OperationFailed;
            }
        }

        bool le_scan_is_running() const override
        {
            return le_scanning_.load();
        }

        // ===== BLE Advertiser =====

        Error le_advertise_start(const std::string& settings_json, const std::string& data_json, std::string& message) override
        {
            if (!initialized_)
                return Error::NotInitialized;

            if (le_advertising_.exchange(true))
            {
                message.clear();
                return Error::Ok;
            }

            try
            {
                // Create advertiser if not exists
                if (!advertiser_)
                {
                    advertiser_ = WDBA::BluetoothLEAdvertisementPublisher();
                    advertiser_.StatusChanged([this](const WDBA::BluetoothLEAdvertisementPublisher&, const WDBA::BluetoothLEAdvertisementPublisherStatusChangedEventArgs& args)
                    {
                        // Handle status changes (Started, Stopped, Aborted, etc)
                    });
                }

                // Parse and configure advertisement
                auto advertisement = advertiser_.Advertisement();

                // TODO: Parse settings_json and data_json to configure:
                // - Local name
                // - Service UUIDs
                // - Manufacturer data
                // - TX power level

                advertiser_.Start();
                message.clear();
                return Error::Ok;
            }
            catch (const winrt::hresult_error& error)
            {
                le_advertising_.store(false);
                message = winrt::to_string(error.message());
                return Error::OperationFailed;
            }
        }

        Error le_advertise_stop(std::string& message) override
        {
            if (!advertiser_)
            {
                le_advertising_.store(false);
                message.clear();
                return Error::Ok;
            }

            try
            {
                advertiser_.Stop();
                le_advertising_.store(false);
                message.clear();
                return Error::Ok;
            }
            catch (const winrt::hresult_error& error)
            {
                le_advertising_.store(false);
                message = winrt::to_string(error.message());
                return Error::OperationFailed;
            }
        }

        bool le_advertise_is_running() const override
        {
            return le_advertising_.load();
        }

        // ===== BLE GATT Server =====

        Error le_server_start(std::string& message) override
        {
            if (le_server_open_.exchange(true))
            {
                message.clear();
                return Error::Ok;
            }
            message.clear();
            return Error::Ok;
        }

        Error le_server_stop(std::string& message) override
        {
            le_server_open_.store(false);
            gatt_services_.clear();
            message.clear();
            return Error::Ok;
        }

        bool le_server_is_running() const override
        {
            return le_server_open_.load();
        }

        Error le_server_add_service(const std::string& service_json, std::string& message) override
        {
            if (!le_server_open_.load())
            {
                message = "Server is not open";
                return Error::InvalidHandle;
            }

            try
            {
                // TODO: Parse service_json and create GATT service
                // GattServiceProvider::CreateAsync(serviceUuid)
                // Add characteristics and descriptors
                // Store in gatt_services_ map

                message.clear();
                return Error::Ok;
            }
            catch (const winrt::hresult_error& error)
            {
                message = winrt::to_string(error.message());
                return Error::OperationFailed;
            }
        }

        Error le_server_clear_services(std::string& message) override
        {
            gatt_services_.clear();
            message.clear();
            return Error::Ok;
        }

        Error le_server_respond_read(std::int32_t request_id, std::int32_t status, const std::string& value_base64, std::string& message) override
        {
            // TODO: Implement read request response
            message = "Not yet implemented";
            return Error::NotSupported;
        }

        Error le_server_respond_write(std::int32_t request_id, std::int32_t status, std::string& message) override
        {
            // TODO: Implement write request response
            message = "Not yet implemented";
            return Error::NotSupported;
        }

        Error le_server_notify_value(const std::string& service_uuid, const std::string& characteristic_uuid, const std::string& value_base64, std::string& message) override
        {
            // TODO: Implement notification
            message = "Not yet implemented";
            return Error::NotSupported;
        }

        Error classic_scan_start(std::string& message) override
        {
            if (!initialized_)
                return Error::NotInitialized;

            if (classic_scanning_.exchange(true))
            {
                message.clear();
                return Error::Ok;
            }

            classic_scan_stop_requested_.store(false);
            if (classic_scan_thread_.joinable())
                classic_scan_thread_.join();

            classic_scan_thread_ = std::thread([this]()
            {
                BLUETOOTH_DEVICE_SEARCH_PARAMS search{};
                search.dwSize = sizeof(search);
                search.fReturnAuthenticated = TRUE;
                search.fReturnRemembered = TRUE;
                search.fReturnUnknown = TRUE;
                search.fReturnConnected = TRUE;
                search.fIssueInquiry = TRUE;
                search.cTimeoutMultiplier = 4; // ~5.12 seconds
                search.hRadio = nullptr;

                BLUETOOTH_DEVICE_INFO info{};
                info.dwSize = sizeof(info);

                HBLUETOOTH_DEVICE_FIND finder =
                    BluetoothFindFirstDevice(&search, &info);

                if (finder)
                {
                    do
                    {
                        if (classic_scan_stop_requested_.load())
                            break;

                        DiscoveredDevice device;
                        device.transport = Transport::Classic;
                        device.address = format_bluetooth_address(info.Address.ullLong);
                        device.id = "win:classic:" + device.address;
                        device.address_available = true;
                        device.name = wide_to_utf8(info.szName);
                        device.connectable = true;
                        device.rssi_available = false;

                        if (hooks_.upsert_device)
                            hooks_.upsert_device(device);

                        info = {};
                        info.dwSize = sizeof(info);
                    }
                    while (BluetoothFindNextDevice(finder, &info));

                    BluetoothFindDeviceClose(finder);
                }

                classic_scanning_.store(false);

                if (hooks_.push_event)
                {
                    BackendEvent event;
                    event.type = BackendEventType::ScanStopped;
                    event.transport = Transport::Classic;
                    event.error = Error::Ok;
                    hooks_.push_event(std::move(event));
                }
            });

            message.clear();
            return Error::Ok;
        }

        Error classic_scan_stop(std::string& message) override
        {
            classic_scan_stop_requested_.store(true);

            if (classic_scan_thread_.joinable())
                classic_scan_thread_.join();

            classic_scanning_.store(false);
            message.clear();
            return Error::Ok;
        }

        bool classic_scan_is_running() const override
        {
            return classic_scanning_.load();
        }

        Error classic_connect(
            std::uint64_t connection,
            const DiscoveredDevice& device,
            const std::string& service_uuid,
            std::string& message) override
        {
            if (!device.address_available)
            {
                message = "Bluetooth Classic device has no usable address";
                return Error::InvalidArgument;
            }

            BTH_ADDR address = 0;
            GUID service_guid{};
            if (!parse_bluetooth_address(device.address, address))
            {
                message = "Invalid Bluetooth address";
                return Error::InvalidArgument;
            }
            if (!parse_guid(service_uuid, service_guid))
            {
                message = "Invalid RFCOMM service UUID";
                return Error::InvalidArgument;
            }

            const auto shared = classic_;
            const auto alive = classic_->alive;
            const std::uint64_t device_handle = hooks_.upsert_device
                ? hooks_.upsert_device(device)
                : 0;

            std::thread([shared, alive, connection, device_handle, address, service_guid]()
            {
                if (!alive->load())
                    return;

                SOCKET socket_handle = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
                if (socket_handle == INVALID_SOCKET)
                {
                    const int error = WSAGetLastError();
                    if (alive->load() && shared->hooks.push_event)
                    {
                        BackendEvent event;
                        event.type = BackendEventType::ClassicConnected;
                        event.transport = Transport::Classic;
                        event.connection = connection;
                        event.device = device_handle;
                        event.error = map_wsa_error(error);
                        event.message = wsa_message("socket", error);
                        shared->hooks.push_event(std::move(event));
                    }
                    return;
                }

                SOCKADDR_BTH remote{};
                remote.addressFamily = AF_BTH;
                remote.btAddr = address;
                remote.serviceClassId = service_guid;
                remote.port = 0;

                if (connect(
                        socket_handle,
                        reinterpret_cast<const sockaddr*>(&remote),
                        sizeof(remote)) == SOCKET_ERROR)
                {
                    const int error = WSAGetLastError();
                    closesocket(socket_handle);

                    if (alive->load() && shared->hooks.push_event)
                    {
                        BackendEvent event;
                        event.type = BackendEventType::ClassicConnected;
                        event.transport = Transport::Classic;
                        event.connection = connection;
                        event.device = device_handle;
                        event.error = map_wsa_error(error);
                        event.message = wsa_message("connect", error);
                        shared->hooks.push_event(std::move(event));
                    }
                    return;
                }

                auto state = std::make_shared<ClassicConnectionState>(
                    connection, device_handle);
                state->socket = socket_handle;
                state->connected.store(true);
                register_connection(shared, state);

                if (alive->load() && shared->hooks.push_event)
                {
                    BackendEvent event;
                    event.type = BackendEventType::ClassicConnected;
                    event.transport = Transport::Classic;
                    event.connection = connection;
                    event.device = device_handle;
                    event.error = Error::Ok;
                    shared->hooks.push_event(std::move(event));
                }

                start_receive_loop(shared, state);
            }).detach();

            message.clear();
            return Error::Ok;
        }

        Error classic_disconnect(
            std::uint64_t connection,
            std::string& message) override
        {
            auto state = find_connection(classic_, connection);
            if (!state)
            {
                message = "Invalid Classic connection handle";
                return Error::InvalidHandle;
            }

            state->closing.store(true);
            state->connected.store(false);

            if (state->socket != INVALID_SOCKET)
                ::shutdown(state->socket, SD_BOTH);

            message.clear();
            return Error::Ok;
        }

        bool classic_connection_is_connected(std::uint64_t connection) const override
        {
            auto state = find_connection(classic_, connection);
            return state && state->connected.load();
        }

        std::int32_t classic_receive_available(std::uint64_t connection) const override
        {
            auto state = find_connection(classic_, connection);
            if (!state)
                return 0;

            std::scoped_lock lock(state->receive_mutex);
            return static_cast<std::int32_t>(state->received.size());
        }

        Error classic_send_bytes(
            std::uint64_t connection,
            const std::uint8_t* data,
            std::size_t size,
            std::string& message) override
        {
            auto state = find_connection(classic_, connection);
            if (!state || !state->connected.load())
            {
                message = "Classic connection is not connected";
                return Error::Disconnected;
            }
            if (!data && size != 0)
                return Error::InvalidArgument;

            std::scoped_lock lock(state->send_mutex);
            std::size_t sent_total = 0;
            while (sent_total < size)
            {
                const int sent = send(
                    state->socket,
                    reinterpret_cast<const char*>(data + sent_total),
                    static_cast<int>(size - sent_total),
                    0);
                if (sent <= 0)
                {
                    const int error = WSAGetLastError();
                    message = wsa_message("send", error);
                    return map_wsa_error(error);
                }
                sent_total += static_cast<std::size_t>(sent);
            }

            message.clear();
            return Error::Ok;
        }

        std::size_t classic_receive_bytes(
            std::uint64_t connection,
            std::uint8_t* out,
            std::size_t max_size) override
        {
            auto state = find_connection(classic_, connection);
            if (!state || !out || max_size == 0)
                return 0;

            std::scoped_lock lock(state->receive_mutex);
            const std::size_t count = std::min(max_size, state->received.size());
            for (std::size_t i = 0; i < count; ++i)
            {
                out[i] = state->received.front();
                state->received.pop_front();
            }
            return count;
        }

        Error classic_server_start(
            const std::string& name,
            const std::string& service_uuid,
            std::string& message) override
        {
            if (classic_server_running_.load())
            {
                message.clear();
                return Error::Ok;
            }

            GUID guid{};
            if (!parse_guid(service_uuid, guid))
            {
                message = "Invalid RFCOMM service UUID";
                return Error::InvalidArgument;
            }

            SOCKET listener = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
            if (listener == INVALID_SOCKET)
            {
                const int error = WSAGetLastError();
                message = wsa_message("socket", error);
                return map_wsa_error(error);
            }

            SOCKADDR_BTH local{};
            local.addressFamily = AF_BTH;
            local.btAddr = 0;
            local.serviceClassId = GUID_NULL;
            local.port = BT_PORT_ANY;

            if (bind(
                    listener,
                    reinterpret_cast<const sockaddr*>(&local),
                    sizeof(local)) == SOCKET_ERROR)
            {
                const int error = WSAGetLastError();
                closesocket(listener);
                message = wsa_message("bind", error);
                return map_wsa_error(error);
            }

            int local_len = sizeof(local);
            if (getsockname(
                    listener,
                    reinterpret_cast<sockaddr*>(&local),
                    &local_len) == SOCKET_ERROR)
            {
                const int error = WSAGetLastError();
                closesocket(listener);
                message = wsa_message("getsockname", error);
                return map_wsa_error(error);
            }

            if (listen(listener, 4) == SOCKET_ERROR)
            {
                const int error = WSAGetLastError();
                closesocket(listener);
                message = wsa_message("listen", error);
                return map_wsa_error(error);
            }

            server_socket_ = listener;
            server_guid_ = guid;
            server_addr_ = local;
            server_name_ = utf8_to_wide(name.empty() ? "GMBluetooth RFCOMM" : name);
            server_comment_ = L"GMBluetooth RFCOMM service";

            server_csaddr_ = {};
            server_csaddr_.LocalAddr.iSockaddrLength = sizeof(SOCKADDR_BTH);
            server_csaddr_.LocalAddr.lpSockaddr =
                reinterpret_cast<LPSOCKADDR>(&server_addr_);
            server_csaddr_.RemoteAddr.iSockaddrLength = sizeof(SOCKADDR_BTH);
            server_csaddr_.RemoteAddr.lpSockaddr =
                reinterpret_cast<LPSOCKADDR>(&server_addr_);
            server_csaddr_.iSocketType = SOCK_STREAM;
            server_csaddr_.iProtocol = BTHPROTO_RFCOMM;

            server_query_ = {};
            server_query_.dwSize = sizeof(WSAQUERYSETW);
            server_query_.lpServiceClassId = &server_guid_;
            server_query_.lpszServiceInstanceName = server_name_.data();
            server_query_.lpszComment = server_comment_.data();
            server_query_.dwNameSpace = NS_BTH;
            server_query_.dwNumberOfCsAddrs = 1;
            server_query_.lpcsaBuffer = &server_csaddr_;

            if (WSASetServiceW(&server_query_, RNRSERVICE_REGISTER, 0) == SOCKET_ERROR)
            {
                const int error = WSAGetLastError();
                closesocket(server_socket_);
                server_socket_ = INVALID_SOCKET;
                message = wsa_message("WSASetService", error);
                return map_wsa_error(error);
            }

            server_registered_ = true;
            classic_server_running_.store(true);

            if (server_thread_.joinable())
                server_thread_.join();

            server_thread_ = std::thread([this]()
            {
                while (classic_server_running_.load())
                {
                    SOCKADDR_BTH remote{};
                    int remote_len = sizeof(remote);
                    SOCKET client = accept(
                        server_socket_,
                        reinterpret_cast<sockaddr*>(&remote),
                        &remote_len);

                    if (client == INVALID_SOCKET)
                    {
                        if (!classic_server_running_.load())
                            break;
                        continue;
                    }

                    DiscoveredDevice device;
                    device.transport = Transport::Classic;
                    device.address = format_bluetooth_address(remote.btAddr);
                    device.id = "win:classic:" + device.address;
                    device.address_available = true;
                    device.connectable = true;

                    const std::uint64_t device_handle = hooks_.upsert_device
                        ? hooks_.upsert_device(device)
                        : 0;
                    const std::uint64_t connection = hooks_.create_classic_connection
                        ? hooks_.create_classic_connection(device_handle)
                        : 0;

                    if (!connection)
                    {
                        closesocket(client);
                        continue;
                    }

                    auto state = std::make_shared<ClassicConnectionState>(
                        connection, device_handle);
                    state->socket = client;
                    state->connected.store(true);
                    register_connection(classic_, state);

                    if (hooks_.push_event)
                    {
                        BackendEvent event;
                        event.type = BackendEventType::ClassicClientConnected;
                        event.transport = Transport::Classic;
                        event.connection = connection;
                        event.device = device_handle;
                        event.error = Error::Ok;
                        hooks_.push_event(std::move(event));
                    }

                    start_receive_loop(classic_, state);
                }
            });

            message.clear();
            return Error::Ok;
        }

        Error classic_server_stop(std::string& message) override
        {
            classic_server_running_.store(false);

            if (server_registered_)
            {
                WSASetServiceW(&server_query_, RNRSERVICE_DELETE, 0);
                server_registered_ = false;
            }

            if (server_socket_ != INVALID_SOCKET)
            {
                closesocket(server_socket_);
                server_socket_ = INVALID_SOCKET;
            }

            if (server_thread_.joinable())
                server_thread_.join();

            message.clear();
            return Error::Ok;
        }

        bool classic_server_is_running() const override
        {
            return classic_server_running_.load();
        }

        ~WindowsBackend() override
        {
            if (initialized_)
                shutdown();
        }

    private:
        void release_watcher_noexcept() noexcept
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

        CoreHooks hooks_;
        std::shared_ptr<SharedClassicState> classic_;

        bool initialized_ = false;
        bool owns_apartment_ = false;
        bool winsock_initialized_ = false;

        std::atomic_bool le_scanning_{false};
        WDBA::BluetoothLEAdvertisementWatcher watcher_{nullptr};
        winrt::event_token received_token_{};
        winrt::event_token stopped_token_{};

        std::atomic_bool le_advertising_{false};
        WDBA::BluetoothLEAdvertisementPublisher advertiser_{nullptr};
        winrt::event_token advertiser_status_token_{};

        std::atomic_bool le_server_open_{false};
        WDBG::GattServiceProvider gatt_service_provider_{nullptr};
        std::unordered_map<std::string, WDBG::GattServiceProvider> gatt_services_;

        std::atomic_bool classic_scanning_{false};
        std::atomic_bool classic_scan_stop_requested_{false};
        std::thread classic_scan_thread_;

        std::atomic_bool classic_server_running_{false};
        SOCKET server_socket_ = INVALID_SOCKET;
        std::thread server_thread_;
        bool server_registered_ = false;
        GUID server_guid_{};
        SOCKADDR_BTH server_addr_{};
        CSADDR_INFO server_csaddr_{};
        WSAQUERYSETW server_query_{};
        std::wstring server_name_;
        std::wstring server_comment_;
    };

    std::unique_ptr<Backend> create_platform_backend(CoreHooks hooks)
    {
        return std::make_unique<WindowsBackend>(std::move(hooks));
    }
}

#endif
