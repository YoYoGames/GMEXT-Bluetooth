#include "native/GMBluetoothInternal_native.h"
#include "GMBluetooth_backend.h"

#include <algorithm>
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
    using gmbluetooth::DiscoveredDevice;
    using gmbluetooth::Error;

    constexpr std::uint64_t HANDLE_MAGIC       = 0x42ULL;
    constexpr std::uint64_t HANDLE_TYPE_DEVICE = 0x01ULL;

    // 48-bit packed handle:
    // [ 8-bit magic | 8-bit type | 32-bit local id ]
    // This remains exactly representable by a GML double if it is ever passed
    // through an untyped/dynamic callback value.
    std::uint64_t make_device_handle(std::uint32_t id)
    {
        return (HANDLE_MAGIC << 40) |
               (HANDLE_TYPE_DEVICE << 32) |
               static_cast<std::uint64_t>(id);
    }

    bool is_device_handle(std::uint64_t handle)
    {
        return ((handle >> 40) & 0xFFULL) == HANDLE_MAGIC &&
               ((handle >> 32) & 0xFFULL) == HANDLE_TYPE_DEVICE &&
               (handle & 0xFFFFFFFFULL) != 0;
    }

    struct DeviceEntry
    {
        std::uint64_t handle = 0;
        std::int32_t transport = 2; // BluetoothTransport.LowEnergy

        std::string id;
        std::string name;
        std::string address;

        bool address_available = false;
        bool connectable = false;

        std::int32_t rssi = 0;
        bool rssi_available = false;
    };

    enum class EventType : std::uint8_t
    {
        DeviceFound,
        ScanStopped,
    };

    struct Event
    {
        EventType type = EventType::DeviceFound;
        std::uint64_t device = 0;
        Error error = Error::Ok;
        std::string message;
    };

    std::unique_ptr<Backend> g_backend;
    bool g_initialized = false;

    std::mutex g_devices_mutex;
    std::unordered_map<std::string, std::uint64_t> g_device_by_id;
    std::map<std::uint64_t, DeviceEntry> g_devices;
    std::vector<std::uint64_t> g_device_order;
    std::uint32_t g_next_device_id = 1;

    std::mutex g_events_mutex;
    std::deque<Event> g_events;

    std::mutex g_error_mutex;
    Error g_last_error = Error::Ok;
    std::string g_last_error_message;

    gm::wire::GMFunction g_callback_device_found = nullptr;
    gm::wire::GMFunction g_callback_scan_stopped = nullptr;

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

    void on_device_discovered(const DiscoveredDevice& incoming)
    {
        if (incoming.id.empty())
            return;

        std::uint64_t handle = 0;
        bool is_new = false;

        {
            std::scoped_lock lock(g_devices_mutex);

            auto existing = g_device_by_id.find(incoming.id);
            if (existing == g_device_by_id.end())
            {
                const std::uint32_t local_id = g_next_device_id++;
                handle = make_device_handle(local_id);

                DeviceEntry entry;
                entry.handle = handle;
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

                    if (!incoming.name.empty())
                        entry.name = incoming.name;

                    if (incoming.address_available)
                    {
                        entry.address = incoming.address;
                        entry.address_available = true;
                    }

                    // A scan-response packet can itself be marked non-connectable,
                    // so never erase a connectable indication observed earlier.
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
    }

    void on_scan_stopped(Error error, const std::string& message)
    {
        if (error != Error::Ok)
            set_last_error(error, message);

        Event event;
        event.type = EventType::ScanStopped;
        event.error = error;
        event.message = message;
        push_event(std::move(event));
    }

    bool copy_device(std::uint64_t handle, DeviceEntry& out)
    {
        if (!is_device_handle(handle))
            return false;

        std::scoped_lock lock(g_devices_mutex);
        const auto it = g_devices.find(handle);
        if (it == g_devices.end())
            return false;

        out = it->second;
        return true;
    }

    void clear_devices()
    {
        std::scoped_lock lock(g_devices_mutex);
        g_device_by_id.clear();
        g_devices.clear();
        g_device_order.clear();
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

    g_backend = gmbluetooth::create_platform_backend(
        on_device_discovered,
        on_scan_stopped);

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
    clear_events();

    g_callback_device_found = nullptr;
    g_callback_scan_stopped = nullptr;

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
        switch (event.type)
        {
            case EventType::DeviceFound:
                if (g_callback_device_found)
                {
                    // Packed handles use only 48 bits, so this conversion is exact.
                    g_callback_device_found.call(static_cast<double>(event.device));
                }
                break;

            case EventType::ScanStopped:
                if (g_callback_scan_stopped)
                {
                    g_callback_scan_stopped.call(
                        static_cast<std::int32_t>(event.error),
                        event.message);
                }
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
    // Classic is intentionally not exposed as supported until the RFCOMM
    // milestone is implemented in this extension.
    return g_backend && g_backend->supports_classic();
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
    {
        set_last_error(Error::NotInitialized, "Bluetooth is not initialized");
        return static_cast<std::int32_t>(Error::NotInitialized);
    }

    std::string message;
    const Error error = g_backend->le_scan_stop(message);
    set_last_error(error, std::move(message));
    return static_cast<std::int32_t>(error);
}

bool bluetooth_le_scan_is_running()
{
    return g_initialized && g_backend && g_backend->le_scan_is_running();
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
    if (i >= g_device_order.size())
        return 0;

    return g_device_order[i];
}

bool bluetooth_device_is_valid(std::uint64_t device)
{
    DeviceEntry entry;
    return copy_device(device, entry);
}

std::int32_t bluetooth_device_get_transport(std::uint64_t device)
{
    DeviceEntry entry;
    return copy_device(device, entry) ? entry.transport : 0;
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
    return copy_device(device, entry) && entry.rssi_available
        ? entry.rssi
        : 0;
}

bool bluetooth_device_is_connectable(std::uint64_t device)
{
    DeviceEntry entry;
    return copy_device(device, entry) && entry.connectable;
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
