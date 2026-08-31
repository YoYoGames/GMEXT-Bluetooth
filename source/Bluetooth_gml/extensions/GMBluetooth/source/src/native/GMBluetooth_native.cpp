#include "native/GMBluetoothInternal_native.h"
#include "GMBluetooth_backend.h"

#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
    using gmbluetooth::Backend;
    using gmbluetooth::BackendEvent;
    using gmbluetooth::BackendEventType;
    using gmbluetooth::CoreHooks;
    using gmbluetooth::DiscoveredDevice;
    using gmbluetooth::Error;
    using gmbluetooth::PermissionStatus;
    using gmbluetooth::Transport;

    constexpr std::uint64_t HANDLE_MAGIC = 0x42ULL;
    constexpr std::uint64_t HANDLE_TYPE_DEVICE = 0x01ULL;
    constexpr std::uint64_t HANDLE_TYPE_CLASSIC_CONNECTION = 0x02ULL;

    std::uint64_t make_handle(std::uint64_t type, std::uint32_t id)
    {
        return (HANDLE_MAGIC << 40) |
               (type << 32) |
               static_cast<std::uint64_t>(id);
    }

    bool is_handle_type(std::uint64_t handle, std::uint64_t type)
    {
        return ((handle >> 40) & 0xFFULL) == HANDLE_MAGIC &&
               ((handle >> 32) & 0xFFULL) == type &&
               (handle & 0xFFFFFFFFULL) != 0;
    }

    struct DeviceEntry
    {
        std::uint64_t handle = 0;
        Transport transport = Transport::Unknown;

        std::string id;
        std::string name;
        std::string address;

        bool address_available = false;
        bool connectable = false;

        std::int32_t rssi = 0;
        bool rssi_available = false;
    };

    struct ClassicConnectionEntry
    {
        std::uint64_t handle = 0;
        std::uint64_t device = 0;
        bool connected = false;
    };

    enum class EventType : std::uint8_t
    {
        DeviceFound,
        Backend,
    };

    struct Event
    {
        EventType type = EventType::DeviceFound;
        std::uint64_t device = 0;
        BackendEvent backend;
    };

    std::unique_ptr<Backend> g_backend;
    bool g_initialized = false;

    std::mutex g_devices_mutex;
    std::unordered_map<std::string, std::uint64_t> g_device_by_id;
    std::map<std::uint64_t, DeviceEntry> g_devices;
    std::vector<std::uint64_t> g_device_order;
    std::uint32_t g_next_device_id = 1;

    std::mutex g_connections_mutex;
    std::map<std::uint64_t, ClassicConnectionEntry> g_classic_connections;
    std::uint32_t g_next_classic_connection_id = 1;

    std::mutex g_events_mutex;
    std::deque<Event> g_events;

    std::mutex g_error_mutex;
    Error g_last_error = Error::Ok;
    std::string g_last_error_message;

    std::mutex g_connect_callbacks_mutex;
    std::unordered_map<std::uint64_t, gm::wire::GMFunction> g_classic_connect_callbacks;

    gm::wire::GMFunction g_callback_device_found = nullptr;
    gm::wire::GMFunction g_callback_scan_stopped = nullptr;
    gm::wire::GMFunction g_callback_classic_client_connected = nullptr;
    gm::wire::GMFunction g_callback_classic_data = nullptr;
    gm::wire::GMFunction g_callback_classic_disconnected = nullptr;

    void set_last_error(Error error, std::string message = {})
    {
        std::scoped_lock lock(g_error_mutex);
        g_last_error = error;
        g_last_error_message = std::move(message);
    }

    void push_event(Event event)
    {
        std::scoped_lock lock(g_events_mutex);
        g_events.emplace_back(std::move(event));
    }

    void push_backend_event(BackendEvent event)
    {
        Event wrapped;
        wrapped.type = EventType::Backend;
        wrapped.backend = std::move(event);
        push_event(std::move(wrapped));
    }

    std::uint64_t upsert_device(const DiscoveredDevice& incoming)
    {
        if (incoming.id.empty())
            return 0;

        std::uint64_t handle = 0;
        bool is_new = false;

        {
            std::scoped_lock lock(g_devices_mutex);

            auto existing = g_device_by_id.find(incoming.id);
            if (existing == g_device_by_id.end())
            {
                handle = make_handle(HANDLE_TYPE_DEVICE, g_next_device_id++);

                DeviceEntry entry;
                entry.handle = handle;
                entry.transport = incoming.transport;
                entry.id = incoming.id;
                entry.name = incoming.name;
                entry.address = incoming.address;
                entry.address_available = incoming.address_available;
                entry.connectable = incoming.connectable;
                entry.rssi = incoming.rssi;
                entry.rssi_available = incoming.rssi_available;

                g_device_by_id.emplace(entry.id, handle);
                g_devices.emplace(handle, std::move(entry));
                g_device_order.push_back(handle);
                is_new = true;
            }
            else
            {
                handle = existing->second;
                auto found = g_devices.find(handle);
                if (found != g_devices.end())
                {
                    DeviceEntry& entry = found->second;
                    entry.transport = incoming.transport;

                    if (!incoming.name.empty())
                        entry.name = incoming.name;

                    if (incoming.address_available)
                    {
                        entry.address = incoming.address;
                        entry.address_available = true;
                    }

                    entry.connectable = entry.connectable || incoming.connectable;

                    if (incoming.rssi_available)
                    {
                        entry.rssi = incoming.rssi;
                        entry.rssi_available = true;
                    }
                }
            }
        }

        if (is_new)
        {
            Event event;
            event.type = EventType::DeviceFound;
            event.device = handle;
            push_event(std::move(event));
        }

        return handle;
    }

    std::uint64_t create_classic_connection(std::uint64_t device)
    {
        std::scoped_lock lock(g_connections_mutex);

        const std::uint64_t handle =
            make_handle(HANDLE_TYPE_CLASSIC_CONNECTION, g_next_classic_connection_id++);

        ClassicConnectionEntry entry;
        entry.handle = handle;
        entry.device = device;
        g_classic_connections.emplace(handle, entry);
        return handle;
    }

    bool copy_device(std::uint64_t handle, DeviceEntry& out)
    {
        if (!is_handle_type(handle, HANDLE_TYPE_DEVICE))
            return false;

        std::scoped_lock lock(g_devices_mutex);
        const auto it = g_devices.find(handle);
        if (it == g_devices.end())
            return false;

        out = it->second;
        return true;
    }

    bool copy_classic_connection(std::uint64_t handle, ClassicConnectionEntry& out)
    {
        if (!is_handle_type(handle, HANDLE_TYPE_CLASSIC_CONNECTION))
            return false;

        std::scoped_lock lock(g_connections_mutex);
        const auto it = g_classic_connections.find(handle);
        if (it == g_classic_connections.end())
            return false;

        out = it->second;
        return true;
    }

    void set_connection_connected(std::uint64_t handle, bool connected)
    {
        std::scoped_lock lock(g_connections_mutex);
        auto it = g_classic_connections.find(handle);
        if (it != g_classic_connections.end())
            it->second.connected = connected;
    }

    void erase_connection(std::uint64_t handle)
    {
        std::scoped_lock lock(g_connections_mutex);
        g_classic_connections.erase(handle);
    }

    void clear_devices()
    {
        std::scoped_lock lock(g_devices_mutex);
        g_device_by_id.clear();
        g_devices.clear();
        g_device_order.clear();
    }

    void clear_connections()
    {
        std::scoped_lock lock(g_connections_mutex);
        g_classic_connections.clear();
    }

    void clear_events()
    {
        std::scoped_lock lock(g_events_mutex);
        g_events.clear();
    }
}

