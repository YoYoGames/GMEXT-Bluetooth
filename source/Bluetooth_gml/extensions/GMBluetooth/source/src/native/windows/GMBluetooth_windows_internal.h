#pragma once

// The Windows backend's shared declarations: the helpers every role
// uses, the state the roles share and WindowsBackend itself. Each role
// defines its WindowsBackend members in its own GMBluetooth_windows_*.cpp.

#include "GMBluetooth_backend.h"
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
#include <string_view>
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

    namespace win
    {
        inline std::string format_bluetooth_address(std::uint64_t address)
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

        inline std::string wide_to_utf8(const wchar_t* text)
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

        inline std::wstring utf8_to_wide(const std::string& text)
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
        inline std::string expand_short_uuid(const std::string& text)
        {
            if (text.size() == 4)
                return "0000" + text + "-0000-1000-8000-00805f9b34fb";
            if (text.size() == 8)
                return text + "-0000-1000-8000-00805f9b34fb";
            return text;
        }

        inline bool parse_guid(const std::string& text, GUID& out)
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

        inline bool parse_bluetooth_address(const std::string& text, BTH_ADDR& out)
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

        inline Error map_bluetooth_error(WDB::BluetoothError error)
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

        inline std::string bluetooth_error_message(WDB::BluetoothError error)
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

        // Windows can report a failure with BluetoothError::Success.
        inline Error map_bluetooth_failure(WDB::BluetoothError error)
        {
            const auto mapped = map_bluetooth_error(error);
            return mapped == Error::Ok ? Error::OperationFailed : mapped;
        }

        inline std::string bluetooth_failure_message(WDB::BluetoothError error)
        {
            auto message = bluetooth_error_message(error);
            return message.empty() ? "Windows gave no reason" : message;
        }

        inline void trim_trailing_space(std::string& text)
        {
            while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0)
                text.pop_back();
        }

        // "connect failed (10060): A connection attempt failed ...", the
        // system text in UTF-8 without the CRLF FormatMessage ends it with.
        // Winsock and Win32 error codes share the system message table.
        inline std::string system_error_message(const char* operation, DWORD error)
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

        inline void worker_finished(WorkerTracker& tracker)
        {
            std::scoped_lock lock(tracker.mutex);
            if (--tracker.count == 0)
                tracker.idle.notify_all();
        }

        // True when every worker finished within timeout.
        inline bool wait_for_workers(WorkerTracker& tracker, std::chrono::milliseconds timeout, std::size_t& remaining)
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
        inline std::string normalize_uuid(std::string value)
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

        inline bool parse_winrt_guid(const std::string& text, winrt::guid& out)
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

        inline WSS::IBuffer bytes_to_buffer(const std::vector<std::uint8_t>& bytes)
        {
            WSS::DataWriter writer;
            if (!bytes.empty())
                writer.WriteBytes(bytes);
            return writer.DetachBuffer();
        }

        inline std::vector<std::uint8_t> buffer_to_bytes(const WSS::IBuffer& buffer)
        {
            if (!buffer)
                return {};

            WSS::DataReader reader = WSS::DataReader::FromBuffer(buffer);
            std::vector<std::uint8_t> bytes(reader.UnconsumedBufferLength());
            if (!bytes.empty())
                reader.ReadBytes(bytes);
            return bytes;
        }

        // "aa:bb:cc:dd:ee:ff" in the uppercase form every id carries; empty
        // when it is not an address in that shape.
        inline std::string normalized_address(std::string address)
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

        // The device ids every platform shares (R2-127): "classic:" and the
        // address for a Classic device; "le:public:" or "le:random:" and the
        // address for an LE device of a known address type, "le:" and the
        // address when the type is not known. address is uppercase.
        inline std::string classic_device_id(const std::string& address)
        {
            return "classic:" + address;
        }

        inline std::string le_device_id(const std::string& address, std::optional<WDB::BluetoothAddressType> type)
        {
            if (type == WDB::BluetoothAddressType::Public)
                return "le:public:" + address;
            if (type == WDB::BluetoothAddressType::Random)
                return "le:random:" + address;
            return "le:" + address;
        }

        // A remote device id ends in its address ("BluetoothLE#BluetoothLE
        // <local>-<remote>"); empty when it does not.
        inline std::string address_from_device_id(const std::string& device_id)
        {
            const auto dash = device_id.rfind('-');
            if (dash == std::string::npos)
                return std::string();
            return normalized_address(device_id.substr(dash + 1));
        }

        struct LocalGattDescriptorState
        {
            WDBG::GattLocalDescriptor descriptor{nullptr};
            winrt::event_token read_token{};
            winrt::event_token write_token{};
        };

        struct LocalGattCharacteristicState
        {
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
            // By the core's id: two characteristics may share a UUID.
            std::unordered_map<std::uint64_t, std::shared_ptr<LocalGattCharacteristicState>> characteristics;
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
            // Signalled when the core gives bytes back (room again) and when
            // the connection must stop.
            std::condition_variable receive_cv;
            // Bytes pushed as ClassicDataAvailable that the core has not
            // given back through classic_receive_consumed. Past
            // k_classic_receive_budget the receive loop stops reading and
            // RFCOMM flow control slows the peer.
            std::size_t outstanding = 0;

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

        struct SharedClassicState
        {
            CoreHooks hooks;
            std::shared_ptr<std::atomic_bool> alive =
                std::make_shared<std::atomic_bool>(true);
            std::shared_ptr<WorkerTracker> workers;

            mutable std::mutex connections_mutex;
            std::unordered_map<std::uint64_t, std::shared_ptr<ClassicConnectionState>> connections;
        };

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

        // Classic discovery. A stop bumps the generation and reports the
        // stop itself, so an inquiry thread whose generation is stale ends
        // unobserved.
        struct ClassicScanState
        {
            std::mutex mutex;
            std::atomic<std::uint64_t> generation{0};
            std::atomic_bool running{false};
        };

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

        // An le_advertise_start in flight. Publisher and service providers start
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

        inline std::string guid_to_uuid_string(const winrt::guid& value)
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
            // The registered services le_advertise_start listed: only these advertise.
            std::unordered_set<std::string> advertise_service_uuids;
            std::unordered_map<std::string, std::vector<std::uint8_t>> advertise_service_data;

            std::mutex gatt_request_mutex;
            std::unordered_map<std::int32_t, PendingGattRead> pending_gatt_reads;
            std::unordered_map<std::int32_t, PendingGattWrite> pending_gatt_writes;
            std::atomic<std::int32_t> next_gatt_request_id{1};

            std::mutex server_sessions_mutex;
            std::unordered_map<std::string, ServerSession> server_sessions;
        };

        inline void push_le_event(const std::shared_ptr<SharedLeState>& le, BackendEvent event)
        {
            if (!le || !le->alive.load() || !le->hooks.push_event)
                return;
            le->hooks.push_event(std::move(event));
        }

        inline void complete_le_op(const std::shared_ptr<SharedLeState>& le, std::uint64_t op_id, Error error, std::string message)
        {
            BackendEvent event;
            event.type = BackendEventType::LeOpCompleted;
            event.transport = Transport::LowEnergy;
            event.op_id = op_id;
            event.error = error;
            event.message = std::move(message);
            push_le_event(le, std::move(event));
        }

        inline void push_bluetooth_state(const std::shared_ptr<SharedLeState>& le)
        {
            BackendEvent event;
            event.type = BackendEventType::StateChanged;
            event.transport = Transport::Unknown;
            event.value = le->bluetooth_state.load();
            push_le_event(le, std::move(event));
        }

        // Defined by the role that owns them, used by another.
        std::string wsa_message(const char* operation, int error);
        void connection_request_close(const std::shared_ptr<ClassicConnectionState>& state);
        Error find_paired_classic_devices(std::vector<DiscoveredDevice>& out, std::string& message);
        void close_le_client_connection_noexcept(
            const std::shared_ptr<RemoteGattConnectionState>& state);
        bool has_classic_radio();
        void query_adapter(const std::shared_ptr<SharedLeState>& le) noexcept;
        void release_radio_noexcept(SharedLeState& le) noexcept;
        void release_adapter_watch_noexcept(SharedLeState& le) noexcept;
        void start_adapter_watch(
            const std::shared_ptr<SharedLeState>& le,
            const std::weak_ptr<SerialWorker>& worker) noexcept;
        std::optional<WDB::BluetoothAddressType> find_address_type(SharedLeState& le, std::uint64_t address);
        bool connection_parameters_api_present() noexcept;
        void request_radio_on(const std::shared_ptr<SharedLeState>& le, const WDR::Radio& radio) noexcept;
        void find_le_devices(
            const std::shared_ptr<SharedLeState>& le,
            const winrt::hstring& selector,
            std::vector<DiscoveredDevice>& out);

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

            Error initialize(std::string& message) override;

            void shutdown() override;

            bool supports_ble() const override { return le_->ble_supported.load(); }
            bool supports_le_advertise() const override { return le_->ble_supported.load() && le_->le_peripheral_supported.load(); }
            bool supports_le_server() const override { return le_->ble_supported.load() && le_->le_peripheral_supported.load(); }
            bool supports_classic() const override { return le_->classic_supported.load(); }
            bool supports_classic_server() const override { return le_->classic_supported.load(); }

            bool pairing_is_supported(const DiscoveredDevice& device) const override;

            bool feature_supported(std::int32_t feature) const override;

            std::int32_t current_bluetooth_state() const override;

            PermissionStatus permission_status() const override;

            Error permission_request(std::string& message) override;

            Error request_enable(std::string& message) override;

            Error device_from_id(const std::string& id, DiscoveredDevice& device, std::string& message) override;

            Error le_connected_devices_query(
                std::uint64_t query_id,
                const std::vector<std::string>& service_uuids,
                std::string& message) override;

            Error paired_devices_query(std::uint64_t query_id, std::string& message) override;

            // ===== LE scan =====

            Error le_scan_start(const LeScanSettings& settings, const std::vector<LeScanFilter>& filters, std::string& message) override;

            Error le_scan_stop(std::string& message) override;

            bool le_scan_is_running() const override;

            // ===== LE client =====

            Error le_connect(
                std::uint64_t connection,
                const DiscoveredDevice& device,
                std::chrono::milliseconds timeout,
                std::string& message) override;

            Error le_disconnect(
                std::uint64_t connection,
                std::string& message) override;

            bool le_connection_is_connected(
                std::uint64_t connection) const override;

            std::int32_t le_connection_mtu(
                std::uint64_t connection) const override;

            Error le_request_connection_priority(
                std::uint64_t connection,
                std::int32_t priority,
                std::string& message) override;

            Error le_services_discover(
                std::uint64_t op_id,
                std::uint64_t connection,
                std::string& message) override;

            Error le_characteristics_discover(
                std::uint64_t op_id,
                std::uint64_t connection,
                const LeAttributeRef& attribute,
                std::string& message) override;

            Error le_descriptors_discover(
                std::uint64_t op_id,
                std::uint64_t connection,
                const LeAttributeRef& attribute,
                std::string& message) override;

            Error le_characteristic_read(
                std::uint64_t op_id,
                std::uint64_t connection,
                const LeAttributeRef& attribute,
                std::string& message) override;

            Error le_characteristic_write(
                std::uint64_t op_id,
                std::uint64_t connection,
                const LeAttributeRef& attribute,
                const std::vector<std::uint8_t>& value,
                bool with_response,
                std::string& message) override;

            Error le_characteristic_subscribe(
                std::uint64_t op_id,
                std::uint64_t connection,
                const LeAttributeRef& attribute,
                std::int32_t mode,
                std::string& message) override;

            Error le_descriptor_read(
                std::uint64_t op_id,
                std::uint64_t connection,
                const LeAttributeRef& attribute,
                std::string& message) override;

            Error le_descriptor_write(
                std::uint64_t op_id,
                std::uint64_t connection,
                const LeAttributeRef& attribute,
                const std::vector<std::uint8_t>& value,
                std::string& message) override;

            // ===== BLE Advertiser =====

            Error le_advertise_start(std::uint64_t op_id, const LeAdvertiseSettings& settings, const LeAdvertiseData& data, std::string& message) override;

            Error le_advertise_stop(std::string& message) override;

            bool le_advertise_is_running() const override;

            // ===== BLE GATT Server =====

            Error le_server_start(std::string& message) override;

            Error le_server_stop(std::string& message) override;

            bool le_server_is_running() const override;

            std::int32_t le_server_connection_mtu(const std::string& central) const override;

            Error le_server_add_service(std::uint64_t op_id, const LeServerService& service, std::string& message) override;

            Error le_server_clear_services(std::string& message) override;

            Error le_server_respond_read(
                std::int32_t request_id,
                std::int32_t status,
                const std::vector<std::uint8_t>& value,
                std::string& message) override;

            Error le_server_respond_write(
                std::int32_t request_id,
                std::int32_t status,
                std::string& message) override;

            Error le_server_notify_value(
                std::uint64_t characteristic_id,
                const std::string& central,
                const std::vector<std::uint8_t>& value,
                std::string& message) override;

            // ===== Classic =====

            Error classic_scan_start(std::string& message) override;

            Error classic_scan_stop(std::string& message) override;

            bool classic_scan_is_running() const override;

            Error classic_connect(
                std::uint64_t connection,
                const DiscoveredDevice& device,
                const std::string& service_uuid,
                std::chrono::milliseconds timeout,
                std::string& message) override;

            Error pair(
                std::uint64_t device_handle,
                const DiscoveredDevice& device,
                std::string& message) override;

            bool is_paired(const DiscoveredDevice& device) const override;

            Error classic_disconnect(
                std::uint64_t connection,
                std::string& message) override;

            bool classic_connection_is_connected(std::uint64_t connection) const override;

            void classic_receive_consumed(std::uint64_t connection, std::size_t bytes) override;

            Error classic_send_bytes(
                std::uint64_t connection,
                const std::uint8_t* data,
                std::size_t size,
                std::string& message) override;

            Error classic_server_start(
                const std::string& name,
                const std::string& service_uuid,
                std::string& message) override;

            Error classic_server_stop(std::string& message) override;

            bool classic_server_is_running() const override;

            Error classic_discoverable_start(std::int32_t duration_seconds, std::string& message) override;

            Error classic_discoverable_stop(std::string& message) override;

            bool classic_discoverable_is_running() const override;

            ~WindowsBackend() override;

        private:
            template <typename Find>
            Error post_devices_query(std::uint64_t query_id, std::string& message, Find find);

            template <typename Body>
            Error post_le_op(
                const std::shared_ptr<RemoteGattConnectionState>& state,
                std::uint64_t op_id,
                std::string& message,
                Body&& body);

            void end_discoverable(HANDLE radio);

            void complete_parked_gatt_requests_noexcept() noexcept;

            void clear_gatt_services_noexcept() noexcept;

            void release_advertiser_noexcept() noexcept;

            void release_watcher_noexcept() noexcept;

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
    }
}

#endif
