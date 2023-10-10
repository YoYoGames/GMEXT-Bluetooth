
// LOW ENERGY

/** 
 * @func bt_le_is_supported
 * @desc This function returns `true` if the device has the necessary Bluetooth LE features, `false` otherwise.
 * @returns {boolean}
 * 
 * @func_end
 */
function bt_le_is_supported() {}

/** 
 * @func bt_le_scan_start
 * @desc This function starts a Bluetooth LE scan with default parameters and no filters. The scan results will be delivered through callback.
 * @returns {real}
 * 
 * @event social
 * @member {string} type The value `"bt_le_scan_start"`
 * @member {real} async_id The unique async identifier that refers to this task
 * @member {boolean} success `true` if the scan completed successfully, `false` otherwise.
 * @member {real} error_code The error code (if the scan failed).
 * @event_end
 * 
 * @event social
 * @member {string} type The value `"bt_le_scan_result"`
 * @member {string} name The name of the device.
 * @member {string} address The unique address of the device.
 * @member {real} raw_signal The device signal strength (RSSI)
 * @member {boolean} is_connectable Whether or not the given device is connectable.
 * @event_end
 * 
 * @func_end
 */
function bt_le_scan_start(address) {}

/** 
 * @func bt_le_scan_stop
 * @desc This function stops the scanning process for Bluetooth LE devices.
 * @returns {real}
 * 
 * @event social
 * @member {string} type The value `"bt_le_scan_stop"`
 * @member {real} async_id The unique identifier of this async task.
 * @member {boolean} success `true` if the scan has stopped successfully, `false` otherwise.
 * @event_end
 * 
 * @func_end
 */
function bt_le_scan_stop() {}

/** 
 * @func bt_le_scan_is_active
 * @desc This function returns whether or not the scan is currently active.
 * @returns {boolean}
 * 
 * @func_end
 */
function bt_le_scan_is_active() {}

/** 
 * @func bt_le_advertise_start
 * @desc This function starts Bluetooth LE Advertising.
 * @param {struct.AdvertiseSettings} settings A struct containing all the settings to be applied to the advertisement.
 * @param {struct.AdvertiseData} data A struct containing all the data to be applied to the advertisement.
 * @returns {real}
 * 
 * @event social
 * @member {string} type The value `"bt_le_advertise_start"`
 * @member {real} async_id The unique identifier of this async task.
 * @member {boolean} success `true` if the advertisement has started successfully, `false` otherwise.
 * @member {real} error_code The error code (if advertise failed).
 * @event_end
 * 
 * @func_end
 */
function bt_le_advertise_start(settings, data) {}

/** 
 * @func bt_le_advertise_stop
 * @desc This function stops Bluetooth LE advertising.
 * 
 * @event social
 * @member {string} type The value `"bt_le_advertise_stop"`
 * @member {real} async_id The unique identifier of this async task.
 * @member {boolean} success `true` if the advertisement has stopped successfully, `false` otherwise.
 * @event_end
 * 
 * @func_end
 */
function bt_le_advertise_stop() {}

/** 
 * @func bt_le_advertise_is_active
 * @desc This function checks if advertisement is currently active.
 * @returns {boolean}
 * @func_end
 */
function bt_le_advertise_is_active() {}