bool bluetooth_initialize()
{
    if (g_initialized)
        return true;

    set_last_error(Error::Ok, {});

    CoreHooks hooks;
    hooks.upsert_device = upsert_device;
    hooks.create_classic_connection = create_classic_connection;
    hooks.push_event = push_backend_event;

    g_backend = gmbluetooth::create_platform_backend(std::move(hooks));
    if (!g_backend)
    {
        set_last_error(Error::NotSupported, "No Bluetooth backend for this platform");
        return false;
    }

    std::string message;
    const Error error = g_backend->initialize(message);
    if (error != Error::Ok)
    {
        set_last_error(error, std::move(message));
        g_backend.reset();
        return false;
    }

    g_initialized = true;
    return true;
}

void bluetooth_shutdown()
{
    if (g_backend)
    {
        g_backend->shutdown();
        g_backend.reset();
    }

    g_initialized = false;
    clear_devices();
    clear_connections();
    clear_events();

    {
        std::scoped_lock lock(g_connect_callbacks_mutex);
        g_classic_connect_callbacks.clear();
    }

    g_callback_device_found = nullptr;
    g_callback_scan_stopped = nullptr;
    g_callback_classic_client_connected = nullptr;
    g_callback_classic_data = nullptr;
    g_callback_classic_disconnected = nullptr;

    set_last_error(Error::Ok, {});
}

