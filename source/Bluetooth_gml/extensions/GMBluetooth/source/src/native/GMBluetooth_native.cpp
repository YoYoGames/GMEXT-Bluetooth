#include "GMBluetooth_native.h"
#include "GMBluetooth_backend.h"
#include "GMBluetooth_log.h"

#include <atomic>

using namespace gm::wire;
using namespace gm_structs;
using namespace gm_enums;
using namespace gmbluetooth;

namespace
{
    std::unique_ptr<Backend> g_backend;
    std::string g_last_error_message;
    Error g_last_error = Error::Ok;

    // Callback storage
    std::mutex g_callback_mutex;
    GMFunction g_callback_device_found;
    GMFunction g_callback_scan_stopped;
    GMFunction g_callback_classic_client_connected;
    GMFunction g_callback_classic_data;
    GMFunction g_callback_classic_disconnected;

    // Connect callbacks are one-shot and tied to a specific connection handle,
    // not a persistently-registered callback like the others.
    std::mutex g_pending_connect_mutex;
    std::unordered_map<std::uint64_t, GMFunction> g_pending_connect_callbacks;

    // Device found event queue (separate because it needs device handle)
    struct DeviceFoundEvent
    {
        std::uint64_t device_handle;
    };
    std::mutex g_device_found_queue_mutex;
    std::vector<DeviceFoundEvent> g_device_found_queue;

    class DeviceManager
    {
    public:
        std::uint64_t upsert_device(const DiscoveredDevice& device)
        {
            std::scoped_lock lock(mutex_);
            const auto it = std::find_if(devices_.begin(), devices_.end(),
                [&device](const auto& d) { return d.id == device.id; });

            if (it != devices_.end())
            {
                *it = device;
                return std::distance(devices_.begin(), it) + 1;
            }

            devices_.push_back(device);
            return devices_.size();
        }

        void clear()
        {
            std::scoped_lock lock(mutex_);
            devices_.clear();
        }

        int get_count() const
        {
            std::scoped_lock lock(mutex_);
            return static_cast<int>(devices_.size());
        }

        std::uint64_t get_at(int index) const
        {
            std::scoped_lock lock(mutex_);
            if (index < 0 || index >= static_cast<int>(devices_.size()))
                return 0;
            return static_cast<std::uint64_t>(index) + 1;
        }

        bool is_valid(std::uint64_t handle) const
        {
            std::scoped_lock lock(mutex_);
            return handle > 0 && handle <= static_cast<std::uint64_t>(devices_.size());
        }

        const DiscoveredDevice* get_device(std::uint64_t handle) const
        {
            std::scoped_lock lock(mutex_);
            if (handle <= 0 || handle > static_cast<std::uint64_t>(devices_.size()))
                return nullptr;
            return &devices_[handle - 1];
        }

    private:
        mutable std::mutex mutex_;
        std::vector<DiscoveredDevice> devices_;
    };

    DeviceManager g_device_manager;

    // Event queue for callback dispatch
    std::mutex g_event_queue_mutex;
    std::vector<BackendEvent> g_event_queue;

    class ClassicConnectionManager
    {
    public:
        std::uint64_t create_connection(std::uint64_t device)
        {
            std::scoped_lock lock(mutex_);
            const std::uint64_t handle = next_handle_++;
            connections_[handle] = device;
            return handle;
        }

        void remove_connection(std::uint64_t handle)
        {
            std::scoped_lock lock(mutex_);
            connections_.erase(handle);
        }

        bool is_valid(std::uint64_t handle) const
        {
            std::scoped_lock lock(mutex_);
            return connections_.find(handle) != connections_.end();
        }

        std::uint64_t get_device(std::uint64_t handle) const
        {
            std::scoped_lock lock(mutex_);
            const auto it = connections_.find(handle);
            return it != connections_.end() ? it->second : 0;
        }

