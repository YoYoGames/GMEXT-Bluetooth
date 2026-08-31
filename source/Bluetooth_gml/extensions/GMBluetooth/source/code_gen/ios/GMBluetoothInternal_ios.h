// ##### extgen :: Auto-generated file do not edit!! #####

#import <Foundation/Foundation.h>

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