/** 
 * @func bt_le_server_open
 * @desc This function initialises the GATT server callback handlers (required before any other `bt_le_server_*` calls).
 * The function call will enable various event callbacks that the user can start listening to.
 * @returns {real}
 * 
 * @event social
 * @member {string} type The value `"bt_le_server_open"`
 * @member {real} async_id The unique async identifier that refers to this task
 * @member {boolean} success `true` if the task completed successfully, `false` otherwise.
 * @event_end
 * 
 * @event social
 * @description This event is triggered when a client requests a read from a characteristic (it's up to the user to respond to the request).
 * @member {string} type The value `"bt_le_server_characteristic_read_request"`
 * @member {real} request_id The unique async identifier that refers to this request (use in ${function.bt_le_server_respond_read})
 * @member {string} service_uuid The UUID of the service that hosts the target characteristic.
 * @member {string} characteristic_uuid The UUID of the characteristic to be read.
 * @event_end
 * 
 * @event social
 * @description This event is triggered when a client requests a write to a characteristic (it's up to the user to accept the write or not).
 * @member {string} type The value `"bt_le_server_characteristic_write_request"`
 * @member {real} request_id The unique async identifier that refers to this request (use in ${function.bt_le_server_respond_write})
 * @member {string} service_uuid The UUID of the service that hosts the target characteristic.
 * @member {string} characteristic_uuid The UUID of the characteristic to be read.
 * @member {string} value The base64 encoded string of the value to be written.
 * @event_end
 * 
 * @event social
 * @description This event is triggered when a client commands a write to a characteristic (it's up to the user to accept the write or not).
 * @member {string} type The value `"bt_le_server_characteristic_write_command"`
 * @member {string} service_uuid The UUID of the service that hosts the target characteristic.
 * @member {string} characteristic_uuid The UUID of the characteristic to be read.
 * @member {string} value The base64 encoded string of the value to be written.
 * @event_end
 * 
 * @event social
 * @description This event is triggered when the connection of a given device changes.
 * @member {string} type The value `"bt_le_server_connection_state_changed"`
 * @member {boolean} success Whether the connection state change was successful.
 * @member {boolean} connected Whether or not the given device has connected or disconnected.
 * @member {struct.BluetoothDevice} device A struct holding data about the given device.
 * @event_end
 * 
 * @event social
 * @description This event is triggered when a client requests a read from a descriptor (it's up to the user to respond to the request).
 * @member {string} type The value `"bt_le_server_descriptor_read_request"`
 * @member {real} request_id The unique async identifier that refers to this request (use in ${function.bt_le_server_respond_read})
 * @member {string} service_uuid The UUID of the service that hosts the target descriptor.
 * @member {string} characteristic_uuid The UUID of the characteristic that hosts the target descriptor.
 * @member {string} descriptor_uuid The UUID of the descriptor to be read.
 * @event_end
 * 
 * @event social
 * @description This function is triggered when a client requests a write to a descriptor (it's up to the user to accept the write or not).
 * @member {string} type The value `"bt_le_server_descriptor_write_request"`
 * @member {real} request_id The unique async identifier that refers to this request (use in ${function.bt_le_server_respond_write})
 * @member {string} service_uuid The UUID of the service that hosts the target descriptor.
 * @member {string} characteristic_uuid The UUID of the characteristic that hosts the target descriptor.
 * @member {string} descriptor_uuid The UUID of the descriptor to be read.
 * @member {string} value The base64 encoded string of the value to be written.
 * @event_end
 * 
 * @event social
 * @description This event is triggered as the result of a call to the function ${function.bt_le_server_notify_value}.
 * @member {string} type The value `"bt_le_server_notify_value"`
 * @member {real} async_id The unique async identifier that refers to this task.
 * @member {boolean} success true if the task was completed successfully, false otherwise.
 * @member {Array[struct.NotifiedDevice]} devices An array of notified devices and the status of the notification. Note of mobile this will be a JSON encoded string that needs to be parsed using ${function.json_parse}.
 * @event_end
 * 
 * @func_end
 */
function bt_le_server_open() {}

/** 
 * @func bt_le_server_add_service
 * @desc This function adds a new service to the currently opened server.
 * @param {struct.ServiceData} data A struct representing the service to be added (on mobile this argument needs to be stringified using ${function.json_stringify}).
 * @returns {real}
 * 
 * @event social
 * @member {string} type The value `"bt_le_server_add_service"`
 * @member {real} async_id The unique async identifier that refers to this task
 * @member {boolean} success `true` if the task was completed successfully, `false` otherwise.
 * @member {real} error_code The error code (if scan failed).
 * @member {string} service The UUID of the service being added.
 * @event_end
 * 
 * @func_end
 */
function bt_le_server_add_service(data) {}

