// ##### extgen :: Auto-generated file do not edit!! #####

#pragma once
#import <Foundation/Foundation.h>

#include <cstdint>
#include <string_view>
#include <vector>
#include <array>
#include <optional>
#include "core/GMExtWire.h"

namespace gm_consts
{
}


namespace gm_enums
{
    enum class BluetoothError : std::int32_t
    {
        Ok = 0,
        Unknown = 1,
        NotSupported = 2,
        NotInitialized = 3,
        BluetoothDisabled = 4,
        PermissionDenied = 5,
        InvalidArgument = 6,
        InvalidHandle = 7,
        Busy = 8,
        Timeout = 9,
        NotFound = 10,
        ConnectionFailed = 11,
        Disconnected = 12,
        OperationFailed = 13
    };

    enum class BluetoothTransport : std::int32_t
    {
        Unknown = 0,
        Classic = 1,
        LowEnergy = 2
    };

    enum class BluetoothPermissionStatus : std::int32_t
    {
        Unknown = 0,
        Granted = 1,
        Denied = 2
    };

}


namespace gm_structs
{

}

namespace gm::wire::codec
{
}

namespace gm::wire::details
{
}

@protocol GMBluetoothInterface <NSObject>
- (bool)bluetooth_initialize;
- (void)bluetooth_shutdown;
- (std::int32_t)bluetooth_update;
- (bool)bluetooth_is_initialized;
- (std::int32_t)bluetooth_last_error_code;
- (std::string)bluetooth_last_error_message;
- (bool)bluetooth_le_is_supported;
- (bool)bluetooth_classic_is_supported;
- (bool)bluetooth_classic_server_is_supported;
- (std::int32_t)bluetooth_permission_get_status;
- (std::int32_t)bluetooth_permission_request;
- (std::int32_t)bluetooth_le_scan_start:(bool)active;
- (std::int32_t)bluetooth_le_scan_stop;
- (bool)bluetooth_le_scan_is_running;
- (std::int32_t)bluetooth_classic_scan_start;
- (std::int32_t)bluetooth_classic_scan_stop;
- (bool)bluetooth_classic_scan_is_running;
- (void)bluetooth_device_clear;
- (std::int32_t)bluetooth_device_get_count;
- (std::uint64_t)bluetooth_device_get_at:(std::int32_t)index;
- (bool)bluetooth_device_is_valid:(std::uint64_t)device;
- (std::int32_t)bluetooth_device_get_transport:(std::uint64_t)device;
- (std::string)bluetooth_device_get_id:(std::uint64_t)device;
- (std::string)bluetooth_device_get_name:(std::uint64_t)device;
- (bool)bluetooth_device_has_address:(std::uint64_t)device;
- (std::string)bluetooth_device_get_address:(std::uint64_t)device;
- (bool)bluetooth_device_has_rssi:(std::uint64_t)device;
- (std::int32_t)bluetooth_device_get_rssi:(std::uint64_t)device;
- (bool)bluetooth_device_is_connectable:(std::uint64_t)device;
- (std::uint64_t)bluetooth_classic_connect:(std::uint64_t)device service_uuid:(std::string_view)service_uuid callback:(gm::wire::GMFunction)callback;
- (std::int32_t)bluetooth_classic_disconnect:(std::uint64_t)connection;
- (bool)bluetooth_classic_connection_is_valid:(std::uint64_t)connection;
- (bool)bluetooth_classic_connection_is_connected:(std::uint64_t)connection;
- (std::uint64_t)bluetooth_classic_connection_get_device:(std::uint64_t)connection;
- (std::int32_t)bluetooth_classic_receive_available:(std::uint64_t)connection;
- (std::int32_t)bluetooth_classic_send:(std::uint64_t)connection data:(gm::wire::GMBuffer)data offset:(std::uint32_t)offset size:(std::uint32_t)size;
- (std::int32_t)bluetooth_classic_receive:(std::uint64_t)connection out_data:(gm::wire::GMBuffer)out_data offset:(std::uint32_t)offset max_size:(std::uint32_t)max_size;
- (std::int32_t)bluetooth_classic_server_start:(std::string_view)name service_uuid:(std::string_view)service_uuid;
- (std::int32_t)bluetooth_classic_server_stop;
- (bool)bluetooth_classic_server_is_running;
- (bool)bluetooth_set_callback_device_found:(gm::wire::GMFunction)callback;
- (bool)bluetooth_remove_callback_device_found;
- (bool)bluetooth_set_callback_scan_stopped:(gm::wire::GMFunction)callback;
- (bool)bluetooth_remove_callback_scan_stopped;
- (bool)bluetooth_set_callback_classic_client_connected:(gm::wire::GMFunction)callback;
- (bool)bluetooth_remove_callback_classic_client_connected;
- (bool)bluetooth_set_callback_classic_data:(gm::wire::GMFunction)callback;
- (bool)bluetooth_remove_callback_classic_data;
- (bool)bluetooth_set_callback_classic_disconnected:(gm::wire::GMFunction)callback;
- (bool)bluetooth_remove_callback_classic_disconnected;
@end


