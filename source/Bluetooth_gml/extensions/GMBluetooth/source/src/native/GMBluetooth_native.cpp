#include "GMBluetooth_native.h"
#include "GMBluetooth_backend.h"
#include "GMBluetooth_log.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace gm::wire;
using namespace gm_structs;
using namespace gm_enums;
using namespace gmbluetooth;

namespace
{
    std::unique_ptr<Backend> g_backend;
    std::string g_last_error_message;
    Error g_last_error = Error::Ok;

    BluetoothError to_gm(Error error)
    {
        return static_cast<BluetoothError>(error);
    }

    // last-error is the detail channel for a failed call: a success leaves
    // whatever an earlier failure wrote.
    void set_last_error(Error error, const std::string& message)
    {
        if (error == Error::Ok)
            return;
        g_last_error = error;
        g_last_error_message = message;
    }

    // A failed call's answer: records why in last-error and returns the code.
    BluetoothError fail(Error error, std::string message)
    {
        g_last_error = error;
        g_last_error_message = std::move(message);
        return to_gm(error);
    }

    // The guard of every call that needs the backend: false, with
    // NotInitialized as the last error, before bluetooth_initialize.
    bool require_backend()
    {
        if (g_backend)
            return true;
        fail(Error::NotInitialized, "Bluetooth backend is not initialized");
        return false;
    }

    // The generated GML wrapper only checks buffer_exists, so offset and size are
    // validated here against the buffer's real length - the same rule as
    // Android's bufferRangeInvalid.
    bool buffer_range_valid(const GMBuffer& buffer, unsigned int offset, unsigned int size, const char* function_name)
    {
        if (static_cast<std::uint64_t>(offset) + size <= buffer.length())
            return true;

        fail(Error::InvalidArgument, std::string("Invalid buffer offset/size for ") + function_name);
        return false;
    }

    // Callback storage
    std::mutex g_callback_mutex;
    GMFunction g_callback_state_changed;
    GMFunction g_callback_device_found;
    GMFunction g_callback_scan_stopped;
    GMFunction g_callback_classic_client_connected;
    GMFunction g_callback_classic_data;
    GMFunction g_callback_classic_disconnected;
    GMFunction g_callback_le_disconnected;
    GMFunction g_callback_le_characteristic_value_changed;
    GMFunction g_callback_le_server_connection_state_changed;
    GMFunction g_callback_le_server_read_request;
    GMFunction g_callback_le_server_write_request;

    // Counts every event the backend hands up that the core then drops, so the
    // gap between "the backend is working" and "GML sees nothing" is visible in
    // the log instead of having to be inferred.
    std::atomic<std::uint64_t> g_dropped_events{ 0 };

    // A registered callback, copied out so it is never called under the lock.
    GMFunction registered_callback(const GMFunction& slot)
    {
        std::scoped_lock lock(g_callback_mutex);
        return slot;
    }

    // The registered callback an event goes to; none set drops the event.
    GMFunction event_callback(const GMFunction& slot)
    {
        GMFunction callback = registered_callback(slot);
        if (!callback)
            g_dropped_events++;
        return callback;
    }

    // Calls a callback, if there is one, and logs what it throws. Never called
    // with a lock held; GMFunction::call only queues the call for GML.
    template <typename... Args>
    void invoke_callback(const char* name, const GMFunction& callback, Args&&... args)
    {
        if (!callback)
            return;
        try
        {
            callback.call(std::forward<Args>(args)...);
        }
        catch (const std::exception& e)
        {
            GMBT_LOG("Error dispatching %s callback: %s", name, e.what());
        }
    }

    bool set_callback(GMFunction& slot, const GMFunction& callback, const char* what)
    {
        std::scoped_lock lock(g_callback_mutex);
        slot = callback;
        GMBT_LOG("%s callback registered", what);
        return true;
    }

    bool remove_callback(GMFunction& slot, const char* what)
    {
        std::scoped_lock lock(g_callback_mutex);
        slot = GMFunction();
        GMBT_LOG("%s callback removed", what);
        return true;
    }

    // Held across reading a state and queueing it, by the registration's initial
    // delivery and by every state event, so GML never ends on a stale state
    // (R1-159). The backend updates its state before it pushes the event.
    std::mutex g_state_dispatch_mutex;

    // Pairing never creates a connection handle, so this is keyed directly by
    // the device handle instead.
    std::mutex g_pending_pair_mutex;
    std::unordered_map<std::uint64_t, GMFunction> g_pending_pair_callbacks;

    // Every permission_request callback waiting for the answer; one answer
    // fires them all, so a second request while the prompt is up just waits.
    std::mutex g_pending_permission_mutex;
    std::uint64_t g_next_permission_request = 1;
    std::map<std::uint64_t, GMFunction> g_pending_permission_callbacks;

    void fire_permission_callbacks(Error error, const std::string& message, PermissionStatus status)
    {
        std::map<std::uint64_t, GMFunction> callbacks;
        { std::scoped_lock lock(g_pending_permission_mutex); callbacks.swap(g_pending_permission_callbacks); }
        for (const auto& [request, callback] : callbacks)
        {
            (void)request;
            // callback(error_code, message, status)
            invoke_callback("permission_request", callback, static_cast<double>(error), message, static_cast<double>(status));
        }
    }

    // Every request_enable callback waiting for the radio; one answer fires
    // them all, so a second request while the dialog is up just waits.
    std::mutex g_pending_enable_mutex;
    std::vector<GMFunction> g_pending_enable_callbacks;

    void fire_enable_callbacks(Error error, const std::string& message)
    {
        std::vector<GMFunction> callbacks;
        { std::scoped_lock lock(g_pending_enable_mutex); callbacks.swap(g_pending_enable_callbacks); }
        for (const auto& callback : callbacks)
        {
            // callback(error_code, message)
            invoke_callback("request_enable", callback, static_cast<double>(error), message);
        }
    }

    // The connected and paired device queries waiting for their answer, by the
    // query id the backend echoes on its DevicesQueried event.
    std::mutex g_pending_query_mutex;
    std::uint64_t g_next_query = 1;
    std::map<std::uint64_t, GMFunction> g_pending_query_callbacks;

    void fire_query_callback(const GMFunction& callback, Error error, const std::string& message,
        const std::vector<double>& devices)
    {
        // callback(error_code, message, devices)
        invoke_callback("device query", callback, static_cast<double>(error), message, devices);
    }

    // Registers a device query's callback under a fresh id, asks the backend,
    // and takes the callback back out if the backend refused before starting.
    template <typename Start>
    BluetoothError start_devices_query(const GMFunction& callback, Start start)
    {
        std::uint64_t query = 0;
        {
            std::scoped_lock lock(g_pending_query_mutex);
            query = g_next_query++;
            g_pending_query_callbacks.emplace(query, callback);
        }

        std::string message;
        const Error error = start(query, message);
        set_last_error(error, message);
        if (error != Error::Ok)
        {
            std::scoped_lock lock(g_pending_query_mutex);
            g_pending_query_callbacks.erase(query);
        }
        return to_gm(error);
    }

    std::string canonical_uuid(std::string_view uuid);

    // A device's advertisement record keeps at most this many service UUIDs,
    // and as many service data and manufacturer entries; a peripheral cycling
    // through keys cannot grow it further.
    constexpr std::size_t kMaxAdvertisementEntries = 32;

    // Folds one packet into a device's advertisement record (R1-138): service
    // UUIDs collect, a service data or manufacturer entry is replaced by the
    // newer one for its key, and so is the TX power.
    void merge_advertisement(std::optional<LeAdvertisement>& stored, const LeAdvertisement& update)
    {
        if (!stored)
            stored.emplace();

        for (const auto& uuid : update.service_uuids)
        {
            std::string canonical = canonical_uuid(uuid);
            auto& uuids = stored->service_uuids;
            if (std::find(uuids.begin(), uuids.end(), canonical) == uuids.end() && uuids.size() < kMaxAdvertisementEntries)
                uuids.push_back(std::move(canonical));
        }

        for (const auto& entry : update.service_data)
        {
            const std::string uuid = canonical_uuid(entry.uuid);
            auto& entries = stored->service_data;
            const auto it = std::find_if(entries.begin(), entries.end(),
                [&](const LeAdvertiseServiceData& e) { return e.uuid == uuid; });
            if (it != entries.end())
                it->data = entry.data;
            else if (entries.size() < kMaxAdvertisementEntries)
                entries.push_back(LeAdvertiseServiceData{ uuid, entry.data });
        }

        for (const auto& entry : update.manufacturer_data)
        {
            auto& entries = stored->manufacturer_data;
            const auto it = std::find_if(entries.begin(), entries.end(),
                [&](const LeAdvertiseManufacturerData& e) { return e.company_id == entry.company_id; });
            if (it != entries.end())
                it->data = entry.data;
            else if (entries.size() < kMaxAdvertisementEntries)
                entries.push_back(entry);
        }

        if (update.tx_power)
            stored->tx_power = update.tx_power;
    }

    // A device as it enters a cache: its advertisement in canonical form.
    DiscoveredDevice normalized_device(const DiscoveredDevice& device)
    {
        DiscoveredDevice out = device;
        out.advertisement.reset();
        if (device.advertisement)
            merge_advertisement(out.advertisement, *device.advertisement);
        return out;
    }

    // An advertisement or a scan response carries only some fields (no
    // LocalName, connectable false), so an update overwrites only what it
    // actually carries.
    void merge_device(DiscoveredDevice& stored, const DiscoveredDevice& update)
    {
        stored.transport = update.transport;
        if (!update.name.empty())
            stored.name = update.name;
        if (update.address_available)
        {
            stored.address = update.address;
            stored.address_available = true;
        }
        if (update.rssi_available)
        {
            stored.rssi = update.rssi;
            stored.rssi_available = true;
        }
        stored.connectable = stored.connectable || update.connectable;
        if (update.advertisement)
            merge_advertisement(stored.advertisement, *update.advertisement);
    }

    // Every handle GML holds carries its kind, laid out as Android's makeHandle:
    // (0x42 << 40) | (kind << 32) | id. It stays below 2^47, so a double holds
    // it exactly, and a handle of one kind passed where another is expected
    // names nothing in that kind's table (R1-210). Value ids and server request
    // ids stay plain counters, as on Android.
    enum class HandleKind : std::uint64_t
    {
        Device            = 1,
        ClassicConnection = 2,
        LeConnection      = 3, // client and server connections alike
        LeService         = 4,
        LeCharacteristic  = 5,
        LeDescriptor      = 6,
        LeServerService        = 7,
        LeServerCharacteristic = 8,
        LeServerDescriptor     = 9,
    };

    constexpr std::uint64_t make_handle(HandleKind kind, std::uint64_t id)
    {
        return (std::uint64_t{ 0x42 } << 40) | (static_cast<std::uint64_t>(kind) << 32) | (id & 0xFFFFFFFFu);
    }

    // Device handles count up and are never reused: clear() forgets every
    // device, and one found again afterwards gets a new handle, so a handle a
    // connection or a pending pair still holds can never name another device.
    class DeviceManager
    {
    public:
        // created says whether this call added the device: device_found fires
        // once per device, not once per advertisement (R1-43).
        std::uint64_t upsert_device(const DiscoveredDevice& device, bool* created = nullptr)
        {
            std::scoped_lock lock(mutex_);
            if (const auto it = by_id_.find(device.id); it != by_id_.end())
            {
                merge_device(devices_.at(it->second), device);
                if (created)
                    *created = false;
                return it->second;
            }

            const std::uint64_t handle = make_handle(HandleKind::Device, next_handle_++);
            devices_.emplace(handle, normalized_device(device));
            by_id_.emplace(device.id, handle);
            order_.push_back(handle);
            if (created)
                *created = true;
            return handle;
        }

        void clear()
        {
            std::scoped_lock lock(mutex_);
            devices_.clear();
            by_id_.clear();
            order_.clear();
        }

        int get_count() const
        {
            std::scoped_lock lock(mutex_);
            return static_cast<int>(order_.size());
        }

        std::uint64_t get_at(int index) const
        {
            std::scoped_lock lock(mutex_);
            if (index < 0 || index >= static_cast<int>(order_.size()))
                return 0;
            return order_[static_cast<std::size_t>(index)];
        }

        bool is_valid(std::uint64_t handle) const
        {
            std::scoped_lock lock(mutex_);
            return devices_.find(handle) != devices_.end();
        }

        // Returns a copy: backend threads upsert concurrently, so neither an
        // element address nor a reference may outlive the lock.
        std::optional<DiscoveredDevice> get_device(std::uint64_t handle) const
        {
            std::scoped_lock lock(mutex_);
            const auto it = devices_.find(handle);
            if (it == devices_.end())
                return std::nullopt;
            return it->second;
        }

        // 0 when no cached device has this id.
        std::uint64_t find_id(const std::string& id) const
        {
            std::scoped_lock lock(mutex_);
            const auto it = by_id_.find(id);
            return it != by_id_.end() ? it->second : 0;
        }

    private:
        mutable std::mutex mutex_;
        std::unordered_map<std::uint64_t, DiscoveredDevice> devices_;
        std::unordered_map<std::string, std::uint64_t> by_id_;
        std::vector<std::uint64_t> order_;
        std::uint64_t next_handle_ = 1;
    };

    DeviceManager g_device_manager;

    // The filters of the LE scan that is running, and the devices it has heard
    // that match none of them yet (R1-139). A device enters the cache, firing
    // device_found, when its merged advertisement first matches: Windows reports
    // an advertisement and its scan response separately, so a filter on a name
    // in one and a service in the other matches only the merged record. The
    // held devices are bounded, the oldest dropped, and emptied by every scan
    // start and by bluetooth_device_clear.
    class ScanFilterState
    {
    public:
        void start(std::vector<LeScanFilter> filters)
        {
            std::scoped_lock lock(mutex_);
            filters_ = std::move(filters);
            pending_.clear();
            order_.clear();
        }

        void clear_pending()
        {
            std::scoped_lock lock(mutex_);
            pending_.clear();
            order_.clear();
        }

        // The device to put in the cache now, merged with what was held for
        // it, or nothing while it still matches no filter.
        std::optional<DiscoveredDevice> admit(const DiscoveredDevice& device)
        {
            std::scoped_lock lock(mutex_);
            if (filters_.empty())
                return device;

            auto it = pending_.find(device.id);
            if (it == pending_.end())
            {
                if (pending_.size() >= kMaxPending && !order_.empty())
                {
                    pending_.erase(order_.front());
                    order_.erase(order_.begin());
                }
                it = pending_.emplace(device.id, normalized_device(device)).first;
                order_.push_back(device.id);
            }
            else
            {
                merge_device(it->second, device);
            }

            if (!matches_any(it->second))
                return std::nullopt;

            DiscoveredDevice admitted = std::move(it->second);
            pending_.erase(it);
            order_.erase(std::find(order_.begin(), order_.end(), admitted.id));
            return admitted;
        }

    private:
        static constexpr std::size_t kMaxPending = 256;

        static bool matches(const LeScanFilter& filter, const DiscoveredDevice& device)
        {
            const LeAdvertisement empty;
            const LeAdvertisement& advertisement = device.advertisement ? *device.advertisement : empty;
            if (filter.service_uuid)
            {
                const auto& uuids = advertisement.service_uuids;
                if (std::find(uuids.begin(), uuids.end(), *filter.service_uuid) == uuids.end())
                    return false;
            }
            if (filter.name && device.name != *filter.name)
                return false;
            if (filter.company_id)
            {
                const auto& entries = advertisement.manufacturer_data;
                if (std::none_of(entries.begin(), entries.end(),
                    [&](const LeAdvertiseManufacturerData& e) { return e.company_id == *filter.company_id; }))
                    return false;
            }
            return true;
        }

        bool matches_any(const DiscoveredDevice& device) const
        {
            return std::any_of(filters_.begin(), filters_.end(),
                [&](const LeScanFilter& filter) { return matches(filter, device); });
        }

        std::mutex mutex_;
        std::vector<LeScanFilter> filters_;
        std::unordered_map<std::string, DiscoveredDevice> pending_;
        std::vector<std::string> order_;
    };

    ScanFilterState g_scan_filters;

    // The connections of one transport, each to the device it was made for,
    // and the one-shot callback of each connect still in flight. Classic and LE
    // connections are minted from two independent handle spaces; handles count
    // up and are never reused, so a handle from before a clear never names a
    // later connection.
    template <HandleKind Kind>
    class ConnectionRegistry
    {
    public:
        std::uint64_t create_connection(std::uint64_t device)
        {
            std::scoped_lock lock(mutex_);
            const std::uint64_t handle = make_handle(Kind, next_handle_++);
            connections_[handle] = device;
            return handle;
        }

