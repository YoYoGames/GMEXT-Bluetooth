

// CLASSIC

/** 
 * @func bt_classic_is_supported
 * @desc Returns true if the device has the necessary bluetooth features, false otherwise
 * @returns {boolean}
 * @func_end
 */
function bt_classic_is_supported() {}

/** 
 * @func bt_classic_scan_start
 * @desc Start the remote device discovery process.
 * @returns {real}
 * 
 * @event social
 * @member {string} type The value `"bt_classic_scan_start"`
 * @member {real} async_id The identifier unique of this async task.
 * @member {boolean} success The value `true` if the scan opearaion started successfuly.
 * @member {real} error_code The error code if the operation didn't succeeded.
 * @event_end
 * 
 * @event social
 * @member {string} type The value `"bt_classic_scan_result"`
 * @member {boolean} success The value `true` if the scan opearaion started successfuly.
 * @member {real} name The identifying name of the scanned device.
 * @member {real} address The unique address of the scanned device.
 * @member {real} connected Whether or not the device is currently connected.
 * @member {real} authenticated Whether or not the device is currently authenticated (paired).
 * @member {real} remembered Whether or not the device is currently remembered (paired).
 * @event_end
 * 
 * @func_end
 */
function bt_classic_scan_start() {}

/** 
 * @func bt_classic_scan_is_active
 * @desc Return true if the local Bluetooth adapter is currently in the device scanning process.
 * @returns {boolean}
 * @func_end
 */
function bt_classic_scan_is_active() {}

/** 
 * @func bt_classic_scan_stop
 * @desc Cancel the current device discovery process.
 * @returns {real}
 * 
 * @event social
 * @member {string} type The value `"bt_classic_scan_stop"`
 * @member {real} async_id The identifier unique of this async task.
 * @member {boolean} success The value `true` if the scan opearaion started successfuly.
 * @member {real} error_code The error code (if the operation didn't succeeded).
 * @event_end
 * 
 * @func_end
 */
function bt_classic_scan_stop() {}

/** 
 * @func bt_classic_discoverability_enable
 * @desc Requests the OS to enable the visibility of the device for an amount of time.
 * @param {real} seconds The amount of seconds to enable discoverability for.
 * @func_end
 */
function bt_classic_discoverability_enable(seconds) {}

/** 
 * @func bt_classic_discoverability_is_active
 * @desc Check if the device is correctly discoverable to others.
 * @returns {boolean}
 * @func_end
 */
function bt_classic_discoverability_is_active() {}

/** 
 * @func bt_classic_server_start
 * @desc Create a listening, secure|insecure RFCOMM Bluetooth socket with Service Record.
 * @param {string} name The service name for SDP record
 * @param {string} uuid The uuid for SDP record
 * @param {string} insecure Should the socket be insecure? (this option needs to match the one in connecting devices)
 * @returns {real}
 * 
 * @event social
 * @member {string} type The value `"bt_classic_server_start"`
 * @member {real} async_id The identifier unique of this async task.
 * @member {boolean} success true if the operation was completed successfully, false otherwise
 * @event_end
 * 
 * @event social
 * @description This event is triggered when a new connection is accepted.
 * @member {string} type The value `"bt_classic_server_accept"`
 * @member {boolean} success true if the operation was completed successfully, false otherwise
 * @member {real} socket_id The unique identifier of the connected socket (used for sending and receiving data).
 * @event_end
 * 
 * @func_end
 */
function bt_classic_server_start(name, uuid, insecure) {}

/** 
 * @func bt_classic_server_stop
 * @desc Immediately close this socket, and release all associated resources.
 * @returns {real}
 * 
 * @event social
 * @member {string} type The value `"bt_classic_server_stop"`
 * @member {real} async_id The identifier unique of this async task.
 * @member {boolean} success true if the operation was completed successfully, false otherwise
 * @event_end
 * 
 * @func_end
 */
function bt_classic_server_stop() {}

/** 
 * @func bt_classic_socket_receive
 * @desc Read incoming data sent by a given opened socket.
 * @param {real} socketId The unique identifier of the socket you want to read the data from.
 * @param {Id.Buffer} bufferId The buffer where the incoming data will be written. Data is written in chuncks, make sure that your buffer is at least the size of a chunck. No resizing will occur.
 * @param {real} offset The byte offset to be used when writting to the buffer.
 * @func_end
 */
function bt_classic_socket_receive(socket, buff, offset) {}

/** 
 * @func bt_classic_socket_send
 * @desc Send data to a given remotely connected device.
 * @param {real} socketId The unique identifier of the socket you want to send data to.
 * @param {Id.Buffer} bufferId The buffer where the outgoing data is stored.
 * @param {real} offset The byte offset to be used when reading from the buffer.
 * @param {real} length The amount of buffer data to be sent.
 * @func_end
 */
function bt_classic_socket_send(socket, buff, offset, length) {}

/** 
 * @func bt_classic_socket_open
 * @desc Create an RFCOMM BluetoothSocket socket ready to start an insecure outgoing connection to this remote device using SDP lookup of uuid.
 * @param {string} address Address of the device to attempt connection to.
 * @param {string} uuid Service record uuid to lookup RFCOMM channel.
 * @param {boolean} insecure Should the Bluetooth connection be insecure (this option needs to match the one in the server).
 * 
 * @event social
 * @member {string} type The value `"bt_classic_socket_open"`
 * @member {boolean} success true if the connection was completed successfully, false otherwise.
 * @member {string} address The address of the device.
 * @member {string} uuid The service record uuid connected to.
 * @member {string} socket_id (if success it's `true`) The index of the created socket.
 * @member {string} error_message (if success it's `false`) The error message.
 * @event_end
 * 
 * @func_end
 */
function bt_classic_socket_open(address, uuid, insecure) {}

/** 
 * @func bt_classic_socket_close
 * @desc Allows the user to close a previously opened connection to a bluetooth server.
 * @param {real} socketId The socket id aquired upon successfully opening a connection (using ${function.bt_classic_socket_open}).
 * @func_end
 */
function bt_classic_socket_close(socketId) {}

/** 
 * @func bt_classic_socket_close_all
 * @desc Allows the user to close ALL previously opened connections to bluetooth servers.
 * @func_end
 */
function bt_classic_socket_close_all() {}

/**
 * @module bt_classic
 * @title Bluetooth Classic
 * @desc This module presents a collection of functionalities specifically designed to manage and regulate the behavior of Classic Bluetooth between Android and Windows devices.
 * 
 * Unlike Bluetooth Low Energy, Classic Bluetooth, often referred to as just Bluetooth, is intended for more data-intensive applications, making it suitable for activities that require higher data rates such as streaming audio or transferring files. This module is, therefore, particularly useful when dealing with these more demanding Bluetooth operations in an Android|Windows environment.
 * 
 * @section_func
 * @ref bt_classic_*
 * @section_end
 * 
 * @module_end
 */
