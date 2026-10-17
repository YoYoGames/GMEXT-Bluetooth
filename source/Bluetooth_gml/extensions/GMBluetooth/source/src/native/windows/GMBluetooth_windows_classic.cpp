// Windows backend: Bluetooth Classic - scan, RFCOMM client and server,
// discoverability and pairing - over Winsock and the Win32 Bluetooth API.

#include "GMBluetooth_windows_internal.h"

#if defined(_WIN32)

namespace gmbluetooth::win
{
    namespace
    {
        // Authenticated and encrypted, as Android's secure RFCOMM is: 0, or
        // the WSA error of the option that was refused.
        int request_secure_link(SOCKET socket_handle)
        {
            const ULONG enabled = TRUE;
            const auto set = [&](unsigned long option)
            {
                return setsockopt(
                    socket_handle,
                    SOL_RFCOMM,
                    static_cast<int>(option),
                    reinterpret_cast<const char*>(&enabled),
                    sizeof(enabled)) == SOCKET_ERROR ? WSAGetLastError() : 0;
            };

            if (const int error = set(SO_BTH_AUTHENTICATE); error != 0)
                return error;
            return set(SO_BTH_ENCRYPT);
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

        // At most this much accepted data waits to be sent.
        constexpr std::size_t k_classic_send_cap = 1024 * 1024;

        constexpr std::size_t k_classic_send_chunk = 64 * 1024;

        constexpr auto k_classic_connect_slice = std::chrono::milliseconds(100);

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
            unregister_connection(shared, state->handle);
        }

