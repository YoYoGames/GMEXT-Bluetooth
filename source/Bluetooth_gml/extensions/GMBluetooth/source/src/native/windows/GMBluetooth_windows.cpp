#include "GMBluetooth_backend.h"
#include "GMBluetooth_json.h"
#include "GMBluetooth_log.h"

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
#include <winrt/Windows.Foundation.Metadata.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Devices.Radios.h>
#include <winrt/Windows.Storage.Streams.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace gmbluetooth
{
    namespace WF = winrt::Windows::Foundation;
    namespace WDB = winrt::Windows::Devices::Bluetooth;
    namespace WDBA = winrt::Windows::Devices::Bluetooth::Advertisement;
    namespace WDBG = winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;
    namespace WDE = winrt::Windows::Devices::Enumeration;
    namespace WDR = winrt::Windows::Devices::Radios;
    namespace WFM = winrt::Windows::Foundation::Metadata;
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

        // A 4- or 8-hex-digit SIG UUID ("180D") onto the Bluetooth base UUID;
        // any other string unchanged.
        std::string expand_short_uuid(const std::string& text)
        {
            if (text.size() == 4)
                return "0000" + text + "-0000-1000-8000-00805f9b34fb";
            if (text.size() == 8)
                return text + "-0000-1000-8000-00805f9b34fb";
            return text;
        }

        bool parse_guid(const std::string& text, GUID& out)
        {
            if (text.empty())
                return false;

            // CLSIDFromString requires braces (verified on Windows 11 10.0.26200:
            // an unbraced "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx" string returns
            // CO_E_CLASSSTRING). Normalize to the braced form before parsing so
            // callers can pass either format.
            const std::string expanded = expand_short_uuid(text);
            const std::string braced = (expanded.front() == '{')
                ? expanded
                : "{" + expanded + "}";

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

        void trim_trailing_space(std::string& text)
        {
            while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0)
                text.pop_back();
        }

        // "connect failed (10060): A connection attempt failed ...", the
        // system text in UTF-8 without the CRLF FormatMessage ends it with.
        // Winsock and Win32 error codes share the system message table.
        std::string system_error_message(const char* operation, DWORD error)
        {
            wchar_t* system_message = nullptr;
            const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                FORMAT_MESSAGE_FROM_SYSTEM |
                                FORMAT_MESSAGE_IGNORE_INSERTS;
            FormatMessageW(
                flags,
                nullptr,
                static_cast<DWORD>(error),
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                reinterpret_cast<LPWSTR>(&system_message),
                0,
                nullptr);

            std::string text;
            if (system_message)
            {
                text = wide_to_utf8(system_message);
                LocalFree(system_message);
            }
            trim_trailing_space(text);

            std::string out(operation ? operation : "Bluetooth operation");
            out += " failed";
            if (error != 0)
                out += " (" + std::to_string(error) + ")";
            if (!text.empty() && error != 0)
            {
                out += ": ";
                out += text;
            }
            return out;
        }

        std::string wsa_message(const char* operation, int error)
        {
            return system_error_message(operation ? operation : "Bluetooth socket operation", static_cast<DWORD>(error));
        }

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

        // Every thread the backend starts is counted here, from before it
        // starts until its body and everything the body captured are gone.
        // Shutdown waits for the count to reach zero before it tears down
        // Winsock and COM under them.
        struct WorkerTracker
        {
            std::mutex mutex;
            std::condition_variable idle;
            std::size_t count = 0;
        };

        void worker_finished(WorkerTracker& tracker)
        {
            std::scoped_lock lock(tracker.mutex);
            if (--tracker.count == 0)
                tracker.idle.notify_all();
        }

        // True when every worker finished within timeout.
        bool wait_for_workers(WorkerTracker& tracker, std::chrono::milliseconds timeout, std::size_t& remaining)
        {
            std::unique_lock lock(tracker.mutex);
            const bool drained = tracker.idle.wait_for(lock, timeout, [&tracker]() { return tracker.count == 0; });
            remaining = tracker.count;
            return drained;
        }

        // Runs body on a new thread counted by tracker. The thread comes back
        // joinable, or empty when Windows could not create it.
        template <typename Body>
        std::thread spawn_worker(const std::shared_ptr<WorkerTracker>& tracker, Body&& body)
        {
            auto task = std::make_unique<std::decay_t<Body>>(std::forward<Body>(body));
            {
                std::scoped_lock lock(tracker->mutex);
                ++tracker->count;
            }

            try
            {
                return std::thread([tracker, task = std::move(task)]() mutable
                {
                    try
                    {
                        (*task)();
                    }
                    catch (const std::exception& error)
                    {
                        GMBT_LOG("Bluetooth worker thread failed: %s", error.what());
                    }
                    catch (...)
                    {
                        GMBT_LOG("Bluetooth worker thread failed");
                    }

                    // The captures go before the count drops, so nothing the
                    // body held is released after shutdown stopped waiting.
                    task.reset();
                    worker_finished(*tracker);
                });
            }
            catch (const std::system_error& error)
            {
                GMBT_LOG("Could not start a Bluetooth worker thread: %s", error.what());
                worker_finished(*tracker);
                return std::thread();
            }
        }

        // spawn_worker for a thread nobody joins. False when it could not
        // start.
        template <typename Body>
        bool spawn_detached_worker(const std::shared_ptr<WorkerTracker>& tracker, Body&& body)
        {
            std::thread thread = spawn_worker(tracker, std::forward<Body>(body));
            if (!thread.joinable())
                return false;
            thread.detach();
            return true;
        }

        // A thread handed to the worker that replaces it, which joins it
        // first. If that worker never starts, it is let go instead of
        // terminating the process; the tracker still counts it.
        struct PreviousWorker
        {
            std::thread thread;

            ~PreviousWorker()
            {
                if (thread.joinable())
                    thread.detach();
            }
        };


        // The 36-character lowercase form. A 4- or 8-hex-digit SIG UUID ("180D")
        // is expanded onto the Bluetooth base, the way CoreBluetooth reads it.
        std::string normalize_uuid(std::string value)
        {
            value.erase(
                std::remove_if(value.begin(), value.end(), [](unsigned char c)
                {
                    return std::isspace(c) != 0 || c == '{' || c == '}';
                }),
                value.end());
            value = expand_short_uuid(value);
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

        // "aa:bb:cc:dd:ee:ff" in the uppercase form every id carries; empty
        // when it is not an address in that shape.
        std::string normalized_address(std::string address)
        {
            if (address.size() != 17)
                return std::string();
            for (std::size_t i = 0; i < address.size(); ++i)
            {
                const bool colon = (i % 3) == 2;
                if (colon ? address[i] != ':' : std::isxdigit(static_cast<unsigned char>(address[i])) == 0)
                    return std::string();
            }
            std::transform(address.begin(), address.end(), address.begin(), [](unsigned char c)
            {
                return static_cast<char>(std::toupper(c));
            });
            return address;
        }

        // A remote device id ends in its address ("BluetoothLE#BluetoothLE
        // <local>-<remote>"); empty when it does not.
        std::string address_from_device_id(const std::string& device_id)
        {
            const auto dash = device_id.rfind('-');
            if (dash == std::string::npos)
                return std::string();
            return normalized_address(device_id.substr(dash + 1));
        }

        // The central fields every GATT server event carries: the key the core
        // maps to a server connection, and the device address when known,
        // with the id a scan result for the same peer has (R1-155).
        std::string server_central_json(const std::string& central, const std::string& address)
        {
            std::string out = "\"central\":\"" + json_escape(central) + "\"";
            if (!address.empty())
            {
                out += ",\"address\":\"" + json_escape(address) + "\"";
                out += ",\"device_id\":\"win:ble:" + json_escape(address) + "\"";
            }
            return out;
        }

        std::string make_server_request_json(
            std::int32_t request_id,
            const std::string& central,
            const std::string& address,
            const std::string& service_uuid,
            const std::string& characteristic_uuid,
            const std::string& descriptor_uuid,
            std::uint32_t offset,
            const std::vector<std::uint8_t>* value,
            bool response_needed = true)
        {
            std::string out = "{\"request_id\":" + std::to_string(request_id) +
                "," + server_central_json(central, address) +
                ",\"service_uuid\":\"" + json_escape(service_uuid) +
                "\",\"characteristic_uuid\":\"" + json_escape(characteristic_uuid) +
                "\",\"descriptor_uuid\":\"" + json_escape(descriptor_uuid) +
                "\",\"offset\":" + std::to_string(offset);

            // A write request always says whether it waits for an answer.
            if (value)
            {
                out += ",\"value\":\"";
                out += json::base64_encode(value->data(), value->size());
                out += "\"";
                out += std::string(",\"response_needed\":") + (response_needed ? "true" : "false");
            }

            out += "}";
            return out;
        }

        // BluetoothLeAttributePermission onto Windows protection levels. READ
        // and WRITE are implied by the characteristic properties; the signed
        // write bits never get here (le_server_add_service refuses them).
        WDBG::GattProtectionLevel read_protection_from_permissions(std::int32_t permissions)
        {
            if ((permissions & kPermissionReadEncryptedMitm) != 0)
                return WDBG::GattProtectionLevel::EncryptionAndAuthenticationRequired;
            if ((permissions & kPermissionReadEncrypted) != 0)
                return WDBG::GattProtectionLevel::EncryptionRequired;
            return WDBG::GattProtectionLevel::Plain;
        }

        WDBG::GattProtectionLevel write_protection_from_permissions(std::int32_t permissions)
        {
            if ((permissions & kPermissionWriteEncryptedMitm) != 0)
                return WDBG::GattProtectionLevel::EncryptionAndAuthenticationRequired;
            if ((permissions & kPermissionWriteEncrypted) != 0)
                return WDBG::GattProtectionLevel::EncryptionRequired;
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
            winrt::event_token subscribed_token{};
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
            WF::Deferral deferral{nullptr};
        };

        struct PendingGattWrite
        {
            WDBG::GattWriteRequest request{nullptr};
            WF::Deferral deferral{nullptr};
            bool with_response = true;
        };

        // At most this much received data waits for the game; past it the
        // receive loop stops reading and RFCOMM flow control slows the peer.
        constexpr std::size_t k_classic_receive_cap = 1024 * 1024;
        // At most this much accepted data waits to be sent.
        constexpr std::size_t k_classic_send_cap = 1024 * 1024;
        constexpr std::size_t k_classic_send_chunk = 64 * 1024;
        constexpr auto k_classic_connect_timeout = std::chrono::seconds(15);
        constexpr auto k_classic_connect_slice = std::chrono::milliseconds(100);

        struct ClassicConnectionState
        {
            explicit ClassicConnectionState(std::uint64_t h, std::uint64_t d)
                : handle(h), device(d)
            {
            }

            // A writer nothing joined (its receive loop never started) is let
            // go instead of terminating the process; the tracker counts it.
            ~ClassicConnectionState()
            {
                if (writer.joinable())
                    writer.detach();
            }

            std::uint64_t handle = 0;
            std::uint64_t device = 0;

            // Orders a connect's outcome against classic_disconnect: the
            // connect reports, and turns connected, only while closing is
            // still unset under this mutex, and classic_disconnect sets
            // closing under it. A disconnect is then either a cancel (the
            // core answers the game, the backend reports nothing) or a
            // disconnect of an open connection, never both.
            std::mutex connect_mutex;

            std::mutex socket_mutex;
            SOCKET socket = INVALID_SOCKET;
            std::atomic_bool connected{false};
            std::atomic_bool closing{false};

            std::mutex receive_mutex;
            // Signalled when the game reads (room again) and when the
            // connection must stop.
            std::condition_variable receive_cv;
            std::deque<std::uint8_t> received;
            // A ClassicDataAvailable went out and the game has not read
            // since; the next one waits for classic_receive_bytes.
            bool data_pending = false;
            // Set under receive_mutex when the receive loop has ended and the
            // socket is closed. A finished state stays registered only while
            // it holds bytes the game has not read.
            bool finished = false;

            // classic_send_bytes queues here; the writer thread is the only
            // caller of send() on the socket.
            std::mutex send_mutex;
            std::condition_variable send_cv;
            std::deque<std::uint8_t> outbound;
            // Queued plus the chunk the writer is sending.
            std::size_t send_pending = 0;
            bool writer_stop = false;
            bool send_failed = false;
            // The writer's failure, reported by the disconnect.
            Error send_error = Error::Ok;
            std::string send_error_message;
            // Joined by the receive loop before it closes the socket.
            std::thread writer;
        };

        // The receive loop thread is the sole owner of closesocket() for a
        // connection: it only closes after its own blocking recv() has
        // returned and after it has joined the connection's writer, so the
        // handle can never be reused while a call on it is in flight.
        // Every other caller (classic_disconnect, WindowsBackend::shutdown,
        // a failed send) must only ever request a shutdown() to unblock
        // those calls - never close the socket directly.
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

        void wake_receive_loop(const std::shared_ptr<ClassicConnectionState>& state)
        {
            {
                std::scoped_lock lock(state->receive_mutex);
            }
            state->receive_cv.notify_all();
        }

        // Asks a connection, connecting or open, to end: its threads notice
        // within one connect slice, or at once when blocked in a socket call
        // or waiting for room.
        void connection_request_close(const std::shared_ptr<ClassicConnectionState>& state)
        {
            {
                std::scoped_lock lock(state->connect_mutex);
                state->closing.store(true);
                state->connected.store(false);
            }
            connection_request_shutdown(state);
            wake_receive_loop(state);
        }

        struct SharedClassicState
        {
            CoreHooks hooks;
            std::shared_ptr<std::atomic_bool> alive =
                std::make_shared<std::atomic_bool>(true);
            std::shared_ptr<WorkerTracker> workers;

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
        // WindowsBackend::shutdown() closes the race where a connection
        // registered concurrently with shutdown() would never be asked to
        // close, leaving a thread blocked forever.
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

        // The writer: takes what classic_send_bytes queued and sends it, a
        // chunk at a time, so a slow peer stalls this thread and never the
        // game. A failed send is recorded for the disconnect and shuts the
        // socket down, which ends the receive loop.
        void run_writer(const std::shared_ptr<ClassicConnectionState>& state)
        {
            std::vector<std::uint8_t> chunk;
            for (;;)
            {
                {
                    std::unique_lock lock(state->send_mutex);
                    state->send_cv.wait(lock, [&state]()
                    {
                        return state->writer_stop || !state->outbound.empty();
                    });
                    if (state->writer_stop)
                        return;

                    const auto count = static_cast<std::ptrdiff_t>(
                        std::min(state->outbound.size(), k_classic_send_chunk));
                    chunk.assign(state->outbound.begin(), state->outbound.begin() + count);
                    state->outbound.erase(state->outbound.begin(), state->outbound.begin() + count);
                }

                std::size_t sent_total = 0;
                while (sent_total < chunk.size())
                {
                    const int sent = send(
                        connection_socket(state),
                        reinterpret_cast<const char*>(chunk.data() + sent_total),
                        static_cast<int>(chunk.size() - sent_total),
                        0);
                    if (sent > 0)
                    {
                        sent_total += static_cast<std::size_t>(sent);
                        continue;
                    }

                    const int error = sent == 0 ? WSAECONNRESET : WSAGetLastError();
                    bool report = false;
                    {
                        std::scoped_lock lock(state->send_mutex);
                        state->send_failed = true;
                        state->outbound.clear();
                        state->send_pending = 0;
                        // A send cut short by a disconnect the game or the
                        // receive loop asked for is not a failure.
                        report = !state->writer_stop && !state->closing.load();
                        if (report)
                        {
                            state->send_error = map_wsa_error(error);
                            state->send_error_message = wsa_message("send", error);
                        }
                    }
                    if (report)
                    {
                        state->connected.store(false);
                        connection_request_shutdown(state);
                        wake_receive_loop(state);
                    }
                    return;
                }

                std::scoped_lock lock(state->send_mutex);
                state->send_pending -= chunk.size();
            }
        }

        bool start_writer(
            const std::shared_ptr<SharedClassicState>& shared,
            const std::shared_ptr<ClassicConnectionState>& state)
        {
            state->writer = spawn_worker(shared->workers, [state]()
            {
                run_writer(state);
            });
            return state->writer.joinable();
        }

        // Stops the connection's writer and waits for it. writer_stop goes
        // first, so the failed send the shutdown causes is not reported, and
        // the shutdown unblocks a writer stuck in send().
        void stop_writer(const std::shared_ptr<ClassicConnectionState>& state)
        {
            {
                std::scoped_lock lock(state->send_mutex);
                state->writer_stop = true;
            }
            state->send_cv.notify_all();
            connection_request_shutdown(state);
            if (state->writer.joinable())
                state->writer.join();
        }

        // The end of an open connection, on the thread that owns its socket:
        // the writer is joined before the socket is closed, and the
        // disconnect reports the writer's failure when it had one. error and
        // message say why the receive side ended.
        void end_connection(
            const std::shared_ptr<SharedClassicState>& shared,
            const std::shared_ptr<ClassicConnectionState>& state,
            Error error,
            std::string message)
        {
            state->connected.store(false);
            stop_writer(state);

            {
                std::scoped_lock lock(state->send_mutex);
                if (state->send_error != Error::Ok)
                {
                    error = state->send_error;
                    message = state->send_error_message;
                }
            }

            if (shared->alive->load() && shared->hooks.push_event)
            {
                BackendEvent event;
                event.type = BackendEventType::ClassicDisconnected;
                event.transport = Transport::Classic;
                event.connection = state->handle;
                event.device = state->device;

                if (state->closing.load())
                {
                    event.error = Error::Ok;
                    event.message = "Disconnected";
                }
                else
                {
                    event.error = error;
                    event.message = std::move(message);
                }

                shared->hooks.push_event(std::move(event));
            }

            connection_close(state);

            // Bytes the game has not read yet outlive a remote hang-up:
            // the state stays registered until classic_receive drains it,
            // classic_disconnect drops it or the backend shuts down.
            bool keep = false;
            {
                std::scoped_lock lock(state->receive_mutex);
                state->finished = true;
                keep = !state->closing.load() &&
                       shared->alive->load() &&
                       !state->received.empty();
            }
            if (!keep)
                unregister_connection(shared, state->handle);
        }

        // An open connection's receive side. It stops reading while
        // k_classic_receive_cap bytes wait for the game, and announces data
        // once until the game reads.
        void run_receive_loop(
            const std::shared_ptr<SharedClassicState>& shared,
            const std::shared_ptr<ClassicConnectionState>& state)
        {
            std::vector<std::uint8_t> buffer(4096);
            Error error = Error::Ok;
            std::string message = "Disconnected";

            const auto stopping = [&shared, &state]()
            {
                return !state->connected.load() ||
                       state->closing.load() ||
                       !shared->alive->load();
            };

            for (;;)
            {
                {
                    std::unique_lock lock(state->receive_mutex);
                    state->receive_cv.wait(lock, [&state, &stopping]()
                    {
                        return state->received.size() < k_classic_receive_cap || stopping();
                    });
                }
                if (stopping())
                    break;

                const int received = recv(
                    connection_socket(state),
                    reinterpret_cast<char*>(buffer.data()),
                    static_cast<int>(buffer.size()),
                    0);

                if (received > 0)
                {
                    std::int32_t available = 0;
                    bool announce = false;
                    {
                        std::scoped_lock lock(state->receive_mutex);
                        state->received.insert(
                            state->received.end(),
                            buffer.begin(),
                            buffer.begin() + received);
                        available = static_cast<std::int32_t>(state->received.size());
                        announce = !state->data_pending;
                        state->data_pending = true;
                    }

                    if (announce && shared->alive->load() && shared->hooks.push_event)
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

                if (received == 0)
                {
                    // A peer that hung up is a disconnect the game did not ask
                    // for, as on Android and macOS (R1-77).
                    error = Error::Disconnected;
                    message = "Remote device disconnected";
                }
                else
                {
                    const int recv_error = WSAGetLastError();
                    error = map_wsa_error(recv_error);
                    message = wsa_message("recv", recv_error);
                }
                break;
            }

            end_connection(shared, state, error, std::move(message));
        }

        // Reports a connect's outcome unless classic_disconnect cancelled it
        // first; on success the connection turns connected in the same step.
        // False when it was cancelled (or the backend is shutting down) and
        // nothing was reported.
        bool complete_classic_connect(
            const std::shared_ptr<SharedClassicState>& shared,
            const std::shared_ptr<ClassicConnectionState>& state,
            Error error,
            std::string message)
        {
            std::scoped_lock lock(state->connect_mutex);
            if (state->closing.load() || !shared->alive->load())
                return false;

            if (error == Error::Ok)
                state->connected.store(true);

            if (shared->hooks.push_event)
            {
                BackendEvent event;
                event.type = BackendEventType::ClassicConnected;
                event.transport = Transport::Classic;
                event.connection = state->handle;
                event.device = state->device;
                event.error = error;
                event.message = std::move(message);
                shared->hooks.push_event(std::move(event));
            }
            return true;
        }

        // The connect worker. The state is registered (connecting) before
        // this runs, so classic_disconnect can cancel it. The connect is
        // non-blocking and polled in k_classic_connect_slice steps: a cancel
        // or a shutdown ends it within one, and a peer that never answers
        // fails after k_classic_connect_timeout. Once open, the connection's
        // receive loop runs on this same thread.
        void run_classic_connect(
            const std::shared_ptr<SharedClassicState>& shared,
            const std::shared_ptr<ClassicConnectionState>& state,
            BTH_ADDR address,
            GUID service_guid)
        {
            const auto abandon = [&shared, &state]()
            {
                stop_writer(state);
                connection_close(state);
                unregister_connection(shared, state->handle);
            };
            const auto fail = [&](Error error, std::string message)
            {
                complete_classic_connect(shared, state, error, std::move(message));
                abandon();
            };
            const auto fail_wsa = [&](const char* operation, int error)
            {
                fail(map_wsa_error(error), wsa_message(operation, error));
            };

            if (state->closing.load() || !shared->alive->load())
            {
                abandon();
                return;
            }

            const SOCKET socket_handle = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
            if (socket_handle == INVALID_SOCKET)
            {
                fail_wsa("socket", WSAGetLastError());
                return;
            }
            {
                std::scoped_lock lock(state->socket_mutex);
                state->socket = socket_handle;
            }

            u_long non_blocking = 1;
            if (ioctlsocket(socket_handle, FIONBIO, &non_blocking) == SOCKET_ERROR)
            {
                fail_wsa("ioctlsocket", WSAGetLastError());
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
                if (error != WSAEWOULDBLOCK)
                {
                    fail_wsa("connect", error);
                    return;
                }

                const auto deadline = std::chrono::steady_clock::now() + k_classic_connect_timeout;
                for (;;)
                {
                    if (state->closing.load() || !shared->alive->load())
                    {
                        abandon();
                        return;
                    }

                    fd_set writable;
                    FD_ZERO(&writable);
                    FD_SET(socket_handle, &writable);
                    fd_set failed;
                    FD_ZERO(&failed);
                    FD_SET(socket_handle, &failed);

                    timeval slice{};
                    slice.tv_sec = 0;
                    slice.tv_usec = static_cast<long>(
                        std::chrono::duration_cast<std::chrono::microseconds>(k_classic_connect_slice).count());

                    const int ready = select(0, nullptr, &writable, &failed, &slice);
                    if (ready == SOCKET_ERROR)
                    {
                        fail_wsa("select", WSAGetLastError());
                        return;
                    }

                    if (ready > 0 && FD_ISSET(socket_handle, &failed))
                    {
                        int connect_error = 0;
                        int length = sizeof(connect_error);
                        getsockopt(
                            socket_handle,
                            SOL_SOCKET,
                            SO_ERROR,
                            reinterpret_cast<char*>(&connect_error),
                            &length);
                        fail_wsa("connect", connect_error != 0 ? connect_error : WSAECONNREFUSED);
                        return;
                    }

                    if (ready > 0 && FD_ISSET(socket_handle, &writable))
                        break;

                    if (std::chrono::steady_clock::now() >= deadline)
                    {
                        fail(Error::Timeout, "RFCOMM connect timed out");
                        return;
                    }
                }
            }

            u_long blocking = 0;
            if (ioctlsocket(socket_handle, FIONBIO, &blocking) == SOCKET_ERROR)
            {
                fail_wsa("ioctlsocket", WSAGetLastError());
                return;
            }

            if (!start_writer(shared, state))
            {
                fail(Error::OperationFailed, "Could not start a thread for the Classic connection");
                return;
            }

            if (!complete_classic_connect(shared, state, Error::Ok, std::string()))
            {
                abandon();
                return;
            }

            run_receive_loop(shared, state);
        }

        // A client the RFCOMM server accepted: announced, then served by its
        // own receive loop and writer.
        void start_accepted_connection(
            const std::shared_ptr<SharedClassicState>& shared,
            SOCKET client,
            std::uint64_t connection,
            std::uint64_t device_handle)
        {
            auto state = std::make_shared<ClassicConnectionState>(
                connection, device_handle);
            state->socket = client;
            state->connected.store(true);

            if (!register_connection_if_alive(shared, state))
            {
                connection_close(state);
                return;
            }

            const bool writer_started = start_writer(shared, state);

            if (shared->hooks.push_event)
            {
                BackendEvent event;
                event.type = BackendEventType::ClassicClientConnected;
                event.transport = Transport::Classic;
                event.connection = connection;
                event.device = device_handle;
                event.error = Error::Ok;
                shared->hooks.push_event(std::move(event));
            }

            const bool loop_started = writer_started &&
                spawn_detached_worker(shared->workers, [shared, state]()
                {
                    run_receive_loop(shared, state);
                });
            if (!loop_started)
            {
                end_connection(
                    shared,
                    state,
                    Error::OperationFailed,
                    "Could not start a thread for the Classic connection");
            }
        }

        // One RFCOMM server's accept loop. The listening socket comes by
        // value and is closed by classic_server_stop under mutex; running is
        // this server's own flag, so a loop outliving its server never sees
        // a newer one's.
        struct ClassicServerState
        {
            std::mutex mutex;
            std::condition_variable wake;
            SOCKET listener = INVALID_SOCKET;
            bool running = true;
        };

        void run_classic_accept_loop(
            const std::shared_ptr<SharedClassicState>& shared,
            const std::shared_ptr<ClassicServerState>& server,
            SOCKET listener)
        {
            constexpr auto first_backoff = std::chrono::milliseconds(250);
            constexpr auto max_backoff = std::chrono::milliseconds(2000);
            auto backoff = first_backoff;

            for (;;)
            {
                {
                    std::scoped_lock lock(server->mutex);
                    if (!server->running)
                        return;
                }

                SOCKADDR_BTH remote{};
                int remote_len = sizeof(remote);
                const SOCKET client = accept(
                    listener,
                    reinterpret_cast<sockaddr*>(&remote),
                    &remote_len);

                if (client == INVALID_SOCKET)
                {
                    const std::string failure = wsa_message("accept", WSAGetLastError());

                    std::unique_lock lock(server->mutex);
                    if (!server->running)
                        return;

                    // Still meant to run: an error that repeats must not
                    // spin, so wait before the next try.
                    GMBT_LOG(
                        "RFCOMM %s; retrying in %d ms",
                        failure.c_str(),
                        static_cast<int>(backoff.count()));
                    server->wake.wait_for(lock, backoff, [&server]() { return !server->running; });
                    if (!server->running)
                        return;
                    backoff = std::min(backoff * 2, max_backoff);
                    continue;
                }

                backoff = first_backoff;

                bool stopped = false;
                {
                    std::scoped_lock lock(server->mutex);
                    stopped = !server->running;
                }
                if (stopped || !shared->alive->load())
                {
                    closesocket(client);
                    return;
                }

                DiscoveredDevice device;
                device.transport = Transport::Classic;
                device.address = format_bluetooth_address(remote.btAddr);
                device.id = "win:classic:" + device.address;
                device.address_available = true;
                device.connectable = true;

                const std::uint64_t device_handle = shared->hooks.upsert_device
                    ? shared->hooks.upsert_device(device)
                    : 0;
                const std::uint64_t connection = shared->hooks.create_classic_connection
                    ? shared->hooks.create_classic_connection(device_handle)
                    : 0;

                if (!connection)
                {
                    closesocket(client);
                    continue;
                }

                start_accepted_connection(shared, client, connection, device_handle);
            }
        }

        // Classic discovery. A stop bumps the generation and reports the
        // stop itself, so an inquiry thread whose generation is stale ends
        // unobserved.
        struct ClassicScanState
        {
            std::mutex mutex;
            std::atomic<std::uint64_t> generation{0};
            std::atomic_bool running{false};
        };

        DiscoveredDevice classic_device(const BLUETOOTH_DEVICE_INFO& info)
        {
            DiscoveredDevice device;
            device.transport = Transport::Classic;
            device.address = format_bluetooth_address(info.Address.ullLong);
            device.id = "win:classic:" + device.address;
            device.address_available = true;
            device.name = wide_to_utf8(info.szName);
            device.connectable = true;
            device.rssi_available = false;
            return device;
        }

        // Every device Windows remembers, by address, with its stLastSeen as
        // it stood before an inquiry; no inquiry runs. nullopt when Windows
        // could not list them, and the scan then keeps every result.
        std::optional<std::unordered_map<BTH_ADDR, SYSTEMTIME>> remembered_devices_last_seen()
        {
            BLUETOOTH_DEVICE_SEARCH_PARAMS search{};
            search.dwSize = sizeof(search);
            search.fReturnAuthenticated = TRUE;
            search.fReturnRemembered = TRUE;
            search.fReturnUnknown = FALSE;
            search.fReturnConnected = TRUE;
            search.fIssueInquiry = FALSE;
            search.cTimeoutMultiplier = 0;
            search.hRadio = nullptr;

            BLUETOOTH_DEVICE_INFO info{};
            info.dwSize = sizeof(info);

            std::unordered_map<BTH_ADDR, SYSTEMTIME> out;
            HBLUETOOTH_DEVICE_FIND finder = BluetoothFindFirstDevice(&search, &info);
            if (!finder)
            {
                const DWORD find_error = GetLastError();
                if (find_error == ERROR_NO_MORE_ITEMS)
                    return out;
                GMBT_LOG(
                    "Classic scan reports remembered devices too: %s",
                    system_error_message("BluetoothFindFirstDevice", find_error).c_str());
                return std::nullopt;
            }

            do
            {
                out[info.Address.ullLong] = info.stLastSeen;
                info = {};
                info.dwSize = sizeof(info);
            }
            while (BluetoothFindNextDevice(finder, &info));

            BluetoothFindDeviceClose(finder);
            return out;
        }

        // A result answered the inquiry when Windows did not remember it
        // before, or its stLastSeen moved since the snapshot.
        bool answered_inquiry(
            const std::optional<std::unordered_map<BTH_ADDR, SYSTEMTIME>>& before,
            const BLUETOOTH_DEVICE_INFO& info)
        {
            if (!before)
                return true;
            const auto it = before->find(info.Address.ullLong);
            return it == before->end() ||
                std::memcmp(&it->second, &info.stLastSeen, sizeof(SYSTEMTIME)) != 0;
        }

        // The Classic devices Windows holds a bond with, in range or not;
        // no inquiry runs, so it answers at once.
        Error find_paired_classic_devices(std::vector<DiscoveredDevice>& out, std::string& message)
        {
            BLUETOOTH_DEVICE_SEARCH_PARAMS search{};
            search.dwSize = sizeof(search);
            search.fReturnAuthenticated = TRUE;
            search.fReturnRemembered = FALSE;
            search.fReturnUnknown = FALSE;
            search.fReturnConnected = FALSE;
            search.fIssueInquiry = FALSE;
            search.cTimeoutMultiplier = 0;
            search.hRadio = nullptr;

            BLUETOOTH_DEVICE_INFO info{};
            info.dwSize = sizeof(info);

            HBLUETOOTH_DEVICE_FIND finder = BluetoothFindFirstDevice(&search, &info);
            if (!finder)
            {
                const DWORD find_error = GetLastError();
                if (find_error == ERROR_NO_MORE_ITEMS)
                    return Error::Ok;
                message = system_error_message("BluetoothFindFirstDevice", find_error);
                return Error::OperationFailed;
            }

            do
            {
                out.push_back(classic_device(info));
                info = {};
                info.dwSize = sizeof(info);
            }
            while (BluetoothFindNextDevice(finder, &info));

            BluetoothFindDeviceClose(finder);
            return Error::Ok;
        }

        void run_classic_inquiry(
            const std::shared_ptr<SharedClassicState>& shared,
            const std::shared_ptr<ClassicScanState>& scan,
            std::uint64_t generation)
        {
            const auto current = [&shared, &scan, generation]()
            {
                return scan->generation.load() == generation && shared->alive->load();
            };

            // An inquiry that could not run is reported as such, not as a scan
            // that found nothing (R1-68).
            Error scan_error = Error::Ok;
            std::string scan_message;
            if (current())
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

                // The flags above also return every remembered device, in
                // range or not, but clearing them would hide a paired device
                // that did answer (R1-145). What answered has a stLastSeen
                // newer than before the inquiry; its time base is not
                // documented, so it is compared only with itself, never with
                // the clock.
                const auto before = remembered_devices_last_seen();

                HBLUETOOTH_DEVICE_FIND finder =
                    BluetoothFindFirstDevice(&search, &info);

                if (finder)
                {
                    do
                    {
                        if (!current())
                            break;

                        if (answered_inquiry(before, info) && shared->hooks.upsert_device)
                            shared->hooks.upsert_device(classic_device(info));

                        info = {};
                        info.dwSize = sizeof(info);
                    }
                    while (BluetoothFindNextDevice(finder, &info));

                    BluetoothFindDeviceClose(finder);
                }
                else if (const DWORD find_error = GetLastError(); find_error != ERROR_NO_MORE_ITEMS)
                {
                    scan_error = Error::OperationFailed;
                    scan_message = system_error_message("BluetoothFindFirstDevice", find_error);
                }
            }

            // The end of a scan nobody stopped is this thread's to report.
            bool report = false;
            {
                std::scoped_lock lock(scan->mutex);
                if (scan->generation.load() == generation)
                {
                    scan->running.store(false);
                    report = true;
                }
            }

            if (report && shared->alive->load() && shared->hooks.push_event)
            {
                BackendEvent event;
                event.type = BackendEventType::ScanStopped;
                event.transport = Transport::Classic;
                event.error = scan_error;
                event.message = std::move(scan_message);
                shared->hooks.push_event(std::move(event));
            }
        }

        // Server notifies leave the game thread in order: one NotifyValueAsync
        // in flight at a time, the next started from the previous one's
        // completion. An Indicate waits for every subscriber's confirmation,
        // which is why none of this may block the caller.
        struct PendingNotify
        {
            WDBG::GattLocalCharacteristic characteristic{nullptr};
            // Set for a notify to one central; null broadcasts to every subscriber.
            WDBG::GattSubscribedClient client{nullptr};
            std::vector<std::uint8_t> bytes;
        };

        struct NotifyQueueState
        {
            std::mutex mutex;
            std::deque<PendingNotify> queue;
            bool in_flight = false;
        };

        constexpr std::size_t k_max_queued_notifies = 64;

        using NotifyOperation = WF::IAsyncOperation<
            WF::Collections::IVectorView<WDBG::GattClientNotificationResult>>;

        // The call is a sync int with no callback, so a client that failed
        // can only be logged.
        void log_notify_failures(const NotifyOperation& operation, WF::AsyncStatus status)
        {
            try
            {
                if (status != WF::AsyncStatus::Completed)
                {
                    GMBT_LOG("GATT notify did not complete: status=%d", static_cast<int>(status));
                    return;
                }

                for (const auto& result : operation.GetResults())
                {
                    if (result.Status() == WDBG::GattCommunicationStatus::Success)
                        continue;

                    GMBT_LOG(
                        "GATT notify failed for a subscriber: status=%d",
                        static_cast<int>(result.Status()));
                }
            }
            catch (const winrt::hresult_error& error)
            {
                GMBT_LOG("GATT notify failed: %s", winrt::to_string(error.message()).c_str());
            }
        }

        void start_next_notify(const std::shared_ptr<NotifyQueueState>& notifies)
        {
            for (;;)
            {
                PendingNotify next;
                {
                    std::scoped_lock lock(notifies->mutex);
                    if (notifies->queue.empty())
                    {
                        notifies->in_flight = false;
                        return;
                    }
                    next = std::move(notifies->queue.front());
                    notifies->queue.pop_front();
                    notifies->in_flight = true;
                }

                try
                {
                    if (next.client)
                    {
                        auto operation = next.characteristic.NotifyValueAsync(bytes_to_buffer(next.bytes), next.client);
                        operation.Completed(
                            [notifies](const WF::IAsyncOperation<WDBG::GattClientNotificationResult>& done, WF::AsyncStatus status)
                            {
                                try
                                {
                                    if (status != WF::AsyncStatus::Completed)
                                        GMBT_LOG("GATT notify did not complete: status=%d", static_cast<int>(status));
                                    else if (done.GetResults().Status() != WDBG::GattCommunicationStatus::Success)
                                        GMBT_LOG("GATT notify failed for its central: status=%d", static_cast<int>(done.GetResults().Status()));
                                }
                                catch (const winrt::hresult_error& error)
                                {
                                    GMBT_LOG("GATT notify failed: %s", winrt::to_string(error.message()).c_str());
                                }
                                start_next_notify(notifies);
                            });
                        return;
                    }

                    auto operation = next.characteristic.NotifyValueAsync(bytes_to_buffer(next.bytes));
                    operation.Completed(
                        [notifies](const NotifyOperation& done, WF::AsyncStatus status)
                        {
                            log_notify_failures(done, status);
                            start_next_notify(notifies);
                        });
                    return;
                }
                catch (const winrt::hresult_error& error)
                {
                    // The characteristic went away (server stopped, services
                    // cleared): drop this one and go on with the queue.
                    GMBT_LOG("GATT notify could not start: %s", winrt::to_string(error.message()).c_str());
                }
            }
        }

        void clear_queued_notifies(const std::shared_ptr<NotifyQueueState>& notifies)
        {
            std::scoped_lock lock(notifies->mutex);
            notifies->queue.clear();
        }

        // An advertise_start in flight. Publisher and service providers start
        // asynchronously; the call completes once each one it started has
        // reported Started, or with the first that aborts or stops first.
        struct AdvertiseStartTracker
        {
            std::uint64_t op_id = 0;
            std::uint64_t generation = 0;
            std::atomic<std::size_t> remaining{0};
            std::atomic_bool done{false};

            // Copies of what this start began, so a failure can stop them
            // without touching the backend's own members off the game thread.
            std::mutex mutex;
            WDBA::BluetoothLEAdvertisementPublisher publisher{nullptr};
            std::vector<std::pair<WDBG::GattServiceProvider, winrt::event_token>> providers;
        };
    }


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

    // One thread running jobs in order. It holds an MTA init for its whole
    // life, so a job may wait on a WinRT async call; while any worker runs,
    // the process has an MTA, which is what the game thread's own
    // non-blocking WinRT calls run in - it never initializes COM itself.
    class SerialWorker
    {
    public:
        // Null when Windows could not start the thread.
        static std::shared_ptr<SerialWorker> start(const std::shared_ptr<WorkerTracker>& tracker)
        {
            auto worker = std::make_shared<SerialWorker>();
            if (!spawn_detached_worker(tracker, [worker]() { worker->run(); }))
                return nullptr;
            return worker;
        }

        // False once the worker has stopped; the job is then not run.
        bool post(std::function<void()> job)
        {
            {
                std::scoped_lock lock(mutex_);
                if (stopped_)
                    return false;
                jobs_.push_back(std::move(job));
            }
            wake_.notify_one();
            return true;
        }

        // The running job finishes; the queued ones are dropped unrun.
        void stop()
        {
            {
                std::scoped_lock lock(mutex_);
                stopped_ = true;
            }
            wake_.notify_one();
        }

    private:
        void run()
        {
            std::optional<WinrtWorkerApartment> apartment;
            try
            {
                apartment.emplace();
            }
            catch (const winrt::hresult_error& error)
            {
                GMBT_LOG("Bluetooth worker could not enter the MTA: %s", winrt::to_string(error.message()).c_str());
                stop();
            }

            for (;;)
            {
                std::function<void()> job;
                // Released here, inside the apartment, with what they captured.
                std::deque<std::function<void()>> dropped;
                {
                    std::unique_lock lock(mutex_);
                    wake_.wait(lock, [this]() { return stopped_ || !jobs_.empty(); });
                    if (stopped_)
                    {
                        dropped.swap(jobs_);
                    }
                    else
                    {
                        job = std::move(jobs_.front());
                        jobs_.pop_front();
                    }
                }

                if (!job)
                    return;

                try
                {
                    job();
                }
                catch (const winrt::hresult_error& error)
                {
                    GMBT_LOG("Bluetooth worker job failed: %s", winrt::to_string(error.message()).c_str());
                }
                catch (const std::exception& error)
                {
                    GMBT_LOG("Bluetooth worker job failed: %s", error.what());
                }
                catch (...)
                {
                    GMBT_LOG("Bluetooth worker job failed");
                }
            }
        }

        std::mutex mutex_;
        std::condition_variable wake_;
        std::deque<std::function<void()>> jobs_;
        bool stopped_ = false;
    };

    // Remote attributes are keyed by their WinRT AttributeHandle, the ATT
    // handle: unique in the device's GATT database and never 0, so it is the
    // instance discovery reports to the core and two attributes that share a
    // UUID stay apart (R1-127).
    struct RemoteGattDescriptorState
    {
        std::string uuid;
        std::uint16_t handle = 0;
        WDBG::GattDescriptor descriptor{nullptr};
    };

    // Every field is read and written under the connection's mutex.
    struct RemoteGattCharacteristicState
    {
        std::string uuid;
        std::uint16_t handle = 0;
        WDBG::GattCharacteristic characteristic{nullptr};
        winrt::event_token value_changed_token{};
        bool value_changed_registered = false;
        std::unordered_map<std::uint16_t, std::shared_ptr<RemoteGattDescriptorState>> descriptors;
    };

    struct RemoteGattServiceState
    {
        std::string uuid;
        std::uint16_t handle = 0;
        WDBG::GattDeviceService service{nullptr};
        std::unordered_map<std::uint16_t, std::shared_ptr<RemoteGattCharacteristicState>> characteristics;
    };

    struct RemoteGattConnectionState
    {
        std::uint64_t handle = 0;
        std::uint64_t device_handle = 0;
        std::atomic_bool connected{false};
        std::atomic_bool closing{false};
        // Set with closing when the peer dropped the link, not the game.
        std::atomic_bool peer_dropped{false};
        // Runs this connection's open and every op on it, in order.
        std::shared_ptr<SerialWorker> worker;
        WDB::BluetoothLEDevice device{nullptr};
        // Held with MaintainConnection for the link's life (R1-114).
        WDBG::GattSession session{nullptr};
        // The newest connection priority request, held for the link's life:
        // closing it withdraws the request (R1-142).
        WDB::BluetoothLEPreferredConnectionParametersRequest connection_parameters{nullptr};
        winrt::event_token connection_status_token{};
        bool connection_status_registered = false;
        mutable std::mutex mutex;
        std::unordered_map<std::uint16_t, std::shared_ptr<RemoteGattServiceState>> services;
    };

    struct SharedLeClientState
    {
        CoreHooks hooks;
        std::shared_ptr<std::atomic_bool> alive =
            std::make_shared<std::atomic_bool>(true);
        // The backend worker, set by initialize before any connection exists:
        // it closes a link the peer dropped.
        std::shared_ptr<SerialWorker> worker;

        mutable std::mutex connections_mutex;
        std::unordered_map<std::uint64_t, std::shared_ptr<RemoteGattConnectionState>> connections;
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

    // A failed bluetooth_le_peripheral_open. error_code keeps the old
    // status (133 for a peer that did not answer); error and message are
    // what the core reports.
    std::string le_error_json(
        std::int32_t error_code,
        std::uint64_t connection,
        Error error,
        const std::string& message)
    {
        std::string out = "{";
        if (connection != 0)
            out += "\"connection\":" + std::to_string(connection) + ",";
        out += "\"error_code\":" + std::to_string(error_code) +
            ",\"error\":" + std::to_string(static_cast<std::int32_t>(error)) +
            ",\"message\":\"" + json_escape(message) + "\"}";
        return out;
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
        push_le_client_event(
            shared,
            "bluetooth_le_peripheral_connection_state_changed",
            "{\"connection\":" +
                std::to_string(current->handle) +
                ",\"is_connected\":false" +
                ",\"error\":" +
                std::to_string(static_cast<std::int32_t>(Error::Disconnected)) +
                ",\"message\":\"The Bluetooth LE device disconnected\"}");

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
        const auto fail_open = [&shared, &state](std::int32_t error_code, Error failure, const std::string& why)
        {
            state->closing.store(true);
            close_le_client_connection_noexcept(state);
            unregister_le_client_connection(shared, state->handle);
            push_le_client_event(
                shared,
                "bluetooth_le_peripheral_open",
                le_error_json(error_code, state->handle, failure, why));
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
                fail_open(1, Error::ConnectionFailed, "Windows found no Bluetooth LE device at this address");
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
                push_le_client_event(
                    shared,
                    "bluetooth_le_peripheral_open",
                    le_error_json(1, state->handle, Error::ConnectionFailed, cancelled_message()));
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
                fail_open(133, Error::Timeout, "The Bluetooth LE device did not answer within 15 seconds");
                return;
            }
            const auto services_result = services_query.GetResults();

            if (services_result.Status() !=
                WDBG::GattCommunicationStatus::Success)
            {
                // A peer that never answered is a timeout (the old 133); a
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

                fail_open(unreachable ? 133 : 1, outcome.error, outcome.message);
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
                push_le_client_event(
                    shared,
                    "bluetooth_le_peripheral_open",
                    le_error_json(1, state->handle, Error::ConnectionFailed, cancelled_message()));
                return;
            }

            push_le_client_event(
                shared,
                "bluetooth_le_peripheral_open",
                "{\"connection\":" +
                    std::to_string(state->handle) + "}");
        }
        catch (const winrt::hresult_error& error)
        {
            fail_open(1, Error::ConnectionFailed, gatt_exception_outcome(error).message);
        }
        catch (...)
        {
            fail_open(1, Error::ConnectionFailed, "Opening the Bluetooth LE device failed with an unexpected exception");
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

    // A local GATT service as le_server_add_service parsed it on the game
    // thread, for the worker to create.
    struct LocalDescriptorDefinition
    {
        winrt::guid guid{};
        std::string uuid;
    };

    struct LocalCharacteristicDefinition
    {
        winrt::guid guid{};
        std::string uuid;
        std::int32_t properties = 0;
        std::int32_t permissions = 0;
        std::vector<std::uint8_t> value;
        std::vector<LocalDescriptorDefinition> descriptors;
    };

    struct LocalServiceDefinition
    {
        winrt::guid guid{};
        std::string uuid;
        std::vector<LocalCharacteristicDefinition> characteristics;
    };

    // A remote central's GattSession, kept from its first server event
    // until it closes. Windows has no "central connected" event for a GATT
    // server; the first request or subscription stands for it, and the
    // session closing is the disconnect (R1-23).
    struct ServerSession
    {
        WDBG::GattSession session{nullptr};
        winrt::event_token status_token{};
    };

    // The adapter DeviceWatcher and the handlers registered on it.
    struct AdapterWatch
    {
        WDE::DeviceWatcher watcher{nullptr};
        winrt::event_token added_token{};
        winrt::event_token removed_token{};
        winrt::event_token updated_token{};
        winrt::event_token enumerated_token{};
    };

    // What the radio, scan, advertise and GATT server handlers touch. They
    // hold it weakly and never the backend, so a handler still running on
    // a WinRT thread during bluetooth_shutdown finds it gone or dead
    // instead of a freed backend (R1-105).
    struct SharedLeState
    {
        CoreHooks hooks;
        std::atomic_bool alive{true};

        std::atomic<std::int32_t> bluetooth_state{0};
        std::atomic_bool ble_supported{false};
        std::atomic_bool le_peripheral_supported{false};
        std::atomic_bool classic_supported{false};
        // BluetoothLEDevice.RequestPreferredConnectionParameters exists
        // (Windows 11); asked once by initialize.
        std::atomic_bool connection_parameters_api{false};
        std::mutex radio_mutex;
        WDR::Radio radio{nullptr};
        winrt::event_token radio_state_token{};

        // Watches for a Bluetooth adapter coming or going (R1-229). What it
        // reports after its first enumeration posts one adapter re-query to
        // the backend worker; a burst while one is pending posts nothing.
        std::mutex adapter_watch_mutex;
        AdapterWatch adapter_watch;
        std::atomic_bool adapter_enumerated{false};
        std::atomic_bool adapter_requery_pending{false};

        std::atomic_bool le_scanning{false};
        // Bumped by every scan start and stop; a watcher's handlers act only
        // while theirs is current (R1-187).
        std::atomic<std::uint64_t> scan_generation{0};
        std::mutex address_types_mutex;
        std::unordered_map<std::uint64_t, WDB::BluetoothAddressType> address_types;

        std::atomic_bool le_advertising{false};
        std::atomic<std::uint64_t> advertise_generation{0};
        // Held where a start claims le_advertising and bumps the generation,
        // and where a publisher's late status clears the flag, so an older
        // publisher can never clear a newer start's flag (R1-242).
        std::mutex advertise_flag_mutex;

        // Guards the services, the adds in flight, the server generation
        // and the advertise parameters start_service_advertising_locked reads.
        std::mutex services_mutex;
        std::unordered_map<std::string, std::shared_ptr<LocalGattServiceState>> gatt_services;
        std::unordered_set<std::string> adding_services;
        // Bumped by clear and stop; an add finishing under an older one is
        // dropped.
        std::uint64_t server_generation = 0;
        bool advertise_connectable = true;
        bool advertise_discoverable = true;
        // The registered services advertise_start listed: only these advertise.
        std::unordered_set<std::string> advertise_service_uuids;
        std::unordered_map<std::string, std::vector<std::uint8_t>> advertise_service_data;

        std::mutex gatt_request_mutex;
        std::unordered_map<std::int32_t, PendingGattRead> pending_gatt_reads;
        std::unordered_map<std::int32_t, PendingGattWrite> pending_gatt_writes;
        std::atomic<std::int32_t> next_gatt_request_id{1};

        std::mutex server_sessions_mutex;
        std::unordered_map<std::string, ServerSession> server_sessions;
    };

    void push_le_event(const std::shared_ptr<SharedLeState>& le, BackendEvent event)
    {
        if (!le || !le->alive.load() || !le->hooks.push_event)
            return;
        le->hooks.push_event(std::move(event));
    }

    void complete_le_op(const std::shared_ptr<SharedLeState>& le, std::uint64_t op_id, Error error, std::string message)
    {
        BackendEvent event;
        event.type = BackendEventType::LeOpCompleted;
        event.transport = Transport::LowEnergy;
        event.op_id = op_id;
        event.error = error;
        event.message = std::move(message);
        push_le_event(le, std::move(event));
    }

    void push_bluetooth_state(const std::shared_ptr<SharedLeState>& le)
    {
        BackendEvent event;
        event.type = BackendEventType::LeEvent;
        event.transport = Transport::Unknown;
        event.event_type = "bluetooth_state_changed";
        event.json = "{\"state\":" + std::to_string(le->bluetooth_state.load()) + "}";
        push_le_event(le, std::move(event));
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

    // BluetoothState raw values from spec.gmidl.
    constexpr std::int32_t k_state_unknown = 0;
    constexpr std::int32_t k_state_unsupported = 2;

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
                device.id = "win:ble:" + device.address;
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

    void push_server_connection_state(
        const std::shared_ptr<SharedLeState>& le,
        const std::string& central,
        const std::string& address,
        bool connected)
    {
        if (central.empty())
            return;

        BackendEvent event;
        event.type = BackendEventType::LeEvent;
        event.transport = Transport::LowEnergy;
        event.event_type = "bluetooth_le_server_connection_state_changed";
        event.json = std::string("{\"connected\":") + (connected ? "true" : "false") + "," +
            server_central_json(central, address) + "}";
        push_le_event(le, std::move(event));
    }

    void server_session_closed(
        const std::shared_ptr<SharedLeState>& le,
        const std::string& central,
        const std::string& address)
    {
        ServerSession entry;
        {
            std::scoped_lock lock(le->server_sessions_mutex);
            const auto it = le->server_sessions.find(central);
            if (it == le->server_sessions.end())
                return;
            entry = std::move(it->second);
            le->server_sessions.erase(it);
        }

        try
        {
            entry.session.SessionStatusChanged(entry.status_token);
        }
        catch (...)
        {
        }
        push_server_connection_state(le, central, address, false);
    }

    // The key the core knows the central by - its session's device id -
    // registering the session on first sight; is_new says it was. Empty for
    // a session that names no device.
    std::string note_server_session(
        const std::shared_ptr<SharedLeState>& le,
        const WDBG::GattSession& session,
        std::string& address,
        bool& is_new)
    {
        is_new = false;
        if (!session)
            return std::string();

        std::string central;
        try
        {
            central = winrt::to_string(session.DeviceId().Id());
        }
        catch (...)
        {
            return std::string();
        }
        address = address_from_device_id(central);

        std::scoped_lock lock(le->server_sessions_mutex);
        if (le->server_sessions.find(central) != le->server_sessions.end())
            return central;

        ServerSession entry;
        entry.session = session;
        try
        {
            std::weak_ptr<SharedLeState> weak = le;
            entry.status_token = session.SessionStatusChanged(
                [weak, central, address](const WDBG::GattSession&, const WDBG::GattSessionStatusChangedEventArgs& args)
                {
                    const auto current = weak.lock();
                    if (current && current->alive.load() && args.Status() == WDBG::GattSessionStatus::Closed)
                        server_session_closed(current, central, address);
                });
        }
        catch (...)
        {
        }
        le->server_sessions.emplace(central, std::move(entry));
        is_new = true;
        return central;
    }

    // The server stopped: the core retires every central itself, so the
    // sessions are only let go.
    void release_server_sessions_noexcept(const std::shared_ptr<SharedLeState>& le) noexcept
    {
        std::unordered_map<std::string, ServerSession> sessions;
        {
            std::scoped_lock lock(le->server_sessions_mutex);
            sessions.swap(le->server_sessions);
        }

        for (auto& [_, entry] : sessions)
        {
            try
            {
                entry.session.SessionStatusChanged(entry.status_token);
            }
            catch (...)
            {
            }
        }
    }

    // Caller holds le.services_mutex.
    void start_service_advertising_locked(
        SharedLeState& le,
        const std::shared_ptr<LocalGattServiceState>& service)
    {
        if (!service || !service->provider)
            return;
        if (le.advertise_service_uuids.find(service->uuid) == le.advertise_service_uuids.end())
            return;

        WDBG::GattServiceProviderAdvertisingParameters parameters;
        parameters.IsConnectable(le.advertise_connectable);
        parameters.IsDiscoverable(le.advertise_discoverable);

        const auto data_it = le.advertise_service_data.find(service->uuid);
        if (data_it != le.advertise_service_data.end() && !data_it->second.empty())
            parameters.ServiceData(bytes_to_buffer(data_it->second));

        service->provider.StartAdvertising(parameters);
    }

    void revoke_advertise_handlers(const std::shared_ptr<AdvertiseStartTracker>& tracker) noexcept
    {
        std::vector<std::pair<WDBG::GattServiceProvider, winrt::event_token>> providers;
        {
            std::scoped_lock lock(tracker->mutex);
            providers.swap(tracker->providers);
        }

        for (auto& [provider, token] : providers)
        {
            try
            {
                provider.AdvertisementStatusChanged(token);
            }
            catch (...)
            {
            }
        }
    }

    // Completes the start once. False when it was already completed.
    bool finish_advertise_start(
        const std::shared_ptr<SharedLeState>& le,
        const std::shared_ptr<AdvertiseStartTracker>& tracker,
        Error error,
        std::string message)
    {
        if (tracker->done.exchange(true))
            return false;

        revoke_advertise_handlers(tracker);
        complete_le_op(le, tracker->op_id, error, std::move(message));
        return true;
    }

    void advertise_start_progress(
        const std::shared_ptr<SharedLeState>& le,
        const std::shared_ptr<AdvertiseStartTracker>& tracker)
    {
        if (tracker->remaining.fetch_sub(1) == 1)
            finish_advertise_start(le, tracker, Error::Ok, {});
    }

    // A failed start leaves nothing it began advertising, unless a newer
    // start already owns the advertisers.
    void advertise_start_failed(
        const std::shared_ptr<SharedLeState>& le,
        const std::shared_ptr<AdvertiseStartTracker>& tracker,
        Error error,
        std::string message)
    {
        WDBA::BluetoothLEAdvertisementPublisher publisher{nullptr};
        std::vector<WDBG::GattServiceProvider> providers;
        {
            std::scoped_lock lock(tracker->mutex);
            publisher = tracker->publisher;
            for (const auto& pair : tracker->providers)
                providers.push_back(pair.first);
        }

        if (!finish_advertise_start(le, tracker, error, std::move(message)))
            return;

        if (tracker->generation != le->advertise_generation.load())
            return;

        try
        {
            if (publisher)
                publisher.Stop();
        }
        catch (...)
        {
        }

        for (auto& provider : providers)
        {
            try
            {
                provider.StopAdvertising();
            }
            catch (...)
            {
            }
        }

        le->le_advertising.store(false);
    }

    // A read request on a local characteristic, or on one of its
    // descriptors when descriptor_uuid is set; on a WinRT thread.
    void on_local_read_request(
        const std::shared_ptr<SharedLeState>& le,
        const std::string& service_uuid,
        const std::string& characteristic_uuid,
        const std::string& descriptor_uuid,
        const WDBG::GattReadRequestedEventArgs& args)
    {
        const auto deferral = args.GetDeferral();
        try
        {
            if (!le || !le->alive.load())
            {
                deferral.Complete();
                return;
            }

            const auto request = args.GetRequestAsync().get();
            if (!request)
            {
                deferral.Complete();
                return;
            }

            const std::int32_t request_id = le->next_gatt_request_id.fetch_add(1);
            {
                std::scoped_lock lock(le->gatt_request_mutex);
                le->pending_gatt_reads[request_id] = PendingGattRead{request, deferral};
            }

            std::string central_address;
            bool central_is_new = false;
            const std::string central = note_server_session(le, args.Session(), central_address, central_is_new);

            BackendEvent event;
            event.type = BackendEventType::LeEvent;
            event.transport = Transport::LowEnergy;
            event.event_type = descriptor_uuid.empty()
                ? "bluetooth_le_server_characteristic_read_request"
                : "bluetooth_le_server_descriptor_read_request";
            event.json = make_server_request_json(
                request_id,
                central,
                central_address,
                service_uuid,
                characteristic_uuid,
                descriptor_uuid,
                request.Offset(),
                nullptr);
            push_le_event(le, std::move(event));
        }
        catch (...)
        {
            try { deferral.Complete(); } catch (...) {}
        }
    }

    // A write request on a local characteristic, or on one of its
    // descriptors when descriptor_uuid is set; on a WinRT thread.
    void on_local_write_request(
        const std::shared_ptr<SharedLeState>& le,
        const std::string& service_uuid,
        const std::string& characteristic_uuid,
        const std::string& descriptor_uuid,
        const WDBG::GattWriteRequestedEventArgs& args)
    {
        const auto deferral = args.GetDeferral();
        try
        {
            if (!le || !le->alive.load())
            {
                deferral.Complete();
                return;
            }

            const auto request = args.GetRequestAsync().get();
            if (!request)
            {
                deferral.Complete();
                return;
            }

            const std::vector<std::uint8_t> value = buffer_to_bytes(request.Value());
            const bool with_response = request.Option() == WDBG::GattWriteOption::WriteWithResponse;
            const std::int32_t request_id = le->next_gatt_request_id.fetch_add(1);
            // A write without response needs no answer: it is completed now and
            // the core keeps its value for GML, instead of the deferral waiting
            // on a respond_write nothing obliges the game to send.
            if (with_response)
            {
                std::scoped_lock lock(le->gatt_request_mutex);
                le->pending_gatt_writes[request_id] = PendingGattWrite{request, deferral, with_response};
            }
            else
            {
                deferral.Complete();
            }

            std::string central_address;
            bool central_is_new = false;
            const std::string central = note_server_session(le, args.Session(), central_address, central_is_new);

            BackendEvent event;
            event.type = BackendEventType::LeEvent;
            event.transport = Transport::LowEnergy;
            event.event_type = descriptor_uuid.empty()
                ? "bluetooth_le_server_characteristic_write_request"
                : "bluetooth_le_server_descriptor_write_request";
            event.json = make_server_request_json(
                request_id,
                central,
                central_address,
                service_uuid,
                characteristic_uuid,
                descriptor_uuid,
                request.Offset(),
                &value,
                with_response);
            push_le_event(le, std::move(event));
        }
        catch (...)
        {
            try { deferral.Complete(); } catch (...) {}
        }
    }

    // Stops a service advertising and revokes every handler it registered,
    // so nothing it held fires after it is dropped (R1-105).
    void release_local_service_noexcept(LocalGattServiceState& service) noexcept
    {
        try
        {
            if (service.provider)
                service.provider.StopAdvertising();
        }
        catch (...)
        {
        }

        for (auto& [_, characteristic] : service.characteristics)
        {
            if (!characteristic || !characteristic->characteristic)
                continue;

            try
            {
                if (characteristic->read_token.value != 0)
                    characteristic->characteristic.ReadRequested(characteristic->read_token);
                if (characteristic->write_token.value != 0)
                    characteristic->characteristic.WriteRequested(characteristic->write_token);
                if (characteristic->subscribed_token.value != 0)
                    characteristic->characteristic.SubscribedClientsChanged(characteristic->subscribed_token);
            }
            catch (...)
            {
            }

            for (auto& descriptor : characteristic->descriptors)
            {
                if (!descriptor || !descriptor->descriptor)
                    continue;

                try
                {
                    if (descriptor->read_token.value != 0)
                        descriptor->descriptor.ReadRequested(descriptor->read_token);
                    if (descriptor->write_token.value != 0)
                        descriptor->descriptor.WriteRequested(descriptor->write_token);
                }
                catch (...)
                {
                }
            }
        }

        service.characteristics.clear();
        service.provider = nullptr;
    }

    // Everything le_server_add_service can refuse, checked on the game
    // thread before any WinRT call.
    Error parse_local_service(const std::string& service_json, LocalServiceDefinition& out, std::string& message)
    {
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

        if (!parse_winrt_guid(uuid_field->string_value, out.guid))
        {
            message = "Invalid BLE service UUID";
            return Error::InvalidArgument;
        }
        out.uuid = normalize_uuid(uuid_field->string_value);

        const auto* characteristics = root->find("characteristics");
        if (!characteristics || !characteristics->is_array())
            return Error::Ok;

        for (const auto& characteristic_value : characteristics->array_value)
        {
            if (!characteristic_value.is_object())
                continue;

            const auto* permissions_field = characteristic_value.find("permissions");
            const std::int32_t permissions = permissions_field ? permissions_field->as_int(0) : 0;
            // Windows has no protection level for a signed write.
            if ((permissions & (kPermissionWriteSigned | kPermissionWriteSignedMitm)) != 0)
            {
                message = "Windows has no signed-write permission: "
                    "BluetoothLeAttributePermission.WriteSigned and WriteSignedMitm are not supported";
                return Error::NotSupported;
            }

            const auto* char_uuid_field = characteristic_value.find("uuid");
            if (!char_uuid_field || !char_uuid_field->is_string())
                continue;

            LocalCharacteristicDefinition characteristic;
            if (!parse_winrt_guid(char_uuid_field->string_value, characteristic.guid))
            {
                message = "Invalid BLE characteristic UUID";
                return Error::InvalidArgument;
            }
            characteristic.uuid = normalize_uuid(char_uuid_field->string_value);

            const auto* properties_field = characteristic_value.find("properties");
            characteristic.properties = properties_field ? properties_field->as_int(0) : 0;
            characteristic.permissions = permissions;

            if (const auto* initial_value = characteristic_value.find("value");
                initial_value && initial_value->is_string() && !initial_value->string_value.empty())
            {
                auto decoded = json::base64_decode(initial_value->string_value);
                if (!decoded)
                {
                    message = "Invalid BLE characteristic value: expected standard base64";
                    return Error::InvalidArgument;
                }
                characteristic.value = std::move(*decoded);
            }

            if (const auto* descriptors = characteristic_value.find("descriptors"); descriptors && descriptors->is_array())
            {
                for (const auto& descriptor_value : descriptors->array_value)
                {
                    if (!descriptor_value.is_object())
                        continue;
                    const auto* descriptor_uuid_field = descriptor_value.find("uuid");
                    if (!descriptor_uuid_field || !descriptor_uuid_field->is_string())
                        continue;

                    LocalDescriptorDefinition descriptor;
                    if (!parse_winrt_guid(descriptor_uuid_field->string_value, descriptor.guid))
                    {
                        message = "Invalid BLE descriptor UUID";
                        return Error::InvalidArgument;
                    }
                    descriptor.uuid = normalize_uuid(descriptor_uuid_field->string_value);

                    // Windows creates the Client Characteristic Configuration
                    // descriptor automatically for Notify/Indicate characteristics.
                    if (descriptor.uuid == "00002902-0000-1000-8000-00805f9b34fb")
                        continue;

                    characteristic.descriptors.push_back(std::move(descriptor));
                }
            }

            out.characteristics.push_back(std::move(characteristic));
        }

        return Error::Ok;
    }

    // Creates the service le_server_add_service accepted, on the backend
    // worker: each Create*Async waits on WinRT (R1-103). generation is the
    // server generation the add started under.
    void build_local_service(
        const std::shared_ptr<SharedLeState>& le,
        std::uint64_t op_id,
        const LocalServiceDefinition& definition,
        std::uint64_t generation)
    {
        const auto fail = [&le, op_id, &definition](Error error, std::string why)
        {
            {
                std::scoped_lock lock(le->services_mutex);
                le->adding_services.erase(definition.uuid);
            }
            complete_le_op(le, op_id, error, std::move(why));
        };

        auto service_state = std::make_shared<LocalGattServiceState>();
        service_state->uuid = definition.uuid;

        try
        {
            const auto provider_result = WDBG::GattServiceProvider::CreateAsync(definition.guid).get();
            if (provider_result.Error() != WDB::BluetoothError::Success || !provider_result.ServiceProvider())
            {
                fail(map_bluetooth_error(provider_result.Error()), bluetooth_error_message(provider_result.Error()));
                return;
            }
            service_state->provider = provider_result.ServiceProvider();

            const std::weak_ptr<SharedLeState> weak = le;
            const std::string& service_uuid = definition.uuid;

            for (const auto& characteristic : definition.characteristics)
            {
                const std::string& characteristic_uuid = characteristic.uuid;

                WDBG::GattLocalCharacteristicParameters parameters;
                // Windows local GATT rejects Broadcast. All other raw
                // BluetoothLeCharacteristicProperty values map directly.
                parameters.CharacteristicProperties(
                    static_cast<WDBG::GattCharacteristicProperties>(characteristic.properties & ~1));
                parameters.ReadProtectionLevel(read_protection_from_permissions(characteristic.permissions));
                parameters.WriteProtectionLevel(write_protection_from_permissions(characteristic.permissions));
                if (!characteristic.value.empty())
                    parameters.StaticValue(bytes_to_buffer(characteristic.value));

                const auto characteristic_result = service_state->provider.Service()
                    .CreateCharacteristicAsync(characteristic.guid, parameters).get();
                if (characteristic_result.Error() != WDB::BluetoothError::Success || !characteristic_result.Characteristic())
                {
                    release_local_service_noexcept(*service_state);
                    fail(map_bluetooth_error(characteristic_result.Error()), bluetooth_error_message(characteristic_result.Error()));
                    return;
                }

                auto characteristic_state = std::make_shared<LocalGattCharacteristicState>();
                characteristic_state->service_uuid = service_uuid;
                characteristic_state->characteristic_uuid = characteristic_uuid;
                characteristic_state->characteristic = characteristic_result.Characteristic();
                // In the map before any handler is registered, so a failure
                // below revokes every one already in place.
                service_state->characteristics[characteristic_uuid] = characteristic_state;

                characteristic_state->subscribed_token = characteristic_state->characteristic.SubscribedClientsChanged(
                    [weak](const WDBG::GattLocalCharacteristic& local, const WF::IInspectable&)
                    {
                        const auto current = weak.lock();
                        if (!current || !current->alive.load())
                            return;

                        try
                        {
                            for (const auto& client : local.SubscribedClients())
                            {
                                std::string address;
                                bool is_new = false;
                                const std::string central = note_server_session(current, client.Session(), address, is_new);
                                if (is_new)
                                    push_server_connection_state(current, central, address, true);
                            }
                        }
                        catch (...)
                        {
                        }
                    });

                characteristic_state->read_token = characteristic_state->characteristic.ReadRequested(
                    [weak, service_uuid, characteristic_uuid](
                        const WDBG::GattLocalCharacteristic&,
                        const WDBG::GattReadRequestedEventArgs& args)
                    {
                        on_local_read_request(weak.lock(), service_uuid, characteristic_uuid, std::string(), args);
                    });

                characteristic_state->write_token = characteristic_state->characteristic.WriteRequested(
                    [weak, service_uuid, characteristic_uuid](
                        const WDBG::GattLocalCharacteristic&,
                        const WDBG::GattWriteRequestedEventArgs& args)
                    {
                        on_local_write_request(weak.lock(), service_uuid, characteristic_uuid, std::string(), args);
                    });

                for (const auto& descriptor : characteristic.descriptors)
                {
                    const std::string& descriptor_uuid = descriptor.uuid;

                    WDBG::GattLocalDescriptorParameters descriptor_parameters;
                    descriptor_parameters.ReadProtectionLevel(WDBG::GattProtectionLevel::Plain);
                    descriptor_parameters.WriteProtectionLevel(WDBG::GattProtectionLevel::Plain);

                    const auto descriptor_result = characteristic_state->characteristic
                        .CreateDescriptorAsync(descriptor.guid, descriptor_parameters).get();
                    if (descriptor_result.Error() != WDB::BluetoothError::Success || !descriptor_result.Descriptor())
                    {
                        release_local_service_noexcept(*service_state);
                        fail(map_bluetooth_error(descriptor_result.Error()), bluetooth_error_message(descriptor_result.Error()));
                        return;
                    }

                    auto descriptor_state = std::make_shared<LocalGattDescriptorState>();
                    descriptor_state->descriptor = descriptor_result.Descriptor();
                    characteristic_state->descriptors.push_back(descriptor_state);

                    descriptor_state->read_token = descriptor_state->descriptor.ReadRequested(
                        [weak, service_uuid, characteristic_uuid, descriptor_uuid](
                            const WDBG::GattLocalDescriptor&,
                            const WDBG::GattReadRequestedEventArgs& args)
                        {
                            on_local_read_request(weak.lock(), service_uuid, characteristic_uuid, descriptor_uuid, args);
                        });

                    descriptor_state->write_token = descriptor_state->descriptor.WriteRequested(
                        [weak, service_uuid, characteristic_uuid, descriptor_uuid](
                            const WDBG::GattLocalDescriptor&,
                            const WDBG::GattWriteRequestedEventArgs& args)
                        {
                            on_local_write_request(weak.lock(), service_uuid, characteristic_uuid, descriptor_uuid, args);
                        });
                }
            }
        }
        catch (const winrt::hresult_error& error)
        {
            release_local_service_noexcept(*service_state);
            fail(Error::OperationFailed, winrt::to_string(error.message()));
            return;
        }

        bool dropped = false;
        std::string advertise_failure;
        {
            std::scoped_lock lock(le->services_mutex);
            le->adding_services.erase(definition.uuid);
            dropped = le->server_generation != generation || !le->alive.load();
            if (!dropped)
            {
                le->gatt_services[definition.uuid] = service_state;
                if (le->le_advertising.load())
                {
                    try
                    {
                        start_service_advertising_locked(*le, service_state);
                    }
                    catch (const winrt::hresult_error& error)
                    {
                        advertise_failure = winrt::to_string(error.message());
                    }
                }
            }
        }

        if (dropped)
        {
            release_local_service_noexcept(*service_state);
            complete_le_op(le, op_id, Error::OperationFailed, "The GATT services were cleared before the service was added");
            return;
        }

        if (!advertise_failure.empty())
            GMBT_LOG("The added GATT service could not start advertising: %s", advertise_failure.c_str());
        complete_le_op(le, op_id, Error::Ok, {});
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
            classic_->workers = workers_;
            le_client_->hooks = hooks_;
            le_->hooks = hooks_;
        }

        Error initialize(std::string& message) override
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

        void shutdown() override
        {
            if (!initialized_)
                return;

            std::vector<std::shared_ptr<ClassicConnectionState>> connections;
            {
                std::scoped_lock lock(classic_->connections_mutex);
                classic_->alive->store(false);
                for (auto it = classic_->connections.begin(); it != classic_->connections.end();)
                {
                    // A finished connection only kept its unread bytes; there
                    // is no receive loop left to wait for.
                    bool finished = false;
                    {
                        std::scoped_lock receive_lock(it->second->receive_mutex);
                        finished = it->second->finished;
                    }
                    if (finished)
                    {
                        it = classic_->connections.erase(it);
                        continue;
                    }
                    connections.push_back(it->second);
                    ++it;
                }
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

        bool supports_ble() const override { return le_->ble_supported.load(); }
        bool supports_le_advertise() const override { return le_->ble_supported.load() && le_->le_peripheral_supported.load(); }
        bool supports_le_server() const override { return le_->ble_supported.load() && le_->le_peripheral_supported.load(); }
        bool supports_classic() const override { return le_->classic_supported.load(); }
        bool supports_classic_server() const override { return le_->classic_supported.load(); }

        bool pairing_is_supported(const DiscoveredDevice& device) const override
        {
            return device.transport == Transport::Classic;
        }

        // BluetoothFeature raw values from spec.gmidl (R1-79). The name, service
        // UUIDs and service data go out through a registered GATT service, so
        // they are there whenever advertising is.
        bool feature_supported(std::int32_t feature) const override
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

        std::int32_t current_bluetooth_state() const override
        {
            return le_->bluetooth_state.load();
        }

        PermissionStatus permission_status() const override
        {
            return PermissionStatus::Granted;
        }

        // Windows asks a desktop app for no Bluetooth permission: the answer
        // is known, so it goes out at once.
        Error permission_request(std::string& message) override
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
        Error request_enable(std::string& message) override
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
        Error device_from_id(const std::string& id, DiscoveredDevice& device, std::string& message) override
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

        Error le_connected_devices_query(
            std::uint64_t query_id,
            const std::vector<std::string>& service_uuids,
            std::string& message) override
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
        Error paired_devices_query(std::uint64_t query_id, std::string& message) override
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
        Error post_devices_query(std::uint64_t query_id, std::string& message, Find find)
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

        Error le_scan_start(bool active, const std::vector<LeScanFilter>& filters, std::string& message) override
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
            // it cannot say "any of these" (R1-139).
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
                    active
                        ? WDBA::BluetoothLEScanningMode::Active
                        : WDBA::BluetoothLEScanningMode::Passive);

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
                        device.id = "win:ble:" + device.address;
                        device.address_available = true;
                        device.rssi = static_cast<std::int32_t>(args.RawSignalStrengthInDBm());
                        device.rssi_available = device.rssi != -127;
                        device.connectable = advertisement_is_connectable(args.AdvertisementType());

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

                        // A stop Windows made (radio off, an error); one the game
                        // asked for has already reported.
                        if (!le->le_scanning.exchange(false))
                            return;

                        BackendEvent event;
                        event.type = BackendEventType::ScanStopped;
                        event.transport = Transport::LowEnergy;
                        event.error = map_bluetooth_error(args.Error());
                        event.message = bluetooth_error_message(args.Error());
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

        // Reports scan_stopped itself, as Android does, with the watcher's
        // handlers already gone: each scan reports one stop.
        Error le_scan_stop(std::string& message) override
        {
            if (!initialized_)
                return Error::NotInitialized;

            const bool was_scanning = le_->le_scanning.exchange(false);
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

            if (was_scanning)
            {
                BackendEvent event;
                event.type = BackendEventType::ScanStopped;
                event.transport = Transport::LowEnergy;
                event.error = Error::Ok;
                push_le_event(le_, std::move(event));
            }

            if (!failure.empty())
            {
                message = failure;
                return Error::OperationFailed;
            }
            message.clear();
            return Error::Ok;
        }

        bool le_scan_is_running() const override
        {
            return le_->le_scanning.load();
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

        // Windows negotiates the MTU when the link opens; the session the
        // connection holds says what it came to (R1-140).
        std::int32_t le_connection_mtu(
            std::uint64_t connection) const override
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
        Error le_request_connection_priority(
            std::uint64_t connection,
            std::int32_t priority,
            std::string& message) override
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
        Error post_le_op(
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

        Error le_services_discover(
            std::uint64_t op_id,
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

        Error le_characteristics_discover(
            std::uint64_t op_id,
            std::uint64_t connection,
            const LeAttributeRef& attribute,
            std::string& message) override
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

        Error le_descriptors_discover(
            std::uint64_t op_id,
            std::uint64_t connection,
            const LeAttributeRef& attribute,
            std::string& message) override
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

        Error le_characteristic_read(
            std::uint64_t op_id,
            std::uint64_t connection,
            const LeAttributeRef& attribute,
            std::string& message) override
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

        Error le_characteristic_write(
            std::uint64_t op_id,
            std::uint64_t connection,
            const LeAttributeRef& attribute,
            const std::string& value_base64,
            bool with_response,
            std::string& message) override
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

            auto decoded = json::base64_decode(value_base64);
            if (!decoded)
            {
                message = "The value to write is not valid base64";
                return Error::OperationFailed;
            }

            const auto payload = std::move(*decoded);
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

        Error le_characteristic_subscribe(
            std::uint64_t op_id,
            std::uint64_t connection,
            const LeAttributeRef& attribute,
            std::int32_t mode,
            std::string& message) override
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

                if (mode != 0 && !registered)
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

                            const auto bytes =
                                buffer_to_bytes(
                                    args.CharacteristicValue());

                            push_le_client_event(
                                shared,
                                "bluetooth_le_characteristic_value_changed",
                                "{\"connection\":" +
                                    std::to_string(
                                        current->handle) +
                                    ",\"service_instance\":" +
                                    std::to_string(
                                        service_instance) +
                                    ",\"characteristic_instance\":" +
                                    std::to_string(
                                        characteristic_instance) +
                                    ",\"value\":\"" +
                                    json::base64_encode(
                                        bytes.data(),
                                        bytes.size()) +
                                    "\"}");
                        });

                    // Stored under the mutex close takes; a close that came
                    // first leaves this handler to revoke now (R1-113).
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
                        push_le_op_completion(shared, op_id, Error::Disconnected, k_le_attribute_closed);
                        return;
                    }
                }

                const auto cccd_value =
                    mode == 1
                        ? WDBG::GattClientCharacteristicConfigurationDescriptorValue::Notify
                    : mode == 2
                        ? WDBG::GattClientCharacteristicConfigurationDescriptorValue::Indicate
                        : WDBG::GattClientCharacteristicConfigurationDescriptorValue::None;

                // The WithResult form carries the ATT code of a refusal.
                const auto result =
                    object.WriteClientCharacteristicConfigurationDescriptorWithResultAsync(
                        cccd_value).get();

                if (result.Status() ==
                        WDBG::GattCommunicationStatus::Success &&
                    mode == 0)
                {
                    winrt::event_token token{};
                    {
                        std::scoped_lock lock(state->mutex);
                        if (characteristic->value_changed_registered)
                            token = characteristic->value_changed_token;
                        characteristic->value_changed_registered = false;
                        characteristic->value_changed_token = {};
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

                push_le_op_completion(
                    shared,
                    op_id,
                    gatt_result_outcome(result));
            });
        }

        Error le_descriptor_read(
            std::uint64_t op_id,
            std::uint64_t connection,
            const LeAttributeRef& attribute,
            std::string& message) override
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

        Error le_descriptor_write(
            std::uint64_t op_id,
            std::uint64_t connection,
            const LeAttributeRef& attribute,
            const std::string& value_base64,
            std::string& message) override
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

            auto decoded = json::base64_decode(value_base64);
            if (!decoded)
            {
                message = "The value to write is not valid base64";
                return Error::OperationFailed;
            }

            const auto payload = std::move(*decoded);
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

        // ===== BLE Advertiser =====

        // BluetoothLeAdvertiseTxPower in dBm: the values Android documents for
        // its four AdvertiseSettings levels.
        static std::int16_t advertise_tx_power_dbm(LeAdvertiseTxPower level)
        {
            switch (level)
            {
                case LeAdvertiseTxPower::UltraLow: return -21;
                case LeAdvertiseTxPower::Low:      return -15;
                case LeAdvertiseTxPower::Medium:   return -7;
                case LeAdvertiseTxPower::High:     return 1;
            }
            return -7;
        }

        Error le_advertise_start(std::uint64_t op_id, const LeAdvertiseSettings& settings, const LeAdvertiseData& data, std::string& message) override
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
            // A second start would ignore its settings and data (R1-188).
            std::uint64_t generation = 0;
            {
                std::scoped_lock flag_lock(le_->advertise_flag_mutex);
                if (le_->le_advertising.exchange(true))
                {
                    message = "Bluetooth LE advertising is already running; stop it first";
                    return Error::Busy;
                }
                generation = ++le_->advertise_generation;
            }

            std::unique_lock services_lock(le_->services_mutex);

            // The generic Windows advertisement publisher cannot write
            // system-reserved sections such as LocalName or Service UUIDs.
            // Those are published by a registered service's GattServiceProvider,
            // so a service UUID, its service data and the name go out only
            // through one; the publisher carries manufacturer data and the
            // TX power. Each refusal names the field, before anything starts.
            const auto refuse = [this, &services_lock, &message](std::string why)
            {
                services_lock.unlock();
                le_->le_advertising.store(false);
                message = std::move(why);
                return Error::NotSupported;
            };

            std::unordered_set<std::string> service_uuids;
            std::vector<WDBG::GattServiceProvider> providers;
            for (const auto& raw_uuid : data.service_uuids)
            {
                const std::string uuid = normalize_uuid(raw_uuid);
                const auto service_it = le_->gatt_services.find(uuid);
                if (service_it == le_->gatt_services.end() || !service_it->second || !service_it->second->provider)
                {
                    return refuse("service_uuids: Windows advertises a service UUID only through a registered GATT service, and " +
                        uuid + " is not one");
                }
                if (service_uuids.insert(uuid).second)
                    providers.push_back(service_it->second->provider);
            }

            if (data.include_name && providers.empty())
                return refuse("include_name: Windows advertises the name only through a registered GATT service in service_uuids");

            const bool start_publisher = !data.manufacturer_data.empty() || data.include_tx_power || settings.tx_power.has_value();
            if (!start_publisher && providers.empty())
                return refuse("Windows cannot advertise without manufacturer data, a power level or a registered GATT service");

            try
            {
                le_->advertise_connectable = settings.connectable;
                le_->advertise_discoverable = data.include_name;
                advertise_include_power_ = data.include_tx_power;
                advertise_tx_power_.reset();
                if (settings.tx_power)
                    advertise_tx_power_ = advertise_tx_power_dbm(*settings.tx_power);
                le_->advertise_service_uuids = std::move(service_uuids);
                le_->advertise_service_data.clear();
                for (const auto& entry : data.service_data)
                    le_->advertise_service_data[normalize_uuid(entry.uuid)] = entry.data;

                auto tracker = std::make_shared<AdvertiseStartTracker>();
                tracker->op_id = op_id;
                tracker->generation = generation;
                advertise_tracker_ = tracker;

                // Counted before anything starts, so no status event can
                // complete the call early. A provider already advertising has
                // nothing left to report.
                std::size_t to_start = start_publisher ? 1 : 0;
                for (const auto& provider : providers)
                {
                    if (provider.AdvertisementStatus() != WDBG::GattServiceProviderAdvertisementStatus::Started)
                        ++to_start;
                }
                tracker->remaining.store(to_start);

                // The publisher a failed start left behind goes first, with
                // its StatusChanged handler (R1-242).
                release_advertiser_noexcept();
                advertiser_ = WDBA::BluetoothLEAdvertisementPublisher{};
                for (const auto& manufacturer : data.manufacturer_data)
                {
                    advertiser_.Advertisement().ManufacturerData().Append(
                        WDBA::BluetoothLEManufacturerData(
                            manufacturer.company_id,
                            bytes_to_buffer(manufacturer.data)));
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

                const std::weak_ptr<SharedLeState> weak = le_;
                advertiser_status_token_ = advertiser_.StatusChanged(
                    [weak, tracker](
                        const WDBA::BluetoothLEAdvertisementPublisher&,
                        const WDBA::BluetoothLEAdvertisementPublisherStatusChangedEventArgs& args)
                    {
                        const auto le = weak.lock();
                        if (!le)
                            return;

                        // Only the newest start's publisher may say advertising
                        // ended; an older one's late event leaves it (R1-242).
                        const auto advertising_ended = [&le, &tracker]()
                        {
                            std::scoped_lock flag_lock(le->advertise_flag_mutex);
                            if (tracker->generation == le->advertise_generation.load())
                                le->le_advertising.store(false);
                        };
                        const auto status = args.Status();
                        if (status == WDBA::BluetoothLEAdvertisementPublisherStatus::Started)
                        {
                            advertise_start_progress(le, tracker);
                        }
                        else if (status == WDBA::BluetoothLEAdvertisementPublisherStatus::Aborted)
                        {
                            advertising_ended();
                            advertise_start_failed(
                                le,
                                tracker,
                                map_bluetooth_error(args.Error()),
                                "Windows aborted the BLE advertisement: " + bluetooth_error_message(args.Error()));
                        }
                        else if (status == WDBA::BluetoothLEAdvertisementPublisherStatus::Stopped)
                        {
                            advertising_ended();
                            advertise_start_failed(
                                le,
                                tracker,
                                Error::OperationFailed,
                                "BLE advertisement stopped before it started");
                        }
                    });

                if (start_publisher)
                {
                    std::scoped_lock lock(tracker->mutex);
                    tracker->publisher = advertiser_;
                }

                for (const auto& provider : providers)
                {
                    if (provider.AdvertisementStatus() == WDBG::GattServiceProviderAdvertisementStatus::Started)
                        continue;

                    const auto token = provider.AdvertisementStatusChanged(
                        [weak, tracker](
                            const WDBG::GattServiceProvider&,
                            const WDBG::GattServiceProviderAdvertisementStatusChangedEventArgs& args)
                        {
                            const auto le = weak.lock();
                            if (!le)
                                return;

                            switch (args.Status())
                            {
                                case WDBG::GattServiceProviderAdvertisementStatus::Started:
                                    advertise_start_progress(le, tracker);
                                    break;
                                case WDBG::GattServiceProviderAdvertisementStatus::StartedWithoutAllAdvertisementData:
                                    GMBT_LOG("GATT service advertising started without all of its advertisement data");
                                    advertise_start_progress(le, tracker);
                                    break;
                                case WDBG::GattServiceProviderAdvertisementStatus::Aborted:
                                    advertise_start_failed(
                                        le,
                                        tracker,
                                        map_bluetooth_error(args.Error()),
                                        "Windows aborted GATT service advertising: " + bluetooth_error_message(args.Error()));
                                    break;
                                case WDBG::GattServiceProviderAdvertisementStatus::Stopped:
                                    advertise_start_failed(
                                        le,
                                        tracker,
                                        Error::OperationFailed,
                                        "GATT service advertising stopped before it started");
                                    break;
                                default:
                                    break;
                            }
                        });

                    std::scoped_lock lock(tracker->mutex);
                    tracker->providers.emplace_back(provider, token);
                }

                if (start_publisher)
                    advertiser_.Start();

                for (const auto& uuid : le_->advertise_service_uuids)
                    start_service_advertising_locked(*le_, le_->gatt_services[uuid]);
                services_lock.unlock();

                // Every provider was already advertising and there is no
                // publisher: nothing will report, so the start is done now.
                if (to_start == 0)
                    finish_advertise_start(le_, tracker, Error::Ok, {});

                message.clear();
                return Error::Ok;
            }
            catch (const winrt::hresult_error& error)
            {
                if (services_lock.owns_lock())
                    services_lock.unlock();

                // The core drops the op when this call fails; nothing may
                // complete it later.
                if (advertise_tracker_)
                {
                    advertise_tracker_->done.store(true);
                    revoke_advertise_handlers(advertise_tracker_);
                    advertise_tracker_.reset();
                }
                le_->le_advertising.store(false);
                release_advertiser_noexcept();
                message = winrt::to_string(error.message());
                return Error::OperationFailed;
            }
        }

        Error le_advertise_stop(std::string& message) override
        {
            if (advertise_tracker_)
            {
                finish_advertise_start(
                    le_,
                    advertise_tracker_,
                    Error::OperationFailed,
                    "Advertising stopped before it started");
                advertise_tracker_.reset();
            }

            try
            {
                {
                    std::scoped_lock lock(le_->services_mutex);
                    for (auto& [_, service] : le_->gatt_services)
                    {
                        if (service && service->provider)
                            service->provider.StopAdvertising();
                    }
                }
                release_advertiser_noexcept();
                le_->le_advertising.store(false);
                message.clear();
                return Error::Ok;
            }
            catch (const winrt::hresult_error& error)
            {
                le_->le_advertising.store(false);
                release_advertiser_noexcept();
                message = winrt::to_string(error.message());
                return Error::OperationFailed;
            }
        }

        bool le_advertise_is_running() const override
        {
            return le_->le_advertising.load();
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
            clear_queued_notifies(notify_queue_);
            complete_parked_gatt_requests_noexcept();
            clear_gatt_services_noexcept();
            release_server_sessions_noexcept(le_);
            message.clear();
            return Error::Ok;
        }

        bool le_server_is_running() const override
        {
            return le_server_open_.load();
        }

        Error le_server_add_service(std::uint64_t op_id, const std::string& service_json, std::string& message) override
        {
            if (!le_server_open_.load())
            {
                message = "The LE server is not running; call bluetooth_le_server_start first";
                return Error::OperationFailed;
            }

            LocalServiceDefinition definition;
            const Error parsed = parse_local_service(service_json, definition, message);
            if (parsed != Error::Ok)
                return parsed;

            std::uint64_t generation = 0;
            {
                std::scoped_lock lock(le_->services_mutex);
                if (le_->gatt_services.find(definition.uuid) != le_->gatt_services.end() ||
                    le_->adding_services.find(definition.uuid) != le_->adding_services.end())
                {
                    message = "BLE service already exists";
                    return Error::Busy;
                }
                le_->adding_services.insert(definition.uuid);
                generation = le_->server_generation;
            }

            // Creating the provider and each attribute waits on WinRT, so it
            // runs on the worker, which completes op_id (R1-103).
            const auto le = le_;
            const bool posted = le_worker_ && le_worker_->post(
                [le, op_id, definition, generation]()
                {
                    build_local_service(le, op_id, definition, generation);
                });

            if (!posted)
            {
                std::scoped_lock lock(le_->services_mutex);
                le_->adding_services.erase(definition.uuid);
                message = "The Bluetooth worker thread is not running";
                return Error::OperationFailed;
            }

            message.clear();
            return Error::Ok;
        }

        Error le_server_clear_services(std::string& message) override
        {
            clear_queued_notifies(notify_queue_);
            complete_parked_gatt_requests_noexcept();
            clear_gatt_services_noexcept();
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
                std::scoped_lock lock(le_->gatt_request_mutex);
                const auto it = le_->pending_gatt_reads.find(request_id);
                if (it == le_->pending_gatt_reads.end())
                {
                    message = "BLE read request not found";
                    return Error::NotFound;
                }
                pending = it->second;
                le_->pending_gatt_reads.erase(it);
            }

            try
            {
                // status is a BluetoothAttError the core validated: the ATT
                // code itself. The value is read only for a success.
                std::optional<std::vector<std::uint8_t>> value;
                if (status == 0)
                    value = json::base64_decode(value_base64);

                GMBT_TRACE(
                    "Responding to GATT read: request_id=%d status=%d request_state=%d request_offset=%u bytes=%zu",
                    request_id,
                    status,
                    static_cast<int>(pending.request.State()),
                    pending.request.Offset(),
                    value ? value->size() : std::size_t{0});

                // A value that will not decode still answers the central,
                // so it never waits out the ATT timeout.
                if (status == 0 && !value)
                {
                    constexpr std::uint8_t unlikely_error = 0x0E;
                    pending.request.RespondWithProtocolError(unlikely_error);
                    if (pending.deferral)
                        pending.deferral.Complete();
                    message = "The read response value is not valid base64; the central was answered with ATT error 0x0E";
                    return Error::OperationFailed;
                }

                if (status == 0)
                    pending.request.RespondWithValue(bytes_to_buffer(*value));
                else
                    pending.request.RespondWithProtocolError(static_cast<std::uint8_t>(status));

                if (pending.deferral)
                    pending.deferral.Complete();

                GMBT_TRACE(
                    "GATT read response completed: request_id=%d final_state=%d",
                    request_id,
                    static_cast<int>(pending.request.State()));

                message.clear();
                return Error::Ok;
            }
            catch (const winrt::hresult_error& error)
            {
                GMBT_LOG(
                    "GATT read response FAILED: request_id=%d hresult=0x%08X message='%s'",
                    request_id,
                    static_cast<unsigned>(error.code().value),
                    winrt::to_string(error.message()).c_str());
                if (pending.deferral)
                {
                    try { pending.deferral.Complete(); } catch (...) {}
                }
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
                std::scoped_lock lock(le_->gatt_request_mutex);
                const auto it = le_->pending_gatt_writes.find(request_id);
                if (it == le_->pending_gatt_writes.end())
                {
                    message = "BLE write request not found";
                    return Error::NotFound;
                }
                pending = it->second;
                le_->pending_gatt_writes.erase(it);
            }

            try
            {
                GMBT_TRACE(
                    "Responding to GATT write: request_id=%d status=%d with_response=%d request_state=%d bytes=%u",
                    request_id,
                    status,
                    pending.with_response ? 1 : 0,
                    static_cast<int>(pending.request.State()),
                    pending.request.Value() ? pending.request.Value().Length() : 0);

                if (pending.with_response)
                {
                    if (status == 0)
                        pending.request.Respond();
                    else
                        pending.request.RespondWithProtocolError(static_cast<std::uint8_t>(status));
                }

                if (pending.deferral)
                    pending.deferral.Complete();

                GMBT_TRACE(
                    "GATT write response completed: request_id=%d final_state=%d",
                    request_id,
                    static_cast<int>(pending.request.State()));

                message.clear();
                return Error::Ok;
            }
            catch (const winrt::hresult_error& error)
            {
                GMBT_LOG(
                    "GATT write response FAILED: request_id=%d hresult=0x%08X message='%s'",
                    request_id,
                    static_cast<unsigned>(error.code().value),
                    winrt::to_string(error.message()).c_str());
                if (pending.deferral)
                {
                    try { pending.deferral.Complete(); } catch (...) {}
                }
                message = winrt::to_string(error.message());
                return Error::OperationFailed;
            }
        }

        Error le_server_notify_value(
            const std::string& service_uuid,
            const std::string& characteristic_uuid,
            const std::string& central,
            const std::string& value_base64,
            std::string& message) override
        {
            WDBG::GattLocalCharacteristic characteristic{nullptr};
            {
                std::scoped_lock lock(le_->services_mutex);
                const auto service_it = le_->gatt_services.find(normalize_uuid(service_uuid));
                if (service_it == le_->gatt_services.end() || !service_it->second)
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
                characteristic = characteristic_it->second->characteristic;
            }

            // A notify to one central goes to its subscription only (R1-48).
            WDBG::GattSubscribedClient client{nullptr};
            if (!central.empty())
            {
                try
                {
                    for (const auto& subscribed : characteristic.SubscribedClients())
                    {
                        if (winrt::to_string(subscribed.Session().DeviceId().Id()) == central)
                        {
                            client = subscribed;
                            break;
                        }
                    }
                }
                catch (const winrt::hresult_error& error)
                {
                    message = winrt::to_string(error.message());
                    return Error::OperationFailed;
                }

                if (!client)
                {
                    message = "The central is not subscribed to this characteristic";
                    return Error::NotFound;
                }
            }

            auto decoded = json::base64_decode(value_base64);
            if (!decoded)
            {
                message = "The notification value is not valid base64";
                return Error::OperationFailed;
            }

            const auto notifies = notify_queue_;
            bool start = false;
            {
                std::scoped_lock lock(notifies->mutex);
                if (notifies->queue.size() >= k_max_queued_notifies)
                {
                    message = "Too many GATT notifications are waiting to be sent";
                    return Error::Busy;
                }

                PendingNotify pending;
                pending.characteristic = characteristic;
                pending.client = client;
                pending.bytes = std::move(*decoded);
                notifies->queue.push_back(std::move(pending));

                start = !notifies->in_flight;
                if (start)
                    notifies->in_flight = true;
            }

            if (start)
                start_next_notify(notifies);

            message.clear();
            return Error::Ok;
        }

        Error classic_scan_start(std::string& message) override
        {
            if (!initialized_)
            {
                message = "Bluetooth backend is not initialized";
                return Error::NotInitialized;
            }

            std::uint64_t generation = 0;
            {
                std::scoped_lock lock(classic_scan_->mutex);
                if (classic_scan_->running.load())
                {
                    message.clear();
                    return Error::Ok;
                }
                classic_scan_->running.store(true);
                generation = ++classic_scan_->generation;
            }

            // An inquiry a stop left running is not joined here: the new
            // scan's thread waits for it, so two inquiries never overlap and
            // the game thread never blocks on one.
            auto previous = std::make_shared<PreviousWorker>();
            previous->thread = std::move(classic_scan_thread_);

            const auto shared = classic_;
            const auto scan = classic_scan_;
            classic_scan_thread_ = spawn_worker(
                workers_,
                [shared, scan, generation, previous]()
                {
                    if (previous->thread.joinable())
                        previous->thread.join();
                    run_classic_inquiry(shared, scan, generation);
                });

            if (!classic_scan_thread_.joinable())
            {
                classic_scan_thread_ = std::move(previous->thread);
                {
                    std::scoped_lock lock(classic_scan_->mutex);
                    if (classic_scan_->generation.load() == generation)
                        classic_scan_->running.store(false);
                }
                message = "Could not start the Classic discovery thread";
                return Error::OperationFailed;
            }

            message.clear();
            return Error::Ok;
        }

        // Never waits for the inquiry, which Windows cannot cut short: the
        // scan is marked stopped and reported here, and the inquiry thread,
        // its generation now stale, ends unobserved.
        Error classic_scan_stop(std::string& message) override
        {
            bool was_running = false;
            {
                std::scoped_lock lock(classic_scan_->mutex);
                was_running = classic_scan_->running.load();
                classic_scan_->running.store(false);
                ++classic_scan_->generation;
            }

            if (was_running && classic_->alive->load() && hooks_.push_event)
            {
                BackendEvent event;
                event.type = BackendEventType::ScanStopped;
                event.transport = Transport::Classic;
                event.error = Error::Ok;
                hooks_.push_event(std::move(event));
            }

            message.clear();
            return Error::Ok;
        }

        bool classic_scan_is_running() const override
        {
            return classic_scan_->running.load();
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
            const std::uint64_t device_handle = hooks_.upsert_device
                ? hooks_.upsert_device(device)
                : 0;

            // Registered while still connecting, before the worker starts, so
            // a classic_disconnect that comes before the connect completes
            // finds it and cancels it.
            auto state = std::make_shared<ClassicConnectionState>(
                connection, device_handle);
            if (!register_connection_if_alive(shared, state))
            {
                message = "Bluetooth backend is shutting down";
                return Error::NotInitialized;
            }

            const bool started = spawn_detached_worker(
                workers_,
                [shared, state, address, service_guid]()
                {
                    run_classic_connect(shared, state, address, service_guid);
                });

            if (!started)
            {
                unregister_connection(shared, connection);
                message = "Could not start a thread for the Classic connection";
                return Error::OperationFailed;
            }

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

            // An already-paired device completes at once, as Android does
            // (R1-111); the core registered the callback before this call.
            if (is_paired(device))
            {
                if (hooks_.push_event)
                {
                    BackendEvent event;
                    event.type = BackendEventType::DevicePaired;
                    event.transport = Transport::Classic;
                    event.device = device_handle;
                    event.error = Error::Ok;
                    hooks_.push_event(std::move(event));
                }
                message.clear();
                return Error::Ok;
            }

            const auto shared = classic_;
            const auto alive = classic_->alive;

            const bool started = spawn_detached_worker(workers_, [shared, alive, device_handle, address]()
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

                // ERROR_NO_MORE_ITEMS: the device was already authenticated.
                // A cancel keeps OperationFailed - no other platform can tell
                // it apart - but says so.
                if (result == ERROR_SUCCESS || result == ERROR_NO_MORE_ITEMS)
                {
                    event.error = Error::Ok;
                }
                else
                {
                    event.error = Error::OperationFailed;
                    event.message = system_error_message(
                        result == ERROR_CANCELLED
                            ? "Pairing was cancelled by the user: BluetoothAuthenticateDeviceEx"
                            : "BluetoothAuthenticateDeviceEx",
                        result);
                }

                shared->hooks.push_event(std::move(event));
            });

            if (!started)
            {
                message = "Could not start a thread for the pairing";
                return Error::OperationFailed;
            }

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

            bool finished = false;
            {
                std::scoped_lock lock(state->receive_mutex);
                finished = state->finished;
            }
            if (finished)
            {
                // The peer already hung up; the game is done with its unread bytes.
                unregister_connection(classic_, connection);
                message.clear();
                return Error::Ok;
            }

            // Open or still connecting. A connect cancelled here reports
            // nothing (the core answers its callback); an open connection's
            // receive loop reports the disconnect once it has closed.
            connection_request_close(state);

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
            {
                message = "Classic send data is missing";
                return Error::InvalidArgument;
            }

            // Queued for the connection's writer; nothing here waits on the
            // socket.
            if (size != 0)
            {
                std::scoped_lock lock(state->send_mutex);
                if (state->writer_stop || state->send_failed)
                {
                    message = state->send_error_message.empty()
                        ? std::string("Classic connection is not connected")
                        : state->send_error_message;
                    return Error::Disconnected;
                }

                if (size > k_classic_send_cap - state->send_pending)
                {
                    message = "The Classic send queue is full: at most 1 MiB may wait to be sent, "
                        "try again once it drains";
                    return Error::Busy;
                }

                state->outbound.insert(state->outbound.end(), data, data + size);
                state->send_pending += size;
            }
            state->send_cv.notify_all();

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

            std::size_t count = 0;
            bool drained = false;
            {
                std::scoped_lock lock(state->receive_mutex);
                count = std::min(max_size, state->received.size());
                for (std::size_t i = 0; i < count; ++i)
                {
                    out[i] = state->received.front();
                    state->received.pop_front();
                }
                drained = state->finished && state->received.empty();
                // The next data to arrive is announced again.
                state->data_pending = false;
            }

            // Room again for a receive loop held back by the cap.
            state->receive_cv.notify_all();

            // The last bytes of a connection the peer closed: nothing is left
            // to keep it registered for.
            if (drained)
                unregister_connection(classic_, connection);
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
                closesocket(listener);
                message = wsa_message("WSASetService", error);
                return map_wsa_error(error);
            }

            auto server = std::make_shared<ClassicServerState>();
            server->listener = listener;

            const auto shared = classic_;
            const bool started = spawn_detached_worker(
                workers_,
                [shared, server, listener]()
                {
                    run_classic_accept_loop(shared, server, listener);
                });

            if (!started)
            {
                WSASetServiceW(&server_query_, RNRSERVICE_DELETE, 0);
                closesocket(listener);
                message = "Could not start the Classic server thread";
                return Error::OperationFailed;
            }

            server_registered_ = true;
            classic_server_ = std::move(server);
            classic_server_running_.store(true);

            message.clear();
            return Error::Ok;
        }

        // Closes the listening socket, which ends a blocked accept(); the
        // accept loop then finds its server stopped and returns on its own.
        Error classic_server_stop(std::string& message) override
        {
            classic_server_running_.store(false);

            if (server_registered_)
            {
                WSASetServiceW(&server_query_, RNRSERVICE_DELETE, 0);
                server_registered_ = false;
            }

            if (classic_server_)
            {
                {
                    std::scoped_lock lock(classic_server_->mutex);
                    classic_server_->running = false;
                    if (classic_server_->listener != INVALID_SOCKET)
                    {
                        closesocket(classic_server_->listener);
                        classic_server_->listener = INVALID_SOCKET;
                    }
                }
                classic_server_->wake.notify_all();
                classic_server_.reset();
            }

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

            // Incoming connections are a system setting; whatever this turns
            // on is turned back off by the stop or the expiry (R1-184).
            const bool restore_connectable = BluetoothIsConnectable(radio_handle) == FALSE;

            if (!BluetoothEnableIncomingConnections(radio_handle, TRUE))
            {
                message = system_error_message("BluetoothEnableIncomingConnections", GetLastError());
                CloseHandle(radio_handle);
                return Error::OperationFailed;
            }

            if (!BluetoothEnableDiscovery(radio_handle, TRUE))
            {
                message = system_error_message("BluetoothEnableDiscovery", GetLastError());
                if (restore_connectable)
                    BluetoothEnableIncomingConnections(radio_handle, FALSE);
                CloseHandle(radio_handle);
                return Error::OperationFailed;
            }

            discoverable_restore_connectable_ = restore_connectable;
            discoverable_radio_ = radio_handle;
            classic_discoverable_active_.store(true);
            {
                std::scoped_lock lock(discoverable_mutex_);
                classic_discoverable_stop_requested_ = false;
            }

            if (duration_seconds > 0)
            {
                // Joined by classic_discoverable_stop (shutdown calls it), so
                // it never outlives the backend; a stop wakes it at once.
                discoverable_timer_thread_ = spawn_worker(workers_, [this, duration_seconds]()
                {
                    bool stopped = false;
                    {
                        std::unique_lock lock(discoverable_mutex_);
                        stopped = discoverable_wake_.wait_for(
                            lock,
                            std::chrono::seconds(duration_seconds),
                            [this]() { return classic_discoverable_stop_requested_; });
                    }

                    // Only auto-disable on natural expiry - if a stop was requested,
                    // classic_discoverable_stop() owns disabling/closing the radio.
                    if (!stopped)
                    {
                        end_discoverable(discoverable_radio_);
                        classic_discoverable_active_.store(false);
                    }
                });
            }

            message.clear();
            return Error::Ok;
        }

        // Discovery off, and incoming connections back off when start turned
        // them on; the restore happens once, on expiry or on stop.
        void end_discoverable(HANDLE radio)
        {
            if (!radio)
                return;
            BluetoothEnableDiscovery(radio, FALSE);
            if (discoverable_restore_connectable_.exchange(false))
                BluetoothEnableIncomingConnections(radio, FALSE);
        }

        Error classic_discoverable_stop(std::string& message) override
        {
            {
                std::scoped_lock lock(discoverable_mutex_);
                classic_discoverable_stop_requested_ = true;
            }
            discoverable_wake_.notify_all();

            if (discoverable_timer_thread_.joinable())
                discoverable_timer_thread_.join();

            if (discoverable_radio_)
            {
                end_discoverable(discoverable_radio_);
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

    private:
        // Completes every request still parked, so no central waits out the ATT
        // timeout on a server that stopped or dropped its services. The core
        // answers the ones it holds first; this catches anything left.
        void complete_parked_gatt_requests_noexcept() noexcept
        {
            std::unordered_map<std::int32_t, PendingGattRead> reads;
            std::unordered_map<std::int32_t, PendingGattWrite> writes;
            {
                std::scoped_lock lock(le_->gatt_request_mutex);
                reads.swap(le_->pending_gatt_reads);
                writes.swap(le_->pending_gatt_writes);
            }

            constexpr std::uint8_t unlikely_error = 0x0E;
            for (auto& [request_id, pending] : reads)
            {
                (void)request_id;
                try { pending.request.RespondWithProtocolError(unlikely_error); } catch (...) {}
                try { if (pending.deferral) pending.deferral.Complete(); } catch (...) {}
            }
            for (auto& [request_id, pending] : writes)
            {
                (void)request_id;
                if (pending.with_response)
                {
                    try { pending.request.RespondWithProtocolError(unlikely_error); } catch (...) {}
                }
                try { if (pending.deferral) pending.deferral.Complete(); } catch (...) {}
            }
        }

        // Drops every service with its handlers revoked; an add still in
        // flight sees the new generation and is dropped too.
        void clear_gatt_services_noexcept() noexcept
        {
            std::unordered_map<std::string, std::shared_ptr<LocalGattServiceState>> services;
            {
                std::scoped_lock lock(le_->services_mutex);
                services.swap(le_->gatt_services);
                ++le_->server_generation;
            }

            for (auto& [_, service] : services)
            {
                if (service)
                    release_local_service_noexcept(*service);
            }
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
        // Counts every thread this backend starts; see spawn_worker.
        std::shared_ptr<WorkerTracker> workers_ = std::make_shared<WorkerTracker>();
        std::shared_ptr<SharedClassicState> classic_;
        std::shared_ptr<SharedLeClientState> le_client_;
        std::shared_ptr<SharedLeState> le_ = std::make_shared<SharedLeState>();
        // Runs the adapter query and its re-queries, service creation and the
        // close of a link the peer dropped; started by initialize, stopped
        // last by shutdown.
        std::shared_ptr<SerialWorker> le_worker_;
        std::shared_ptr<NotifyQueueState> notify_queue_ = std::make_shared<NotifyQueueState>();
        std::shared_ptr<AdvertiseStartTracker> advertise_tracker_;

        bool initialized_ = false;
        bool winsock_initialized_ = false;

        WDBA::BluetoothLEAdvertisementWatcher watcher_{nullptr};
        winrt::event_token received_token_{};
        winrt::event_token stopped_token_{};

        WDBA::BluetoothLEAdvertisementPublisher advertiser_{nullptr};
        winrt::event_token advertiser_status_token_{};
        bool advertise_include_power_ = false;
        std::optional<std::int16_t> advertise_tx_power_;

        std::atomic_bool le_server_open_{false};

        std::shared_ptr<ClassicScanState> classic_scan_ = std::make_shared<ClassicScanState>();
        // The newest inquiry thread. Joined by the next scan's thread, or by
        // shutdown once every worker has finished; never by a stop.
        std::thread classic_scan_thread_;

        std::atomic_bool classic_server_running_{false};
        std::shared_ptr<ClassicServerState> classic_server_;
        bool server_registered_ = false;
        GUID server_guid_{};
        SOCKADDR_BTH server_addr_{};
        CSADDR_INFO server_csaddr_{};
        WSAQUERYSETW server_query_{};
        std::wstring server_name_;
        std::wstring server_comment_;

        std::atomic_bool classic_discoverable_active_{false};
        std::mutex discoverable_mutex_;
        std::condition_variable discoverable_wake_;
        bool classic_discoverable_stop_requested_ = false; // under discoverable_mutex_
        std::thread discoverable_timer_thread_;
        HANDLE discoverable_radio_ = nullptr;
        // Start turned incoming connections on, so the end turns them off.
        std::atomic_bool discoverable_restore_connectable_{false};
    };

    std::unique_ptr<Backend> create_platform_backend(CoreHooks hooks)
    {
        return std::make_unique<WindowsBackend>(std::move(hooks));
    }
}

#endif
