#include "GMBluetooth_native.h"
#include "GMBluetooth_backend.h"
#include "GMBluetooth_log.h"
#include "GMBluetooth_json.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstring>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
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

    // The generated GML wrapper only checks buffer_exists, so offset and size are
    // validated here against the buffer's real length - the same rule as
    // Android's bufferRangeInvalid.
    bool buffer_range_valid(const GMBuffer& buffer, unsigned int offset, unsigned int size, const char* function_name)
    {
        if (static_cast<std::uint64_t>(offset) + size <= buffer.length())
            return true;

        g_last_error = Error::InvalidArgument;
        g_last_error_message = std::string("Invalid buffer offset/size for ") + function_name;
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

    // Connect callbacks are one-shot and tied to a specific connection handle,
    // not a persistently-registered callback like the others.
    std::mutex g_pending_connect_mutex;
    std::unordered_map<std::uint64_t, GMFunction> g_pending_connect_callbacks;

    // Same idea as g_pending_connect_callbacks, kept separate because LE and
    // Classic connections are minted from two independent handle spaces.
    std::mutex g_pending_le_connect_mutex;
    std::unordered_map<std::uint64_t, GMFunction> g_pending_le_connect_callbacks;

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
            try
            {
                // callback(error_code, message, status)
                callback.call(static_cast<double>(error), message, static_cast<double>(status));
            }
            catch (const std::exception& e)
            {
                GMBT_LOG("Error dispatching permission_request callback: %s", e.what());
            }
        }
    }

    // Device handles count up and are never reused: clear() forgets every
    // device, and one found again afterwards gets a new handle, so a handle a
    // connection or a pending pair still holds can never name another device.
    class DeviceManager
    {
    public:
        std::uint64_t upsert_device(const DiscoveredDevice& device)
        {
            std::scoped_lock lock(mutex_);
            if (const auto it = by_id_.find(device.id); it != by_id_.end())
            {
                merge(devices_.at(it->second), device);
                return it->second;
            }

            const std::uint64_t handle = next_handle_++;
            devices_.emplace(handle, device);
            by_id_.emplace(device.id, handle);
            order_.push_back(handle);
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

    private:
        // An advertisement or a scan response carries only some fields (no
        // LocalName, connectable false), so an update overwrites only what it
        // actually carries.
        static void merge(DiscoveredDevice& stored, const DiscoveredDevice& update)
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
        }

        mutable std::mutex mutex_;
        std::unordered_map<std::uint64_t, DiscoveredDevice> devices_;
        std::unordered_map<std::string, std::uint64_t> by_id_;
        std::vector<std::uint64_t> order_;
        std::uint64_t next_handle_ = 1;
    };

    DeviceManager g_device_manager;

    // A Classic handle outlives a remote hang-up while the backend still holds
    // bytes the game has not read (R1-25), so the disconnect event only marks
    // it closed; the game-thread calls retire it once nothing is left.
    class ClassicConnectionManager
    {
    public:
        std::uint64_t create_connection(std::uint64_t device)
        {
            std::scoped_lock lock(mutex_);
            const std::uint64_t handle = next_handle_++;
            connections_[handle] = Record{ device, false };
            return handle;
        }

        void remove_connection(std::uint64_t handle)
        {
            std::scoped_lock lock(mutex_);
            connections_.erase(handle);
        }

        void mark_closed(std::uint64_t handle)
        {
            std::scoped_lock lock(mutex_);
            if (const auto it = connections_.find(handle); it != connections_.end())
                it->second.closed = true;
        }

        bool is_closed(std::uint64_t handle) const
        {
            std::scoped_lock lock(mutex_);
            const auto it = connections_.find(handle);
            return it != connections_.end() && it->second.closed;
        }

        // next_handle_ keeps counting, so a handle from before the clear
        // never names a later connection.
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

    private:
        struct Record
        {
            std::uint64_t device = 0;
            bool closed = false;
        };

        mutable std::mutex mutex_;
        std::unordered_map<std::uint64_t, Record> connections_;
        std::uint64_t next_handle_ = 1;
    };

    ClassicConnectionManager g_classic_connection_manager;

    // Counts every event the backend hands up that the core then drops, so the
    // gap between "the backend is working" and "GML sees nothing" is visible in
    // the log instead of having to be inferred.
    std::atomic<std::uint64_t> g_dropped_events{ 0 };

    class LeConnectionManager
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

        // A handle from the same counter that names no client connection: the
        // server side's connection to a remote central.
        std::uint64_t reserve_handle()
        {
            std::scoped_lock lock(mutex_);
            return next_handle_++;
        }

        // next_handle_ keeps counting, so a handle from before the clear
        // never names a later connection.
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

    private:
        mutable std::mutex mutex_;
        std::unordered_map<std::uint64_t, std::uint64_t> connections_;
        std::uint64_t next_handle_ = 1;
    };

    LeConnectionManager g_le_connection_manager;

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

        g_last_error = Error::InvalidArgument;
        g_last_error_message = "Invalid UUID: " + std::string(uuid);
        return false;
    }

    // Parent-scoped handle caches for GATT services/characteristics/descriptors.
    // Handles count up and are never reused, like device handles; an entry
    // leaves when its LE connection is retired, so a reconnect does not pile
    // fresh entries on top of the old ones. The map is ordered by handle, which
    // is creation order, so get_at enumerates in discovery order. find_or_insert
    // is idempotent so re-running discovery doesn't mint duplicates. UUIDs are
    // stored canonical, the one form GML ever sees (R1-14).
    template <typename Entry>
    class AttributeCache
    {
    public:
        std::uint64_t find_by_uuid(std::uint64_t parent, const std::string& uuid) const
        {
            const std::string key = canonical_uuid(uuid);
            std::scoped_lock lock(mutex_);
            for (const auto& [handle, entry] : entries_)
            {
                if (entry.parent == parent && entry.uuid == key)
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

        void clear()
        {
            std::scoped_lock lock(mutex_);
            entries_.clear();
        }

    protected:
        // Caller holds mutex_. Returns the existing entry's handle or a new one.
        std::uint64_t find_or_insert_locked(std::uint64_t parent, const std::string& uuid, Entry*& entry)
        {
            const std::string key = canonical_uuid(uuid);
            for (auto& [handle, existing] : entries_)
            {
                if (existing.parent == parent && existing.uuid == key)
                {
                    entry = &existing;
                    return handle;
                }
            }

            const std::uint64_t handle = next_handle_++;
            Entry& inserted = entries_[handle];
            inserted.parent = parent;
            inserted.uuid = key;
            entry = &inserted;
            return handle;
        }

        mutable std::mutex mutex_;
        std::map<std::uint64_t, Entry> entries_;
        std::uint64_t next_handle_ = 1;
    };

    // parent: the LE connection.
    struct ServiceEntry
    {
        std::uint64_t parent = 0;
        std::string uuid;
    };

    class ServiceCache : public AttributeCache<ServiceEntry>
    {
    public:
        std::uint64_t find_or_insert(std::uint64_t connection, const std::string& uuid)
        {
            std::scoped_lock lock(mutex_);
            ServiceEntry* entry = nullptr;
            return find_or_insert_locked(connection, uuid, entry);
        }
    };

    ServiceCache g_service_cache;

    // parent: the service.
    struct CharacteristicEntry
    {
        std::uint64_t parent = 0;
        std::string uuid;
        std::int32_t properties = 0;
    };

    // parent: the characteristic.
    struct DescriptorEntry
    {
        std::uint64_t parent = 0;
        std::string uuid;
    };

    class CharacteristicCache : public AttributeCache<CharacteristicEntry>
    {
    public:
        std::uint64_t find_or_insert(std::uint64_t service, const std::string& uuid, std::int32_t properties)
        {
            std::scoped_lock lock(mutex_);
            CharacteristicEntry* entry = nullptr;
            const std::uint64_t handle = find_or_insert_locked(service, uuid, entry);
            entry->properties = properties;
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

    class DescriptorCache : public AttributeCache<DescriptorEntry>
    {
    public:
        std::uint64_t find_or_insert(std::uint64_t characteristic, const std::string& uuid)
        {
            std::scoped_lock lock(mutex_);
            DescriptorEntry* entry = nullptr;
            return find_or_insert_locked(characteristic, uuid, entry);
        }
    };

    DescriptorCache g_descriptor_cache;

    // The bytes a read or a notification delivered, held under a value id until
    // GML copies or releases them (R1-11): each event carries its own value, so
    // a burst no longer collapses to the last one. Ids count up and are never
    // reused. A value outlives its link, since its callback may still be queued
    // behind the disconnect; the cap bounds what a game that never copies leaves
    // behind, and shutdown frees the rest.
    class ValueStore
    {
    public:
        static constexpr std::size_t k_max_values = 256;

        std::uint64_t add(std::vector<std::uint8_t> bytes)
        {
            std::scoped_lock lock(mutex_);
            const std::uint64_t id = next_id_++;
            values_.emplace(id, std::move(bytes));
            if (values_.size() > k_max_values)
            {
                GMBT_LOG("value store full, dropping value %llu",
                    static_cast<unsigned long long>(values_.begin()->first));
                values_.erase(values_.begin());
            }
            return id;
        }

        // Copies the whole value to out at offset and frees it. Too small a
        // buffer copies nothing and keeps the value, so the call can be retried.
        Error copy(std::uint64_t id, const GMBuffer& out, unsigned int offset, std::string& message)
        {
            std::scoped_lock lock(mutex_);
            const auto it = values_.find(id);
            if (it == values_.end())
            {
                message = "Unknown or already copied value " + std::to_string(id);
                return Error::InvalidHandle;
            }

            const auto& bytes = it->second;
            if (static_cast<std::uint64_t>(offset) + bytes.size() > out.length())
            {
                message = "Buffer too small for value " + std::to_string(id) + ": it needs " +
                    std::to_string(bytes.size()) + " bytes at offset " + std::to_string(offset);
                return Error::InvalidArgument;
            }

            if (!bytes.empty())
                std::memcpy(static_cast<std::uint8_t*>(out.data()) + offset, bytes.data(), bytes.size());
            values_.erase(it);
            return Error::Ok;
        }

        bool release(std::uint64_t id)
        {
            std::scoped_lock lock(mutex_);
            return values_.erase(id) != 0;
        }

        void clear()
        {
            std::scoped_lock lock(mutex_);
            values_.clear();
        }

    private:
        std::mutex mutex_;
        std::map<std::uint64_t, std::vector<std::uint8_t>> values_;
        std::uint64_t next_id_ = 1;
    };

    ValueStore g_values;

    // Drops what discovery cached for one LE connection: its services, their
    // characteristics and those characteristics' descriptors.
    void erase_le_connection_attributes(std::uint64_t connection)
    {
        const auto services = g_service_cache.erase_children({ connection });
        if (services.empty())
            return;
        const auto characteristics = g_characteristic_cache.erase_children(services);
        if (!characteristics.empty())
            g_descriptor_cache.erase_children(characteristics);
    }

    enum class LeOpKind : std::uint8_t
    {
        ServicesDiscover,
        CharacteristicsDiscover,
        DescriptorsDiscover,
        CharacteristicRead,
        CharacteristicWrite,
        CharacteristicSubscribe,
        DescriptorRead,
        DescriptorWrite,
        AdvertiseStart,
        ServerAddService,
    };

    const char* le_op_name(LeOpKind kind)
    {
        switch (kind)
        {
            case LeOpKind::ServicesDiscover:        return "le_services_discover";
            case LeOpKind::CharacteristicsDiscover: return "le_characteristics_discover";
            case LeOpKind::DescriptorsDiscover:     return "le_descriptors_discover";
            case LeOpKind::CharacteristicRead:      return "le_characteristic_read";
            case LeOpKind::CharacteristicWrite:     return "le_characteristic_write";
            case LeOpKind::CharacteristicSubscribe: return "le_characteristic_subscribe";
            case LeOpKind::DescriptorRead:          return "le_descriptor_read";
            case LeOpKind::DescriptorWrite:         return "le_descriptor_write";
            case LeOpKind::AdvertiseStart:          return "le_advertise_start";
            case LeOpKind::ServerAddService:        return "le_server_add_service";
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

    // Fires a taken op's callback with its usual arguments. Called with no lock
    // held, after the op has left the registry. value and size are the read's
    // value id and byte count; 0 for every other op and every failure.
    void fire_le_op(const PendingLeOp& op, Error error, const std::string& message = std::string(),
        std::uint64_t value = 0, std::size_t size = 0)
    {
        if (!op.callback)
            return;

        try
        {
            switch (op.kind)
            {
                case LeOpKind::AdvertiseStart:
                case LeOpKind::ServerAddService:
                    // callback(error_code, message)
                    op.callback.call(static_cast<double>(error), message);
                    break;
                case LeOpKind::CharacteristicRead:
                case LeOpKind::DescriptorRead:
                    // callback(error_code, message, handle, value, size)
                    op.callback.call(static_cast<double>(error), message, static_cast<double>(op.context),
                        static_cast<double>(value), static_cast<double>(size));
                    break;
                default:
                    // callback(error_code, message, handle)
                    op.callback.call(static_cast<double>(error), message, static_cast<double>(op.context));
                    break;
            }
        }
        catch (const std::exception& e)
        {
            GMBT_LOG("Error dispatching %s callback: %s", le_op_name(op.kind), e.what());
        }
    }

    // Settles the op a LeOpCompleted event names: caches what discovery
    // returned, or holds a read's bytes under a new value id, then fires its
    // callback. An id
    // the registry no longer holds - its op was purged on disconnect or
    // shutdown - is logged and dropped.
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

        std::uint64_t value = 0;
        std::size_t size = 0;
        if (event.error == Error::Ok)
        {
            const LeOpResult& result = event.result;
            switch (op->kind)
            {
                case LeOpKind::ServicesDiscover:
                    for (const auto& attribute : result.attributes)
                        g_service_cache.find_or_insert(op->context, attribute.uuid);
                    break;
                case LeOpKind::CharacteristicsDiscover:
                    for (const auto& attribute : result.attributes)
                        g_characteristic_cache.find_or_insert(op->context, attribute.uuid, attribute.properties);
                    break;
                case LeOpKind::DescriptorsDiscover:
                    for (const auto& attribute : result.attributes)
                        g_descriptor_cache.find_or_insert(op->context, attribute.uuid);
                    break;
                case LeOpKind::CharacteristicRead:
                case LeOpKind::DescriptorRead:
                    // Nobody would copy a value for an op with no callback.
                    if (op->callback)
                    {
                        size = result.value.size();
                        value = g_values.add(result.value);
                    }
                    break;
                default:
                    break;
            }
        }

        fire_le_op(*op, event.error, event.message, value, size);
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
        std::string service_uuid;
        std::string characteristic_uuid;
        std::string descriptor_uuid;
        bool is_write = false;
        // False only for a write without response, which the backend has
        // already completed; the entry is kept for GML to read its value.
        bool response_needed = true;
        std::vector<std::uint8_t> write_value;
        std::chrono::steady_clock::time_point received_at = std::chrono::steady_clock::now();
    };

    std::mutex g_pending_le_server_requests_mutex;
    std::unordered_map<std::int32_t, PendingLeServerRequest> g_pending_le_server_requests;

    // ATT error codes the core answers with when GML does not.
    constexpr std::int32_t k_att_request_not_supported = 0x06;
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
            : g_backend->le_server_respond_read(request_id, att_error, std::string(), message);
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
                it = g_pending_le_server_requests.erase(it);
            }
        }

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

        g_last_error = Error::InvalidArgument;
        g_last_error_message = std::string(function_name) + ": error_code " + std::to_string(value) +
            " is not a BluetoothAttError member";
        return false;
    }

    bool check_le_server_request(std::int32_t request_id, bool is_write, const char* other_function)
    {
        std::scoped_lock lock(g_pending_le_server_requests_mutex);
        const auto it = g_pending_le_server_requests.find(request_id);
        if (it == g_pending_le_server_requests.end())
        {
            g_last_error = Error::InvalidHandle;
            g_last_error_message = "Unknown or expired LE server request id " + std::to_string(request_id);
            return false;
        }
        if (it->second.is_write != is_write)
        {
            g_last_error = Error::InvalidArgument;
            g_last_error_message = "LE server request " + std::to_string(request_id) + " is a " +
                (it->second.is_write ? "write" : "read") + " request; answer it with " + other_function;
            return false;
        }
        return true;
    }

    // Removes a checked request; empty if the expiry sweep took it meanwhile.
    std::optional<PendingLeServerRequest> take_le_server_request(std::int32_t request_id)
    {
        std::scoped_lock lock(g_pending_le_server_requests_mutex);
        const auto it = g_pending_le_server_requests.find(request_id);
        if (it == g_pending_le_server_requests.end())
        {
            g_last_error = Error::InvalidHandle;
            g_last_error_message = "Unknown or expired LE server request id " + std::to_string(request_id);
            return std::nullopt;
        }
        PendingLeServerRequest request = std::move(it->second);
        g_pending_le_server_requests.erase(it);
        return request;
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
        GMFunction callback;
        { std::scoped_lock lock(g_callback_mutex); callback = g_callback_le_server_connection_state_changed; }
        if (!callback)
        {
            g_dropped_events++;
            return;
        }

        try
        {
            // callback(connection, connected, device)
            callback.call(static_cast<double>(connection), connected, static_cast<double>(device));
        }
        catch (const std::exception& e)
        {
            GMBT_LOG("Error dispatching le_server_connection_state_changed callback: %s", e.what());
        }
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

    // --- LE event JSON helpers ---

    bool le_event_has_error(const json::Value& root)
    {
        return root.find("error_code") != nullptr;
    }

    std::int32_t le_event_error_code(const json::Value& root)
    {
        const auto* field = root.find("error_code");
        return field ? field->as_int(0) : 0;
    }

    // The BluetoothError a backend already mapped ("error"), or fallback when
    // the event does not carry one.
    Error le_event_error(const json::Value& root, Error fallback)
    {
        const auto* field = root.find("error");
        if (!field)
            return fallback;
        const std::int32_t value = field->as_int(static_cast<std::int32_t>(fallback));
        if (value < static_cast<std::int32_t>(Error::Ok) || value > static_cast<std::int32_t>(Error::InsufficientSecurity))
            return fallback;
        return static_cast<Error>(value);
    }

    std::uint64_t le_event_connection(const json::Value& root)
    {
        const auto* field = root.find("connection");
        return field ? static_cast<std::uint64_t>(field->as_double(0)) : 0;
    }

    std::string le_event_string(const json::Value& root, const char* key)
    {
        const auto* field = root.find(key);
        return field ? field->as_string() : std::string();
    }

    // Several Apple event payloads embed a JSON array/object as a *string*
    // rather than a nested value (e.g. "services", "device") - decode that
    // inner JSON here rather than treating the field as opaque text.
    std::optional<json::Value> le_event_nested(const json::Value& root, const char* key)
    {
        const auto* field = root.find(key);
        if (!field || !field->is_string())
            return std::nullopt;
        return json::parse(field->string_value);
    }

    // The device a server event's central is, from the "device" object Apple
    // nests as a string or the "address" Windows sends; 0 when neither is there.
    std::uint64_t le_server_event_device(const json::Value& root)
    {
        std::string address;
        if (auto nested = le_event_nested(root, "device"); nested && nested->is_object())
        {
            if (const auto* field = nested->find("address"); field && field->is_string())
                address = field->string_value;
        }
        if (address.empty())
            address = le_event_string(root, "address");
        if (address.empty())
            return 0;

        DiscoveredDevice d;
        d.transport = Transport::LowEnergy;
        d.id = address;
        d.address = address;
        d.address_available = true;
        return g_device_manager.upsert_device(d);
    }

    void append_json_string(std::string& out, std::string_view value)
    {
        out.push_back('"');
        for (const char ch : value)
        {
            switch (ch)
            {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\b': out += "\\b"; break;
                case '\f': out += "\\f"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:
                {
                    const auto c = static_cast<unsigned char>(ch);
                    if (c < 0x20)
                    {
                        static constexpr char hex[] = "0123456789abcdef";
                        out += "\\u00";
                        out.push_back(hex[(c >> 4) & 0x0f]);
                        out.push_back(hex[c & 0x0f]);
                    }
                    else
                    {
                        out.push_back(ch);
                    }
                    break;
                }
            }
        }
        out.push_back('"');
    }

    std::string serialize_le_service_definition(const BluetoothLeServiceDefinition& service)
    {
        std::string out;
        out.reserve(256);
        out += "{\"uuid\":";
        append_json_string(out, service.uuid);
        out += ",\"characteristics\":[";

        for (std::size_t i = 0; i < service.characteristics.size(); ++i)
        {
            if (i != 0)
                out.push_back(',');

            const auto& characteristic = service.characteristics[i];
            out += "{\"uuid\":";
            append_json_string(out, characteristic.uuid);
            out += ",\"properties\":" + std::to_string(characteristic.properties);
            out += ",\"permissions\":" + std::to_string(characteristic.permissions);

            if (!characteristic.value.empty())
            {
                out += ",\"value\":";
                append_json_string(out, json::base64_encode(characteristic.value.data(), characteristic.value.size()));
            }

            out += ",\"descriptors\":[";
            for (std::size_t d = 0; d < characteristic.descriptors.size(); ++d)
            {
                if (d != 0)
                    out.push_back(',');
                out += "{\"uuid\":";
                append_json_string(out, characteristic.descriptors[d].uuid);
                out.push_back('}');
            }
            out += "]}";
        }

        out += "]}";
        return out;
    }

    void dispatch_le_event(const BackendEvent& event)
    {
        const std::string& type = event.event_type;

        auto parsed = json::parse(event.json);
        if (!parsed || !parsed->is_object())
        {
            GMBT_LOG("LE event '%s' has unparseable JSON payload, dropping", type.c_str());
            g_dropped_events++;
            return;
        }
        const json::Value& root = *parsed;

        if (type == "bluetooth_state_changed")
        {
            const auto* state_field = root.find("state");
            const std::int32_t state = state_field ? state_field->as_int(0) : 0;

            GMFunction callback;
            { std::scoped_lock lock(g_callback_mutex); callback = g_callback_state_changed; }
            if (callback)
            {
                try
                {
                    callback.call(static_cast<double>(state));
                }
                catch (const std::exception& e)
                {
                    GMBT_LOG("Error dispatching state_changed callback: %s", e.what());
                }
            }
            else
            {
                g_dropped_events++;
            }
        }
        else if (type == "bluetooth_le_peripheral_open")
        {
            const std::uint64_t connection = le_event_connection(root);
            GMFunction callback;
            {
                std::scoped_lock lock(g_pending_le_connect_mutex);
                const auto it = g_pending_le_connect_callbacks.find(connection);
                if (it != g_pending_le_connect_callbacks.end())
                {
                    callback = it->second;
                    g_pending_le_connect_callbacks.erase(it);
                }
            }
            if (!callback)
            {
                g_dropped_events++;
                return;
            }
            const bool failed = le_event_has_error(root);
            const Error error = !failed ? Error::Ok
                : le_event_error(root, le_event_error_code(root) == 133 ? Error::Timeout : Error::ConnectionFailed);
            const std::string message = failed ? le_event_string(root, "message") : std::string();
            const std::uint64_t device = g_le_connection_manager.get_device(connection);
            if (failed)
            {
                // The link never came up: nothing queued on it can complete.
                retire_le_connection(connection);
            }
            try
            {
                // callback(error_code, message, connection, device)
                callback.call(static_cast<double>(error), message, static_cast<double>(connection), static_cast<double>(device));
            }
            catch (const std::exception& e)
            {
                GMBT_LOG("Error dispatching le_connect callback: %s", e.what());
            }
        }
        else if (type == "bluetooth_le_peripheral_disconnect")
        {
            // Apple: the backend resolves the peripheral's address to its
            // connection. 0 only if the peripheral was never connected through
            // bluetooth_le_connect. A connection already retired - by
            // bluetooth_le_disconnect, or by the other of the two events Apple
            // can send for one drop - has had its disconnect.
            const std::uint64_t connection = le_event_connection(root);
            if (!g_le_connection_manager.is_valid(connection))
            {
                g_dropped_events++;
                return;
            }
            retire_le_connection(connection);

            GMFunction callback;
            { std::scoped_lock lock(g_callback_mutex); callback = g_callback_le_disconnected; }
            if (callback)
            {
                try
                {
                    // callback(error_code, message, connection)
                    callback.call(static_cast<double>(le_event_error(root, Error::Disconnected)), le_event_string(root, "message"),
                        static_cast<double>(connection));
                }
                catch (const std::exception& e)
                {
                    GMBT_LOG("Error dispatching le_disconnected callback: %s", e.what());
                }
            }
            else
            {
                g_dropped_events++;
            }
        }
        else if (type == "bluetooth_le_peripheral_connection_state_changed")
        {
            const auto* is_connected_field = root.find("is_connected");
            const bool is_connected = is_connected_field && is_connected_field->as_bool(false);
            if (is_connected)
            {
                // No spec callback carries "(re)connected" for this iOS-only event.
                g_dropped_events++;
                return;
            }

            // As above: only a connection still live gets its disconnect.
            const std::uint64_t connection = le_event_connection(root);
            if (!g_le_connection_manager.is_valid(connection))
            {
                g_dropped_events++;
                return;
            }
            retire_le_connection(connection);

            GMFunction callback;
            { std::scoped_lock lock(g_callback_mutex); callback = g_callback_le_disconnected; }
            if (callback)
            {
                try
                {
                    // callback(error_code, message, connection)
                    callback.call(static_cast<double>(le_event_error(root, Error::Disconnected)), le_event_string(root, "message"),
                        static_cast<double>(connection));
                }
                catch (const std::exception& e)
                {
                    GMBT_LOG("Error dispatching le_disconnected callback: %s", e.what());
                }
            }
            else
            {
                g_dropped_events++;
            }
        }
        else if (type == "bluetooth_le_characteristic_value_changed")
        {
            const std::uint64_t connection = le_event_connection(root);
            const std::string service_uuid = le_event_string(root, "service_uuid");
            const std::string characteristic_uuid = le_event_string(root, "characteristic_uuid");

            const std::uint64_t service = g_service_cache.find_by_uuid(connection, service_uuid);
            const std::uint64_t characteristic = service ? g_characteristic_cache.find_by_uuid(service, characteristic_uuid) : 0;
            if (!characteristic)
            {
                GMBT_LOG("LE characteristic_value_changed for unknown characteristic '%s' (connection=%llu), dropping",
                    characteristic_uuid.c_str(), static_cast<unsigned long long>(connection));
                g_dropped_events++;
                return;
            }

            GMFunction callback;
            { std::scoped_lock lock(g_callback_mutex); callback = g_callback_le_characteristic_value_changed; }
            if (!callback)
            {
                g_dropped_events++;
                return;
            }

            std::vector<std::uint8_t> bytes;
            if (const auto* value = root.find("value"); value && value->is_string())
                bytes = json::base64_decode(value->string_value);
            const std::size_t size = bytes.size();
            const std::uint64_t value = g_values.add(std::move(bytes));

            try
            {
                // callback(characteristic, connection, value, size)
                callback.call(static_cast<double>(characteristic), static_cast<double>(connection),
                    static_cast<double>(value), static_cast<double>(size));
            }
            catch (const std::exception& e)
            {
                GMBT_LOG("Error dispatching le_characteristic_value_changed callback: %s", e.what());
            }
        }
        else if (type == "bluetooth_le_server_connection_state_changed")
        {
            // A backend reports a central's arrival where it sees one (a
            // subscription) and its departure where the platform has one; the
            // first event of any kind from a central is its connect.
            const auto* connected_field = root.find("connected");
            const bool connected = connected_field && connected_field->as_bool(false);
            const std::string central = le_event_string(root, "central");

            if (connected)
                note_server_central(central, le_server_event_device(root));
            else
                retire_server_central(central);
        }
        else if (type == "bluetooth_le_server_characteristic_read_request" ||
                 type == "bluetooth_le_server_descriptor_read_request")
        {
            const auto* request_id_field = root.find("request_id");
            const std::int32_t request_id = request_id_field ? request_id_field->as_int(0) : 0;
            const std::string service_uuid = canonical_uuid(le_event_string(root, "service_uuid"));
            const std::string characteristic_uuid = canonical_uuid(le_event_string(root, "characteristic_uuid"));
            const std::string descriptor_uuid = canonical_uuid(le_event_string(root, "descriptor_uuid"));
            const auto* offset_field = root.find("offset");
            const std::int32_t offset = offset_field ? offset_field->as_int(0) : 0;
            const std::uint64_t connection = note_server_central(le_event_string(root, "central"), le_server_event_device(root));

            GMFunction callback;
            { std::scoped_lock lock(g_callback_mutex); callback = g_callback_le_server_read_request; }
            if (!callback)
            {
                // Nobody will answer it, so the central hears now instead of
                // waiting out the ATT timeout and dropping the link.
                answer_le_server_request(request_id, false, k_att_request_not_supported);
                g_dropped_events++;
                return;
            }

            expire_le_server_requests();
            {
                std::scoped_lock lock(g_pending_le_server_requests_mutex);
                PendingLeServerRequest request;
                request.connection = connection;
                request.service_uuid = service_uuid;
                request.characteristic_uuid = characteristic_uuid;
                request.descriptor_uuid = descriptor_uuid;
                request.is_write = false;
                g_pending_le_server_requests[request_id] = std::move(request);
            }

            try
            {
                // callback(request_id, connection, service_uuid, characteristic_uuid, descriptor_uuid_or_empty, offset)
                // GML answers with the value's bytes from offset on, as on Android.
                callback.call(static_cast<double>(request_id), static_cast<double>(connection), service_uuid, characteristic_uuid,
                    descriptor_uuid, static_cast<double>(offset));
            }
            catch (const std::exception& e)
            {
                GMBT_LOG("Error dispatching le_server_read_request callback: %s", e.what());
            }
        }
        else if (type == "bluetooth_le_server_characteristic_write_request" ||
                 type == "bluetooth_le_server_descriptor_write_request")
        {
            const auto* request_id_field = root.find("request_id");
            const std::int32_t request_id = request_id_field ? request_id_field->as_int(0) : 0;
            const std::string service_uuid = canonical_uuid(le_event_string(root, "service_uuid"));
            const std::string characteristic_uuid = canonical_uuid(le_event_string(root, "characteristic_uuid"));
            const std::string descriptor_uuid = canonical_uuid(le_event_string(root, "descriptor_uuid"));
            const auto* response_needed_field = root.find("response_needed");
            const bool response_needed = !response_needed_field || response_needed_field->as_bool(true);
            const auto* offset_field = root.find("offset");
            const std::int32_t offset = offset_field ? offset_field->as_int(0) : 0;
            const std::uint64_t connection = note_server_central(le_event_string(root, "central"), le_server_event_device(root));

            GMFunction callback;
            { std::scoped_lock lock(g_callback_mutex); callback = g_callback_le_server_write_request; }
            if (!callback)
            {
                // As for a read; a write without response needs no answer.
                if (response_needed)
                    answer_le_server_request(request_id, true, k_att_request_not_supported);
                g_dropped_events++;
                return;
            }

            std::vector<std::uint8_t> value;
            if (const auto* value_field = root.find("value"); value_field && value_field->is_string())
                value = json::base64_decode(value_field->string_value);

            expire_le_server_requests();
            {
                std::scoped_lock lock(g_pending_le_server_requests_mutex);
                PendingLeServerRequest request;
                request.connection = connection;
                request.service_uuid = service_uuid;
                request.characteristic_uuid = characteristic_uuid;
                request.descriptor_uuid = descriptor_uuid;
                request.is_write = true;
                request.response_needed = response_needed;
                request.write_value = std::move(value);
                g_pending_le_server_requests[request_id] = std::move(request);
            }

            try
            {
                // callback(request_id, connection, service_uuid, characteristic_uuid, descriptor_uuid_or_empty,
                //          offset, response_needed)
                callback.call(static_cast<double>(request_id), static_cast<double>(connection), service_uuid, characteristic_uuid,
                    descriptor_uuid, static_cast<double>(offset), response_needed);
            }
            catch (const std::exception& e)
            {
                GMBT_LOG("Error dispatching le_server_write_request callback: %s", e.what());
            }
        }
        else
        {
            // Known synchronous-only events with no GML callback
            // (bluetooth_le_advertise_stop, bluetooth_le_server_open/close/clear_services,
            // bluetooth_le_server_notify_value's fire-and-forget error signal) land here
            // deliberately, alongside any genuinely unrecognized event_type.
            GMBT_LOG("LE event dropped: no route for event_type '%s'", type.c_str());
            g_dropped_events++;
        }
    }

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

            GMFunction callback;
            {
                std::scoped_lock lock(g_callback_mutex);
                callback = g_callback_device_found;
            }

            if (callback)
            {
                try
                {
                    callback.call(static_cast<double>(handle));
                }
                catch (const std::exception& e)
                {
                    GMBT_LOG("Error dispatching device_found callback: %s", e.what());
                }
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
        hooks.create_le_connection = [](std::uint64_t device) {
            const std::uint64_t connection = g_le_connection_manager.create_connection(device);
            GMBT_LOG("LE connection created: connection=%llu device=%llu",
                static_cast<unsigned long long>(connection),
                static_cast<unsigned long long>(device));
            return connection;
        };
        hooks.push_event = [](BackendEvent event) {
            GMBT_LOG("event: type=%d transport=%d event_type='%s'",
                static_cast<int>(event.type),
                static_cast<int>(event.transport),
                event.event_type.c_str());

            // The connect completion callback is one-shot and per-connection,
            // not one of the persistently-registered callbacks below.
            if (event.type == BackendEventType::ClassicConnected)
            {
                // A connect that failed leaves nothing behind its handle.
                if (event.error != Error::Ok)
                    g_classic_connection_manager.remove_connection(event.connection);

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
                    }
                    catch (const std::exception& e)
                    {
                        GMBT_LOG("Error dispatching classic_connect callback: %s", e.what());
                    }
                }
                return;
            }

            // Same one-shot idea as ClassicConnected above, but keyed by device
            // handle since pairing never creates a connection.
            if (event.type == BackendEventType::DevicePaired)
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

                if (callback)
                {
                    try
                    {
                        // callback(error_code, message, device)
                        callback.call(
                            static_cast<double>(event.error),
                            event.message,
                            static_cast<double>(event.device)
                        );
                    }
                    catch (const std::exception& e)
                    {
                        GMBT_LOG("Error dispatching pair callback: %s", e.what());
                    }
                }
                return;
            }

            if (event.type == BackendEventType::LeOpCompleted)
            {
                complete_le_op(event);
                return;
            }

            if (event.type == BackendEventType::PermissionResult)
            {
                fire_permission_callbacks(Error::Ok, std::string(), static_cast<PermissionStatus>(event.value));
                return;
            }

            if (event.type == BackendEventType::LeEvent)
            {
                dispatch_le_event(event);
                return;
            }

            // Retired by the game-thread calls once the backend holds no unread
            // bytes for it; nothing is asked of the backend from its own thread.
            if (event.type == BackendEventType::ClassicDisconnected)
                g_classic_connection_manager.mark_closed(event.connection);

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
                        // callback(error_code, message, transport)
                        callback.call(static_cast<double>(event.error), event.message, static_cast<double>(event.transport));
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
                        // callback(error_code, message, connection)
                        callback.call(static_cast<double>(event.error), event.message, static_cast<double>(event.connection));
                        break;
                    default:
                        break;
                    }
                }
                catch (const std::exception& e)
                {
                    GMBT_LOG("Error dispatching callback: %s", e.what());
                }
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
        g_last_error_message = "Failed to create platform backend";
        g_last_error = Error::OperationFailed;
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

    return error == Error::Ok;
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

    std::unordered_map<std::uint64_t, GMFunction> connect_callbacks;
    { std::scoped_lock lock(g_pending_connect_mutex); connect_callbacks.swap(g_pending_connect_callbacks); }
    for (const auto& [connection, callback] : connect_callbacks)
    {
        try
        {
            // callback(error_code, message, connection, device)
            callback.call(static_cast<double>(Error::NotInitialized), message, static_cast<double>(connection),
                static_cast<double>(g_classic_connection_manager.get_device(connection)));
        }
        catch (const std::exception& e)
        {
            GMBT_LOG("Error dispatching classic_connect callback: %s", e.what());
        }
    }

    std::unordered_map<std::uint64_t, GMFunction> le_connect_callbacks;
    { std::scoped_lock lock(g_pending_le_connect_mutex); le_connect_callbacks.swap(g_pending_le_connect_callbacks); }
    for (const auto& [connection, callback] : le_connect_callbacks)
    {
        try
        {
            // callback(error_code, message, connection, device)
            callback.call(static_cast<double>(Error::NotInitialized), message, static_cast<double>(connection),
                static_cast<double>(g_le_connection_manager.get_device(connection)));
        }
        catch (const std::exception& e)
        {
            GMBT_LOG("Error dispatching le_connect callback: %s", e.what());
        }
    }

    std::unordered_map<std::uint64_t, GMFunction> pair_callbacks;
    { std::scoped_lock lock(g_pending_pair_mutex); pair_callbacks.swap(g_pending_pair_callbacks); }
    for (const auto& [device, callback] : pair_callbacks)
    {
        if (!callback)
            continue;
        try
        {
            // callback(error_code, message, device)
            callback.call(static_cast<double>(Error::NotInitialized), message, static_cast<double>(device));
        }
        catch (const std::exception& e)
        {
            GMBT_LOG("Error dispatching pair callback: %s", e.what());
        }
    }

    fire_permission_callbacks(Error::NotInitialized, message, PermissionStatus::Unknown);

    {
        std::scoped_lock lock(g_pending_le_server_requests_mutex);
        g_pending_le_server_requests.clear();
    }
    {
        std::scoped_lock lock(g_server_centrals_mutex);
        g_server_centrals.clear();
    }
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
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        GMBT_LOG("permission request rejected: backend is not initialized");
        return to_gm(Error::NotInitialized);
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

