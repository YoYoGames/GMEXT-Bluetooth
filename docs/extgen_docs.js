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
 * @function_partial bluetooth_le_advertise_is_supported
 * @returns {Bool}
 * @function_end
 */

/**
 * @function_partial bluetooth_le_server_is_supported
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
 * @function_partial bluetooth_classic_discoverable_start
 * @param {Real} duration_seconds
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_classic_discoverable_stop
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_classic_discoverable_is_running
 * @returns {Bool}
 * @function_end
 */

/**
 * @function_partial bluetooth_pairing_is_supported
 * @param {Real} device
 * @returns {Bool}
 * @function_end
 */

/**
 * @function_partial bluetooth_pair
 * @param {Real} device
 * @param {Function} callback
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_device_is_paired
 * @param {Real} device
 * @returns {Bool}
 * @function_end
 */

/**
 * @function_partial bluetooth_le_connect
 * @param {Real} device
 * @param {Function} callback
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_le_disconnect
 * @param {Real} connection
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_le_connection_is_valid
 * @param {Real} connection
 * @returns {Bool}
 * @function_end
 */

/**
 * @function_partial bluetooth_le_connection_is_connected
 * @param {Real} connection
 * @returns {Bool}
 * @function_end
 */

/**
 * @function_partial bluetooth_le_connection_get_device
 * @param {Real} connection
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_le_services_discover
 * @param {Real} connection
 * @param {Function} callback
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_le_service_get_count
 * @param {Real} connection
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_le_service_get_at
 * @param {Real} connection
 * @param {Real} index
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_le_service_get_uuid
 * @param {Real} service
 * @returns {String}
 * @function_end
 */

/**
 * @function_partial bluetooth_le_characteristics_discover
 * @param {Real} service
 * @param {Function} callback
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_le_characteristic_get_count
 * @param {Real} service
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_le_characteristic_get_at
 * @param {Real} service
 * @param {Real} index
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_le_characteristic_get_uuid
 * @param {Real} characteristic
 * @returns {String}
 * @function_end
 */

/**
 * @function_partial bluetooth_le_characteristic_get_properties
 * @param {Real} characteristic
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_le_descriptors_discover
 * @param {Real} characteristic
 * @param {Function} callback
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_le_descriptor_get_count
 * @param {Real} characteristic
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_le_descriptor_get_at
 * @param {Real} characteristic
 * @param {Real} index
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_le_descriptor_get_uuid
 * @param {Real} descriptor
 * @returns {String}
 * @function_end
 */

/**
 * @function_partial bluetooth_le_characteristic_read
 * @param {Real} characteristic
 * @param {Function} callback
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_le_characteristic_get_value
 * @param {Real} characteristic
 * @param {Buffer} out_data
 * @param {Real} offset
 * @param {Real} max_size
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_le_characteristic_write
 * @param {Real} characteristic
 * @param {Buffer} data
 * @param {Real} offset
 * @param {Real} size
 * @param {Real} write_type
 * @param {Function} callback
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_le_characteristic_subscribe
 * @param {Real} characteristic
 * @param {Real} mode
 * @param {Function} callback
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_le_descriptor_read
 * @param {Real} descriptor
 * @param {Function} callback
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_le_descriptor_get_value
 * @param {Real} descriptor
 * @param {Buffer} out_data
 * @param {Real} offset
 * @param {Real} max_size
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_le_descriptor_write
 * @param {Real} descriptor
 * @param {Buffer} data
 * @param {Real} offset
 * @param {Real} size
 * @param {Function} callback
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_le_advertise_start
 * @param {String} settings_json
 * @param {String} data_json
 * @param {Function} callback
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_le_advertise_stop
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_le_advertise_is_running
 * @returns {Bool}
 * @function_end
 */

/**
 * @function_partial bluetooth_le_server_start
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_le_server_stop
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_le_server_is_running
 * @returns {Bool}
 * @function_end
 */

