#include "GMBluetooth_native.h"
#include "GMBluetooth_backend.h"
#include "GMBluetooth_log.h"
#include "GMBluetooth_json.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <deque>
#include <optional>

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

    // Parent-scoped handle caches for GATT services/characteristics/descriptors.
    // Handles are 1-based indices into a flat vector, same idiom as DeviceManager;
    // find_or_insert is idempotent so re-running discovery doesn't mint duplicates.
    struct ServiceEntry
    {
        std::uint64_t connection = 0;
        std::string uuid;
    };

    class ServiceCache
    {
    public:
        std::uint64_t find_or_insert(std::uint64_t connection, const std::string& uuid)
        {
            std::scoped_lock lock(mutex_);
            for (std::size_t i = 0; i < entries_.size(); ++i)
            {
                if (entries_[i].connection == connection && entries_[i].uuid == uuid)
                    return i + 1;
            }
            entries_.push_back(ServiceEntry{ connection, uuid });
            return entries_.size();
        }

        std::uint64_t find_by_uuid(std::uint64_t connection, const std::string& uuid) const
        {
            std::scoped_lock lock(mutex_);
            for (std::size_t i = 0; i < entries_.size(); ++i)
            {
                if (entries_[i].connection == connection && entries_[i].uuid == uuid)
                    return i + 1;
            }
            return 0;
        }

        int get_count(std::uint64_t connection) const
        {
            std::scoped_lock lock(mutex_);
            int count = 0;
            for (const auto& e : entries_)
                if (e.connection == connection)
                    ++count;
            return count;
        }

        std::uint64_t get_at(std::uint64_t connection, int index) const
        {
            std::scoped_lock lock(mutex_);
            int seen = 0;
            for (std::size_t i = 0; i < entries_.size(); ++i)
            {
                if (entries_[i].connection != connection)
                    continue;
                if (seen == index)
                    return i + 1;
                ++seen;
            }
            return 0;
        }

        bool is_valid(std::uint64_t handle) const
        {
            std::scoped_lock lock(mutex_);
            return handle > 0 && handle <= entries_.size();
        }

        std::string get_uuid(std::uint64_t handle) const
        {
            std::scoped_lock lock(mutex_);
            if (handle == 0 || handle > entries_.size())
                return {};
            return entries_[handle - 1].uuid;
        }

        std::uint64_t get_parent(std::uint64_t handle) const
        {
            std::scoped_lock lock(mutex_);
            if (handle == 0 || handle > entries_.size())
                return 0;
            return entries_[handle - 1].connection;
        }

    private:
        mutable std::mutex mutex_;
        std::vector<ServiceEntry> entries_;
    };

    ServiceCache g_service_cache;

    struct CharacteristicEntry
    {
        std::uint64_t service = 0;
        std::string uuid;
        std::int32_t properties = 0;
        std::vector<std::uint8_t> value;
        bool has_value = false;
    };

    class CharacteristicCache
    {
    public:
        std::uint64_t find_or_insert(std::uint64_t service, const std::string& uuid, std::int32_t properties)
        {
            std::scoped_lock lock(mutex_);
            for (std::size_t i = 0; i < entries_.size(); ++i)
            {
                if (entries_[i].service == service && entries_[i].uuid == uuid)
                {
                    entries_[i].properties = properties;
                    return i + 1;
                }
            }
            CharacteristicEntry entry;
            entry.service = service;
            entry.uuid = uuid;
            entry.properties = properties;
            entries_.push_back(std::move(entry));
            return entries_.size();
        }

        std::uint64_t find_by_uuid(std::uint64_t service, const std::string& uuid) const
        {
            std::scoped_lock lock(mutex_);
            for (std::size_t i = 0; i < entries_.size(); ++i)
            {
                if (entries_[i].service == service && entries_[i].uuid == uuid)
                    return i + 1;
            }
            return 0;
        }

        int get_count(std::uint64_t service) const
        {
            std::scoped_lock lock(mutex_);
            int count = 0;
            for (const auto& e : entries_)
                if (e.service == service)
                    ++count;
            return count;
        }

        std::uint64_t get_at(std::uint64_t service, int index) const
        {
            std::scoped_lock lock(mutex_);
            int seen = 0;
            for (std::size_t i = 0; i < entries_.size(); ++i)
            {
                if (entries_[i].service != service)
                    continue;
                if (seen == index)
                    return i + 1;
                ++seen;
            }
            return 0;
        }

        bool is_valid(std::uint64_t handle) const
        {
            std::scoped_lock lock(mutex_);
            return handle > 0 && handle <= entries_.size();
        }

        std::string get_uuid(std::uint64_t handle) const
        {
            std::scoped_lock lock(mutex_);
            if (handle == 0 || handle > entries_.size())
                return {};
            return entries_[handle - 1].uuid;
        }

        std::uint64_t get_parent(std::uint64_t handle) const
        {
            std::scoped_lock lock(mutex_);
            if (handle == 0 || handle > entries_.size())
                return 0;
            return entries_[handle - 1].service;
        }

        std::int32_t get_properties(std::uint64_t handle) const
        {
            std::scoped_lock lock(mutex_);
            if (handle == 0 || handle > entries_.size())
                return 0;
            return entries_[handle - 1].properties;
        }

        void set_value(std::uint64_t handle, std::vector<std::uint8_t> value)
        {
            std::scoped_lock lock(mutex_);
            if (handle == 0 || handle > entries_.size())
                return;
            entries_[handle - 1].value = std::move(value);
            entries_[handle - 1].has_value = true;
        }

        std::int32_t get_value(std::uint64_t handle, std::uint8_t* out, std::size_t max_size) const
        {
            std::scoped_lock lock(mutex_);
            if (handle == 0 || handle > entries_.size() || !entries_[handle - 1].has_value)
                return 0;
            const auto& value = entries_[handle - 1].value;
            const std::size_t n = std::min(max_size, value.size());
            if (n > 0 && out)
                std::memcpy(out, value.data(), n);
            return static_cast<std::int32_t>(n);
        }

    private:
        mutable std::mutex mutex_;
        std::vector<CharacteristicEntry> entries_;
    };

    CharacteristicCache g_characteristic_cache;

    struct DescriptorEntry
    {
        std::uint64_t characteristic = 0;
        std::string uuid;
        std::vector<std::uint8_t> value;
        bool has_value = false;
    };

    class DescriptorCache
    {
    public:
        std::uint64_t find_or_insert(std::uint64_t characteristic, const std::string& uuid)
        {
            std::scoped_lock lock(mutex_);
            for (std::size_t i = 0; i < entries_.size(); ++i)
            {
                if (entries_[i].characteristic == characteristic && entries_[i].uuid == uuid)
                    return i + 1;
            }
            entries_.push_back(DescriptorEntry{ characteristic, uuid, {}, false });
            return entries_.size();
        }

        std::uint64_t find_by_uuid(std::uint64_t characteristic, const std::string& uuid) const
        {
            std::scoped_lock lock(mutex_);
            for (std::size_t i = 0; i < entries_.size(); ++i)
            {
                if (entries_[i].characteristic == characteristic && entries_[i].uuid == uuid)
                    return i + 1;
            }
            return 0;
        }

        int get_count(std::uint64_t characteristic) const
        {
            std::scoped_lock lock(mutex_);
            int count = 0;
            for (const auto& e : entries_)
                if (e.characteristic == characteristic)
                    ++count;
            return count;
        }

        std::uint64_t get_at(std::uint64_t characteristic, int index) const
        {
            std::scoped_lock lock(mutex_);
            int seen = 0;
            for (std::size_t i = 0; i < entries_.size(); ++i)
            {
                if (entries_[i].characteristic != characteristic)
                    continue;
                if (seen == index)
                    return i + 1;
                ++seen;
            }
            return 0;
        }

        bool is_valid(std::uint64_t handle) const
        {
            std::scoped_lock lock(mutex_);
            return handle > 0 && handle <= entries_.size();
        }

        std::string get_uuid(std::uint64_t handle) const
        {
            std::scoped_lock lock(mutex_);
            if (handle == 0 || handle > entries_.size())
                return {};
            return entries_[handle - 1].uuid;
        }

        std::uint64_t get_parent(std::uint64_t handle) const
        {
            std::scoped_lock lock(mutex_);
            if (handle == 0 || handle > entries_.size())
                return 0;
            return entries_[handle - 1].characteristic;
        }

        void set_value(std::uint64_t handle, std::vector<std::uint8_t> value)
        {
            std::scoped_lock lock(mutex_);
            if (handle == 0 || handle > entries_.size())
                return;
            entries_[handle - 1].value = std::move(value);
            entries_[handle - 1].has_value = true;
        }

        std::int32_t get_value(std::uint64_t handle, std::uint8_t* out, std::size_t max_size) const
        {
            std::scoped_lock lock(mutex_);
            if (handle == 0 || handle > entries_.size() || !entries_[handle - 1].has_value)
                return 0;
            const auto& value = entries_[handle - 1].value;
            const std::size_t n = std::min(max_size, value.size());
            if (n > 0 && out)
                std::memcpy(out, value.data(), n);
            return static_cast<std::int32_t>(n);
        }

    private:
        mutable std::mutex mutex_;
        std::vector<DescriptorEntry> entries_;
    };

    DescriptorCache g_descriptor_cache;

    // FIFO correlation for LE async completions. Every call that dispatches to
    // the backend pushes an entry - even with a null callback - and every
    // matching completion event pops exactly one, because Apple's (and the
    // mirrored Windows/Android) backends complete these operations strictly in
    // call order on a single serial queue per operation type. Skipping the push
    // when there's no callback would desync this queue against a later call's
    // completion.
    struct NoContext {};

    template <typename Context>
    struct PendingLeOp
    {
        GMFunction callback;
        Context context{};
    };

    template <typename Context>
    class LeOpQueue
    {
    public:
        void push(GMFunction callback, Context context)
        {
            std::scoped_lock lock(mutex_);
            queue_.push_back(PendingLeOp<Context>{ std::move(callback), std::move(context) });
        }

        bool pop_front(PendingLeOp<Context>& out)
        {
            std::scoped_lock lock(mutex_);
            if (queue_.empty())
                return false;
            out = std::move(queue_.front());
            queue_.pop_front();
            return true;
        }

        // Rolls back the push() done just before a backend call that then failed
        // synchronously, since no completion event will ever arrive for it.
        void cancel_last()
        {
            std::scoped_lock lock(mutex_);
            if (!queue_.empty())
                queue_.pop_back();
        }

    private:
        mutable std::mutex mutex_;
        std::deque<PendingLeOp<Context>> queue_;
    };

    LeOpQueue<std::uint64_t> g_services_discover_queue;
    LeOpQueue<std::uint64_t> g_characteristics_discover_queue;
    LeOpQueue<std::uint64_t> g_descriptors_discover_queue;
    LeOpQueue<std::uint64_t> g_characteristic_read_queue;
    LeOpQueue<std::uint64_t> g_characteristic_write_queue;
    LeOpQueue<std::uint64_t> g_characteristic_subscribe_queue;
    LeOpQueue<std::uint64_t> g_descriptor_read_queue;
    LeOpQueue<std::uint64_t> g_descriptor_write_queue;
    LeOpQueue<NoContext> g_server_add_service_queue;
    LeOpQueue<NoContext> g_advertise_start_queue;

    struct PendingLeServerRequest
    {
        std::uint64_t connection = 0;
        std::string service_uuid;
        std::string characteristic_uuid;
        std::string descriptor_uuid;
        bool is_write = false;
        std::vector<std::uint8_t> write_value;
    };

    std::mutex g_pending_le_server_requests_mutex;
    std::unordered_map<std::int32_t, PendingLeServerRequest> g_pending_le_server_requests;

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

            if (characteristic.value.has_value())
            {
                out += ",\"value\":";
                append_json_string(out, *characteristic.value);
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
        auto parsed = json::parse(event.json);
        if (!parsed || !parsed->is_object())
        {
            GMBT_LOG("LE event '%s' has unparseable JSON payload, dropping", event.event_type.c_str());
            g_dropped_events++;
            return;
        }
        const json::Value& root = *parsed;
        const std::string& type = event.event_type;

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
                : (le_event_error_code(root) == 133 ? Error::Timeout : Error::ConnectionFailed);
            const std::uint64_t device = g_le_connection_manager.get_device(connection);
            try
            {
                // callback(error_code, message, connection, device)
                callback.call(static_cast<double>(error), std::string(), static_cast<double>(connection), static_cast<double>(device));
            }
            catch (const std::exception& e)
            {
                GMBT_LOG("Error dispatching le_connect callback: %s", e.what());
            }
        }
        else if (type == "bluetooth_le_peripheral_disconnect")
        {
            GMFunction callback;
            { std::scoped_lock lock(g_callback_mutex); callback = g_callback_le_disconnected; }
            if (callback)
            {
                try
                {
                    // callback(connection, error_code, message) - Apple never reports
                    // which connection this is for on this event, so connection is 0.
                    callback.call(0.0, static_cast<double>(Error::Disconnected), std::string());
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

            GMFunction callback;
            { std::scoped_lock lock(g_callback_mutex); callback = g_callback_le_disconnected; }
            if (callback)
            {
                try
                {
                    callback.call(static_cast<double>(le_event_connection(root)), static_cast<double>(Error::Disconnected), std::string());
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
        else if (type == "bluetooth_le_peripheral_get_services")
        {
            PendingLeOp<std::uint64_t> op;
            if (!g_services_discover_queue.pop_front(op))
            {
                g_dropped_events++;
                return;
            }

            const bool failed = le_event_has_error(root);
            if (!failed)
            {
                if (auto services = le_event_nested(root, "services"); services && services->is_array())
                {
                    for (const auto& item : services->array_value)
                    {
                        const auto* uuid = item.find("uuid");
                        if (uuid && uuid->is_string())
                            g_service_cache.find_or_insert(op.context, uuid->string_value);
                    }
                }
            }

            if (op.callback)
            {
                try
                {
                    // callback(error_code, message, connection)
                    op.callback.call(static_cast<double>(failed ? Error::OperationFailed : Error::Ok), std::string(), static_cast<double>(op.context));
                }
                catch (const std::exception& e)
                {
                    GMBT_LOG("Error dispatching le_services_discover callback: %s", e.what());
                }
            }
        }
        else if (type == "bluetooth_le_service_get_characteristics")
        {
            PendingLeOp<std::uint64_t> op;
            if (!g_characteristics_discover_queue.pop_front(op))
            {
                g_dropped_events++;
                return;
            }

            const bool failed = le_event_has_error(root);
            if (!failed)
            {
                if (auto characteristics = le_event_nested(root, "characteristics"); characteristics && characteristics->is_array())
                {
                    for (const auto& item : characteristics->array_value)
                    {
                        const auto* uuid = item.find("uuid");
                        if (!uuid || !uuid->is_string())
                            continue;
                        const auto* properties = item.find("properties");
                        g_characteristic_cache.find_or_insert(op.context, uuid->string_value, properties ? properties->as_int(0) : 0);
                    }
                }
            }

            if (op.callback)
            {
                try
                {
                    // callback(error_code, message, service)
                    op.callback.call(static_cast<double>(failed ? Error::OperationFailed : Error::Ok), std::string(), static_cast<double>(op.context));
                }
                catch (const std::exception& e)
                {
                    GMBT_LOG("Error dispatching le_characteristics_discover callback: %s", e.what());
                }
            }
        }
        else if (type == "bluetooth_le_characteristic_get_descriptors")
        {
            PendingLeOp<std::uint64_t> op;
            if (!g_descriptors_discover_queue.pop_front(op))
            {
                g_dropped_events++;
                return;
            }

            const bool failed = le_event_has_error(root);
            if (!failed)
            {
                if (auto descriptors = le_event_nested(root, "descriptors"); descriptors && descriptors->is_array())
                {
                    for (const auto& item : descriptors->array_value)
                    {
                        const auto* uuid = item.find("uuid");
                        if (uuid && uuid->is_string())
                            g_descriptor_cache.find_or_insert(op.context, uuid->string_value);
                    }
                }
            }

            if (op.callback)
            {
                try
                {
                    // callback(error_code, message, characteristic)
                    op.callback.call(static_cast<double>(failed ? Error::OperationFailed : Error::Ok), std::string(), static_cast<double>(op.context));
                }
                catch (const std::exception& e)
                {
                    GMBT_LOG("Error dispatching le_descriptors_discover callback: %s", e.what());
                }
            }
        }
        else if (type == "bluetooth_le_characteristic_unsubscribe" ||
                 type == "bluetooth_le_characteristic_notify" ||
                 type == "bluetooth_le_characteristic_indicate")
        {
            PendingLeOp<std::uint64_t> op;
            if (!g_characteristic_subscribe_queue.pop_front(op))
            {
                g_dropped_events++;
                return;
            }

            const bool failed = le_event_has_error(root);
            if (op.callback)
            {
                try
                {
                    // callback(error_code, message, characteristic)
                    op.callback.call(static_cast<double>(failed ? Error::OperationFailed : Error::Ok), std::string(), static_cast<double>(op.context));
                }
                catch (const std::exception& e)
                {
                    GMBT_LOG("Error dispatching le_characteristic_subscribe callback: %s", e.what());
                }
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

            if (const auto* value = root.find("value"); value && value->is_string())
                g_characteristic_cache.set_value(characteristic, json::base64_decode(value->string_value));

            GMFunction callback;
            { std::scoped_lock lock(g_callback_mutex); callback = g_callback_le_characteristic_value_changed; }
            if (callback)
            {
                try
                {
                    // callback(characteristic, connection)
                    callback.call(static_cast<double>(characteristic), static_cast<double>(connection));
                }
                catch (const std::exception& e)
                {
                    GMBT_LOG("Error dispatching le_characteristic_value_changed callback: %s", e.what());
                }
            }
            else
            {
                g_dropped_events++;
            }
        }
        else if (type == "bluetooth_le_characteristic_read")
        {
            PendingLeOp<std::uint64_t> op;
            if (!g_characteristic_read_queue.pop_front(op))
            {
                g_dropped_events++;
                return;
            }

            const bool failed = le_event_has_error(root);
            if (!failed)
            {
                if (const auto* value = root.find("value"); value && value->is_string())
                    g_characteristic_cache.set_value(op.context, json::base64_decode(value->string_value));
            }

            if (op.callback)
            {
                try
                {
                    // callback(error_code, message, characteristic)
                    op.callback.call(static_cast<double>(failed ? Error::OperationFailed : Error::Ok), std::string(), static_cast<double>(op.context));
                }
                catch (const std::exception& e)
                {
                    GMBT_LOG("Error dispatching le_characteristic_read callback: %s", e.what());
                }
            }
        }
        else if (type == "bluetooth_le_characteristic_write_request" ||
                 type == "bluetooth_le_characteristic_write_command")
        {
            PendingLeOp<std::uint64_t> op;
            if (!g_characteristic_write_queue.pop_front(op))
            {
                g_dropped_events++;
                return;
            }

            const bool failed = le_event_has_error(root);
            if (op.callback)
            {
                try
                {
                    // callback(error_code, message, characteristic)
                    op.callback.call(static_cast<double>(failed ? Error::OperationFailed : Error::Ok), std::string(), static_cast<double>(op.context));
                }
                catch (const std::exception& e)
                {
                    GMBT_LOG("Error dispatching le_characteristic_write callback: %s", e.what());
                }
            }
        }
        else if (type == "bluetooth_le_descriptor_read")
        {
            PendingLeOp<std::uint64_t> op;
            if (!g_descriptor_read_queue.pop_front(op))
            {
                g_dropped_events++;
                return;
            }

            const bool failed = le_event_has_error(root);
            if (!failed)
            {
                if (const auto* value = root.find("value"); value && value->is_string())
                    g_descriptor_cache.set_value(op.context, json::base64_decode(value->string_value));
            }

            if (op.callback)
            {
                try
                {
                    // callback(error_code, message, descriptor)
                    op.callback.call(static_cast<double>(failed ? Error::OperationFailed : Error::Ok), std::string(), static_cast<double>(op.context));
                }
                catch (const std::exception& e)
                {
                    GMBT_LOG("Error dispatching le_descriptor_read callback: %s", e.what());
                }
            }
        }
        else if (type == "bluetooth_le_descriptor_write")
        {
            PendingLeOp<std::uint64_t> op;
            if (!g_descriptor_write_queue.pop_front(op))
            {
                g_dropped_events++;
                return;
            }

            const bool failed = le_event_has_error(root);
            if (op.callback)
            {
                try
                {
                    // callback(error_code, message, descriptor)
                    op.callback.call(static_cast<double>(failed ? Error::OperationFailed : Error::Ok), std::string(), static_cast<double>(op.context));
                }
                catch (const std::exception& e)
                {
                    GMBT_LOG("Error dispatching le_descriptor_write callback: %s", e.what());
                }
            }
        }
        else if (type == "bluetooth_le_advertise_start")
        {
            PendingLeOp<NoContext> op;
            if (!g_advertise_start_queue.pop_front(op))
            {
                g_dropped_events++;
                return;
            }

            const bool failed = le_event_has_error(root);
            if (op.callback)
            {
                try
                {
                    // callback(error_code, message)
                    op.callback.call(static_cast<double>(failed ? Error::OperationFailed : Error::Ok), std::string());
                }
                catch (const std::exception& e)
                {
                    GMBT_LOG("Error dispatching le_advertise_start callback: %s", e.what());
                }
            }
        }
        else if (type == "bluetooth_le_server_add_service")
        {
            PendingLeOp<NoContext> op;
            if (!g_server_add_service_queue.pop_front(op))
            {
                g_dropped_events++;
                return;
            }

            const bool failed = le_event_has_error(root);
            if (op.callback)
            {
                try
                {
                    // callback(error_code, message)
                    op.callback.call(static_cast<double>(failed ? Error::OperationFailed : Error::Ok), std::string());
                }
                catch (const std::exception& e)
                {
                    GMBT_LOG("Error dispatching le_server_add_service callback: %s", e.what());
                }
            }
        }
        else if (type == "bluetooth_le_server_connection_state_changed")
        {
            const auto* connected_field = root.find("connected");
            const bool connected = connected_field && connected_field->as_bool(false);

            std::uint64_t device = 0;
            if (auto nested = le_event_nested(root, "device"); nested && nested->is_object())
            {
                if (const auto* address = nested->find("address"); address && address->is_string())
                {
                    DiscoveredDevice d;
                    d.transport = Transport::LowEnergy;
                    d.id = address->string_value;
                    d.address = address->string_value;
                    d.address_available = true;
                    device = g_device_manager.upsert_device(d);
                }
            }

            GMFunction callback;
            { std::scoped_lock lock(g_callback_mutex); callback = g_callback_le_server_connection_state_changed; }
            if (callback)
            {
                try
                {
                    // callback(connection, connected, device) - Apple never reports
                    // which connection this is for on this event, so connection is 0.
                    callback.call(0.0, static_cast<double>(connected ? 1 : 0), static_cast<double>(device));
                }
                catch (const std::exception& e)
                {
                    GMBT_LOG("Error dispatching le_server_connection_state_changed callback: %s", e.what());
                }
            }
            else
            {
                g_dropped_events++;
            }
        }
        else if (type == "bluetooth_le_server_characteristic_read_request" ||
                 type == "bluetooth_le_server_descriptor_read_request")
        {
            const auto* request_id_field = root.find("request_id");
            const std::int32_t request_id = request_id_field ? request_id_field->as_int(0) : 0;
            const std::string service_uuid = le_event_string(root, "service_uuid");
            const std::string characteristic_uuid = le_event_string(root, "characteristic_uuid");
            const std::string descriptor_uuid = le_event_string(root, "descriptor_uuid");

            {
                std::scoped_lock lock(g_pending_le_server_requests_mutex);
                PendingLeServerRequest request;
                request.connection = 0; // never reported by Apple for this event
                request.service_uuid = service_uuid;
                request.characteristic_uuid = characteristic_uuid;
                request.descriptor_uuid = descriptor_uuid;
                request.is_write = false;
                g_pending_le_server_requests[request_id] = std::move(request);
            }

            GMFunction callback;
            { std::scoped_lock lock(g_callback_mutex); callback = g_callback_le_server_read_request; }
            if (callback)
            {
                try
                {
                    // callback(request_id, connection, service_uuid, characteristic_uuid, descriptor_uuid_or_empty, offset)
                    callback.call(static_cast<double>(request_id), 0.0, service_uuid, characteristic_uuid, descriptor_uuid, 0.0);
                }
                catch (const std::exception& e)
                {
                    GMBT_LOG("Error dispatching le_server_read_request callback: %s", e.what());
                }
            }
            else
            {
                g_dropped_events++;
            }
        }
        else if (type == "bluetooth_le_server_characteristic_write_request" ||
                 type == "bluetooth_le_server_descriptor_write_request")
        {
            const auto* request_id_field = root.find("request_id");
            const std::int32_t request_id = request_id_field ? request_id_field->as_int(0) : 0;
            const std::string service_uuid = le_event_string(root, "service_uuid");
            const std::string characteristic_uuid = le_event_string(root, "characteristic_uuid");
            const std::string descriptor_uuid = le_event_string(root, "descriptor_uuid");

            std::vector<std::uint8_t> value;
            if (const auto* value_field = root.find("value"); value_field && value_field->is_string())
                value = json::base64_decode(value_field->string_value);

            {
                std::scoped_lock lock(g_pending_le_server_requests_mutex);
                PendingLeServerRequest request;
                request.connection = 0; // never reported by Apple for this event
                request.service_uuid = service_uuid;
                request.characteristic_uuid = characteristic_uuid;
                request.descriptor_uuid = descriptor_uuid;
                request.is_write = true;
                request.write_value = std::move(value);
                g_pending_le_server_requests[request_id] = std::move(request);
            }

            GMFunction callback;
            { std::scoped_lock lock(g_callback_mutex); callback = g_callback_le_server_write_request; }
            if (callback)
            {
                try
                {
                    // callback(request_id, connection, service_uuid, characteristic_uuid, descriptor_uuid_or_empty)
                    callback.call(static_cast<double>(request_id), 0.0, service_uuid, characteristic_uuid, descriptor_uuid);
                }
                catch (const std::exception& e)
                {
                    GMBT_LOG("Error dispatching le_server_write_request callback: %s", e.what());
                }
            }
            else
            {
                g_dropped_events++;
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

            if (event.type == BackendEventType::LeEvent)
            {
                dispatch_le_event(event);
                return;
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
    g_last_error = error;
    g_last_error_message = message;

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

    if (g_backend)
    {
        g_backend->shutdown();
        g_backend.reset();
    }
    g_device_manager.clear();
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

bool bluetooth_pairing_is_supported(std::uint64_t device)
{
    if (!g_backend)
        return false;

    const auto* dev = g_device_manager.get_device(device);
    return dev && g_backend->pairing_is_supported(*dev);
}

std::int32_t bluetooth_pair(std::uint64_t device, const gm::wire::GMFunction& callback)
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return static_cast<std::int32_t>(Error::NotInitialized);
    }

    const auto* dev = g_device_manager.get_device(device);
    if (!dev)
    {
        g_last_error = Error::InvalidArgument;
        g_last_error_message = "Invalid device handle";
        return static_cast<std::int32_t>(Error::InvalidArgument);
    }

    // Registered before calling the backend: pairing runs asynchronously and may
    // push its DevicePaired completion event before this call even returns.
    if (callback)
    {
        std::scoped_lock lock(g_pending_pair_mutex);
        g_pending_pair_callbacks[device] = callback;
    }

    std::string message;
    const Error error = g_backend->pair(device, *dev, message);
    g_last_error = error;
    g_last_error_message = message;

    if (error != Error::Ok)
    {
        std::scoped_lock lock(g_pending_pair_mutex);
        g_pending_pair_callbacks.erase(device);
    }

    return static_cast<std::int32_t>(error);
}

bool bluetooth_device_is_paired(std::uint64_t device)
{
    const auto* dev = g_device_manager.get_device(device);
    return dev && g_backend && g_backend->is_paired(*dev);
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

std::int32_t bluetooth_classic_discoverable_start(std::int32_t duration_seconds)
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return static_cast<std::int32_t>(Error::NotInitialized);
    }

    std::string message;
    const Error error = g_backend->classic_discoverable_start(duration_seconds, message);
    g_last_error = error;
    g_last_error_message = message;
    return static_cast<std::int32_t>(error);
}

std::int32_t bluetooth_classic_discoverable_stop()
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return static_cast<std::int32_t>(Error::NotInitialized);
    }

    std::string message;
    const Error error = g_backend->classic_discoverable_stop(message);
    g_last_error = error;
    g_last_error_message = message;
    return static_cast<std::int32_t>(error);
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

    const auto* dev = g_device_manager.get_device(device);
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
    g_last_error = error;
    g_last_error_message = message;

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

std::int32_t bluetooth_le_disconnect(std::uint64_t connection)
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return static_cast<std::int32_t>(Error::NotInitialized);
    }

    std::string message;
    const Error error = g_backend->le_disconnect(connection, message);
    g_last_error = error;
    g_last_error_message = message;
    return static_cast<std::int32_t>(error);
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

std::int32_t bluetooth_le_services_discover(std::uint64_t connection, const gm::wire::GMFunction& callback)
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return static_cast<std::int32_t>(Error::NotInitialized);
    }

    if (!g_le_connection_manager.is_valid(connection))
    {
        g_last_error = Error::InvalidHandle;
        g_last_error_message = "Invalid LE connection handle";
        return static_cast<std::int32_t>(Error::InvalidHandle);
    }

    g_services_discover_queue.push(callback, connection);

    std::string message;
    const Error error = g_backend->le_services_discover(connection, message);
    g_last_error = error;
    g_last_error_message = message;

    if (error != Error::Ok)
        g_services_discover_queue.cancel_last();

    return static_cast<std::int32_t>(error);
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

std::int32_t bluetooth_le_characteristics_discover(std::uint64_t service, const gm::wire::GMFunction& callback)
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return static_cast<std::int32_t>(Error::NotInitialized);
    }

    if (!g_service_cache.is_valid(service))
    {
        g_last_error = Error::InvalidHandle;
        g_last_error_message = "Invalid service handle";
        return static_cast<std::int32_t>(Error::InvalidHandle);
    }

    const std::uint64_t connection = g_service_cache.get_parent(service);
    const std::string uuid = g_service_cache.get_uuid(service);

    g_characteristics_discover_queue.push(callback, service);

    std::string message;
    const Error error = g_backend->le_characteristics_discover(connection, uuid, message);
    g_last_error = error;
    g_last_error_message = message;

    if (error != Error::Ok)
        g_characteristics_discover_queue.cancel_last();

    return static_cast<std::int32_t>(error);
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

std::int32_t bluetooth_le_descriptors_discover(std::uint64_t characteristic, const gm::wire::GMFunction& callback)
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return static_cast<std::int32_t>(Error::NotInitialized);
    }

    if (!g_characteristic_cache.is_valid(characteristic))
    {
        g_last_error = Error::InvalidHandle;
        g_last_error_message = "Invalid characteristic handle";
        return static_cast<std::int32_t>(Error::InvalidHandle);
    }

    const std::uint64_t service = g_characteristic_cache.get_parent(characteristic);
    const std::uint64_t connection = g_service_cache.get_parent(service);
    const std::string service_uuid = g_service_cache.get_uuid(service);
    const std::string characteristic_uuid = g_characteristic_cache.get_uuid(characteristic);

    g_descriptors_discover_queue.push(callback, characteristic);

    std::string message;
    const Error error = g_backend->le_descriptors_discover(connection, service_uuid, characteristic_uuid, message);
    g_last_error = error;
    g_last_error_message = message;

    if (error != Error::Ok)
        g_descriptors_discover_queue.cancel_last();

    return static_cast<std::int32_t>(error);
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

std::int32_t bluetooth_le_characteristic_read(std::uint64_t characteristic, const gm::wire::GMFunction& callback)
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return static_cast<std::int32_t>(Error::NotInitialized);
    }

    if (!g_characteristic_cache.is_valid(characteristic))
    {
        g_last_error = Error::InvalidHandle;
        g_last_error_message = "Invalid characteristic handle";
        return static_cast<std::int32_t>(Error::InvalidHandle);
    }

    const std::uint64_t service = g_characteristic_cache.get_parent(characteristic);
    const std::uint64_t connection = g_service_cache.get_parent(service);
    const std::string service_uuid = g_service_cache.get_uuid(service);
    const std::string characteristic_uuid = g_characteristic_cache.get_uuid(characteristic);

    g_characteristic_read_queue.push(callback, characteristic);

    std::string message;
    const Error error = g_backend->le_characteristic_read(connection, service_uuid, characteristic_uuid, message);
    g_last_error = error;
    g_last_error_message = message;

    if (error != Error::Ok)
        g_characteristic_read_queue.cancel_last();

    return static_cast<std::int32_t>(error);
}