    private:
        mutable std::mutex mutex_;
        std::unordered_map<std::uint64_t, std::uint64_t> connections_;
        std::uint64_t next_handle_ = 1;
    };

    ClassicConnectionManager g_classic_connection_manager;

    // Counts every event the backend hands up that the core then drops, so the
    // gap between "the backend is working" and "GML sees nothing" is visible in
    // the log instead of having to be inferred.
    std::atomic<std::uint64_t> g_dropped_events{ 0 };

    CoreHooks create_core_hooks()
    {
        CoreHooks hooks;
        hooks.upsert_device = [](const DiscoveredDevice& device) {
            const std::uint64_t handle = g_device_manager.upsert_device(device);
            GMBT_LOG("device upserted: handle=%llu transport=%d id='%s' name='%s' rssi=%d (available=%d) connectable=%d",
                static_cast<unsigned long long>(handle),
                static_cast<int>(device.transport),
                device.id.c_str(),
                device.name.c_str(),
                device.rssi,
                device.rssi_available ? 1 : 0,
                device.connectable ? 1 : 0);

            // Queue device_found event for callback dispatch
            {
                std::scoped_lock lock(g_device_found_queue_mutex);
                g_device_found_queue.push_back(DeviceFoundEvent{handle});
            }

            return handle;
        };
        hooks.create_classic_connection = [](std::uint64_t device) {
            const std::uint64_t connection = g_classic_connection_manager.create_connection(device);
            GMBT_LOG("classic connection created: connection=%llu device=%llu",
                static_cast<unsigned long long>(connection),
                static_cast<unsigned long long>(device));
            return connection;
        };
        hooks.push_event = [](BackendEvent event) {
            {
                std::scoped_lock lock(g_event_queue_mutex);
                g_event_queue.push_back(event);
            }
            GMBT_LOG("event QUEUED: type=%d transport=%d event_type='%s' queue_size=%zu",
                static_cast<int>(event.type),
                static_cast<int>(event.transport),
                event.event_type.c_str(),
                g_event_queue.size());
        };
        return hooks;
    }
}

bool bluetooth_initialize()
{
    if (g_backend)
    {
        GMBT_LOG("already initialized, nothing to do");
        return true;
    }

    GMBT_LOG("creating platform backend...");
    g_backend = create_platform_backend(create_core_hooks());
    if (!g_backend)
    {
        g_last_error_message = "Failed to create platform backend";
        g_last_error = Error::OperationFailed;
        GMBT_LOG("FAILED: no platform backend for this build");
        return false;
    }

    std::string message;
    const Error error = g_backend->initialize(message);
    g_last_error = error;
    g_last_error_message = message;

    GMBT_LOG("backend initialize() -> error=%d message='%s' | ble=%d classic=%d classic_server=%d",
        static_cast<int>(error),
        message.c_str(),
        g_backend->supports_ble() ? 1 : 0,
        g_backend->supports_classic() ? 1 : 0,
        g_backend->supports_classic_server() ? 1 : 0);

    return error == Error::Ok;
}

void bluetooth_shutdown()
{
    GMBT_LOG("shutting down (backend=%s, devices cached=%d, events dropped=%llu)",
        g_backend ? "present" : "null",
        g_device_manager.get_count(),
        static_cast<unsigned long long>(g_dropped_events.load()));

    if (g_backend)
    {
        g_backend->shutdown();
        g_backend.reset();
    }
    g_device_manager.clear();
}