        // An open connection's receive side. Each chunk read goes to the core
        // as its own ClassicDataAvailable; reading stops while
        // Backend::k_classic_receive_budget bytes are outstanding, and the
        // paused loop sends ClassicReceivePaused on a timer.
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
                // The core expires values only as they come and go, so while
                // paused the loop asks it to on a timer: a chunk GML never
                // fetches still ends the connection.
                for (;;)
                {
                    bool paused = false;
                    {
                        std::unique_lock lock(state->receive_mutex);
                        paused = !state->receive_cv.wait_for(lock, Backend::k_classic_receive_paused_expiry_check,
                            [&state, &stopping]()
                            {
                                return state->outstanding < Backend::k_classic_receive_budget || stopping();
                            });
                    }
                    if (!paused)
                        break;
                    if (shared->alive->load() && shared->hooks.push_event)
                    {
                        BackendEvent event;
                        event.type = BackendEventType::ClassicReceivePaused;
                        event.transport = Transport::Classic;
                        event.connection = state->handle;
                        event.device = state->device;
                        shared->hooks.push_event(std::move(event));
                    }
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
                    if (shared->alive->load() && shared->hooks.push_event)
                    {
                        BackendEvent event;
                        event.type = BackendEventType::ClassicDataAvailable;
                        event.transport = Transport::Classic;
                        event.connection = state->handle;
                        event.device = state->device;
                        event.bytes.assign(buffer.begin(), buffer.begin() + received);
                        {
                            std::scoped_lock lock(state->receive_mutex);
                            state->outstanding += event.bytes.size();
                        }
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
        // or a shutdown ends it within one, and a link not open by deadline,
        // the end of the window the connect call was given, fails Timeout
        // (R2-135); a pairing prompt counts toward it. Once open, the
        // connection's receive loop runs on this same thread.
        void run_classic_connect(
            const std::shared_ptr<SharedClassicState>& shared,
            const std::shared_ptr<ClassicConnectionState>& state,
            BTH_ADDR address,
            GUID service_guid,
            std::chrono::steady_clock::time_point deadline)
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

            if (const int error = request_secure_link(socket_handle); error != 0)
            {
                fail_wsa("setsockopt", error);
                return;
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
                device.id = classic_device_id(device.address);
                device.address_available = true;
                device.connectable = true;

                // A peer that connects in was not seen by a scan (R2-73).
                const std::uint64_t device_handle = shared->hooks.add_device
                    ? shared->hooks.add_device(device)
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

        DiscoveredDevice classic_device(const BLUETOOTH_DEVICE_INFO& info)
        {
            DiscoveredDevice device;
            device.transport = Transport::Classic;
            device.address = format_bluetooth_address(info.Address.ullLong);
            device.id = classic_device_id(device.address);
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
    }

    std::string wsa_message(const char* operation, int error)
    {
        return system_error_message(operation ? operation : "Bluetooth socket operation", static_cast<DWORD>(error));
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

    Error WindowsBackend::classic_scan_start(std::string& message)
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
    // scan is marked stopped, and the inquiry thread, its generation now
    // stale, ends unobserved. Nothing is reported: the core asked.
    Error WindowsBackend::classic_scan_stop(std::string& message)
    {
        {
            std::scoped_lock lock(classic_scan_->mutex);
            classic_scan_->running.store(false);
            ++classic_scan_->generation;
        }

        message.clear();
        return Error::Ok;
    }

    bool WindowsBackend::classic_scan_is_running() const
    {
        return classic_scan_->running.load();
    }

    Error WindowsBackend::classic_connect(
        std::uint64_t connection,
        const DiscoveredDevice& device,
        const std::string& service_uuid,
        std::chrono::milliseconds timeout,
        std::string& message)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;

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

        // The core has the device cached under this id already.
        const auto shared = classic_;
        const std::uint64_t device_handle = hooks_.add_device
            ? hooks_.add_device(device)
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
            [shared, state, address, service_guid, deadline]()
            {
                run_classic_connect(shared, state, address, service_guid, deadline);
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

    Error WindowsBackend::pair(
        std::uint64_t device_handle,
        const DiscoveredDevice& device,
        std::string& message)
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

    bool WindowsBackend::is_paired(const DiscoveredDevice& device) const
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

    Error WindowsBackend::classic_disconnect(
        std::uint64_t connection,
        std::string& message)
    {
        auto state = find_connection(classic_, connection);
        if (!state)
        {
            message = "Invalid Classic connection handle";
            return Error::InvalidHandle;
        }

        // Open or still connecting. A connect cancelled here reports
        // nothing (the core answers its callback); an open connection's
        // receive loop reports the disconnect once it has closed.
        connection_request_close(state);

        message.clear();
        return Error::Ok;
    }

    bool WindowsBackend::classic_connection_is_connected(std::uint64_t connection) const
    {
        auto state = find_connection(classic_, connection);
        return state && state->connected.load();
    }

    void WindowsBackend::classic_receive_consumed(std::uint64_t connection, std::size_t bytes)
    {
        auto state = find_connection(classic_, connection);
        if (!state)
            return;

        {
            std::scoped_lock lock(state->receive_mutex);
            state->outstanding -= std::min(bytes, state->outstanding);
        }
        state->receive_cv.notify_all();
    }

    Error WindowsBackend::classic_send_bytes(
        std::uint64_t connection,
        const std::uint8_t* data,
        std::size_t size,
        std::string& message)
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

    Error WindowsBackend::classic_server_start(
        const std::string& name,
        const std::string& service_uuid,
        std::string& message)
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

        if (const int error = request_secure_link(listener); error != 0)
        {
            closesocket(listener);
            message = wsa_message("setsockopt", error);
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
    Error WindowsBackend::classic_server_stop(std::string& message)
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

    bool WindowsBackend::classic_server_is_running() const
    {
        return classic_server_running_.load();
    }

    Error WindowsBackend::classic_discoverable_start(std::int32_t duration_seconds, std::string& message)
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
    void WindowsBackend::end_discoverable(HANDLE radio)
    {
        if (!radio)
            return;
        BluetoothEnableDiscovery(radio, FALSE);
        if (discoverable_restore_connectable_.exchange(false))
            BluetoothEnableIncomingConnections(radio, FALSE);
    }

    Error WindowsBackend::classic_discoverable_stop(std::string& message)
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

    bool WindowsBackend::classic_discoverable_is_running() const
    {
        if (!discoverable_radio_)
            return classic_discoverable_active_.load();
        return BluetoothIsDiscoverable(discoverable_radio_) != FALSE;
    }
}

#endif