std::int32_t bluetooth_le_characteristic_get_value(std::uint64_t characteristic, struct gm::wire::GMBuffer out_data, unsigned int offset, unsigned int max_size)
{
    std::uint8_t* buffer = static_cast<std::uint8_t*>(out_data.data()) + offset;
    return g_characteristic_cache.get_value(characteristic, buffer, max_size);
}

std::int32_t bluetooth_le_characteristic_write(std::uint64_t characteristic, struct gm::wire::GMBuffer data, unsigned int offset, unsigned int size, std::int32_t write_type, const gm::wire::GMFunction& callback)
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return static_cast<std::int32_t>(Error::NotInitialized);
    }

    if (!g_characteristic_cache.is_valid(characteristic))
    {
        g_last_error = Error::InvalidHandle;
        g_last_error_message = "Invalid characteristic handle";
        return static_cast<std::int32_t>(Error::InvalidHandle);
    }

    const std::uint64_t service = g_characteristic_cache.get_parent(characteristic);
    const std::uint64_t connection = g_service_cache.get_parent(service);
    const std::string service_uuid = g_service_cache.get_uuid(service);
    const std::string characteristic_uuid = g_characteristic_cache.get_uuid(characteristic);

    const std::uint8_t* buffer = static_cast<const std::uint8_t*>(data.data()) + offset;
    const std::string value_base64 = json::base64_encode(buffer, size);
    const bool with_response = (write_type == 0);

    g_characteristic_write_queue.push(callback, characteristic);

    std::string message;
    const Error error = g_backend->le_characteristic_write(connection, service_uuid, characteristic_uuid, value_base64, with_response, message);
    g_last_error = error;
    g_last_error_message = message;

    if (error != Error::Ok)
        g_characteristic_write_queue.cancel_last();

    return static_cast<std::int32_t>(error);
}

