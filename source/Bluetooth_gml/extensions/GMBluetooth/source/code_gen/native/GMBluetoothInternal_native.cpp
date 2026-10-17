// ##### extgen :: Auto-generated file do not edit!! #####

#include "GMBluetoothInternal_native.h"
#include "GMBluetoothInternal_exports.h"

using namespace gm_structs;
using namespace gm::wire::codec;

static gm::runtime::DispatchQueue __dispatch_queue;

// Internal function used for fetching dispatched function calls to GML
GMEXPORT double __EXT_NATIVE__GMBluetooth_invocation_handler(char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferWriter __bw{ __ret_buffer, static_cast<size_t>(__ret_buffer_length) };
    return __dispatch_queue.fetch(__bw);
}

static std::queue<gm::wire::GMBuffer> __buffer_queue;

// Internal function used for queueing buffers to native code
GMEXPORT double __EXT_NATIVE__GMBluetooth_queue_buffer(char* __arg_buffer, double __arg_buffer_length)
{
    gm::wire::GMBuffer __buff{__arg_buffer, static_cast<uint64_t>(__arg_buffer_length)};
    __buffer_queue.push(__buff);

    return 1.0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_initialize()
{
    auto&& __result = bluetooth_initialize();
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_shutdown()
{
    bluetooth_shutdown();
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_is_initialized()
{
    auto&& __result = bluetooth_is_initialized();
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_last_error_code(char* __ret_buffer, double __ret_buffer_length)
{
    auto&& __result = bluetooth_last_error_code();
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: enum BluetoothError
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT char* __EXT_NATIVE__bluetooth_last_error_message()
{
    static std::string __result;
    __result = bluetooth_last_error_message();
    return (char*)__result.c_str();
}

GMEXPORT double __EXT_NATIVE__bluetooth_feature_is_supported(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: feature, type: enum BluetoothFeature
    gm_enums::BluetoothFeature feature = gm::wire::codec::readValue<gm_enums::BluetoothFeature>(__br);

    auto&& __result = bluetooth_feature_is_supported(feature);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_permission_get_status(char* __ret_buffer, double __ret_buffer_length)
{
    auto&& __result = bluetooth_permission_get_status();
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: enum BluetoothPermissionStatus
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_permission_request(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    auto&& __result = bluetooth_permission_request(callback);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: enum BluetoothError
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_request_enable(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    auto&& __result = bluetooth_request_enable(callback);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: enum BluetoothError
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_scan_start(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: settings, type: optional<struct BluetoothLeScanSettings>
    std::optional<gm_structs::BluetoothLeScanSettings> settings = gm::wire::codec::readOptional<gm_structs::BluetoothLeScanSettings>(__br);

    // field: filters, type: optional<struct BluetoothLeScanFilter[]>
    std::optional<std::vector<gm_structs::BluetoothLeScanFilter>> filters = gm::wire::codec::readOptional<std::vector<gm_structs::BluetoothLeScanFilter>>(__br);

    // field: on_found, type: Function
    gm::wire::GMFunction on_found = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    // field: on_updated, type: optional<Function>
    std::optional<gm::wire::GMFunction> on_updated = std::nullopt;
    if (gm::wire::codec::readValue<bool>(__br))
    {
        on_updated = gm::wire::codec::readFunction(__br, &__dispatch_queue);
    }

    // field: on_stopped, type: optional<Function>
    std::optional<gm::wire::GMFunction> on_stopped = std::nullopt;
    if (gm::wire::codec::readValue<bool>(__br))
    {
        on_stopped = gm::wire::codec::readFunction(__br, &__dispatch_queue);
    }

    auto&& __result = bluetooth_le_scan_start(settings, filters, on_found, on_updated, on_stopped);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: enum BluetoothError
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_scan_stop(char* __ret_buffer, double __ret_buffer_length)
{
    auto&& __result = bluetooth_le_scan_stop();
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: enum BluetoothError
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_scan_is_running()
{
    auto&& __result = bluetooth_le_scan_is_running();
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_classic_scan_start(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: on_found, type: Function
    gm::wire::GMFunction on_found = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    // field: on_updated, type: optional<Function>
    std::optional<gm::wire::GMFunction> on_updated = std::nullopt;
    if (gm::wire::codec::readValue<bool>(__br))
    {
        on_updated = gm::wire::codec::readFunction(__br, &__dispatch_queue);
    }

    // field: on_stopped, type: optional<Function>
    std::optional<gm::wire::GMFunction> on_stopped = std::nullopt;
    if (gm::wire::codec::readValue<bool>(__br))
    {
        on_stopped = gm::wire::codec::readFunction(__br, &__dispatch_queue);
    }

    auto&& __result = bluetooth_classic_scan_start(on_found, on_updated, on_stopped);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: enum BluetoothError
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_classic_scan_stop(char* __ret_buffer, double __ret_buffer_length)
{
    auto&& __result = bluetooth_classic_scan_stop();
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: enum BluetoothError
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_classic_scan_is_running()
{
    auto&& __result = bluetooth_classic_scan_is_running();
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_device_clear()
{
    bluetooth_device_clear();
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_device_get_count()
{
    auto&& __result = bluetooth_device_get_count();
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_device_get_at(double index, char* __ret_buffer, double __ret_buffer_length)
{
    auto&& __result = bluetooth_device_get_at(static_cast<std::int32_t>(index));
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: UInt64
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_device_is_valid(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: device, type: UInt64
    std::uint64_t device = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = bluetooth_device_is_valid(device);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_device_get_transport(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: device, type: UInt64
    std::uint64_t device = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = bluetooth_device_get_transport(device);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: enum BluetoothTransport
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT char* __EXT_NATIVE__bluetooth_device_get_id(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: device, type: UInt64
    std::uint64_t device = gm::wire::codec::readValue<std::uint64_t>(__br);

    static std::string __result;
    __result = bluetooth_device_get_id(device);
    return (char*)__result.c_str();
}

GMEXPORT char* __EXT_NATIVE__bluetooth_device_get_name(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: device, type: UInt64
    std::uint64_t device = gm::wire::codec::readValue<std::uint64_t>(__br);

    static std::string __result;
    __result = bluetooth_device_get_name(device);
    return (char*)__result.c_str();
}

GMEXPORT double __EXT_NATIVE__bluetooth_device_get_address(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: device, type: UInt64
    std::uint64_t device = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = bluetooth_device_get_address(device);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: optional<String>
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_device_get_rssi(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: device, type: UInt64
    std::uint64_t device = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = bluetooth_device_get_rssi(device);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: optional<Int32>
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_device_get_last_seen_age(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: device, type: UInt64
    std::uint64_t device = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = bluetooth_device_get_last_seen_age(device);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: optional<Int32>
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_device_is_connectable(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: device, type: UInt64
    std::uint64_t device = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = bluetooth_device_is_connectable(device);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_device_get_advertisement(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: device, type: UInt64
    std::uint64_t device = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = bluetooth_device_get_advertisement(device);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: struct BluetoothLeAdvertisement
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_device_from_id(char* id, char* __ret_buffer, double __ret_buffer_length)
{
    auto&& __result = bluetooth_device_from_id(id);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: UInt64
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_connected_devices_query(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: service_uuids, type: String[]
    std::vector<std::string_view> service_uuids = gm::wire::codec::readVector<std::string_view>(__br);

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    auto&& __result = bluetooth_le_connected_devices_query(service_uuids, callback);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: enum BluetoothError
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_paired_devices_query(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    auto&& __result = bluetooth_paired_devices_query(callback);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: enum BluetoothError
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_classic_connect(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: device, type: UInt64
    std::uint64_t device = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: service_uuid, type: String
    std::string_view service_uuid = gm::wire::codec::readValue<std::string_view>(__br);

    // field: options, type: optional<struct BluetoothConnectOptions>
    std::optional<gm_structs::BluetoothConnectOptions> options = gm::wire::codec::readOptional<gm_structs::BluetoothConnectOptions>(__br);

    // field: on_data, type: Function
    gm::wire::GMFunction on_data = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    // field: on_disconnected, type: Function
    gm::wire::GMFunction on_disconnected = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    auto&& __result = bluetooth_classic_connect(device, service_uuid, options, on_data, on_disconnected, callback);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: UInt64
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_classic_disconnect(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: connection, type: UInt64
    std::uint64_t connection = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = bluetooth_classic_disconnect(connection);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: enum BluetoothError
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_classic_connection_is_valid(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: connection, type: UInt64
    std::uint64_t connection = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = bluetooth_classic_connection_is_valid(connection);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_classic_connection_is_connected(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: connection, type: UInt64
    std::uint64_t connection = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = bluetooth_classic_connection_is_connected(connection);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_classic_connection_get_device(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: connection, type: UInt64
    std::uint64_t connection = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = bluetooth_classic_connection_get_device(connection);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: UInt64
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_classic_send(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: connection, type: UInt64
    std::uint64_t connection = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: data, type: Buffer
    gm::wire::GMBuffer data = __buffer_queue.front();
    __buffer_queue.pop();

    // field: offset, type: UInt32
    std::uint32_t offset = gm::wire::codec::readValue<std::uint32_t>(__br);

    // field: size, type: UInt32
    std::uint32_t size = gm::wire::codec::readValue<std::uint32_t>(__br);

    auto&& __result = bluetooth_classic_send(connection, data, offset, size);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: enum BluetoothError
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_classic_server_start(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: name, type: String
    std::string_view name = gm::wire::codec::readValue<std::string_view>(__br);

    // field: service_uuid, type: String
    std::string_view service_uuid = gm::wire::codec::readValue<std::string_view>(__br);

    // field: on_connected, type: Function
    gm::wire::GMFunction on_connected = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    // field: on_data, type: Function
    gm::wire::GMFunction on_data = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    // field: on_disconnected, type: Function
    gm::wire::GMFunction on_disconnected = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    // field: on_stopped, type: optional<Function>
    std::optional<gm::wire::GMFunction> on_stopped = std::nullopt;
    if (gm::wire::codec::readValue<bool>(__br))
    {
        on_stopped = gm::wire::codec::readFunction(__br, &__dispatch_queue);
    }

    auto&& __result = bluetooth_classic_server_start(name, service_uuid, on_connected, on_data, on_disconnected, on_stopped);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: enum BluetoothError
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_classic_server_stop(char* __ret_buffer, double __ret_buffer_length)
{
    auto&& __result = bluetooth_classic_server_stop();
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: enum BluetoothError
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_classic_server_is_running()
{
    auto&& __result = bluetooth_classic_server_is_running();
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_classic_discoverable_start(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: duration_seconds, type: Int32
    std::int32_t duration_seconds = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    auto&& __result = bluetooth_classic_discoverable_start(duration_seconds, callback);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: enum BluetoothError
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_classic_discoverable_stop(char* __ret_buffer, double __ret_buffer_length)
{
    auto&& __result = bluetooth_classic_discoverable_stop();
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: enum BluetoothError
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_classic_discoverable_is_running()
{
    auto&& __result = bluetooth_classic_discoverable_is_running();
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_pairing_is_supported(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: device, type: UInt64
    std::uint64_t device = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = bluetooth_pairing_is_supported(device);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_pair(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: device, type: UInt64
    std::uint64_t device = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    auto&& __result = bluetooth_pair(device, callback);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: enum BluetoothError
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_device_is_paired(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: device, type: UInt64
    std::uint64_t device = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = bluetooth_device_is_paired(device);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_connect(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: device, type: UInt64
    std::uint64_t device = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: options, type: optional<struct BluetoothConnectOptions>
    std::optional<gm_structs::BluetoothConnectOptions> options = gm::wire::codec::readOptional<gm_structs::BluetoothConnectOptions>(__br);

    // field: on_disconnected, type: Function
    gm::wire::GMFunction on_disconnected = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    auto&& __result = bluetooth_le_connect(device, options, on_disconnected, callback);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: UInt64
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_disconnect(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: connection, type: UInt64
    std::uint64_t connection = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = bluetooth_le_disconnect(connection);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: enum BluetoothError
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_connection_is_valid(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: connection, type: UInt64
    std::uint64_t connection = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = bluetooth_le_connection_is_valid(connection);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_connection_is_connected(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: connection, type: UInt64
    std::uint64_t connection = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = bluetooth_le_connection_is_connected(connection);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_connection_get_device(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: connection, type: UInt64
    std::uint64_t connection = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = bluetooth_le_connection_get_device(connection);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: UInt64
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_connection_get_mtu(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: connection, type: UInt64
    std::uint64_t connection = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = bluetooth_le_connection_get_mtu(connection);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_connection_request_mtu(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: connection, type: UInt64
    std::uint64_t connection = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: mtu, type: Int32
    std::int32_t mtu = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    auto&& __result = bluetooth_le_connection_request_mtu(connection, mtu, callback);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: enum BluetoothError
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_connection_read_rssi(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: connection, type: UInt64
    std::uint64_t connection = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    auto&& __result = bluetooth_le_connection_read_rssi(connection, callback);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: enum BluetoothError
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_connection_request_priority(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: connection, type: UInt64
    std::uint64_t connection = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: priority, type: enum BluetoothLeConnectionPriority
    gm_enums::BluetoothLeConnectionPriority priority = gm::wire::codec::readValue<gm_enums::BluetoothLeConnectionPriority>(__br);

    auto&& __result = bluetooth_le_connection_request_priority(connection, priority);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: enum BluetoothError
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_services_discover(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: connection, type: UInt64
    std::uint64_t connection = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: options, type: optional<struct BluetoothLeDiscoverOptions>
    std::optional<gm_structs::BluetoothLeDiscoverOptions> options = gm::wire::codec::readOptional<gm_structs::BluetoothLeDiscoverOptions>(__br);

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    auto&& __result = bluetooth_le_services_discover(connection, options, callback);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: enum BluetoothError
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_service_get_count(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: connection, type: UInt64
    std::uint64_t connection = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = bluetooth_le_service_get_count(connection);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_service_get_at(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: connection, type: UInt64
    std::uint64_t connection = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: index, type: Int32
    std::int32_t index = gm::wire::codec::readValue<std::int32_t>(__br);

    auto&& __result = bluetooth_le_service_get_at(connection, index);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: UInt64
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT char* __EXT_NATIVE__bluetooth_le_service_get_uuid(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: service, type: UInt64
    std::uint64_t service = gm::wire::codec::readValue<std::uint64_t>(__br);

    static std::string __result;
    __result = bluetooth_le_service_get_uuid(service);
    return (char*)__result.c_str();
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_service_find(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: connection, type: UInt64
    std::uint64_t connection = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: uuid, type: String
    std::string_view uuid = gm::wire::codec::readValue<std::string_view>(__br);

    auto&& __result = bluetooth_le_service_find(connection, uuid);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: UInt64
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_characteristics_discover(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: service, type: UInt64
    std::uint64_t service = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    auto&& __result = bluetooth_le_characteristics_discover(service, callback);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: enum BluetoothError
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_characteristic_get_count(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: service, type: UInt64
    std::uint64_t service = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = bluetooth_le_characteristic_get_count(service);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_characteristic_get_at(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: service, type: UInt64
    std::uint64_t service = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: index, type: Int32
    std::int32_t index = gm::wire::codec::readValue<std::int32_t>(__br);

    auto&& __result = bluetooth_le_characteristic_get_at(service, index);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: UInt64
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT char* __EXT_NATIVE__bluetooth_le_characteristic_get_uuid(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: characteristic, type: UInt64
    std::uint64_t characteristic = gm::wire::codec::readValue<std::uint64_t>(__br);

    static std::string __result;
    __result = bluetooth_le_characteristic_get_uuid(characteristic);
    return (char*)__result.c_str();
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_characteristic_find(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: service, type: UInt64
    std::uint64_t service = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: uuid, type: String
    std::string_view uuid = gm::wire::codec::readValue<std::string_view>(__br);

    auto&& __result = bluetooth_le_characteristic_find(service, uuid);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: UInt64
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_characteristic_get_properties(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: characteristic, type: UInt64
    std::uint64_t characteristic = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = bluetooth_le_characteristic_get_properties(characteristic);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_descriptors_discover(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: characteristic, type: UInt64
    std::uint64_t characteristic = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    auto&& __result = bluetooth_le_descriptors_discover(characteristic, callback);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: enum BluetoothError
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_descriptor_get_count(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: characteristic, type: UInt64
    std::uint64_t characteristic = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = bluetooth_le_descriptor_get_count(characteristic);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_descriptor_get_at(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: characteristic, type: UInt64
    std::uint64_t characteristic = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: index, type: Int32
    std::int32_t index = gm::wire::codec::readValue<std::int32_t>(__br);

    auto&& __result = bluetooth_le_descriptor_get_at(characteristic, index);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: UInt64
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT char* __EXT_NATIVE__bluetooth_le_descriptor_get_uuid(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: descriptor, type: UInt64
    std::uint64_t descriptor = gm::wire::codec::readValue<std::uint64_t>(__br);

    static std::string __result;
    __result = bluetooth_le_descriptor_get_uuid(descriptor);
    return (char*)__result.c_str();
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_descriptor_find(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: characteristic, type: UInt64
    std::uint64_t characteristic = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: uuid, type: String
    std::string_view uuid = gm::wire::codec::readValue<std::string_view>(__br);

    auto&& __result = bluetooth_le_descriptor_find(characteristic, uuid);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: UInt64
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_characteristic_read(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: characteristic, type: UInt64
    std::uint64_t characteristic = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    auto&& __result = bluetooth_le_characteristic_read(characteristic, callback);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: enum BluetoothError
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_characteristic_write(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: characteristic, type: UInt64
    std::uint64_t characteristic = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: data, type: Buffer
    gm::wire::GMBuffer data = __buffer_queue.front();
    __buffer_queue.pop();

    // field: offset, type: UInt32
    std::uint32_t offset = gm::wire::codec::readValue<std::uint32_t>(__br);

    // field: size, type: UInt32
    std::uint32_t size = gm::wire::codec::readValue<std::uint32_t>(__br);

    // field: write_type, type: enum BluetoothLeWriteType
    gm_enums::BluetoothLeWriteType write_type = gm::wire::codec::readValue<gm_enums::BluetoothLeWriteType>(__br);

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    auto&& __result = bluetooth_le_characteristic_write(characteristic, data, offset, size, write_type, callback);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: enum BluetoothError
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_characteristic_subscribe(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: characteristic, type: UInt64
    std::uint64_t characteristic = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: mode, type: enum BluetoothLeSubscribeMode
    gm_enums::BluetoothLeSubscribeMode mode = gm::wire::codec::readValue<gm_enums::BluetoothLeSubscribeMode>(__br);

    // field: on_value, type: Function
    gm::wire::GMFunction on_value = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    auto&& __result = bluetooth_le_characteristic_subscribe(characteristic, mode, on_value, callback);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: enum BluetoothError
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_characteristic_unsubscribe(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: characteristic, type: UInt64
    std::uint64_t characteristic = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    auto&& __result = bluetooth_le_characteristic_unsubscribe(characteristic, callback);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: enum BluetoothError
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_descriptor_read(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: descriptor, type: UInt64
    std::uint64_t descriptor = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    auto&& __result = bluetooth_le_descriptor_read(descriptor, callback);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: enum BluetoothError
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_descriptor_write(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: descriptor, type: UInt64
    std::uint64_t descriptor = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: data, type: Buffer
    gm::wire::GMBuffer data = __buffer_queue.front();
    __buffer_queue.pop();

    // field: offset, type: UInt32
    std::uint32_t offset = gm::wire::codec::readValue<std::uint32_t>(__br);

    // field: size, type: UInt32
    std::uint32_t size = gm::wire::codec::readValue<std::uint32_t>(__br);

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    auto&& __result = bluetooth_le_descriptor_write(descriptor, data, offset, size, callback);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: enum BluetoothError
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_value_fetch(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: value, type: UInt64
    std::uint64_t value = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: out_data, type: Buffer
    gm::wire::GMBuffer out_data = __buffer_queue.front();
    __buffer_queue.pop();

    // field: offset, type: UInt32
    std::uint32_t offset = gm::wire::codec::readValue<std::uint32_t>(__br);

    auto&& __result = bluetooth_value_fetch(value, out_data, offset);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: enum BluetoothError
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_value_release(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: value, type: UInt64
    std::uint64_t value = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = bluetooth_value_release(value);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: enum BluetoothError
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_advertise_start(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: settings, type: optional<struct BluetoothLeAdvertiseSettings>
    std::optional<gm_structs::BluetoothLeAdvertiseSettings> settings = gm::wire::codec::readOptional<gm_structs::BluetoothLeAdvertiseSettings>(__br);

    // field: data, type: struct BluetoothLeAdvertiseData
    gm_structs::BluetoothLeAdvertiseData data = gm::wire::codec::readValue<gm_structs::BluetoothLeAdvertiseData>(__br);

    // field: on_stopped, type: optional<Function>
    std::optional<gm::wire::GMFunction> on_stopped = std::nullopt;
    if (gm::wire::codec::readValue<bool>(__br))
    {
        on_stopped = gm::wire::codec::readFunction(__br, &__dispatch_queue);
    }

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    auto&& __result = bluetooth_le_advertise_start(settings, data, on_stopped, callback);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: enum BluetoothError
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_advertise_stop(char* __ret_buffer, double __ret_buffer_length)
{
    auto&& __result = bluetooth_le_advertise_stop();
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: enum BluetoothError
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_advertise_is_running()
{
    auto&& __result = bluetooth_le_advertise_is_running();
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_server_start(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: on_connected, type: Function
    gm::wire::GMFunction on_connected = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    // field: on_disconnected, type: Function
    gm::wire::GMFunction on_disconnected = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    // field: on_read, type: Function
    gm::wire::GMFunction on_read = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    // field: on_write, type: Function
    gm::wire::GMFunction on_write = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    // field: on_stopped, type: optional<Function>
    std::optional<gm::wire::GMFunction> on_stopped = std::nullopt;
    if (gm::wire::codec::readValue<bool>(__br))
    {
        on_stopped = gm::wire::codec::readFunction(__br, &__dispatch_queue);
    }

    auto&& __result = bluetooth_le_server_start(on_connected, on_disconnected, on_read, on_write, on_stopped);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: enum BluetoothError
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_server_stop(char* __ret_buffer, double __ret_buffer_length)
{
    auto&& __result = bluetooth_le_server_stop();
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: enum BluetoothError
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_server_is_running()
{
    auto&& __result = bluetooth_le_server_is_running();
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_server_add_service(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: service, type: struct BluetoothLeServiceDefinition
    gm_structs::BluetoothLeServiceDefinition service = gm::wire::codec::readValue<gm_structs::BluetoothLeServiceDefinition>(__br);

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    auto&& __result = bluetooth_le_server_add_service(service, callback);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: enum BluetoothError
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_server_clear_services(char* __ret_buffer, double __ret_buffer_length)
{
    auto&& __result = bluetooth_le_server_clear_services();
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: enum BluetoothError
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_server_respond_read(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: request_id, type: Int32
    std::int32_t request_id = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: data, type: Buffer
    gm::wire::GMBuffer data = __buffer_queue.front();
    __buffer_queue.pop();

    // field: offset, type: UInt32
    std::uint32_t offset = gm::wire::codec::readValue<std::uint32_t>(__br);

    // field: size, type: UInt32
    std::uint32_t size = gm::wire::codec::readValue<std::uint32_t>(__br);

    auto&& __result = bluetooth_le_server_respond_read(request_id, data, offset, size);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: enum BluetoothError
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_server_respond_read_error(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: request_id, type: Int32
    std::int32_t request_id = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: error_code, type: enum BluetoothAttError
    gm_enums::BluetoothAttError error_code = gm::wire::codec::readValue<gm_enums::BluetoothAttError>(__br);

    auto&& __result = bluetooth_le_server_respond_read_error(request_id, error_code);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: enum BluetoothError
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_server_respond_write(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: request_id, type: Int32
    std::int32_t request_id = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: error_code, type: enum BluetoothAttError
    gm_enums::BluetoothAttError error_code = gm::wire::codec::readValue<gm_enums::BluetoothAttError>(__br);

    auto&& __result = bluetooth_le_server_respond_write(request_id, error_code);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: enum BluetoothError
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_server_notify_value(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: characteristic, type: UInt64
    std::uint64_t characteristic = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: connection, type: UInt64
    std::uint64_t connection = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: data, type: Buffer
    gm::wire::GMBuffer data = __buffer_queue.front();
    __buffer_queue.pop();

    // field: offset, type: UInt32
    std::uint32_t offset = gm::wire::codec::readValue<std::uint32_t>(__br);

    // field: size, type: UInt32
    std::uint32_t size = gm::wire::codec::readValue<std::uint32_t>(__br);

    auto&& __result = bluetooth_le_server_notify_value(characteristic, connection, data, offset, size);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: enum BluetoothError
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_set_callback_state_changed(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    auto&& __result = bluetooth_set_callback_state_changed(callback);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_remove_callback_state_changed()
{
    auto&& __result = bluetooth_remove_callback_state_changed();
    return static_cast<double>(__result);
}