BluetoothError bluetooth_le_scan_start(bool active)
{
    GMBT_LOG("BLE scan start requested (active=%d)", active ? 1 : 0);

    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        GMBT_LOG("BLE scan start rejected: backend is not initialized");
        return to_gm(Error::NotInitialized);
    }

    std::string message;
    const Error error = g_backend->le_scan_start(active, message);
    set_last_error(error, message);
    GMBT_LOG("BLE scan start -> error=%d message='%s' | backend reports running=%d",
        static_cast<int>(error),
        message.c_str(),
        g_backend->le_scan_is_running() ? 1 : 0);
    return to_gm(error);
}

BluetoothError bluetooth_le_scan_stop()
{
    GMBT_LOG("BLE scan stop requested");

    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        GMBT_LOG("BLE scan stop rejected: backend is not initialized");
        return to_gm(Error::NotInitialized);
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
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return to_gm(Error::NotInitialized);
    }

    std::string message;
    const Error error = g_backend->classic_scan_start(message);
    set_last_error(error, message);
    return to_gm(error);
}

BluetoothError bluetooth_classic_scan_stop()
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return to_gm(Error::NotInitialized);
    }

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

std::uint64_t bluetooth_classic_connect(std::uint64_t device, std::string_view service_uuid, const gm::wire::GMFunction& callback)
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return 0;
    }

    const auto dev = g_device_manager.get_device(device);
    if (!dev)
    {
        g_last_error = Error::InvalidArgument;
        g_last_error_message = "Invalid device handle";
        return 0;
    }

    if (!check_uuid(service_uuid))
        return 0;

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
    set_last_error(error, message);

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

