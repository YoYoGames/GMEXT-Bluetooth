// ##### extgen :: Auto-generated file do not edit!! #####

#pragma once
#include "core/GMExtUtils.h"

// Internal function used for fetching dispatched function calls to GML
GMEXPORT double __EXT_NATIVE__GMBluetooth_invocation_handler(char* __ret_buffer, double __ret_buffer_length);

GMEXPORT double __EXT_NATIVE__bluetooth_initialize();
GMEXPORT double __EXT_NATIVE__bluetooth_shutdown();
GMEXPORT double __EXT_NATIVE__bluetooth_update();
GMEXPORT double __EXT_NATIVE__bluetooth_is_initialized();
GMEXPORT double __EXT_NATIVE__bluetooth_last_error_code();
GMEXPORT char* __EXT_NATIVE__bluetooth_last_error_message();
GMEXPORT double __EXT_NATIVE__bluetooth_le_is_supported();
GMEXPORT double __EXT_NATIVE__bluetooth_classic_is_supported();
GMEXPORT double __EXT_NATIVE__bluetooth_le_scan_start(double active);
GMEXPORT double __EXT_NATIVE__bluetooth_le_scan_stop();
GMEXPORT double __EXT_NATIVE__bluetooth_le_scan_is_running();
GMEXPORT double __EXT_NATIVE__bluetooth_device_clear();
GMEXPORT double __EXT_NATIVE__bluetooth_device_get_count();
GMEXPORT double __EXT_NATIVE__bluetooth_device_get_at(double index, char* __ret_buffer, double __ret_buffer_length);
GMEXPORT double __EXT_NATIVE__bluetooth_device_is_valid(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__bluetooth_device_get_transport(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT char* __EXT_NATIVE__bluetooth_device_get_id(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT char* __EXT_NATIVE__bluetooth_device_get_name(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__bluetooth_device_has_address(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT char* __EXT_NATIVE__bluetooth_device_get_address(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__bluetooth_device_has_rssi(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__bluetooth_device_get_rssi(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__bluetooth_device_is_connectable(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__bluetooth_set_callback_device_found(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__bluetooth_remove_callback_device_found();
GMEXPORT double __EXT_NATIVE__bluetooth_set_callback_scan_stopped(char* __arg_buffer, double __arg_buffer_length);
GMEXPORT double __EXT_NATIVE__bluetooth_remove_callback_scan_stopped();