@interface GMBluetoothInternal : NSObject
- (double)__EXT_NATIVE__bluetooth_initialize;
- (double)__EXT_NATIVE__bluetooth_shutdown;
- (double)__EXT_NATIVE__bluetooth_update;
- (double)__EXT_NATIVE__bluetooth_is_initialized;
- (double)__EXT_NATIVE__bluetooth_last_error_code;
- (char*)__EXT_NATIVE__bluetooth_last_error_message;
- (double)__EXT_NATIVE__bluetooth_le_is_supported;
- (double)__EXT_NATIVE__bluetooth_classic_is_supported;
- (double)__EXT_NATIVE__bluetooth_classic_server_is_supported;
- (double)__EXT_NATIVE__bluetooth_permission_get_status;
- (double)__EXT_NATIVE__bluetooth_permission_request;
- (double)__EXT_NATIVE__bluetooth_le_scan_start:(double)active;
- (double)__EXT_NATIVE__bluetooth_le_scan_stop;
- (double)__EXT_NATIVE__bluetooth_le_scan_is_running;
- (double)__EXT_NATIVE__bluetooth_classic_scan_start;
- (double)__EXT_NATIVE__bluetooth_classic_scan_stop;
- (double)__EXT_NATIVE__bluetooth_classic_scan_is_running;
- (double)__EXT_NATIVE__bluetooth_device_clear;
- (double)__EXT_NATIVE__bluetooth_device_get_count;
- (double)__EXT_NATIVE__bluetooth_device_get_at:(double)index arg1:(char*)__ret_buffer arg2:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__bluetooth_device_is_valid:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_device_get_transport:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (char*)__EXT_NATIVE__bluetooth_device_get_id:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (char*)__EXT_NATIVE__bluetooth_device_get_name:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_device_has_address:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (char*)__EXT_NATIVE__bluetooth_device_get_address:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_device_has_rssi:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_device_get_rssi:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_device_is_connectable:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_classic_connect:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__bluetooth_classic_disconnect:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_classic_connection_is_valid:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_classic_connection_is_connected:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_classic_connection_get_device:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__bluetooth_classic_receive_available:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_classic_send:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_classic_receive:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_classic_server_start:(char*)name arg1:(char*)service_uuid;
- (double)__EXT_NATIVE__bluetooth_classic_server_stop;
- (double)__EXT_NATIVE__bluetooth_classic_server_is_running;
- (double)__EXT_NATIVE__bluetooth_set_callback_device_found:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_remove_callback_device_found;
- (double)__EXT_NATIVE__bluetooth_set_callback_scan_stopped:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_remove_callback_scan_stopped;
- (double)__EXT_NATIVE__bluetooth_set_callback_classic_client_connected:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_remove_callback_classic_client_connected;
- (double)__EXT_NATIVE__bluetooth_set_callback_classic_data:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_remove_callback_classic_data;
- (double)__EXT_NATIVE__bluetooth_set_callback_classic_disconnected:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_remove_callback_classic_disconnected;
- (double)__EXT_NATIVE__GMBluetooth_invocation_handler:(char*)__ret_buffer arg1:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__GMBluetooth_queue_buffer:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
@end


