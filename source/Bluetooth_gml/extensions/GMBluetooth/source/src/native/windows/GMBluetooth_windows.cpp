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
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Devices.Radios.h>
#include <winrt/Windows.Storage.Streams.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <deque>
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
        std::string wsa_message(const char* operation, int error)
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

            std::string out(operation ? operation : "Bluetooth socket operation");
            out += " failed (" + std::to_string(error) + ")";
            if (!text.empty())
            {
                out += ": ";
                out += text;
            }
            return out;
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

        // A remote device id ends in its address ("BluetoothLE#BluetoothLE
        // <local>-<remote>"); empty when it does not.
        std::string address_from_device_id(const std::string& device_id)
        {
            const auto dash = device_id.rfind('-');
            if (dash == std::string::npos)
                return std::string();

            std::string address = device_id.substr(dash + 1);
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

        // The central fields every GATT server event carries: the key the core
        // maps to a server connection, and the device address when known.
        std::string server_central_json(const std::string& central, const std::string& address)
        {
            std::string out = "\"central\":\"" + json_escape(central) + "\"";
            if (!address.empty())
                out += ",\"address\":\"" + json_escape(address) + "\"";
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

        void run_classic_inquiry(
            const std::shared_ptr<SharedClassicState>& shared,
            const std::shared_ptr<ClassicScanState>& scan,
            std::uint64_t generation)
        {
            const auto current = [&shared, &scan, generation]()
            {
                return scan->generation.load() == generation && shared->alive->load();
            };

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

                HBLUETOOTH_DEVICE_FIND finder =
                    BluetoothFindFirstDevice(&search, &info);

                if (finder)
                {
                    do
                    {
                        if (!current())
                            break;

                        DiscoveredDevice device;
                        device.transport = Transport::Classic;
                        device.address = format_bluetooth_address(info.Address.ullLong);
                        device.id = "win:classic:" + device.address;
                        device.address_available = true;
                        device.name = wide_to_utf8(info.szName);
                        device.connectable = true;
                        device.rssi_available = false;

                        if (shared->hooks.upsert_device)
                            shared->hooks.upsert_device(device);

                        info = {};
                        info.dwSize = sizeof(info);
                    }
                    while (BluetoothFindNextDevice(finder, &info));

                    BluetoothFindDeviceClose(finder);
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
                event.error = Error::Ok;
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

    // What a worker's catch (...) reports.
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

    // The completion of the LE call the core registered as op_id. Each op
    // runs on its own thread, so completions arrive in any order; the id is
    // what the core matches them by.
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
    constexpr const char* k_le_worker_failed = "Could not start a thread for the BLE operation";

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
        }

        Error initialize(std::string& message) override
        {
            if (initialized_)
                return Error::Ok;

            pin_module();

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
            release_radio_noexcept();

            // Everything is asked to stop; every thread the backend started
            // gets a bounded window to finish. Tearing down Winsock or COM
            // while a worker still has a call in flight is undefined
            // behavior, so a worker still running past it leaves both up
            // (the module is pinned, so its code stays mapped).
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

                if (owns_apartment_)
                    winrt::uninit_apartment();
            }
            else
            {
                GMBT_LOG(
                    "Bluetooth shutdown: %zu worker thread(s) still running after 5 s; "
                    "leaving Winsock and the COM apartment initialized",
                    remaining);
            }

            winsock_initialized_ = false;
            owns_apartment_ = false;
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

            // The failure of an open that got past the device lookup: what
            // it opened is closed and the core told why.
            const auto fail_open = [](
                const std::shared_ptr<SharedLeClientState>& owner,
                const std::shared_ptr<RemoteGattConnectionState>& opening,
                Error failure,
                const std::string& why)
            {
                opening->closing.store(true);
                close_le_client_connection_noexcept(opening);
                unregister_le_client_connection(owner, opening->handle);
                push_le_client_event(
                    owner,
                    "bluetooth_le_peripheral_open",
                    le_error_json(1, opening->handle, failure, why));
            };

            const bool started = spawn_detached_worker(
                workers_,
                [shared, state, address, fail_open]()
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
                                le_error_json(
                                    1,
                                    state->handle,
                                    Error::ConnectionFailed,
                                    "Windows found no Bluetooth LE device at this address"));
                            return;
                        }

                        // le_disconnect sets closing before it takes the state
                        // mutex to close what is stored, so checking under that
                        // mutex leaves one owner for the device: this worker if
                        // the open was already cancelled, close otherwise.
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

                                            // Only a link the game did not
                                            // close reports; Windows gives no
                                            // reason for the loss.
                                            if (!connected &&
                                                was_connected &&
                                                !current->closing.load())
                                            {
                                                push_le_client_event(
                                                    shared,
                                                    "bluetooth_le_peripheral_connection_state_changed",
                                                    "{\"connection\":" +
                                                        std::to_string(current->handle) +
                                                        ",\"is_connected\":false" +
                                                        ",\"error\":" +
                                                        std::to_string(static_cast<std::int32_t>(Error::Disconnected)) +
                                                        ",\"message\":\"The Bluetooth LE device disconnected\"}");
                                            }
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
                                le_error_json(1, state->handle, Error::ConnectionFailed, k_le_open_cancelled));
                            return;
                        }

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
                            // A peer that never answered is a timeout (the
                            // old 133); a refusal keeps its ATT meaning.
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

                            state->closing.store(true);
                            close_le_client_connection_noexcept(state);
                            unregister_le_client_connection(shared, state->handle);
                            push_le_client_event(
                                shared,
                                "bluetooth_le_peripheral_open",
                                le_error_json(
                                    unreachable ? 133 : 1,
                                    state->handle,
                                    outcome.error,
                                    outcome.message));
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
                            cancelled = state->closing.load();
                            if (!cancelled)
                            {
                                state->services = std::move(services);
                                state->connected.store(true);
                            }
                        }

                        if (cancelled)
                        {
                            // le_disconnect already closed the device; the
                            // services found after it are this worker's.
                            for (auto& pair : services)
                                close_remote_service_noexcept(pair.second);
                            push_le_client_event(
                                shared,
                                "bluetooth_le_peripheral_open",
                                le_error_json(1, state->handle, Error::ConnectionFailed, k_le_open_cancelled));
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
                        fail_open(shared, state, Error::ConnectionFailed, gatt_exception_outcome(error).message);
                    }
                    catch (...)
                    {
                        fail_open(
                            shared,
                            state,
                            Error::ConnectionFailed,
                            "Opening the Bluetooth LE device failed with an unexpected exception");
                    }
                });

            if (!started)
            {
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
            const bool started = spawn_detached_worker(
                workers_,
                [shared, op_id, state, remote]()
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
                            push_le_op_completion(
                                shared,
                                op_id,
                                gatt_result_outcome(result));
                            return;
                        }

                        std::unordered_map<
                            std::string,
                            std::shared_ptr<RemoteGattServiceState>> services;
                        LeOpResult found;

                        for (const auto& service : result.Services())
                        {
                            auto service_state =
                                std::make_shared<RemoteGattServiceState>();
                            service_state->uuid =
                                guid_to_uuid_string(service.Uuid());
                            service_state->service = service;
                            services[service_state->uuid] = service_state;

                            found.attributes.push_back(
                                LeAttribute{ service_state->uuid, 0 });
                        }

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
                        push_le_op_completion(
                            shared,
                            op_id,
                            Error::Ok,
                            std::string(),
                            std::move(found));
                    }
                    catch (const winrt::hresult_error& error)
                    {
                        push_le_op_completion(
                            shared,
                            op_id,
                            gatt_exception_outcome(error));
                    }
                    catch (...)
                    {
                        push_le_op_completion(
                            shared,
                            op_id,
                            Error::OperationFailed,
                            k_gatt_unexpected_failure);
                    }
                });

            if (!started)
            {
                message = k_le_worker_failed;
                return Error::OperationFailed;
            }

            message.clear();
            return Error::Ok;
        }

        Error le_characteristics_discover(
            std::uint64_t op_id,
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
            const bool started = spawn_detached_worker(
                workers_,
                [shared, op_id, state, service]()
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

                        std::unordered_map<
                            std::string,
                            std::shared_ptr<RemoteGattCharacteristicState>>
                            characteristics;

                        LeOpResult found;

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

                            found.attributes.push_back(LeAttribute{
                                characteristic_state->uuid,
                                static_cast<std::int32_t>(
                                    characteristic.CharacteristicProperties()) });
                        }

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

                        push_le_op_completion(
                            shared,
                            op_id,
                            Error::Ok,
                            std::string(),
                            std::move(found));
                    }
                    catch (const winrt::hresult_error& error)
                    {
                        push_le_op_completion(
                            shared,
                            op_id,
                            gatt_exception_outcome(error));
                    }
                    catch (...)
                    {
                        push_le_op_completion(
                            shared,
                            op_id,
                            Error::OperationFailed,
                            k_gatt_unexpected_failure);
                    }
                });

            if (!started)
            {
                message = k_le_worker_failed;
                return Error::OperationFailed;
            }

            message.clear();
            return Error::Ok;
        }

        Error le_descriptors_discover(
            std::uint64_t op_id,
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
            const bool started = spawn_detached_worker(
                workers_,
                [shared, op_id, state, characteristic]()
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
                            push_le_op_completion(
                                shared,
                                op_id,
                                gatt_result_outcome(result));
                            return;
                        }

                        std::unordered_map<
                            std::string,
                            std::shared_ptr<RemoteGattDescriptorState>>
                            descriptors;

                        LeOpResult found;

                        for (const auto& descriptor : result.Descriptors())
                        {
                            auto descriptor_state =
                                std::make_shared<RemoteGattDescriptorState>();
                            descriptor_state->uuid =
                                guid_to_uuid_string(descriptor.Uuid());
                            descriptor_state->descriptor = descriptor;
                            descriptors[descriptor_state->uuid] =
                                descriptor_state;

                            found.attributes.push_back(
                                LeAttribute{ descriptor_state->uuid, 0 });
                        }

                        {
                            std::scoped_lock lock(state->mutex);
                            characteristic->descriptors =
                                std::move(descriptors);
                        }

                        push_le_op_completion(
                            shared,
                            op_id,
                            Error::Ok,
                            std::string(),
                            std::move(found));
                    }
                    catch (const winrt::hresult_error& error)
                    {
                        push_le_op_completion(
                            shared,
                            op_id,
                            gatt_exception_outcome(error));
                    }
                    catch (...)
                    {
                        push_le_op_completion(
                            shared,
                            op_id,
                            Error::OperationFailed,
                            k_gatt_unexpected_failure);
                    }
                });

            if (!started)
            {
                message = k_le_worker_failed;
                return Error::OperationFailed;
            }

            message.clear();
            return Error::Ok;
        }

        Error le_characteristic_read(
            std::uint64_t op_id,
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
            const bool started = spawn_detached_worker(
                workers_,
                [shared, op_id, state, characteristic]()
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
                    }
                    catch (const winrt::hresult_error& error)
                    {
                        push_le_op_completion(
                            shared,
                            op_id,
                            gatt_exception_outcome(error));
                    }
                    catch (...)
                    {
                        push_le_op_completion(
                            shared,
                            op_id,
                            Error::OperationFailed,
                            k_gatt_unexpected_failure);
                    }
                });

            if (!started)
            {
                message = k_le_worker_failed;
                return Error::OperationFailed;
            }

            message.clear();
            return Error::Ok;
        }

        Error le_characteristic_write(
            std::uint64_t op_id,
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
            const bool started = spawn_detached_worker(
                workers_,
                [shared, op_id, state, characteristic, payload, with_response]()
                {
                    try
                    {
                        WinrtWorkerApartment apartment;
                        std::scoped_lock operation_lock(state->operation_mutex);
                        const auto option = with_response
                            ? WDBG::GattWriteOption::WriteWithResponse
                            : WDBG::GattWriteOption::WriteWithoutResponse;

                        const auto result =
                            characteristic->characteristic.WriteValueWithResultAsync(
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
                    }
                    catch (const winrt::hresult_error& error)
                    {
                        push_le_op_completion(
                            shared,
                            op_id,
                            gatt_exception_outcome(error));
                    }
                    catch (...)
                    {
                        push_le_op_completion(
                            shared,
                            op_id,
                            Error::OperationFailed,
                            k_gatt_unexpected_failure);
                    }
                });

            if (!started)
            {
                message = k_le_worker_failed;
                return Error::OperationFailed;
            }

            message.clear();
            return Error::Ok;
        }

        Error le_characteristic_subscribe(
            std::uint64_t op_id,
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

            const bool started = spawn_detached_worker(
                workers_,
                [shared, op_id,
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

                        // The WithResult form carries the ATT code of a refusal.
                        const auto result =
                            characteristic->characteristic
                                .WriteClientCharacteristicConfigurationDescriptorWithResultAsync(
                                    cccd_value)
                                .get();

                        if (result.Status() ==
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

                        push_le_op_completion(
                            shared,
                            op_id,
                            gatt_result_outcome(result));
                    }
                    catch (const winrt::hresult_error& error)
                    {
                        push_le_op_completion(
                            shared,
                            op_id,
                            gatt_exception_outcome(error));
                    }
                    catch (...)
                    {
                        push_le_op_completion(
                            shared,
                            op_id,
                            Error::OperationFailed,
                            k_gatt_unexpected_failure);
                    }
                });

            if (!started)
            {
                message = k_le_worker_failed;
                return Error::OperationFailed;
            }

            message.clear();
            return Error::Ok;
        }

        Error le_descriptor_read(
            std::uint64_t op_id,
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
            const bool started = spawn_detached_worker(
                workers_,
                [shared, op_id, state, descriptor]()
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
                    }
                    catch (const winrt::hresult_error& error)
                    {
                        push_le_op_completion(
                            shared,
                            op_id,
                            gatt_exception_outcome(error));
                    }
                    catch (...)
                    {
                        push_le_op_completion(
                            shared,
                            op_id,
                            Error::OperationFailed,
                            k_gatt_unexpected_failure);
                    }
                });

            if (!started)
            {
                message = k_le_worker_failed;
                return Error::OperationFailed;
            }

            message.clear();
            return Error::Ok;
        }

        Error le_descriptor_write(
            std::uint64_t op_id,
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

            const bool started = spawn_detached_worker(
                workers_,
                [shared, op_id, state, descriptor, payload]()
                {
                    try
                    {
                        WinrtWorkerApartment apartment;
                        std::scoped_lock operation_lock(state->operation_mutex);
                        // The WithResult form carries the ATT code of a refusal.
                        const auto result =
                            descriptor->descriptor.WriteValueWithResultAsync(
                                bytes_to_buffer(payload)).get();

                        push_le_op_completion(
                            shared,
                            op_id,
                            gatt_result_outcome(result));
                    }
                    catch (const winrt::hresult_error& error)
                    {
                        push_le_op_completion(
                            shared,
                            op_id,
                            gatt_exception_outcome(error));
                    }
                    catch (...)
                    {
                        push_le_op_completion(
                            shared,
                            op_id,
                            Error::OperationFailed,
                            k_gatt_unexpected_failure);
                    }
                });

            if (!started)
            {
                message = k_le_worker_failed;
                return Error::OperationFailed;
            }

            message.clear();
            return Error::Ok;
        }

        // Completes the call registered as op_id from another thread, after
        // the call has returned to the core.
        void push_le_completion_async(std::uint64_t op_id)
        {
            if (!hooks_.push_event)
                return;

            auto push_event = hooks_.push_event;

            const bool started = spawn_detached_worker(
                workers_,
                [push_event, op_id]()
                {
                    BackendEvent event;
                    event.type = BackendEventType::LeOpCompleted;
                    event.transport = Transport::LowEnergy;
                    event.op_id = op_id;
                    push_event(std::move(event));
                });

            // No thread: the core has registered the op already, so it can
            // take the completion before the call returns.
            if (!started)
            {
                BackendEvent event;
                event.type = BackendEventType::LeOpCompleted;
                event.transport = Transport::LowEnergy;
                event.op_id = op_id;
                hooks_.push_event(std::move(event));
            }
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
            if (le_advertising_.exchange(true))
            {
                push_le_completion_async(op_id);
                message.clear();
                return Error::Ok;
            }

            // The generic Windows advertisement publisher cannot write
            // system-reserved sections such as LocalName or Service UUIDs.
            // Those are published by a registered service's GattServiceProvider,
            // so a service UUID, its service data and the name go out only
            // through one; the publisher carries manufacturer data and the
            // TX power. Each refusal names the field, before anything starts.
            std::unordered_set<std::string> service_uuids;
            std::vector<WDBG::GattServiceProvider> providers;
            for (const auto& raw_uuid : data.service_uuids)
            {
                const std::string uuid = normalize_uuid(raw_uuid);
                const auto service_it = gatt_services_.find(uuid);
                if (service_it == gatt_services_.end() || !service_it->second || !service_it->second->provider)
                {
                    le_advertising_.store(false);
                    message = "service_uuids: Windows advertises a service UUID only through a registered GATT service, and " +
                        uuid + " is not one";
                    return Error::NotSupported;
                }
                if (service_uuids.insert(uuid).second)
                    providers.push_back(service_it->second->provider);
            }

            if (data.include_name && providers.empty())
            {
                le_advertising_.store(false);
                message = "include_name: Windows advertises the name only through a registered GATT service in service_uuids";
                return Error::NotSupported;
            }

            const bool start_publisher = !data.manufacturer_data.empty() || data.include_tx_power || settings.tx_power.has_value();
            if (!start_publisher && providers.empty())
            {
                le_advertising_.store(false);
                message = "Windows cannot advertise without manufacturer data, a power level or a registered GATT service";
                return Error::NotSupported;
            }

            try
            {
                advertise_connectable_ = settings.connectable;
                advertise_discoverable_ = data.include_name;
                advertise_include_power_ = data.include_tx_power;
                advertise_tx_power_.reset();
                if (settings.tx_power)
                    advertise_tx_power_ = advertise_tx_power_dbm(*settings.tx_power);
                advertise_service_uuids_ = std::move(service_uuids);
                advertise_service_data_.clear();
                for (const auto& entry : data.service_data)
                    advertise_service_data_[normalize_uuid(entry.uuid)] = entry.data;

                auto tracker = std::make_shared<AdvertiseStartTracker>();
                tracker->op_id = op_id;
                tracker->generation = ++advertise_generation_;
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

                advertiser_status_token_ = advertiser_.StatusChanged(
                    [this, tracker](
                        const WDBA::BluetoothLEAdvertisementPublisher&,
                        const WDBA::BluetoothLEAdvertisementPublisherStatusChangedEventArgs& args)
                    {
                        const auto status = args.Status();
                        if (status == WDBA::BluetoothLEAdvertisementPublisherStatus::Started)
                        {
                            advertise_start_progress(tracker);
                        }
                        else if (status == WDBA::BluetoothLEAdvertisementPublisherStatus::Aborted)
                        {
                            le_advertising_.store(false);
                            advertise_start_failed(
                                tracker,
                                map_bluetooth_error(args.Error()),
                                "Windows aborted the BLE advertisement: " + bluetooth_error_message(args.Error()));
                        }
                        else if (status == WDBA::BluetoothLEAdvertisementPublisherStatus::Stopped)
                        {
                            le_advertising_.store(false);
                            advertise_start_failed(
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
                        [this, tracker](
                            const WDBG::GattServiceProvider&,
                            const WDBG::GattServiceProviderAdvertisementStatusChangedEventArgs& args)
                        {
                            switch (args.Status())
                            {
                                case WDBG::GattServiceProviderAdvertisementStatus::Started:
                                    advertise_start_progress(tracker);
                                    break;
                                case WDBG::GattServiceProviderAdvertisementStatus::StartedWithoutAllAdvertisementData:
                                    GMBT_LOG("GATT service advertising started without all of its advertisement data");
                                    advertise_start_progress(tracker);
                                    break;
                                case WDBG::GattServiceProviderAdvertisementStatus::Aborted:
                                    advertise_start_failed(
                                        tracker,
                                        map_bluetooth_error(args.Error()),
                                        "Windows aborted GATT service advertising: " + bluetooth_error_message(args.Error()));
                                    break;
                                case WDBG::GattServiceProviderAdvertisementStatus::Stopped:
                                    advertise_start_failed(
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

                for (const auto& uuid : advertise_service_uuids_)
                    start_service_advertising(gatt_services_[uuid]);

                // Every provider was already advertising and there is no
                // publisher: nothing will report, so the start is done now.
                if (to_start == 0)
                    finish_advertise_start(tracker, Error::Ok, {});

                message.clear();
                return Error::Ok;
            }
            catch (const winrt::hresult_error& error)
            {
                // The core drops the op when this call fails; nothing may
                // complete it later.
                if (advertise_tracker_)
                {
                    advertise_tracker_->done.store(true);
                    revoke_advertise_handlers(advertise_tracker_);
                    advertise_tracker_.reset();
                }
                le_advertising_.store(false);
                release_advertiser_noexcept();
                message = winrt::to_string(error.message());
                return Error::OperationFailed;
            }
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
            const std::shared_ptr<AdvertiseStartTracker>& tracker,
            Error error,
            std::string message)
        {
            if (tracker->done.exchange(true))
                return false;

            revoke_advertise_handlers(tracker);

            if (hooks_.push_event)
            {
                BackendEvent event;
                event.type = BackendEventType::LeOpCompleted;
                event.transport = Transport::LowEnergy;
                event.op_id = tracker->op_id;
                event.error = error;
                event.message = std::move(message);
                hooks_.push_event(std::move(event));
            }
            return true;
        }

        void advertise_start_progress(const std::shared_ptr<AdvertiseStartTracker>& tracker)
        {
            if (tracker->remaining.fetch_sub(1) == 1)
                finish_advertise_start(tracker, Error::Ok, {});
        }

        // A failed start leaves nothing it began advertising, unless a newer
        // start already owns the advertisers.
        void advertise_start_failed(
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

            if (!finish_advertise_start(tracker, error, std::move(message)))
                return;

            if (tracker->generation != advertise_generation_.load())
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

            le_advertising_.store(false);
        }

        Error le_advertise_stop(std::string& message) override
        {
            if (advertise_tracker_)
            {
                finish_advertise_start(
                    advertise_tracker_,
                    Error::OperationFailed,
                    "Advertising stopped before it started");
                advertise_tracker_.reset();
            }

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
            clear_queued_notifies(notify_queue_);
            complete_parked_gatt_requests_noexcept();
            clear_gatt_services_noexcept();
            release_server_sessions_noexcept();
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

            // Refused before anything is created: Windows has no protection
            // level for a signed write.
            if (const auto* characteristics = root->find("characteristics"); characteristics && characteristics->is_array())
            {
                for (const auto& characteristic_value : characteristics->array_value)
                {
                    const auto* permissions_field = characteristic_value.is_object()
                        ? characteristic_value.find("permissions")
                        : nullptr;
                    const std::int32_t permissions = permissions_field ? permissions_field->as_int(0) : 0;
                    if ((permissions & (kPermissionWriteSigned | kPermissionWriteSignedMitm)) != 0)
                    {
                        message = "Windows has no signed-write permission: "
                            "BluetoothLeAttributePermission.WriteSigned and WriteSignedMitm are not supported";
                        return Error::NotSupported;
                    }
                }
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

                        characteristic_state->subscribed_token = characteristic_state->characteristic.SubscribedClientsChanged(
                            [this](const WDBG::GattLocalCharacteristic& characteristic, const winrt::Windows::Foundation::IInspectable&)
                            {
                                try
                                {
                                    for (const auto& client : characteristic.SubscribedClients())
                                    {
                                        std::string address;
                                        bool is_new = false;
                                        const std::string central = note_server_session(client.Session(), address, is_new);
                                        if (is_new)
                                            push_server_connection_state(central, address, true);
                                    }
                                }
                                catch (...)
                                {
                                }
                            });

                        characteristic_state->read_token = characteristic_state->characteristic.ReadRequested(
                            [this, service_uuid, characteristic_uuid](
                                const WDBG::GattLocalCharacteristic&,
                                const WDBG::GattReadRequestedEventArgs& args)
                            {
                                const auto deferral = args.GetDeferral();
                                try
                                {
                                    const auto request = args.GetRequestAsync().get();
                                    if (!request)
                                    {
                                        deferral.Complete();
                                        return;
                                    }

                                    const std::int32_t request_id = next_gatt_request_id_.fetch_add(1);
                                    {
                                        std::scoped_lock lock(gatt_request_mutex_);
                                        pending_gatt_reads_[request_id] = PendingGattRead{request, deferral};
                                    }

                                    std::string central_address;
                                    bool central_is_new = false;
                                    const std::string central = note_server_session(args.Session(), central_address, central_is_new);

                                    if (hooks_.push_event)
                                    {
                                        BackendEvent event;
                                        event.type = BackendEventType::LeEvent;
                                        event.transport = Transport::LowEnergy;
                                        event.event_type = "bluetooth_le_server_characteristic_read_request";
                                        event.json = make_server_request_json(
                                            request_id,
                                            central,
                                            central_address,
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
                                    deferral.Complete();
                                }
                            });

                        characteristic_state->write_token = characteristic_state->characteristic.WriteRequested(
                            [this, service_uuid, characteristic_uuid](
                                const WDBG::GattLocalCharacteristic&,
                                const WDBG::GattWriteRequestedEventArgs& args)
                            {
                                const auto deferral = args.GetDeferral();
                                try
                                {
                                    const auto request = args.GetRequestAsync().get();
                                    if (!request)
                                    {
                                        deferral.Complete();
                                        return;
                                    }

                                    const std::vector<std::uint8_t> value = buffer_to_bytes(request.Value());
                                    const bool with_response = request.Option() == WDBG::GattWriteOption::WriteWithResponse;
                                    const std::int32_t request_id = next_gatt_request_id_.fetch_add(1);
                                    // A write without response needs no answer: it is completed now and
                                    // the core keeps its value for GML, instead of the deferral waiting
                                    // on a respond_write nothing obliges the game to send.
                                    if (with_response)
                                    {
                                        std::scoped_lock lock(gatt_request_mutex_);
                                        pending_gatt_writes_[request_id] = PendingGattWrite{request, deferral, with_response};
                                    }
                                    else
                                    {
                                        deferral.Complete();
                                    }

                                    std::string central_address;
                                    bool central_is_new = false;
                                    const std::string central = note_server_session(args.Session(), central_address, central_is_new);

                                    if (hooks_.push_event)
                                    {
                                        BackendEvent event;
                                        event.type = BackendEventType::LeEvent;
                                        event.transport = Transport::LowEnergy;
                                        event.event_type = "bluetooth_le_server_characteristic_write_request";
                                        event.json = make_server_request_json(
                                            request_id,
                                            central,
                                            central_address,
                                            service_uuid,
                                            characteristic_uuid,
                                            {},
                                            request.Offset(),
                                            &value,
                                            with_response);
                                        hooks_.push_event(std::move(event));
                                    }
                                }
                                catch (...)
                                {
                                    deferral.Complete();
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
                                        const auto deferral = args.GetDeferral();
                                        try
                                        {
                                            const auto request = args.GetRequestAsync().get();
                                            if (!request)
                                            {
                                                deferral.Complete();
                                                return;
                                            }

                                            const std::int32_t request_id = next_gatt_request_id_.fetch_add(1);
                                            {
                                                std::scoped_lock lock(gatt_request_mutex_);
                                                pending_gatt_reads_[request_id] = PendingGattRead{request, deferral};
                                            }

                                            std::string central_address;
                                            bool central_is_new = false;
                                            const std::string central = note_server_session(args.Session(), central_address, central_is_new);

                                            if (hooks_.push_event)
                                            {
                                                BackendEvent event;
                                                event.type = BackendEventType::LeEvent;
                                                event.transport = Transport::LowEnergy;
                                                event.event_type = "bluetooth_le_server_descriptor_read_request";
                                                event.json = make_server_request_json(
                                                    request_id,
                                                    central,
                                                    central_address,
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
                                            deferral.Complete();
                                        }
                                    });

                                descriptor_state->write_token = descriptor_state->descriptor.WriteRequested(
                                    [this, service_uuid, characteristic_uuid, descriptor_uuid](
                                        const WDBG::GattLocalDescriptor&,
                                        const WDBG::GattWriteRequestedEventArgs& args)
                                    {
                                        const auto deferral = args.GetDeferral();
                                        try
                                        {
                                            const auto request = args.GetRequestAsync().get();
                                            if (!request)
                                            {
                                                deferral.Complete();
                                                return;
                                            }

                                            const std::vector<std::uint8_t> value = buffer_to_bytes(request.Value());
                                            const bool with_response = request.Option() == WDBG::GattWriteOption::WriteWithResponse;
                                            const std::int32_t request_id = next_gatt_request_id_.fetch_add(1);
                                            // A write without response needs no answer: it is completed now and
                                            // the core keeps its value for GML, instead of the deferral waiting
                                            // on a respond_write nothing obliges the game to send.
                                            if (with_response)
                                            {
                                                std::scoped_lock lock(gatt_request_mutex_);
                                                pending_gatt_writes_[request_id] = PendingGattWrite{request, deferral, with_response};
                                            }
                                            else
                                            {
                                                deferral.Complete();
                                            }

                                            std::string central_address;
                                            bool central_is_new = false;
                                            const std::string central = note_server_session(args.Session(), central_address, central_is_new);

                                            if (hooks_.push_event)
                                            {
                                                BackendEvent event;
                                                event.type = BackendEventType::LeEvent;
                                                event.transport = Transport::LowEnergy;
                                                event.event_type = "bluetooth_le_server_descriptor_write_request";
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
                                                hooks_.push_event(std::move(event));
                                            }
                                        }
                                        catch (...)
                                        {
                                            deferral.Complete();
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

                push_le_completion_async(op_id);

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
                WinrtWorkerApartment apartment;

                GMBT_LOG(
                    "Responding to GATT read: request_id=%d status=%d request_state=%d request_offset=%u bytes=%zu",
                    request_id,
                    status,
                    static_cast<int>(pending.request.State()),
                    pending.request.Offset(),
                    json::base64_decode(value_base64).size());

                // status is a BluetoothAttError the core validated: the ATT
                // code itself.
                if (status == 0)
                    pending.request.RespondWithValue(bytes_to_buffer(json::base64_decode(value_base64)));
                else
                    pending.request.RespondWithProtocolError(static_cast<std::uint8_t>(status));

                if (pending.deferral)
                    pending.deferral.Complete();

                GMBT_LOG(
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
                WinrtWorkerApartment apartment;

                GMBT_LOG(
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

                GMBT_LOG(
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

            // A notify to one central goes to its subscription only (R1-48).
            WDBG::GattSubscribedClient client{nullptr};
            if (!central.empty())
            {
                try
                {
                    for (const auto& subscribed : characteristic_it->second->characteristic.SubscribedClients())
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
                pending.characteristic = characteristic_it->second->characteristic;
                pending.client = client;
                pending.bytes = json::base64_decode(value_base64);
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
            {
                std::scoped_lock lock(discoverable_mutex_);
                classic_discoverable_stop_requested_ = true;
            }
            discoverable_wake_.notify_all();

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
        // A remote central's GattSession, kept from its first server event
        // until it closes. Windows has no "central connected" event for a
        // GATT server; the first request or subscription stands for it, and
        // the session closing is the disconnect (R1-23).
        struct ServerSession
        {
            WDBG::GattSession session{nullptr};
            winrt::event_token status_token{};
        };

        // The key the core knows the central by - its session's device id -
        // registering the session on first sight; is_new says it was. Empty
        // for a session that names no device.
        std::string note_server_session(const WDBG::GattSession& session, std::string& address, bool& is_new)
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

            std::scoped_lock lock(server_sessions_mutex_);
            if (server_sessions_.find(central) != server_sessions_.end())
                return central;

            ServerSession entry;
            entry.session = session;
            try
            {
                entry.status_token = session.SessionStatusChanged(
                    [this, central, address](const WDBG::GattSession&, const WDBG::GattSessionStatusChangedEventArgs& args)
                    {
                        if (args.Status() == WDBG::GattSessionStatus::Closed)
                            server_session_closed(central, address);
                    });
            }
            catch (...)
            {
            }
            server_sessions_.emplace(central, std::move(entry));
            is_new = true;
            return central;
        }

        void push_server_connection_state(const std::string& central, const std::string& address, bool connected)
        {
            if (!hooks_.push_event || central.empty())
                return;

            BackendEvent event;
            event.type = BackendEventType::LeEvent;
            event.transport = Transport::LowEnergy;
            event.event_type = "bluetooth_le_server_connection_state_changed";
            event.json = std::string("{\"connected\":") + (connected ? "true" : "false") + "," +
                server_central_json(central, address) + "}";
            hooks_.push_event(std::move(event));
        }

        void server_session_closed(const std::string& central, const std::string& address)
        {
            ServerSession entry;
            {
                std::scoped_lock lock(server_sessions_mutex_);
                const auto it = server_sessions_.find(central);
                if (it == server_sessions_.end())
                    return;
                entry = std::move(it->second);
                server_sessions_.erase(it);
            }

            try
            {
                entry.session.SessionStatusChanged(entry.status_token);
            }
            catch (...)
            {
            }
            push_server_connection_state(central, address, false);
        }

        // The server stopped: the core retires every central itself, so the
        // sessions are only let go.
        void release_server_sessions_noexcept() noexcept
        {
            std::unordered_map<std::string, ServerSession> sessions;
            {
                std::scoped_lock lock(server_sessions_mutex_);
                sessions.swap(server_sessions_);
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

        void start_service_advertising(const std::shared_ptr<LocalGattServiceState>& service)
        {
            if (!service || !service->provider)
                return;
            if (advertise_service_uuids_.find(service->uuid) == advertise_service_uuids_.end())
                return;

            WDBG::GattServiceProviderAdvertisingParameters parameters;
            parameters.IsConnectable(advertise_connectable_);
            parameters.IsDiscoverable(advertise_discoverable_);

            const auto data_it = advertise_service_data_.find(service->uuid);
            if (data_it != advertise_service_data_.end() && !data_it->second.empty())
                parameters.ServiceData(bytes_to_buffer(data_it->second));

            service->provider.StartAdvertising(parameters);
        }

        // Completes every request still parked, so no central waits out the ATT
        // timeout on a server that stopped or dropped its services. The core
        // answers the ones it holds first; this catches anything left.
        void complete_parked_gatt_requests_noexcept() noexcept
        {
            std::unordered_map<std::int32_t, PendingGattRead> reads;
            std::unordered_map<std::int32_t, PendingGattWrite> writes;
            {
                std::scoped_lock lock(gatt_request_mutex_);
                reads.swap(pending_gatt_reads_);
                writes.swap(pending_gatt_writes_);
            }
            if (reads.empty() && writes.empty())
                return;

            constexpr std::uint8_t unlikely_error = 0x0E;
            try
            {
                WinrtWorkerApartment apartment;
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
            catch (...)
            {
            }
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
        // Counts every thread this backend starts; see spawn_worker.
        std::shared_ptr<WorkerTracker> workers_ = std::make_shared<WorkerTracker>();
        std::shared_ptr<SharedClassicState> classic_;
        std::shared_ptr<SharedLeClientState> le_client_;
        std::shared_ptr<NotifyQueueState> notify_queue_ = std::make_shared<NotifyQueueState>();
        std::shared_ptr<AdvertiseStartTracker> advertise_tracker_;
        std::atomic<std::uint64_t> advertise_generation_{0};

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
        std::optional<std::int16_t> advertise_tx_power_;
        // The registered services advertise_start listed: only these advertise.
        std::unordered_set<std::string> advertise_service_uuids_;

        std::mutex server_sessions_mutex_;
        std::unordered_map<std::string, ServerSession> server_sessions_;
        std::unordered_map<std::string, std::vector<std::uint8_t>> advertise_service_data_;

        std::atomic_bool le_server_open_{false};
        std::unordered_map<std::string, std::shared_ptr<LocalGattServiceState>> gatt_services_;
        std::mutex gatt_request_mutex_;
        std::unordered_map<std::int32_t, PendingGattRead> pending_gatt_reads_;
        std::unordered_map<std::int32_t, PendingGattWrite> pending_gatt_writes_;
        std::atomic<std::int32_t> next_gatt_request_id_{1};

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
    };

    std::unique_ptr<Backend> create_platform_backend(CoreHooks hooks)
    {
        return std::make_unique<WindowsBackend>(std::move(hooks));
    }
}

#endif
