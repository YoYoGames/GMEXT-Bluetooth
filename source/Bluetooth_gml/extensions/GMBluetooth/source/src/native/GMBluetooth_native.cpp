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
            const std::uint64_t dropped = g_dropped_events.fetch_add(1) + 1;
            // TODO: Queue events for GML callback dispatch
            GMBT_LOG("event DROPPED (no dispatch queue implemented): type=%d transport=%d event_type='%s' json=%s [dropped so far: %llu]",
                static_cast<int>(event.type),
                static_cast<int>(event.transport),
                event.event_type.c_str(),
                event.json.c_str(),
                static_cast<unsigned long long>(dropped));
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
    // TODO: Process queued events
    //
    // Called every frame, so this cannot log unconditionally. Report the
    // missing dispatch once, then stay quiet: push_event already logs each
    // individual event it drops.
    static bool warned = false;
    if (!warned)
    {
        warned = true;
        GMBT_LOG("WARNING: event dispatch is not implemented - bluetooth_update() will always report 0 "
                 "and no GML callback will ever fire. This message is logged once.");
    }

    return 0;
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

std::uint64_t bluetooth_classic_connect(std::uint64_t device, std::string_view service_uuid, const gm::wire::GMFunction&)
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
    std::string message;
    const Error error = g_backend->classic_connect(connection, *dev, std::string(service_uuid), message);
    g_last_error = error;
    g_last_error_message = message;

    if (error != Error::Ok)
    {
        g_classic_connection_manager.remove_connection(connection);
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

// Every setter below reports success to GML while discarding the GMFunction,
// so a caller has no way to tell registration apart from a callback that simply
// never fires. Log loudly until the dispatch queue exists.
#define GMBT_LOG_CALLBACK_STUB(what) \
    GMBT_LOG("STUB: %s - accepted and DISCARDED, this callback will never fire", what)

bool bluetooth_set_callback_device_found(const gm::wire::GMFunction&)
{
    // TODO: Store callback for device found events
    GMBT_LOG_CALLBACK_STUB("set_callback_device_found");
    return true;
}

bool bluetooth_remove_callback_device_found()
{
    // TODO: Remove device found callback
    GMBT_LOG_CALLBACK_STUB("remove_callback_device_found");
    return true;
}

bool bluetooth_set_callback_scan_stopped(const gm::wire::GMFunction&)
{
    // TODO: Store callback for scan stopped events
    GMBT_LOG_CALLBACK_STUB("set_callback_scan_stopped");
    return true;
}

bool bluetooth_remove_callback_scan_stopped()
{
    // TODO: Remove scan stopped callback
    GMBT_LOG_CALLBACK_STUB("remove_callback_scan_stopped");
    return true;
}

bool bluetooth_set_callback_classic_client_connected(const gm::wire::GMFunction&)
{
    // TODO: Store callback for classic client connected events
    GMBT_LOG_CALLBACK_STUB("set_callback_classic_client_connected");
    return true;
}

bool bluetooth_remove_callback_classic_client_connected()
{
    // TODO: Remove classic client connected callback
    GMBT_LOG_CALLBACK_STUB("remove_callback_classic_client_connected");
    return true;
}

bool bluetooth_set_callback_classic_data(const gm::wire::GMFunction&)
{
    // TODO: Store callback for classic data available events
    GMBT_LOG_CALLBACK_STUB("set_callback_classic_data");
    return true;
}

bool bluetooth_remove_callback_classic_data()
{
    // TODO: Remove classic data callback
    GMBT_LOG_CALLBACK_STUB("remove_callback_classic_data");
    return true;
}

bool bluetooth_set_callback_classic_disconnected(const gm::wire::GMFunction&)
{
    // TODO: Store callback for classic disconnected events
    GMBT_LOG_CALLBACK_STUB("set_callback_classic_disconnected");
    return true;
}

bool bluetooth_remove_callback_classic_disconnected()
{
    // TODO: Remove classic disconnected callback
    GMBT_LOG_CALLBACK_STUB("remove_callback_classic_disconnected");
    return true;
}

#undef GMBT_LOG_CALLBACK_STUB