std::int32_t bluetooth_update()
{
    std::deque<Event> pending;
    {
        std::scoped_lock lock(g_events_mutex);
        pending.swap(g_events);
    }

    std::int32_t dispatched = 0;

    for (const Event& event : pending)
    {
        if (event.type == EventType::DeviceFound)
        {
            if (g_callback_device_found)
                g_callback_device_found.call(static_cast<double>(event.device));

            ++dispatched;
            continue;
        }

        const BackendEvent& be = event.backend;

        switch (be.type)
        {
            case BackendEventType::ScanStopped:
                if (be.error != Error::Ok)
                    set_last_error(be.error, be.message);

                if (g_callback_scan_stopped)
                {
                    // Preserve the milestone-1 callback ABI: callback(error, message).
                    g_callback_scan_stopped.call(
                        static_cast<std::int32_t>(be.error),
                        be.message);
                }
                break;

            case BackendEventType::ClassicConnected:
            {
                set_connection_connected(be.connection, be.error == Error::Ok);

                gm::wire::GMFunction callback = nullptr;
                {
                    std::scoped_lock lock(g_connect_callbacks_mutex);
                    auto it = g_classic_connect_callbacks.find(be.connection);
                    if (it != g_classic_connect_callbacks.end())
                    {
                        callback = it->second;
                        g_classic_connect_callbacks.erase(it);
                    }
                }

                if (callback)
                {
                    callback.call(
                        static_cast<std::int32_t>(be.error),
                        be.message,
                        static_cast<double>(be.connection),
                        static_cast<double>(be.device));
                }

                if (be.error != Error::Ok)
                    erase_connection(be.connection);
                break;
            }

            case BackendEventType::ClassicClientConnected:
                set_connection_connected(be.connection, true);
                if (g_callback_classic_client_connected)
                {
                    g_callback_classic_client_connected.call(
                        static_cast<double>(be.connection),
                        static_cast<double>(be.device));
                }
                break;

            case BackendEventType::ClassicDataAvailable:
                if (g_callback_classic_data)
                {
                    g_callback_classic_data.call(
                        static_cast<double>(be.connection),
                        be.value);
                }
                break;

            case BackendEventType::ClassicDisconnected:
                set_connection_connected(be.connection, false);
                if (g_callback_classic_disconnected)
                {
                    g_callback_classic_disconnected.call(
                        static_cast<double>(be.connection),
                        static_cast<std::int32_t>(be.error),
                        be.message);
                }
                erase_connection(be.connection);
                break;
        }

        ++dispatched;
    }

    return dispatched;
}

bool bluetooth_is_initialized()
{
    return g_initialized;
}