        bool remove_connection(std::uint64_t handle)
        {
            std::scoped_lock lock(mutex_);
            return connections_.erase(handle) != 0;
        }

        // A handle from the same counter that names no connection: the LE
        // server side's connection to a remote central.
        std::uint64_t reserve_handle()
        {
            std::scoped_lock lock(mutex_);
            return make_handle(Kind, next_handle_++);
        }

        void clear()
        {
            std::scoped_lock lock(mutex_);
            connections_.clear();
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

        // A device has at most one LE client connection, connecting or
        // connected.
        bool has_device(std::uint64_t device) const
        {
            std::scoped_lock lock(mutex_);
            for (const auto& [handle, connected_device] : connections_)
            {
                (void)handle;
                if (connected_device == device)
                    return true;
            }
            return false;
        }

        // Registered before the backend is asked: a connect runs
        // asynchronously and may complete before the call returns. Kept apart
        // from the connection, so retiring a handle leaves its callback where
        // the connect's completion, a disconnect or shutdown takes it.
        void set_connect_callback(std::uint64_t handle, const GMFunction& callback)
        {
            if (!callback)
                return;
            std::scoped_lock lock(mutex_);
            connect_callbacks_[handle] = callback;
        }

        // Empty when none is waiting: the connect already completed, was
        // cancelled, or was made without a callback.
        GMFunction take_connect_callback(std::uint64_t handle)
        {
            std::scoped_lock lock(mutex_);
            const auto it = connect_callbacks_.find(handle);
            if (it == connect_callbacks_.end())
                return GMFunction();
            GMFunction callback = std::move(it->second);
            connect_callbacks_.erase(it);
            return callback;
        }

        std::unordered_map<std::uint64_t, GMFunction> take_connect_callbacks()
        {
            std::unordered_map<std::uint64_t, GMFunction> callbacks;
            std::scoped_lock lock(mutex_);
            callbacks.swap(connect_callbacks_);
            return callbacks;
        }

    private:
        mutable std::mutex mutex_;
        std::unordered_map<std::uint64_t, std::uint64_t> connections_;
        std::unordered_map<std::uint64_t, GMFunction> connect_callbacks_;
        std::uint64_t next_handle_ = 1;
    };

    // A Classic handle outlives a remote hang-up while the backend still holds
    // bytes the game has not read (R1-25), so the disconnect event only marks
    // it closed; the game-thread calls retire it once nothing is left.
    ConnectionRegistry<HandleKind::ClassicConnection> g_classic_connection_manager;

    // Client connections; server connections take reserve_handle.
    ConnectionRegistry<HandleKind::LeConnection> g_le_connection_manager;