std::int32_t bluetooth_update()
{
    int dispatched_count = 0;

    // Dispatch device_found events
    {
        std::vector<DeviceFoundEvent> devices_to_dispatch;
        {
            std::scoped_lock lock(g_device_found_queue_mutex);
            devices_to_dispatch = std::move(g_device_found_queue);
            g_device_found_queue.clear();
        }

        GMFunction callback;
        {
            std::scoped_lock lock(g_callback_mutex);
            callback = g_callback_device_found;
        }

        if (callback)
        {
            for (const auto& device_event : devices_to_dispatch)
            {
                try
                {
                    callback.call(device_event.device_handle);
                    ++dispatched_count;
                }
                catch (const std::exception& e)
                {
                    GMBT_LOG("Error dispatching device_found callback: %s", e.what());
                }
            }
        }
    }

    // Dispatch other backend events
    {
        std::vector<BackendEvent> events_to_dispatch;
        {
            std::scoped_lock lock(g_event_queue_mutex);
            events_to_dispatch = std::move(g_event_queue);
            g_event_queue.clear();
        }

        for (const auto& event : events_to_dispatch)
        {
            // The connect completion callback is one-shot and per-connection,
            // not one of the persistently-registered callbacks below.
            if (event.type == BackendEventType::ClassicConnected)
            {
                GMFunction callback;
                {
                    std::scoped_lock lock(g_pending_connect_mutex);
                    const auto it = g_pending_connect_callbacks.find(event.connection);
                    if (it != g_pending_connect_callbacks.end())
                    {
                        callback = it->second;
                        g_pending_connect_callbacks.erase(it);
                    }
                }

                if (callback)
                {
                    try
                    {
                        // callback(error_code, message, connection, device)
                        callback.call(
                            static_cast<double>(event.error),
                            event.message,
                            static_cast<double>(event.connection),
                            static_cast<double>(event.device)
                        );
                        ++dispatched_count;
                    }
                    catch (const std::exception& e)
                    {
                        GMBT_LOG("Error dispatching classic_connect callback: %s", e.what());
                    }
                }
                continue;
            }

            GMFunction callback;

            // Select callback based on event type
            {
                std::scoped_lock lock(g_callback_mutex);
                if (event.type == BackendEventType::ScanStopped && g_callback_scan_stopped)
                {
                    callback = g_callback_scan_stopped;
                }
                else if (event.type == BackendEventType::ClassicDataAvailable && g_callback_classic_data)
                {
                    callback = g_callback_classic_data;
                }
                else if (event.type == BackendEventType::ClassicClientConnected && g_callback_classic_client_connected)
                {
                    callback = g_callback_classic_client_connected;
                }
                else if (event.type == BackendEventType::ClassicDisconnected && g_callback_classic_disconnected)
                {
                    callback = g_callback_classic_disconnected;
                }
            }

            // Dispatch callback if found, using the signature spec.gmidl documents
            // for this event type — these differ per callback, they are not
            // interchangeable.
            if (callback)
            {
                try
                {
                    switch (event.type)
                    {
                    case BackendEventType::ScanStopped:
                        // callback(error_code, message)
                        callback.call(static_cast<double>(event.error), event.message);
                        break;
                    case BackendEventType::ClassicDataAvailable:
                        // callback(connection, available_bytes)
                        callback.call(static_cast<double>(event.connection), static_cast<double>(event.value));
                        break;
                    case BackendEventType::ClassicClientConnected:
                        // callback(connection, device)
                        callback.call(static_cast<double>(event.connection), static_cast<double>(event.device));
                        break;
                    case BackendEventType::ClassicDisconnected:
                        // callback(connection, error_code, message)
                        callback.call(static_cast<double>(event.connection), static_cast<double>(event.error), event.message);
                        break;
                    default:
                        break;
                    }
                    ++dispatched_count;
                }
                catch (const std::exception& e)
                {
                    GMBT_LOG("Error dispatching callback: %s", e.what());
                }
            }
        }
    }

    return dispatched_count;
}

bool bluetooth_is_initialized()
{
    return g_backend != nullptr;
}

std::int32_t bluetooth_last_error_code()
{
    return static_cast<std::int32_t>(g_last_error);
}