std::int32_t bluetooth_last_error_code()
{
    std::scoped_lock lock(g_error_mutex);
    return static_cast<std::int32_t>(g_last_error);
}

std::string bluetooth_last_error_message()
{
    std::scoped_lock lock(g_error_mutex);
    return g_last_error_message;
}

bool bluetooth_le_is_supported()
{
    return g_backend && g_backend->supports_ble();
}

bool bluetooth_classic_is_supported()
{
    return g_backend && g_backend->supports_classic();
}

bool bluetooth_classic_server_is_supported()
{
    return g_backend && g_backend->supports_classic_server();
}

std::int32_t bluetooth_permission_get_status()
{
    return g_backend
        ? static_cast<std::int32_t>(g_backend->permission_status())
        : static_cast<std::int32_t>(PermissionStatus::Unknown);
}

std::int32_t bluetooth_permission_request()
{
    if (!g_initialized || !g_backend)
    {
        set_last_error(Error::NotInitialized, "Bluetooth is not initialized");
        return static_cast<std::int32_t>(Error::NotInitialized);
    }

    std::string message;
    const Error error = g_backend->permission_request(message);
    set_last_error(error, std::move(message));
    return static_cast<std::int32_t>(error);
}

std::int32_t bluetooth_le_scan_start(bool active)
{
    if (!g_initialized || !g_backend)
    {
        set_last_error(Error::NotInitialized, "Bluetooth is not initialized");
        return static_cast<std::int32_t>(Error::NotInitialized);
    }

    std::string message;
    const Error error = g_backend->le_scan_start(active, message);
    set_last_error(error, std::move(message));
    return static_cast<std::int32_t>(error);
}

std::int32_t bluetooth_le_scan_stop()
{
    if (!g_initialized || !g_backend)
        return static_cast<std::int32_t>(Error::NotInitialized);

    std::string message;
    const Error error = g_backend->le_scan_stop(message);
    set_last_error(error, std::move(message));
    return static_cast<std::int32_t>(error);
}

bool bluetooth_le_scan_is_running()
{
    return g_initialized && g_backend && g_backend->le_scan_is_running();
}

std::int32_t bluetooth_classic_scan_start()
{
    if (!g_initialized || !g_backend)
    {
        set_last_error(Error::NotInitialized, "Bluetooth is not initialized");
        return static_cast<std::int32_t>(Error::NotInitialized);
    }

    std::string message;
    const Error error = g_backend->classic_scan_start(message);
    set_last_error(error, std::move(message));
    return static_cast<std::int32_t>(error);
}

std::int32_t bluetooth_classic_scan_stop()
{
    if (!g_initialized || !g_backend)
        return static_cast<std::int32_t>(Error::NotInitialized);

    std::string message;
    const Error error = g_backend->classic_scan_stop(message);
    set_last_error(error, std::move(message));
    return static_cast<std::int32_t>(error);
}

bool bluetooth_classic_scan_is_running()
{
    return g_initialized && g_backend && g_backend->classic_scan_is_running();
}

void bluetooth_device_clear()
{
    clear_devices();
}

std::int32_t bluetooth_device_get_count()
{
    std::scoped_lock lock(g_devices_mutex);
    return static_cast<std::int32_t>(g_device_order.size());
}

std::uint64_t bluetooth_device_get_at(std::int32_t index)
{
    if (index < 0)
        return 0;

    std::scoped_lock lock(g_devices_mutex);
    const auto i = static_cast<std::size_t>(index);
    return i < g_device_order.size() ? g_device_order[i] : 0;
}

bool bluetooth_device_is_valid(std::uint64_t device)
{
    DeviceEntry entry;
    return copy_device(device, entry);
}

std::int32_t bluetooth_device_get_transport(std::uint64_t device)
{
    DeviceEntry entry;
    return copy_device(device, entry)
        ? static_cast<std::int32_t>(entry.transport)
        : static_cast<std::int32_t>(Transport::Unknown);
}

