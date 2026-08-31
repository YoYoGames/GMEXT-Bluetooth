/**
 * @function_partial bluetooth_initialize
 * @returns {Bool}
 * @function_end
 */

/**
 * @function_partial bluetooth_shutdown
 * @function_end
 */

/**
 * @function_partial bluetooth_update
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_is_initialized
 * @returns {Bool}
 * @function_end
 */

/**
 * @function_partial bluetooth_last_error_code
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_last_error_message
 * @returns {String}
 * @function_end
 */

/**
 * @function_partial bluetooth_le_is_supported
 * @returns {Bool}
 * @function_end
 */

/**
 * @function_partial bluetooth_classic_is_supported
 * @returns {Bool}
 * @function_end
 */

/**
 * @function_partial bluetooth_le_scan_start
 * @param {Bool} active
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_le_scan_stop
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_le_scan_is_running
 * @returns {Bool}
 * @function_end
 */

/**
 * @function_partial bluetooth_device_clear
 * @function_end
 */

/**
 * @function_partial bluetooth_device_get_count
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_device_get_at
 * @param {Real} index
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_device_is_valid
 * @param {Real} device
 * @returns {Bool}
 * @function_end
 */

/**
 * @function_partial bluetooth_device_get_transport
 * @param {Real} device
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_device_get_id
 * @param {Real} device
 * @returns {String}
 * @function_end
 */

/**
 * @function_partial bluetooth_device_get_name
 * @param {Real} device
 * @returns {String}
 * @function_end
 */

/**
 * @function_partial bluetooth_device_has_address
 * @param {Real} device
 * @returns {Bool}
 * @function_end
 */

/**
 * @function_partial bluetooth_device_get_address
 * @param {Real} device
 * @returns {String}
 * @function_end
 */

/**
 * @function_partial bluetooth_device_has_rssi
 * @param {Real} device
 * @returns {Bool}
 * @function_end
 */

/**
 * @function_partial bluetooth_device_get_rssi
 * @param {Real} device
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_device_is_connectable
 * @param {Real} device
 * @returns {Bool}
 * @function_end
 */

/**
 * @function_partial bluetooth_set_callback_device_found
 * @param {Function} callback
 * @returns {Bool}
 * @function_end
 */

/**
 * @function_partial bluetooth_remove_callback_device_found
 * @returns {Bool}
 * @function_end
 */

/**
 * @function_partial bluetooth_set_callback_scan_stopped
 * @param {Function} callback
 * @returns {Bool}
 * @function_end
 */

/**
 * @function_partial bluetooth_remove_callback_scan_stopped
 * @returns {Bool}
 * @function_end
 */

/**
 * @enum_partial BluetoothError
 * @member Ok
 * @member Unknown
 * @member NotSupported
 * @member NotInitialized
 * @member BluetoothDisabled
 * @member PermissionDenied
 * @member InvalidArgument
 * @member InvalidHandle
 * @member Busy
 * @member Timeout
 * @member NotFound
 * @member ConnectionFailed
 * @member Disconnected
 * @member OperationFailed
 * @enum_end
 */

/**
 * @enum_partial BluetoothTransport
 * @member Unknown
 * @member Classic
 * @member LowEnergy
 * @enum_end
 */

/**
 * @const_partial macros
 * @const_end
 */