/** 
 * @func bt_le_server_clear_services
 * @desc This function clears all previously added services to the GATT server.
 * @returns {boolean}
 * @func_end
 */
function bt_le_server_clear_services() {}

/** 
 * @func bt_le_server_close
 * @desc This function closes the current GATT server and unregisters all the callback handlers regarding it.
 * @returns {real}
 * 
 * @event social
 * @member {string} type The value `"bt_le_server_close"`
 * @member {real} async_id The unique async identifier that refers to this task
 * @member {boolean} success `true` if the task was completed successfully, `false` otherwise.
 * @event_end
 * 
 * @func_end
 */
function bt_le_server_close() {}

/** 
 * @func bt_le_server_respond_read
 * @desc You can use this function to respond to an incoming read request.
 * @param {real} request_id The identifier of the request.
 * @param {constant.BluetoothStatus} status The status of the read attempt.
 * @param {string} value The base64 encoded string with the value requested (if status is `BT_STATUS_FAILURE` it is ignored).
 * @returns {boolean}
 * @func_end
 */
function bt_le_server_respond_read(requestId, status, value) {}

/** 
 * @func bt_le_server_respond_write
 * @desc You can use this function to respond to an incoming write request.
 * @param {real} request_id The identifier of the request.
 * @param {constant.BluetoothStatus} status The status of the write attempt.
 * @returns {boolean}
 * @func_end
 */
function bt_le_server_respond_write(requestId, status) {}

/** 
 * @func bt_le_server_notify_value
 * @desc This function notifies all the subscribers of a given characteristic that its value has changed.
 * @param {string} service_uuid The UUID of the service the characteristic belongs to.
 * @param {string} charactertistic_uuid The UUID of the characteristic.
 * @param {string} value The base64 encoded string with the new value.
 * @returns {boolean}
 * @func_end
 */
function bt_le_server_notify_value(service, characteristic, value) {}

/** 
 * @func bt_le_peripheral_open
 * @desc This function opens a connection between the current client and a peripheral.
 * @param {string} address The unique address of the device you want to connect to.
 * @returns {real}
 * 
 * @event social
 * @member {string} type The value `"bt_le_peripheral_open"`
 * @member {real} async_id The unique async identifier that refers to this task
 * @member {boolean} is_paired Are we already paired with the connected device
 * @member {string} address The unique address of the peripheral we are connecting to.
 * @member {string} name The name of the peripheral (empty string if null).
 * @event_end
 * 
 * @func_end
 */
function bt_le_peripheral_open(address) {}

/** 
 * @func bt_le_peripheral_is_open
 * @desc This function checks if a GATT connection to a given peripheral is opened.
 * @param {string} address The unique address of the peripheral.
 * @returns {boolean}
 * @func_end
 */
function bt_le_peripheral_is_open(address) {}

/** 
 * @func bt_le_peripheral_close_all
 * @desc This function closes all open GATT connections to all peripherals.
 * @returns {boolean}
 * @func_end
 */
function bt_le_peripheral_close_all() {}

/** 
 * @func bt_le_peripheral_close
 * @desc This function closes the GATT connection to a given peripheral.
 * @param {string} address The unique address of the peripheral.
 * @returns {boolean}
 * @func_end
 */
function bt_le_peripheral_close(address) {}

/** 
 * @func bt_le_peripheral_is_connected
 * @desc This function checks if a given peripheral is connected.
 * @param {string} address The unique address of the peripheral.
 * @returns {boolean}
 * @func_end
 */
function bt_le_peripheral_is_connected(address) {}

/** 
 * @func bt_le_peripheral_is_paired
 * @desc This function checks if a given peripheral is paired.
 * @param {string} address The unique address of the peripheral.
 * @returns {boolean}
 * @func_end
 */
function bt_le_peripheral_is_paired(address) {}