std::int32_t bluetooth_le_characteristic_subscribe(std::uint64_t characteristic, std::int32_t mode, const gm::wire::GMFunction& callback)
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return static_cast<std::int32_t>(Error::NotInitialized);
    }

    if (!g_characteristic_cache.is_valid(characteristic))
    {
        g_last_error = Error::InvalidHandle;
        g_last_error_message = "Invalid characteristic handle";
        return static_cast<std::int32_t>(Error::InvalidHandle);
    }

    const std::uint64_t service = g_characteristic_cache.get_parent(characteristic);
    const std::uint64_t connection = g_service_cache.get_parent(service);
    const std::string service_uuid = g_service_cache.get_uuid(service);
    const std::string characteristic_uuid = g_characteristic_cache.get_uuid(characteristic);

    g_characteristic_subscribe_queue.push(callback, characteristic);

    std::string message;
    const Error error = g_backend->le_characteristic_subscribe(connection, service_uuid, characteristic_uuid, mode, message);
    g_last_error = error;
    g_last_error_message = message;

    if (error != Error::Ok)
        g_characteristic_subscribe_queue.cancel_last();

    return static_cast<std::int32_t>(error);
}

std::int32_t bluetooth_le_descriptor_read(std::uint64_t descriptor, const gm::wire::GMFunction& callback)
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return static_cast<std::int32_t>(Error::NotInitialized);
    }

    if (!g_descriptor_cache.is_valid(descriptor))
    {
        g_last_error = Error::InvalidHandle;
        g_last_error_message = "Invalid descriptor handle";
        return static_cast<std::int32_t>(Error::InvalidHandle);
    }

    const std::uint64_t characteristic = g_descriptor_cache.get_parent(descriptor);
    const std::uint64_t service = g_characteristic_cache.get_parent(characteristic);
    const std::uint64_t connection = g_service_cache.get_parent(service);
    const std::string service_uuid = g_service_cache.get_uuid(service);
    const std::string characteristic_uuid = g_characteristic_cache.get_uuid(characteristic);
    const std::string descriptor_uuid = g_descriptor_cache.get_uuid(descriptor);

    g_descriptor_read_queue.push(callback, descriptor);

    std::string message;
    const Error error = g_backend->le_descriptor_read(connection, service_uuid, characteristic_uuid, descriptor_uuid, message);
    g_last_error = error;
    g_last_error_message = message;

    if (error != Error::Ok)
        g_descriptor_read_queue.cancel_last();

    return static_cast<std::int32_t>(error);
}

