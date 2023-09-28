

// CORE

/** 
 * @func bt_is_enabled
 * @desc Returns true if the bluetooth module is enabled, false otherwise
 * @returns {boolean}
 * @func_end
 */
function bt_is_enabled() {}

/** 
 * @func bt_request_enable
 * @desc Show a system activity that allows the user to turn on Bluetooth
 * 
 * @event social
 * @member {string} type The value `"bt_request_enable"`
 * @member {boolean} success true if user accepted, false if not
 * @event_end
 * 
 * @func_end
 */
function bt_request_enable() {}

/** 
 * @func bt_get_name
 * @desc Get the friendly Bluetooth name of the local Bluetooth adapter.
 * @returns {string}
 * @func_end
 */
function bt_get_name() {}

/** 
 * @func bt_get_address
 * @desc Returns the hardware address of the local Bluetooth adapter.
 * @returns {string}
 * @func_end
 */
function bt_get_address() {}

/** 
 * @func bt_paired_devices
 * @desc Return an array of ${struct.BluetoothDevice} objects that are bonded (paired) to the local adapter.
 * @returns {string}
 * @func_end
 */
function bt_paired_devices() {}

/**
 * @module bt_core
 * @title Bluetooth Core
 * @desc This module presents a collection of core functions common to both Bluetooth Classic and Bluetooth Low Energy.
 * 
 * @section_func
 * @ref bt_is_enabled
 * @ref bt_request_enable
 * @ref bt_get_name
 * @ref bt_get_address
 * @ref bt_paired_devices
 * @section_end
 * 
 * @module_end
 */