std::string bluetooth_device_get_id(std::uint64_t device)
{
    DeviceEntry entry;
    return copy_device(device, entry) ? entry.id : std::string{};
}

std::string bluetooth_device_get_name(std::uint64_t device)
{
    DeviceEntry entry;
    return copy_device(device, entry) ? entry.name : std::string{};
}

bool bluetooth_device_has_address(std::uint64_t device)
{
    DeviceEntry entry;
    return copy_device(device, entry) && entry.address_available;
}

std::string bluetooth_device_get_address(std::uint64_t device)
{
    DeviceEntry entry;
    return copy_device(device, entry) && entry.address_available
        ? entry.address
        : std::string{};
}

bool bluetooth_device_has_rssi(std::uint64_t device)
{
    DeviceEntry entry;
    return copy_device(device, entry) && entry.rssi_available;
}

std::int32_t bluetooth_device_get_rssi(std::uint64_t device)
{
    DeviceEntry entry;
    return copy_device(device, entry) && entry.rssi_available ? entry.rssi : 0;
}

bool bluetooth_device_is_connectable(std::uint64_t device)
{
    DeviceEntry entry;
    return copy_device(device, entry) && entry.connectable;
}

std::uint64_t bluetooth_classic_connect(
    std::uint64_t device,
    std::string_view service_uuid,
    const gm::wire::GMFunction& callback)
{
    if (!g_initialized || !g_backend)
    {
        set_last_error(Error::NotInitialized, "Bluetooth is not initialized");
        return 0;
    }

    DeviceEntry entry;
    if (!copy_device(device, entry) || entry.transport != Transport::Classic)
    {
        set_last_error(Error::InvalidHandle, "Expected a Bluetooth Classic device handle");
        return 0;
    }

    if (service_uuid.empty())
    {
        set_last_error(Error::InvalidArgument, "service_uuid cannot be empty");
        return 0;
    }

    const std::uint64_t connection = create_classic_connection(device);
    {
        std::scoped_lock lock(g_connect_callbacks_mutex);
        g_classic_connect_callbacks[connection] = callback;
    }

    DiscoveredDevice backend_device;
    backend_device.transport = entry.transport;
    backend_device.id = entry.id;
    backend_device.name = entry.name;
    backend_device.address = entry.address;
    backend_device.address_available = entry.address_available;
    backend_device.connectable = entry.connectable;
    backend_device.rssi = entry.rssi;
    backend_device.rssi_available = entry.rssi_available;

    std::string message;
    const Error error = g_backend->classic_connect(
        connection,
        backend_device,
        std::string(service_uuid),
        message);

    set_last_error(error, message);

    if (error != Error::Ok)
    {
        BackendEvent event;
        event.type = BackendEventType::ClassicConnected;
        event.transport = Transport::Classic;
        event.connection = connection;
        event.device = device;
        event.error = error;
        event.message = std::move(message);
        push_backend_event(std::move(event));
    }

    return connection;
}

std::int32_t bluetooth_classic_disconnect(std::uint64_t connection)
{
    ClassicConnectionEntry entry;
    if (!copy_classic_connection(connection, entry))
        return static_cast<std::int32_t>(Error::InvalidHandle);

    if (!g_backend)
        return static_cast<std::int32_t>(Error::NotInitialized);

    std::string message;
    const Error error = g_backend->classic_disconnect(connection, message);
    set_last_error(error, std::move(message));
    return static_cast<std::int32_t>(error);
}

bool bluetooth_classic_connection_is_valid(std::uint64_t connection)
{
    ClassicConnectionEntry entry;
    return copy_classic_connection(connection, entry);
}

bool bluetooth_classic_connection_is_connected(std::uint64_t connection)
{
    ClassicConnectionEntry entry;
    return copy_classic_connection(connection, entry) &&
           g_backend &&
           g_backend->classic_connection_is_connected(connection);
}