std::int32_t bluetooth_le_descriptor_get_value(std::uint64_t descriptor, struct gm::wire::GMBuffer out_data, unsigned int offset, unsigned int max_size)
{
    std::uint8_t* buffer = static_cast<std::uint8_t*>(out_data.data()) + offset;
    return g_descriptor_cache.get_value(descriptor, buffer, max_size);
}

std::int32_t bluetooth_le_descriptor_write(std::uint64_t descriptor, struct gm::wire::GMBuffer data, unsigned int offset, unsigned int size, const gm::wire::GMFunction& callback)
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return static_cast<std::int32_t>(Error::NotInitialized);
    }

    if (!g_descriptor_cache.is_valid(descriptor))
    {
        g_last_error = Error::InvalidHandle;
        g_last_error_message = "Invalid descriptor handle";
        return static_cast<std::int32_t>(Error::InvalidHandle);
    }

    const std::uint64_t characteristic = g_descriptor_cache.get_parent(descriptor);
    const std::uint64_t service = g_characteristic_cache.get_parent(characteristic);
    const std::uint64_t connection = g_service_cache.get_parent(service);
    const std::string service_uuid = g_service_cache.get_uuid(service);
    const std::string characteristic_uuid = g_characteristic_cache.get_uuid(characteristic);
    const std::string descriptor_uuid = g_descriptor_cache.get_uuid(descriptor);

    const std::uint8_t* buffer = static_cast<const std::uint8_t*>(data.data()) + offset;
    const std::string value_base64 = json::base64_encode(buffer, size);

    g_descriptor_write_queue.push(callback, descriptor);

    std::string message;
    const Error error = g_backend->le_descriptor_write(connection, service_uuid, characteristic_uuid, descriptor_uuid, value_base64, message);
    g_last_error = error;
    g_last_error_message = message;

    if (error != Error::Ok)
        g_descriptor_write_queue.cancel_last();

    return static_cast<std::int32_t>(error);
}