bool bluetooth_pairing_is_supported(std::uint64_t device)
{
    if (!g_backend)
        return false;

    const auto dev = g_device_manager.get_device(device);
    return dev && g_backend->pairing_is_supported(*dev);
}

BluetoothError bluetooth_pair(std::uint64_t device, const gm::wire::GMFunction& callback)
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return to_gm(Error::NotInitialized);
    }

    const auto dev = g_device_manager.get_device(device);
    if (!dev)
    {
        g_last_error = Error::InvalidArgument;
        g_last_error_message = "Invalid device handle";
        return to_gm(Error::InvalidArgument);
    }

    // Registered before calling the backend: pairing runs asynchronously and may
    // push its DevicePaired completion event before this call even returns.
    // Recorded with or without a callback, so a second pair of the same device
    // is refused instead of overwriting the first one's callback.
    {
        std::scoped_lock lock(g_pending_pair_mutex);
        if (g_pending_pair_callbacks.find(device) != g_pending_pair_callbacks.end())
        {
            g_last_error = Error::Busy;
            g_last_error_message = "A pairing for this device is already in progress";
            return to_gm(Error::Busy);
        }
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
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return to_gm(Error::NotInitialized);
    }

    const bool remote_closed = g_classic_connection_manager.is_closed(connection);

    std::string message;
    Error error = g_backend->classic_disconnect(connection, message);

    // The peer already hung up and the backend dropped its side: the game
    // releasing the handle is still a success.
    if (remote_closed && error == Error::InvalidHandle)
    {
        error = Error::Ok;
        message.clear();
    }
    set_last_error(error, message);

    // A connect still in flight is cancelled, not completed: its callback
    // fires now, and a late completion from the backend finds none.
    GMFunction connect_callback;
    {
        std::scoped_lock lock(g_pending_connect_mutex);
        const auto it = g_pending_connect_callbacks.find(connection);
        if (it != g_pending_connect_callbacks.end())
        {
            connect_callback = it->second;
            g_pending_connect_callbacks.erase(it);
        }
    }
    const std::uint64_t device = g_classic_connection_manager.get_device(connection);
    g_classic_connection_manager.remove_connection(connection);
    if (connect_callback)
    {
        try
        {
            // callback(error_code, message, connection, device)
            connect_callback.call(static_cast<double>(Error::ConnectionFailed), std::string("Connection cancelled by bluetooth_classic_disconnect"), static_cast<double>(connection), static_cast<double>(device));
        }
        catch (const std::exception& e)
        {
            GMBT_LOG("Error dispatching classic_connect callback: %s", e.what());
        }
    }

    return to_gm(error);
}

