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
 * @function_partial bluetooth_classic_server_is_supported
 * @returns {Bool}
 * @function_end
 */

/**
 * @function_partial bluetooth_permission_get_status
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_permission_request
 * @returns {Real}
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
 * @function_partial bluetooth_classic_scan_start
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_classic_scan_stop
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_classic_scan_is_running
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
 * @function_partial bluetooth_classic_connect
 * @param {Real} device
 * @param {String} service_uuid
 * @param {Function} callback
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_classic_disconnect
 * @param {Real} connection
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_classic_connection_is_valid
 * @param {Real} connection
 * @returns {Bool}
 * @function_end
 */

/**
 * @function_partial bluetooth_classic_connection_is_connected
 * @param {Real} connection
 * @returns {Bool}
 * @function_end
 */

/**
 * @function_partial bluetooth_classic_connection_get_device
 * @param {Real} connection
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_classic_receive_available
 * @param {Real} connection
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_classic_send
 * @param {Real} connection
 * @param {Buffer} data
 * @param {Real} offset
 * @param {Real} size
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_classic_receive
 * @param {Real} connection
 * @param {Buffer} out_data
 * @param {Real} offset
 * @param {Real} max_size
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_classic_server_start
 * @param {String} name
 * @param {String} service_uuid
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_classic_server_stop
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_classic_server_is_running
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
 * @function_partial bluetooth_set_callback_classic_client_connected
 * @param {Function} callback
 * @returns {Bool}
 * @function_end
 */

/**
 * @function_partial bluetooth_remove_callback_classic_client_connected
 * @returns {Bool}
 * @function_end
 */

/**
 * @function_partial bluetooth_set_callback_classic_data
 * @param {Function} callback
 * @returns {Bool}
 * @function_end
 */

/**
 * @function_partial bluetooth_remove_callback_classic_data
 * @returns {Bool}
 * @function_end
 */

/**
 * @function_partial bluetooth_set_callback_classic_disconnected
 * @param {Function} callback
 * @returns {Bool}
 * @function_end
 */

/**
 * @function_partial bluetooth_remove_callback_classic_disconnected
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
 * @enum_partial BluetoothPermissionStatus
 * @member Unknown
 * @member Granted
 * @member Denied
 * @enum_end
 */

/**
 * @const_partial macros
 * @const_end
 */