std::string bluetooth_last_error_message()
{
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
    const std::int32_t status = g_backend
        ? static_cast<std::int32_t>(g_backend->permission_status())
        : static_cast<std::int32_t>(PermissionStatus::Unknown);

    // Polled from Step and Draw every frame - only speak up on a transition.
    static std::int32_t last_status = -1;
    if (status != last_status)
    {
        GMBT_LOG("permission status changed: %d -> %d (0=Unknown 1=Granted 2=Denied)", last_status, status);
        last_status = status;
    }

    return status;
}

std::int32_t bluetooth_permission_request()
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        GMBT_LOG("permission request rejected: backend is not initialized");
        return static_cast<std::int32_t>(Error::NotInitialized);
    }

    std::string message;
    const Error error = g_backend->permission_request(message);
    g_last_error = error;
    g_last_error_message = message;
    GMBT_LOG("permission request -> error=%d message='%s' status now %d",
        static_cast<int>(error),
        message.c_str(),
        static_cast<int>(g_backend->permission_status()));
    return static_cast<std::int32_t>(error);
}

std::int32_t bluetooth_le_scan_start(bool active)
{
    GMBT_LOG("BLE scan start requested (active=%d)", active ? 1 : 0);

    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        GMBT_LOG("BLE scan start rejected: backend is not initialized");
        return static_cast<std::int32_t>(Error::NotInitialized);
    }

    std::string message;
    const Error error = g_backend->le_scan_start(active, message);
    g_last_error = error;
    g_last_error_message = message;
    GMBT_LOG("BLE scan start -> error=%d message='%s' | backend reports running=%d",
        static_cast<int>(error),
        message.c_str(),
        g_backend->le_scan_is_running() ? 1 : 0);
    return static_cast<std::int32_t>(error);
}

std::int32_t bluetooth_le_scan_stop()
{
    GMBT_LOG("BLE scan stop requested");

    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        GMBT_LOG("BLE scan stop rejected: backend is not initialized");
        return static_cast<std::int32_t>(Error::NotInitialized);
    }

    std::string message;
    const Error error = g_backend->le_scan_stop(message);
    g_last_error = error;
    g_last_error_message = message;
    GMBT_LOG("BLE scan stop -> error=%d message='%s' | backend reports running=%d",
        static_cast<int>(error),
        message.c_str(),
        g_backend->le_scan_is_running() ? 1 : 0);
    return static_cast<std::int32_t>(error);
}

bool bluetooth_le_scan_is_running()
{
    const bool running = g_backend && g_backend->le_scan_is_running();

    // Polled from Step and Draw every frame - only speak up on a transition.
    static int last_running = -1;
    if (static_cast<int>(running) != last_running)
    {
        GMBT_LOG("BLE scan running state changed: %d -> %d", last_running, running ? 1 : 0);
        last_running = static_cast<int>(running);
    }

    return running;
}

std::int32_t bluetooth_classic_scan_start()
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return static_cast<std::int32_t>(Error::NotInitialized);
    }

    std::string message;
    const Error error = g_backend->classic_scan_start(message);
    g_last_error = error;
    g_last_error_message = message;
    return static_cast<std::int32_t>(error);
}

std::int32_t bluetooth_classic_scan_stop()
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return static_cast<std::int32_t>(Error::NotInitialized);
    }

    std::string message;
    const Error error = g_backend->classic_scan_stop(message);
    g_last_error = error;
    g_last_error_message = message;
    return static_cast<std::int32_t>(error);
}

bool bluetooth_classic_scan_is_running()
{
    return g_backend && g_backend->classic_scan_is_running();
}

void bluetooth_device_clear()
{
    GMBT_LOG("clearing device cache (%d cached)", g_device_manager.get_count());
    g_device_manager.clear();
}

int bluetooth_device_get_count()
{
    return g_device_manager.get_count();
}

std::uint64_t bluetooth_device_get_at(int index)
{
    return g_device_manager.get_at(index);
}

bool bluetooth_device_is_valid(std::uint64_t device)
{
    return g_device_manager.is_valid(device);
}