// --- BLE advertise ---

std::int32_t bluetooth_le_advertise_start(std::string_view settings_json, std::string_view data_json, const gm::wire::GMFunction& callback)
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return static_cast<std::int32_t>(Error::NotInitialized);
    }

    g_advertise_start_queue.push(callback, NoContext{});

    std::string message;
    const Error error = g_backend->le_advertise_start(std::string(settings_json), std::string(data_json), message);
    g_last_error = error;
    g_last_error_message = message;

    if (error != Error::Ok)
        g_advertise_start_queue.cancel_last();

    return static_cast<std::int32_t>(error);
}

std::int32_t bluetooth_le_advertise_stop()
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return static_cast<std::int32_t>(Error::NotInitialized);
    }

    std::string message;
    const Error error = g_backend->le_advertise_stop(message);
    g_last_error = error;
    g_last_error_message = message;
    return static_cast<std::int32_t>(error);
}

bool bluetooth_le_advertise_is_running()
{
    return g_backend && g_backend->le_advertise_is_running();
}

// --- BLE GATT server (peripheral) ---

std::int32_t bluetooth_le_server_start()
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return static_cast<std::int32_t>(Error::NotInitialized);
    }

    std::string message;
    const Error error = g_backend->le_server_start(message);
    g_last_error = error;
    g_last_error_message = message;
    return static_cast<std::int32_t>(error);
}