namespace
{
    // A handle whose peer hung up stays valid while the backend still holds
    // bytes for it, and is retired by the first call that finds none.
    void retire_classic_connection_if_drained(std::uint64_t connection)
    {
        if (!g_classic_connection_manager.is_closed(connection))
            return;
        if (g_backend && g_backend->classic_receive_available(connection) > 0)
            return;
        g_classic_connection_manager.remove_connection(connection);
    }
}

bool bluetooth_classic_connection_is_valid(std::uint64_t connection)
{
    retire_classic_connection_if_drained(connection);
    return g_classic_connection_manager.is_valid(connection);
}

bool bluetooth_classic_connection_is_connected(std::uint64_t connection)
{
    return g_backend && g_backend->classic_connection_is_connected(connection);
}

std::uint64_t bluetooth_classic_connection_get_device(std::uint64_t connection)
{
    retire_classic_connection_if_drained(connection);
    return g_classic_connection_manager.get_device(connection);
}

std::int32_t bluetooth_classic_receive_available(std::uint64_t connection)
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return 0;
    }
    const std::int32_t available = g_backend->classic_receive_available(connection);
    if (available == 0)
        retire_classic_connection_if_drained(connection);
    return available;
}

BluetoothError bluetooth_classic_send(std::uint64_t connection, struct gm::wire::GMBuffer data, unsigned int offset, unsigned int size)
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return to_gm(Error::NotInitialized);
    }

    if (!buffer_range_valid(data, offset, size, "bluetooth_classic_send"))
        return to_gm(Error::InvalidArgument);

    const std::uint8_t* buffer = static_cast<const std::uint8_t*>(data.data()) + offset;
    std::string message;
    const Error error = g_backend->classic_send_bytes(connection, buffer, size, message);
    set_last_error(error, message);
    return to_gm(error);
}