    // Backends spell one UUID several ways (Apple caches a SIG service as
    // "0000180D-0000-1000-8000-00805F9B34FB" but its notifications carry
    // "180D"), so the caches compare UUIDs in one form: 128-bit, lowercase. The
    // stored string stays as the backend reported it.
    std::string canonical_uuid(std::string_view uuid)
    {
        static constexpr std::string_view base_suffix = "-0000-1000-8000-00805f9b34fb";

        std::string out;
        if (uuid.size() == 4)
            out.append("0000").append(uuid).append(base_suffix);
        else if (uuid.size() == 8)
            out.append(uuid).append(base_suffix);
        else
            out.assign(uuid);

        std::transform(out.begin(), out.end(), out.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return out;
    }

    // The UUID spellings every backend can take: 4 or 8 hex digits, or the
    // 36-character 8-4-4-4-12 form. Apple's [CBUUID UUIDWithString:] throws on
    // anything else, so every UUID GML passes in is checked here first.
    bool is_valid_uuid(std::string_view uuid)
    {
        const auto is_hex = [](char c) { return std::isxdigit(static_cast<unsigned char>(c)) != 0; };

        if (uuid.size() == 4 || uuid.size() == 8)
            return std::all_of(uuid.begin(), uuid.end(), is_hex);

        if (uuid.size() != 36)
            return false;

        for (std::size_t i = 0; i < uuid.size(); ++i)
        {
            const bool dash = (i == 8 || i == 13 || i == 18 || i == 23);
            if (dash ? uuid[i] != '-' : !is_hex(uuid[i]))
                return false;
        }
        return true;
    }

    // The pre-flight for a UUID argument: false, with the last error set, when
    // it is not one is_valid_uuid accepts.
    bool check_uuid(std::string_view uuid)
    {
        if (is_valid_uuid(uuid))
            return true;

        fail(Error::InvalidArgument, "Invalid UUID: " + std::string(uuid));
        return false;
    }

    // The radio and permission pre-flight every call that starts radio work
    // makes, so the same condition fails the same way, synchronously, on every
    // platform (R1-66, R1-68). Unknown and Resetting pass: a backend may still
    // be waiting for its first state, and starts the work once it has one.
    // Assumes g_backend.
    bool check_radio()
    {
        if (g_backend->permission_status() == PermissionStatus::Denied)
        {
            fail(Error::PermissionDenied, "Bluetooth permission was denied");
            return false;
        }

        // BluetoothState raw values from spec.gmidl.
        switch (g_backend->current_bluetooth_state())
        {
            case 2: // Unsupported
                fail(Error::NotSupported, "Bluetooth is not supported on this device");
                return false;
            case 3: // Unauthorized
                fail(Error::PermissionDenied, "Bluetooth permission has not been granted");
                return false;
            case 4: // PoweredOff
                fail(Error::BluetoothDisabled, "Bluetooth is turned off");
                return false;
            default:
                return true;
        }
    }

    // The device pre-flight of the connect and pair calls: InvalidHandle for a
    // handle that names no device, InvalidArgument for a device of the other
    // transport (R1-67). Transport::Unknown is let through: the backend decides.
    std::optional<DiscoveredDevice> check_device(std::uint64_t device, Transport transport)
    {
        auto dev = g_device_manager.get_device(device);
        if (!dev)
        {
            fail(Error::InvalidHandle, "Invalid device handle");
            return std::nullopt;
        }
        if (transport != Transport::Unknown && dev->transport != Transport::Unknown && dev->transport != transport)
        {
            fail(Error::InvalidArgument, transport == Transport::LowEnergy
                ? "The device is not a Bluetooth LE device"
                : "The device is not a Bluetooth Classic device");
            return std::nullopt;
        }
        return dev;
    }

    // The shared pre-flight of the calls on a client connection: initialized,
    // and a client handle (a server connection is InvalidHandle).
    bool check_le_client_connection(std::uint64_t connection)
    {
        if (!require_backend())
            return false;
        if (!g_le_connection_manager.is_valid(connection))
        {
            fail(Error::InvalidHandle, "Invalid LE connection handle");
            return false;
        }
        return true;
    }

    // Assumes g_backend.
    bool check_le_connected(std::uint64_t connection)
    {
        if (g_backend->le_connection_is_connected(connection))
            return true;
        fail(Error::Disconnected, "The LE connection is not connected");
        return false;
    }

    // Assumes g_backend.
    bool check_feature(BluetoothFeature feature, const char* message)
    {
        if (g_backend->feature_supported(static_cast<std::int32_t>(feature)))
            return true;
        fail(Error::NotSupported, message);
        return false;
    }

    // Parent-scoped handle caches for GATT services/characteristics/descriptors.
    // Handles count up and are never reused, like device handles; an entry
    // leaves when its LE connection is retired, so a reconnect does not pile
    // fresh entries on top of the old ones. The map is ordered by handle, which
    // is creation order, so get_at enumerates in discovery order. An entry is
    // keyed by its parent and the instance its backend reported, never by its
    // UUID, so two attributes that share a UUID get two handles (R1-127);
    // find_or_insert is idempotent so re-running discovery doesn't mint
    // duplicates. UUIDs are stored canonical, the one form GML ever sees (R1-14).
    template <typename Entry, HandleKind Kind>
    class AttributeCache
    {
    public:
        std::uint64_t find_by_instance(std::uint64_t parent, std::uint64_t instance) const
        {
            std::scoped_lock lock(mutex_);
            for (const auto& [handle, entry] : entries_)
            {
                if (entry.parent == parent && entry.instance == instance)
                    return handle;
            }
            return 0;
        }

        int get_count(std::uint64_t parent) const
        {
            std::scoped_lock lock(mutex_);
            int count = 0;
            for (const auto& [handle, entry] : entries_)
            {
                (void)handle;
                if (entry.parent == parent)
                    ++count;
            }
            return count;
        }

        std::uint64_t get_at(std::uint64_t parent, int index) const
        {
            std::scoped_lock lock(mutex_);
            int seen = 0;
            for (const auto& [handle, entry] : entries_)
            {
                if (entry.parent != parent)
                    continue;
                if (seen == index)
                    return handle;
                ++seen;
            }
            return 0;
        }

        bool is_valid(std::uint64_t handle) const
        {
            std::scoped_lock lock(mutex_);
            return entries_.find(handle) != entries_.end();
        }

        std::string get_uuid(std::uint64_t handle) const
        {
            std::scoped_lock lock(mutex_);
            const auto it = entries_.find(handle);
            return it != entries_.end() ? it->second.uuid : std::string();
        }

        std::uint64_t get_parent(std::uint64_t handle) const
        {
            std::scoped_lock lock(mutex_);
            const auto it = entries_.find(handle);
            return it != entries_.end() ? it->second.parent : 0;
        }

        // The backend's instance for the handle; 0 when the handle is unknown.
        std::uint64_t get_instance(std::uint64_t handle) const
        {
            std::scoped_lock lock(mutex_);
            const auto it = entries_.find(handle);
            return it != entries_.end() ? it->second.instance : 0;
        }

        // Erases every entry whose parent is in parents; returns the erased
        // handles, the parents of the next level down.
        std::vector<std::uint64_t> erase_children(const std::vector<std::uint64_t>& parents)
        {
            std::vector<std::uint64_t> erased;
            std::scoped_lock lock(mutex_);
            for (auto it = entries_.begin(); it != entries_.end();)
            {
                if (std::find(parents.begin(), parents.end(), it->second.parent) != parents.end())
                {
                    erased.push_back(it->first);
                    it = entries_.erase(it);
                }
                else
                {
                    ++it;
                }
            }
            return erased;
        }

        // Erases the parent's entries whose instance a discovery no longer
        // reported (R1-258); returns the erased handles, as erase_children.
        std::vector<std::uint64_t> erase_unreported(std::uint64_t parent, const std::vector<LeAttribute>& reported)
        {
            std::vector<std::uint64_t> erased;
            std::scoped_lock lock(mutex_);
            for (auto it = entries_.begin(); it != entries_.end();)
            {
                const std::uint64_t instance = it->second.instance;
                const bool kept = it->second.parent != parent ||
                    std::any_of(reported.begin(), reported.end(),
                        [instance](const LeAttribute& attribute) { return attribute.instance == instance; });
                if (kept)
                {
                    ++it;
                }
                else
                {
                    erased.push_back(it->first);
                    it = entries_.erase(it);
                }
            }
            return erased;
        }

        void clear()
        {
            std::scoped_lock lock(mutex_);
            entries_.clear();
        }

        std::uint64_t find_or_insert(std::uint64_t parent, const LeAttribute& attribute)
        {
            std::scoped_lock lock(mutex_);
            Entry* entry = nullptr;
            return find_or_insert_locked(parent, attribute, entry);
        }

    protected:
        // Caller holds mutex_. Returns the existing entry's handle or a new one.
        std::uint64_t find_or_insert_locked(std::uint64_t parent, const LeAttribute& attribute, Entry*& entry)
        {
            for (auto& [handle, existing] : entries_)
            {
                if (existing.parent == parent && existing.instance == attribute.instance)
                {
                    entry = &existing;
                    return handle;
                }
            }

            const std::uint64_t handle = make_handle(Kind, next_handle_++);
            Entry& inserted = entries_[handle];
            inserted.parent = parent;
            inserted.instance = attribute.instance;
            inserted.uuid = canonical_uuid(attribute.uuid);
            entry = &inserted;
            return handle;
        }

        mutable std::mutex mutex_;
        std::map<std::uint64_t, Entry> entries_;
        std::uint64_t next_handle_ = 1;
    };

    struct AttributeEntry
    {
        std::uint64_t parent = 0;
        std::uint64_t instance = 0;
        std::string uuid;
    };

    // parent: the LE connection.
    AttributeCache<AttributeEntry, HandleKind::LeService> g_service_cache;

    // parent: the service.
    struct CharacteristicEntry : AttributeEntry
    {
        std::int32_t properties = 0;
    };

    class CharacteristicCache : public AttributeCache<CharacteristicEntry, HandleKind::LeCharacteristic>
    {
    public:
        std::uint64_t find_or_insert(std::uint64_t service, const LeAttribute& attribute)
        {
            std::scoped_lock lock(mutex_);
            CharacteristicEntry* entry = nullptr;
            const std::uint64_t handle = find_or_insert_locked(service, attribute, entry);
            entry->properties = attribute.properties & 0xFF;
            return handle;
        }

        std::int32_t get_properties(std::uint64_t handle) const
        {
            std::scoped_lock lock(mutex_);
            const auto it = entries_.find(handle);
            return it != entries_.end() ? it->second.properties : 0;
        }
    };

    CharacteristicCache g_characteristic_cache;

    // parent: the characteristic.
    AttributeCache<AttributeEntry, HandleKind::LeDescriptor> g_descriptor_cache;

    // An attribute as its backend knows it: the instances its discovery
    // reported, for the handle and its parents. Assumes a valid handle.
    LeAttributeRef service_ref(std::uint64_t service)
    {
        LeAttributeRef ref;
        ref.service = g_service_cache.get_instance(service);
        return ref;
    }

    LeAttributeRef characteristic_ref(std::uint64_t characteristic)
    {
        LeAttributeRef ref = service_ref(g_characteristic_cache.get_parent(characteristic));
        ref.characteristic = g_characteristic_cache.get_instance(characteristic);
        return ref;
    }

    LeAttributeRef descriptor_ref(std::uint64_t descriptor)
    {
        LeAttributeRef ref = characteristic_ref(g_descriptor_cache.get_parent(descriptor));
        ref.descriptor = g_descriptor_cache.get_instance(descriptor);
        return ref;
    }

    // The LE connection an attribute was discovered on. Assumes a valid handle.
    std::uint64_t characteristic_connection(std::uint64_t characteristic)
    {
        return g_service_cache.get_parent(g_characteristic_cache.get_parent(characteristic));
    }

    std::uint64_t descriptor_connection(std::uint64_t descriptor)
    {
        return characteristic_connection(g_descriptor_cache.get_parent(descriptor));
    }

    // The bytes GML passed, copied out of its buffer before the call returns.
    // Assumes buffer_range_valid passed.
    std::vector<std::uint8_t> slice_bytes(const GMBuffer& buffer, unsigned int offset, unsigned int size)
    {
        const std::uint8_t* begin = static_cast<const std::uint8_t*>(buffer.data()) + offset;
        return std::vector<std::uint8_t>(begin, begin + size);
    }

    // The bytes a callback delivered - an LE read or notification, a server
    // write request, a Classic chunk - held under a value id until GML fetches
    // or releases them (R1-11, R1-245). Ids count up and are never reused. A
    // value outlives its link, since its callback may still be queued behind
    // the disconnect. Every value expires 30 s after it was stored; LE values
    // are also capped, a server write value goes with its request, and a
    // Classic chunk counts toward its connection's receive budget until it
    // leaves.
    enum class ValueOwner : std::uint8_t
    {
        Le,
        ServerWrite,
        Classic,
    };

    // A value that left the store, for its owner to hear about.
    struct LeftValue
    {
        ValueOwner owner = ValueOwner::Le;
        std::uint64_t connection = 0;
        std::size_t size = 0;
        bool expired = false;
    };

    class ValueStore
    {
    public:
        static constexpr std::size_t k_max_le_values = 256;
        static constexpr auto k_ttl = std::chrono::seconds(30);

        std::uint64_t add(std::vector<std::uint8_t> bytes, ValueOwner owner, std::uint64_t connection, std::vector<LeftValue>& left)
        {
            std::scoped_lock lock(mutex_);
            expire_locked(left);

            const std::uint64_t id = next_id_++;
            values_.emplace(id, Held{ std::move(bytes), owner, connection, std::chrono::steady_clock::now() });
            if (owner == ValueOwner::Le && ++le_values_ > k_max_le_values)
            {
                const auto oldest = std::find_if(values_.begin(), values_.end(),
                    [](const auto& entry) { return entry.second.owner == ValueOwner::Le; });
                GMBT_LOG("value store holds %zu LE values, dropping value %llu", k_max_le_values,
                    static_cast<unsigned long long>(oldest->first));
                erase_locked(oldest);
            }
            return id;
        }

        // Copies the whole value to out at offset and frees it. Too small a
        // buffer copies nothing and keeps the value, so the call can be retried.
        Error fetch(std::uint64_t id, const GMBuffer& out, unsigned int offset, std::size_t& copied, std::string& message,
            std::vector<LeftValue>& left)
        {
            std::scoped_lock lock(mutex_);
            expire_locked(left);

            const auto it = values_.find(id);
            if (it == values_.end())
            {
                message = "Unknown, fetched, released or expired value " + std::to_string(id);
                return Error::InvalidHandle;
            }

            const auto& bytes = it->second.bytes;
            if (static_cast<std::uint64_t>(offset) + bytes.size() > out.length())
            {
                message = "Buffer too small for value " + std::to_string(id) + ": it needs " +
                    std::to_string(bytes.size()) + " bytes at offset " + std::to_string(offset);
                return Error::InvalidArgument;
            }

            if (!bytes.empty())
                std::memcpy(static_cast<std::uint8_t*>(out.data()) + offset, bytes.data(), bytes.size());
            copied = bytes.size();
            left.push_back(LeftValue{ it->second.owner, it->second.connection, bytes.size(), false });
            erase_locked(it);
            return Error::Ok;
        }

        bool release(std::uint64_t id, std::vector<LeftValue>& left)
        {
            std::scoped_lock lock(mutex_);
            expire_locked(left);

            const auto it = values_.find(id);
            if (it == values_.end())
                return false;
            left.push_back(LeftValue{ it->second.owner, it->second.connection, it->second.bytes.size(), false });
            erase_locked(it);
            return true;
        }

        void expire(std::vector<LeftValue>& left)
        {
            std::scoped_lock lock(mutex_);
            expire_locked(left);
        }

        void clear()
        {
            std::scoped_lock lock(mutex_);
            values_.clear();
            le_values_ = 0;
        }

    private:
        struct Held
        {
            std::vector<std::uint8_t> bytes;
            ValueOwner owner = ValueOwner::Le;
            std::uint64_t connection = 0;
            std::chrono::steady_clock::time_point stored_at;
        };

        using Values = std::map<std::uint64_t, Held>;

        void erase_locked(Values::iterator it)
        {
            if (it->second.owner == ValueOwner::Le)
                --le_values_;
            values_.erase(it);
        }

        // Ids and store times rise together, so the expired values are a
        // prefix of the map.
        void expire_locked(std::vector<LeftValue>& left)
        {
            const auto now = std::chrono::steady_clock::now();
            while (!values_.empty() && now - values_.begin()->second.stored_at >= k_ttl)
            {
                const auto it = values_.begin();
                GMBT_LOG("value %llu expired unfetched", static_cast<unsigned long long>(it->first));
                left.push_back(LeftValue{ it->second.owner, it->second.connection, it->second.bytes.size(), true });
                erase_locked(it);
            }
        }

        std::mutex mutex_;
        Values values_;
        std::size_t le_values_ = 0;
        std::uint64_t next_id_ = 1;
    };

    ValueStore g_values;

    // A Classic connection whose data expired unfetched ends rather than leave
    // a gap in its stream, as macOS ends one at the byte budget.
    void end_classic_connection_on_expiry(std::uint64_t connection)
    {
        if (!g_classic_connection_manager.remove_connection(connection))
            return;

        GMBT_LOG("classic connection %llu: receive data expired, ending it", static_cast<unsigned long long>(connection));
        if (g_backend)
        {
            std::string message;
            g_backend->classic_disconnect(connection, message);
        }

        // callback(error_code, message, connection)
        invoke_callback("classic_disconnected", event_callback(g_callback_classic_disconnected),
            static_cast<double>(Error::Disconnected), std::string("receive data expired"), static_cast<double>(connection));
    }

    // Tells the owners about values that left the store: a Classic chunk's
    // bytes go back to its connection's receive budget. Called with no lock held.
    void settle_left_values(const std::vector<LeftValue>& left)
    {
        std::vector<std::uint64_t> expired_connections;
        for (const auto& value : left)
        {
            if (value.owner != ValueOwner::Classic)
                continue;
            if (g_backend)
                g_backend->classic_receive_consumed(value.connection, value.size);
            if (value.expired &&
                std::find(expired_connections.begin(), expired_connections.end(), value.connection) == expired_connections.end())
                expired_connections.push_back(value.connection);
        }

        for (const std::uint64_t connection : expired_connections)
            end_classic_connection_on_expiry(connection);
    }

    std::uint64_t store_value(std::vector<std::uint8_t> bytes, ValueOwner owner, std::uint64_t connection = 0)
    {
        std::vector<LeftValue> left;
        const std::uint64_t id = g_values.add(std::move(bytes), owner, connection, left);
        settle_left_values(left);
        return id;
    }

    void release_value(std::uint64_t id)
    {
        if (id == 0)
            return;
        std::vector<LeftValue> left;
        g_values.release(id, left);
        settle_left_values(left);
    }

    // Expiry otherwise runs only as values come and go; a Classic receive
    // loop paused at its budget asks for it on a timer (ClassicReceivePaused).
    void expire_values()
    {
        std::vector<LeftValue> left;
        g_values.expire(left);
        settle_left_values(left);
    }

    // Drops what discovery cached for one LE connection: its services, their
    // characteristics and those characteristics' descriptors.
    void erase_le_descriptors_of(const std::vector<std::uint64_t>& characteristics)
    {
        if (!characteristics.empty())
            g_descriptor_cache.erase_children(characteristics);
    }

    void erase_le_characteristics_of(const std::vector<std::uint64_t>& services)
    {
        if (!services.empty())
            erase_le_descriptors_of(g_characteristic_cache.erase_children(services));
    }

    void erase_le_connection_attributes(std::uint64_t connection)
    {
        erase_le_characteristics_of(g_service_cache.erase_children({ connection }));
    }

    enum class LeOpKind : std::uint8_t
    {
        ServicesDiscover,
        CharacteristicsDiscover,
        DescriptorsDiscover,
        CharacteristicRead,
        CharacteristicWrite,
        CharacteristicWriteWithoutResponse,
        CharacteristicSubscribe,
        DescriptorRead,
        DescriptorWrite,
        AdvertiseStart,
        ServerAddService,
        RssiRead,
    };

    const char* le_op_name(LeOpKind kind)
    {
        switch (kind)
        {
            case LeOpKind::ServicesDiscover:        return "le_services_discover";
            case LeOpKind::CharacteristicsDiscover: return "le_characteristics_discover";
            case LeOpKind::DescriptorsDiscover:     return "le_descriptors_discover";
            case LeOpKind::CharacteristicRead:      return "le_characteristic_read";
            case LeOpKind::CharacteristicWrite:
            case LeOpKind::CharacteristicWriteWithoutResponse:
                return "le_characteristic_write";
            case LeOpKind::CharacteristicSubscribe: return "le_characteristic_subscribe";
            case LeOpKind::DescriptorRead:          return "le_descriptor_read";
            case LeOpKind::DescriptorWrite:         return "le_descriptor_write";
            case LeOpKind::AdvertiseStart:          return "le_advertise_start";
            case LeOpKind::ServerAddService:        return "le_server_add_service";
            case LeOpKind::RssiRead:                return "le_connection_read_rssi";
        }
        return "le_op";
    }

    struct PendingLeOp
    {
        LeOpKind kind = LeOpKind::ServicesDiscover;
        GMFunction callback;
        // The handle the callback reports: connection, service, characteristic
        // or descriptor. Unused by advertise start and add_service.
        std::uint64_t context = 0;
        // The LE connection the op runs on, so a disconnect can fail it; 0 for
        // advertise start and add_service, which belong to no connection.
        std::uint64_t connection = 0;
    };

    // Every LE call that completes asynchronously is registered here under an
    // op id the backend echoes on its completion event. Backends finish these
    // calls in no guaranteed order - across connections, and on Windows not
    // even within one - so call order cannot stand in for identity. An op leaves
    // the registry exactly once: by its completion, by a disconnect or shutdown
    // purge, or by its own call failing synchronously. Whichever comes first
    // fires the callback; anything that arrives later finds no id.
    class PendingLeOps
    {
    public:
        std::uint64_t add(LeOpKind kind, GMFunction callback, std::uint64_t context, std::uint64_t connection)
        {
            const std::uint64_t op_id = next_id_++;
            std::scoped_lock lock(mutex_);
            ops_.emplace(op_id, PendingLeOp{ kind, std::move(callback), context, connection });
            return op_id;
        }

        // For a call that failed synchronously: no completion will arrive.
        void erase(std::uint64_t op_id)
        {
            std::scoped_lock lock(mutex_);
            ops_.erase(op_id);
        }

        std::optional<PendingLeOp> take(std::uint64_t op_id)
        {
            std::scoped_lock lock(mutex_);
            const auto it = ops_.find(op_id);
            if (it == ops_.end())
                return std::nullopt;
            PendingLeOp op = std::move(it->second);
            ops_.erase(it);
            return op;
        }

        // In call order, since the map is ordered by id.
        std::vector<PendingLeOp> take_connection(std::uint64_t connection)
        {
            std::vector<PendingLeOp> out;
            if (connection == 0)
                return out;
            std::scoped_lock lock(mutex_);
            for (auto it = ops_.begin(); it != ops_.end();)
            {
                if (it->second.connection == connection)
                {
                    out.push_back(std::move(it->second));
                    it = ops_.erase(it);
                }
                else
                {
                    ++it;
                }
            }
            return out;
        }

        std::size_t count(LeOpKind kind, std::uint64_t connection)
        {
            std::scoped_lock lock(mutex_);
            std::size_t n = 0;
            for (const auto& [op_id, op] : ops_)
            {
                (void)op_id;
                if (op.kind == kind && op.connection == connection)
                    ++n;
            }
            return n;
        }

        std::vector<PendingLeOp> take_all()
        {
            std::vector<PendingLeOp> out;
            std::scoped_lock lock(mutex_);
            out.reserve(ops_.size());
            for (auto& [op_id, op] : ops_)
            {
                (void)op_id;
                out.push_back(std::move(op));
            }
            ops_.clear();
            return out;
        }

    private:
        std::atomic<std::uint64_t> next_id_{ 1 }; // 0 means "no op"
        std::mutex mutex_;
        std::map<std::uint64_t, PendingLeOp> ops_;
    };

    PendingLeOps g_le_ops;

    // Registers an asynchronous LE call under a fresh op id, asks the backend,
    // and takes the op back out if the backend refused before starting.
    template <typename Start>
    BluetoothError start_le_op(LeOpKind kind, const GMFunction& callback, std::uint64_t context,
        std::uint64_t connection, Start start)
    {
        const auto op_id = g_le_ops.add(kind, callback, context, connection);

        std::string message;
        const Error error = start(op_id, message);
        set_last_error(error, message);

        if (error != Error::Ok)
            g_le_ops.erase(op_id);

        return to_gm(error);
    }

    constexpr std::size_t k_max_pending_writes_without_response = 64;

    bool characteristic_has_property(std::uint64_t characteristic, BluetoothLeCharacteristicProperty property)
    {
        return (g_characteristic_cache.get_properties(characteristic) & static_cast<std::int32_t>(property)) != 0;
    }

    // Fires a taken op's callback with its usual arguments. Called with no lock
    // held, after the op has left the registry. value and size are the read's
    // value id and byte count, number an RSSI read's dBm; 0 for every other op
    // and every failure.
    void fire_le_op(const PendingLeOp& op, Error error, const std::string& message = std::string(),
        std::uint64_t value = 0, std::size_t size = 0, std::int32_t number = 0)
    {
        const char* name = le_op_name(op.kind);
        switch (op.kind)
        {
            case LeOpKind::AdvertiseStart:
                // callback(error_code, message)
                invoke_callback(name, op.callback, static_cast<double>(error), message);
                break;
            case LeOpKind::ServerAddService:
                // callback(error_code, message, service)
                invoke_callback(name, op.callback, static_cast<double>(error), message,
                    static_cast<double>(error == Error::Ok ? op.context : 0));
                break;
            case LeOpKind::CharacteristicRead:
            case LeOpKind::DescriptorRead:
                // callback(error_code, message, handle, value, size)
                invoke_callback(name, op.callback, static_cast<double>(error), message, static_cast<double>(op.context),
                    static_cast<double>(value), static_cast<double>(size));
                break;
            case LeOpKind::RssiRead:
                // callback(error_code, message, connection, rssi)
                invoke_callback(name, op.callback, static_cast<double>(error), message, static_cast<double>(op.context),
                    static_cast<double>(number));
                break;
            default:
                // callback(error_code, message, handle)
                invoke_callback(name, op.callback, static_cast<double>(error), message, static_cast<double>(op.context));
                break;
        }
    }

    // Settles the op a LeOpCompleted event names: caches what discovery
    // returned, or holds a read's bytes under a new value id, then fires its
    // callback. An id
    // the registry no longer holds - its op was purged on disconnect or
    // shutdown - is logged and dropped.
    // The LE server's services, characteristics and descriptors under the
    // handles add_service mints, which the backends key them by (R1-241): a
    // UUID can repeat in a service, a handle cannot. The service UUIDs are kept
    // too, so a second add of one is Busy everywhere (R1-223). An add that
    // fails gives its handles and UUID back; clear, stop and shutdown empty
    // both. Ids keep counting, so a handle is never reused.
    struct LeServerAttribute
    {
        HandleKind kind = HandleKind::LeServerService;
        std::uint64_t parent = 0;
        std::string uuid;
        std::int32_t properties = 0;
        std::vector<std::uint64_t> children;
    };

    std::mutex g_le_server_services_mutex;
    std::unordered_set<std::string> g_le_server_services;
    std::unordered_map<std::uint64_t, std::uint64_t> g_le_server_service_ops;
    std::unordered_map<std::uint64_t, LeServerAttribute> g_le_server_attributes;
    std::uint64_t g_next_le_server_attribute_id = 1;

    void forget_le_server_services()
    {
        std::scoped_lock lock(g_le_server_services_mutex);
        g_le_server_services.clear();
        g_le_server_service_ops.clear();
        g_le_server_attributes.clear();
    }

    void erase_le_server_service_locked(std::uint64_t service)
    {
        const auto it = g_le_server_attributes.find(service);
        if (it == g_le_server_attributes.end())
            return;

        for (const std::uint64_t characteristic : it->second.children)
        {
            if (const auto c = g_le_server_attributes.find(characteristic); c != g_le_server_attributes.end())
            {
                for (const std::uint64_t descriptor : c->second.children)
                    g_le_server_attributes.erase(descriptor);
                g_le_server_attributes.erase(c);
            }
        }
        g_le_server_services.erase(it->second.uuid);
        g_le_server_attributes.erase(it);
    }

    void settle_le_server_service_op(std::uint64_t op_id, bool added)
    {
        std::scoped_lock lock(g_le_server_services_mutex);
        const auto it = g_le_server_service_ops.find(op_id);
        if (it == g_le_server_service_ops.end())
            return;
        if (!added)
            erase_le_server_service_locked(it->second);
        g_le_server_service_ops.erase(it);
    }

    std::uint64_t mint_le_server_attribute_locked(HandleKind kind, std::uint64_t parent, std::string uuid, std::int32_t properties = 0)
    {
        const std::uint64_t handle = make_handle(kind, g_next_le_server_attribute_id++);
        g_le_server_attributes.emplace(handle, LeServerAttribute{ kind, parent, std::move(uuid), properties, {} });
        if (parent != 0)
            g_le_server_attributes[parent].children.push_back(handle);
        return handle;
    }

    std::optional<LeServerAttribute> find_le_server_attribute(std::uint64_t handle, HandleKind kind)
    {
        std::scoped_lock lock(g_le_server_services_mutex);
        const auto it = g_le_server_attributes.find(handle);
        if (it == g_le_server_attributes.end() || it->second.kind != kind)
            return std::nullopt;
        return it->second;
    }

    std::uint64_t le_server_child_at(std::uint64_t parent, HandleKind kind, std::int32_t index)
    {
        const auto attribute = find_le_server_attribute(parent, kind);
        if (!attribute || index < 0 || static_cast<std::size_t>(index) >= attribute->children.size())
            return 0;
        return attribute->children[static_cast<std::size_t>(index)];
    }

    void complete_le_op(const BackendEvent& event)
    {
        const auto op = g_le_ops.take(event.op_id);
        if (!op)
        {
            GMBT_LOG("LE completion for op_id=%llu matches no pending op, dropping",
                static_cast<unsigned long long>(event.op_id));
            g_dropped_events++;
            return;
        }

        if (op->kind == LeOpKind::ServerAddService)
            settle_le_server_service_op(event.op_id, event.error == Error::Ok);

        std::uint64_t value = 0;
        std::size_t size = 0;
        std::int32_t number = 0;
        if (event.error == Error::Ok)
        {
            const LeOpResult& result = event.result;
            switch (op->kind)
            {
                // A discovery's result is the whole level: what it no longer
                // reports is gone, with everything under it (R1-258).
                case LeOpKind::ServicesDiscover:
                    erase_le_characteristics_of(g_service_cache.erase_unreported(op->context, result.attributes));
                    for (const auto& attribute : result.attributes)
                        g_service_cache.find_or_insert(op->context, attribute);
                    break;
                case LeOpKind::CharacteristicsDiscover:
                    // The GATT core properties byte: the bits above it mean
                    // something different on each platform (R1-153).
                    erase_le_descriptors_of(g_characteristic_cache.erase_unreported(op->context, result.attributes));
                    for (const auto& attribute : result.attributes)
                        g_characteristic_cache.find_or_insert(op->context, attribute);
                    break;
                case LeOpKind::DescriptorsDiscover:
                    g_descriptor_cache.erase_unreported(op->context, result.attributes);
                    for (const auto& attribute : result.attributes)
                        g_descriptor_cache.find_or_insert(op->context, attribute);
                    break;
                case LeOpKind::CharacteristicRead:
                case LeOpKind::DescriptorRead:
                    // Nobody would copy a value for an op with no callback.
                    if (op->callback)
                    {
                        size = result.value.size();
                        value = store_value(result.value, ValueOwner::Le);
                    }
                    break;
                case LeOpKind::RssiRead:
                    number = result.number;
                    break;
                default:
                    break;
            }
        }

        fire_le_op(*op, event.error, event.message, value, size, number);
    }

    void fail_le_ops(std::vector<PendingLeOp> ops, Error error, const std::string& message)
    {
        for (const auto& op : ops)
            fire_le_op(op, error, message);
    }

    // Fails every op still waiting on this connection. The link is gone, so no
    // completion will arrive for them; a late one finds no id and is dropped.
    void purge_le_connection(std::uint64_t connection)
    {
        auto ops = g_le_ops.take_connection(connection);
        if (ops.empty())
            return;
        GMBT_LOG("LE connection %llu closed with %zu op(s) pending, failing them",
            static_cast<unsigned long long>(connection), ops.size());
        fail_le_ops(std::move(ops), Error::Disconnected, "LE connection closed before the operation completed");
    }

    // Retires an LE connection whose link is gone: its waiting ops fail, its
    // handle stops validating and what discovery cached for it is dropped.
    void retire_le_connection(std::uint64_t connection)
    {
        if (connection == 0)
            return;
        purge_le_connection(connection);
        g_le_connection_manager.remove_connection(connection);
        erase_le_connection_attributes(connection);
    }

    struct PendingLeServerRequest
    {
        std::uint64_t connection = 0;
        bool is_write = false;
        // False only for a write without response, which the backend has
        // already completed; the entry is kept for GML to fetch its value.
        bool response_needed = true;
        // A write's value id: the value goes with the request.
        std::uint64_t value = 0;
        std::chrono::steady_clock::time_point received_at = std::chrono::steady_clock::now();
    };

    std::mutex g_pending_le_server_requests_mutex;
    std::unordered_map<std::int32_t, PendingLeServerRequest> g_pending_le_server_requests;

    // ATT error codes the core answers with when GML does not.
    constexpr std::int32_t k_att_request_not_supported = 0x06;
    constexpr std::int32_t k_att_attribute_not_found = 0x0A;
    constexpr std::int32_t k_att_unlikely_error = 0x0E;

    // The ATT transaction timeout is 30 s: a request still waiting after that
    // has already cost the central its link. A write without response is kept
    // only long enough for GML to read its value, as on Android.
    constexpr auto k_le_server_request_ttl = std::chrono::seconds(30);
    constexpr auto k_le_server_no_response_write_ttl = std::chrono::seconds(5);

    // Answers a request through the backend without GML - a default, an expiry
    // or a stop - so the backend's own request object is completed too.
    void answer_le_server_request(std::int32_t request_id, bool is_write, std::int32_t att_error)
    {
        if (!g_backend)
            return;

        std::string message;
        const Error error = is_write
            ? g_backend->le_server_respond_write(request_id, att_error, message)
            : g_backend->le_server_respond_read(request_id, att_error, {}, message);
        if (error != Error::Ok)
            GMBT_LOG("LE server request %d could not be answered (0x%02x): %s", request_id, att_error, message.c_str());
    }

    // Drops the requests past their lifetime, answering those a central still
    // waits on. Runs on each new request, so nothing piles up while a server
    // runs; the answers are sent with no lock held.
    void expire_le_server_requests()
    {
        const auto now = std::chrono::steady_clock::now();
        std::vector<std::pair<std::int32_t, bool>> to_answer;
        std::vector<std::uint64_t> values;
        {
            std::scoped_lock lock(g_pending_le_server_requests_mutex);
            for (auto it = g_pending_le_server_requests.begin(); it != g_pending_le_server_requests.end();)
            {
                const auto& request = it->second;
                const auto ttl = request.response_needed ? k_le_server_request_ttl : k_le_server_no_response_write_ttl;
                if (now - request.received_at < ttl)
                {
                    ++it;
                    continue;
                }
                if (request.response_needed)
                    to_answer.emplace_back(it->first, request.is_write);
                values.push_back(request.value);
                it = g_pending_le_server_requests.erase(it);
            }
        }

        for (const std::uint64_t value : values)
            release_value(value);

        for (const auto& [request_id, is_write] : to_answer)
        {
            GMBT_LOG("LE server request %d was never answered, expiring it", request_id);
            answer_le_server_request(request_id, is_write, k_att_unlikely_error);
        }
    }

    // Answers every request still waiting, before the server stops or drops
    // its services and the backend forgets them.
    void answer_pending_le_server_requests()
    {
        std::unordered_map<std::int32_t, PendingLeServerRequest> pending;
        {
            std::scoped_lock lock(g_pending_le_server_requests_mutex);
            pending.swap(g_pending_le_server_requests);
        }

        for (const auto& [request_id, request] : pending)
        {
            release_value(request.value);
            if (request.response_needed)
                answer_le_server_request(request_id, request.is_write, k_att_unlikely_error);
        }
    }

    // The pre-flight for respond_*: the id names a waiting request of the
    // kind being answered. Nothing is consumed, so a bad call can be retried.
    // Only the declared BluetoothAttError members go on the air: every
    // platform sends the code as the ATT error byte.
    bool check_att_error(BluetoothAttError error_code, const char* function_name)
    {
        const auto value = static_cast<std::int32_t>(error_code);
        if (value >= static_cast<std::int32_t>(BluetoothAttError::Success) &&
            value <= static_cast<std::int32_t>(BluetoothAttError::InsufficientResources))
            return true;

        fail(Error::InvalidArgument, std::string(function_name) + ": error_code " + std::to_string(value) +
            " is not a BluetoothAttError member");
        return false;
    }

    // The waiting request an id names; end(), with the last error set, when it
    // names none. Caller holds g_pending_le_server_requests_mutex.
    auto find_le_server_request_locked(std::int32_t request_id)
    {
        const auto it = g_pending_le_server_requests.find(request_id);
        if (it == g_pending_le_server_requests.end())
            fail(Error::InvalidHandle, "Unknown or expired LE server request id " + std::to_string(request_id));
        return it;
    }

    bool check_le_server_request(std::int32_t request_id, bool is_write, const char* other_function)
    {
        std::scoped_lock lock(g_pending_le_server_requests_mutex);
        const auto it = find_le_server_request_locked(request_id);
        if (it == g_pending_le_server_requests.end())
            return false;
        if (it->second.is_write != is_write)
        {
            fail(Error::InvalidArgument, "LE server request " + std::to_string(request_id) + " is a " +
                (it->second.is_write ? "write" : "read") + " request; answer it with " + other_function);
            return false;
        }
        return true;
    }

    // Removes a checked request; empty if the expiry sweep took it meanwhile.
    std::optional<PendingLeServerRequest> take_le_server_request(std::int32_t request_id)
    {
        std::scoped_lock lock(g_pending_le_server_requests_mutex);
        const auto it = find_le_server_request_locked(request_id);
        if (it == g_pending_le_server_requests.end())
            return std::nullopt;
        PendingLeServerRequest request = std::move(it->second);
        g_pending_le_server_requests.erase(it);
        return request;
    }

    // A server request event's attribute: the characteristic named by its id
    // and, for a descriptor request, the descriptor named by its id under it.
    // False for an id the server does not hold.
    bool le_server_request_attribute(const LeAttributeRef& attribute, std::uint64_t& characteristic, std::uint64_t& descriptor)
    {
        characteristic = attribute.characteristic;
        descriptor = attribute.descriptor;

        const auto c = find_le_server_attribute(characteristic, HandleKind::LeServerCharacteristic);
        if (!c)
            return false;
        if (descriptor == 0)
            return true;
        const auto d = find_le_server_attribute(descriptor, HandleKind::LeServerDescriptor);
        return d && d->parent == characteristic;
    }

    // A remote central using our GATT server, under the key its backend names
    // it by (Windows: the session's device id; Apple: the central's identifier).
    // connection is minted from the LE connection handle space, so it never
    // equals a client connection, and is retired on the central's disconnect
    // (R1-49): a later visit gets a new one.
    struct ServerCentral
    {
        std::uint64_t connection = 0;
        std::uint64_t device = 0;
    };

    std::mutex g_server_centrals_mutex;
    std::unordered_map<std::string, ServerCentral> g_server_centrals;

    void fire_le_server_connection_state(std::uint64_t connection, bool connected, std::uint64_t device)
    {
        // callback(connection, connected, device)
        invoke_callback("le_server_connection_state_changed", event_callback(g_callback_le_server_connection_state_changed),
            static_cast<double>(connection), connected, static_cast<double>(device));
    }

    // The server connection for a central key, minted on its first event, which
    // is also when GML hears it connected. 0 for an empty key, from a backend
    // that names no central.
    std::uint64_t note_server_central(const std::string& key, std::uint64_t device)
    {
        if (key.empty())
            return 0;

        ServerCentral central;
        {
            std::scoped_lock lock(g_server_centrals_mutex);
            const auto it = g_server_centrals.find(key);
            if (it != g_server_centrals.end())
                return it->second.connection;

            central.connection = g_le_connection_manager.reserve_handle();
            central.device = device;
            g_server_centrals.emplace(key, central);
        }

        fire_le_server_connection_state(central.connection, true, central.device);
        return central.connection;
    }

    void retire_server_central(const std::string& key)
    {
        ServerCentral central;
        {
            std::scoped_lock lock(g_server_centrals_mutex);
            const auto it = g_server_centrals.find(key);
            if (it == g_server_centrals.end())
                return;
            central = it->second;
            g_server_centrals.erase(it);
        }

        fire_le_server_connection_state(central.connection, false, central.device);
    }

    // Every central leaves with the server: each one GML heard connect hears
    // the disconnect.
    void retire_server_centrals()
    {
        std::unordered_map<std::string, ServerCentral> centrals;
        {
            std::scoped_lock lock(g_server_centrals_mutex);
            centrals.swap(g_server_centrals);
        }

        for (const auto& [key, central] : centrals)
        {
            (void)key;
            fire_le_server_connection_state(central.connection, false, central.device);
        }
    }

    // The key a server connection handle names; empty when it names none.
    std::string server_central_key(std::uint64_t connection)
    {
        std::scoped_lock lock(g_server_centrals_mutex);
        for (const auto& [key, central] : g_server_centrals)
        {
            if (central.connection == connection)
                return key;
        }
        return std::string();
    }

    // The device a server event's central is: device_id in the backend's own
    // id scheme, so the central and a scan result for the same peer are one
    // device, and the address only where the platform has one. Added without
    // device_found (R1-155); 0 when the event names no central.
    std::uint64_t le_server_event_device(const LeServerCentral& central)
    {
        const std::string& device_id = central.device_id.empty() ? central.address : central.device_id;
        if (device_id.empty())
            return 0;

        DiscoveredDevice d;
        d.transport = Transport::LowEnergy;
        d.id = device_id;
        d.address = central.address;
        d.address_available = !central.address.empty();
        return g_device_manager.upsert_device(d);
    }

    constexpr std::string_view k_cccd_uuid = "00002902-0000-1000-8000-00805f9b34fb";

    // Mints the service's handles in definition order and gives each to its
    // backend as the id it keys the attribute by. The CCCD belongs to the
    // platform: it gets no handle and is left out, as every backend skips it.
    LeServerService register_le_server_service_locked(const BluetoothLeServiceDefinition& service)
    {
        LeServerService out;
        out.id = mint_le_server_attribute_locked(HandleKind::LeServerService, 0, canonical_uuid(service.uuid));
        out.uuid = service.uuid;
        out.characteristics.reserve(service.characteristics.size());

        for (const auto& characteristic : service.characteristics)
        {
            LeServerCharacteristic entry;
            entry.id = mint_le_server_attribute_locked(HandleKind::LeServerCharacteristic,
                out.id, canonical_uuid(characteristic.uuid), characteristic.properties & 0xFF);
            entry.uuid = characteristic.uuid;
            entry.properties = characteristic.properties;
            entry.permissions = characteristic.permissions;
            entry.value = characteristic.value;

            for (const auto& descriptor : characteristic.descriptors)
            {
                const std::string descriptor_uuid = canonical_uuid(descriptor.uuid);
                if (descriptor_uuid == k_cccd_uuid)
                    continue;

                LeServerDescriptor d;
                d.id = mint_le_server_attribute_locked(HandleKind::LeServerDescriptor, entry.id, descriptor_uuid);
                d.uuid = descriptor.uuid;
                entry.descriptors.push_back(std::move(d));
            }

            out.characteristics.push_back(std::move(entry));
        }

        return out;
    }

    // Sends the current known state to the state callback, if one is set.
    // GMFunction::call queues safely into GameMaker's dispatcher.
    void dispatch_current_state()
    {
        // GMFunction::call only queues, so the dispatch lock is safe to hold
        // across it; it keeps a state event from overtaking this one.
        std::scoped_lock dispatch_lock(g_state_dispatch_mutex);
        const GMFunction callback = registered_callback(g_callback_state_changed);
        const std::int32_t state = g_backend
            ? g_backend->current_bluetooth_state()
            : 0; // BluetoothState.Unknown
        invoke_callback("current state_changed", callback, static_cast<double>(state));
    }

    void dispatch_state_changed(const BackendEvent& event)
    {
        std::scoped_lock dispatch_lock(g_state_dispatch_mutex);
        invoke_callback("state_changed", event_callback(g_callback_state_changed), static_cast<double>(event.value));
    }

    void dispatch_le_connect_result(const BackendEvent& event)
    {
        const std::uint64_t connection = event.connection;
        const GMFunction callback = g_le_connection_manager.take_connect_callback(connection);
        if (!callback)
        {
            g_dropped_events++;
            return;
        }
        const bool failed = event.error != Error::Ok;
        const std::string message = failed ? event.message : std::string();
        const std::uint64_t device = g_le_connection_manager.get_device(connection);
        if (failed)
        {
            // The link never came up: nothing queued on it can complete.
            retire_le_connection(connection);
        }
        // callback(error_code, message, connection, device)
        invoke_callback("le_connect", callback, static_cast<double>(event.error), message, static_cast<double>(connection),
            static_cast<double>(device));
    }

    void dispatch_le_disconnected(const BackendEvent& event)
    {
        // 0 only if the peripheral was never connected through
        // bluetooth_le_connect. A connection already retired - by
        // bluetooth_le_disconnect, or by the other of the two reports Apple
        // can make for one drop - has had its disconnect.
        const std::uint64_t connection = event.connection;
        if (!g_le_connection_manager.is_valid(connection))
        {
            g_dropped_events++;
            return;
        }
        retire_le_connection(connection);

        const Error error = event.error != Error::Ok ? event.error : Error::Disconnected;
        // callback(error_code, message, connection)
        invoke_callback("le_disconnected", event_callback(g_callback_le_disconnected),
            static_cast<double>(error), event.message, static_cast<double>(connection));
    }

    void dispatch_le_value_changed(BackendEvent& event)
    {
        const std::uint64_t connection = event.connection;
        const std::uint64_t service_instance = event.attribute.service;
        const std::uint64_t characteristic_instance = event.attribute.characteristic;

        const std::uint64_t service = service_instance ? g_service_cache.find_by_instance(connection, service_instance) : 0;
        const std::uint64_t characteristic = service && characteristic_instance
            ? g_characteristic_cache.find_by_instance(service, characteristic_instance)
            : 0;
        if (!characteristic)
        {
            GMBT_TRACE("LE characteristic_value_changed for unknown characteristic %llu (connection=%llu), dropping",
                static_cast<unsigned long long>(characteristic_instance), static_cast<unsigned long long>(connection));
            g_dropped_events++;
            return;
        }

        const GMFunction callback = event_callback(g_callback_le_characteristic_value_changed);
        if (!callback)
            return;

        const std::size_t size = event.bytes.size();
        const std::uint64_t value = store_value(std::move(event.bytes), ValueOwner::Le);

        // callback(characteristic, connection, value, size)
        invoke_callback("le_characteristic_value_changed", callback, static_cast<double>(characteristic),
            static_cast<double>(connection), static_cast<double>(value), static_cast<double>(size));
    }

    // A backend reports a central's arrival where it sees one (a
    // subscription) and its departure where the platform has one; the first
    // event of any kind from a central is its connect.
    void dispatch_le_server_central(const BackendEvent& event)
    {
        if (event.connected)
            note_server_central(event.central.key, le_server_event_device(event.central));
        else
            retire_server_central(event.central.key);
    }

    void dispatch_le_server_read_request(const BackendEvent& event)
    {
        const std::int32_t request_id = event.request_id;
        std::uint64_t characteristic = 0;
        std::uint64_t descriptor = 0;
        if (!le_server_request_attribute(event.attribute, characteristic, descriptor))
        {
            GMBT_LOG("LE server read request %d names no attribute the server holds, answering it", request_id);
            answer_le_server_request(request_id, false, k_att_attribute_not_found);
            g_dropped_events++;
            return;
        }
        const std::int32_t offset = static_cast<std::int32_t>(event.offset);
        const std::uint64_t connection = note_server_central(event.central.key, le_server_event_device(event.central));

        const GMFunction callback = event_callback(g_callback_le_server_read_request);
        if (!callback)
        {
            // Nobody will answer it, so the central hears now instead of
            // waiting out the ATT timeout and dropping the link.
            answer_le_server_request(request_id, false, k_att_request_not_supported);
            return;
        }

        expire_le_server_requests();
        {
            std::scoped_lock lock(g_pending_le_server_requests_mutex);
            PendingLeServerRequest request;
            request.connection = connection;
            request.is_write = false;
            g_pending_le_server_requests[request_id] = std::move(request);
        }

        // callback(request_id, connection, characteristic, descriptor_or_0, offset)
        // GML answers with the value's bytes from offset on, as on Android.
        invoke_callback("le_server_read_request", callback, static_cast<double>(request_id), static_cast<double>(connection),
            static_cast<double>(characteristic), static_cast<double>(descriptor), static_cast<double>(offset));
    }

    void dispatch_le_server_write_request(BackendEvent& event)
    {
        const std::int32_t request_id = event.request_id;
        const bool response_needed = event.response_needed;
        std::uint64_t characteristic = 0;
        std::uint64_t descriptor = 0;
        if (!le_server_request_attribute(event.attribute, characteristic, descriptor))
        {
            GMBT_LOG("LE server write request %d names no attribute the server holds, answering it", request_id);
            if (response_needed)
                answer_le_server_request(request_id, true, k_att_attribute_not_found);
            g_dropped_events++;
            return;
        }
        const std::int32_t offset = static_cast<std::int32_t>(event.offset);
        const std::uint64_t connection = note_server_central(event.central.key, le_server_event_device(event.central));

        const GMFunction callback = event_callback(g_callback_le_server_write_request);
        if (!callback)
        {
            // As for a read; a write without response needs no answer.
            if (response_needed)
                answer_le_server_request(request_id, true, k_att_request_not_supported);
            return;
        }

        expire_le_server_requests();
        const std::size_t size = event.bytes.size();
        const std::uint64_t value_id = store_value(std::move(event.bytes), ValueOwner::ServerWrite);
        {
            std::scoped_lock lock(g_pending_le_server_requests_mutex);
            PendingLeServerRequest request;
            request.connection = connection;
            request.is_write = true;
            request.response_needed = response_needed;
            request.value = value_id;
            g_pending_le_server_requests[request_id] = std::move(request);
        }

        // callback(request_id, connection, characteristic, descriptor_or_0, offset, response_needed, value, size)
        invoke_callback("le_server_write_request", callback, static_cast<double>(request_id), static_cast<double>(connection),
            static_cast<double>(characteristic), static_cast<double>(descriptor), static_cast<double>(offset), response_needed,
            static_cast<double>(value_id), static_cast<double>(size));
    }

    void dispatch_scan_stopped(const BackendEvent& event)
    {
        // callback(error_code, message, transport)
        invoke_callback("scan_stopped", event_callback(g_callback_scan_stopped),
            static_cast<double>(event.error), event.message, static_cast<double>(event.transport));
    }

    // The connect completion callback is one-shot and per-connection, not
    // one of the persistently-registered callbacks.
    void dispatch_classic_connect_result(const BackendEvent& event)
    {
        // The core knows which device the handle was made for; a backend
        // that leaves the event's device 0 still reports it (R1-75).
        const std::uint64_t device = event.device
            ? event.device
            : g_classic_connection_manager.get_device(event.connection);

        // A connect that failed leaves nothing behind its handle.
        if (event.error != Error::Ok)
            g_classic_connection_manager.remove_connection(event.connection);

        const GMFunction callback = g_classic_connection_manager.take_connect_callback(event.connection);
        if (!callback)
        {
            g_dropped_events++;
            return;
        }
        // callback(error_code, message, connection, device)
        invoke_callback("classic_connect", callback, static_cast<double>(event.error), event.message,
            static_cast<double>(event.connection), static_cast<double>(device));
    }

    void dispatch_classic_client_connected(const BackendEvent& event)
    {
        // callback(connection, device)
        invoke_callback("classic_client_connected", event_callback(g_callback_classic_client_connected),
            static_cast<double>(event.connection), static_cast<double>(event.device));
    }

    // A handle bluetooth_classic_disconnect already retired has had its end:
    // the backend's late report of that close fires nothing, the same as for
    // LE (R1-77), and its late bytes go back to the receive budget.
    void dispatch_classic_data(BackendEvent& event)
    {
        const bool valid = g_classic_connection_manager.is_valid(event.connection);
        const GMFunction callback = valid ? event_callback(g_callback_classic_data) : GMFunction();
        if (!callback)
        {
            if (!valid)
                g_dropped_events++;
            if (g_backend)
                g_backend->classic_receive_consumed(event.connection, event.bytes.size());
            return;
        }

        const std::size_t size = event.bytes.size();
        const std::uint64_t value = store_value(std::move(event.bytes), ValueOwner::Classic, event.connection);
        // callback(connection, value, size)
        invoke_callback("classic_data", callback, static_cast<double>(event.connection), static_cast<double>(value),
            static_cast<double>(size));
    }

    void dispatch_classic_disconnected(const BackendEvent& event)
    {
        if (!g_classic_connection_manager.is_valid(event.connection))
        {
            g_dropped_events++;
            return;
        }

        // Every byte reached GML as a value before this, so the handle is
        // retired here, as an LE link is (R1-245).
        g_classic_connection_manager.remove_connection(event.connection);

        // callback(error_code, message, connection)
        invoke_callback("classic_disconnected", event_callback(g_callback_classic_disconnected),
            static_cast<double>(event.error), event.message, static_cast<double>(event.connection));
    }

    // Same one-shot idea as a connect, but keyed by device handle since
    // pairing never creates a connection.
    void dispatch_device_paired(const BackendEvent& event)
    {
        GMFunction callback;
        {
            std::scoped_lock lock(g_pending_pair_mutex);
            const auto it = g_pending_pair_callbacks.find(event.device);
            if (it != g_pending_pair_callbacks.end())
            {
                callback = it->second;
                g_pending_pair_callbacks.erase(it);
            }
        }
        if (!callback)
        {
            g_dropped_events++;
            return;
        }
        // callback(error_code, message, device)
        invoke_callback("pair", callback, static_cast<double>(event.error), event.message, static_cast<double>(event.device));
    }

    // The devices a query found enter the cache without device_found, like a
    // server central, and the callback gets their handles.
    void dispatch_devices_queried(const BackendEvent& event)
    {
        GMFunction callback;
        {
            std::scoped_lock lock(g_pending_query_mutex);
            const auto it = g_pending_query_callbacks.find(event.op_id);
            if (it == g_pending_query_callbacks.end())
            {
                GMBT_LOG("device query %llu answered after shutdown, dropping",
                    static_cast<unsigned long long>(event.op_id));
                g_dropped_events++;
                return;
            }
            callback = it->second;
            g_pending_query_callbacks.erase(it);
        }

        std::vector<double> devices;
        if (event.error == Error::Ok)
        {
            for (const auto& device : event.devices)
            {
                const double handle = static_cast<double>(g_device_manager.upsert_device(device));
                if (std::find(devices.begin(), devices.end(), handle) == devices.end())
                    devices.push_back(handle);
            }
        }
        fire_query_callback(callback, event.error, event.message, devices);
    }

    CoreHooks create_core_hooks()
    {
        CoreHooks hooks;
        hooks.upsert_device = [](const DiscoveredDevice& device) {
            // Only LE scans report LE devices through this hook, so the scan's
            // filters decide whether a new one enters the cache; one already in
            // it keeps updating. 0 tells the scan path the device was held.
            std::optional<DiscoveredDevice> admitted;
            if (device.transport == Transport::LowEnergy && g_device_manager.find_id(device.id) == 0)
            {
                admitted = g_scan_filters.admit(device);
                if (!admitted)
                    return std::uint64_t{ 0 };
            }

            bool created = false;
            const std::uint64_t handle = g_device_manager.upsert_device(admitted ? *admitted : device, &created);
            if (!created)
                return handle;

            GMBT_LOG("device added: handle=%llu transport=%d id='%s' name='%s' rssi=%d (available=%d) connectable=%d",
                static_cast<unsigned long long>(handle),
                static_cast<int>(device.transport),
                device.id.c_str(),
                device.name.c_str(),
                device.rssi,
                device.rssi_available ? 1 : 0,
                device.connectable ? 1 : 0);

            invoke_callback("device_found", event_callback(g_callback_device_found), static_cast<double>(handle));
            return handle;
        };
        hooks.create_classic_connection = [](std::uint64_t device) {
            const std::uint64_t connection = g_classic_connection_manager.create_connection(device);
            GMBT_LOG("classic connection created: connection=%llu device=%llu",
                static_cast<unsigned long long>(connection),
                static_cast<unsigned long long>(device));
            return connection;
        };
        hooks.create_le_connection = [](std::uint64_t device) {
            const std::uint64_t connection = g_le_connection_manager.create_connection(device);
            GMBT_LOG("LE connection created: connection=%llu device=%llu",
                static_cast<unsigned long long>(connection),
                static_cast<unsigned long long>(device));
            return connection;
        };
        hooks.push_event = [](BackendEvent event) {
            GMBT_TRACE("event: type=%d transport=%d",
                static_cast<int>(event.type),
                static_cast<int>(event.transport));

            switch (event.type)
            {
                case BackendEventType::ScanStopped:
                    dispatch_scan_stopped(event);
                    return;
                case BackendEventType::ClassicConnected:
                    dispatch_classic_connect_result(event);
                    return;
                case BackendEventType::ClassicClientConnected:
                    dispatch_classic_client_connected(event);
                    return;
                case BackendEventType::ClassicDataAvailable:
                    dispatch_classic_data(event);
                    return;
                case BackendEventType::ClassicDisconnected:
                    dispatch_classic_disconnected(event);
                    return;
                case BackendEventType::ClassicReceivePaused:
                    expire_values();
                    return;
                case BackendEventType::StateChanged:
                    dispatch_state_changed(event);
                    return;
                case BackendEventType::LeConnectResult:
                    dispatch_le_connect_result(event);
                    return;
                case BackendEventType::LeDisconnected:
                    dispatch_le_disconnected(event);
                    return;
                case BackendEventType::LeValueChanged:
                    dispatch_le_value_changed(event);
                    return;
                case BackendEventType::LeServerCentral:
                    dispatch_le_server_central(event);
                    return;
                case BackendEventType::LeServerReadRequest:
                    dispatch_le_server_read_request(event);
                    return;
                case BackendEventType::LeServerWriteRequest:
                    dispatch_le_server_write_request(event);
                    return;
                case BackendEventType::LeServerServicesReset:
                    // The platform dropped the server's services and the game
                    // adds them again, so none of them is a duplicate now.
                    forget_le_server_services();
                    return;
                case BackendEventType::LeOpCompleted:
                    complete_le_op(event);
                    return;
                case BackendEventType::DevicePaired:
                    dispatch_device_paired(event);
                    return;
                case BackendEventType::PermissionResult:
                    fire_permission_callbacks(Error::Ok, std::string(), static_cast<PermissionStatus>(event.value));
                    return;
                case BackendEventType::EnableResult:
                    fire_enable_callbacks(event.error, event.message);
                    return;
                case BackendEventType::DevicesQueried:
                    dispatch_devices_queried(event);
                    return;
            }
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
        fail(Error::OperationFailed, "Failed to create platform backend");
        GMBT_LOG("FAILED: no platform backend for this build");
        return false;
    }

    std::string message;
    const Error error = g_backend->initialize(message);
    set_last_error(error, message);

    GMBT_LOG("backend initialize() -> error=%d message='%s' | ble=%d le_advertise=%d le_server=%d classic=%d classic_server=%d",
        static_cast<int>(error),
        message.c_str(),
        g_backend->supports_ble() ? 1 : 0,
        g_backend->supports_le_advertise() ? 1 : 0,
        g_backend->supports_le_server() ? 1 : 0,
        g_backend->supports_classic() ? 1 : 0,
        g_backend->supports_classic_server() ? 1 : 0);

    // A backend whose initialize failed is let go, so bluetooth_is_initialized
    // says false and a retry really retries (R1-52). The error stays set.
    if (error != Error::Ok)
    {
        g_backend->shutdown();
        g_backend.reset();
        return false;
    }

    // A callback set before this heard Unknown (R1-259).
    dispatch_current_state();
    return true;
}

void bluetooth_shutdown()
{
    GMBT_LOG("shutting down (backend=%s, devices cached=%d, events dropped=%llu)",
        g_backend ? "present" : "null",
        g_device_manager.get_count(),
        static_cast<unsigned long long>(g_dropped_events.load()));

    // The backend goes first, so no event arrives while the state below is
    // failed and cleared; the server requests it holds are answered before.
    if (g_backend)
    {
        answer_pending_le_server_requests();
        g_backend->shutdown();
        g_backend.reset();
    }

    // Every one-shot callback still waiting is failed once, with its usual
    // arguments; the registered event callbacks stay (R1-64).
    const std::string message = "Bluetooth was shut down before the operation completed";
    fail_le_ops(g_le_ops.take_all(), Error::NotInitialized, message);

    // callback(error_code, message, connection, device)
    for (const auto& [connection, callback] : g_classic_connection_manager.take_connect_callbacks())
        invoke_callback("classic_connect", callback, static_cast<double>(Error::NotInitialized), message,
            static_cast<double>(connection), static_cast<double>(g_classic_connection_manager.get_device(connection)));
    for (const auto& [connection, callback] : g_le_connection_manager.take_connect_callbacks())
        invoke_callback("le_connect", callback, static_cast<double>(Error::NotInitialized), message,
            static_cast<double>(connection), static_cast<double>(g_le_connection_manager.get_device(connection)));

    std::unordered_map<std::uint64_t, GMFunction> pair_callbacks;
    { std::scoped_lock lock(g_pending_pair_mutex); pair_callbacks.swap(g_pending_pair_callbacks); }
    // callback(error_code, message, device)
    for (const auto& [device, callback] : pair_callbacks)
        invoke_callback("pair", callback, static_cast<double>(Error::NotInitialized), message, static_cast<double>(device));

    fire_permission_callbacks(Error::NotInitialized, message, PermissionStatus::Unknown);
    fire_enable_callbacks(Error::NotInitialized, message);

    std::map<std::uint64_t, GMFunction> query_callbacks;
    { std::scoped_lock lock(g_pending_query_mutex); query_callbacks.swap(g_pending_query_callbacks); }
    for (const auto& [query, callback] : query_callbacks)
        fire_query_callback(callback, Error::NotInitialized, message, {});

    g_scan_filters.start({});

    {
        std::scoped_lock lock(g_pending_le_server_requests_mutex);
        g_pending_le_server_requests.clear();
    }
    {
        std::scoped_lock lock(g_server_centrals_mutex);
        g_server_centrals.clear();
    }
    forget_le_server_services();
    g_values.clear();
    g_descriptor_cache.clear();
    g_characteristic_cache.clear();
    g_service_cache.clear();
    g_le_connection_manager.clear();
    g_classic_connection_manager.clear();
    g_device_manager.clear();
}

bool bluetooth_is_initialized()
{
    return g_backend != nullptr;
}

BluetoothError bluetooth_last_error_code()
{
    return to_gm(g_last_error);
}

std::string bluetooth_last_error_message()
{
    return g_last_error_message;
}

bool bluetooth_le_is_supported()
{
    return g_backend && g_backend->supports_ble();
}

bool bluetooth_le_advertise_is_supported()
{
    return g_backend && g_backend->supports_le_advertise();
}

bool bluetooth_le_server_is_supported()
{
    return g_backend && g_backend->supports_le_server();
}

bool bluetooth_classic_is_supported()
{
    return g_backend && g_backend->supports_classic();
}

bool bluetooth_classic_server_is_supported()
{
    return g_backend && g_backend->supports_classic_server();
}

bool bluetooth_feature_is_supported(BluetoothFeature feature)
{
    if (!g_backend)
        return false;

    // The five with their own query answer the same here; the backend knows
    // the rest (R1-79).
    switch (feature)
    {
        case BluetoothFeature::LeCentral: return g_backend->supports_ble();
        case BluetoothFeature::LeAdvertise: return g_backend->supports_le_advertise();
        case BluetoothFeature::LeServer: return g_backend->supports_le_server();
        case BluetoothFeature::Classic: return g_backend->supports_classic();
        case BluetoothFeature::ClassicServer: return g_backend->supports_classic_server();
        default: return g_backend->feature_supported(static_cast<std::int32_t>(feature));
    }
}

BluetoothPermissionStatus bluetooth_permission_get_status()
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

    return static_cast<BluetoothPermissionStatus>(status);
}

BluetoothError bluetooth_permission_request(const gm::wire::GMFunction& callback)
{
    if (!require_backend())
    {
        GMBT_LOG("permission request rejected: backend is not initialized");
        return to_gm(g_last_error);
    }

    // Registered before the backend is asked, which may answer at once; taken
    // back out on a pre-flight failure, which never fires it.
    std::uint64_t request = 0;
    {
        std::scoped_lock lock(g_pending_permission_mutex);
        request = g_next_permission_request++;
        g_pending_permission_callbacks.emplace(request, callback);
    }

    std::string message;
    const Error error = g_backend->permission_request(message);
    set_last_error(error, message);
    if (error != Error::Ok)
    {
        std::scoped_lock lock(g_pending_permission_mutex);
        g_pending_permission_callbacks.erase(request);
    }
    GMBT_LOG("permission request -> error=%d message='%s' status now %d",
        static_cast<int>(error),
        message.c_str(),
        static_cast<int>(g_backend->permission_status()));
    return to_gm(error);
}

BluetoothError bluetooth_request_enable(const gm::wire::GMFunction& callback)
{
    if (!require_backend())
        return to_gm(g_last_error);

    if (!check_feature(BluetoothFeature::RequestEnable, "This platform cannot turn the Bluetooth radio on"))
        return to_gm(g_last_error);

    // Already on: the answer is known, so it fires at once (R1-143).
    if (g_backend->current_bluetooth_state() == 5) // PoweredOn
    {
        // callback(error_code, message)
        invoke_callback("request_enable", callback, static_cast<double>(Error::Ok), std::string());
        return to_gm(Error::Ok);
    }

    // One request at a time: a second one waits for the same answer.
    bool first = false;
    {
        std::scoped_lock lock(g_pending_enable_mutex);
        first = g_pending_enable_callbacks.empty();
        g_pending_enable_callbacks.push_back(callback);
    }
    if (!first)
        return to_gm(Error::Ok);

    std::string message;
    const Error error = g_backend->request_enable(message);
    set_last_error(error, message);
    if (error != Error::Ok)
    {
        std::scoped_lock lock(g_pending_enable_mutex);
        g_pending_enable_callbacks.clear();
    }
    return to_gm(error);
}

BluetoothError bluetooth_le_scan_start(bool active, const std::vector<BluetoothLeScanFilter>& filters)
{
    GMBT_LOG("BLE scan start requested (active=%d, filters=%zu)", active ? 1 : 0, filters.size());

    if (!require_backend())
    {
        GMBT_LOG("BLE scan start rejected: backend is not initialized");
        return to_gm(g_last_error);
    }

    std::vector<LeScanFilter> checked;
    checked.reserve(filters.size());
    for (const auto& filter : filters)
    {
        if (!filter.service_uuid && !filter.name && !filter.company_id)
            return fail(Error::InvalidArgument, "A scan filter must set service_uuid, name or company_id");

        LeScanFilter out;
        if (filter.service_uuid)
        {
            if (!check_uuid(*filter.service_uuid))
                return to_gm(g_last_error);
            out.service_uuid = canonical_uuid(*filter.service_uuid);
        }
        out.name = filter.name;
        if (filter.company_id)
        {
            if (*filter.company_id < 0 || *filter.company_id > 0xFFFF)
                return fail(Error::InvalidArgument, "A scan filter's company_id must be 0-65535");
            out.company_id = static_cast<std::uint16_t>(*filter.company_id);
        }
        checked.push_back(std::move(out));
    }

    if (!check_radio())
        return to_gm(g_last_error);

    // A start while scanning starts nothing new and keeps that scan's filters.
    if (g_backend->le_scan_is_running())
    {
        GMBT_LOG("BLE scan start: already running, filters unchanged");
        return to_gm(Error::Ok);
    }

    // In place before the backend starts: results can arrive before it returns.
    g_scan_filters.start(checked);

    std::string message;
    const Error error = g_backend->le_scan_start(active, checked, message);
    set_last_error(error, message);
    if (error != Error::Ok)
        g_scan_filters.start({});
    GMBT_LOG("BLE scan start -> error=%d message='%s' | backend reports running=%d",
        static_cast<int>(error),
        message.c_str(),
        g_backend->le_scan_is_running() ? 1 : 0);
    return to_gm(error);
}

BluetoothError bluetooth_le_scan_stop()
{
    GMBT_LOG("BLE scan stop requested");

    if (!require_backend())
    {
        GMBT_LOG("BLE scan stop rejected: backend is not initialized");
        return to_gm(g_last_error);
    }

    std::string message;
    const Error error = g_backend->le_scan_stop(message);
    set_last_error(error, message);
    GMBT_LOG("BLE scan stop -> error=%d message='%s' | backend reports running=%d",
        static_cast<int>(error),
        message.c_str(),
        g_backend->le_scan_is_running() ? 1 : 0);
    return to_gm(error);
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

BluetoothError bluetooth_classic_scan_start()
{
    if (!require_backend())
        return to_gm(g_last_error);

    if (!check_radio())
        return to_gm(g_last_error);

    std::string message;
    const Error error = g_backend->classic_scan_start(message);
    set_last_error(error, message);
    return to_gm(error);
}

BluetoothError bluetooth_classic_scan_stop()
{
    if (!require_backend())
        return to_gm(g_last_error);

    std::string message;
    const Error error = g_backend->classic_scan_stop(message);
    set_last_error(error, message);
    return to_gm(error);
}

bool bluetooth_classic_scan_is_running()
{
    return g_backend && g_backend->classic_scan_is_running();
}

void bluetooth_device_clear()
{
    GMBT_LOG("clearing device cache (%d cached)", g_device_manager.get_count());
    g_device_manager.clear();
    g_scan_filters.clear_pending();
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

BluetoothTransport bluetooth_device_get_transport(std::uint64_t device)
{
    const auto dev = g_device_manager.get_device(device);
    return static_cast<BluetoothTransport>(dev ? dev->transport : Transport::Unknown);
}

std::string bluetooth_device_get_id(std::uint64_t device)
{
    const auto dev = g_device_manager.get_device(device);
    return dev ? dev->id : std::string();
}

std::string bluetooth_device_get_name(std::uint64_t device)
{
    const auto dev = g_device_manager.get_device(device);
    return dev ? dev->name : std::string();
}

bool bluetooth_device_has_address(std::uint64_t device)
{
    const auto dev = g_device_manager.get_device(device);
    return dev ? dev->address_available : false;
}

std::string bluetooth_device_get_address(std::uint64_t device)
{
    const auto dev = g_device_manager.get_device(device);
    return dev ? dev->address : std::string();
}

bool bluetooth_device_has_rssi(std::uint64_t device)
{
    const auto dev = g_device_manager.get_device(device);
    return dev ? dev->rssi_available : false;
}

int bluetooth_device_get_rssi(std::uint64_t device)
{
    const auto dev = g_device_manager.get_device(device);
    return dev ? dev->rssi : 0;
}

bool bluetooth_device_is_connectable(std::uint64_t device)
{
    const auto dev = g_device_manager.get_device(device);
    return dev ? dev->connectable : false;
}

BluetoothLeAdvertisement bluetooth_device_get_advertisement(std::uint64_t device)
{
    BluetoothLeAdvertisement out;
    const auto dev = g_device_manager.get_device(device);
    if (!dev || !dev->advertisement)
        return out;

    // Stored canonical and merged (R1-138).
    const LeAdvertisement& advertisement = *dev->advertisement;
    out.service_uuids = advertisement.service_uuids;
    for (const auto& entry : advertisement.service_data)
        out.service_data.push_back(BluetoothLeAdvertiseServiceData{ entry.uuid, entry.data });
    for (const auto& entry : advertisement.manufacturer_data)
        out.manufacturer_data.push_back(BluetoothLeAdvertiseManufacturerData{ static_cast<std::int32_t>(entry.company_id), entry.data });
    out.tx_power = advertisement.tx_power;
    return out;
}

std::uint64_t bluetooth_device_from_id(std::string_view id)
{
    if (!require_backend())
        return 0;

    const std::string key(id);
    if (const std::uint64_t handle = g_device_manager.find_id(key))
        return handle;

    // The backend spells the id the way it issues it, so the entry it builds
    // may name a device already cached under that spelling: upsert keeps it.
    DiscoveredDevice device;
    std::string message;
    const Error error = g_backend->device_from_id(key, device, message);
    set_last_error(error, message);
    if (error != Error::Ok)
        return 0;

    const std::uint64_t handle = g_device_manager.upsert_device(device);
    GMBT_LOG("device from id '%s' -> handle=%llu", key.c_str(), static_cast<unsigned long long>(handle));
    return handle;
}

BluetoothError bluetooth_le_connected_devices_query(const std::vector<std::string_view>& service_uuids, const gm::wire::GMFunction& callback)
{
    if (!require_backend())
        return to_gm(g_last_error);

    std::vector<std::string> uuids;
    uuids.reserve(service_uuids.size());
    for (const auto uuid : service_uuids)
    {
        if (!check_uuid(uuid))
            return to_gm(g_last_error);
        uuids.push_back(canonical_uuid(uuid));
    }

    if (!check_radio())
        return to_gm(g_last_error);

    return start_devices_query(callback, [&](std::uint64_t query, std::string& message) {
        return g_backend->le_connected_devices_query(query, uuids, message);
    });
}

BluetoothError bluetooth_paired_devices_query(const gm::wire::GMFunction& callback)
{
    if (!require_backend())
        return to_gm(g_last_error);

    if (!check_feature(BluetoothFeature::PairedDevicesQuery, "Listing paired devices is not supported on this platform"))
        return to_gm(g_last_error);

    if (!check_radio())
        return to_gm(g_last_error);

    return start_devices_query(callback, [&](std::uint64_t query, std::string& message) {
        return g_backend->paired_devices_query(query, message);
    });
}

std::uint64_t bluetooth_classic_connect(std::uint64_t device, std::string_view service_uuid, const gm::wire::GMFunction& callback)
{
    if (!require_backend())
        return 0;

    const auto dev = check_device(device, Transport::Classic);
    if (!dev || !check_uuid(service_uuid) || !check_radio())
        return 0;

    const std::uint64_t connection = g_classic_connection_manager.create_connection(device);
    g_classic_connection_manager.set_connect_callback(connection, callback);

    std::string message;
    const Error error = g_backend->classic_connect(connection, *dev, std::string(service_uuid), message);
    set_last_error(error, message);

    if (error != Error::Ok)
    {
        g_classic_connection_manager.remove_connection(connection);
        g_classic_connection_manager.take_connect_callback(connection);
        return 0;
    }

    return connection;
}

bool bluetooth_pairing_is_supported(std::uint64_t device)
{
    if (!g_backend)
        return false;

    const auto dev = g_device_manager.get_device(device);
    return dev && g_backend->pairing_is_supported(*dev);
}

BluetoothError bluetooth_pair(std::uint64_t device, const gm::wire::GMFunction& callback)
{
    if (!require_backend())
        return to_gm(g_last_error);

    const auto dev = check_device(device, Transport::Unknown);
    if (!dev || !check_radio())
        return to_gm(g_last_error);

    // Registered before calling the backend: pairing runs asynchronously and may
    // push its DevicePaired completion event before this call even returns.
    // Recorded with or without a callback, so a second pair of the same device
    // is refused instead of overwriting the first one's callback.
    {
        std::scoped_lock lock(g_pending_pair_mutex);
        if (g_pending_pair_callbacks.find(device) != g_pending_pair_callbacks.end())
            return fail(Error::Busy, "A pairing for this device is already in progress");
        g_pending_pair_callbacks[device] = callback;
    }

    std::string message;
    const Error error = g_backend->pair(device, *dev, message);
    set_last_error(error, message);

    if (error != Error::Ok)
    {
        std::scoped_lock lock(g_pending_pair_mutex);
        g_pending_pair_callbacks.erase(device);
    }

    return to_gm(error);
}

bool bluetooth_device_is_paired(std::uint64_t device)
{
    const auto dev = g_device_manager.get_device(device);
    return dev && g_backend && g_backend->is_paired(*dev);
}

BluetoothError bluetooth_classic_disconnect(std::uint64_t connection)
{
    if (!require_backend())
        return to_gm(g_last_error);

    if (!g_classic_connection_manager.is_valid(connection))
        return fail(Error::InvalidHandle, "Invalid Bluetooth Classic connection handle");

    std::string message;
    const Error error = g_backend->classic_disconnect(connection, message);
    set_last_error(error, message);

    // A connect still in flight is cancelled, not completed: its callback
    // fires now, and a late completion from the backend finds none.
    const GMFunction connect_callback = g_classic_connection_manager.take_connect_callback(connection);
    const std::uint64_t device = g_classic_connection_manager.get_device(connection);
    g_classic_connection_manager.remove_connection(connection);
    // callback(error_code, message, connection, device)
    invoke_callback("classic_connect", connect_callback, static_cast<double>(Error::ConnectionFailed),
        std::string("Connection cancelled by bluetooth_classic_disconnect"), static_cast<double>(connection),
        static_cast<double>(device));

    return to_gm(error);
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

BluetoothError bluetooth_classic_send(std::uint64_t connection, struct gm::wire::GMBuffer data, unsigned int offset, unsigned int size)
{
    if (!require_backend())
        return to_gm(g_last_error);

    if (!g_classic_connection_manager.is_valid(connection))
        return fail(Error::InvalidHandle, "Invalid Bluetooth Classic connection handle");

    if (!buffer_range_valid(data, offset, size, "bluetooth_classic_send"))
        return to_gm(Error::InvalidArgument);

    std::string message;
    const Error error = g_backend->classic_send_bytes(connection,
        static_cast<const std::uint8_t*>(data.data()) + offset, size, message);
    set_last_error(error, message);
    return to_gm(error);
}


BluetoothError bluetooth_classic_server_start(std::string_view name, std::string_view service_uuid)
{
    if (!require_backend())
        return to_gm(g_last_error);

    if (!g_backend->supports_classic_server())
        return fail(Error::NotSupported, "A Bluetooth Classic server is not supported on this platform");

    if (!check_uuid(service_uuid) || !check_radio())
        return to_gm(g_last_error);

    // One answer on every platform, rather than a running server that quietly
    // keeps its old name and UUID.
    if (g_backend->classic_server_is_running())
        return fail(Error::Busy, "A Classic server is already running; stop it first");

    std::string message;
    const Error error = g_backend->classic_server_start(std::string(name), std::string(service_uuid), message);
    set_last_error(error, message);
    return to_gm(error);
}

BluetoothError bluetooth_classic_server_stop()
{
    if (!require_backend())
        return to_gm(g_last_error);

    std::string message;
    const Error error = g_backend->classic_server_stop(message);
    set_last_error(error, message);
    return to_gm(error);
}

bool bluetooth_classic_server_is_running()
{
    return g_backend && g_backend->classic_server_is_running();
}

BluetoothError bluetooth_classic_discoverable_start(std::int32_t duration_seconds)
{
    if (!require_backend())
        return to_gm(g_last_error);

    if (!check_feature(BluetoothFeature::ClassicDiscoverable, "Making the device discoverable is not supported on this platform"))
        return to_gm(g_last_error);

    if (!check_radio())
        return to_gm(g_last_error);

    std::string message;
    const Error error = g_backend->classic_discoverable_start(duration_seconds, message);
    set_last_error(error, message);
    return to_gm(error);
}

BluetoothError bluetooth_classic_discoverable_stop()
{
    if (!require_backend())
        return to_gm(g_last_error);

    std::string message;
    const Error error = g_backend->classic_discoverable_stop(message);
    set_last_error(error, message);
    return to_gm(error);
}

bool bluetooth_classic_discoverable_is_running()
{
    return g_backend && g_backend->classic_discoverable_is_running();
}

// Callback registration functions
bool bluetooth_set_callback_state_changed(const gm::wire::GMFunction& callback)
{
    set_callback(g_callback_state_changed, callback, "Bluetooth state changed");

    // The public contract promises the current known state immediately after
    // registration.
    dispatch_current_state();

    return true;
}

bool bluetooth_remove_callback_state_changed()
{
    return remove_callback(g_callback_state_changed, "Bluetooth state changed");
}

bool bluetooth_set_callback_device_found(const gm::wire::GMFunction& callback)
{
    return set_callback(g_callback_device_found, callback, "Device found");
}

bool bluetooth_remove_callback_device_found()
{
    return remove_callback(g_callback_device_found, "Device found");
}

bool bluetooth_set_callback_scan_stopped(const gm::wire::GMFunction& callback)
{
    return set_callback(g_callback_scan_stopped, callback, "Scan stopped");
}

bool bluetooth_remove_callback_scan_stopped()
{
    return remove_callback(g_callback_scan_stopped, "Scan stopped");
}

bool bluetooth_set_callback_classic_client_connected(const gm::wire::GMFunction& callback)
{
    return set_callback(g_callback_classic_client_connected, callback, "Classic client connected");
}

bool bluetooth_remove_callback_classic_client_connected()
{
    return remove_callback(g_callback_classic_client_connected, "Classic client connected");
}

bool bluetooth_set_callback_classic_data(const gm::wire::GMFunction& callback)
{
    return set_callback(g_callback_classic_data, callback, "Classic data");
}

bool bluetooth_remove_callback_classic_data()
{
    return remove_callback(g_callback_classic_data, "Classic data");
}

bool bluetooth_set_callback_classic_disconnected(const gm::wire::GMFunction& callback)
{
    return set_callback(g_callback_classic_disconnected, callback, "Classic disconnected");
}

bool bluetooth_remove_callback_classic_disconnected()
{
    return remove_callback(g_callback_classic_disconnected, "Classic disconnected");
}

// --- BLE GATT client: connect / disconnect ---

std::uint64_t bluetooth_le_connect(std::uint64_t device, const gm::wire::GMFunction& callback)
{
    if (!require_backend())
        return 0;

    const auto dev = check_device(device, Transport::LowEnergy);
    if (!dev || !check_radio())
        return 0;

    // One client connection per device, connecting or connected (R1-197).
    if (g_le_connection_manager.has_device(device))
    {
        fail(Error::Busy, "The device already has a connection");
        return 0;
    }

    const std::uint64_t connection = g_le_connection_manager.create_connection(device);
    g_le_connection_manager.set_connect_callback(connection, callback);

    std::string message;
    const Error error = g_backend->le_connect(connection, *dev, message);
    set_last_error(error, message);

    if (error != Error::Ok)
    {
        g_le_connection_manager.remove_connection(connection);
        g_le_connection_manager.take_connect_callback(connection);
        return 0;
    }

    return connection;
}

BluetoothError bluetooth_le_disconnect(std::uint64_t connection)
{
    if (!require_backend())
        return to_gm(g_last_error);

    // A handle already retired - by an earlier disconnect, a drop or a failed
    // open - may share its peripheral with a newer link; the backend is never
    // asked to close it (R1-225). A connect still in flight is a valid handle.
    if (!g_le_connection_manager.is_valid(connection))
        return fail(Error::InvalidHandle, "Invalid LE connection handle");

    std::string message;
    const Error error = g_backend->le_disconnect(connection, message);
    set_last_error(error, message);

    // Whatever the backend answered, the game is done with this link: no op
    // waiting on it may outlive the call, and its handle is retired.
    const std::uint64_t device = g_le_connection_manager.get_device(connection);
    retire_le_connection(connection);

    // A connect still in flight is cancelled, not completed: its callback
    // fires now, and a late open event from the backend finds none.
    // callback(error_code, message, connection, device)
    invoke_callback("le_connect", g_le_connection_manager.take_connect_callback(connection),
        static_cast<double>(Error::ConnectionFailed), std::string("Connection cancelled by bluetooth_le_disconnect"),
        static_cast<double>(connection), static_cast<double>(device));

    return to_gm(error);
}

bool bluetooth_le_connection_is_valid(std::uint64_t connection)
{
    return g_le_connection_manager.is_valid(connection);
}

bool bluetooth_le_connection_is_connected(std::uint64_t connection)
{
    return g_backend && g_backend->le_connection_is_connected(connection);
}

std::uint64_t bluetooth_le_connection_get_device(std::uint64_t connection)
{
    return g_le_connection_manager.get_device(connection);
}

std::int32_t bluetooth_le_connection_get_mtu(std::uint64_t connection)
{
    if (!g_backend || !g_le_connection_manager.is_valid(connection) ||
        !g_backend->le_connection_is_connected(connection))
        return 0;
    return g_backend->le_connection_mtu(connection);
}

BluetoothError bluetooth_le_connection_request_mtu(std::uint64_t connection, std::int32_t mtu, const gm::wire::GMFunction& callback)
{
    if (!check_le_client_connection(connection))
        return to_gm(g_last_error);

    if (mtu < 23 || mtu > 517)
        return fail(Error::InvalidArgument, "mtu must be 23-517");

    if (!check_le_connected(connection))
        return to_gm(g_last_error);

    // Windows and Apple negotiate the MTU themselves when the link opens, and
    // only Android can ask (LeMtuRequest, R1-140): answer with the one in effect.
    const std::int32_t current = g_backend->le_connection_mtu(connection);
    // callback(error_code, message, connection, mtu)
    invoke_callback("request_mtu", callback, static_cast<double>(Error::Ok), std::string(), static_cast<double>(connection),
        static_cast<double>(current));
    return to_gm(Error::Ok);
}

BluetoothError bluetooth_le_connection_read_rssi(std::uint64_t connection, const gm::wire::GMFunction& callback)
{
    if (!check_le_client_connection(connection))
        return to_gm(g_last_error);

    if (!check_feature(BluetoothFeature::LeReadRssi, "Reading the RSSI of a connection is not supported on this platform") ||
        !check_le_connected(connection))
        return to_gm(g_last_error);

    return start_le_op(LeOpKind::RssiRead, callback, connection, connection, [&](std::uint64_t op_id, std::string& message) {
        return g_backend->le_read_rssi(op_id, connection, message);
    });
}

BluetoothError bluetooth_le_connection_request_priority(std::uint64_t connection, BluetoothLeConnectionPriority priority)
{
    if (!check_le_client_connection(connection))
        return to_gm(g_last_error);

    if (priority != BluetoothLeConnectionPriority::Balanced && priority != BluetoothLeConnectionPriority::High &&
        priority != BluetoothLeConnectionPriority::LowPower)
        return fail(Error::InvalidArgument, "priority must be a BluetoothLeConnectionPriority");

    if (!check_feature(BluetoothFeature::LeConnectionPriority, "Connection priority is not supported on this platform") ||
        !check_le_connected(connection))
        return to_gm(g_last_error);

    std::string message;
    const Error error = g_backend->le_request_connection_priority(connection, static_cast<std::int32_t>(priority), message);
    set_last_error(error, message);
    return to_gm(error);
}

// --- BLE GATT client: service / characteristic / descriptor discovery ---

BluetoothError bluetooth_le_services_discover(std::uint64_t connection, const gm::wire::GMFunction& callback)
{
    if (!check_le_client_connection(connection))
        return to_gm(g_last_error);

    return start_le_op(LeOpKind::ServicesDiscover, callback, connection, connection, [&](std::uint64_t op_id, std::string& message) {
        return g_backend->le_services_discover(op_id, connection, message);
    });
}

std::int32_t bluetooth_le_service_get_count(std::uint64_t connection)
{
    return g_service_cache.get_count(connection);
}

std::uint64_t bluetooth_le_service_get_at(std::uint64_t connection, std::int32_t index)
{
    return g_service_cache.get_at(connection, index);
}

std::string bluetooth_le_service_get_uuid(std::uint64_t service)
{
    if (const auto server = find_le_server_attribute(service, HandleKind::LeServerService))
        return server->uuid;
    return g_service_cache.get_uuid(service);
}

BluetoothError bluetooth_le_characteristics_discover(std::uint64_t service, const gm::wire::GMFunction& callback)
{
    if (!require_backend())
        return to_gm(g_last_error);

    if (!g_service_cache.is_valid(service))
        return fail(Error::InvalidHandle, "Invalid service handle");

    const std::uint64_t connection = g_service_cache.get_parent(service);
    const LeAttributeRef ref = service_ref(service);

    return start_le_op(LeOpKind::CharacteristicsDiscover, callback, service, connection, [&](std::uint64_t op_id, std::string& message) {
        return g_backend->le_characteristics_discover(op_id, connection, ref, message);
    });
}

// The getters walk a server service the same way, through the handles
// add_service minted (R1-241).
std::int32_t bluetooth_le_characteristic_get_count(std::uint64_t service)
{
    if (const auto server = find_le_server_attribute(service, HandleKind::LeServerService))
        return static_cast<std::int32_t>(server->children.size());
    return g_characteristic_cache.get_count(service);
}

std::uint64_t bluetooth_le_characteristic_get_at(std::uint64_t service, std::int32_t index)
{
    if (find_le_server_attribute(service, HandleKind::LeServerService))
        return le_server_child_at(service, HandleKind::LeServerService, index);
    return g_characteristic_cache.get_at(service, index);
}

std::string bluetooth_le_characteristic_get_uuid(std::uint64_t characteristic)
{
    if (const auto server = find_le_server_attribute(characteristic, HandleKind::LeServerCharacteristic))
        return server->uuid;
    return g_characteristic_cache.get_uuid(characteristic);
}

std::int32_t bluetooth_le_characteristic_get_properties(std::uint64_t characteristic)
{
    if (const auto server = find_le_server_attribute(characteristic, HandleKind::LeServerCharacteristic))
        return server->properties;
    return g_characteristic_cache.get_properties(characteristic);
}

BluetoothError bluetooth_le_descriptors_discover(std::uint64_t characteristic, const gm::wire::GMFunction& callback)
{
    if (!require_backend())
        return to_gm(g_last_error);

    if (!g_characteristic_cache.is_valid(characteristic))
        return fail(Error::InvalidHandle, "Invalid characteristic handle");

    const std::uint64_t connection = characteristic_connection(characteristic);
    const LeAttributeRef ref = characteristic_ref(characteristic);

    return start_le_op(LeOpKind::DescriptorsDiscover, callback, characteristic, connection, [&](std::uint64_t op_id, std::string& message) {
        return g_backend->le_descriptors_discover(op_id, connection, ref, message);
    });
}

std::int32_t bluetooth_le_descriptor_get_count(std::uint64_t characteristic)
{
    if (const auto server = find_le_server_attribute(characteristic, HandleKind::LeServerCharacteristic))
        return static_cast<std::int32_t>(server->children.size());
    return g_descriptor_cache.get_count(characteristic);
}

std::uint64_t bluetooth_le_descriptor_get_at(std::uint64_t characteristic, std::int32_t index)
{
    if (find_le_server_attribute(characteristic, HandleKind::LeServerCharacteristic))
        return le_server_child_at(characteristic, HandleKind::LeServerCharacteristic, index);
    return g_descriptor_cache.get_at(characteristic, index);
}

std::string bluetooth_le_descriptor_get_uuid(std::uint64_t descriptor)
{
    if (const auto server = find_le_server_attribute(descriptor, HandleKind::LeServerDescriptor))
        return server->uuid;
    return g_descriptor_cache.get_uuid(descriptor);
}

// --- BLE GATT client: characteristic / descriptor read, write, subscribe ---

BluetoothError bluetooth_le_characteristic_read(std::uint64_t characteristic, const gm::wire::GMFunction& callback)
{
    if (!require_backend())
        return to_gm(g_last_error);

    if (!g_characteristic_cache.is_valid(characteristic))
        return fail(Error::InvalidHandle, "Invalid characteristic handle");

    const std::uint64_t connection = characteristic_connection(characteristic);
    const LeAttributeRef ref = characteristic_ref(characteristic);

    return start_le_op(LeOpKind::CharacteristicRead, callback, characteristic, connection, [&](std::uint64_t op_id, std::string& message) {
        return g_backend->le_characteristic_read(op_id, connection, ref, message);
    });
}

BluetoothError bluetooth_le_characteristic_write(std::uint64_t characteristic, struct gm::wire::GMBuffer data, unsigned int offset, unsigned int size, BluetoothLeWriteType write_type, const gm::wire::GMFunction& callback)
{
    if (!require_backend())
        return to_gm(g_last_error);

    if (!g_characteristic_cache.is_valid(characteristic))
        return fail(Error::InvalidHandle, "Invalid characteristic handle");

    // The wire carries any int32; only the two members mean anything (R1-149).
    if (write_type != BluetoothLeWriteType::WithResponse && write_type != BluetoothLeWriteType::WithoutResponse)
        return fail(Error::InvalidArgument, "write_type must be a BluetoothLeWriteType");

    const std::uint64_t connection = characteristic_connection(characteristic);
    const LeAttributeRef ref = characteristic_ref(characteristic);

    if (!buffer_range_valid(data, offset, size, "bluetooth_le_characteristic_write"))
        return to_gm(Error::InvalidArgument);

    const bool with_response = (write_type == BluetoothLeWriteType::WithResponse);
    const LeOpKind kind = with_response ? LeOpKind::CharacteristicWrite : LeOpKind::CharacteristicWriteWithoutResponse;

    if (!with_response)
    {
        if (!characteristic_has_property(characteristic, BluetoothLeCharacteristicProperty::WriteWithoutResponse))
            return fail(Error::NotSupported, "The characteristic does not take writes without response");

        if (g_le_ops.count(kind, connection) >= k_max_pending_writes_without_response)
            return fail(Error::Busy, "Too many writes without response are waiting to be sent");
    }

    const std::vector<std::uint8_t> value = slice_bytes(data, offset, size);

    return start_le_op(kind, callback, characteristic, connection, [&](std::uint64_t op_id, std::string& message) {
        return g_backend->le_characteristic_write(op_id, connection, ref, value, with_response, message);
    });
}

BluetoothError bluetooth_le_characteristic_subscribe(std::uint64_t characteristic, BluetoothLeSubscribeMode mode, const gm::wire::GMFunction& callback)
{
    if (!require_backend())
        return to_gm(g_last_error);

    if (!g_characteristic_cache.is_valid(characteristic))
        return fail(Error::InvalidHandle, "Invalid characteristic handle");

    if (mode != BluetoothLeSubscribeMode::Unsubscribe && mode != BluetoothLeSubscribeMode::Notify &&
        mode != BluetoothLeSubscribeMode::Indicate)
        return fail(Error::InvalidArgument, "mode must be a BluetoothLeSubscribeMode");

    if (mode == BluetoothLeSubscribeMode::Notify &&
        !characteristic_has_property(characteristic, BluetoothLeCharacteristicProperty::Notify))
        return fail(Error::NotSupported, "The characteristic does not support notifications");

    if (mode == BluetoothLeSubscribeMode::Indicate &&
        !characteristic_has_property(characteristic, BluetoothLeCharacteristicProperty::Indicate))
        return fail(Error::NotSupported, "The characteristic does not support indications");

    const std::uint64_t connection = characteristic_connection(characteristic);
    const LeAttributeRef ref = characteristic_ref(characteristic);

    return start_le_op(LeOpKind::CharacteristicSubscribe, callback, characteristic, connection, [&](std::uint64_t op_id, std::string& message) {
        return g_backend->le_characteristic_subscribe(op_id, connection, ref, static_cast<std::int32_t>(mode), message);
    });
}

BluetoothError bluetooth_le_descriptor_read(std::uint64_t descriptor, const gm::wire::GMFunction& callback)
{
    if (!require_backend())
        return to_gm(g_last_error);

    if (!g_descriptor_cache.is_valid(descriptor))
        return fail(Error::InvalidHandle, "Invalid descriptor handle");

    const std::uint64_t connection = descriptor_connection(descriptor);
    const LeAttributeRef ref = descriptor_ref(descriptor);

    return start_le_op(LeOpKind::DescriptorRead, callback, descriptor, connection, [&](std::uint64_t op_id, std::string& message) {
        return g_backend->le_descriptor_read(op_id, connection, ref, message);
    });
}

BluetoothError bluetooth_le_descriptor_write(std::uint64_t descriptor, struct gm::wire::GMBuffer data, unsigned int offset, unsigned int size, const gm::wire::GMFunction& callback)
{
    if (!require_backend())
        return to_gm(g_last_error);

    if (!g_descriptor_cache.is_valid(descriptor))
        return fail(Error::InvalidHandle, "Invalid descriptor handle");

    const std::uint64_t connection = descriptor_connection(descriptor);
    const LeAttributeRef ref = descriptor_ref(descriptor);

    // The CCCD has one writer on every platform, bluetooth_le_characteristic_subscribe;
    // CoreBluetooth throws on a direct write.
    if (g_descriptor_cache.get_uuid(descriptor) == k_cccd_uuid)
        return fail(Error::InvalidArgument, "The CCCD is written by bluetooth_le_characteristic_subscribe");

    if (!buffer_range_valid(data, offset, size, "bluetooth_le_descriptor_write"))
        return to_gm(Error::InvalidArgument);

    const std::vector<std::uint8_t> value = slice_bytes(data, offset, size);

    return start_le_op(LeOpKind::DescriptorWrite, callback, descriptor, connection, [&](std::uint64_t op_id, std::string& message) {
        return g_backend->le_descriptor_write(op_id, connection, ref, value, message);
    });
}

// --- Values ---

// The bytes copied, or -1 on error (R1-214).
std::int32_t bluetooth_value_fetch(std::uint64_t value, struct gm::wire::GMBuffer out_data, unsigned int offset)
{
    std::string message;
    std::size_t copied = 0;
    std::vector<LeftValue> left;
    const Error error = g_values.fetch(value, out_data, offset, copied, message, left);
    set_last_error(error, message);
    settle_left_values(left);
    return error == Error::Ok ? static_cast<std::int32_t>(copied) : -1;
}

BluetoothError bluetooth_value_release(std::uint64_t value)
{
    std::vector<LeftValue> left;
    const bool released = g_values.release(value, left);
    settle_left_values(left);
    if (released)
        return to_gm(Error::Ok);

    return fail(Error::InvalidHandle, "Unknown, fetched, released or expired value " + std::to_string(value));
}

// --- BLE advertise ---

BluetoothError bluetooth_le_advertise_start(const BluetoothLeAdvertiseSettings& settings, const BluetoothLeAdvertiseData& data, const gm::wire::GMFunction& callback)
{
    if (!require_backend())
        return to_gm(g_last_error);

    // Checked once here, so every backend sees only what it can act on; what a
    // backend cannot send is its own NotSupported (R1-36).
    LeAdvertiseSettings advertise_settings;
    advertise_settings.connectable = settings.connectable;
    if (settings.tx_power)
    {
        const auto level = static_cast<std::int32_t>(*settings.tx_power);
        if (level < static_cast<std::int32_t>(LeAdvertiseTxPower::UltraLow) || level > static_cast<std::int32_t>(LeAdvertiseTxPower::High))
            return fail(Error::InvalidArgument, "tx_power must be a BluetoothLeAdvertiseTxPower");
        advertise_settings.tx_power = static_cast<LeAdvertiseTxPower>(level);
    }

    LeAdvertiseData advertise_data;
    advertise_data.include_name = data.include_name;
    advertise_data.include_tx_power = data.include_tx_power;

    for (const auto& uuid : data.service_uuids)
    {
        if (!check_uuid(uuid))
            return to_gm(Error::InvalidArgument);
        advertise_data.service_uuids.push_back(canonical_uuid(uuid));
    }

    for (const auto& entry : data.service_data)
    {
        if (!check_uuid(entry.uuid))
            return to_gm(Error::InvalidArgument);
        const std::string uuid = canonical_uuid(entry.uuid);
        // A UUID with data is advertised as a UUID too, on every platform.
        if (std::find(advertise_data.service_uuids.begin(), advertise_data.service_uuids.end(), uuid) == advertise_data.service_uuids.end())
            return fail(Error::InvalidArgument, "Service data for " + uuid + " needs the uuid in service_uuids too");
        advertise_data.service_data.push_back({ uuid, entry.data });
    }

    for (const auto& entry : data.manufacturer_data)
    {
        if (entry.company_id < 0 || entry.company_id > 0xFFFF)
            return fail(Error::InvalidArgument, "company_id must be 0-65535, got " + std::to_string(entry.company_id));
        advertise_data.manufacturer_data.push_back({ static_cast<std::uint16_t>(entry.company_id), entry.data });
    }

    if (!check_radio())
        return to_gm(g_last_error);

    // A start or a running advertisement is stopped first: the new settings,
    // data and callback would otherwise be dropped (R1-72).
    if (g_backend->le_advertise_is_running())
        return fail(Error::Busy, "Bluetooth LE advertising is already running; stop it first");

    return start_le_op(LeOpKind::AdvertiseStart, callback, 0, 0, [&](std::uint64_t op_id, std::string& message) {
        return g_backend->le_advertise_start(op_id, advertise_settings, advertise_data, message);
    });
}

BluetoothError bluetooth_le_advertise_stop()
{
    if (!require_backend())
        return to_gm(g_last_error);

    std::string message;
    const Error error = g_backend->le_advertise_stop(message);
    set_last_error(error, message);
    return to_gm(error);
}

bool bluetooth_le_advertise_is_running()
{
    return g_backend && g_backend->le_advertise_is_running();
}

// --- BLE GATT server (peripheral) ---

BluetoothError bluetooth_le_server_start()
{
    if (!require_backend())
        return to_gm(g_last_error);

    // Starting a running server starts nothing new (R1-72, R1-223).
    if (g_backend->le_server_is_running())
        return to_gm(Error::Ok);

    if (!check_radio())
        return to_gm(g_last_error);

    std::string message;
    const Error error = g_backend->le_server_start(message);
    set_last_error(error, message);
    return to_gm(error);
}

BluetoothError bluetooth_le_server_stop()
{
    if (!require_backend())
        return to_gm(g_last_error);

    // Stopping a stopped server is Ok and fires nothing.
    if (!g_backend->le_server_is_running())
        return to_gm(Error::Ok);

    // Answered while the backend still holds them; it forgets them on stop.
    answer_pending_le_server_requests();

    std::string message;
    const Error error = g_backend->le_server_stop(message);
    set_last_error(error, message);
    if (error == Error::Ok)
    {
        retire_server_centrals();
        forget_le_server_services();
    }
    return to_gm(error);
}

bool bluetooth_le_server_is_running()
{
    return g_backend && g_backend->le_server_is_running();
}

BluetoothError bluetooth_le_server_add_service(const BluetoothLeServiceDefinition& service, const gm::wire::GMFunction& callback)
{
    if (!require_backend())
        return to_gm(g_last_error);

    if (!check_uuid(service.uuid))
        return to_gm(Error::InvalidArgument);
    for (const auto& characteristic : service.characteristics)
    {
        if (!check_uuid(characteristic.uuid))
            return to_gm(Error::InvalidArgument);
        if ((characteristic.properties & ~0xFF) != 0)
        {
            return fail(Error::InvalidArgument, "Characteristic " + characteristic.uuid + ": properties " +
                std::to_string(characteristic.properties) + " has bits that are not BluetoothLeCharacteristicProperty flags");
        }
        if ((characteristic.permissions & ~kPermissionAll) != 0)
        {
            return fail(Error::InvalidArgument, "Characteristic " + characteristic.uuid + ": permissions " +
                std::to_string(characteristic.permissions) + " has bits that are not BluetoothLeAttributePermission flags");
        }
        // The ATT limit on an attribute value.
        if (characteristic.value.size() > 512)
        {
            return fail(Error::InvalidArgument, "Characteristic " + characteristic.uuid + ": the initial value is " +
                std::to_string(characteristic.value.size()) + " bytes; an attribute value holds at most 512");
        }
        for (const auto& descriptor : characteristic.descriptors)
        {
            if (!check_uuid(descriptor.uuid))
                return to_gm(Error::InvalidArgument);
        }
    }

    // The server pre-flight every platform shares (R1-223).
    if (!g_backend->le_server_is_running())
        return fail(Error::OperationFailed, "The LE server is not running; call bluetooth_le_server_start first");

    const std::string service_uuid = canonical_uuid(service.uuid);
    std::uint64_t service_handle = 0;
    LeServerService definition;
    {
        std::scoped_lock lock(g_le_server_services_mutex);
        if (!g_le_server_services.insert(service_uuid).second)
            return fail(Error::Busy, "Service " + service_uuid + " has already been added");
        definition = register_le_server_service_locked(service);
        service_handle = definition.id;
    }

    return start_le_op(LeOpKind::ServerAddService, callback, service_handle, 0, [&](std::uint64_t op_id, std::string& message) {
        {
            std::scoped_lock lock(g_le_server_services_mutex);
            g_le_server_service_ops[op_id] = service_handle;
        }
        const Error error = g_backend->le_server_add_service(op_id, definition, message);
        // A refused add gives its handles and UUID back.
        if (error != Error::Ok)
            settle_le_server_service_op(op_id, false);
        return error;
    });
}

BluetoothError bluetooth_le_server_clear_services()
{
    if (!require_backend())
        return to_gm(g_last_error);

    // A stopped server holds no services: clearing it is Ok (R1-223).
    if (!g_backend->le_server_is_running())
        return to_gm(Error::Ok);

    // The services the requests name are going away.
    answer_pending_le_server_requests();

    std::string message;
    const Error error = g_backend->le_server_clear_services(message);
    set_last_error(error, message);
    if (error == Error::Ok)
        forget_le_server_services();
    return to_gm(error);
}

BluetoothError bluetooth_le_server_respond_read(std::int32_t request_id, BluetoothAttError error_code, struct gm::wire::GMBuffer data, unsigned int offset, unsigned int size)
{
    if (!require_backend())
        return to_gm(g_last_error);

    if (!check_att_error(error_code, "bluetooth_le_server_respond_read"))
        return to_gm(Error::InvalidArgument);

    // Checked before the request is erased, so a call with a bad id, kind or
    // range can be retried instead of leaving the remote central to time out.
    if (!check_le_server_request(request_id, false, "bluetooth_le_server_respond_write"))
        return to_gm(g_last_error);

    // A refused read sends no value, so its buffer is never touched.
    const bool success = error_code == BluetoothAttError::Success;
    if (success && size > 0 && !buffer_range_valid(data, offset, size, "bluetooth_le_server_respond_read"))
        return to_gm(Error::InvalidArgument);

    if (!take_le_server_request(request_id))
        return to_gm(g_last_error);

    std::vector<std::uint8_t> value;
    if (success && size > 0)
        value = slice_bytes(data, offset, size);

    std::string message;
    const Error error = g_backend->le_server_respond_read(request_id, static_cast<std::int32_t>(error_code), value, message);
    set_last_error(error, message);
    return to_gm(error);
}

BluetoothError bluetooth_le_server_respond_write(std::int32_t request_id, BluetoothAttError error_code)
{
    if (!require_backend())
        return to_gm(g_last_error);

    if (!check_att_error(error_code, "bluetooth_le_server_respond_write"))
        return to_gm(Error::InvalidArgument);

    if (!check_le_server_request(request_id, true, "bluetooth_le_server_respond_read"))
        return to_gm(g_last_error);

    const auto request = take_le_server_request(request_id);
    if (!request)
        return to_gm(g_last_error);
    release_value(request->value);

    // A write without response was completed when it arrived; answering it is
    // allowed everywhere and changes nothing (R1-15).
    if (!request->response_needed)
    {
        return to_gm(Error::Ok);
    }

    std::string message;
    const Error error = g_backend->le_server_respond_write(request_id, static_cast<std::int32_t>(error_code), message);
    set_last_error(error, message);
    return to_gm(error);
}

BluetoothError bluetooth_le_server_notify_value(std::uint64_t characteristic, std::uint64_t connection, struct gm::wire::GMBuffer data, unsigned int offset, unsigned int size)
{
    if (!require_backend())
        return to_gm(g_last_error);

    if (!find_le_server_attribute(characteristic, HandleKind::LeServerCharacteristic))
        return fail(Error::InvalidHandle, "Not a server characteristic handle: " + std::to_string(characteristic));

    // 0 broadcasts; a server connection names one central (R1-48).
    std::string central;
    if (connection != 0)
    {
        central = server_central_key(connection);
        if (central.empty())
            return fail(Error::InvalidHandle, "Unknown or disconnected server connection " + std::to_string(connection));
    }

    if (!buffer_range_valid(data, offset, size, "bluetooth_le_server_notify_value"))
        return to_gm(Error::InvalidArgument);

    const std::vector<std::uint8_t> value = slice_bytes(data, offset, size);

    std::string message;
    const Error error = g_backend->le_server_notify_value(characteristic, central, value, message);
    set_last_error(error, message);
    return to_gm(error);
}

// --- BLE callbacks ---

bool bluetooth_set_callback_le_disconnected(const gm::wire::GMFunction& callback)
{
    return set_callback(g_callback_le_disconnected, callback, "LE disconnected");
}

bool bluetooth_remove_callback_le_disconnected()
{
    return remove_callback(g_callback_le_disconnected, "LE disconnected");
}

bool bluetooth_set_callback_le_characteristic_value_changed(const gm::wire::GMFunction& callback)
{
    return set_callback(g_callback_le_characteristic_value_changed, callback, "LE characteristic value changed");
}

bool bluetooth_remove_callback_le_characteristic_value_changed()
{
    return remove_callback(g_callback_le_characteristic_value_changed, "LE characteristic value changed");
}

bool bluetooth_set_callback_le_server_connection_state_changed(const gm::wire::GMFunction& callback)
{
    return set_callback(g_callback_le_server_connection_state_changed, callback, "LE server connection state changed");
}

bool bluetooth_remove_callback_le_server_connection_state_changed()
{
    return remove_callback(g_callback_le_server_connection_state_changed, "LE server connection state changed");
}

bool bluetooth_set_callback_le_server_read_request(const gm::wire::GMFunction& callback)
{
    return set_callback(g_callback_le_server_read_request, callback, "LE server read request");
}

bool bluetooth_remove_callback_le_server_read_request()
{
    return remove_callback(g_callback_le_server_read_request, "LE server read request");
}

bool bluetooth_set_callback_le_server_write_request(const gm::wire::GMFunction& callback)
{
    return set_callback(g_callback_le_server_write_request, callback, "LE server write request");
}

bool bluetooth_remove_callback_le_server_write_request()
{
    return remove_callback(g_callback_le_server_write_request, "LE server write request");
}