int bluetooth_device_get_transport(std::uint64_t device)
{
    const auto* dev = g_device_manager.get_device(device);
    return dev ? static_cast<int>(dev->transport) : static_cast<int>(Transport::Unknown);
}

std::string bluetooth_device_get_id(std::uint64_t device)
{
    const auto* dev = g_device_manager.get_device(device);
    return dev ? dev->id : std::string();
}

std::string bluetooth_device_get_name(std::uint64_t device)
{
    const auto* dev = g_device_manager.get_device(device);
    return dev ? dev->name : std::string();
}

bool bluetooth_device_has_address(std::uint64_t device)
{
    const auto* dev = g_device_manager.get_device(device);
    return dev ? dev->address_available : false;
}

std::string bluetooth_device_get_address(std::uint64_t device)
{
    const auto* dev = g_device_manager.get_device(device);
    return dev ? dev->address : std::string();
}

bool bluetooth_device_has_rssi(std::uint64_t device)
{
    const auto* dev = g_device_manager.get_device(device);
    return dev ? dev->rssi_available : false;
}

int bluetooth_device_get_rssi(std::uint64_t device)
{
    const auto* dev = g_device_manager.get_device(device);
    return dev ? dev->rssi : 0;
}

bool bluetooth_device_is_connectable(std::uint64_t device)
{
    const auto* dev = g_device_manager.get_device(device);
    return dev ? dev->connectable : false;
}

std::uint64_t bluetooth_classic_connect(std::uint64_t device, std::string_view service_uuid, const gm::wire::GMFunction& callback)
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return 0;
    }

    const auto* dev = g_device_manager.get_device(device);
    if (!dev)
    {
        g_last_error = Error::InvalidArgument;
        g_last_error_message = "Invalid device handle";
        return 0;
    }

    const std::uint64_t connection = g_classic_connection_manager.create_connection(device);

    // Registered before calling the backend: connect runs asynchronously and may
    // push its ClassicConnected completion event before this call even returns.
    if (callback)
    {
        std::scoped_lock lock(g_pending_connect_mutex);
        g_pending_connect_callbacks[connection] = callback;
    }

    std::string message;
    const Error error = g_backend->classic_connect(connection, *dev, std::string(service_uuid), message);
    g_last_error = error;
    g_last_error_message = message;

    if (error != Error::Ok)
    {
        g_classic_connection_manager.remove_connection(connection);
        {
            std::scoped_lock lock(g_pending_connect_mutex);
            g_pending_connect_callbacks.erase(connection);
        }
        return 0;
    }

    return connection;
}

std::int32_t bluetooth_classic_disconnect(std::uint64_t connection)
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return static_cast<std::int32_t>(Error::NotInitialized);
    }

    std::string message;
    const Error error = g_backend->classic_disconnect(connection, message);
    g_last_error = error;
    g_last_error_message = message;
    return static_cast<std::int32_t>(error);
}

bool bluetooth_classic_connection_is_valid(std::uint64_t connection)
{
    return g_classic_connection_manager.is_valid(connection);
}

bool bluetooth_classic_connection_is_connected(std::uint64_t connection)
{
    return g_backend && g_backend->classic_connection_is_connected(connection);
}

std::uint64_t bluetooth_classic_connection_get_device(std::uint64_t connection)
{
    return g_classic_connection_manager.get_device(connection);
}

std::int32_t bluetooth_classic_receive_available(std::uint64_t connection)
{
    return g_backend ? g_backend->classic_receive_available(connection) : 0;
}

std::int32_t bluetooth_classic_send(std::uint64_t connection, struct gm::wire::GMBuffer data, unsigned int offset, unsigned int size)
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return static_cast<std::int32_t>(Error::NotInitialized);
    }

    const std::uint8_t* buffer = static_cast<const std::uint8_t*>(data.data()) + offset;
    std::string message;
    const Error error = g_backend->classic_send_bytes(connection, buffer, size, message);
    g_last_error = error;
    g_last_error_message = message;
    return static_cast<std::int32_t>(error);
}

