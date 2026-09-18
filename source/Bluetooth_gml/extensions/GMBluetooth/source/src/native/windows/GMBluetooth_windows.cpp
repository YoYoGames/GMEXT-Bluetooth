#include "GMBluetooth_backend.h"
#include "GMBluetooth_json.h"

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
#include <winrt/Windows.Devices.Radios.h>
#include <winrt/Windows.Storage.Streams.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <deque>
#include <memory>
#include <optional>
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
    namespace WDR = winrt::Windows::Devices::Radios;
    namespace WSS = winrt::Windows::Storage::Streams;

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
            if (text.empty())
                return false;

            // CLSIDFromString requires braces (verified on Windows 11 10.0.26200:
            // an unbraced "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx" string returns
            // CO_E_CLASSSTRING). Normalize to the braced form before parsing so
            // callers can pass either format.
            const std::string braced = (text.front() == '{')
                ? text
                : "{" + text + "}";

            std::wstring wide = utf8_to_wide(braced);
            if (wide.empty())
                return false;

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


        std::string normalize_uuid(std::string value)
        {
            value.erase(
                std::remove_if(value.begin(), value.end(), [](unsigned char c)
                {
                    return std::isspace(c) != 0 || c == '{' || c == '}';
                }),
                value.end());
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });
            return value;
        }

        bool parse_winrt_guid(const std::string& text, winrt::guid& out)
        {
            GUID guid{};
            if (!parse_guid(text, guid))
                return false;

            out = winrt::guid{
                guid.Data1,
                guid.Data2,
                guid.Data3,
                {
                    guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
                    guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]
                }};
            return true;
        }

        WSS::IBuffer bytes_to_buffer(const std::vector<std::uint8_t>& bytes)
        {
            WSS::DataWriter writer;
            if (!bytes.empty())
                writer.WriteBytes(bytes);
            return writer.DetachBuffer();
        }

        std::vector<std::uint8_t> buffer_to_bytes(const WSS::IBuffer& buffer)
        {
            if (!buffer)
                return {};

            WSS::DataReader reader = WSS::DataReader::FromBuffer(buffer);
            std::vector<std::uint8_t> bytes(reader.UnconsumedBufferLength());
            if (!bytes.empty())
                reader.ReadBytes(bytes);
            return bytes;
        }

        std::string json_escape(std::string_view value)
        {
            std::string out;
            out.reserve(value.size() + 8);
            for (const unsigned char c : value)
            {
                switch (c)
                {
                    case '\\': out += "\\\\"; break;
                    case '"': out += "\\\""; break;
                    case '\b': out += "\\b"; break;
                    case '\f': out += "\\f"; break;
                    case '\n': out += "\\n"; break;
                    case '\r': out += "\\r"; break;
                    case '\t': out += "\\t"; break;
                    default:
                        if (c < 0x20)
                        {
                            char escaped[7]{};
                            std::snprintf(escaped, sizeof(escaped), "\\u%04x", c);
                            out += escaped;
                        }
                        else
                        {
                            out.push_back(static_cast<char>(c));
                        }
                        break;
                }
            }
            return out;
        }

        std::string make_server_request_json(
            std::int32_t request_id,
            const std::string& service_uuid,
            const std::string& characteristic_uuid,
            const std::string& descriptor_uuid,
            std::uint32_t offset,
            const std::vector<std::uint8_t>* value)
        {
            std::string out = "{\"request_id\":" + std::to_string(request_id) +
                ",\"service_uuid\":\"" + json_escape(service_uuid) +
                "\",\"characteristic_uuid\":\"" + json_escape(characteristic_uuid) +
                "\",\"descriptor_uuid\":\"" + json_escape(descriptor_uuid) +
                "\",\"offset\":" + std::to_string(offset);

            if (value)
            {
                out += ",\"value\":\"";
                out += json::base64_encode(value->data(), value->size());
                out += "\"";
            }

            out += "}";
            return out;
        }

        WDBG::GattProtectionLevel read_protection_from_permissions(std::int32_t permissions)
        {
            // Android-compatible permission bits retained by the public API:
            // READ=1, READ_ENCRYPTED=2, READ_ENCRYPTED_MITM=4.
            if ((permissions & 4) != 0)
                return WDBG::GattProtectionLevel::EncryptionAndAuthenticationRequired;
            if ((permissions & 2) != 0)
                return WDBG::GattProtectionLevel::EncryptionRequired;
            return WDBG::GattProtectionLevel::Plain;
        }

        WDBG::GattProtectionLevel write_protection_from_permissions(std::int32_t permissions)
        {
            // WRITE=16, WRITE_ENCRYPTED=32, WRITE_ENCRYPTED_MITM=64,
            // WRITE_SIGNED=128, WRITE_SIGNED_MITM=256.
            if ((permissions & (64 | 256)) != 0)
                return WDBG::GattProtectionLevel::EncryptionAndAuthenticationRequired;
            if ((permissions & 32) != 0)
                return WDBG::GattProtectionLevel::EncryptionRequired;
            if ((permissions & 128) != 0)
                return WDBG::GattProtectionLevel::AuthenticationRequired;
            return WDBG::GattProtectionLevel::Plain;
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

        struct LocalGattDescriptorState
        {
            WDBG::GattLocalDescriptor descriptor{nullptr};
            winrt::event_token read_token{};
            winrt::event_token write_token{};
        };

        struct LocalGattCharacteristicState
        {
            std::string service_uuid;
            std::string characteristic_uuid;
            WDBG::GattLocalCharacteristic characteristic{nullptr};
            winrt::event_token read_token{};
            winrt::event_token write_token{};
            std::vector<std::shared_ptr<LocalGattDescriptorState>> descriptors;
        };

        struct LocalGattServiceState
        {
            std::string uuid;
            WDBG::GattServiceProvider provider{nullptr};
            std::unordered_map<std::string, std::shared_ptr<LocalGattCharacteristicState>> characteristics;
        };

        struct PendingGattRead
        {
            WDBG::GattReadRequest request{nullptr};
        };

        struct PendingGattWrite
        {
            WDBG::GattWriteRequest request{nullptr};
            bool with_response = true;
        };

        struct ClassicConnectionState
        {
            explicit ClassicConnectionState(std::uint64_t h, std::uint64_t d)
                : handle(h), device(d)
            {
            }

            std::uint64_t handle = 0;
            std::uint64_t device = 0;
            std::mutex socket_mutex;
            SOCKET socket = INVALID_SOCKET;
            std::atomic_bool connected{false};
            std::atomic_bool closing{false};
            std::mutex receive_mutex;
            std::deque<std::uint8_t> received;
            std::mutex send_mutex;
        };

        // The receive loop thread is the sole owner of closesocket() for a
        // connection: it only closes after its own blocking recv() has
        // returned, so the handle can never be reused while still in flight.
        // Every other caller (classic_disconnect, WindowsBackend::shutdown)
        // must only ever request a shutdown() to unblock that recv() -
        // never close the socket directly - or the handle can be double
        // closed / reused out from under a concurrent send().
        SOCKET connection_socket(const std::shared_ptr<ClassicConnectionState>& state)
        {
            std::scoped_lock lock(state->socket_mutex);
            return state->socket;
        }

        void connection_request_shutdown(const std::shared_ptr<ClassicConnectionState>& state)
        {
            std::scoped_lock lock(state->socket_mutex);
            if (state->socket != INVALID_SOCKET)
                ::shutdown(state->socket, SD_BOTH);
        }

        void connection_close(const std::shared_ptr<ClassicConnectionState>& state)
        {
            SOCKET s = INVALID_SOCKET;
            {
                std::scoped_lock lock(state->socket_mutex);
                s = state->socket;
                state->socket = INVALID_SOCKET;
            }
            if (s != INVALID_SOCKET)
                closesocket(s);
        }

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

        // Returns false (and inserts nothing) once the backend has started
        // shutting down. Sharing connections_mutex with the alive flip in
        // WindowsBackend::shutdown() closes the race where a connect() that
        // is completing concurrently with shutdown() could register a
        // socket/receive-loop thread that nothing will ever ask to
        // shutdown() again, leaking a blocked-forever thread.
        bool register_connection_if_alive(
            const std::shared_ptr<SharedClassicState>& shared,
            const std::shared_ptr<ClassicConnectionState>& state)
        {
            std::scoped_lock lock(shared->connections_mutex);
            if (!shared->alive->load())
                return false;
            shared->connections[state->handle] = state;
            return true;
        }

        void unregister_connection(
            const std::shared_ptr<SharedClassicState>& shared,
            std::uint64_t handle)
        {
            std::scoped_lock lock(shared->connections_mutex);
            shared->connections.erase(handle);
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
                        connection_socket(state),
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

                connection_close(state);
                unregister_connection(shared, state->handle);
            }).detach();
        }
    }


    struct RemoteGattDescriptorState
    {
        std::string uuid;
        WDBG::GattDescriptor descriptor{nullptr};
    };

    struct RemoteGattCharacteristicState
    {
        std::string uuid;
        WDBG::GattCharacteristic characteristic{nullptr};
        winrt::event_token value_changed_token{};
        bool value_changed_registered = false;
        std::unordered_map<std::string, std::shared_ptr<RemoteGattDescriptorState>> descriptors;
    };

    struct RemoteGattServiceState
    {
        std::string uuid;
        WDBG::GattDeviceService service{nullptr};
        std::unordered_map<std::string, std::shared_ptr<RemoteGattCharacteristicState>> characteristics;
    };

    struct RemoteGattConnectionState
    {
        std::uint64_t handle = 0;
        std::uint64_t device_handle = 0;
        std::atomic_bool connected{false};
        std::atomic_bool closing{false};
        WDB::BluetoothLEDevice device{nullptr};
        winrt::event_token connection_status_token{};
        bool connection_status_registered = false;
        mutable std::mutex mutex;
        std::mutex operation_mutex;
        std::unordered_map<std::string, std::shared_ptr<RemoteGattServiceState>> services;
    };

    struct SharedLeClientState
    {
        CoreHooks hooks;
        std::shared_ptr<std::atomic_bool> alive =
            std::make_shared<std::atomic_bool>(true);

        mutable std::mutex connections_mutex;
        std::unordered_map<std::uint64_t, std::shared_ptr<RemoteGattConnectionState>> connections;
    };

    struct WinrtWorkerApartment
    {
        bool owns = false;

        WinrtWorkerApartment()
        {
            try
            {
                winrt::init_apartment(winrt::apartment_type::multi_threaded);
                owns = true;
            }
            catch (const winrt::hresult_error& error)
            {
                if (error.code() != winrt::hresult{RPC_E_CHANGED_MODE})
                    throw;
            }
        }

        ~WinrtWorkerApartment()
        {
            if (owns)
            {
                try
                {
                    winrt::uninit_apartment();
                }
                catch (...)
                {
                }
            }
        }
    };

    std::string guid_to_uuid_string(const winrt::guid& value)
    {
        char buffer[37]{};
        std::snprintf(
            buffer,
            sizeof(buffer),
            "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            static_cast<unsigned>(value.Data1),
            static_cast<unsigned>(value.Data2),
            static_cast<unsigned>(value.Data3),
            static_cast<unsigned>(value.Data4[0]),
            static_cast<unsigned>(value.Data4[1]),
            static_cast<unsigned>(value.Data4[2]),
            static_cast<unsigned>(value.Data4[3]),
            static_cast<unsigned>(value.Data4[4]),
            static_cast<unsigned>(value.Data4[5]),
            static_cast<unsigned>(value.Data4[6]),
            static_cast<unsigned>(value.Data4[7]));
        return std::string(buffer);
    }

    Error map_gatt_status(WDBG::GattCommunicationStatus status)
    {
        switch (status)
        {
            case WDBG::GattCommunicationStatus::Success:
                return Error::Ok;
            case WDBG::GattCommunicationStatus::Unreachable:
                return Error::Disconnected;
            case WDBG::GattCommunicationStatus::AccessDenied:
                return Error::PermissionDenied;
            case WDBG::GattCommunicationStatus::ProtocolError:
            default:
                return Error::OperationFailed;
        }
    }

    std::string gatt_status_message(WDBG::GattCommunicationStatus status)
    {
        switch (status)
        {
            case WDBG::GattCommunicationStatus::Success:
                return {};
            case WDBG::GattCommunicationStatus::Unreachable:
                return "Windows GATT peer is unreachable";
            case WDBG::GattCommunicationStatus::AccessDenied:
                return "Windows denied access to the GATT operation";
            case WDBG::GattCommunicationStatus::ProtocolError:
                return "Windows GATT protocol error";
            default:
                return "Windows GATT operation failed";
        }
    }

    void push_le_client_event(
        const std::shared_ptr<SharedLeClientState>& shared,
        std::string event_type,
        std::string json)
    {
        if (!shared || !shared->alive->load() || !shared->hooks.push_event)
            return;

        BackendEvent event;
        event.type = BackendEventType::LeEvent;
        event.transport = Transport::LowEnergy;
        event.event_type = std::move(event_type);
        event.json = std::move(json);
        shared->hooks.push_event(std::move(event));
    }

    std::string le_error_json(
        std::int32_t error_code,
        std::uint64_t connection = 0)
    {
        std::string out = "{";
        if (connection != 0)
            out += "\"connection\":" + std::to_string(connection) + ",";
        out += "\"error_code\":" + std::to_string(error_code) + "}";
        return out;
    }

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

    std::shared_ptr<RemoteGattServiceState> find_remote_service(
        const std::shared_ptr<RemoteGattConnectionState>& connection,
        const std::string& uuid)
    {
        if (!connection)
            return {};

        const std::string key = normalize_uuid(uuid);
        std::scoped_lock lock(connection->mutex);
        const auto it = connection->services.find(key);
        return it != connection->services.end() ? it->second : nullptr;
    }

    std::shared_ptr<RemoteGattCharacteristicState> find_remote_characteristic(
        const std::shared_ptr<RemoteGattConnectionState>& connection,
        const std::string& service_uuid,
        const std::string& characteristic_uuid)
    {
        auto service = find_remote_service(connection, service_uuid);
        if (!service)
            return {};

        const std::string key = normalize_uuid(characteristic_uuid);
        std::scoped_lock lock(connection->mutex);
        const auto it = service->characteristics.find(key);
        return it != service->characteristics.end() ? it->second : nullptr;
    }

    std::shared_ptr<RemoteGattDescriptorState> find_remote_descriptor(
        const std::shared_ptr<RemoteGattConnectionState>& connection,
        const std::string& service_uuid,
        const std::string& characteristic_uuid,
        const std::string& descriptor_uuid)
    {
        auto characteristic = find_remote_characteristic(
            connection,
            service_uuid,
            characteristic_uuid);
        if (!characteristic)
            return {};

        const std::string key = normalize_uuid(descriptor_uuid);
        std::scoped_lock lock(connection->mutex);
        const auto it = characteristic->descriptors.find(key);
        return it != characteristic->descriptors.end() ? it->second : nullptr;
    }

    void close_remote_characteristic_noexcept(
        const std::shared_ptr<RemoteGattCharacteristicState>& characteristic)
    {
        if (!characteristic)
            return;

        try
        {
            if (characteristic->characteristic &&
                characteristic->value_changed_registered &&
                characteristic->value_changed_token.value != 0)
            {
                characteristic->characteristic.ValueChanged(
                    characteristic->value_changed_token);
            }
        }
        catch (...)
        {
        }

        characteristic->value_changed_token = {};
        characteristic->value_changed_registered = false;
        characteristic->descriptors.clear();
        characteristic->characteristic = nullptr;
    }

    void close_remote_service_noexcept(
        const std::shared_ptr<RemoteGattServiceState>& service)
    {
        if (!service)
            return;

        for (auto& pair : service->characteristics)
            close_remote_characteristic_noexcept(pair.second);

        service->characteristics.clear();

        try
        {
            if (service->service)
                service->service.Close();
        }
        catch (...)
        {
        }

        service->service = nullptr;
    }

    void close_le_client_connection_noexcept(
        const std::shared_ptr<RemoteGattConnectionState>& state)
    {
        if (!state)
            return;

        std::unordered_map<std::string, std::shared_ptr<RemoteGattServiceState>> services;
        WDB::BluetoothLEDevice device{nullptr};
        winrt::event_token connection_token{};
        bool remove_connection_token = false;

        {
            std::scoped_lock lock(state->mutex);
            services.swap(state->services);
            device = state->device;
            state->device = nullptr;
            connection_token = state->connection_status_token;
            remove_connection_token = state->connection_status_registered;
            state->connection_status_registered = false;
            state->connection_status_token = {};
        }

        for (auto& pair : services)
            close_remote_service_noexcept(pair.second);

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
            if (device)
                device.Close();
        }
        catch (...)
        {
        }

        state->connected.store(false);
    }

    std::string make_nested_json_array_field(
        const char* field_name,
        const std::string& array_json)
    {
        return "{\"" + std::string(field_name ? field_name : "") +
            "\":\"" + json_escape(array_json) + "\"}";
    }


    class WindowsBackend final : public Backend
    {
    public:
        explicit WindowsBackend(CoreHooks hooks)
            : hooks_(std::move(hooks)),
              classic_(std::make_shared<SharedClassicState>()),
              le_client_(std::make_shared<SharedLeClientState>())
        {
            classic_->hooks = hooks_;
            le_client_->hooks = hooks_;
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

            try
            {
                bluetooth_adapter_ = WDB::BluetoothAdapter::GetDefaultAsync().get();
                if (bluetooth_adapter_)
                {
                    ble_supported_ = bluetooth_adapter_.IsLowEnergySupported();
                    le_peripheral_supported_ = bluetooth_adapter_.IsPeripheralRoleSupported();
                    bluetooth_radio_ = bluetooth_adapter_.GetRadioAsync().get();
                    if (bluetooth_radio_)
                    {
                        bluetooth_state_.store(normalized_radio_state(bluetooth_radio_.State()));
                        radio_state_token_ = bluetooth_radio_.StateChanged(
                            [this](const WDR::Radio& radio, const winrt::Windows::Foundation::IInspectable&)
                            {
                                const std::int32_t state = normalized_radio_state(radio.State());
                                bluetooth_state_.store(state);

                                if (hooks_.push_event)
                                {
                                    BackendEvent event;
                                    event.type = BackendEventType::LeEvent;
                                    event.transport = Transport::Unknown;
                                    event.event_type = "bluetooth_state_changed";
                                    event.json = "{\"state\":" + std::to_string(state) + "}";
                                    hooks_.push_event(std::move(event));
                                }
                            });
                    }
                    else
                    {
                        bluetooth_state_.store(0);
                    }
                }
                else
                {
                    ble_supported_ = false;
                    le_peripheral_supported_ = false;
                    bluetooth_state_.store(2); // Unsupported
                }
            }
            catch (...)
            {
                // Bluetooth Classic still works through the Win32 APIs even if
                // the WinRT adapter query is unavailable. Keep initialization alive.
                bluetooth_adapter_ = nullptr;
                bluetooth_radio_ = nullptr;
                ble_supported_ = true;
                le_peripheral_supported_ = true;
                bluetooth_state_.store(0);
            }

            initialized_ = true;
            message.clear();
            return Error::Ok;
        }

        void shutdown() override
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

            for (const auto& state : le_connections)
            {
                state->closing.store(true);
                close_le_client_connection_noexcept(state);
            }

            std::string ignored;
            le_scan_stop(ignored);
            le_advertise_stop(ignored);
            le_server_stop(ignored);
            classic_scan_stop(ignored);
            classic_server_stop(ignored);
            classic_discoverable_stop(ignored);

            // Only request a shutdown() here - never close the socket
            // directly. Each connection's own receive-loop thread is the
            // sole owner of closesocket() (and of erasing itself from the
            // map); it is kept alive by the shared_ptr it captured even
            // after this WindowsBackend is destroyed, so it is safe to let
            // it finish asynchronously instead of racing it here.
            for (const auto& state : connections)
            {
                state->closing.store(true);
                state->connected.store(false);
                connection_request_shutdown(state);
            }

            // Each receive-loop thread closes its own socket asynchronously
            // once its recv() unblocks. Give them a bounded window to do
            // that (and unregister themselves) before WSACleanup() runs -
            // tearing down Winsock while another thread still has a socket
            // call in flight is undefined behavior.
            if (!connections.empty())
            {
                const auto deadline =
                    std::chrono::steady_clock::now() + std::chrono::seconds(2);
                for (;;)
                {
                    bool empty = false;
                    {
                        std::scoped_lock lock(classic_->connections_mutex);
                        empty = classic_->connections.empty();
                    }
                    if (empty || std::chrono::steady_clock::now() >= deadline)
                        break;
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }

            release_watcher_noexcept();
            release_radio_noexcept();

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

        bool supports_ble() const override { return ble_supported_; }
        bool supports_le_advertise() const override { return ble_supported_ && le_peripheral_supported_; }
        bool supports_le_server() const override { return ble_supported_ && le_peripheral_supported_; }
        bool supports_classic() const override { return true; }
        bool supports_classic_server() const override { return true; }

        // Kept platform-local for now. The shared Backend interface can expose
        // this directly in the later common-source step.
        bool pairing_is_supported(const DiscoveredDevice& device) const
        {
            return device.transport == Transport::Classic;
        }

        std::int32_t current_bluetooth_state() const
        {
            return bluetooth_state_.load();
        }

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

        Error le_connect(
            std::uint64_t connection,
            const DiscoveredDevice& device,
            std::string& message) override
        {
            if (!initialized_)
            {
                message = "Bluetooth backend is not initialized";
                return Error::NotInitialized;
            }

            if (device.transport != Transport::LowEnergy)
            {
                message = "Expected a Bluetooth LE device";
                return Error::InvalidArgument;
            }

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

            auto state = std::make_shared<RemoteGattConnectionState>();
            state->handle = connection;

            {
                std::scoped_lock lock(le_client_->connections_mutex);
                le_client_->connections[connection] = state;
            }

            const auto shared = le_client_;

            std::thread(
                [shared, state, address]()
                {
                    try
                    {
                        WinrtWorkerApartment apartment;

                        if (!shared->alive->load() || state->closing.load())
                            return;

                        auto remote =
                            WDB::BluetoothLEDevice::FromBluetoothAddressAsync(
                                static_cast<std::uint64_t>(address)).get();

                        if (!remote)
                        {
                            unregister_le_client_connection(shared, state->handle);
                            push_le_client_event(
                                shared,
                                "bluetooth_le_peripheral_open",
                                le_error_json(1, state->handle));
                            return;
                        }

                        {
                            std::scoped_lock lock(state->mutex);
                            state->device = remote;
                        }

                        std::weak_ptr<RemoteGattConnectionState> weak_state = state;
                        state->connection_status_token =
                            remote.ConnectionStatusChanged(
                                [shared, weak_state](
                                    const WDB::BluetoothLEDevice& sender,
                                    const winrt::Windows::Foundation::IInspectable&)
                                {
                                    auto current = weak_state.lock();
                                    if (!current || !shared->alive->load())
                                        return;

                                    const bool connected =
                                        sender.ConnectionStatus() ==
                                        WDB::BluetoothConnectionStatus::Connected;

                                    const bool was_connected =
                                        current->connected.exchange(connected);

                                    if (!connected &&
                                        was_connected &&
                                        !current->closing.load())
                                    {
                                        push_le_client_event(
                                            shared,
                                            "bluetooth_le_peripheral_connection_state_changed",
                                            "{\"connection\":" +
                                                std::to_string(current->handle) +
                                                ",\"is_connected\":false}");
                                    }
                                });
                        state->connection_status_registered = true;

                        // An uncached GATT query is intentional here. Microsoft
                        // documents that creating BluetoothLEDevice alone does not
                        // necessarily initiate a physical connection; an uncached
                        // GATT operation does.
                        const auto services_result =
                            remote.GetGattServicesAsync(
                                WDB::BluetoothCacheMode::Uncached).get();

                        if (services_result.Status() !=
                            WDBG::GattCommunicationStatus::Success)
                        {
                            const auto status = services_result.Status();
                            state->closing.store(true);
                            close_le_client_connection_noexcept(state);
                            unregister_le_client_connection(shared, state->handle);
                            push_le_client_event(
                                shared,
                                "bluetooth_le_peripheral_open",
                                le_error_json(
                                    status == WDBG::GattCommunicationStatus::Unreachable
                                        ? 133
                                        : 1,
                                    state->handle));
                            return;
                        }

                        std::unordered_map<
                            std::string,
                            std::shared_ptr<RemoteGattServiceState>> services;

                        for (const auto& service : services_result.Services())
                        {
                            auto service_state =
                                std::make_shared<RemoteGattServiceState>();
                            service_state->uuid =
                                guid_to_uuid_string(service.Uuid());
                            service_state->service = service;
                            services[service_state->uuid] = service_state;
                        }

                        {
                            std::scoped_lock lock(state->mutex);
                            state->services = std::move(services);
                        }

                        state->connected.store(true);

                        push_le_client_event(
                            shared,
                            "bluetooth_le_peripheral_open",
                            "{\"connection\":" +
                                std::to_string(state->handle) + "}");
                    }
                    catch (...)
                    {
                        state->closing.store(true);
                        close_le_client_connection_noexcept(state);
                        unregister_le_client_connection(shared, state->handle);
                        push_le_client_event(
                            shared,
                            "bluetooth_le_peripheral_open",
                            le_error_json(1, state->handle));
                    }
                })
                .detach();

            message.clear();
            return Error::Ok;
        }

        Error le_disconnect(
            std::uint64_t connection,
            std::string& message) override
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

        bool le_connection_is_connected(
            std::uint64_t connection) const override
        {
            auto state = find_le_client_connection(le_client_, connection);
            return state && state->connected.load() && !state->closing.load();
        }

        Error le_services_discover(
            std::uint64_t connection,
            std::string& message) override
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
            std::thread(
                [shared, state, remote]()
                {
                    try
                    {
                        WinrtWorkerApartment apartment;
                        std::scoped_lock operation_lock(state->operation_mutex);

                        const auto result =
                            remote.GetGattServicesAsync(
                                WDB::BluetoothCacheMode::Uncached).get();

                        if (result.Status() !=
                            WDBG::GattCommunicationStatus::Success)
                        {
                            push_le_client_event(
                                shared,
                                "bluetooth_le_peripheral_get_services",
                                le_error_json(1));
                            return;
                        }

                        std::unordered_map<
                            std::string,
                            std::shared_ptr<RemoteGattServiceState>> services;
                        std::string array = "[";
                        bool first = true;

                        for (const auto& service : result.Services())
                        {
                            auto service_state =
                                std::make_shared<RemoteGattServiceState>();
                            service_state->uuid =
                                guid_to_uuid_string(service.Uuid());
                            service_state->service = service;
                            services[service_state->uuid] = service_state;

                            if (!first)
                                array += ",";
                            first = false;
                            array += "{\"uuid\":\"" +
                                json_escape(service_state->uuid) + "\"}";
                        }
                        array += "]";

                        std::unordered_map<
                            std::string,
                            std::shared_ptr<RemoteGattServiceState>> old_services;
                        {
                            std::scoped_lock lock(state->mutex);
                            old_services.swap(state->services);
                            state->services = std::move(services);
                        }
                        for (auto& pair : old_services)
                            close_remote_service_noexcept(pair.second);

                        state->connected.store(true);
                        push_le_client_event(
                            shared,
                            "bluetooth_le_peripheral_get_services",
                            make_nested_json_array_field("services", array));
                    }
                    catch (...)
                    {
                        push_le_client_event(
                            shared,
                            "bluetooth_le_peripheral_get_services",
                            le_error_json(1));
                    }
                })
                .detach();

            message.clear();
            return Error::Ok;
        }

        Error le_characteristics_discover(
            std::uint64_t connection,
            const std::string& service_uuid,
            std::string& message) override
        {
            auto state = find_le_client_connection(le_client_, connection);
            auto service = find_remote_service(state, service_uuid);
            if (!state || !service || !service->service)
            {
                message = "BLE GATT service was not found";
                return Error::InvalidHandle;
            }

            const auto shared = le_client_;
            std::thread(
                [shared, state, service]()
                {
                    try
                    {
                        WinrtWorkerApartment apartment;
                        std::scoped_lock operation_lock(state->operation_mutex);
                        const auto result =
                            service->service.GetCharacteristicsAsync(
                                WDB::BluetoothCacheMode::Uncached).get();

                        if (result.Status() !=
                            WDBG::GattCommunicationStatus::Success)
                        {
                            push_le_client_event(
                                shared,
                                "bluetooth_le_service_get_characteristics",
                                le_error_json(1));
                            return;
                        }

                        std::unordered_map<
                            std::string,
                            std::shared_ptr<RemoteGattCharacteristicState>>
                            characteristics;

                        std::string array = "[";
                        bool first = true;

                        for (const auto& characteristic :
                             result.Characteristics())
                        {
                            auto characteristic_state =
                                std::make_shared<RemoteGattCharacteristicState>();
                            characteristic_state->uuid =
                                guid_to_uuid_string(characteristic.Uuid());
                            characteristic_state->characteristic =
                                characteristic;
                            characteristics[characteristic_state->uuid] =
                                characteristic_state;

                            if (!first)
                                array += ",";
                            first = false;
                            array += "{\"uuid\":\"" +
                                json_escape(characteristic_state->uuid) +
                                "\",\"properties\":" +
                                std::to_string(
                                    static_cast<std::uint32_t>(
                                        characteristic.CharacteristicProperties())) +
                                "}";
                        }
                        array += "]";

                        std::unordered_map<
                            std::string,
                            std::shared_ptr<RemoteGattCharacteristicState>>
                            old_characteristics;
                        {
                            std::scoped_lock lock(state->mutex);
                            old_characteristics.swap(service->characteristics);
                            service->characteristics =
                                std::move(characteristics);
                        }
                        for (auto& pair : old_characteristics)
                            close_remote_characteristic_noexcept(pair.second);

                        push_le_client_event(
                            shared,
                            "bluetooth_le_service_get_characteristics",
                            make_nested_json_array_field(
                                "characteristics",
                                array));
                    }
                    catch (...)
                    {
                        push_le_client_event(
                            shared,
                            "bluetooth_le_service_get_characteristics",
                            le_error_json(1));
                    }
                })
                .detach();

            message.clear();
            return Error::Ok;
        }

        Error le_descriptors_discover(
            std::uint64_t connection,
            const std::string& service_uuid,
            const std::string& characteristic_uuid,
            std::string& message) override
        {
            auto state = find_le_client_connection(le_client_, connection);
            auto characteristic = find_remote_characteristic(
                state,
                service_uuid,
                characteristic_uuid);

            if (!state || !characteristic ||
                !characteristic->characteristic)
            {
                message = "BLE GATT characteristic was not found";
                return Error::InvalidHandle;
            }

            const auto shared = le_client_;
            std::thread(
                [shared, state, characteristic]()
                {
                    try
                    {
                        WinrtWorkerApartment apartment;
                        std::scoped_lock operation_lock(state->operation_mutex);
                        const auto result =
                            characteristic->characteristic.GetDescriptorsAsync(
                                WDB::BluetoothCacheMode::Uncached).get();

                        if (result.Status() !=
                            WDBG::GattCommunicationStatus::Success)
                        {
                            push_le_client_event(
                                shared,
                                "bluetooth_le_characteristic_get_descriptors",
                                le_error_json(1));
                            return;
                        }

                        std::unordered_map<
                            std::string,
                            std::shared_ptr<RemoteGattDescriptorState>>
                            descriptors;

                        std::string array = "[";
                        bool first = true;

                        for (const auto& descriptor : result.Descriptors())
                        {
                            auto descriptor_state =
                                std::make_shared<RemoteGattDescriptorState>();
                            descriptor_state->uuid =
                                guid_to_uuid_string(descriptor.Uuid());
                            descriptor_state->descriptor = descriptor;
                            descriptors[descriptor_state->uuid] =
                                descriptor_state;

                            if (!first)
                                array += ",";
                            first = false;
                            array += "{\"uuid\":\"" +
                                json_escape(descriptor_state->uuid) + "\"}";
                        }
                        array += "]";

                        {
                            std::scoped_lock lock(state->mutex);
                            characteristic->descriptors =
                                std::move(descriptors);
                        }

                        push_le_client_event(
                            shared,
                            "bluetooth_le_characteristic_get_descriptors",
                            make_nested_json_array_field(
                                "descriptors",
                                array));
                    }
                    catch (...)
                    {
                        push_le_client_event(
                            shared,
                            "bluetooth_le_characteristic_get_descriptors",
                            le_error_json(1));
                    }
                })
                .detach();

            message.clear();
            return Error::Ok;
        }

        Error le_characteristic_read(
            std::uint64_t connection,
            const std::string& service_uuid,
            const std::string& characteristic_uuid,
            std::string& message) override
        {
            auto state = find_le_client_connection(le_client_, connection);
            auto characteristic = find_remote_characteristic(
                state,
                service_uuid,
                characteristic_uuid);

            if (!state || !characteristic ||
                !characteristic->characteristic)
            {
                message = "BLE GATT characteristic was not found";
                return Error::InvalidHandle;
            }

            const auto shared = le_client_;
            std::thread(
                [shared, state, characteristic]()
                {
                    try
                    {
                        WinrtWorkerApartment apartment;
                        std::scoped_lock operation_lock(state->operation_mutex);
                        const auto result =
                            characteristic->characteristic.ReadValueAsync(
                                WDB::BluetoothCacheMode::Uncached).get();

                        if (result.Status() !=
                            WDBG::GattCommunicationStatus::Success)
                        {
                            push_le_client_event(
                                shared,
                                "bluetooth_le_characteristic_read",
                                le_error_json(1));
                            return;
                        }

                        const auto bytes = buffer_to_bytes(result.Value());
                        push_le_client_event(
                            shared,
                            "bluetooth_le_characteristic_read",
                            "{\"value\":\"" +
                                json::base64_encode(
                                    bytes.data(),
                                    bytes.size()) +
                                "\"}");
                    }
                    catch (...)
                    {
                        push_le_client_event(
                            shared,
                            "bluetooth_le_characteristic_read",
                            le_error_json(1));
                    }
                })
                .detach();

            message.clear();
            return Error::Ok;
        }

        Error le_characteristic_write(
            std::uint64_t connection,
            const std::string& service_uuid,
            const std::string& characteristic_uuid,
            const std::string& value_base64,
            bool with_response,
            std::string& message) override
        {
            auto state = find_le_client_connection(le_client_, connection);
            auto characteristic = find_remote_characteristic(
                state,
                service_uuid,
                characteristic_uuid);

            if (!state || !characteristic ||
                !characteristic->characteristic)
            {
                message = "BLE GATT characteristic was not found";
                return Error::InvalidHandle;
            }

            const auto payload = json::base64_decode(value_base64);
            const auto shared = le_client_;
            std::thread(
                [shared, state, characteristic, payload, with_response]()
                {
                    try
                    {
                        WinrtWorkerApartment apartment;
                        std::scoped_lock operation_lock(state->operation_mutex);
                        const auto option = with_response
                            ? WDBG::GattWriteOption::WriteWithResponse
                            : WDBG::GattWriteOption::WriteWithoutResponse;

                        const auto status =
                            characteristic->characteristic.WriteValueAsync(
                                bytes_to_buffer(payload),
                                option).get();

                        push_le_client_event(
                            shared,
                            with_response
                                ? "bluetooth_le_characteristic_write_request"
                                : "bluetooth_le_characteristic_write_command",
                            status == WDBG::GattCommunicationStatus::Success
                                ? "{}"
                                : le_error_json(1));
                    }
                    catch (...)
                    {
                        push_le_client_event(
                            shared,
                            with_response
                                ? "bluetooth_le_characteristic_write_request"
                                : "bluetooth_le_characteristic_write_command",
                            le_error_json(1));
                    }
                })
                .detach();

            message.clear();
            return Error::Ok;
        }

        Error le_characteristic_subscribe(
            std::uint64_t connection,
            const std::string& service_uuid,
            const std::string& characteristic_uuid,
            std::int32_t mode,
            std::string& message) override
        {
            auto state = find_le_client_connection(le_client_, connection);
            auto characteristic = find_remote_characteristic(
                state,
                service_uuid,
                characteristic_uuid);

            if (!state || !characteristic ||
                !characteristic->characteristic)
            {
                message = "BLE GATT characteristic was not found";
                return Error::InvalidHandle;
            }

            if (mode < 0 || mode > 2)
            {
                message = "Invalid BLE subscribe mode";
                return Error::InvalidArgument;
            }

            const auto shared = le_client_;
            const std::string normalized_service =
                normalize_uuid(service_uuid);
            const std::string normalized_characteristic =
                normalize_uuid(characteristic_uuid);

            std::thread(
                [shared,
                 state,
                 characteristic,
                 normalized_service,
                 normalized_characteristic,
                 mode]()
                {
                    try
                    {
                        WinrtWorkerApartment apartment;
                        std::scoped_lock operation_lock(state->operation_mutex);

                        if (mode != 0 &&
                            !characteristic->value_changed_registered)
                        {
                            std::weak_ptr<RemoteGattConnectionState>
                                weak_connection = state;

                            characteristic->value_changed_token =
                                characteristic->characteristic.ValueChanged(
                                    [shared,
                                     weak_connection,
                                     normalized_service,
                                     normalized_characteristic](
                                        const WDBG::GattCharacteristic&,
                                        const WDBG::GattValueChangedEventArgs& args)
                                    {
                                        auto current =
                                            weak_connection.lock();
                                        if (!current ||
                                            !shared->alive->load())
                                            return;

                                        const auto bytes =
                                            buffer_to_bytes(
                                                args.CharacteristicValue());

                                        push_le_client_event(
                                            shared,
                                            "bluetooth_le_characteristic_value_changed",
                                            "{\"connection\":" +
                                                std::to_string(
                                                    current->handle) +
                                                ",\"service_uuid\":\"" +
                                                json_escape(
                                                    normalized_service) +
                                                "\",\"characteristic_uuid\":\"" +
                                                json_escape(
                                                    normalized_characteristic) +
                                                "\",\"value\":\"" +
                                                json::base64_encode(
                                                    bytes.data(),
                                                    bytes.size()) +
                                                "\"}");
                                    });

                            characteristic->value_changed_registered = true;
                        }

                        const auto cccd_value =
                            mode == 1
                                ? WDBG::GattClientCharacteristicConfigurationDescriptorValue::Notify
                            : mode == 2
                                ? WDBG::GattClientCharacteristicConfigurationDescriptorValue::Indicate
                                : WDBG::GattClientCharacteristicConfigurationDescriptorValue::None;

                        const auto status =
                            characteristic->characteristic
                                .WriteClientCharacteristicConfigurationDescriptorAsync(
                                    cccd_value)
                                .get();

                        if (status ==
                                WDBG::GattCommunicationStatus::Success &&
                            mode == 0)
                        {
                            try
                            {
                                if (characteristic->value_changed_registered &&
                                    characteristic->value_changed_token.value != 0)
                                {
                                    characteristic->characteristic.ValueChanged(
                                        characteristic->value_changed_token);
                                }
                            }
                            catch (...)
                            {
                            }
                            characteristic->value_changed_registered = false;
                            characteristic->value_changed_token = {};
                        }

                        const char* event_name =
                            mode == 0
                                ? "bluetooth_le_characteristic_unsubscribe"
                            : mode == 1
                                ? "bluetooth_le_characteristic_notify"
                                : "bluetooth_le_characteristic_indicate";

                        push_le_client_event(
                            shared,
                            event_name,
                            status == WDBG::GattCommunicationStatus::Success
                                ? "{}"
                                : le_error_json(1));
                    }
                    catch (...)
                    {
                        const char* event_name =
                            mode == 0
                                ? "bluetooth_le_characteristic_unsubscribe"
                            : mode == 1
                                ? "bluetooth_le_characteristic_notify"
                                : "bluetooth_le_characteristic_indicate";

                        push_le_client_event(
                            shared,
                            event_name,
                            le_error_json(1));
                    }
                })
                .detach();

            message.clear();
            return Error::Ok;
        }

        Error le_descriptor_read(
            std::uint64_t connection,
            const std::string& service_uuid,
            const std::string& characteristic_uuid,
            const std::string& descriptor_uuid,
            std::string& message) override
        {
            auto state = find_le_client_connection(le_client_, connection);
            auto descriptor = find_remote_descriptor(
                state,
                service_uuid,
                characteristic_uuid,
                descriptor_uuid);

            if (!state || !descriptor || !descriptor->descriptor)
            {
                message = "BLE GATT descriptor was not found";
                return Error::InvalidHandle;
            }

            const auto shared = le_client_;
            std::thread(
                [shared, state, descriptor]()
                {
                    try
                    {
                        WinrtWorkerApartment apartment;
                        std::scoped_lock operation_lock(state->operation_mutex);
                        const auto result =
                            descriptor->descriptor.ReadValueAsync(
                                WDB::BluetoothCacheMode::Uncached).get();

                        if (result.Status() !=
                            WDBG::GattCommunicationStatus::Success)
                        {
                            push_le_client_event(
                                shared,
                                "bluetooth_le_descriptor_read",
                                le_error_json(1));
                            return;
                        }

                        const auto bytes = buffer_to_bytes(result.Value());
                        push_le_client_event(
                            shared,
                            "bluetooth_le_descriptor_read",
                            "{\"value\":\"" +
                                json::base64_encode(
                                    bytes.data(),
                                    bytes.size()) +
                                "\"}");
                    }
                    catch (...)
                    {
                        push_le_client_event(
                            shared,
                            "bluetooth_le_descriptor_read",
                            le_error_json(1));
                    }
                })
                .detach();

            message.clear();
            return Error::Ok;
        }

        Error le_descriptor_write(
            std::uint64_t connection,
            const std::string& service_uuid,
            const std::string& characteristic_uuid,
            const std::string& descriptor_uuid,
            const std::string& value_base64,
            std::string& message) override
        {
            auto state = find_le_client_connection(le_client_, connection);
            auto descriptor = find_remote_descriptor(
                state,
                service_uuid,
                characteristic_uuid,
                descriptor_uuid);

            if (!state || !descriptor || !descriptor->descriptor)
            {
                message = "BLE GATT descriptor was not found";
                return Error::InvalidHandle;
            }

            const auto payload = json::base64_decode(value_base64);
            const auto shared = le_client_;

            std::thread(
                [shared, state, descriptor, payload]()
                {
                    try
                    {
                        WinrtWorkerApartment apartment;
                        std::scoped_lock operation_lock(state->operation_mutex);
                        const auto status =
                            descriptor->descriptor.WriteValueAsync(
                                bytes_to_buffer(payload)).get();

                        push_le_client_event(
                            shared,
                            "bluetooth_le_descriptor_write",
                            status == WDBG::GattCommunicationStatus::Success
                                ? "{}"
                                : le_error_json(1));
                    }
                    catch (...)
                    {
                        push_le_client_event(
                            shared,
                            "bluetooth_le_descriptor_write",
                            le_error_json(1));
                    }
                })
                .detach();

            message.clear();
            return Error::Ok;
        }

        void push_le_completion_async(const char* event_type)
        {
            if (!hooks_.push_event)
                return;

            auto push_event = hooks_.push_event;
            const std::string type = event_type ? event_type : "";

            std::thread(
                [push_event = std::move(push_event), type]() mutable
                {
                    BackendEvent event;
                    event.type = BackendEventType::LeEvent;
                    event.transport = Transport::LowEnergy;
                    event.event_type = type;
                    event.json = "{}";
                    push_event(std::move(event));
                })
                .detach();
        }

        // ===== BLE Advertiser =====

        Error le_advertise_start(const std::string& settings_json, const std::string& data_json, std::string& message) override
        {
            if (!initialized_)
            {
                message = "Bluetooth backend is not initialized";
                return Error::NotInitialized;
            }
            if (!supports_le_advertise())
            {
                message = "BLE peripheral advertising is not supported by this Windows adapter";
                return Error::NotSupported;
            }
            if (le_advertising_.exchange(true))
            {
                push_le_completion_async("bluetooth_le_advertise_start");
                message.clear();
                return Error::Ok;
            }

            try
            {
                advertise_connectable_ = true;
                advertise_discoverable_ = true;
                advertise_include_power_ = false;
                advertise_tx_power_.reset();
                advertise_service_data_.clear();

                if (const auto settings = json::parse(settings_json); settings && settings->is_object())
                {
                    if (const auto* connectable = settings->find("connectable"))
                        advertise_connectable_ = connectable->as_bool(true);
                    if (const auto* tx = settings->find("txPowerLevel"); tx && tx->is_number())
                        advertise_tx_power_ = tx->as_int(0);
                }

                std::optional<std::uint16_t> manufacturer_id;
                std::vector<std::uint8_t> manufacturer_bytes;

                if (const auto data = json::parse(data_json); data && data->is_object())
                {
                    if (const auto* include_name = data->find("includeName"))
                        advertise_discoverable_ = include_name->as_bool(true);
                    if (const auto* include_power = data->find("includePowerLevel"))
                        advertise_include_power_ = include_power->as_bool(false);

                    if (const auto* services = data->find("services"); services && services->is_array())
                    {
                        for (const auto& service : services->array_value)
                        {
                            if (!service.is_object())
                                continue;
                            const auto* uuid = service.find("uuid");
                            if (!uuid || !uuid->is_string())
                                continue;

                            std::vector<std::uint8_t> service_data;
                            if (const auto* value = service.find("data"); value && value->is_string())
                                service_data = json::base64_decode(value->string_value);
                            advertise_service_data_[normalize_uuid(uuid->string_value)] = std::move(service_data);
                        }
                    }

                    if (const auto* manufacturer = data->find("manufacturer"); manufacturer && manufacturer->is_object())
                    {
                        if (const auto* id = manufacturer->find("id"); id && id->is_number())
                        {
                            const auto raw_id = id->as_int(-1);
                            if (raw_id >= 0 && raw_id <= 0xFFFF)
                                manufacturer_id = static_cast<std::uint16_t>(raw_id);
                        }
                        if (const auto* value = manufacturer->find("data"); value && value->is_string())
                            manufacturer_bytes = json::base64_decode(value->string_value);
                    }
                }

                // The generic Windows advertisement publisher cannot write
                // system-reserved sections such as LocalName or Service UUIDs.
                // Those are published by GattServiceProvider below. The generic
                // publisher is used for manufacturer data and TX-power metadata.
                advertiser_ = WDBA::BluetoothLEAdvertisementPublisher{};
                if (manufacturer_id)
                {
                    advertiser_.Advertisement().ManufacturerData().Append(
                        WDBA::BluetoothLEManufacturerData(
                            *manufacturer_id,
                            bytes_to_buffer(manufacturer_bytes)));
                }

                advertiser_.IncludeTransmitPowerLevel(advertise_include_power_);
                if (advertise_tx_power_)
                {
                    try
                    {
                        advertiser_.PreferredTransmitPowerLevelInDBm(*advertise_tx_power_);
                    }
                    catch (...)
                    {
                        // Older Windows builds may not expose the optional power-level setter.
                    }
                }

                advertiser_status_token_ = advertiser_.StatusChanged(
                    [this](
                        const WDBA::BluetoothLEAdvertisementPublisher&,
                        const WDBA::BluetoothLEAdvertisementPublisherStatusChangedEventArgs& args)
                    {
                        const auto status = args.Status();
                        if (status == WDBA::BluetoothLEAdvertisementPublisherStatus::Aborted ||
                            status == WDBA::BluetoothLEAdvertisementPublisherStatus::Stopped)
                        {
                            le_advertising_.store(false);
                        }
                    });

                if (manufacturer_id || advertise_include_power_)
                    advertiser_.Start();

                for (auto& [_, service] : gatt_services_)
                    start_service_advertising(service);

                push_le_completion_async("bluetooth_le_advertise_start");
                message.clear();
                return Error::Ok;
            }
            catch (const winrt::hresult_error& error)
            {
                le_advertising_.store(false);
                release_advertiser_noexcept();
                message = winrt::to_string(error.message());
                return Error::OperationFailed;
            }
        }

        Error le_advertise_stop(std::string& message) override
        {
            try
            {
                for (auto& [_, service] : gatt_services_)
                {
                    if (service && service->provider)
                        service->provider.StopAdvertising();
                }
                release_advertiser_noexcept();
                le_advertising_.store(false);
                message.clear();
                return Error::Ok;
            }
            catch (const winrt::hresult_error& error)
            {
                le_advertising_.store(false);
                release_advertiser_noexcept();
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
            if (!initialized_)
            {
                message = "Bluetooth backend is not initialized";
                return Error::NotInitialized;
            }
            if (!supports_le_server())
            {
                message = "BLE GATT server mode is not supported by this Windows adapter";
                return Error::NotSupported;
            }

            le_server_open_.store(true);
            message.clear();
            return Error::Ok;
        }

        Error le_server_stop(std::string& message) override
        {
            le_server_open_.store(false);
            clear_gatt_services_noexcept();
            {
                std::scoped_lock lock(gatt_request_mutex_);
                pending_gatt_reads_.clear();
                pending_gatt_writes_.clear();
            }
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
                message = "BLE GATT server is not open";
                return Error::InvalidHandle;
            }

            const auto root = json::parse(service_json);
            if (!root || !root->is_object())
            {
                message = "Invalid BLE service definition";
                return Error::InvalidArgument;
            }

            const auto* uuid_field = root->find("uuid");
            if (!uuid_field || !uuid_field->is_string() || uuid_field->string_value.empty())
            {
                message = "BLE service UUID is required";
                return Error::InvalidArgument;
            }

            winrt::guid service_guid{};
            if (!parse_winrt_guid(uuid_field->string_value, service_guid))
            {
                message = "Invalid BLE service UUID";
                return Error::InvalidArgument;
            }

            try
            {
                const std::string service_uuid = normalize_uuid(uuid_field->string_value);
                if (gatt_services_.find(service_uuid) != gatt_services_.end())
                {
                    message = "BLE service already exists";
                    return Error::Busy;
                }

                const auto provider_result = WDBG::GattServiceProvider::CreateAsync(service_guid).get();
                if (provider_result.Error() != WDB::BluetoothError::Success || !provider_result.ServiceProvider())
                {
                    message = bluetooth_error_message(provider_result.Error());
                    return map_bluetooth_error(provider_result.Error());
                }

                auto service_state = std::make_shared<LocalGattServiceState>();
                service_state->uuid = service_uuid;
                service_state->provider = provider_result.ServiceProvider();

                if (const auto* characteristics = root->find("characteristics"); characteristics && characteristics->is_array())
                {
                    for (const auto& characteristic_value : characteristics->array_value)
                    {
                        if (!characteristic_value.is_object())
                            continue;

                        const auto* char_uuid_field = characteristic_value.find("uuid");
                        if (!char_uuid_field || !char_uuid_field->is_string())
                            continue;

                        winrt::guid characteristic_guid{};
                        if (!parse_winrt_guid(char_uuid_field->string_value, characteristic_guid))
                        {
                            message = "Invalid BLE characteristic UUID";
                            return Error::InvalidArgument;
                        }

                        const std::string characteristic_uuid = normalize_uuid(char_uuid_field->string_value);
                        const std::int32_t raw_properties = characteristic_value.find("properties")
                            ? characteristic_value.find("properties")->as_int(0)
                            : 0;
                        const std::int32_t permissions = characteristic_value.find("permissions")
                            ? characteristic_value.find("permissions")->as_int(0)
                            : 0;

                        WDBG::GattLocalCharacteristicParameters parameters;
                        // Windows local GATT rejects Broadcast. All other raw
                        // BluetoothLeCharacteristicProperty values map directly.
                        const auto windows_properties = static_cast<WDBG::GattCharacteristicProperties>(raw_properties & ~1);
                        parameters.CharacteristicProperties(windows_properties);
                        parameters.ReadProtectionLevel(read_protection_from_permissions(permissions));
                        parameters.WriteProtectionLevel(write_protection_from_permissions(permissions));

                        if (const auto* initial_value = characteristic_value.find("value");
                            initial_value && initial_value->is_string() && !initial_value->string_value.empty())
                        {
                            parameters.StaticValue(bytes_to_buffer(json::base64_decode(initial_value->string_value)));
                        }

                        const auto characteristic_result = service_state->provider.Service()
                            .CreateCharacteristicAsync(characteristic_guid, parameters).get();
                        if (characteristic_result.Error() != WDB::BluetoothError::Success || !characteristic_result.Characteristic())
                        {
                            message = bluetooth_error_message(characteristic_result.Error());
                            return map_bluetooth_error(characteristic_result.Error());
                        }

                        auto characteristic_state = std::make_shared<LocalGattCharacteristicState>();
                        characteristic_state->service_uuid = service_uuid;
                        characteristic_state->characteristic_uuid = characteristic_uuid;
                        characteristic_state->characteristic = characteristic_result.Characteristic();

                        characteristic_state->read_token = characteristic_state->characteristic.ReadRequested(
                            [this, service_uuid, characteristic_uuid](
                                const WDBG::GattLocalCharacteristic&,
                                const WDBG::GattReadRequestedEventArgs& args)
                            {
                                try
                                {
                                    const auto request = args.GetRequestAsync().get();
                                    if (!request)
                                        return;

                                    const std::int32_t request_id = next_gatt_request_id_.fetch_add(1);
                                    {
                                        std::scoped_lock lock(gatt_request_mutex_);
                                        pending_gatt_reads_[request_id] = PendingGattRead{request};
                                    }

                                    if (hooks_.push_event)
                                    {
                                        BackendEvent event;
                                        event.type = BackendEventType::LeEvent;
                                        event.transport = Transport::LowEnergy;
                                        event.event_type = "bluetooth_le_server_characteristic_read_request";
                                        event.json = make_server_request_json(
                                            request_id,
                                            service_uuid,
                                            characteristic_uuid,
                                            {},
                                            request.Offset(),
                                            nullptr);
                                        hooks_.push_event(std::move(event));
                                    }
                                }
                                catch (...)
                                {
                                }
                            });

                        characteristic_state->write_token = characteristic_state->characteristic.WriteRequested(
                            [this, service_uuid, characteristic_uuid](
                                const WDBG::GattLocalCharacteristic&,
                                const WDBG::GattWriteRequestedEventArgs& args)
                            {
                                try
                                {
                                    const auto request = args.GetRequestAsync().get();
                                    if (!request)
                                        return;

                                    const std::vector<std::uint8_t> value = buffer_to_bytes(request.Value());
                                    const bool with_response = request.Option() == WDBG::GattWriteOption::WriteWithResponse;
                                    const std::int32_t request_id = next_gatt_request_id_.fetch_add(1);
                                    {
                                        std::scoped_lock lock(gatt_request_mutex_);
                                        pending_gatt_writes_[request_id] = PendingGattWrite{request, with_response};
                                    }

                                    if (hooks_.push_event)
                                    {
                                        BackendEvent event;
                                        event.type = BackendEventType::LeEvent;
                                        event.transport = Transport::LowEnergy;
                                        event.event_type = "bluetooth_le_server_characteristic_write_request";
                                        event.json = make_server_request_json(
                                            request_id,
                                            service_uuid,
                                            characteristic_uuid,
                                            {},
                                            request.Offset(),
                                            &value);
                                        hooks_.push_event(std::move(event));
                                    }
                                }
                                catch (...)
                                {
                                }
                            });

                        if (const auto* descriptors = characteristic_value.find("descriptors"); descriptors && descriptors->is_array())
                        {
                            for (const auto& descriptor_value : descriptors->array_value)
                            {
                                if (!descriptor_value.is_object())
                                    continue;
                                const auto* descriptor_uuid_field = descriptor_value.find("uuid");
                                if (!descriptor_uuid_field || !descriptor_uuid_field->is_string())
                                    continue;

                                winrt::guid descriptor_guid{};
                                if (!parse_winrt_guid(descriptor_uuid_field->string_value, descriptor_guid))
                                {
                                    message = "Invalid BLE descriptor UUID";
                                    return Error::InvalidArgument;
                                }

                                const std::string descriptor_uuid = normalize_uuid(descriptor_uuid_field->string_value);

                                // Windows creates the Client Characteristic Configuration
                                // descriptor automatically for Notify/Indicate characteristics.
                                if (descriptor_uuid == "00002902-0000-1000-8000-00805f9b34fb")
                                    continue;

                                WDBG::GattLocalDescriptorParameters descriptor_parameters;
                                descriptor_parameters.ReadProtectionLevel(WDBG::GattProtectionLevel::Plain);
                                descriptor_parameters.WriteProtectionLevel(WDBG::GattProtectionLevel::Plain);

                                const auto descriptor_result = characteristic_state->characteristic
                                    .CreateDescriptorAsync(descriptor_guid, descriptor_parameters).get();
                                if (descriptor_result.Error() != WDB::BluetoothError::Success || !descriptor_result.Descriptor())
                                {
                                    message = bluetooth_error_message(descriptor_result.Error());
                                    return map_bluetooth_error(descriptor_result.Error());
                                }

                                auto descriptor_state = std::make_shared<LocalGattDescriptorState>();
                                descriptor_state->descriptor = descriptor_result.Descriptor();
                                descriptor_state->read_token = descriptor_state->descriptor.ReadRequested(
                                    [this, service_uuid, characteristic_uuid, descriptor_uuid](
                                        const WDBG::GattLocalDescriptor&,
                                        const WDBG::GattReadRequestedEventArgs& args)
                                    {
                                        try
                                        {
                                            const auto request = args.GetRequestAsync().get();
                                            if (!request)
                                                return;

                                            const std::int32_t request_id = next_gatt_request_id_.fetch_add(1);
                                            {
                                                std::scoped_lock lock(gatt_request_mutex_);
                                                pending_gatt_reads_[request_id] = PendingGattRead{request};
                                            }

                                            if (hooks_.push_event)
                                            {
                                                BackendEvent event;
                                                event.type = BackendEventType::LeEvent;
                                                event.transport = Transport::LowEnergy;
                                                event.event_type = "bluetooth_le_server_descriptor_read_request";
                                                event.json = make_server_request_json(
                                                    request_id,
                                                    service_uuid,
                                                    characteristic_uuid,
                                                    descriptor_uuid,
                                                    request.Offset(),
                                                    nullptr);
                                                hooks_.push_event(std::move(event));
                                            }
                                        }
                                        catch (...)
                                        {
                                        }
                                    });

                                descriptor_state->write_token = descriptor_state->descriptor.WriteRequested(
                                    [this, service_uuid, characteristic_uuid, descriptor_uuid](
                                        const WDBG::GattLocalDescriptor&,
                                        const WDBG::GattWriteRequestedEventArgs& args)
                                    {
                                        try
                                        {
                                            const auto request = args.GetRequestAsync().get();
                                            if (!request)
                                                return;

                                            const std::vector<std::uint8_t> value = buffer_to_bytes(request.Value());
                                            const bool with_response = request.Option() == WDBG::GattWriteOption::WriteWithResponse;
                                            const std::int32_t request_id = next_gatt_request_id_.fetch_add(1);
                                            {
                                                std::scoped_lock lock(gatt_request_mutex_);
                                                pending_gatt_writes_[request_id] = PendingGattWrite{request, with_response};
                                            }

                                            if (hooks_.push_event)
                                            {
                                                BackendEvent event;
                                                event.type = BackendEventType::LeEvent;
                                                event.transport = Transport::LowEnergy;
                                                event.event_type = "bluetooth_le_server_descriptor_write_request";
                                                event.json = make_server_request_json(
                                                    request_id,
                                                    service_uuid,
                                                    characteristic_uuid,
                                                    descriptor_uuid,
                                                    request.Offset(),
                                                    &value);
                                                hooks_.push_event(std::move(event));
                                            }
                                        }
                                        catch (...)
                                        {
                                        }
                                    });

                                characteristic_state->descriptors.push_back(std::move(descriptor_state));
                            }
                        }

                        service_state->characteristics[characteristic_uuid] = characteristic_state;
                    }
                }

                gatt_services_[service_uuid] = service_state;
                if (le_advertising_.load())
                    start_service_advertising(service_state);

                push_le_completion_async("bluetooth_le_server_add_service");

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
            clear_gatt_services_noexcept();
            {
                std::scoped_lock lock(gatt_request_mutex_);
                pending_gatt_reads_.clear();
                pending_gatt_writes_.clear();
            }
            message.clear();
            return Error::Ok;
        }

        Error le_server_respond_read(
            std::int32_t request_id,
            std::int32_t status,
            const std::string& value_base64,
            std::string& message) override
        {
            PendingGattRead pending;
            {
                std::scoped_lock lock(gatt_request_mutex_);
                const auto it = pending_gatt_reads_.find(request_id);
                if (it == pending_gatt_reads_.end())
                {
                    message = "BLE read request not found";
                    return Error::NotFound;
                }
                pending = it->second;
                pending_gatt_reads_.erase(it);
            }

            try
            {
                if (status == 0)
                    pending.request.RespondWithValue(bytes_to_buffer(json::base64_decode(value_base64)));
                else
                    pending.request.RespondWithProtocolError(static_cast<std::uint8_t>(std::clamp(status, 1, 255)));

                message.clear();
                return Error::Ok;
            }
            catch (const winrt::hresult_error& error)
            {
                message = winrt::to_string(error.message());
                return Error::OperationFailed;
            }
        }

        Error le_server_respond_write(
            std::int32_t request_id,
            std::int32_t status,
            std::string& message) override
        {
            PendingGattWrite pending;
            {
                std::scoped_lock lock(gatt_request_mutex_);
                const auto it = pending_gatt_writes_.find(request_id);
                if (it == pending_gatt_writes_.end())
                {
                    message = "BLE write request not found";
                    return Error::NotFound;
                }
                pending = it->second;
                pending_gatt_writes_.erase(it);
            }

            try
            {
                if (pending.with_response)
                {
                    if (status == 0)
                        pending.request.Respond();
                    else
                        pending.request.RespondWithProtocolError(static_cast<std::uint8_t>(std::clamp(status, 1, 255)));
                }

                message.clear();
                return Error::Ok;
            }
            catch (const winrt::hresult_error& error)
            {
                message = winrt::to_string(error.message());
                return Error::OperationFailed;
            }
        }

        Error le_server_notify_value(
            const std::string& service_uuid,
            const std::string& characteristic_uuid,
            const std::string& value_base64,
            std::string& message) override
        {
            const auto service_it = gatt_services_.find(normalize_uuid(service_uuid));
            if (service_it == gatt_services_.end() || !service_it->second)
            {
                message = "BLE service not found";
                return Error::NotFound;
            }

            const auto characteristic_it = service_it->second->characteristics.find(normalize_uuid(characteristic_uuid));
            if (characteristic_it == service_it->second->characteristics.end() ||
                !characteristic_it->second || !characteristic_it->second->characteristic)
            {
                message = "BLE characteristic not found";
                return Error::NotFound;
            }

            try
            {
                characteristic_it->second->characteristic
                    .NotifyValueAsync(bytes_to_buffer(json::base64_decode(value_base64))).get();
                message.clear();
                return Error::Ok;
            }
            catch (const winrt::hresult_error& error)
            {
                message = winrt::to_string(error.message());
                return Error::OperationFailed;
            }
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

                if (!register_connection_if_alive(shared, state))
                {
                    closesocket(socket_handle);
                    return;
                }

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

        Error pair(
            std::uint64_t device_handle,
            const DiscoveredDevice& device,
            std::string& message) override
        {
            if (device.transport != Transport::Classic)
            {
                message = "Bluetooth pairing is only supported for Classic devices on Windows";
                return Error::NotSupported;
            }

            if (!device.address_available)
            {
                message = "Bluetooth Classic device has no usable address";
                return Error::InvalidArgument;
            }

            BTH_ADDR address = 0;
            if (!parse_bluetooth_address(device.address, address))
            {
                message = "Invalid Bluetooth address";
                return Error::InvalidArgument;
            }

            const auto shared = classic_;
            const auto alive = classic_->alive;

            std::thread([shared, alive, device_handle, address]()
            {
                if (!alive->load())
                    return;

                BLUETOOTH_DEVICE_INFO device_info{};
                device_info.dwSize = sizeof(device_info);
                device_info.Address.ullLong = address;

                const DWORD result = BluetoothAuthenticateDeviceEx(
                    nullptr, nullptr, &device_info, nullptr, MITMProtectionNotRequired);

                if (!alive->load() || !shared->hooks.push_event)
                    return;

                BackendEvent event;
                event.type = BackendEventType::DevicePaired;
                event.transport = Transport::Classic;
                event.device = device_handle;

                if (result == ERROR_SUCCESS)
                {
                    event.error = Error::Ok;
                }
                else
                {
                    event.error = Error::OperationFailed;
                    event.message = "BluetoothAuthenticateDeviceEx failed: " + std::to_string(result);
                }

                shared->hooks.push_event(std::move(event));
            }).detach();

            message.clear();
            return Error::Ok;
        }

        bool is_paired(const DiscoveredDevice& device) const override
        {
            if (device.transport != Transport::Classic || !device.address_available)
                return false;

            BTH_ADDR address = 0;
            if (!parse_bluetooth_address(device.address, address))
                return false;

            BLUETOOTH_DEVICE_INFO device_info{};
            device_info.dwSize = sizeof(device_info);
            device_info.Address.ullLong = address;

            if (BluetoothGetDeviceInfo(nullptr, &device_info) != ERROR_SUCCESS)
                return false;

            return device_info.fAuthenticated != FALSE;
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
            connection_request_shutdown(state);

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
                const SOCKET socket_handle = connection_socket(state);
                if (socket_handle == INVALID_SOCKET)
                {
                    message = "Classic connection is not connected";
                    return Error::Disconnected;
                }

                const int sent = send(
                    socket_handle,
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

                    if (!register_connection_if_alive(classic_, state))
                    {
                        closesocket(client);
                        continue;
                    }

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

        Error classic_discoverable_start(std::int32_t duration_seconds, std::string& message) override
        {
            // Idempotent: tear down any previous session/timer before starting a new one.
            std::string ignored;
            classic_discoverable_stop(ignored);

            BLUETOOTH_FIND_RADIO_PARAMS params{};
            params.dwSize = sizeof(params);
            HANDLE radio_handle = nullptr;
            HBLUETOOTH_RADIO_FIND find = BluetoothFindFirstRadio(&params, &radio_handle);
            if (!find)
            {
                message = "No Bluetooth radio found on this system";
                return Error::NotSupported;
            }
            BluetoothFindRadioClose(find);

            if (!BluetoothEnableIncomingConnections(radio_handle, TRUE))
            {
                CloseHandle(radio_handle);
                message = "BluetoothEnableIncomingConnections failed";
                return Error::OperationFailed;
            }

            if (!BluetoothEnableDiscovery(radio_handle, TRUE))
            {
                CloseHandle(radio_handle);
                message = "BluetoothEnableDiscovery failed";
                return Error::OperationFailed;
            }

            discoverable_radio_ = radio_handle;
            classic_discoverable_active_.store(true);
            classic_discoverable_stop_requested_.store(false);

            if (duration_seconds > 0)
            {
                discoverable_timer_thread_ = std::thread([this, duration_seconds]()
                {
                    const auto deadline =
                        std::chrono::steady_clock::now() + std::chrono::seconds(duration_seconds);
                    while (!classic_discoverable_stop_requested_.load() &&
                           std::chrono::steady_clock::now() < deadline)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    }

                    // Only auto-disable on natural expiry - if a stop was requested,
                    // classic_discoverable_stop() owns disabling/closing the radio.
                    if (!classic_discoverable_stop_requested_.load())
                    {
                        if (discoverable_radio_)
                            BluetoothEnableDiscovery(discoverable_radio_, FALSE);
                        classic_discoverable_active_.store(false);
                    }
                });
            }

            message.clear();
            return Error::Ok;
        }

        Error classic_discoverable_stop(std::string& message) override
        {
            classic_discoverable_stop_requested_.store(true);

            if (discoverable_timer_thread_.joinable())
                discoverable_timer_thread_.join();

            if (discoverable_radio_)
            {
                BluetoothEnableDiscovery(discoverable_radio_, FALSE);
                CloseHandle(discoverable_radio_);
                discoverable_radio_ = nullptr;
            }

            classic_discoverable_active_.store(false);
            message.clear();
            return Error::Ok;
        }

        bool classic_discoverable_is_running() const override
        {
            if (!discoverable_radio_)
                return classic_discoverable_active_.load();
            return BluetoothIsDiscoverable(discoverable_radio_) != FALSE;
        }

        ~WindowsBackend() override
        {
            if (initialized_)
                shutdown();
        }

    private:
        void start_service_advertising(const std::shared_ptr<LocalGattServiceState>& service)
        {
            if (!service || !service->provider)
                return;

            WDBG::GattServiceProviderAdvertisingParameters parameters;
            parameters.IsConnectable(advertise_connectable_);
            parameters.IsDiscoverable(advertise_discoverable_);

            const auto data_it = advertise_service_data_.find(service->uuid);
            if (data_it != advertise_service_data_.end() && !data_it->second.empty())
                parameters.ServiceData(bytes_to_buffer(data_it->second));

            service->provider.StartAdvertising(parameters);
        }

        void clear_gatt_services_noexcept() noexcept
        {
            try
            {
                for (auto& [_, service] : gatt_services_)
                {
                    if (service && service->provider)
                        service->provider.StopAdvertising();
                }
            }
            catch (...)
            {
            }
            gatt_services_.clear();
        }

        void release_advertiser_noexcept() noexcept
        {
            try
            {
                if (advertiser_)
                {
                    if (advertiser_status_token_.value != 0)
                    {
                        advertiser_.StatusChanged(advertiser_status_token_);
                        advertiser_status_token_ = {};
                    }
                    advertiser_.Stop();
                    advertiser_ = nullptr;
                }
            }
            catch (...)
            {
                advertiser_ = nullptr;
                advertiser_status_token_ = {};
            }
        }

        void release_radio_noexcept() noexcept
        {
            try
            {
                if (bluetooth_radio_ && radio_state_token_.value != 0)
                    bluetooth_radio_.StateChanged(radio_state_token_);
            }
            catch (...)
            {
            }
            radio_state_token_ = {};
            bluetooth_radio_ = nullptr;
            bluetooth_adapter_ = nullptr;
        }

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
        std::shared_ptr<SharedLeClientState> le_client_;

        bool initialized_ = false;
        bool owns_apartment_ = false;
        bool winsock_initialized_ = false;

        std::atomic_bool le_scanning_{false};
        WDBA::BluetoothLEAdvertisementWatcher watcher_{nullptr};
        winrt::event_token received_token_{};
        winrt::event_token stopped_token_{};

        WDB::BluetoothAdapter bluetooth_adapter_{nullptr};
        WDR::Radio bluetooth_radio_{nullptr};
        winrt::event_token radio_state_token_{};
        std::atomic<std::int32_t> bluetooth_state_{0};
        bool ble_supported_ = true;
        bool le_peripheral_supported_ = true;

        std::atomic_bool le_advertising_{false};
        WDBA::BluetoothLEAdvertisementPublisher advertiser_{nullptr};
        winrt::event_token advertiser_status_token_{};
        bool advertise_connectable_ = true;
        bool advertise_discoverable_ = true;
        bool advertise_include_power_ = false;
        std::optional<std::int32_t> advertise_tx_power_;
        std::unordered_map<std::string, std::vector<std::uint8_t>> advertise_service_data_;

        std::atomic_bool le_server_open_{false};
        std::unordered_map<std::string, std::shared_ptr<LocalGattServiceState>> gatt_services_;
        std::mutex gatt_request_mutex_;
        std::unordered_map<std::int32_t, PendingGattRead> pending_gatt_reads_;
        std::unordered_map<std::int32_t, PendingGattWrite> pending_gatt_writes_;
        std::atomic<std::int32_t> next_gatt_request_id_{1};

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

        std::atomic_bool classic_discoverable_active_{false};
        std::atomic_bool classic_discoverable_stop_requested_{false};
        std::thread discoverable_timer_thread_;
        HANDLE discoverable_radio_ = nullptr;
    };

    std::unique_ptr<Backend> create_platform_backend(CoreHooks hooks)
    {
        return std::make_unique<WindowsBackend>(std::move(hooks));
    }
}

#endif