std::int32_t bluetooth_classic_receive(std::uint64_t connection, struct gm::wire::GMBuffer data, unsigned int offset, unsigned int max_size)
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return 0;
    }

    if (!buffer_range_valid(data, offset, max_size, "bluetooth_classic_receive"))
        return 0;

    std::uint8_t* buffer = static_cast<std::uint8_t*>(data.data()) + offset;
    const std::size_t received = g_backend->classic_receive_bytes(connection, buffer, max_size);
    retire_classic_connection_if_drained(connection);
    return static_cast<std::int32_t>(received);
}

BluetoothError bluetooth_classic_server_start(std::string_view name, std::string_view service_uuid)
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return to_gm(Error::NotInitialized);
    }

    if (!check_uuid(service_uuid))
        return to_gm(Error::InvalidArgument);

    // One answer on every platform, rather than a running server that quietly
    // keeps its old name and UUID.
    if (g_backend->classic_server_is_running())
    {
        g_last_error = Error::Busy;
        g_last_error_message = "A Classic server is already running; stop it first";
        return to_gm(Error::Busy);
    }

    std::string message;
    const Error error = g_backend->classic_server_start(std::string(name), std::string(service_uuid), message);
    set_last_error(error, message);
    return to_gm(error);
}

BluetoothError bluetooth_classic_server_stop()
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return to_gm(Error::NotInitialized);
    }

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
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return to_gm(Error::NotInitialized);
    }

    std::string message;
    const Error error = g_backend->classic_discoverable_start(duration_seconds, message);
    set_last_error(error, message);
    return to_gm(error);
}

