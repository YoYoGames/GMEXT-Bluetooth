#include "GMBluetooth_native.h"
#include "GMBluetooth_backend.h"

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

    CoreHooks create_core_hooks()
    {
        CoreHooks hooks;
        hooks.upsert_device = [](const DiscoveredDevice& device) {
            return g_device_manager.upsert_device(device);
        };
        hooks.create_classic_connection = [](std::uint64_t device) {
            return g_classic_connection_manager.create_connection(device);
        };
        hooks.push_event = [](BackendEvent) {
            // TODO: Queue events for GML callback dispatch
        };
        return hooks;
    }
}

bool bluetooth_initialize()
{
    if (g_backend)
        return true;

    g_backend = create_platform_backend(create_core_hooks());
    if (!g_backend)
    {
        g_last_error_message = "Failed to create platform backend";
        g_last_error = Error::OperationFailed;
        return false;
    }

    std::string message;
    const Error error = g_backend->initialize(message);
    g_last_error = error;
    g_last_error_message = message;
    return error == Error::Ok;
}

void bluetooth_shutdown()
{
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
    if (!g_backend)
        return static_cast<std::int32_t>(PermissionStatus::Unknown);
    return static_cast<std::int32_t>(g_backend->permission_status());
}

std::int32_t bluetooth_permission_request()
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return static_cast<std::int32_t>(Error::NotInitialized);
    }

    std::string message;
    const Error error = g_backend->permission_request(message);
    g_last_error = error;
    g_last_error_message = message;
    return static_cast<std::int32_t>(error);
}

std::int32_t bluetooth_le_scan_start(bool active)
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return static_cast<std::int32_t>(Error::NotInitialized);
    }

    std::string message;
    const Error error = g_backend->le_scan_start(active, message);
    g_last_error = error;
    g_last_error_message = message;
    return static_cast<std::int32_t>(error);
}

std::int32_t bluetooth_le_scan_stop()
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return static_cast<std::int32_t>(Error::NotInitialized);
    }

    std::string message;
    const Error error = g_backend->le_scan_stop(message);
    g_last_error = error;
    g_last_error_message = message;
    return static_cast<std::int32_t>(error);
}

bool bluetooth_le_scan_is_running()
{
    return g_backend && g_backend->le_scan_is_running();
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

bool bluetooth_set_callback_device_found(const gm::wire::GMFunction&)
{
    // TODO: Store callback for device found events
    return true;
}

bool bluetooth_remove_callback_device_found()
{
    // TODO: Remove device found callback
    return true;
}

bool bluetooth_set_callback_scan_stopped(const gm::wire::GMFunction&)
{
    // TODO: Store callback for scan stopped events
    return true;
}

bool bluetooth_remove_callback_scan_stopped()
{
    // TODO: Remove scan stopped callback
    return true;
}

bool bluetooth_set_callback_classic_client_connected(const gm::wire::GMFunction&)
{
    // TODO: Store callback for classic client connected events
    return true;
}

bool bluetooth_remove_callback_classic_client_connected()
{
    // TODO: Remove classic client connected callback
    return true;
}

bool bluetooth_set_callback_classic_data(const gm::wire::GMFunction&)
{
    // TODO: Store callback for classic data available events
    return true;
}

bool bluetooth_remove_callback_classic_data()
{
    // TODO: Remove classic data callback
    return true;
}

bool bluetooth_set_callback_classic_disconnected(const gm::wire::GMFunction&)
{
    // TODO: Store callback for classic disconnected events
    return true;
}

bool bluetooth_remove_callback_classic_disconnected()
{
    // TODO: Remove classic disconnected callback
    return true;
}