/** 
 * @func bt_le_peripheral_get_services
 * @desc This function gets the services for a given peripheral.
 * @param {string} address The unique address of the peripheral.
 * @returns {real}
 * 
 * @event social
 * @member {string} type The value `"bt_le_peripheral_get_services"`
 * @member {real} async_id The unique async identifier that refers to this task
 * @member {boolean} success true if the task was completed successfully, false otherwise.
 * @member {Array[struct.BluetoothService]} services An array of ${struct.BluetoothService} associated with the peripheral. On mobile platforms this will be a JSON encoded string that needs to be parsed using ${function.json_parse}.
 * @event_end
 * 
 * @func_end
 */
function bt_le_peripheral_get_services(address) {}

/** 
 * @func bt_le_service_get_characteristics
 * @desc This function gets the characteristics of a given service.
 * @param {string} address The unique address of the peripheral.
 * @param {string} service_uuid The UUID of the service.
 * @returns {real}
 * 
 * @event social
 * @member {string} type The value `"bt_le_service_get_characteristics"`
 * @member {real} async_id The unique async identifier that refers to this task
 * @member {boolean} success true if the task was completed successfully, false otherwise.
 * @member {Array[struct.BluetoothCharacteristic]} characteristics An array of ${struct.BluetoothCharacteristic} associated with the service. On mobile platforms this will be a JSON encoded string that needs to be parsed using ${function.json_parse}.
 * @event_end
 * 
 * @func_end
 */
function bt_le_service_get_characteristics(address, service) {}

/** 
 * @func bt_le_characteristic_get_descriptors
 * @desc This function gets the descriptors of a given characteristic.
 * @param {string} address The unique address of the peripheral.
 * @param {string} service_uuid The UUID of the service the characteristic belongs to.
 * @param {string} charactertistic_uuid The UUID of the characteristic.
 * @returns {real}
 * 
 * @event social
 * @member {string} type The value `"bt_le_characteristic_get_descriptors"`
 * @member {real} async_id The unique async identifier that refers to this task
 * @member {boolean} success true if the task was completed successfully, false otherwise.
 * @member {Array[struct.BluetoothDescriptor]} descriptors An array of ${struct.BluetoothDescriptor} associated with the characteristic. On mobile platforms this will be a JSON encoded string that needs to be parsed using ${function.json_parse}.
 * @event_end
 * 
 * @func_end
 */
function bt_le_characteristic_get_descriptors(address, service, characteristic) {}

/** 
 * @func bt_le_characteristic_read
 * @desc This function requests a read operation on a peripheral's characteristic.
 * @param {string} address The unique address of the peripheral.
 * @param {string} service_uuid The UUID of the service the characteristic belongs to.
 * @param {string} charactertistic_uuid The UUID of the characteristic.
 * @returns {real}
 * 
 * @event social
 * @member {string} type The value `"bt_le_characteristic_read"`
 * @member {real} async_id The unique async identifier that refers to this task
 * @member {boolean} success `true` if the task completed successfully, `false` otherwise.
 * @member {real} error_code The error code if the task was not completed successfully.
 * @member {string} value The base64 encoded string with the read value.
 * @event_end
 * 
 * @func_end
 */
function bt_le_characteristic_read(address, service, characteristic) {}

/** 
 * @func bt_le_characteristic_write_request
 * @desc This function requests a write operation on a peripheral's characteristic.
 * @param {string} address The unique address of the peripheral.
 * @param {string} service_uuid The UUID of the service the characteristic belongs to.
 * @param {string} charactertistic_uuid The UUID of the characteristic.
 * @param {string} value The base64 encoded string with the new value.
 * @returns {real}
 * 
 * @event social
 * @member {string} type The value `"bt_le_characteristic_write_request"`
 * @member {real} async_id The unique async identifier that refers to this task
 * @member {boolean} success true if the task was completed successfully, false otherwise.
 * @member {real} error_code The error code if the task was not completed successfully.
 * @event_end
 * 
 * @func_end
 */
function bt_le_characteristic_write_request(address, service, characteristic, value) {}

/** 
 * @func bt_le_characteristic_write_command
 * @desc This function commands a write operation on a peripheral's characteristic (note that this might not happen, there is no confirmation).
 * @param {string} address The unique address of the peripheral.
 * @param {string} service_uuid The UUID of the service the characteristic belongs to.
 * @param {string} charactertistic_uuid The UUID of the characteristic.
 * @param {string} value The base64 encoded string with the new value.
 * @returns {real}
 * @func_end
 */