BluetoothError bluetooth_classic_discoverable_stop()
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return to_gm(Error::NotInitialized);
    }

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
    {
        std::scoped_lock lock(g_callback_mutex);
        g_callback_state_changed = callback;
    }

    GMBT_LOG("Bluetooth state changed callback registered");

    // The public contract promises the current known state immediately after
    // registration. GMFunction::call queues safely into GameMaker's dispatcher.
    if (callback)
    {
        const std::int32_t state = g_backend
            ? g_backend->current_bluetooth_state()
            : 0; // BluetoothState.Unknown
        try
        {
            callback.call(static_cast<double>(state));
        }
        catch (const std::exception& e)
        {
            GMBT_LOG("Error dispatching initial state_changed callback: %s", e.what());
        }
    }

    return true;
}

bool bluetooth_remove_callback_state_changed()
{
    std::scoped_lock lock(g_callback_mutex);
    g_callback_state_changed = GMFunction();
    GMBT_LOG("Bluetooth state changed callback removed");
    return true;
}

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

// --- BLE GATT client: connect / disconnect ---

std::uint64_t bluetooth_le_connect(std::uint64_t device, const gm::wire::GMFunction& callback)
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return 0;
    }

    const auto dev = g_device_manager.get_device(device);
    if (!dev)
    {
        g_last_error = Error::InvalidArgument;
        g_last_error_message = "Invalid device handle";
        return 0;
    }

    const std::uint64_t connection = g_le_connection_manager.create_connection(device);

    // Registered before calling the backend: connect runs asynchronously and may
    // push its completion event before this call even returns.
    if (callback)
    {
        std::scoped_lock lock(g_pending_le_connect_mutex);
        g_pending_le_connect_callbacks[connection] = callback;
    }

    std::string message;
    const Error error = g_backend->le_connect(connection, *dev, message);
    set_last_error(error, message);

    if (error != Error::Ok)
    {
        g_le_connection_manager.remove_connection(connection);
        {
            std::scoped_lock lock(g_pending_le_connect_mutex);
            g_pending_le_connect_callbacks.erase(connection);
        }
        return 0;
    }

    return connection;
}

BluetoothError bluetooth_le_disconnect(std::uint64_t connection)
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return to_gm(Error::NotInitialized);
    }

    std::string message;
    const Error error = g_backend->le_disconnect(connection, message);
    set_last_error(error, message);

    // Whatever the backend answered, the game is done with this link: no op
    // waiting on it may outlive the call, and its handle is retired.
    const std::uint64_t device = g_le_connection_manager.get_device(connection);
    retire_le_connection(connection);

    // A connect still in flight is cancelled, not completed: its callback
    // fires now, and a late open event from the backend finds none.
    GMFunction connect_callback;
    {
        std::scoped_lock lock(g_pending_le_connect_mutex);
        const auto it = g_pending_le_connect_callbacks.find(connection);
        if (it != g_pending_le_connect_callbacks.end())
        {
            connect_callback = it->second;
            g_pending_le_connect_callbacks.erase(it);
        }
    }
    if (connect_callback)
    {
        try
        {
            // callback(error_code, message, connection, device)
            connect_callback.call(static_cast<double>(Error::ConnectionFailed), std::string("Connection cancelled by bluetooth_le_disconnect"), static_cast<double>(connection), static_cast<double>(device));
        }
        catch (const std::exception& e)
        {
            GMBT_LOG("Error dispatching le_connect callback: %s", e.what());
        }
    }

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

// --- BLE GATT client: service / characteristic / descriptor discovery ---

BluetoothError bluetooth_le_services_discover(std::uint64_t connection, const gm::wire::GMFunction& callback)
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return to_gm(Error::NotInitialized);
    }

    if (!g_le_connection_manager.is_valid(connection))
    {
        g_last_error = Error::InvalidHandle;
        g_last_error_message = "Invalid LE connection handle";
        return to_gm(Error::InvalidHandle);
    }

    const auto op_id = g_le_ops.add(LeOpKind::ServicesDiscover, callback, connection, connection);

    std::string message;
    const Error error = g_backend->le_services_discover(op_id, connection, message);
    set_last_error(error, message);

    if (error != Error::Ok)
        g_le_ops.erase(op_id);

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
    return g_service_cache.get_uuid(service);
}

BluetoothError bluetooth_le_characteristics_discover(std::uint64_t service, const gm::wire::GMFunction& callback)
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return to_gm(Error::NotInitialized);
    }

    if (!g_service_cache.is_valid(service))
    {
        g_last_error = Error::InvalidHandle;
        g_last_error_message = "Invalid service handle";
        return to_gm(Error::InvalidHandle);
    }

    const std::uint64_t connection = g_service_cache.get_parent(service);
    const std::string uuid = g_service_cache.get_uuid(service);

    const auto op_id = g_le_ops.add(LeOpKind::CharacteristicsDiscover, callback, service, connection);

    std::string message;
    const Error error = g_backend->le_characteristics_discover(op_id, connection, uuid, message);
    set_last_error(error, message);

    if (error != Error::Ok)
        g_le_ops.erase(op_id);

    return to_gm(error);
}

std::int32_t bluetooth_le_characteristic_get_count(std::uint64_t service)
{
    return g_characteristic_cache.get_count(service);
}

std::uint64_t bluetooth_le_characteristic_get_at(std::uint64_t service, std::int32_t index)
{
    return g_characteristic_cache.get_at(service, index);
}

std::string bluetooth_le_characteristic_get_uuid(std::uint64_t characteristic)
{
    return g_characteristic_cache.get_uuid(characteristic);
}

std::int32_t bluetooth_le_characteristic_get_properties(std::uint64_t characteristic)
{
    return g_characteristic_cache.get_properties(characteristic);
}

BluetoothError bluetooth_le_descriptors_discover(std::uint64_t characteristic, const gm::wire::GMFunction& callback)
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return to_gm(Error::NotInitialized);
    }

    if (!g_characteristic_cache.is_valid(characteristic))
    {
        g_last_error = Error::InvalidHandle;
        g_last_error_message = "Invalid characteristic handle";
        return to_gm(Error::InvalidHandle);
    }

    const std::uint64_t service = g_characteristic_cache.get_parent(characteristic);
    const std::uint64_t connection = g_service_cache.get_parent(service);
    const std::string service_uuid = g_service_cache.get_uuid(service);
    const std::string characteristic_uuid = g_characteristic_cache.get_uuid(characteristic);

    const auto op_id = g_le_ops.add(LeOpKind::DescriptorsDiscover, callback, characteristic, connection);

    std::string message;
    const Error error = g_backend->le_descriptors_discover(op_id, connection, service_uuid, characteristic_uuid, message);
    set_last_error(error, message);

    if (error != Error::Ok)
        g_le_ops.erase(op_id);

    return to_gm(error);
}

std::int32_t bluetooth_le_descriptor_get_count(std::uint64_t characteristic)
{
    return g_descriptor_cache.get_count(characteristic);
}

std::uint64_t bluetooth_le_descriptor_get_at(std::uint64_t characteristic, std::int32_t index)
{
    return g_descriptor_cache.get_at(characteristic, index);
}

std::string bluetooth_le_descriptor_get_uuid(std::uint64_t descriptor)
{
    return g_descriptor_cache.get_uuid(descriptor);
}

// --- BLE GATT client: characteristic / descriptor read, write, subscribe ---

BluetoothError bluetooth_le_characteristic_read(std::uint64_t characteristic, const gm::wire::GMFunction& callback)
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return to_gm(Error::NotInitialized);
    }

    if (!g_characteristic_cache.is_valid(characteristic))
    {
        g_last_error = Error::InvalidHandle;
        g_last_error_message = "Invalid characteristic handle";
        return to_gm(Error::InvalidHandle);
    }

    const std::uint64_t service = g_characteristic_cache.get_parent(characteristic);
    const std::uint64_t connection = g_service_cache.get_parent(service);
    const std::string service_uuid = g_service_cache.get_uuid(service);
    const std::string characteristic_uuid = g_characteristic_cache.get_uuid(characteristic);

    const auto op_id = g_le_ops.add(LeOpKind::CharacteristicRead, callback, characteristic, connection);

    std::string message;
    const Error error = g_backend->le_characteristic_read(op_id, connection, service_uuid, characteristic_uuid, message);
    set_last_error(error, message);

    if (error != Error::Ok)
        g_le_ops.erase(op_id);

    return to_gm(error);
}

