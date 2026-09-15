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

    enum class BluetoothLeSubscribeMode : std::int32_t
    {
        Unsubscribe = 0,
        Notify = 1,
        Indicate = 2
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
- (std::int32_t)bluetooth_classic_discoverable_start:(std::int32_t)duration_seconds;
- (std::int32_t)bluetooth_classic_discoverable_stop;
- (bool)bluetooth_classic_discoverable_is_running;
- (std::uint64_t)bluetooth_le_connect:(std::uint64_t)device callback:(gm::wire::GMFunction)callback;
- (std::int32_t)bluetooth_le_disconnect:(std::uint64_t)connection;
- (bool)bluetooth_le_connection_is_valid:(std::uint64_t)connection;
- (bool)bluetooth_le_connection_is_connected:(std::uint64_t)connection;
- (std::uint64_t)bluetooth_le_connection_get_device:(std::uint64_t)connection;
- (std::int32_t)bluetooth_le_services_discover:(std::uint64_t)connection callback:(gm::wire::GMFunction)callback;
- (std::int32_t)bluetooth_le_service_get_count:(std::uint64_t)connection;
- (std::uint64_t)bluetooth_le_service_get_at:(std::uint64_t)connection index:(std::int32_t)index;
- (std::string)bluetooth_le_service_get_uuid:(std::uint64_t)service;
- (std::int32_t)bluetooth_le_characteristics_discover:(std::uint64_t)service callback:(gm::wire::GMFunction)callback;
- (std::int32_t)bluetooth_le_characteristic_get_count:(std::uint64_t)service;
- (std::uint64_t)bluetooth_le_characteristic_get_at:(std::uint64_t)service index:(std::int32_t)index;
- (std::string)bluetooth_le_characteristic_get_uuid:(std::uint64_t)characteristic;
- (std::int32_t)bluetooth_le_characteristic_get_properties:(std::uint64_t)characteristic;
- (std::int32_t)bluetooth_le_descriptors_discover:(std::uint64_t)characteristic callback:(gm::wire::GMFunction)callback;
- (std::int32_t)bluetooth_le_descriptor_get_count:(std::uint64_t)characteristic;
- (std::uint64_t)bluetooth_le_descriptor_get_at:(std::uint64_t)characteristic index:(std::int32_t)index;
- (std::string)bluetooth_le_descriptor_get_uuid:(std::uint64_t)descriptor;
- (std::int32_t)bluetooth_le_characteristic_read:(std::uint64_t)characteristic callback:(gm::wire::GMFunction)callback;
- (std::int32_t)bluetooth_le_characteristic_get_value:(std::uint64_t)characteristic out_data:(gm::wire::GMBuffer)out_data offset:(std::uint32_t)offset max_size:(std::uint32_t)max_size;
- (std::int32_t)bluetooth_le_characteristic_write:(std::uint64_t)characteristic data:(gm::wire::GMBuffer)data offset:(std::uint32_t)offset size:(std::uint32_t)size write_type:(std::int32_t)write_type callback:(gm::wire::GMFunction)callback;
- (std::int32_t)bluetooth_le_characteristic_subscribe:(std::uint64_t)characteristic mode:(std::int32_t)mode callback:(gm::wire::GMFunction)callback;
- (std::int32_t)bluetooth_le_descriptor_read:(std::uint64_t)descriptor callback:(gm::wire::GMFunction)callback;
- (std::int32_t)bluetooth_le_descriptor_get_value:(std::uint64_t)descriptor out_data:(gm::wire::GMBuffer)out_data offset:(std::uint32_t)offset max_size:(std::uint32_t)max_size;
- (std::int32_t)bluetooth_le_descriptor_write:(std::uint64_t)descriptor data:(gm::wire::GMBuffer)data offset:(std::uint32_t)offset size:(std::uint32_t)size callback:(gm::wire::GMFunction)callback;
- (std::int32_t)bluetooth_le_advertise_start:(std::string_view)settings_json data_json:(std::string_view)data_json callback:(gm::wire::GMFunction)callback;
- (std::int32_t)bluetooth_le_advertise_stop;
- (bool)bluetooth_le_advertise_is_running;
- (std::int32_t)bluetooth_le_server_start;
- (std::int32_t)bluetooth_le_server_stop;
- (bool)bluetooth_le_server_is_running;
- (std::int32_t)bluetooth_le_server_add_service:(std::string_view)service_json callback:(gm::wire::GMFunction)callback;
- (std::int32_t)bluetooth_le_server_clear_services;
- (std::int32_t)bluetooth_le_server_respond_read:(std::int32_t)request_id error_code:(std::int32_t)error_code data:(gm::wire::GMBuffer)data offset:(std::uint32_t)offset size:(std::uint32_t)size;
- (std::int32_t)bluetooth_le_server_respond_write:(std::int32_t)request_id error_code:(std::int32_t)error_code;
- (std::int32_t)bluetooth_le_server_write_request_get_value:(std::int32_t)request_id out_data:(gm::wire::GMBuffer)out_data offset:(std::uint32_t)offset max_size:(std::uint32_t)max_size;
- (std::int32_t)bluetooth_le_server_notify_value:(std::string_view)service_uuid characteristic_uuid:(std::string_view)characteristic_uuid connection:(std::uint64_t)connection data:(gm::wire::GMBuffer)data offset:(std::uint32_t)offset size:(std::uint32_t)size;
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
- (bool)bluetooth_set_callback_le_disconnected:(gm::wire::GMFunction)callback;
- (bool)bluetooth_remove_callback_le_disconnected;
- (bool)bluetooth_set_callback_le_characteristic_value_changed:(gm::wire::GMFunction)callback;
- (bool)bluetooth_remove_callback_le_characteristic_value_changed;
- (bool)bluetooth_set_callback_le_server_connection_state_changed:(gm::wire::GMFunction)callback;
- (bool)bluetooth_remove_callback_le_server_connection_state_changed;
- (bool)bluetooth_set_callback_le_server_read_request:(gm::wire::GMFunction)callback;
- (bool)bluetooth_remove_callback_le_server_read_request;
- (bool)bluetooth_set_callback_le_server_write_request:(gm::wire::GMFunction)callback;
- (bool)bluetooth_remove_callback_le_server_write_request;
@end


@interface GMBluetoothInternal : NSObject
- (double)__EXT_NATIVE__bluetooth_initialize;
- (double)__EXT_NATIVE__bluetooth_shutdown;
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
- (double)__EXT_NATIVE__bluetooth_classic_discoverable_start:(double)duration_seconds;
- (double)__EXT_NATIVE__bluetooth_classic_discoverable_stop;
- (double)__EXT_NATIVE__bluetooth_classic_discoverable_is_running;
- (double)__EXT_NATIVE__bluetooth_le_connect:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_disconnect:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_connection_is_valid:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_connection_is_connected:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_connection_get_device:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_services_discover:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_service_get_count:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_service_get_at:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length;
- (char*)__EXT_NATIVE__bluetooth_le_service_get_uuid:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_characteristics_discover:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_characteristic_get_count:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_characteristic_get_at:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length;
- (char*)__EXT_NATIVE__bluetooth_le_characteristic_get_uuid:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_characteristic_get_properties:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_descriptors_discover:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_descriptor_get_count:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_descriptor_get_at:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length;
- (char*)__EXT_NATIVE__bluetooth_le_descriptor_get_uuid:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_characteristic_read:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_characteristic_get_value:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_characteristic_write:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_characteristic_subscribe:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_descriptor_read:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_descriptor_get_value:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_descriptor_write:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_advertise_start:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_advertise_stop;
- (double)__EXT_NATIVE__bluetooth_le_advertise_is_running;
- (double)__EXT_NATIVE__bluetooth_le_server_start;
- (double)__EXT_NATIVE__bluetooth_le_server_stop;
- (double)__EXT_NATIVE__bluetooth_le_server_is_running;
- (double)__EXT_NATIVE__bluetooth_le_server_add_service:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_server_clear_services;
- (double)__EXT_NATIVE__bluetooth_le_server_respond_read:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_server_respond_write:(double)request_id arg1:(double)error_code;
- (double)__EXT_NATIVE__bluetooth_le_server_write_request_get_value:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_server_notify_value:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
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
- (double)__EXT_NATIVE__bluetooth_set_callback_le_disconnected:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_remove_callback_le_disconnected;
- (double)__EXT_NATIVE__bluetooth_set_callback_le_characteristic_value_changed:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_remove_callback_le_characteristic_value_changed;
- (double)__EXT_NATIVE__bluetooth_set_callback_le_server_connection_state_changed:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_remove_callback_le_server_connection_state_changed;
- (double)__EXT_NATIVE__bluetooth_set_callback_le_server_read_request:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_remove_callback_le_server_read_request;
- (double)__EXT_NATIVE__bluetooth_set_callback_le_server_write_request:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_remove_callback_le_server_write_request;
- (double)__EXT_NATIVE__GMBluetooth_invocation_handler:(char*)__ret_buffer arg1:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__GMBluetooth_queue_buffer:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
@end