function bt_le_characteristic_write_command(address, service, characteristic, value) {}

/** 
 * @func bt_le_characteristic_notify
 * @desc This function enables notifications when then value of a characteristic changes.
 * @param {string} address The unique address of the peripheral.
 * @param {string} service_uuid The UUID of the service the characteristic belongs to.
 * @param {string} charactertistic_uuid The UUID of the characteristic.
 * @returns {real}
 * 
 * @event social
 * @member {string} type The value `"bt_le_characteristic_notify"`
 * @member {real} async_id The unique async identifier that refers to this task
 * @member {boolean} success `true` if the task completed successfully, `false` otherwise.
 * @member {boolean} error_code The error code if the task didn't complete successfully.
 * @event_end
 * 
 * @event social
 * @description This event is triggered when there is a change in the subscribed characteristic.
 * @member {string} type The value `"bt_le_characteristic_value_changed"`
 * @member {string} characteristic_uuid The UUID of the characteristic whose value changed.
 * @member {string} service_uuid The UUID of the service the characteristic belongs to.
 * @member {string} address The unique address of the peripheral we are connecting to.
 * @member {string} value The base64 encoded string with the new value.
 * @event_end
 * 
 * @func_end
 */
function bt_le_characteristic_notify(address, service, characteristic) {}

/** 
 * @func bt_le_characteristic_indicate
 * @desc This function enables indication of characteristic value changes.
 * @param {string} address The unique address of the peripheral.
 * @param {string} service The UUID of the service the characteristic belongs to.
 * @param {string} characteristic The UUID of the characteristic.
 * @returns {real}
 * 
 * @event social
 * @member {string} type The value `"bt_le_characteristic_indicate"`
 * @member {real} async_id The unique async identifier that refers to this task.
 * @member {boolean} success `true` if the task completed successfully, `false` otherwise.
 * @member {boolean} error_code The error code if the task didn't complete successfully.
 * @event_end
 * 
 * @event social
 * @description This event is triggered when there is a change in the subscribed characteristic.
 * @member {string} type The value `"bt_le_characteristic_value_changed"`
 * @member {real} characteristic_uuid The UUID of the characteristic whose value changed.
 * @member {boolean} service_uuid The UUID of the service the characteristic belongs to.
 * @member {string} address The unique address of the peripheral where the change occurred.
 * @member {string} name The base64 encoded string with the new value.
 * @event_end
 * 
 * @func_end
 */
function bt_le_characteristic_indicate(address, service, characteristic) {}

/** 
 * @func bt_le_characteristic_unsubscribe
 * @desc This function unsubscribes from any previous notification|indication on a given characteristic.
 * @param {string} address The unique address of the peripheral.
 * @param {string} service The UUID of the service the characteristic belongs to.
 * @param {string} characteristic The UUID of the characteristic.
 * @returns {real}
 * 
 * @event social
 * @member {string} type The value `"bt_le_characteristic_unsubscribe"`
 * @member {real} async_id The unique async identifier that refers to this task.
 * @member {boolean} success `true` if the task completed successfully, `false` otherwise.
 * @member {boolean} error_code The error code if the task didn't complete successfully.
 * @event_end
 * 
 * @func_end
 */
function bt_le_characteristic_unsubscribe(address, service, characteristic) {}

/** 
 * @func bt_le_descriptor_read
 * @desc This function requests a read operation on a characteristic's descriptor.
 * @param {string} address The unique address of the peripheral.
 * @param {string} service The UUID of the service the characteristic belongs to.
 * @param {string} characteristic The UUID of the characteristic the descriptor belongs to.
 * @param {string} descriptor The UUID of the descriptor.
 * @returns {real}
 * 
 * @event social
 * @member {string} type The value `"bt_le_descriptor_read"`
 * @member {real} async_id The unique async identifier that refers to this task
 * @member {boolean} success `true` if the task completed successfully, `false` otherwise.
 * @member {real} error_code The error code if the task didn't complete successfully.
 * @member {string} value The base64 encoded string with the read value.
 * @event_end
 * 
 * @func_end
 */
