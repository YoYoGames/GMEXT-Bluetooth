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

GMEXPORT double __EXT_NATIVE__bluetooth_last_error_code()
{
    auto&& __result = bluetooth_last_error_code();
    return static_cast<double>(__result);
}

GMEXPORT char* __EXT_NATIVE__bluetooth_last_error_message()
{
    static std::string __result;
    __result = bluetooth_last_error_message();
    return (char*)__result.c_str();
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_is_supported()
{
    auto&& __result = bluetooth_le_is_supported();
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_classic_is_supported()
{
    auto&& __result = bluetooth_classic_is_supported();
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_classic_server_is_supported()
{
    auto&& __result = bluetooth_classic_server_is_supported();
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_permission_get_status()
{
    auto&& __result = bluetooth_permission_get_status();
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_permission_request()
{
    auto&& __result = bluetooth_permission_request();
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_scan_start(double active)
{
    auto&& __result = bluetooth_le_scan_start(static_cast<bool>(active));
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_scan_stop()
{
    auto&& __result = bluetooth_le_scan_stop();
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_scan_is_running()
{
    auto&& __result = bluetooth_le_scan_is_running();
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_classic_scan_start()
{
    auto&& __result = bluetooth_classic_scan_start();
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_classic_scan_stop()
{
    auto&& __result = bluetooth_classic_scan_stop();
    return static_cast<double>(__result);
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

GMEXPORT double __EXT_NATIVE__bluetooth_device_get_transport(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: device, type: UInt64
    std::uint64_t device = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = bluetooth_device_get_transport(device);
    return static_cast<double>(__result);
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

GMEXPORT double __EXT_NATIVE__bluetooth_device_has_address(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: device, type: UInt64
    std::uint64_t device = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = bluetooth_device_has_address(device);
    return static_cast<double>(__result);
}

GMEXPORT char* __EXT_NATIVE__bluetooth_device_get_address(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: device, type: UInt64
    std::uint64_t device = gm::wire::codec::readValue<std::uint64_t>(__br);

    static std::string __result;
    __result = bluetooth_device_get_address(device);
    return (char*)__result.c_str();
}

GMEXPORT double __EXT_NATIVE__bluetooth_device_has_rssi(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: device, type: UInt64
    std::uint64_t device = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = bluetooth_device_has_rssi(device);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_device_get_rssi(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: device, type: UInt64
    std::uint64_t device = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = bluetooth_device_get_rssi(device);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_device_is_connectable(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: device, type: UInt64
    std::uint64_t device = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = bluetooth_device_is_connectable(device);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_classic_connect(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: device, type: UInt64
    std::uint64_t device = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: service_uuid, type: String
    std::string_view service_uuid = gm::wire::codec::readValue<std::string_view>(__br);

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    auto&& __result = bluetooth_classic_connect(device, service_uuid, callback);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: UInt64
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_classic_disconnect(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: connection, type: UInt64
    std::uint64_t connection = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = bluetooth_classic_disconnect(connection);
    return static_cast<double>(__result);
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

GMEXPORT double __EXT_NATIVE__bluetooth_classic_receive_available(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: connection, type: UInt64
    std::uint64_t connection = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = bluetooth_classic_receive_available(connection);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_classic_send(char* __arg_buffer, double __arg_buffer_length)
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
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_classic_receive(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: connection, type: UInt64
    std::uint64_t connection = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: out_data, type: Buffer
    gm::wire::GMBuffer out_data = __buffer_queue.front();
    __buffer_queue.pop();

    // field: offset, type: UInt32
    std::uint32_t offset = gm::wire::codec::readValue<std::uint32_t>(__br);

    // field: max_size, type: UInt32
    std::uint32_t max_size = gm::wire::codec::readValue<std::uint32_t>(__br);

    auto&& __result = bluetooth_classic_receive(connection, out_data, offset, max_size);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_classic_server_start(char* name, char* service_uuid)
{
    auto&& __result = bluetooth_classic_server_start(name, service_uuid);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_classic_server_stop()
{
    auto&& __result = bluetooth_classic_server_stop();
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_classic_server_is_running()
{
    auto&& __result = bluetooth_classic_server_is_running();
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_connect(char* __arg_buffer, double __arg_buffer_length, char* __ret_buffer, double __ret_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: device, type: UInt64
    std::uint64_t device = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    auto&& __result = bluetooth_le_connect(device, callback);
    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: UInt64
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_disconnect(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: connection, type: UInt64
    std::uint64_t connection = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = bluetooth_le_disconnect(connection);
    return static_cast<double>(__result);
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

GMEXPORT double __EXT_NATIVE__bluetooth_le_services_discover(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: connection, type: UInt64
    std::uint64_t connection = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    auto&& __result = bluetooth_le_services_discover(connection, callback);
    return static_cast<double>(__result);
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

GMEXPORT double __EXT_NATIVE__bluetooth_le_characteristics_discover(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: service, type: UInt64
    std::uint64_t service = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    auto&& __result = bluetooth_le_characteristics_discover(service, callback);
    return static_cast<double>(__result);
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

GMEXPORT double __EXT_NATIVE__bluetooth_le_characteristic_get_properties(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: characteristic, type: UInt64
    std::uint64_t characteristic = gm::wire::codec::readValue<std::uint64_t>(__br);

    auto&& __result = bluetooth_le_characteristic_get_properties(characteristic);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_descriptors_discover(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: characteristic, type: UInt64
    std::uint64_t characteristic = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    auto&& __result = bluetooth_le_descriptors_discover(characteristic, callback);
    return static_cast<double>(__result);
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

GMEXPORT double __EXT_NATIVE__bluetooth_le_characteristic_read(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: characteristic, type: UInt64
    std::uint64_t characteristic = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    auto&& __result = bluetooth_le_characteristic_read(characteristic, callback);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_characteristic_get_value(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: characteristic, type: UInt64
    std::uint64_t characteristic = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: out_data, type: Buffer
    gm::wire::GMBuffer out_data = __buffer_queue.front();
    __buffer_queue.pop();

    // field: offset, type: UInt32
    std::uint32_t offset = gm::wire::codec::readValue<std::uint32_t>(__br);

    // field: max_size, type: UInt32
    std::uint32_t max_size = gm::wire::codec::readValue<std::uint32_t>(__br);

    auto&& __result = bluetooth_le_characteristic_get_value(characteristic, out_data, offset, max_size);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_characteristic_write(char* __arg_buffer, double __arg_buffer_length)
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

    // field: write_type, type: Int32
    std::int32_t write_type = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    auto&& __result = bluetooth_le_characteristic_write(characteristic, data, offset, size, write_type, callback);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_characteristic_subscribe(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: characteristic, type: UInt64
    std::uint64_t characteristic = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: mode, type: Int32
    std::int32_t mode = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    auto&& __result = bluetooth_le_characteristic_subscribe(characteristic, mode, callback);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_descriptor_read(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: descriptor, type: UInt64
    std::uint64_t descriptor = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    auto&& __result = bluetooth_le_descriptor_read(descriptor, callback);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_descriptor_get_value(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: descriptor, type: UInt64
    std::uint64_t descriptor = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: out_data, type: Buffer
    gm::wire::GMBuffer out_data = __buffer_queue.front();
    __buffer_queue.pop();

    // field: offset, type: UInt32
    std::uint32_t offset = gm::wire::codec::readValue<std::uint32_t>(__br);

    // field: max_size, type: UInt32
    std::uint32_t max_size = gm::wire::codec::readValue<std::uint32_t>(__br);

    auto&& __result = bluetooth_le_descriptor_get_value(descriptor, out_data, offset, max_size);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_descriptor_write(char* __arg_buffer, double __arg_buffer_length)
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
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_advertise_start(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: settings_json, type: String
    std::string_view settings_json = gm::wire::codec::readValue<std::string_view>(__br);

    // field: data_json, type: String
    std::string_view data_json = gm::wire::codec::readValue<std::string_view>(__br);

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    auto&& __result = bluetooth_le_advertise_start(settings_json, data_json, callback);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_advertise_stop()
{
    auto&& __result = bluetooth_le_advertise_stop();
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_advertise_is_running()
{
    auto&& __result = bluetooth_le_advertise_is_running();
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_server_start()
{
    auto&& __result = bluetooth_le_server_start();
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_server_stop()
{
    auto&& __result = bluetooth_le_server_stop();
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_server_is_running()
{
    auto&& __result = bluetooth_le_server_is_running();
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_server_add_service(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: service_json, type: String
    std::string_view service_json = gm::wire::codec::readValue<std::string_view>(__br);

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    auto&& __result = bluetooth_le_server_add_service(service_json, callback);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_server_clear_services()
{
    auto&& __result = bluetooth_le_server_clear_services();
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_server_respond_read(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: request_id, type: Int32
    std::int32_t request_id = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: error_code, type: Int32
    std::int32_t error_code = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: data, type: Buffer
    gm::wire::GMBuffer data = __buffer_queue.front();
    __buffer_queue.pop();

    // field: offset, type: UInt32
    std::uint32_t offset = gm::wire::codec::readValue<std::uint32_t>(__br);

    // field: size, type: UInt32
    std::uint32_t size = gm::wire::codec::readValue<std::uint32_t>(__br);

    auto&& __result = bluetooth_le_server_respond_read(request_id, error_code, data, offset, size);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_server_respond_write(double request_id, double error_code)
{
    auto&& __result = bluetooth_le_server_respond_write(static_cast<std::int32_t>(request_id), static_cast<std::int32_t>(error_code));
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_server_write_request_get_value(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: request_id, type: Int32
    std::int32_t request_id = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: out_data, type: Buffer
    gm::wire::GMBuffer out_data = __buffer_queue.front();
    __buffer_queue.pop();

    // field: offset, type: UInt32
    std::uint32_t offset = gm::wire::codec::readValue<std::uint32_t>(__br);

    // field: max_size, type: UInt32
    std::uint32_t max_size = gm::wire::codec::readValue<std::uint32_t>(__br);

    auto&& __result = bluetooth_le_server_write_request_get_value(request_id, out_data, offset, max_size);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_le_server_notify_value(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: service_uuid, type: String
    std::string_view service_uuid = gm::wire::codec::readValue<std::string_view>(__br);

    // field: characteristic_uuid, type: String
    std::string_view characteristic_uuid = gm::wire::codec::readValue<std::string_view>(__br);

    // field: connection, type: UInt64
    std::uint64_t connection = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: data, type: Buffer
    gm::wire::GMBuffer data = __buffer_queue.front();
    __buffer_queue.pop();

    // field: offset, type: UInt32
    std::uint32_t offset = gm::wire::codec::readValue<std::uint32_t>(__br);

    // field: size, type: UInt32
    std::uint32_t size = gm::wire::codec::readValue<std::uint32_t>(__br);

    auto&& __result = bluetooth_le_server_notify_value(service_uuid, characteristic_uuid, connection, data, offset, size);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_set_callback_device_found(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    auto&& __result = bluetooth_set_callback_device_found(callback);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_remove_callback_device_found()
{
    auto&& __result = bluetooth_remove_callback_device_found();
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_set_callback_scan_stopped(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    auto&& __result = bluetooth_set_callback_scan_stopped(callback);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_remove_callback_scan_stopped()
{
    auto&& __result = bluetooth_remove_callback_scan_stopped();
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_set_callback_classic_client_connected(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    auto&& __result = bluetooth_set_callback_classic_client_connected(callback);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_remove_callback_classic_client_connected()
{
    auto&& __result = bluetooth_remove_callback_classic_client_connected();
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_set_callback_classic_data(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    auto&& __result = bluetooth_set_callback_classic_data(callback);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_remove_callback_classic_data()
{
    auto&& __result = bluetooth_remove_callback_classic_data();
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_set_callback_classic_disconnected(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    auto&& __result = bluetooth_set_callback_classic_disconnected(callback);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_remove_callback_classic_disconnected()
{
    auto&& __result = bluetooth_remove_callback_classic_disconnected();
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_set_callback_le_disconnected(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    auto&& __result = bluetooth_set_callback_le_disconnected(callback);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_remove_callback_le_disconnected()
{
    auto&& __result = bluetooth_remove_callback_le_disconnected();
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_set_callback_le_characteristic_value_changed(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    auto&& __result = bluetooth_set_callback_le_characteristic_value_changed(callback);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_remove_callback_le_characteristic_value_changed()
{
    auto&& __result = bluetooth_remove_callback_le_characteristic_value_changed();
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_set_callback_le_server_connection_state_changed(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    auto&& __result = bluetooth_set_callback_le_server_connection_state_changed(callback);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_remove_callback_le_server_connection_state_changed()
{
    auto&& __result = bluetooth_remove_callback_le_server_connection_state_changed();
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_set_callback_le_server_read_request(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    auto&& __result = bluetooth_set_callback_le_server_read_request(callback);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_remove_callback_le_server_read_request()
{
    auto&& __result = bluetooth_remove_callback_le_server_read_request();
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_set_callback_le_server_write_request(char* __arg_buffer, double __arg_buffer_length)
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    auto&& __result = bluetooth_set_callback_le_server_write_request(callback);
    return static_cast<double>(__result);
}

GMEXPORT double __EXT_NATIVE__bluetooth_remove_callback_le_server_write_request()
{
    auto&& __result = bluetooth_remove_callback_le_server_write_request();
    return static_cast<double>(__result);
}