/**
 * @function_partial bluetooth_le_server_add_service
 * @param {Struct.BluetoothLeServiceDefinition} service
 * @param {Function} callback
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_le_server_clear_services
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_le_server_respond_read
 * @param {Real} request_id
 * @param {Real} error_code
 * @param {Buffer} data
 * @param {Real} offset
 * @param {Real} size
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_le_server_respond_write
 * @param {Real} request_id
 * @param {Real} error_code
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_le_server_write_request_get_value
 * @param {Real} request_id
 * @param {Buffer} out_data
 * @param {Real} offset
 * @param {Real} max_size
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_le_server_notify_value
 * @param {String} service_uuid
 * @param {String} characteristic_uuid
 * @param {Real} connection
 * @param {Buffer} data
 * @param {Real} offset
 * @param {Real} size
 * @returns {Real}
 * @function_end
 */

/**
 * @function_partial bluetooth_set_callback_state_changed
 * @param {Function} callback
 * @returns {Bool}
 * @function_end
 */

/**
 * @function_partial bluetooth_remove_callback_state_changed
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
 * @function_partial bluetooth_set_callback_le_disconnected
 * @param {Function} callback
 * @returns {Bool}
 * @function_end
 */

/**
 * @function_partial bluetooth_remove_callback_le_disconnected
 * @returns {Bool}
 * @function_end
 */

/**
 * @function_partial bluetooth_set_callback_le_characteristic_value_changed
 * @param {Function} callback
 * @returns {Bool}
 * @function_end
 */

/**
 * @function_partial bluetooth_remove_callback_le_characteristic_value_changed
 * @returns {Bool}
 * @function_end
 */

/**
 * @function_partial bluetooth_set_callback_le_server_connection_state_changed
 * @param {Function} callback
 * @returns {Bool}
 * @function_end
 */

/**
 * @function_partial bluetooth_remove_callback_le_server_connection_state_changed
 * @returns {Bool}
 * @function_end
 */

/**
 * @function_partial bluetooth_set_callback_le_server_read_request
 * @param {Function} callback
 * @returns {Bool}
 * @function_end
 */

/**
 * @function_partial bluetooth_remove_callback_le_server_read_request
 * @returns {Bool}
 * @function_end
 */

/**
 * @function_partial bluetooth_set_callback_le_server_write_request
 * @param {Function} callback
 * @returns {Bool}
 * @function_end
 */

/**
 * @function_partial bluetooth_remove_callback_le_server_write_request
 * @returns {Bool}
 * @function_end
 */

/**
 * @struct_partial BluetoothLeDescriptorDefinition
 * @member {String} uuid
 * @struct_end
 */

/**
 * @struct_partial BluetoothLeCharacteristicDefinition
 * @member {String} uuid
 * @member {Real} properties
 * @member {Real} permissions
 * @member {String} [value]
 * @member {Array[Struct.BluetoothLeDescriptorDefinition]} descriptors
 * @struct_end
 */

/**
 * @struct_partial BluetoothLeServiceDefinition
 * @member {String} uuid
 * @member {Array[Struct.BluetoothLeCharacteristicDefinition]} characteristics
 * @struct_end
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
 * @enum_partial BluetoothLeSubscribeMode
 * @member Unsubscribe
 * @member Notify
 * @member Indicate
 * @enum_end
 */

/**
 * @enum_partial BluetoothLeCharacteristicProperty
 * @member None
 * @member Broadcast
 * @member Read
 * @member WriteWithoutResponse
 * @member Write
 * @member Notify
 * @member Indicate
 * @member AuthenticatedSignedWrites
 * @member ExtendedProperties
 * @enum_end
 */

/**
 * @enum_partial BluetoothState
 * @member Unknown
 * @member Resetting
 * @member Unsupported
 * @member Unauthorized
 * @member PoweredOff
 * @member PoweredOn
 * @enum_end
 */

/**
 * @const_partial macros
 * @const_end
 */