function bt_le_descriptor_read(address, service, characteristic, descriptor) {}

/** 
 * @func bt_le_descriptor_write
 * @desc This function requests a write operation on a characteristic's descriptor.
 * @param {string} address The unique address of the peripheral.
 * @param {string} service The UUID of the service the characteristic belongs to.
 * @param {string} characteristic The UUID of the characteristic the descriptor belongs to.
 * @param {string} descriptor The UUID of the descriptor.
 * @param {string} value The base64 encoded string with the new value.
 * @returns {real}
 * 
 * @event social
 * @member {string} type The value `"bt_le_descriptor_write"`
 * @member {real} async_id The unique async identifier that refers to this task
 * @member {boolean} success `true` if the task completed successfully, `false` otherwise.
 * @member {real} error_code The error code if the task didn't complete successfully.
 * @event_end
 * 
 * @func_end
 */
function bt_le_descriptor_write(address, service, characteristic, descriptor, value) {}

// CONSTANTS

/**
 * @const BluetoothStatus
 * These constants represent the various status responses that can be obtained while using Bluetooth LE.
 * @member BT_STATUS_SUCCESS A GATT operation completed successfully (0)
 * @member BT_STATUS_WRITE_NOT_PERMITTED GATT write operation is not permitted (3)
 * @member BT_STATUS_REQUEST_NOT_SUPPORTED The given request is not supported (6)
 * @member BT_STATUS_READ_NOT_PERMITTED GATT read operation is not permitted (2)
 * @member BT_STATUS_INVALID_OFFSET A read or write operation was requested with an invalid offset (7)
 * @member BT_STATUS_INVALID_ATTRIBUTE_LENGTH A write operation exceeds the maximum length of the attribute (13)
 * @member BT_STATUS_INSUFFICIENT_ENCRYPTION Insufficient encryption for a given operation (15)
 * @member BT_STATUS_INSUFFICIENT_AUTHORIZATION Insufficient authorization for a given operation (8)
 * @member BT_STATUS_INSUFFICIENT_AUTHENTICATION Insufficient authentication for a given operation (5)
 * @member BT_STATUS_CONNECTION_CONGESTED A remote device connection is congested (143)
 * @member BT_STATUS_FAILURE A GATT operation failed, errors other than the above (257)
 * @member BT_STATUS_ACTION_NOT_STARTED An operation didn't start, for unknown reason (-10)
 * @member BT_STATUS_SERVICE_NOT_FOUND A service with the given UUID was not found (-1)
 * @member BT_STATUS_CHARACTERISTIC_NOT_FOUND A characteristic with the given UUID was not found (-2)
 * @member BT_STATUS_DESCRIPTOR_NOT_FOUND A descriptor with the given UUID was not found (-3)
 * @const_end
 */

/**
 * @const BluetoothProperty
 * These constants represent the various properties that can be applied to a given characteristic.
 * @member BT_PROPERTY_BROADCAST Characteristic is broadcastable. (1)
 * @member BT_PROPERTY_EXTENDED_PROPS Characteristic has extended properties (128)
 * @member BT_PROPERTY_INDICATE Characteristic supports indication (32)
 * @member BT_PROPERTY_NOTIFY Characteristic supports notification (16)
 * @member BT_PROPERTY_READ Characteristic is readable. (2)
 * @member BT_PROPERTY_SIGNED_WRITE Characteristic supports write with signature (64)
 * @member BT_PROPERTY_WRITE Characteristic can be written. (8)
 * @member BT_PROPERTY_WRITE_NO_RESPONSE Characteristic can be written without response. (4)
 * @const_end
 */

