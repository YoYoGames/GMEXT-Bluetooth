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

    // The one global slot: the adapter has no start call to take it (rule 1 of
    // the spec's conventions). Every other event goes to the call that started
    // its subject.
    std::mutex g_callback_mutex;
    GMFunction g_callback_state_changed;

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

    // An event's receiver; an empty one drops the event.
    GMFunction counted(GMFunction callback)
    {
        if (!callback)
            g_dropped_events++;
        return callback;
    }

    GMFunction optional_callback(const std::optional<GMFunction>& callback)
    {
        return callback ? *callback : GMFunction();
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
        // created says whether this call added the device. scanned stamps the
        // time a scan saw it, for get_last_seen_age.
        std::uint64_t upsert_device(const DiscoveredDevice& device, bool scanned, bool* created = nullptr)
        {
            std::scoped_lock lock(mutex_);
            std::uint64_t handle = 0;
            if (const auto it = by_id_.find(device.id); it != by_id_.end())
            {
                handle = it->second;
                merge_device(devices_.at(handle), device);
                if (created)
                    *created = false;
            }
            else
            {
                handle = make_handle(HandleKind::Device, next_handle_++);
                devices_.emplace(handle, normalized_device(device));
                by_id_.emplace(device.id, handle);
                order_.push_back(handle);
                if (created)
                    *created = true;
            }
            if (scanned)
                seen_at_[handle] = std::chrono::steady_clock::now();
            return handle;
        }

        std::optional<std::chrono::steady_clock::duration> last_seen_age(std::uint64_t handle) const
        {
            std::scoped_lock lock(mutex_);
            const auto it = seen_at_.find(handle);
            if (it == seen_at_.end())
                return std::nullopt;
            return std::chrono::steady_clock::now() - it->second;
        }

        void clear()
        {
            std::scoped_lock lock(mutex_);
            devices_.clear();
            by_id_.clear();
            order_.clear();
            seen_at_.clear();
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
        std::unordered_map<std::uint64_t, std::chrono::steady_clock::time_point> seen_at_;
        std::uint64_t next_handle_ = 1;
    };

    DeviceManager g_device_manager;

    // The filters of the LE scan that is running, and the uncached devices it
    // has heard that match none of them yet (R1-139). A device enters the cache
    // when its merged advertisement first matches: Windows reports an
    // advertisement and its scan response separately, so a filter on a name in
    // one and a service in the other matches only the merged record. A device
    // already cached is matched on its cached record. The held devices are
    // bounded, the oldest dropped, and emptied by every scan start and by
    // bluetooth_device_clear.
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

        bool admits(const DiscoveredDevice& device) const
        {
            std::scoped_lock lock(mutex_);
            return filters_.empty() || matches_any(device);
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

        // data's first data.size() bytes equal pattern under mask, all ones
        // when there is none (Android's ScanFilter rule).
        static bool data_matches(const std::vector<std::uint8_t>& data,
            const std::optional<std::vector<std::uint8_t>>& pattern, const std::optional<std::vector<std::uint8_t>>& mask)
        {
            if (!pattern)
                return true;
            if (data.size() < pattern->size())
                return false;
            for (std::size_t i = 0; i < pattern->size(); ++i)
            {
                const std::uint8_t m = mask ? (*mask)[i] : std::uint8_t{ 0xFF };
                if ((data[i] & m) != ((*pattern)[i] & m))
                    return false;
            }
            return true;
        }

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
            if (filter.name_prefix && device.name.compare(0, filter.name_prefix->size(), *filter.name_prefix) != 0)
                return false;
            if (filter.company_id)
            {
                const auto& entries = advertisement.manufacturer_data;
                if (std::none_of(entries.begin(), entries.end(), [&](const LeAdvertiseManufacturerData& e) {
                        return e.company_id == *filter.company_id &&
                            data_matches(e.data, filter.manufacturer_data, filter.manufacturer_data_mask);
                    }))
                    return false;
            }
            if (filter.service_data_uuid)
            {
                const auto& entries = advertisement.service_data;
                if (std::none_of(entries.begin(), entries.end(), [&](const LeAdvertiseServiceData& e) {
                        return e.uuid == *filter.service_data_uuid &&
                            data_matches(e.data, filter.service_data, filter.service_data_mask);
                    }))
                    return false;
            }
            return true;
        }

        bool matches_any(const DiscoveredDevice& device) const
        {
            return std::any_of(filters_.begin(), filters_.end(),
                [&](const LeScanFilter& filter) { return matches(filter, device); });
        }

        mutable std::mutex mutex_;
        std::vector<LeScanFilter> filters_;
        std::unordered_map<std::string, DiscoveredDevice> pending_;
        std::vector<std::string> order_;
    };

    ScanFilterState g_scan_filters;

    // A connection's device and the event callbacks its connect call (or, for
    // an inbound Classic link, its server) gave it: on_data for Classic only.
    struct ConnectionRecord
    {
        std::uint64_t device = 0;
        GMFunction on_data;
        GMFunction on_disconnected;
    };

    // The connections of one transport, each to the device it was made for,
    // and the one-shot callback of each connect still in flight. Classic and LE
    // connections are minted from two independent handle spaces; handles count
    // up and are never reused, so a handle from before a clear never names a
    // later connection.
    template <HandleKind Kind>
    class ConnectionRegistry
    {
    public:
        std::uint64_t create_connection(ConnectionRecord record)
        {
            std::scoped_lock lock(mutex_);
            const std::uint64_t handle = make_handle(Kind, next_handle_++);
            connections_[handle] = std::move(record);
            return handle;
        }

        // The record of a connection that leaves now; empty when it had left.
        std::optional<ConnectionRecord> take_connection(std::uint64_t handle)
        {
            std::scoped_lock lock(mutex_);
            const auto it = connections_.find(handle);
            if (it == connections_.end())
                return std::nullopt;
            ConnectionRecord record = std::move(it->second);
            connections_.erase(it);
            return record;
        }

        bool remove_connection(std::uint64_t handle)
        {
            return take_connection(handle).has_value();
        }

        GMFunction get_on_data(std::uint64_t handle) const
        {
            std::scoped_lock lock(mutex_);
            const auto it = connections_.find(handle);
            return it != connections_.end() ? it->second.on_data : GMFunction();
        }

        std::vector<std::uint64_t> handles() const
        {
            std::scoped_lock lock(mutex_);
            std::vector<std::uint64_t> out;
            out.reserve(connections_.size());
            for (const auto& [handle, record] : connections_)
            {
                (void)record;
                out.push_back(handle);
            }
            return out;
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
            return it != connections_.end() ? it->second.device : 0;
        }

        // A device has at most one LE client connection, connecting or
        // connected.
        bool has_device(std::uint64_t device) const
        {
            std::scoped_lock lock(mutex_);
            for (const auto& [handle, record] : connections_)
            {
                (void)handle;
                if (record.device == device)
                    return true;
            }
            return false;
        }

        bool has_connect_callback(std::uint64_t handle) const
        {
            std::scoped_lock lock(mutex_);
            return connect_callbacks_.find(handle) != connect_callbacks_.end();
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
        std::map<std::uint64_t, ConnectionRecord> connections_;
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
    // platform (R1-66, R1-68). Unknown passes: a backend may still be waiting
    // for its first state, and starts the work once it has one. Resetting is
    // off, as Android's isEnabled() has it (R2-114). Assumes g_backend.
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
            case 1: // Resetting
                fail(Error::BluetoothDisabled, "Bluetooth is resetting");
                return false;
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

        // The parent's first entry with this canonical UUID, in discovery
        // order; 0 when it has none.
        std::uint64_t find_by_uuid(std::uint64_t parent, const std::string& uuid) const
        {
            std::scoped_lock lock(mutex_);
            for (const auto& [handle, entry] : entries_)
            {
                if (entry.parent == parent && entry.uuid == uuid)
                    return handle;
            }
            return 0;
        }

        std::vector<std::uint64_t> children(std::uint64_t parent) const
        {
            std::scoped_lock lock(mutex_);
            std::vector<std::uint64_t> out;
            for (const auto& [handle, entry] : entries_)
            {
                if (entry.parent == parent)
                    out.push_back(handle);
            }
            return out;
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
        const auto record = g_classic_connection_manager.take_connection(connection);
        if (!record)
            return;

        GMBT_LOG("classic connection %llu: receive data expired, ending it", static_cast<unsigned long long>(connection));
        if (g_backend)
        {
            std::string message;
            g_backend->classic_disconnect(connection, message);
        }

        // on_disconnected(error_code, message, connection)
        invoke_callback("classic on_disconnected", counted(record->on_disconnected),
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

    // The on_value of each subscribed characteristic, set by the subscribe call
    // and held until unsubscribe, a failed subscribe, the characteristic
    // leaving the cache (a disconnect, a rediscovery) or shutdown. token tells
    // a later subscription of the same handle from the one a late failure is for.
    struct Subscription
    {
        std::uint64_t token = 0;
        GMFunction on_value;
    };

    std::mutex g_subscriptions_mutex;
    std::unordered_map<std::uint64_t, Subscription> g_subscriptions;
    std::uint64_t g_next_subscription_token = 1;

    // 0 when the characteristic is already subscribed.
    std::uint64_t add_subscription(std::uint64_t characteristic, const GMFunction& on_value)
    {
        std::scoped_lock lock(g_subscriptions_mutex);
        const std::uint64_t token = g_next_subscription_token++;
        if (!g_subscriptions.emplace(characteristic, Subscription{ token, on_value }).second)
            return 0;
        return token;
    }

    // token 0 drops whatever subscription the characteristic has. True when
    // one was dropped.
    bool drop_subscription(std::uint64_t characteristic, std::uint64_t token = 0)
    {
        std::scoped_lock lock(g_subscriptions_mutex);
        const auto it = g_subscriptions.find(characteristic);
        if (it == g_subscriptions.end() || (token != 0 && it->second.token != token))
            return false;
        g_subscriptions.erase(it);
        return true;
    }

    GMFunction subscription_receiver(std::uint64_t characteristic)
    {
        std::scoped_lock lock(g_subscriptions_mutex);
        const auto it = g_subscriptions.find(characteristic);
        return it != g_subscriptions.end() ? it->second.on_value : GMFunction();
    }

    // Drops what discovery cached for one LE connection: its services, their
    // characteristics and those characteristics' descriptors and subscriptions.
    void erase_le_descriptors_of(const std::vector<std::uint64_t>& characteristics)
    {
        if (characteristics.empty())
            return;
        g_descriptor_cache.erase_children(characteristics);
        for (const std::uint64_t characteristic : characteristics)
            drop_subscription(characteristic);
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
        CharacteristicUnsubscribe,
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
            case LeOpKind::CharacteristicUnsubscribe: return "le_characteristic_unsubscribe";
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
        // The discovery job a step of a deeper discovery reports to instead of
        // a callback of its own.
        std::uint64_t job = 0;
        // A subscribe's subscription token, or an advertise start's session
        // token: what a failure of the op ends.
        std::uint64_t token = 0;
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
        std::uint64_t add(PendingLeOp op)
        {
            const std::uint64_t op_id = next_id_++;
            std::scoped_lock lock(mutex_);
            ops_.emplace(op_id, std::move(op));
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

        std::vector<PendingLeOp> take_kind(LeOpKind kind)
        {
            std::vector<PendingLeOp> out;
            std::scoped_lock lock(mutex_);
            for (auto it = ops_.begin(); it != ops_.end();)
            {
                if (it->second.kind == kind)
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
    Error start_le_op_quiet(PendingLeOp op, Start start, std::string& message)
    {
        const auto op_id = g_le_ops.add(std::move(op));
        const Error error = start(op_id, message);
        if (error != Error::Ok)
            g_le_ops.erase(op_id);
        return error;
    }

    template <typename Start>
    BluetoothError start_le_op(PendingLeOp op, Start start)
    {
        std::string message;
        const Error error = start_le_op_quiet(std::move(op), start, message);
        set_last_error(error, message);
        return to_gm(error);
    }

    template <typename Start>
    BluetoothError start_le_op(LeOpKind kind, const GMFunction& callback, std::uint64_t context,
        std::uint64_t connection, Start start)
    {
        return start_le_op(PendingLeOp{ kind, callback, context, connection }, start);
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

    // The subjects that run one at a time, each holding the callbacks of the
    // call that started it until it ends (rule 1). A start while one runs is
    // Busy (rule 2); a stop the game asks for ends it without an event (rule
    // 3); every other end fires its on_stopped once, from the path that ends it.
    std::mutex g_session_mutex;

    struct ScanSession
    {
        bool running = false;
        GMFunction on_found;
        GMFunction on_updated;
        GMFunction on_stopped;
        std::chrono::milliseconds update_interval{ 0 };
        // When this scan last announced each device; absent until on_found.
        std::unordered_map<std::uint64_t, std::chrono::steady_clock::time_point> announced;
    };

    ScanSession g_le_scan;
    ScanSession g_classic_scan;

    ScanSession& scan_session(Transport transport)
    {
        return transport == Transport::Classic ? g_classic_scan : g_le_scan;
    }

    struct AdvertiseSession
    {
        bool running = false;
        std::uint64_t token = 0;
        GMFunction on_stopped;
    };

    AdvertiseSession g_advertise;
    std::uint64_t g_next_advertise_token = 1;

    struct LeServerSession
    {
        bool running = false;
        GMFunction on_connected;
        GMFunction on_disconnected;
        GMFunction on_read;
        GMFunction on_write;
        GMFunction on_stopped;
    };

    LeServerSession g_le_server;

    struct ClassicServerSession
    {
        bool running = false;
        GMFunction on_connected;
        GMFunction on_data;
        GMFunction on_disconnected;
        GMFunction on_stopped;
    };

    ClassicServerSession g_classic_server;

    struct DiscoverableSession
    {
        bool running = false;
        GMFunction on_stopped;
    };

    DiscoverableSession g_discoverable;

    template <typename Session>
    bool session_running(const Session& session)
    {
        std::scoped_lock lock(g_session_mutex);
        return session.running;
    }

    // Starts a session unless one runs; false when one does.
    template <typename Session>
    bool begin_session(Session& session, Session started)
    {
        std::scoped_lock lock(g_session_mutex);
        if (session.running)
            return false;
        session = std::move(started);
        session.running = true;
        return true;
    }

    // Ends a session and hands back what it held: running is false when it had
    // already ended.
    template <typename Session>
    Session end_session(Session& session)
    {
        std::scoped_lock lock(g_session_mutex);
        Session ended = std::move(session);
        session = Session();
        return ended;
    }

    void fire_stopped(const char* name, const GMFunction& on_stopped, Error error, const std::string& message)
    {
        // on_stopped(error_code, message)
        invoke_callback(name, on_stopped, static_cast<double>(error), message);
    }

    // Reports a device a scan saw to the running scan of its transport: on_found
    // the first time this scan sees it, then on_updated at most once per update
    // interval. Nothing runs, nothing is announced.
    void announce_device(Transport transport, std::uint64_t device)
    {
        if (transport != Transport::LowEnergy && transport != Transport::Classic)
            return;

        GMFunction callback;
        const char* name = "scan on_found";
        {
            std::scoped_lock lock(g_session_mutex);
            ScanSession& session = scan_session(transport);
            if (!session.running)
                return;
            const auto now = std::chrono::steady_clock::now();
            const auto [it, first] = session.announced.try_emplace(device, now);
            if (first)
            {
                callback = session.on_found;
            }
            else if (session.on_updated && now - it->second >= session.update_interval)
            {
                it->second = now;
                callback = session.on_updated;
                name = "scan on_updated";
            }
        }
        // on_found(device), on_updated(device)
        invoke_callback(name, callback, static_cast<double>(device));
    }

    // A bluetooth_le_services_discover with options: the services step, a
    // characteristics step for each service the filter takes, a descriptors
    // step for each of their characteristics. The one callback fires once the
    // last step settles, with the first failure (R2-35).
    struct DiscoveryJob
    {
        GMFunction callback;
        std::uint64_t connection = 0;
        std::vector<std::string> service_uuids; // canonical; empty takes every service
        bool characteristics = false;
        bool descriptors = false;
        std::size_t outstanding = 0;
        Error error = Error::Ok;
        std::string message;
    };

    std::mutex g_discovery_jobs_mutex;
    std::unordered_map<std::uint64_t, DiscoveryJob> g_discovery_jobs;
    std::uint64_t g_next_discovery_job = 1;

    void settle_discovery_step(std::uint64_t job_id, Error error, const std::string& message)
    {
        std::optional<DiscoveryJob> done;
        {
            std::scoped_lock lock(g_discovery_jobs_mutex);
            const auto it = g_discovery_jobs.find(job_id);
            if (it == g_discovery_jobs.end())
                return;
            DiscoveryJob& job = it->second;
            if (error != Error::Ok && job.error == Error::Ok)
            {
                job.error = error;
                job.message = message;
            }
            if (--job.outstanding == 0)
            {
                done = std::move(job);
                g_discovery_jobs.erase(it);
            }
        }

        if (done)
        {
            // callback(error_code, message, connection)
            invoke_callback("le_services_discover", done->callback, static_cast<double>(done->error), done->message,
                static_cast<double>(done->connection));
        }
    }

    // Counts the step before it starts: the backend may complete it before
    // the call returns.
    Error start_discovery_step(std::uint64_t job_id, LeOpKind kind, std::uint64_t context, std::uint64_t connection,
        std::string& message)
    {
        {
            std::scoped_lock lock(g_discovery_jobs_mutex);
            const auto it = g_discovery_jobs.find(job_id);
            if (it == g_discovery_jobs.end())
                return Error::Ok;
            ++it->second.outstanding;
        }

        PendingLeOp op{ kind, GMFunction(), context, connection, job_id };
        return start_le_op_quiet(std::move(op), [&](std::uint64_t op_id, std::string& msg) {
            if (!g_backend)
            {
                msg = "Bluetooth backend is not initialized";
                return Error::NotInitialized;
            }
            switch (kind)
            {
                case LeOpKind::ServicesDiscover:
                    return g_backend->le_services_discover(op_id, connection, msg);
                case LeOpKind::CharacteristicsDiscover:
                    return g_backend->le_characteristics_discover(op_id, connection, service_ref(context), msg);
                default:
                    return g_backend->le_descriptors_discover(op_id, connection, characteristic_ref(context), msg);
            }
        }, message);
    }

    void start_discovery_child(std::uint64_t job_id, LeOpKind kind, std::uint64_t context, std::uint64_t connection)
    {
        std::string message;
        const Error error = start_discovery_step(job_id, kind, context, connection, message);
        if (error != Error::Ok)
            settle_discovery_step(job_id, error, message);
    }

    // One step of a job completed or failed: the level under it starts, then
    // the step settles, so the job cannot finish in between.
    void advance_discovery_job(const PendingLeOp& op, Error error, const std::string& message)
    {
        std::vector<std::string> service_uuids;
        bool characteristics = false;
        bool descriptors = false;
        {
            std::scoped_lock lock(g_discovery_jobs_mutex);
            const auto it = g_discovery_jobs.find(op.job);
            if (it == g_discovery_jobs.end())
                return;
            service_uuids = it->second.service_uuids;
            characteristics = it->second.characteristics;
            descriptors = it->second.descriptors;
        }

        if (error == Error::Ok && op.kind == LeOpKind::ServicesDiscover && characteristics)
        {
            for (const std::uint64_t service : g_service_cache.children(op.connection))
            {
                const std::string uuid = g_service_cache.get_uuid(service);
                if (service_uuids.empty() || std::find(service_uuids.begin(), service_uuids.end(), uuid) != service_uuids.end())
                    start_discovery_child(op.job, LeOpKind::CharacteristicsDiscover, service, op.connection);
            }
        }
        else if (error == Error::Ok && op.kind == LeOpKind::CharacteristicsDiscover && descriptors)
        {
            for (const std::uint64_t characteristic : g_characteristic_cache.children(op.context))
                start_discovery_child(op.job, LeOpKind::DescriptorsDiscover, characteristic, op.connection);
        }

        settle_discovery_step(op.job, error, message);
    }

    // A failed advertise start ends the session it began, unless the game
    // already stopped it and started another.
    void end_advertise_session_of(std::uint64_t token)
    {
        std::scoped_lock lock(g_session_mutex);
        if (g_advertise.running && g_advertise.token == token)
            g_advertise = AdvertiseSession();
    }

    // Settles a taken op: a job step goes to its job, a failure undoes what
    // the call began, and the callback fires.
    void finish_le_op(const PendingLeOp& op, Error error, const std::string& message,
        std::uint64_t value = 0, std::size_t size = 0, std::int32_t number = 0)
    {
        if (op.job != 0)
        {
            advance_discovery_job(op, error, message);
            return;
        }
        if (error != Error::Ok)
        {
            if (op.kind == LeOpKind::CharacteristicSubscribe)
                drop_subscription(op.context, op.token);
            else if (op.kind == LeOpKind::AdvertiseStart)
                end_advertise_session_of(op.token);
        }
        fire_le_op(op, error, message, value, size, number);
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

        finish_le_op(*op, event.error, event.message, value, size, number);
    }

    void fail_le_ops(std::vector<PendingLeOp> ops, Error error, const std::string& message)
    {
        for (const auto& op : ops)
            finish_le_op(op, error, message);
    }

    // Fails every op still waiting on this connection. The link is gone, so no
    // completion will arrive for them; a late one finds no id and is dropped.
    void purge_le_connection(std::uint64_t connection, Error error, const std::string& message)
    {
        auto ops = g_le_ops.take_connection(connection);
        if (ops.empty())
            return;
        GMBT_LOG("LE connection %llu closed with %zu op(s) pending, failing them",
            static_cast<unsigned long long>(connection), ops.size());
        fail_le_ops(std::move(ops), error, message);
    }

    // Retires an LE connection whose link is gone: its handle stops
    // validating, its waiting ops fail and what discovery cached for it is
    // dropped, subscriptions included. Returns its record, empty when it had
    // already been retired.
    std::optional<ConnectionRecord> retire_le_connection(std::uint64_t connection, Error error = Error::Disconnected,
        const std::string& message = "LE connection closed before the operation completed")
    {
        if (connection == 0)
            return std::nullopt;
        auto record = g_le_connection_manager.take_connection(connection);
        purge_le_connection(connection, error, message);
        erase_le_connection_attributes(connection);
        return record;
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
    // equals a client connection, and is retired when the central leaves
    // (R1-49): a later visit gets a new one.
    struct ServerCentral
    {
        std::uint64_t connection = 0;
        std::uint64_t device = 0;
    };

    std::mutex g_server_centrals_mutex;
    std::unordered_map<std::string, ServerCentral> g_server_centrals;

    // One of the running LE server's callbacks; empty while none runs.
    GMFunction le_server_callback(GMFunction LeServerSession::*member)
    {
        std::scoped_lock lock(g_session_mutex);
        return g_le_server.running ? g_le_server.*member : GMFunction();
    }

    // The server connection for a central key, minted on its first event, which
    // is also when the server's on_connected hears it. 0 for an empty key, from
    // a backend that names no central, and while no server runs.
    std::uint64_t note_server_central(const std::string& key, std::uint64_t device)
    {
        if (key.empty() || !session_running(g_le_server))
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

        // on_connected(connection, device)
        invoke_callback("le_server on_connected", counted(le_server_callback(&LeServerSession::on_connected)),
            static_cast<double>(central.connection), static_cast<double>(central.device));
        return central.connection;
    }

    void fire_server_central_gone(const GMFunction& on_disconnected, std::uint64_t connection, Error error,
        const std::string& message)
    {
        // on_disconnected(error_code, message, connection)
        invoke_callback("le_server on_disconnected", on_disconnected, static_cast<double>(error), message,
            static_cast<double>(connection));
    }

    void retire_server_central(const std::string& key, Error error, const std::string& message)
    {
        std::uint64_t connection = 0;
        {
            std::scoped_lock lock(g_server_centrals_mutex);
            const auto it = g_server_centrals.find(key);
            if (it == g_server_centrals.end())
                return;
            connection = it->second.connection;
            g_server_centrals.erase(it);
        }

        fire_server_central_gone(counted(le_server_callback(&LeServerSession::on_disconnected)), connection, error, message);
    }

    // Every central leaves with the server. on_disconnected is the ended
    // session's, or empty for a stop the game asked for (rule 3).
    void retire_server_centrals(const GMFunction& on_disconnected, Error error, const std::string& message)
    {
        std::unordered_map<std::string, ServerCentral> centrals;
        {
            std::scoped_lock lock(g_server_centrals_mutex);
            centrals.swap(g_server_centrals);
        }

        for (const auto& [key, central] : centrals)
        {
            (void)key;
            fire_server_central_gone(on_disconnected, central.connection, error, message);
        }
    }

    // The central a server connection handle names, and its key; empty when it
    // names none.
    std::optional<ServerCentral> find_server_central(std::uint64_t connection, std::string* key = nullptr)
    {
        std::scoped_lock lock(g_server_centrals_mutex);
        for (const auto& [k, central] : g_server_centrals)
        {
            if (central.connection == connection)
            {
                if (key)
                    *key = k;
                return central;
            }
        }
        return std::nullopt;
    }

    std::string server_central_key(std::uint64_t connection)
    {
        std::string key;
        find_server_central(connection, &key);
        return key;
    }

    // The device a server event's central is: device_id in the backend's own
    // id scheme, so the central and a scan result for the same peer are one
    // device, and the address only where the platform has one. Added without
    // any scan callback (R1-155); 0 when the event names no central.
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
        return g_device_manager.upsert_device(d, false);
    }

    // The child of a server service or characteristic with this canonical
    // UUID, first in definition order; 0 when it has none.
    std::uint64_t find_le_server_child(const LeServerAttribute& parent, const std::string& uuid)
    {
        std::scoped_lock lock(g_le_server_services_mutex);
        for (const std::uint64_t child : parent.children)
        {
            const auto it = g_le_server_attributes.find(child);
            if (it != g_le_server_attributes.end() && it->second.uuid == uuid)
                return child;
        }
        return 0;
    }

    // The *_find answer (R2-34): the handle, or 0 with why in last-error, so
    // every 0 carries a reason.
    std::uint64_t found(std::uint64_t handle, const char* what, const std::string& uuid)
    {
        if (handle == 0)
            fail(Error::NotFound, std::string("No ") + what + " with UUID " + uuid);
        return handle;
    }

    constexpr std::string_view k_cccd_uuid = "00002902-0000-1000-8000-00805f9b34fb";

    // The ATT limit on an attribute value.
    constexpr std::size_t k_max_attribute_value = 512;

    // Mints the service's handles in definition order and gives each to its
    // backend as the id it keys the attribute by. The CCCD belongs to the
    // platform: it gets no handle and is left out, as every backend skips it.
    LeServerService register_le_server_service_locked(const BluetoothLeServiceDefinition& service)
    {
        LeServerService out;
        out.id = mint_le_server_attribute_locked(HandleKind::LeServerService, 0, canonical_uuid(service.uuid));
        out.uuid = service.uuid;
        if (!service.characteristics)
            return out;

        out.characteristics.reserve(service.characteristics->size());
        for (const auto& characteristic : *service.characteristics)
        {
            LeServerCharacteristic entry;
            entry.id = mint_le_server_attribute_locked(HandleKind::LeServerCharacteristic,
                out.id, canonical_uuid(characteristic.uuid), characteristic.properties & 0xFF);
            entry.uuid = characteristic.uuid;
            entry.properties = characteristic.properties;
            entry.permissions = characteristic.permissions;
            if (characteristic.value)
                entry.value = *characteristic.value;

            if (characteristic.descriptors)
            {
                for (const auto& descriptor : *characteristic.descriptors)
                {
                    const std::string descriptor_uuid = canonical_uuid(descriptor.uuid);
                    if (descriptor_uuid == k_cccd_uuid)
                        continue;

                    LeServerDescriptor d;
                    d.id = mint_le_server_attribute_locked(HandleKind::LeServerDescriptor, entry.id, descriptor_uuid);
                    d.uuid = descriptor.uuid;
                    d.value = descriptor.value;
                    entry.descriptors.push_back(std::move(d));
                }
            }

            out.characteristics.push_back(std::move(entry));
        }

        return out;
    }

    // The window a connect call asked for: k_default_connect_timeout when
    // options or its timeout_ms is undefined. False, with InvalidArgument as
    // the last error, outside 1-300 s.
    bool connect_timeout(const std::optional<BluetoothConnectOptions>& options, std::chrono::milliseconds& timeout)
    {
        timeout = k_default_connect_timeout;
        if (!options || !options->timeout_ms)
            return true;

        const std::int32_t ms = *options->timeout_ms;
        if (ms < 1000 || ms > 300000)
        {
            fail(Error::InvalidArgument, "timeout_ms must be 1000-300000, got " + std::to_string(ms));
            return false;
        }
        timeout = std::chrono::milliseconds(ms);
        return true;
    }

    // A BluetoothLeScanFilter checked and in the form the matcher and the
    // backends take: UUIDs canonical, every data entry with its key and every
    // mask as long as its data (R2-134).
    bool check_scan_filter(const BluetoothLeScanFilter& filter, LeScanFilter& out)
    {
        if (!filter.service_uuid && !filter.name && !filter.name_prefix && !filter.company_id && !filter.service_data_uuid)
        {
            fail(Error::InvalidArgument,
                "A scan filter must set service_uuid, name, name_prefix, company_id or service_data_uuid");
            return false;
        }

        if (filter.service_uuid)
        {
            if (!check_uuid(*filter.service_uuid))
                return false;
            out.service_uuid = canonical_uuid(*filter.service_uuid);
        }
        out.name = filter.name;
        out.name_prefix = filter.name_prefix;

        if (filter.company_id)
        {
            if (*filter.company_id < 0 || *filter.company_id > 0xFFFF)
            {
                fail(Error::InvalidArgument, "A scan filter's company_id must be 0-65535");
                return false;
            }
            out.company_id = static_cast<std::uint16_t>(*filter.company_id);
        }
        if (filter.manufacturer_data && !filter.company_id)
        {
            fail(Error::InvalidArgument, "A scan filter's manufacturer_data needs its company_id");
            return false;
        }
        if (filter.manufacturer_data_mask &&
            (!filter.manufacturer_data || filter.manufacturer_data_mask->size() != filter.manufacturer_data->size()))
        {
            fail(Error::InvalidArgument, "A scan filter's manufacturer_data_mask needs manufacturer_data of the same length");
            return false;
        }
        out.manufacturer_data = filter.manufacturer_data;
        out.manufacturer_data_mask = filter.manufacturer_data_mask;

        if (filter.service_data_uuid)
        {
            if (!check_uuid(*filter.service_data_uuid))
                return false;
            out.service_data_uuid = canonical_uuid(*filter.service_data_uuid);
        }
        if (filter.service_data && !filter.service_data_uuid)
        {
            fail(Error::InvalidArgument, "A scan filter's service_data needs its service_data_uuid");
            return false;
        }
        if (filter.service_data_mask &&
            (!filter.service_data || filter.service_data_mask->size() != filter.service_data->size()))
        {
            fail(Error::InvalidArgument, "A scan filter's service_data_mask needs service_data of the same length");
            return false;
        }
        out.service_data = filter.service_data;
        out.service_data_mask = filter.service_data_mask;
        return true;
    }

    // Ends a scan session and, for LE, its filters. running is false in what
    // comes back when no scan of that transport ran.
    ScanSession end_scan(Transport transport)
    {
        ScanSession ended = end_session(scan_session(transport));
        if (transport == Transport::LowEnergy)
            g_scan_filters.start({});
        return ended;
    }

    // A device a scan saw: cached and announced to the running scan of its
    // transport (R2-73). An LE device must match that scan's filters on its
    // merged record, and one not cached yet is held until it does. Nothing is
    // taken while no scan of its transport runs. 0 when nothing took it.
    std::uint64_t scan_saw_device(const DiscoveredDevice& device)
    {
        const Transport transport = device.transport;
        if ((transport != Transport::LowEnergy && transport != Transport::Classic) ||
            !session_running(scan_session(transport)))
        {
            g_dropped_events++;
            return 0;
        }

        bool created = false;
        std::uint64_t handle = 0;
        if (transport == Transport::Classic)
        {
            handle = g_device_manager.upsert_device(device, true, &created);
        }
        else if (g_device_manager.find_id(device.id) == 0)
        {
            const auto admitted = g_scan_filters.admit(device);
            if (!admitted)
                return 0;
            handle = g_device_manager.upsert_device(*admitted, true, &created);
        }
        else
        {
            handle = g_device_manager.upsert_device(device, true, &created);
            const auto merged = g_device_manager.get_device(handle);
            if (!merged || !g_scan_filters.admits(*merged))
                return handle;
        }

        if (created)
        {
            GMBT_LOG("device added: handle=%llu transport=%d id='%s' name='%s' rssi=%d (available=%d) connectable=%d",
                static_cast<unsigned long long>(handle),
                static_cast<int>(device.transport),
                device.id.c_str(),
                device.name.c_str(),
                device.rssi,
                device.rssi_available ? 1 : 0,
                device.connectable ? 1 : 0);
        }

        announce_device(transport, handle);
        return handle;
    }

    // Fails or ends one LE client connection the core retires itself: a
    // connect still in progress fails with error, an open link reports
    // on_disconnected. The backend is told after the handle is retired, so
    // anything it reports for the link later finds nothing.
    void end_le_client_connection(std::uint64_t connection, Error error, const std::string& message)
    {
        const GMFunction connect_callback = g_le_connection_manager.take_connect_callback(connection);
        const std::uint64_t device = g_le_connection_manager.get_device(connection);
        const auto record = retire_le_connection(connection, error, message);
        if (!record && !connect_callback)
            return;

        if (g_backend)
        {
            std::string ignored;
            g_backend->le_disconnect(connection, ignored);
        }

        if (connect_callback)
        {
            // callback(error_code, message, connection, device)
            invoke_callback("le_connect", connect_callback, static_cast<double>(error), message,
                static_cast<double>(connection), static_cast<double>(device));
        }
        else
        {
            // on_disconnected(error_code, message, connection)
            invoke_callback("le on_disconnected", counted(record->on_disconnected), static_cast<double>(error), message,
                static_cast<double>(connection));
        }
    }

    // As end_le_client_connection, for a Classic connection.
    void end_classic_connection(std::uint64_t connection, Error error, const std::string& message)
    {
        const GMFunction connect_callback = g_classic_connection_manager.take_connect_callback(connection);
        const std::uint64_t device = g_classic_connection_manager.get_device(connection);
        const auto record = g_classic_connection_manager.take_connection(connection);
        if (!record && !connect_callback)
            return;

        if (g_backend)
        {
            std::string ignored;
            g_backend->classic_disconnect(connection, ignored);
        }

        if (connect_callback)
        {
            // callback(error_code, message, connection, device)
            invoke_callback("classic_connect", connect_callback, static_cast<double>(error), message,
                static_cast<double>(connection), static_cast<double>(device));
        }
        else
        {
            // on_disconnected(error_code, message, connection)
            invoke_callback("classic on_disconnected", counted(record->on_disconnected), static_cast<double>(error),
                message, static_cast<double>(connection));
        }
    }

    // Ends advertising the core ends itself. With notify, a running
    // advertisement reports on_stopped and a start still waiting fails with
    // error; without, the start fails OperationFailed and nothing else fires
    // (a server stop, rule 3).
    void end_advertising(bool notify, Error error, const std::string& message)
    {
        const AdvertiseSession ended = end_session(g_advertise);
        if (!ended.running)
            return;

        auto starts = g_le_ops.take_kind(LeOpKind::AdvertiseStart);
        if (g_backend)
        {
            std::string ignored;
            g_backend->le_advertise_stop(ignored);
        }

        if (!starts.empty())
            fail_le_ops(std::move(starts), notify ? error : Error::OperationFailed,
                notify ? message : std::string("Advertising was stopped before it started"));
        else if (notify)
            fire_stopped("advertise on_stopped", ended.on_stopped, error, message);
    }

    // Ends the LE server: its waiting requests are answered, queued service
    // adds fail, advertising ends, every central leaves and the services go.
    // With notify (the radio-off rule), each central reports on_disconnected,
    // then the server on_stopped, all with error; without, nothing fires but
    // the failed adds. Returns the backend's answer to the stop.
    Error end_le_server(bool notify, Error error, const std::string& message, std::string& stop_message)
    {
        const LeServerSession ended = end_session(g_le_server);
        if (!ended.running)
            return Error::Ok;

        answer_pending_le_server_requests();
        fail_le_ops(g_le_ops.take_kind(LeOpKind::ServerAddService), notify ? error : Error::OperationFailed,
            notify ? message : std::string("The LE server stopped before the service was added"));
        end_advertising(notify, error, message);

        Error stopped = Error::Ok;
        if (g_backend)
            stopped = g_backend->le_server_stop(stop_message);

        retire_server_centrals(notify ? ended.on_disconnected : GMFunction(), error, message);
        forget_le_server_services();

        if (notify)
            fire_stopped("le_server on_stopped", ended.on_stopped, error, message);
        return stopped;
    }

    // Rule 5: when the radio is no longer on, everything running ends with
    // BluetoothDisabled, each subject's last event fired from here, and
    // nothing restarts when it comes back. Runs before state_changed fires,
    // so GML hears the subjects end before the state that ended them.
    void end_everything_for_radio_off()
    {
        const Error error = Error::BluetoothDisabled;
        const std::string message = "Bluetooth was turned off or became unavailable";
        std::string ignored;

        for (const Transport transport : { Transport::LowEnergy, Transport::Classic })
        {
            const ScanSession ended = end_scan(transport);
            if (!ended.running)
                continue;
            if (g_backend)
            {
                if (transport == Transport::LowEnergy)
                    g_backend->le_scan_stop(ignored);
                else
                    g_backend->classic_scan_stop(ignored);
            }
            fire_stopped("scan on_stopped", ended.on_stopped, error, message);
        }

        end_advertising(true, error, message);

        for (const std::uint64_t connection : g_le_connection_manager.handles())
            end_le_client_connection(connection, error, message);
        for (const std::uint64_t connection : g_classic_connection_manager.handles())
            end_classic_connection(connection, error, message);

        end_le_server(true, error, message, ignored);

        const ClassicServerSession classic_server = end_session(g_classic_server);
        if (classic_server.running)
        {
            if (g_backend)
                g_backend->classic_server_stop(ignored);
            fire_stopped("classic_server on_stopped", classic_server.on_stopped, error, message);
        }

        const DiscoverableSession discoverable = end_session(g_discoverable);
        if (g_backend && (discoverable.running || g_backend->classic_discoverable_is_running()))
            g_backend->classic_discoverable_stop(ignored);
        if (discoverable.running)
            fire_stopped("classic_discoverable on_stopped", discoverable.on_stopped, error, message);
    }

    // BluetoothState raw values from spec.gmidl.
    constexpr std::int32_t k_state_unknown = 0;
    constexpr std::int32_t k_state_powered_on = 5;

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
            : k_state_unknown;
        invoke_callback("current state_changed", callback, static_cast<double>(state));
    }

    void dispatch_state_changed(const BackendEvent& event)
    {
        if (event.value != k_state_unknown && event.value != k_state_powered_on)
            end_everything_for_radio_off();

        std::scoped_lock dispatch_lock(g_state_dispatch_mutex);
        invoke_callback("state_changed", counted(registered_callback(g_callback_state_changed)),
            static_cast<double>(event.value));
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
        // A connection already retired - by bluetooth_le_disconnect, the
        // radio-off rule, or the other of the two reports Apple can make for
        // one drop - has had its end (R1-77).
        const std::uint64_t connection = event.connection;
        const auto record = retire_le_connection(connection);
        if (!record)
        {
            g_dropped_events++;
            return;
        }

        const Error error = event.error != Error::Ok ? event.error : Error::Disconnected;
        if (const GMFunction connect_callback = g_le_connection_manager.take_connect_callback(connection))
        {
            // A link that drops before its open was reported never opened.
            // callback(error_code, message, connection, device)
            invoke_callback("le_connect", connect_callback, static_cast<double>(Error::ConnectionFailed), event.message,
                static_cast<double>(connection), static_cast<double>(record->device));
            return;
        }
        // on_disconnected(error_code, message, connection)
        invoke_callback("le on_disconnected", counted(record->on_disconnected), static_cast<double>(error), event.message,
            static_cast<double>(connection));
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
            GMBT_TRACE("LE value for unknown characteristic %llu (connection=%llu), dropping",
                static_cast<unsigned long long>(characteristic_instance), static_cast<unsigned long long>(connection));
            g_dropped_events++;
            return;
        }

        // Unsubscribed meanwhile: the value is never stored.
        const GMFunction on_value = counted(subscription_receiver(characteristic));
        if (!on_value)
            return;

        const std::size_t size = event.bytes.size();
        const std::uint64_t value = store_value(std::move(event.bytes), ValueOwner::Le);

        // on_value(characteristic, connection, value, size)
        invoke_callback("le on_value", on_value, static_cast<double>(characteristic),
            static_cast<double>(connection), static_cast<double>(value), static_cast<double>(size));
    }

    // A backend reports a central's arrival where it sees one (a
    // subscription) and its departure where the platform has one; the first
    // event of any kind from a central is its connect.
    void dispatch_le_server_central(const BackendEvent& event)
    {
        if (event.connected)
        {
            note_server_central(event.central.key, le_server_event_device(event.central));
            return;
        }
        const Error error = event.error != Error::Ok ? event.error : Error::Disconnected;
        retire_server_central(event.central.key, error, event.message);
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

        const GMFunction on_read = counted(le_server_callback(&LeServerSession::on_read));
        if (!on_read)
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

        // on_read(request_id, connection, characteristic, descriptor, offset)
        // GML answers with the value's bytes from offset on, as on Android.
        invoke_callback("le_server on_read", on_read, static_cast<double>(request_id), static_cast<double>(connection),
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

        const GMFunction on_write = counted(le_server_callback(&LeServerSession::on_write));
        if (!on_write)
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

        // on_write(request_id, connection, characteristic, descriptor, offset, response_needed, value, size)
        invoke_callback("le_server on_write", on_write, static_cast<double>(request_id), static_cast<double>(connection),
            static_cast<double>(characteristic), static_cast<double>(descriptor), static_cast<double>(offset), response_needed,
            static_cast<double>(value_id), static_cast<double>(size));
    }

    // A scan the backend ended: a stop the core asked for is never reported,
    // so this is the scan's last event.
    void dispatch_scan_stopped(const BackendEvent& event)
    {
        const ScanSession ended = end_scan(event.transport);
        if (!ended.running)
        {
            g_dropped_events++;
            return;
        }
        fire_stopped("scan on_stopped", ended.on_stopped, event.error, event.message);
    }

    void dispatch_le_advertise_stopped(const BackendEvent& event)
    {
        const AdvertiseSession ended = end_session(g_advertise);
        if (!ended.running)
        {
            g_dropped_events++;
            return;
        }
        const Error error = event.error != Error::Ok ? event.error : Error::OperationFailed;
        fire_stopped("advertise on_stopped", ended.on_stopped, error, event.message);
    }

    void dispatch_classic_discoverable_stopped(const BackendEvent& event)
    {
        const DiscoverableSession ended = end_session(g_discoverable);
        if (!ended.running)
        {
            g_dropped_events++;
            return;
        }
        fire_stopped("classic_discoverable on_stopped", ended.on_stopped, event.error, event.message);
    }

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

    // An inbound link the backend accepted, already given its connection
    // through create_classic_connection.
    void dispatch_classic_client_connected(const BackendEvent& event)
    {
        GMFunction on_connected;
        {
            std::scoped_lock lock(g_session_mutex);
            if (g_classic_server.running)
                on_connected = g_classic_server.on_connected;
        }
        // on_connected(connection, device)
        invoke_callback("classic_server on_connected", counted(on_connected),
            static_cast<double>(event.connection), static_cast<double>(event.device));
    }

    // A handle bluetooth_classic_disconnect already retired has had its end:
    // the backend's late report of that close fires nothing, the same as for
    // LE (R1-77), and its late bytes go back to the receive budget.
    void dispatch_classic_data(BackendEvent& event)
    {
        const bool valid = g_classic_connection_manager.is_valid(event.connection);
        const GMFunction on_data = valid ? g_classic_connection_manager.get_on_data(event.connection) : GMFunction();
        if (!on_data)
        {
            g_dropped_events++;
            if (g_backend)
                g_backend->classic_receive_consumed(event.connection, event.bytes.size());
            return;
        }

        const std::size_t size = event.bytes.size();
        const std::uint64_t value = store_value(std::move(event.bytes), ValueOwner::Classic, event.connection);
        // on_data(connection, value, size)
        invoke_callback("classic on_data", on_data, static_cast<double>(event.connection), static_cast<double>(value),
            static_cast<double>(size));
    }

    void dispatch_classic_disconnected(const BackendEvent& event)
    {
        // Every byte reached GML as a value before this, so the handle is
        // retired here, as an LE link is (R1-245).
        const auto record = g_classic_connection_manager.take_connection(event.connection);
        if (!record)
        {
            g_dropped_events++;
            return;
        }

        if (const GMFunction connect_callback = g_classic_connection_manager.take_connect_callback(event.connection))
        {
            // callback(error_code, message, connection, device)
            invoke_callback("classic_connect", connect_callback, static_cast<double>(Error::ConnectionFailed),
                event.message, static_cast<double>(event.connection), static_cast<double>(record->device));
            return;
        }
        const Error error = event.error != Error::Ok ? event.error : Error::Disconnected;
        // on_disconnected(error_code, message, connection)
        invoke_callback("classic on_disconnected", counted(record->on_disconnected), static_cast<double>(error),
            event.message, static_cast<double>(event.connection));
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

    // The devices a query found enter the cache without any scan callback,
    // like a server central, and the callback gets their handles.
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
                const double handle = static_cast<double>(g_device_manager.upsert_device(device, false));
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
            return scan_saw_device(device);
        };
        hooks.add_device = [](const DiscoveredDevice& device) {
            return g_device_manager.upsert_device(device, false);
        };
        // An inbound link serves the running server's on_data and
        // on_disconnected, and keeps them after the server stops.
        hooks.create_classic_connection = [](std::uint64_t device) {
            ConnectionRecord record;
            record.device = device;
            {
                std::scoped_lock lock(g_session_mutex);
                if (g_classic_server.running)
                {
                    record.on_data = g_classic_server.on_data;
                    record.on_disconnected = g_classic_server.on_disconnected;
                }
            }
            const std::uint64_t connection = g_classic_connection_manager.create_connection(std::move(record));
            GMBT_LOG("classic connection created: connection=%llu device=%llu",
                static_cast<unsigned long long>(connection),
                static_cast<unsigned long long>(device));
            return connection;
        };
        hooks.create_le_connection = [](std::uint64_t device) {
            ConnectionRecord record;
            record.device = device;
            const std::uint64_t connection = g_le_connection_manager.create_connection(std::move(record));
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
                case BackendEventType::LeAdvertiseStopped:
                    dispatch_le_advertise_stopped(event);
                    return;
                case BackendEventType::ClassicDiscoverableStopped:
                    dispatch_classic_discoverable_stopped(event);
                    return;
                case BackendEventType::LeServerReadRequest:
                    dispatch_le_server_read_request(event);
                    return;
                case BackendEventType::LeServerWriteRequest:
                    dispatch_le_server_write_request(event);
                    return;
                case BackendEventType::LeServerServicesReset:
                {
                    // The services are gone with the radio: the server ends as
                    // the radio-off rule ends it (R2-85).
                    std::string ignored;
                    end_le_server(true, Error::BluetoothDisabled, "The platform dropped the GATT server's services",
                        ignored);
                    return;
                }
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
    // arguments (R1-64).
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

    // Every event callback goes without firing (rule 7); state_changed stays.
    end_session(g_le_scan);
    end_session(g_classic_scan);
    end_session(g_advertise);
    end_session(g_le_server);
    end_session(g_classic_server);
    end_session(g_discoverable);
    { std::scoped_lock lock(g_subscriptions_mutex); g_subscriptions.clear(); }
    { std::scoped_lock lock(g_discovery_jobs_mutex); g_discovery_jobs.clear(); }
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

bool bluetooth_feature_is_supported(BluetoothFeature feature)
{
    if (!g_backend)
        return false;

    // The roles come from the backend's supports_* calls, the rest from its
    // feature list (R1-79).
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
    if (g_backend->current_bluetooth_state() == k_state_powered_on)
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

// --- Scans ---

BluetoothError bluetooth_le_scan_start(const std::optional<BluetoothLeScanSettings>& settings,
    const std::optional<std::vector<BluetoothLeScanFilter>>& filters, const gm::wire::GMFunction& on_found,
    const std::optional<gm::wire::GMFunction>& on_updated, const std::optional<gm::wire::GMFunction>& on_stopped)
{
    GMBT_LOG("BLE scan start requested (filters=%zu)", filters ? filters->size() : std::size_t{ 0 });

    if (!require_backend())
        return to_gm(g_last_error);

    LeScanSettings scan_settings;
    std::chrono::milliseconds update_interval(1000);
    if (settings)
    {
        if (settings->active)
            scan_settings.active = *settings->active;
        if (settings->mode)
        {
            const auto mode = static_cast<std::int32_t>(*settings->mode);
            if (mode < static_cast<std::int32_t>(LeScanMode::LowPower) || mode > static_cast<std::int32_t>(LeScanMode::LowLatency))
                return fail(Error::InvalidArgument, "mode must be a BluetoothLeScanMode");
            scan_settings.mode = static_cast<LeScanMode>(mode);
        }
        if (settings->update_interval_ms)
        {
            const std::int32_t ms = *settings->update_interval_ms;
            if (ms < 0 || ms > 60000)
                return fail(Error::InvalidArgument, "update_interval_ms must be 0-60000, got " + std::to_string(ms));
            update_interval = std::chrono::milliseconds(ms);
        }
    }

    std::vector<LeScanFilter> checked;
    if (filters)
    {
        checked.reserve(filters->size());
        for (const auto& filter : *filters)
        {
            LeScanFilter out;
            if (!check_scan_filter(filter, out))
                return to_gm(g_last_error);
            checked.push_back(std::move(out));
        }
    }

    if (!check_radio())
        return to_gm(g_last_error);

    if (session_running(g_le_scan))
        return fail(Error::Busy, "An LE scan is already running; stop it first");

    // In place before the backend starts: results can arrive before it
    // returns. Starts run on the game thread only, and no result is taken
    // while no scan runs, so nothing reads the filters in between.
    g_scan_filters.start(checked);

    ScanSession session;
    session.on_found = on_found;
    session.on_updated = optional_callback(on_updated);
    session.on_stopped = optional_callback(on_stopped);
    session.update_interval = update_interval;
    if (!begin_session(g_le_scan, std::move(session)))
        return fail(Error::Busy, "An LE scan is already running; stop it first");

    std::string message;
    const Error error = g_backend->le_scan_start(scan_settings, checked, message);
    set_last_error(error, message);
    if (error != Error::Ok)
        end_scan(Transport::LowEnergy);
    GMBT_LOG("BLE scan start -> error=%d message='%s'", static_cast<int>(error), message.c_str());
    return to_gm(error);
}

BluetoothError bluetooth_le_scan_stop()
{
    if (!require_backend())
        return to_gm(g_last_error);

    // Ended before the backend is asked, so nothing it reports for the scan
    // reaches GML (rule 3).
    if (!end_scan(Transport::LowEnergy).running)
        return to_gm(Error::Ok);

    std::string message;
    const Error error = g_backend->le_scan_stop(message);
    set_last_error(error, message);
    GMBT_LOG("BLE scan stop -> error=%d message='%s'", static_cast<int>(error), message.c_str());
    return to_gm(error);
}

bool bluetooth_le_scan_is_running()
{
    const bool running = session_running(g_le_scan);

    // Polled from Step and Draw every frame - only speak up on a transition.
    static int last_running = -1;
    if (static_cast<int>(running) != last_running)
    {
        GMBT_LOG("BLE scan running state changed: %d -> %d", last_running, running ? 1 : 0);
        last_running = static_cast<int>(running);
    }

    return running;
}

BluetoothError bluetooth_classic_scan_start(const gm::wire::GMFunction& on_found,
    const std::optional<gm::wire::GMFunction>& on_updated, const std::optional<gm::wire::GMFunction>& on_stopped)
{
    if (!require_backend())
        return to_gm(g_last_error);

    if (!check_radio())
        return to_gm(g_last_error);

    // No throttle: a Classic device answers an inquiry rarely, and a resolved
    // name is worth reporting at once.
    ScanSession session;
    session.on_found = on_found;
    session.on_updated = optional_callback(on_updated);
    session.on_stopped = optional_callback(on_stopped);
    if (!begin_session(g_classic_scan, std::move(session)))
        return fail(Error::Busy, "A Classic scan is already running; stop it first");

    std::string message;
    const Error error = g_backend->classic_scan_start(message);
    set_last_error(error, message);
    if (error != Error::Ok)
        end_scan(Transport::Classic);
    return to_gm(error);
}

BluetoothError bluetooth_classic_scan_stop()
{
    if (!require_backend())
        return to_gm(g_last_error);

    if (!end_scan(Transport::Classic).running)
        return to_gm(Error::Ok);

    std::string message;
    const Error error = g_backend->classic_scan_stop(message);
    set_last_error(error, message);
    return to_gm(error);
}

bool bluetooth_classic_scan_is_running()
{
    return session_running(g_classic_scan);
}

// --- Device cache ---

void bluetooth_device_clear()
{
    GMBT_LOG("clearing device cache (%d cached)", g_device_manager.get_count());
    g_device_manager.clear();
    g_scan_filters.clear_pending();

    // Handles are never reused, so what the scans announced names nothing now.
    std::scoped_lock lock(g_session_mutex);
    g_le_scan.announced.clear();
    g_classic_scan.announced.clear();
}

std::int32_t bluetooth_device_get_count()
{
    return g_device_manager.get_count();
}

std::uint64_t bluetooth_device_get_at(std::int32_t index)
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

std::optional<std::string> bluetooth_device_get_address(std::uint64_t device)
{
    const auto dev = g_device_manager.get_device(device);
    if (!dev || !dev->address_available)
        return std::nullopt;
    return dev->address;
}

std::optional<std::int32_t> bluetooth_device_get_rssi(std::uint64_t device)
{
    const auto dev = g_device_manager.get_device(device);
    if (!dev || !dev->rssi_available)
        return std::nullopt;
    return dev->rssi;
}

std::optional<std::int32_t> bluetooth_device_get_last_seen_age(std::uint64_t device)
{
    const auto age = g_device_manager.last_seen_age(device);
    if (!age)
        return std::nullopt;
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(*age).count();
    return static_cast<std::int32_t>(std::min<long long>(ms, INT32_MAX));
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

    const std::uint64_t handle = g_device_manager.upsert_device(device, false);
    GMBT_LOG("device from id '%s' -> handle=%llu", key.c_str(), static_cast<unsigned long long>(handle));
    return handle;
}

BluetoothError bluetooth_le_connected_devices_query(const std::vector<std::string_view>& service_uuids, const gm::wire::GMFunction& callback)
{
    if (!require_backend())
        return to_gm(g_last_error);

    // The same answer everywhere, though only Apple can filter (R2-128).
    if (service_uuids.empty())
        return fail(Error::InvalidArgument, "service_uuids must name at least one service");

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

// --- Bluetooth Classic ---

std::uint64_t bluetooth_classic_connect(std::uint64_t device, std::string_view service_uuid,
    const std::optional<BluetoothConnectOptions>& options, const gm::wire::GMFunction& on_data,
    const gm::wire::GMFunction& on_disconnected, const gm::wire::GMFunction& callback)
{
    if (!require_backend())
        return 0;

    std::chrono::milliseconds timeout;
    const auto dev = check_device(device, Transport::Classic);
    if (!dev || !check_uuid(service_uuid) || !connect_timeout(options, timeout) || !check_radio())
        return 0;

    ConnectionRecord record;
    record.device = device;
    record.on_data = on_data;
    record.on_disconnected = on_disconnected;
    const std::uint64_t connection = g_classic_connection_manager.create_connection(std::move(record));
    g_classic_connection_manager.set_connect_callback(connection, callback);

    std::string message;
    const Error error = g_backend->classic_connect(connection, *dev, canonical_uuid(service_uuid), timeout, message);
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

    // Retired before the backend is asked, so its report of the close finds
    // nothing (rule 3). A connect still in flight is cancelled, not
    // completed: its callback fires now, and a late completion finds none.
    const GMFunction connect_callback = g_classic_connection_manager.take_connect_callback(connection);
    const auto record = g_classic_connection_manager.take_connection(connection);
    if (!record)
        return fail(Error::InvalidHandle, "Invalid Bluetooth Classic connection handle");

    std::string message;
    const Error error = g_backend->classic_disconnect(connection, message);
    set_last_error(error, message);

    // callback(error_code, message, connection, device)
    invoke_callback("classic_connect", connect_callback, static_cast<double>(Error::ConnectionFailed),
        std::string("Connection cancelled by bluetooth_classic_disconnect"), static_cast<double>(connection),
        static_cast<double>(record->device));

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

BluetoothError bluetooth_classic_server_start(std::string_view name, std::string_view service_uuid,
    const gm::wire::GMFunction& on_connected, const gm::wire::GMFunction& on_data,
    const gm::wire::GMFunction& on_disconnected, const std::optional<gm::wire::GMFunction>& on_stopped)
{
    if (!require_backend())
        return to_gm(g_last_error);

    if (!g_backend->supports_classic_server())
        return fail(Error::NotSupported, "A Bluetooth Classic server is not supported on this platform");

    if (!check_uuid(service_uuid) || !check_radio())
        return to_gm(g_last_error);

    // In place before the backend starts: a link can arrive before it returns.
    ClassicServerSession session;
    session.on_connected = on_connected;
    session.on_data = on_data;
    session.on_disconnected = on_disconnected;
    session.on_stopped = optional_callback(on_stopped);
    if (!begin_session(g_classic_server, std::move(session)))
        return fail(Error::Busy, "A Classic server is already running; stop it first");

    std::string message;
    const Error error = g_backend->classic_server_start(std::string(name), canonical_uuid(service_uuid), message);
    set_last_error(error, message);
    if (error != Error::Ok)
        end_session(g_classic_server);
    return to_gm(error);
}

// The links the server accepted stay open, with their callbacks.
BluetoothError bluetooth_classic_server_stop()
{
    if (!require_backend())
        return to_gm(g_last_error);

    if (!end_session(g_classic_server).running)
        return to_gm(Error::Ok);

    std::string message;
    const Error error = g_backend->classic_server_stop(message);
    set_last_error(error, message);
    return to_gm(error);
}

bool bluetooth_classic_server_is_running()
{
    return session_running(g_classic_server);
}

BluetoothError bluetooth_classic_discoverable_start(std::int32_t duration_seconds, const gm::wire::GMFunction& callback,
    const std::optional<gm::wire::GMFunction>& on_stopped)
{
    if (!require_backend())
        return to_gm(g_last_error);

    if (!check_feature(BluetoothFeature::ClassicDiscoverable, "Making the device discoverable is not supported on this platform"))
        return to_gm(g_last_error);

    if (!check_radio())
        return to_gm(g_last_error);

    if (g_backend->classic_discoverable_is_running())
        return fail(Error::Busy, "The device is already discoverable");

    // The same bounds everywhere, Android's (R2-130).
    const std::int32_t granted = duration_seconds <= 0 ? 120 : std::clamp(duration_seconds, 1, 3600);

    // Running before the backend starts, so an end it reports at once finds it.
    DiscoverableSession session;
    session.on_stopped = optional_callback(on_stopped);
    if (!begin_session(g_discoverable, std::move(session)))
        return fail(Error::Busy, "The device is already discoverable");

    std::string message;
    const Error error = g_backend->classic_discoverable_start(granted, message);
    set_last_error(error, message);
    if (error != Error::Ok)
    {
        end_session(g_discoverable);
        return to_gm(error);
    }

    // The backends here grant it at once (R2-129).
    // callback(error_code, message, duration_seconds)
    invoke_callback("classic_discoverable_start", callback, static_cast<double>(Error::Ok), std::string(),
        static_cast<double>(granted));
    return to_gm(Error::Ok);
}

BluetoothError bluetooth_classic_discoverable_stop()
{
    if (!require_backend())
        return to_gm(g_last_error);

    // Retired first: an end the backend reports while the stop joins its timer
    // finds nothing to fire.
    end_session(g_discoverable);

    std::string message;
    const Error error = g_backend->classic_discoverable_stop(message);
    set_last_error(error, message);
    return to_gm(error);
}

bool bluetooth_classic_discoverable_is_running()
{
    return g_backend && g_backend->classic_discoverable_is_running();
}

// --- The state callback, the one global slot ---

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

// --- BLE GATT client: connect / disconnect ---

std::uint64_t bluetooth_le_connect(std::uint64_t device, const std::optional<BluetoothConnectOptions>& options,
    const gm::wire::GMFunction& on_disconnected, const gm::wire::GMFunction& callback)
{
    if (!require_backend())
        return 0;

    std::chrono::milliseconds timeout;
    const auto dev = check_device(device, Transport::LowEnergy);
    if (!dev || !connect_timeout(options, timeout) || !check_radio())
        return 0;

    // One client connection per device, connecting or connected (R1-197).
    if (g_le_connection_manager.has_device(device))
    {
        fail(Error::Busy, "The device already has a connection");
        return 0;
    }

    ConnectionRecord record;
    record.device = device;
    record.on_disconnected = on_disconnected;
    const std::uint64_t connection = g_le_connection_manager.create_connection(std::move(record));
    g_le_connection_manager.set_connect_callback(connection, callback);

    std::string message;
    const Error error = g_backend->le_connect(connection, *dev, timeout, message);
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
    // Retired before the backend is asked, so its report of the close finds
    // nothing (rule 3), and no op waiting on the link outlives the call.
    const auto record = retire_le_connection(connection);
    if (!record)
        return fail(Error::InvalidHandle, "Invalid LE connection handle");

    std::string message;
    const Error error = g_backend->le_disconnect(connection, message);
    set_last_error(error, message);

    // A connect still in flight is cancelled, not completed: its callback
    // fires now, and a late open event from the backend finds none.
    // callback(error_code, message, connection, device)
    invoke_callback("le_connect", g_le_connection_manager.take_connect_callback(connection),
        static_cast<double>(Error::ConnectionFailed), std::string("Connection cancelled by bluetooth_le_disconnect"),
        static_cast<double>(connection), static_cast<double>(record->device));

    return to_gm(error);
}

// The connection getters answer for a server connection too (R2-32, R2-119).
bool bluetooth_le_connection_is_valid(std::uint64_t connection)
{
    return g_le_connection_manager.is_valid(connection) || find_server_central(connection).has_value();
}

bool bluetooth_le_connection_is_connected(std::uint64_t connection)
{
    if (g_le_connection_manager.is_valid(connection))
        return g_backend && g_backend->le_connection_is_connected(connection);
    return find_server_central(connection).has_value();
}

std::uint64_t bluetooth_le_connection_get_device(std::uint64_t connection)
{
    if (const std::uint64_t device = g_le_connection_manager.get_device(connection))
        return device;
    const auto central = find_server_central(connection);
    return central ? central->device : 0;
}

std::int32_t bluetooth_le_connection_get_mtu(std::uint64_t connection)
{
    if (!g_backend)
        return 0;
    if (g_le_connection_manager.is_valid(connection))
        return g_backend->le_connection_is_connected(connection) ? g_backend->le_connection_mtu(connection) : 0;

    std::string key;
    if (!find_server_central(connection, &key))
        return 0;
    return g_backend->le_server_connection_mtu(key);
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

BluetoothError bluetooth_le_services_discover(std::uint64_t connection, const std::optional<BluetoothLeDiscoverOptions>& options,
    const gm::wire::GMFunction& callback)
{
    if (!check_le_client_connection(connection))
        return to_gm(g_last_error);

    DiscoveryJob job;
    job.callback = callback;
    job.connection = connection;
    if (options)
    {
        if (options->service_uuids)
        {
            for (const auto& uuid : *options->service_uuids)
            {
                if (!check_uuid(uuid))
                    return to_gm(g_last_error);
                job.service_uuids.push_back(canonical_uuid(uuid));
            }
        }
        job.descriptors = options->descriptors.value_or(false);
        job.characteristics = job.descriptors || options->characteristics.value_or(false);
    }

    if (!job.characteristics)
    {
        return start_le_op(LeOpKind::ServicesDiscover, callback, connection, connection, [&](std::uint64_t op_id, std::string& message) {
            return g_backend->le_services_discover(op_id, connection, message);
        });
    }

    // The deeper levels chain from the services step's completion (R2-35).
    std::uint64_t job_id = 0;
    {
        std::scoped_lock lock(g_discovery_jobs_mutex);
        job_id = g_next_discovery_job++;
        g_discovery_jobs.emplace(job_id, std::move(job));
    }

    std::string message;
    const Error error = start_discovery_step(job_id, LeOpKind::ServicesDiscover, connection, connection, message);
    if (error != Error::Ok)
    {
        // Failed before it started: the callback never fires.
        std::scoped_lock lock(g_discovery_jobs_mutex);
        g_discovery_jobs.erase(job_id);
    }
    set_last_error(error, message);
    return to_gm(error);
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

std::uint64_t bluetooth_le_service_find(std::uint64_t connection, std::string_view uuid)
{
    if (!g_le_connection_manager.is_valid(connection) && !find_server_central(connection))
    {
        fail(Error::InvalidHandle, "Invalid LE connection handle");
        return 0;
    }
    if (!check_uuid(uuid))
        return 0;

    const std::string canonical = canonical_uuid(uuid);
    return found(g_service_cache.find_by_uuid(connection, canonical), "discovered service", canonical);
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

std::uint64_t bluetooth_le_characteristic_find(std::uint64_t service, std::string_view uuid)
{
    const auto server = find_le_server_attribute(service, HandleKind::LeServerService);
    if (!server && !g_service_cache.is_valid(service))
    {
        fail(Error::InvalidHandle, "Invalid service handle");
        return 0;
    }
    if (!check_uuid(uuid))
        return 0;

    const std::string canonical = canonical_uuid(uuid);
    const std::uint64_t handle = server
        ? find_le_server_child(*server, canonical)
        : g_characteristic_cache.find_by_uuid(service, canonical);
    return found(handle, "characteristic", canonical);
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

std::uint64_t bluetooth_le_descriptor_find(std::uint64_t characteristic, std::string_view uuid)
{
    const auto server = find_le_server_attribute(characteristic, HandleKind::LeServerCharacteristic);
    if (!server && !g_characteristic_cache.is_valid(characteristic))
    {
        fail(Error::InvalidHandle, "Invalid characteristic handle");
        return 0;
    }
    if (!check_uuid(uuid))
        return 0;

    const std::string canonical = canonical_uuid(uuid);
    const std::uint64_t handle = server
        ? find_le_server_child(*server, canonical)
        : g_descriptor_cache.find_by_uuid(characteristic, canonical);
    return found(handle, "descriptor", canonical);
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

BluetoothError bluetooth_le_characteristic_subscribe(std::uint64_t characteristic, BluetoothLeSubscribeMode mode,
    const gm::wire::GMFunction& on_value, const gm::wire::GMFunction& callback)
{
    if (!require_backend())
        return to_gm(g_last_error);

    if (!g_characteristic_cache.is_valid(characteristic))
        return fail(Error::InvalidHandle, "Invalid characteristic handle");

    if (mode != BluetoothLeSubscribeMode::Notify && mode != BluetoothLeSubscribeMode::Indicate)
        return fail(Error::InvalidArgument, "mode must be a BluetoothLeSubscribeMode");

    if (mode == BluetoothLeSubscribeMode::Notify &&
        !characteristic_has_property(characteristic, BluetoothLeCharacteristicProperty::Notify))
        return fail(Error::NotSupported, "The characteristic does not support notifications");

    if (mode == BluetoothLeSubscribeMode::Indicate &&
        !characteristic_has_property(characteristic, BluetoothLeCharacteristicProperty::Indicate))
        return fail(Error::NotSupported, "The characteristic does not support indications");

    // In place before the backend is asked: a value can arrive before the
    // subscribe completes.
    const std::uint64_t token = add_subscription(characteristic, on_value);
    if (token == 0)
        return fail(Error::Busy, "The characteristic is already subscribed or subscribing");

    const std::uint64_t connection = characteristic_connection(characteristic);
    const LeAttributeRef ref = characteristic_ref(characteristic);

    PendingLeOp op{ LeOpKind::CharacteristicSubscribe, callback, characteristic, connection };
    op.token = token;
    const BluetoothError result = start_le_op(std::move(op), [&](std::uint64_t op_id, std::string& message) {
        return g_backend->le_characteristic_subscribe(op_id, connection, ref, static_cast<std::int32_t>(mode), message);
    });
    if (result != BluetoothError::Ok)
        drop_subscription(characteristic, token);
    return result;
}

BluetoothError bluetooth_le_characteristic_unsubscribe(std::uint64_t characteristic, const gm::wire::GMFunction& callback)
{
    if (!require_backend())
        return to_gm(g_last_error);

    if (!g_characteristic_cache.is_valid(characteristic))
        return fail(Error::InvalidHandle, "Invalid characteristic handle");

    // on_value stops here, whatever the platform answers.
    if (!drop_subscription(characteristic))
    {
        // callback(error_code, message, characteristic)
        invoke_callback("le_characteristic_unsubscribe", callback, static_cast<double>(Error::Ok), std::string(),
            static_cast<double>(characteristic));
        return to_gm(Error::Ok);
    }

    const std::uint64_t connection = characteristic_connection(characteristic);
    const LeAttributeRef ref = characteristic_ref(characteristic);

    // Mode 0 is the backends' unsubscribe.
    return start_le_op(LeOpKind::CharacteristicUnsubscribe, callback, characteristic, connection, [&](std::uint64_t op_id, std::string& message) {
        return g_backend->le_characteristic_subscribe(op_id, connection, ref, 0, message);
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

// The size the callback reported is what lands in the buffer (R2-120).
BluetoothError bluetooth_value_fetch(std::uint64_t value, struct gm::wire::GMBuffer out_data, unsigned int offset)
{
    std::string message;
    std::size_t copied = 0;
    std::vector<LeftValue> left;
    const Error error = g_values.fetch(value, out_data, offset, copied, message, left);
    set_last_error(error, message);
    settle_left_values(left);
    return to_gm(error);
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

BluetoothError bluetooth_le_advertise_start(const std::optional<BluetoothLeAdvertiseSettings>& settings,
    const BluetoothLeAdvertiseData& data, const std::optional<gm::wire::GMFunction>& on_stopped,
    const gm::wire::GMFunction& callback)
{
    if (!require_backend())
        return to_gm(g_last_error);

    // Checked once here, so every backend sees only what it can act on; what a
    // backend cannot send is its own NotSupported (R1-36). An undefined field
    // is off or empty, except connectable (R2-49).
    LeAdvertiseSettings advertise_settings;
    if (settings)
    {
        advertise_settings.connectable = settings->connectable.value_or(true);
        if (settings->tx_power)
        {
            const auto level = static_cast<std::int32_t>(*settings->tx_power);
            if (level < static_cast<std::int32_t>(LeAdvertiseTxPower::UltraLow) || level > static_cast<std::int32_t>(LeAdvertiseTxPower::High))
                return fail(Error::InvalidArgument, "tx_power must be a BluetoothLeAdvertiseTxPower");
            advertise_settings.tx_power = static_cast<LeAdvertiseTxPower>(level);
        }
    }

    LeAdvertiseData advertise_data;
    advertise_data.include_name = data.include_name.value_or(false);
    advertise_data.include_tx_power = data.include_tx_power.value_or(false);

    if (data.service_uuids)
    {
        for (const auto& uuid : *data.service_uuids)
        {
            if (!check_uuid(uuid))
                return to_gm(Error::InvalidArgument);
            advertise_data.service_uuids.push_back(canonical_uuid(uuid));
        }
    }

    if (data.service_data)
    {
        for (const auto& entry : *data.service_data)
        {
            if (!check_uuid(entry.uuid))
                return to_gm(Error::InvalidArgument);
            const std::string uuid = canonical_uuid(entry.uuid);
            // A UUID with data is advertised as a UUID too, on every platform.
            if (std::find(advertise_data.service_uuids.begin(), advertise_data.service_uuids.end(), uuid) == advertise_data.service_uuids.end())
                return fail(Error::InvalidArgument, "Service data for " + uuid + " needs the uuid in service_uuids too");
            advertise_data.service_data.push_back({ uuid, entry.data });
        }
    }

    if (data.manufacturer_data)
    {
        for (const auto& entry : *data.manufacturer_data)
        {
            if (entry.company_id < 0 || entry.company_id > 0xFFFF)
                return fail(Error::InvalidArgument, "company_id must be 0-65535, got " + std::to_string(entry.company_id));
            advertise_data.manufacturer_data.push_back({ static_cast<std::uint16_t>(entry.company_id), entry.data });
        }
    }

    if (!check_radio())
        return to_gm(g_last_error);

    // Busy while starting or running: the new settings, data and callbacks
    // would otherwise be dropped (R1-72).
    AdvertiseSession session;
    session.token = g_next_advertise_token++;
    session.on_stopped = optional_callback(on_stopped);
    const std::uint64_t token = session.token;
    if (!begin_session(g_advertise, std::move(session)))
        return fail(Error::Busy, "Bluetooth LE advertising is already running; stop it first");

    PendingLeOp op{ LeOpKind::AdvertiseStart, callback };
    op.token = token;
    const BluetoothError result = start_le_op(std::move(op), [&](std::uint64_t op_id, std::string& message) {
        return g_backend->le_advertise_start(op_id, advertise_settings, advertise_data, message);
    });
    if (result != BluetoothError::Ok)
        end_advertise_session_of(token);
    return result;
}

BluetoothError bluetooth_le_advertise_stop()
{
    if (!require_backend())
        return to_gm(g_last_error);

    if (!end_session(g_advertise).running)
        return to_gm(Error::Ok);

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

BluetoothError bluetooth_le_server_start(const gm::wire::GMFunction& on_connected, const gm::wire::GMFunction& on_disconnected,
    const gm::wire::GMFunction& on_read, const gm::wire::GMFunction& on_write, const std::optional<gm::wire::GMFunction>& on_stopped)
{
    if (!require_backend())
        return to_gm(g_last_error);

    if (!check_radio())
        return to_gm(g_last_error);

    // In place before the backend starts: a central can arrive before it
    // returns.
    LeServerSession session;
    session.on_connected = on_connected;
    session.on_disconnected = on_disconnected;
    session.on_read = on_read;
    session.on_write = on_write;
    session.on_stopped = optional_callback(on_stopped);
    if (!begin_session(g_le_server, std::move(session)))
        return fail(Error::Busy, "The LE server is already running; stop it first");

    std::string message;
    const Error error = g_backend->le_server_start(message);
    set_last_error(error, message);
    if (error != Error::Ok)
        end_session(g_le_server);
    return to_gm(error);
}

// Ends advertising too (R2-20), and fires nothing (rule 3).
BluetoothError bluetooth_le_server_stop()
{
    if (!require_backend())
        return to_gm(g_last_error);

    std::string message;
    const Error error = end_le_server(false, Error::Ok, std::string(), message);
    set_last_error(error, message);
    return to_gm(error);
}

bool bluetooth_le_server_is_running()
{
    return session_running(g_le_server);
}

BluetoothError bluetooth_le_server_add_service(const BluetoothLeServiceDefinition& service, const gm::wire::GMFunction& callback)
{
    if (!require_backend())
        return to_gm(g_last_error);

    if (!check_uuid(service.uuid))
        return to_gm(Error::InvalidArgument);
    if (service.characteristics)
    {
        for (const auto& characteristic : *service.characteristics)
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
            if (characteristic.value && characteristic.value->size() > k_max_attribute_value)
            {
                return fail(Error::InvalidArgument, "Characteristic " + characteristic.uuid + ": the initial value is " +
                    std::to_string(characteristic.value->size()) + " bytes; an attribute value holds at most 512");
            }
            if (!characteristic.descriptors)
                continue;
            for (const auto& descriptor : *characteristic.descriptors)
            {
                if (!check_uuid(descriptor.uuid))
                    return to_gm(Error::InvalidArgument);
                if (descriptor.value && descriptor.value->size() > k_max_attribute_value)
                {
                    return fail(Error::InvalidArgument, "Descriptor " + descriptor.uuid + ": the value is " +
                        std::to_string(descriptor.value->size()) + " bytes; an attribute value holds at most 512");
                }
            }
        }
    }

    // The server pre-flight every platform shares (R1-223).
    if (!session_running(g_le_server))
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
    if (!session_running(g_le_server))
        return to_gm(Error::Ok);

    // The services the requests and the queued adds name are going away.
    answer_pending_le_server_requests();
    fail_le_ops(g_le_ops.take_kind(LeOpKind::ServerAddService), Error::OperationFailed,
        "The LE server's services were cleared before the service was added");

    std::string message;
    const Error error = g_backend->le_server_clear_services(message);
    set_last_error(error, message);
    forget_le_server_services();
    return to_gm(error);
}

BluetoothError bluetooth_le_server_respond_read(std::int32_t request_id, struct gm::wire::GMBuffer data, unsigned int offset, unsigned int size)
{
    if (!require_backend())
        return to_gm(g_last_error);

    // Checked before the request is erased, so a call with a bad id, kind or
    // range can be retried instead of leaving the remote central to time out.
    if (!check_le_server_request(request_id, false, "bluetooth_le_server_respond_write"))
        return to_gm(g_last_error);

    if (!buffer_range_valid(data, offset, size, "bluetooth_le_server_respond_read"))
        return to_gm(Error::InvalidArgument);

    if (!take_le_server_request(request_id))
        return to_gm(g_last_error);

    const std::vector<std::uint8_t> value = slice_bytes(data, offset, size);

    std::string message;
    const Error error = g_backend->le_server_respond_read(request_id, static_cast<std::int32_t>(BluetoothAttError::Success),
        value, message);
    set_last_error(error, message);
    return to_gm(error);
}

// A refusal carries no value, so it needs no buffer (R2-122).
BluetoothError bluetooth_le_server_respond_read_error(std::int32_t request_id, BluetoothAttError error_code)
{
    if (!require_backend())
        return to_gm(g_last_error);

    if (!check_att_error(error_code, "bluetooth_le_server_respond_read_error"))
        return to_gm(Error::InvalidArgument);
    if (error_code == BluetoothAttError::Success)
        return fail(Error::InvalidArgument, "bluetooth_le_server_respond_read_error: Success is answered with bluetooth_le_server_respond_read");

    if (!check_le_server_request(request_id, false, "bluetooth_le_server_respond_write"))
        return to_gm(g_last_error);

    if (!take_le_server_request(request_id))
        return to_gm(g_last_error);

    std::string message;
    const Error error = g_backend->le_server_respond_read(request_id, static_cast<std::int32_t>(error_code), {}, message);
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
        return to_gm(Error::Ok);

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