BluetoothError bluetooth_le_characteristic_write(std::uint64_t characteristic, struct gm::wire::GMBuffer data, unsigned int offset, unsigned int size, BluetoothLeWriteType write_type, const gm::wire::GMFunction& callback)
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return to_gm(Error::NotInitialized);
    }

    if (!g_characteristic_cache.is_valid(characteristic))
    {
        g_last_error = Error::InvalidHandle;
        g_last_error_message = "Invalid characteristic handle";
        return to_gm(Error::InvalidHandle);
    }

    // The wire carries any int32; only the two members mean anything (R1-149).
    if (write_type != BluetoothLeWriteType::WithResponse && write_type != BluetoothLeWriteType::WithoutResponse)
    {
        g_last_error = Error::InvalidArgument;
        g_last_error_message = "write_type must be a BluetoothLeWriteType";
        return to_gm(Error::InvalidArgument);
    }

    const std::uint64_t service = g_characteristic_cache.get_parent(characteristic);
    const std::uint64_t connection = g_service_cache.get_parent(service);
    const std::string service_uuid = g_service_cache.get_uuid(service);
    const std::string characteristic_uuid = g_characteristic_cache.get_uuid(characteristic);

    if (!buffer_range_valid(data, offset, size, "bluetooth_le_characteristic_write"))
        return to_gm(Error::InvalidArgument);

    const std::uint8_t* buffer = static_cast<const std::uint8_t*>(data.data()) + offset;
    const std::string value_base64 = json::base64_encode(buffer, size);
    const bool with_response = (write_type == BluetoothLeWriteType::WithResponse);

    const auto op_id = g_le_ops.add(LeOpKind::CharacteristicWrite, callback, characteristic, connection);

    std::string message;
    const Error error = g_backend->le_characteristic_write(op_id, connection, service_uuid, characteristic_uuid, value_base64, with_response, message);
    set_last_error(error, message);

    if (error != Error::Ok)
        g_le_ops.erase(op_id);

    return to_gm(error);
}

BluetoothError bluetooth_le_characteristic_subscribe(std::uint64_t characteristic, BluetoothLeSubscribeMode mode, const gm::wire::GMFunction& callback)
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return to_gm(Error::NotInitialized);
    }

    if (!g_characteristic_cache.is_valid(characteristic))
    {
        g_last_error = Error::InvalidHandle;
        g_last_error_message = "Invalid characteristic handle";
        return to_gm(Error::InvalidHandle);
    }

    if (mode != BluetoothLeSubscribeMode::Unsubscribe && mode != BluetoothLeSubscribeMode::Notify &&
        mode != BluetoothLeSubscribeMode::Indicate)
    {
        g_last_error = Error::InvalidArgument;
        g_last_error_message = "mode must be a BluetoothLeSubscribeMode";
        return to_gm(Error::InvalidArgument);
    }

    const std::uint64_t service = g_characteristic_cache.get_parent(characteristic);
    const std::uint64_t connection = g_service_cache.get_parent(service);
    const std::string service_uuid = g_service_cache.get_uuid(service);
    const std::string characteristic_uuid = g_characteristic_cache.get_uuid(characteristic);

    const auto op_id = g_le_ops.add(LeOpKind::CharacteristicSubscribe, callback, characteristic, connection);

    std::string message;
    const Error error = g_backend->le_characteristic_subscribe(op_id, connection, service_uuid, characteristic_uuid,
        static_cast<std::int32_t>(mode), message);
    set_last_error(error, message);

    if (error != Error::Ok)
        g_le_ops.erase(op_id);

    return to_gm(error);
}

BluetoothError bluetooth_le_descriptor_read(std::uint64_t descriptor, const gm::wire::GMFunction& callback)
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return to_gm(Error::NotInitialized);
    }

    if (!g_descriptor_cache.is_valid(descriptor))
    {
        g_last_error = Error::InvalidHandle;
        g_last_error_message = "Invalid descriptor handle";
        return to_gm(Error::InvalidHandle);
    }

    const std::uint64_t characteristic = g_descriptor_cache.get_parent(descriptor);
    const std::uint64_t service = g_characteristic_cache.get_parent(characteristic);
    const std::uint64_t connection = g_service_cache.get_parent(service);
    const std::string service_uuid = g_service_cache.get_uuid(service);
    const std::string characteristic_uuid = g_characteristic_cache.get_uuid(characteristic);
    const std::string descriptor_uuid = g_descriptor_cache.get_uuid(descriptor);

    const auto op_id = g_le_ops.add(LeOpKind::DescriptorRead, callback, descriptor, connection);

    std::string message;
    const Error error = g_backend->le_descriptor_read(op_id, connection, service_uuid, characteristic_uuid, descriptor_uuid, message);
    set_last_error(error, message);

    if (error != Error::Ok)
        g_le_ops.erase(op_id);

    return to_gm(error);
}

BluetoothError bluetooth_le_descriptor_write(std::uint64_t descriptor, struct gm::wire::GMBuffer data, unsigned int offset, unsigned int size, const gm::wire::GMFunction& callback)
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return to_gm(Error::NotInitialized);
    }

    if (!g_descriptor_cache.is_valid(descriptor))
    {
        g_last_error = Error::InvalidHandle;
        g_last_error_message = "Invalid descriptor handle";
        return to_gm(Error::InvalidHandle);
    }

    const std::uint64_t characteristic = g_descriptor_cache.get_parent(descriptor);
    const std::uint64_t service = g_characteristic_cache.get_parent(characteristic);
    const std::uint64_t connection = g_service_cache.get_parent(service);
    const std::string service_uuid = g_service_cache.get_uuid(service);
    const std::string characteristic_uuid = g_characteristic_cache.get_uuid(characteristic);
    const std::string descriptor_uuid = g_descriptor_cache.get_uuid(descriptor);

    // The CCCD has one writer on every platform, bluetooth_le_characteristic_subscribe;
    // CoreBluetooth throws on a direct write.
    if (canonical_uuid(descriptor_uuid) == "00002902-0000-1000-8000-00805f9b34fb")
    {
        g_last_error = Error::InvalidArgument;
        g_last_error_message = "The CCCD is written by bluetooth_le_characteristic_subscribe";
        return to_gm(Error::InvalidArgument);
    }

    if (!buffer_range_valid(data, offset, size, "bluetooth_le_descriptor_write"))
        return to_gm(Error::InvalidArgument);

    const std::uint8_t* buffer = static_cast<const std::uint8_t*>(data.data()) + offset;
    const std::string value_base64 = json::base64_encode(buffer, size);

    const auto op_id = g_le_ops.add(LeOpKind::DescriptorWrite, callback, descriptor, connection);

    std::string message;
    const Error error = g_backend->le_descriptor_write(op_id, connection, service_uuid, characteristic_uuid, descriptor_uuid, value_base64, message);
    set_last_error(error, message);

    if (error != Error::Ok)
        g_le_ops.erase(op_id);

    return to_gm(error);
}

// --- BLE values ---

BluetoothError bluetooth_le_value_copy(std::uint64_t value, struct gm::wire::GMBuffer out_data, unsigned int offset)
{
    std::string message;
    const Error error = g_values.copy(value, out_data, offset, message);
    set_last_error(error, message);
    return to_gm(error);
}

BluetoothError bluetooth_le_value_release(std::uint64_t value)
{
    if (g_values.release(value))
        return to_gm(Error::Ok);

    g_last_error = Error::InvalidHandle;
    g_last_error_message = "Unknown or already copied value " + std::to_string(value);
    return to_gm(Error::InvalidHandle);
}

// --- BLE advertise ---

BluetoothError bluetooth_le_advertise_start(const BluetoothLeAdvertiseSettings& settings, const BluetoothLeAdvertiseData& data, const gm::wire::GMFunction& callback)
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return to_gm(Error::NotInitialized);
    }

    // Checked once here, so every backend sees only what it can act on; what a
    // backend cannot send is its own NotSupported (R1-36).
    LeAdvertiseSettings advertise_settings;
    advertise_settings.connectable = settings.connectable;
    if (settings.tx_power)
    {
        const auto level = static_cast<std::int32_t>(*settings.tx_power);
        if (level < static_cast<std::int32_t>(LeAdvertiseTxPower::UltraLow) || level > static_cast<std::int32_t>(LeAdvertiseTxPower::High))
        {
            g_last_error = Error::InvalidArgument;
            g_last_error_message = "tx_power must be a BluetoothLeAdvertiseTxPower";
            return to_gm(Error::InvalidArgument);
        }
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
        {
            g_last_error = Error::InvalidArgument;
            g_last_error_message = "Service data for " + uuid + " needs the uuid in service_uuids too";
            return to_gm(Error::InvalidArgument);
        }
        advertise_data.service_data.push_back({ uuid, entry.data });
    }

    for (const auto& entry : data.manufacturer_data)
    {
        if (entry.company_id < 0 || entry.company_id > 0xFFFF)
        {
            g_last_error = Error::InvalidArgument;
            g_last_error_message = "company_id must be 0-65535, got " + std::to_string(entry.company_id);
            return to_gm(Error::InvalidArgument);
        }
        advertise_data.manufacturer_data.push_back({ static_cast<std::uint16_t>(entry.company_id), entry.data });
    }

    const auto op_id = g_le_ops.add(LeOpKind::AdvertiseStart, callback, 0, 0);

    std::string message;
    const Error error = g_backend->le_advertise_start(op_id, advertise_settings, advertise_data, message);
    set_last_error(error, message);

    if (error != Error::Ok)
        g_le_ops.erase(op_id);

    return to_gm(error);
}