/**
 * @const BluetoothPermission
 * These constants represent the various permissions that can be applied to a given characteristic.
 * @member BT_PERMISSION_READ Characteristic read permission. (1)
 * @member BT_PERMISSION_READ_ENCRYPTED Allow encrypted read operations (2)
 * @member BT_PERMISSION_READ_ENCRYPTED_MITM Allow reading with person-in-the-middle protection (4)
 * @member BT_PERMISSION_WRITE Characteristic write permission (16)
 * @member BT_PERMISSION_WRITE_ENCRYPTED Allow encrypted writes. (32)
 * @member BT_PERMISSION_WRITE_ENCRYPTED_MITM Allow encrypted writes with person-in-the-middle protection (64)
 * @member BT_PERMISSION_WRITE_SIGNED Allow signed write operations. (128)
 * @member BT_PERMISSION_WRITE_SIGNED_MITM Allow signed write operations with person-in-the-middle protection. (256)
 * @const_end
 */

/**
 * @const BluetoothServiceType
 * These constants represent the various various service types available in Bluetooth LE.
 * @member BT_SERVICE_TYPE_PRIMARY Primary service (0)
 * @member BT_SERVICE_TYPE_SECONDARY Secondary service - included by primary services (1)
 * @const_end
 */

/**
 * @const AdvertiseMode
 * These constants represent the mode to control the advertising power and latency.
 * @member BT_ADVERTISE_MODE_BALANCED Perform Bluetooth LE advertising in balanced power mode. This is balanced between advertising frequency and power consumption. (1)
 * @member BT_ADVERTISE_MODE_LOW_LATENCY Perform Bluetooth LE advertising in low latency, high power mode. This has the highest power consumption and should not be used for continuous background advertising. (2)
 * @member BT_ADVERTISE_MODE_LOW_POWER Perform Bluetooth LE advertising in low power mode. This is the default and preferred advertising mode as it consumes the least power. (0)
 * @const_end
 */

/**
 * @const AdvertiseTxPower
 * These constants represent the TX power level for advertising.
 * @member BT_ADVERTISE_TX_POWER_HIGH Advertise using high TX power level. This corresponds to largest visibility range of the advertising packet. (3)
 * @member BT_ADVERTISE_TX_POWER_LOW Advertise using low TX power level. (1)
 * @member BT_ADVERTISE_TX_POWER_MEDIUM Advertise using medium TX power level. (2)
 * @member BT_ADVERTISE_TX_POWER_ULTRA_LOW Advertise using the lowest transmission (TX) power level. Low transmission power can be used to restrict the visibility range of advertising packets. (0)
 * @const_end
 */

// STRUCTS

/**
 * @struct AdvertiseSettings
 * This struct represents the collection of settings to apply to the advertisement.
 * @member {constant.AdvertiseMode} advertiseMode Set the advertise mode to control the advertising power and latency.
 * @member {boolean} connectable Whether the device should be connectable.
 * @member {boolean} discoverable Whether the device should be discoverable (Windows and macOS|iOS only)
 * @member {real} timeout Limit advertising to a given amount of time. May not exceed 180000 milliseconds. A value of 0 will disable the time limit.
 * @member {constant.AdvertiseTxPower} txPowerLevel Set advertise TX power level to control the transmission power level for the advertising.
 * @struct_end
 */

/**
 * @struct AdvertiseData
 * This struct represents the data to be included in the advertisement.
 * @member {string} name The name to be used.
 * @member {boolean} includeName The visible device name.
 * @member {boolean} includePowerLevel The device unique address.
 * @member {array[struct.AdvertiseServiceData]} services Services to be advertised.
 * @member {struct.AdvertiseManufacturerData} manufacturer Manufacturer information.
 * @struct_end
 */

/**
 * @struct AdvertiseServiceData
 * This struct represents the service data to be included in the advertisement.
 * @member {string} uuid The UUID of the service to be advertised.
 * @struct_end
 */

/**
 * @struct AdvertiseManufacturerData
 * This struct represents the manufacturer data to be included in the advertisement.
 * @member {real} id The manufacturer unique ID.
 * @member {string} data A base64 encoded string with the data associated with the manufacturer.
 * @struct_end
 */