std::uint64_t bluetooth_classic_connection_get_device(std::uint64_t connection)
{
    ClassicConnectionEntry entry;
    return copy_classic_connection(connection, entry) ? entry.device : 0;
}

std::int32_t bluetooth_classic_receive_available(std::uint64_t connection)
{
    if (!g_backend)
        return 0;
    return g_backend->classic_receive_available(connection);
}

std::int32_t bluetooth_classic_send(
    std::uint64_t connection,
    gm::wire::GMBuffer data,
    std::uint32_t offset,
    std::uint32_t size)
{
    (void)connection;
    (void)data;
    (void)offset;
    (void)size;

    // TODO: Implement GameMaker GMBuffer -> native byte transfer.
    // The platform RFCOMM transports already expose classic_send_bytes();
    // only the project-specific GMBuffer extraction convention is intentionally
    // deferred until supplied by the user.
    set_last_error(Error::NotSupported, "TODO: GameMaker buffer send bridge not implemented yet");
    return static_cast<std::int32_t>(Error::NotSupported);
}

std::int32_t bluetooth_classic_receive(
    std::uint64_t connection,
    gm::wire::GMBuffer out_data,
    std::uint32_t offset,
    std::uint32_t max_size)
{
    (void)connection;
    (void)out_data;
    (void)offset;
    (void)max_size;

    // TODO: Implement native byte queue -> GameMaker GMBuffer transfer.
    // Platform receive loops and native byte queues are already implemented;
    // only the final GMBuffer write convention is intentionally deferred.
    set_last_error(Error::NotSupported, "TODO: GameMaker buffer receive bridge not implemented yet");
    return 0;
}

std::int32_t bluetooth_classic_server_start(
    std::string_view name,
    std::string_view service_uuid)
{
    if (!g_initialized || !g_backend)
        return static_cast<std::int32_t>(Error::NotInitialized);

    std::string message;
    const Error error = g_backend->classic_server_start(
        std::string(name),
        std::string(service_uuid),
        message);
    set_last_error(error, std::move(message));
    return static_cast<std::int32_t>(error);
}

std::int32_t bluetooth_classic_server_stop()
{
    if (!g_initialized || !g_backend)
        return static_cast<std::int32_t>(Error::NotInitialized);

    std::string message;
    const Error error = g_backend->classic_server_stop(message);
    set_last_error(error, std::move(message));
    return static_cast<std::int32_t>(error);
}

bool bluetooth_classic_server_is_running()
{
    return g_initialized && g_backend && g_backend->classic_server_is_running();
}

bool bluetooth_set_callback_device_found(const gm::wire::GMFunction& callback)
{
    g_callback_device_found = callback;
    return true;
}

bool bluetooth_remove_callback_device_found()
{
    g_callback_device_found = nullptr;
    return true;
}

bool bluetooth_set_callback_scan_stopped(const gm::wire::GMFunction& callback)
{
    g_callback_scan_stopped = callback;
    return true;
}

bool bluetooth_remove_callback_scan_stopped()
{
    g_callback_scan_stopped = nullptr;
    return true;
}

bool bluetooth_set_callback_classic_client_connected(const gm::wire::GMFunction& callback)
{
    g_callback_classic_client_connected = callback;
    return true;
}

bool bluetooth_remove_callback_classic_client_connected()
{
    g_callback_classic_client_connected = nullptr;
    return true;
}

bool bluetooth_set_callback_classic_data(const gm::wire::GMFunction& callback)
{
    g_callback_classic_data = callback;
    return true;
}

bool bluetooth_remove_callback_classic_data()
{
    g_callback_classic_data = nullptr;
    return true;
}

bool bluetooth_set_callback_classic_disconnected(const gm::wire::GMFunction& callback)
{
    g_callback_classic_disconnected = callback;
    return true;
}

bool bluetooth_remove_callback_classic_disconnected()
{
    g_callback_classic_disconnected = nullptr;
    return true;
}