BluetoothError bluetooth_le_advertise_stop()
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return to_gm(Error::NotInitialized);
    }

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
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return to_gm(Error::NotInitialized);
    }

    std::string message;
    const Error error = g_backend->le_server_start(message);
    set_last_error(error, message);
    return to_gm(error);
}

BluetoothError bluetooth_le_server_stop()
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return to_gm(Error::NotInitialized);
    }

    // Answered while the backend still holds them; it forgets them on stop.
    answer_pending_le_server_requests();

    std::string message;
    const Error error = g_backend->le_server_stop(message);
    set_last_error(error, message);
    if (error == Error::Ok)
        retire_server_centrals();
    return to_gm(error);
}

bool bluetooth_le_server_is_running()
{
    return g_backend && g_backend->le_server_is_running();
}

BluetoothError bluetooth_le_server_add_service(const BluetoothLeServiceDefinition& service, const gm::wire::GMFunction& callback)
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return to_gm(Error::NotInitialized);
    }

    if (!check_uuid(service.uuid))
        return to_gm(Error::InvalidArgument);
    for (const auto& characteristic : service.characteristics)
    {
        if (!check_uuid(characteristic.uuid))
            return to_gm(Error::InvalidArgument);
        if ((characteristic.permissions & ~kPermissionAll) != 0)
        {
            g_last_error = Error::InvalidArgument;
            g_last_error_message = "Characteristic " + characteristic.uuid + ": permissions " +
                std::to_string(characteristic.permissions) + " has bits that are not BluetoothLeAttributePermission flags";
            return to_gm(Error::InvalidArgument);
        }
        // The ATT limit on an attribute value.
        if (characteristic.value.size() > 512)
        {
            g_last_error = Error::InvalidArgument;
            g_last_error_message = "Characteristic " + characteristic.uuid + ": the initial value is " +
                std::to_string(characteristic.value.size()) + " bytes; an attribute value holds at most 512";
            return to_gm(Error::InvalidArgument);
        }
        for (const auto& descriptor : characteristic.descriptors)
        {
            if (!check_uuid(descriptor.uuid))
                return to_gm(Error::InvalidArgument);
        }
    }

    const auto op_id = g_le_ops.add(LeOpKind::ServerAddService, callback, 0, 0);

    const std::string service_json = serialize_le_service_definition(service);
    std::string message;
    const Error error = g_backend->le_server_add_service(op_id, service_json, message);
    set_last_error(error, message);

    if (error != Error::Ok)
        g_le_ops.erase(op_id);

    return to_gm(error);
}

BluetoothError bluetooth_le_server_clear_services()
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return to_gm(Error::NotInitialized);
    }

    // The services the requests name are going away.
    answer_pending_le_server_requests();

    std::string message;
    const Error error = g_backend->le_server_clear_services(message);
    set_last_error(error, message);
    return to_gm(error);
}

BluetoothError bluetooth_le_server_respond_read(std::int32_t request_id, BluetoothAttError error_code, struct gm::wire::GMBuffer data, unsigned int offset, unsigned int size)
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return to_gm(Error::NotInitialized);
    }

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

    std::string value_base64;
    if (success && size > 0)
        value_base64 = json::base64_encode(static_cast<const std::uint8_t*>(data.data()) + offset, size);

    std::string message;
    const Error error = g_backend->le_server_respond_read(request_id, static_cast<std::int32_t>(error_code), value_base64, message);
    set_last_error(error, message);
    return to_gm(error);
}

BluetoothError bluetooth_le_server_respond_write(std::int32_t request_id, BluetoothAttError error_code)
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return to_gm(Error::NotInitialized);
    }

    if (!check_att_error(error_code, "bluetooth_le_server_respond_write"))
        return to_gm(Error::InvalidArgument);

    if (!check_le_server_request(request_id, true, "bluetooth_le_server_respond_read"))
        return to_gm(g_last_error);

    const auto request = take_le_server_request(request_id);
    if (!request)
        return to_gm(g_last_error);

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

std::int32_t bluetooth_le_server_write_request_get_value(std::int32_t request_id, struct gm::wire::GMBuffer out_data, unsigned int offset, unsigned int max_size)
{
    if (!buffer_range_valid(out_data, offset, max_size, "bluetooth_le_server_write_request_get_value"))
        return 0;

    std::scoped_lock lock(g_pending_le_server_requests_mutex);
    const auto it = g_pending_le_server_requests.find(request_id);
    if (it == g_pending_le_server_requests.end())
    {
        g_last_error = Error::InvalidHandle;
        g_last_error_message = "Unknown or expired LE server request id " + std::to_string(request_id);
        return 0;
    }
    if (!it->second.is_write)
    {
        g_last_error = Error::InvalidArgument;
        g_last_error_message = "LE server request " + std::to_string(request_id) + " is a read request; it has no written value";
        return 0;
    }

    const auto& value = it->second.write_value;
    const std::size_t n = std::min(static_cast<std::size_t>(max_size), value.size());
    if (n > 0)
    {
        std::uint8_t* buffer = static_cast<std::uint8_t*>(out_data.data()) + offset;
        std::memcpy(buffer, value.data(), n);
    }
    return static_cast<std::int32_t>(n);
}

BluetoothError bluetooth_le_server_notify_value(std::string_view service_uuid, std::string_view characteristic_uuid, std::uint64_t connection, struct gm::wire::GMBuffer data, unsigned int offset, unsigned int size)
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return to_gm(Error::NotInitialized);
    }

    if (!check_uuid(service_uuid) || !check_uuid(characteristic_uuid))
        return to_gm(Error::InvalidArgument);

    // 0 broadcasts; a server connection names one central (R1-48).
    std::string central;
    if (connection != 0)
    {
        central = server_central_key(connection);
        if (central.empty())
        {
            g_last_error = Error::InvalidHandle;
            g_last_error_message = "Unknown or disconnected server connection " + std::to_string(connection);
            return to_gm(Error::InvalidHandle);
        }
    }

    if (!buffer_range_valid(data, offset, size, "bluetooth_le_server_notify_value"))
        return to_gm(Error::InvalidArgument);

    const std::uint8_t* buffer = static_cast<const std::uint8_t*>(data.data()) + offset;
    const std::string value_base64 = json::base64_encode(buffer, size);

    std::string message;
    const Error error = g_backend->le_server_notify_value(canonical_uuid(service_uuid), canonical_uuid(characteristic_uuid), central,
        value_base64, message);
    set_last_error(error, message);
    return to_gm(error);
}

// --- BLE callbacks ---

bool bluetooth_set_callback_le_disconnected(const gm::wire::GMFunction& callback)
{
    std::scoped_lock lock(g_callback_mutex);
    g_callback_le_disconnected = callback;
    GMBT_LOG("LE disconnected callback registered");
    return true;
}

bool bluetooth_remove_callback_le_disconnected()
{
    std::scoped_lock lock(g_callback_mutex);
    g_callback_le_disconnected = GMFunction();
    GMBT_LOG("LE disconnected callback removed");
    return true;
}

bool bluetooth_set_callback_le_characteristic_value_changed(const gm::wire::GMFunction& callback)
{
    std::scoped_lock lock(g_callback_mutex);
    g_callback_le_characteristic_value_changed = callback;
    GMBT_LOG("LE characteristic value changed callback registered");
    return true;
}

bool bluetooth_remove_callback_le_characteristic_value_changed()
{
    std::scoped_lock lock(g_callback_mutex);
    g_callback_le_characteristic_value_changed = GMFunction();
    GMBT_LOG("LE characteristic value changed callback removed");
    return true;
}

bool bluetooth_set_callback_le_server_connection_state_changed(const gm::wire::GMFunction& callback)
{
    std::scoped_lock lock(g_callback_mutex);
    g_callback_le_server_connection_state_changed = callback;
    GMBT_LOG("LE server connection state changed callback registered");
    return true;
}

bool bluetooth_remove_callback_le_server_connection_state_changed()
{
    std::scoped_lock lock(g_callback_mutex);
    g_callback_le_server_connection_state_changed = GMFunction();
    GMBT_LOG("LE server connection state changed callback removed");
    return true;
}

bool bluetooth_set_callback_le_server_read_request(const gm::wire::GMFunction& callback)
{
    std::scoped_lock lock(g_callback_mutex);
    g_callback_le_server_read_request = callback;
    GMBT_LOG("LE server read request callback registered");
    return true;
}

bool bluetooth_remove_callback_le_server_read_request()
{
    std::scoped_lock lock(g_callback_mutex);
    g_callback_le_server_read_request = GMFunction();
    GMBT_LOG("LE server read request callback removed");
    return true;
}

bool bluetooth_set_callback_le_server_write_request(const gm::wire::GMFunction& callback)
{
    std::scoped_lock lock(g_callback_mutex);
    g_callback_le_server_write_request = callback;
    GMBT_LOG("LE server write request callback registered");
    return true;
}

bool bluetooth_remove_callback_le_server_write_request()
{
    std::scoped_lock lock(g_callback_mutex);
    g_callback_le_server_write_request = GMFunction();
    GMBT_LOG("LE server write request callback removed");
    return true;
}