/**
 * @struct BluetoothDevice
 * This struct represents a fetched Bluetooth device.
 * @member {string} name The visible device name.
 * @member {string} address The device unique address.
 * @member {boolean} is_paired Whether or not the devices are paired.
 * @member {real} type The Bluetooth device type of the remote device.
 * @member {string} alias The locally modifiable name (alias) of the remote Bluetooth device.
 * @struct_end
 */

/**
 * @struct BluetoothService
 * This struct represents a fetched Bluetooth service.
 * @member {string} uuid The service unique UUID.
 * @member {constant.BluetoothServiceType} type The type of this service (primary/secondary).
 * @struct_end
 */

/**
 * @struct BluetoothCharacteristic
 * This struct represents a fetched Bluetooth characteristic.
 * @member {string} uuid The characteristic unique UUID.
 * @member {constant.BluetoothProperty} properties The properties of this characteristic. The properties contain a bit mask of property flags indicating the features of this characteristic.
 * @member {constant.BluetoothPermission} permissions The permissions for this characteristic.
 * @struct_end
 */

/**
 * @struct BluetoothDescriptor
 * This struct represents a fetched Bluetooth descriptor.
 * @member {string} uuid The descriptor unique UUID.
 * @struct_end
 */

/**
 * @struct ServiceData
 * This struct represents a service that is being created.
 * @member {string} uuid The service unique UUID.
 * @member {array[struct.CharacteristicData]} charactertistics An array of all the charactertistics contained in this service.
 * @struct_end
 */

/**
 * @struct CharacteristicData
 * This struct represents a characteristic that is being created.
 * @member {string} uuid The characteristic unique UUID.
 * @member {constant.BluetoothProperty} properties The properties to be applied to this characteristic.
 * @member {constant.BluetoothPermission} permissions The permissions to be applied to this characteristic.
 * @member {array[struct.DescriptorData]} descriptors An array of all the descriptors contained in this characteristic.
 * @struct_end
 */

/**
 * @struct DescriptorData
 * This struct represents a descriptor that is being created.
 * @member {string} uuid The descriptor unique UUID.
 * @member {constant.BluetoothPermission} permissions The permissions to be applied to this descriptor.
 * @struct_end
 */

/**
 * @struct NotifiedDevice
 * This struct represents a device that was notified of a change in the value of a characteristic.
 * @member {string} address The unique address of the device that was notified.
 * @member {constant.BluetoothStatus} status The status of the notification operation.
 * @struct_end
 */

// MODULES

/**
 * @module bt_le
 * @title Bluetooth LE
 * @desc This module serves as a comprehensive toolkit, offering an array of functionalities to effectively manage the behaviour of Bluetooth Low Energy (LE) in the context of a connection hosted on an Android|iOS|Windows device.
 *
 * The module is specifically designed for Bluetooth LE, a power-efficient version of Bluetooth intended for short-range communication between devices, making it an ideal tool for managing connections with low-power peripherals such as fitness monitors, smart home devices, and similar Internet of Things (IoT) devices.
 * 
 * With the module's functionalities, developers can have fine-grained control over various aspects of Bluetooth LE behaviour. This could include establishing and managing connections, handling data transmission, scanning for devices, and optimising power usage, all within the Android environment.
 * 
 * @section_func
 * @ref bt_le_*
 * @section_end
 * 
 * @section_const
 * @ref BluetoothStatus
 * @ref BluetoothProperty
 * @ref BluetoothPermission
 * @ref BluetoothServiceType
 * @ref AdvertiseMode
 * @ref AdvertiseTxPower
 * @section_end
 * 
 * @section_struct
 * @ref AdvertiseSettings
 * @ref AdvertiseData
 * @ref AdvertiseServiceData
 * @ref AdvertiseManufacturerData
 * @ref BluetoothDevice
 * @ref BluetoothService
 * @ref BluetoothCharacteristic
 * @ref BluetoothDescriptor
 * @ref ServiceData
 * @ref CharacteristicData
 * @ref DescriptorData
 * @ref NotifiedDevice
 * @section_end
 * 
 * @module_end
 */