std::int32_t bluetooth_le_server_stop()
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return static_cast<std::int32_t>(Error::NotInitialized);
    }

    std::string message;
    const Error error = g_backend->le_server_stop(message);
    g_last_error = error;
    g_last_error_message = message;
    return static_cast<std::int32_t>(error);
}

bool bluetooth_le_server_is_running()
{
    return g_backend && g_backend->le_server_is_running();
}

std::int32_t bluetooth_le_server_add_service(const BluetoothLeServiceDefinition& service, const gm::wire::GMFunction& callback)
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return static_cast<std::int32_t>(Error::NotInitialized);
    }

    if (service.uuid.empty())
    {
        g_last_error = Error::InvalidArgument;
        g_last_error_message = "BLE service UUID is required";
        return static_cast<std::int32_t>(Error::InvalidArgument);
    }

    g_server_add_service_queue.push(callback, NoContext{});

    const std::string service_json = serialize_le_service_definition(service);
    std::string message;
    const Error error = g_backend->le_server_add_service(service_json, message);
    g_last_error = error;
    g_last_error_message = message;

    if (error != Error::Ok)
        g_server_add_service_queue.cancel_last();

    return static_cast<std::int32_t>(error);
}

std::int32_t bluetooth_le_server_clear_services()
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return static_cast<std::int32_t>(Error::NotInitialized);
    }

    std::string message;
    const Error error = g_backend->le_server_clear_services(message);
    g_last_error = error;
    g_last_error_message = message;
    return static_cast<std::int32_t>(error);
}

