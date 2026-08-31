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