std::int32_t bluetooth_classic_receive(std::uint64_t connection, struct gm::wire::GMBuffer data, unsigned int offset, unsigned int max_size)
{
    if (!g_backend)
        return 0;

    std::uint8_t* buffer = static_cast<std::uint8_t*>(data.data()) + offset;
    const std::size_t received = g_backend->classic_receive_bytes(connection, buffer, max_size);
    return static_cast<std::int32_t>(received);
}

std::int32_t bluetooth_classic_server_start(std::string_view name, std::string_view service_uuid)
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return static_cast<std::int32_t>(Error::NotInitialized);
    }

    std::string message;
    const Error error = g_backend->classic_server_start(std::string(name), std::string(service_uuid), message);
    g_last_error = error;
    g_last_error_message = message;
    return static_cast<std::int32_t>(error);
}

std::int32_t bluetooth_classic_server_stop()
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return static_cast<std::int32_t>(Error::NotInitialized);
    }

    std::string message;
    const Error error = g_backend->classic_server_stop(message);
    g_last_error = error;
    g_last_error_message = message;
    return static_cast<std::int32_t>(error);
}

bool bluetooth_classic_server_is_running()
{
    return g_backend && g_backend->classic_server_is_running();
}

// Callback registration functions
bool bluetooth_set_callback_device_found(const gm::wire::GMFunction& callback)
{
    std::scoped_lock lock(g_callback_mutex);
    g_callback_device_found = callback;
    GMBT_LOG("Device found callback registered");
    return true;
}

bool bluetooth_remove_callback_device_found()
{
    std::scoped_lock lock(g_callback_mutex);
    g_callback_device_found = GMFunction();
    GMBT_LOG("Device found callback removed");
    return true;
}

bool bluetooth_set_callback_scan_stopped(const gm::wire::GMFunction& callback)
{
    std::scoped_lock lock(g_callback_mutex);
    g_callback_scan_stopped = callback;
    GMBT_LOG("Scan stopped callback registered");
    return true;
}

bool bluetooth_remove_callback_scan_stopped()
{
    std::scoped_lock lock(g_callback_mutex);
    g_callback_scan_stopped = GMFunction();
    GMBT_LOG("Scan stopped callback removed");
    return true;
}

bool bluetooth_set_callback_classic_client_connected(const gm::wire::GMFunction& callback)
{
    std::scoped_lock lock(g_callback_mutex);
    g_callback_classic_client_connected = callback;
    GMBT_LOG("Classic client connected callback registered");
    return true;
}

bool bluetooth_remove_callback_classic_client_connected()
{
    std::scoped_lock lock(g_callback_mutex);
    g_callback_classic_client_connected = GMFunction();
    GMBT_LOG("Classic client connected callback removed");
    return true;
}

bool bluetooth_set_callback_classic_data(const gm::wire::GMFunction& callback)
{
    std::scoped_lock lock(g_callback_mutex);
    g_callback_classic_data = callback;
    GMBT_LOG("Classic data callback registered");
    return true;
}

bool bluetooth_remove_callback_classic_data()
{
    std::scoped_lock lock(g_callback_mutex);
    g_callback_classic_data = GMFunction();
    GMBT_LOG("Classic data callback removed");
    return true;
}

bool bluetooth_set_callback_classic_disconnected(const gm::wire::GMFunction& callback)
{
    std::scoped_lock lock(g_callback_mutex);
    g_callback_classic_disconnected = callback;
    GMBT_LOG("Classic disconnected callback registered");
    return true;
}

bool bluetooth_remove_callback_classic_disconnected()
{
    std::scoped_lock lock(g_callback_mutex);
    g_callback_classic_disconnected = GMFunction();
    GMBT_LOG("Classic disconnected callback removed");
    return true;
}