std::int32_t bluetooth_le_server_respond_read(std::int32_t request_id, std::int32_t error_code, struct gm::wire::GMBuffer data, unsigned int offset, unsigned int size)
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return static_cast<std::int32_t>(Error::NotInitialized);
    }

    {
        std::scoped_lock lock(g_pending_le_server_requests_mutex);
        g_pending_le_server_requests.erase(request_id);
    }

    const std::uint8_t* buffer = static_cast<const std::uint8_t*>(data.data()) + offset;
    const std::string value_base64 = json::base64_encode(buffer, size);

    std::string message;
    const Error error = g_backend->le_server_respond_read(request_id, error_code, value_base64, message);
    g_last_error = error;
    g_last_error_message = message;
    return static_cast<std::int32_t>(error);
}

std::int32_t bluetooth_le_server_respond_write(std::int32_t request_id, std::int32_t error_code)
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return static_cast<std::int32_t>(Error::NotInitialized);
    }

    {
        std::scoped_lock lock(g_pending_le_server_requests_mutex);
        g_pending_le_server_requests.erase(request_id);
    }

    std::string message;
    const Error error = g_backend->le_server_respond_write(request_id, error_code, message);
    g_last_error = error;
    g_last_error_message = message;
    return static_cast<std::int32_t>(error);
}

