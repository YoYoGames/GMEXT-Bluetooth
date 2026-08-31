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

GMEXPORT double __EXT_NATIVE__bluetooth_update()
{
    auto&& __result = bluetooth_update();
    return static_cast<double>(__result);
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