std::int32_t bluetooth_le_server_write_request_get_value(std::int32_t request_id, struct gm::wire::GMBuffer out_data, unsigned int offset, unsigned int max_size)
{
    std::scoped_lock lock(g_pending_le_server_requests_mutex);
    const auto it = g_pending_le_server_requests.find(request_id);
    if (it == g_pending_le_server_requests.end() || !it->second.is_write)
        return 0;

    const auto& value = it->second.write_value;
    const std::size_t n = std::min(static_cast<std::size_t>(max_size), value.size());
    if (n > 0)
    {
        std::uint8_t* buffer = static_cast<std::uint8_t*>(out_data.data()) + offset;
        std::memcpy(buffer, value.data(), n);
    }
    return static_cast<std::int32_t>(n);
}

std::int32_t bluetooth_le_server_notify_value(std::string_view service_uuid, std::string_view characteristic_uuid, std::uint64_t connection, struct gm::wire::GMBuffer data, unsigned int offset, unsigned int size)
{
    if (!g_backend)
    {
        g_last_error = Error::NotInitialized;
        g_last_error_message = "Bluetooth backend is not initialized";
        return static_cast<std::int32_t>(Error::NotInitialized);
    }

    // The backend broadcasts to every current subscriber; there is no
    // per-connection targeting available below this layer yet, so
    // `connection` is accepted for forward-compatibility but not honored.
    (void)connection;

    const std::uint8_t* buffer = static_cast<const std::uint8_t*>(data.data()) + offset;
    const std::string value_base64 = json::base64_encode(buffer, size);

    std::string message;
    const Error error = g_backend->le_server_notify_value(std::string(service_uuid), std::string(characteristic_uuid), value_base64, message);
    g_last_error = error;
    g_last_error_message = message;
    return static_cast<std::int32_t>(error);
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
