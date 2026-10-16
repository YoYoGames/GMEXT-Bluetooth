// Bluetooth Classic (RFCOMM): support queries, discovery, client connections, sending, the server,
// discoverability and the Classic event callbacks.

/**
 * @function bluetooth_classic_is_supported
 * @desc This function checks whether Bluetooth Classic (RFCOMM) client connections and Classic
 * discovery are available on this platform and adapter. It returns `false` before
 * ${function.bluetooth_initialize} has been called, so ask after initialising. It gives the same
 * answer as ${function.bluetooth_feature_is_supported} with ${constant.BluetoothFeature}.Classic.
 *
 * [[Note: Bluetooth Classic is available on Windows, Android and macOS. On iOS this function always
 * returns `false`, and every Classic call that starts radio work fails with
 * ${constant.BluetoothError}.NotSupported.]]
 *
 * [[Note: On Windows the answer comes from the adapter, so a machine with an LE-only adapter returns
 * `false`. On macOS it is `true` once initialised. On Android it is `true` when the device has a
 * Bluetooth adapter.]]
 * @returns {Bool} `true` if Bluetooth Classic is supported, otherwise `false`.
 * @example
 * ```gml
 * if (!bluetooth_classic_is_supported())
 * {
 *     show_debug_message("Bluetooth Classic is not available on this device");
 * }
 * ```
 * The above code checks whether Bluetooth Classic can be used and shows a debug message when it
 * cannot.
 * @function_end
 */

/**
 * @function bluetooth_classic_server_is_supported
 * @desc This function checks whether this platform and adapter can run a Bluetooth Classic RFCOMM
 * server with ${function.bluetooth_classic_server_start}. It returns `false` before
 * ${function.bluetooth_initialize} has been called. It gives the same answer as
 * ${function.bluetooth_feature_is_supported} with ${constant.BluetoothFeature}.ClassicServer.
 *
 * [[Note: The server is available on Windows, Android and macOS wherever
 * ${function.bluetooth_classic_is_supported} returns `true`. On iOS this function always returns
 * `false`.]]
 * @returns {Bool} `true` if a Classic server can be started, otherwise `false`.
 * @example
 * ```gml
 * if (bluetooth_classic_server_is_supported())
 * {
 *     bluetooth_classic_server_start("My Game", "00001101-0000-1000-8000-00805F9B34FB");
 * }
 * ```
 * The above code starts an RFCOMM server that uses the Serial Port Profile UUID, but only when the
 * platform supports a Classic server.
 * @function_end
 */

/**
 * @function bluetooth_classic_scan_start
 * @desc This function starts a Bluetooth Classic discovery (an inquiry scan) for nearby Classic
 * devices. Every new device the scan finds enters the device cache and fires the callback set with
 * ${function.bluetooth_set_callback_device_found}, with a device whose transport is
 * ${constant.BluetoothTransport}.Classic. The scan ends on its own after one inquiry, and the
 * callback set with ${function.bluetooth_set_callback_scan_stopped} is then called with the
 * transport ${constant.BluetoothTransport}.Classic, and with an error when the inquiry could not run.
 *
 * Starting a scan while one is already running returns ${constant.BluetoothError}.Ok and starts
 * nothing new.
 *
 * The function returns a ${constant.BluetoothError}. When it is not
 * ${constant.BluetoothError}.Ok the scan did not start, no callback is called, and the reason is in
 * ${function.bluetooth_last_error_code} and ${function.bluetooth_last_error_message}:
 * NotInitialized before ${function.bluetooth_initialize}, PermissionDenied when the Bluetooth
 * permission is missing or was refused, BluetoothDisabled when the radio is off, NotSupported where
 * Classic is not available, or OperationFailed when the system could not start the inquiry.
 *
 * [[Note: An inquiry runs for about 5 seconds on Windows and 10 seconds on macOS; on Android it lasts
 * as long as the system's discovery.]]
 *
 * [[Note: On Windows the inquiry also returns every device that Windows remembers, in range or not.
 * The extension reports only the remembered devices that answered this inquiry. If Windows cannot
 * list its remembered devices first, every result is reported, including remembered devices that
 * are out of range. Use ${function.bluetooth_paired_devices_query} to list paired devices.]]
 *
 * [[Note: On Android the scan needs the Bluetooth scan and connect permissions, and on Android
 * versions before 12 the Location setting must be on; otherwise the call returns PermissionDenied.
 * On macOS it returns PermissionDenied until ${function.bluetooth_permission_request} has been
 * called. On iOS it returns NotSupported.]]
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the scan started or was already
 * running, otherwise the reason it did not start.
 * @example
 * ```gml
 * var _result = bluetooth_classic_scan_start();
 * if (_result != BluetoothError.Ok)
 * {
 *     show_debug_message($"Classic scan did not start: {bluetooth_last_error_message()}");
 * }
 * ```
 * The above code starts a Classic scan and shows the reason in a debug message when it could not
 * start. The devices it finds are reported to the callback set with
 * ${function.bluetooth_set_callback_device_found}.
 * @function_end
 */

/**
 * @function bluetooth_classic_scan_stop
 * @desc This function stops a running Bluetooth Classic scan. When a scan was running, the callback
 * set with ${function.bluetooth_set_callback_scan_stopped} is called with
 * ${constant.BluetoothError}.Ok and the transport ${constant.BluetoothTransport}.Classic. Stopping
 * when no scan is running returns ${constant.BluetoothError}.Ok and calls nothing.
 *
 * The function returns ${constant.BluetoothError}.NotInitialized before
 * ${function.bluetooth_initialize}, with the reason in ${function.bluetooth_last_error_message}.
 *
 * [[Note: Windows cannot cut an inquiry short. The scan is reported as stopped at once and nothing
 * the inquiry still finds is reported.]]
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the scan is stopped, otherwise the
 * reason.
 * @example
 * ```gml
 * if (bluetooth_classic_scan_is_running())
 * {
 *     bluetooth_classic_scan_stop();
 * }
 * ```
 * The above code stops the Classic scan if one is running.
 * @function_end
 */

/**
 * @function bluetooth_classic_scan_is_running
 * @desc This function checks whether a Bluetooth Classic scan is running. It returns `true` from a
 * successful ${function.bluetooth_classic_scan_start} until the scan ends on its own or is stopped
 * with ${function.bluetooth_classic_scan_stop}.
 * @returns {Bool} `true` if a Classic scan is running, otherwise `false`.
 * @example
 * ```gml
 * scanning_text = bluetooth_classic_scan_is_running() ? "Searching..." : "Idle";
 * ```
 * The above code sets the instance's `scanning_text` variable to a status string that depends on
 * whether a Classic scan is running.
 * @function_end
 */

/**
 * @function bluetooth_classic_connect
 * @desc This function starts an RFCOMM connection to the given service of the given Bluetooth Classic
 * device, which must have been found by a scan, listed by ${function.bluetooth_paired_devices_query}
 * or looked up with ${function.bluetooth_device_from_id}. The service is named by its UUID, either
 * in the full 36-character form or as 4 or 8 hex digits. It returns a connection handle straight
 * away, before the device has answered, and the callback is called once when the connection is made
 * or fails.
 *
 * The function returns 0 when the connect could not be started, and the reason is then in
 * ${function.bluetooth_last_error_code} and ${function.bluetooth_last_error_message}:
 * NotInitialized, InvalidHandle for a device handle that is not valid, InvalidArgument for an LE
 * device or a service UUID that is not valid, BluetoothDisabled, PermissionDenied, or NotSupported
 * where Classic is not available. In that case the callback is never called.
 *
 * You can cancel a connect that is still in progress by passing the returned handle to
 * ${function.bluetooth_classic_disconnect}, and the callback is then called once with
 * ${constant.BluetoothError}.ConnectionFailed. If ${function.bluetooth_shutdown} is called first,
 * the callback is called with ${constant.BluetoothError}.NotInitialized.
 *
 * Once connected, the bytes the device sends arrive through the callback set with
 * ${function.bluetooth_set_callback_classic_data}, you send bytes with
 * ${function.bluetooth_classic_send}, and the callback set with
 * ${function.bluetooth_set_callback_classic_disconnected} tells you when the device ends the
 * connection.
 *
 * [[Note: On Windows a device that does not answer within 15 seconds fails with
 * ${constant.BluetoothError}.Timeout. On Android the system's own connect timeout applies, and a
 * failed connect reports ${constant.BluetoothError}.ConnectionFailed.]]
 *
 * [[Note: On macOS the RFCOMM channel is found by looking up the service UUID in the device's SDP
 * records, through IOBluetooth. The records cached from pairing are used when they hold the
 * service; otherwise an SDP query runs, which fails with ${constant.BluetoothError}.Timeout after 15
 * seconds, and with ${constant.BluetoothError}.NotFound when the device does not offer the service.
 * The call itself returns 0 with NotFound when macOS cannot resolve the device's address, and with
 * PermissionDenied until ${function.bluetooth_permission_request} has been called.]]
 *
 * [[Note: The connection is a secure (authenticated and encrypted) RFCOMM link, so an unpaired device
 * may show a pairing prompt. On Windows the pairing counts toward the 15 seconds. On macOS the
 * authenticated link is opened before the SDP lookup and fails with
 * ${constant.BluetoothError}.Timeout when it is not open after 15 seconds; a link to the device that
 * is already open is used as it is.]]
 *
 * [[Note: On Android a running Classic scan is cancelled before the connect.]]
 * @param {Real} device The handle of the Classic device to connect to.
 * @param {String} service_uuid The UUID of the RFCOMM service to connect to.
 * @param {Function} callback The function to call once the connection is made or fails.
 * @returns {Real} The new connection handle, or 0 if the connect did not start.
 * @event callback
 * @desc Called once, when the connection is made, fails or is cancelled.
 * @member {Enum.BluetoothError} error_code ${constant.BluetoothError}.Ok if the device is
 * connected, otherwise the reason it is not.
 * @member {String} message A description of the failure, or an empty string on success.
 * @member {Real} connection The connection handle that ${function.bluetooth_classic_connect}
 * returned.
 * @member {Real} device The handle of the device.
 * @event_end
 * @example
 * ```gml
 * connection = bluetooth_classic_connect(device, "00001101-0000-1000-8000-00805F9B34FB", function(_error_code, _message, _connection, _device)
 * {
 *     if (_error_code != BluetoothError.Ok)
 *     {
 *         show_debug_message($"Connect failed: {_message}");
 *         connection = 0;
 *         exit;
 *     }
 *     show_debug_message($"Connected to {bluetooth_device_get_name(_device)}");
 * });
 *
 * if (connection == 0)
 * {
 *     show_debug_message($"Connect did not start: {bluetooth_last_error_message()}");
 * }
 * ```
 * The above code starts a connection to the Serial Port Profile service of the device stored in the
 * instance's `device` variable and keeps the returned handle in `connection`. When the callback
 * reports a failure the handle is cleared; on success the name of the device is shown with
 * ${function.bluetooth_device_get_name}. If the connect could not be started at all, the returned
 * handle is 0 and the reason is read with ${function.bluetooth_last_error_message}.
 * @function_end
 */

/**
 * @function bluetooth_classic_disconnect
 * @desc This function ends the given RFCOMM connection, whether it is still connecting or already
 * connected. The disconnect is synchronous: the handle is no longer valid when the function returns,
 * and the callback set with ${function.bluetooth_set_callback_classic_disconnected} is not called
 * for it. A connect that is still in progress is cancelled, and its callback is called once with
 * ${constant.BluetoothError}.ConnectionFailed.
 *
 * Values that the callback set with ${function.bluetooth_set_callback_classic_data} has already
 * delivered can still be fetched with ${function.bluetooth_value_fetch} after the disconnect.
 *
 * The function returns a ${constant.BluetoothError}: NotInitialized before
 * ${function.bluetooth_initialize}, or InvalidHandle for a handle that is not valid, with the reason
 * in ${function.bluetooth_last_error_message}.
 *
 * [[Note: On Android the bytes already queued with ${function.bluetooth_classic_send} are sent
 * before the link closes, for up to 2 seconds. On Windows and macOS the bytes not yet sent are
 * discarded.]]
 * @param {Real} connection The connection handle to end.
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the connection was ended, otherwise
 * the reason.
 * @example
 * ```gml
 * if (bluetooth_classic_connection_is_valid(connection))
 * {
 *     bluetooth_classic_disconnect(connection);
 * }
 * connection = 0;
 * ```
 * The above code ends the connection stored in the instance's `connection` variable if its handle
 * is still valid, then clears the variable.
 * @function_end
 */

/**
 * @function bluetooth_classic_connection_is_valid
 * @desc This function checks whether the given RFCOMM connection handle is valid. A handle is valid
 * from the moment ${function.bluetooth_classic_connect} returns it, or the callback set with
 * ${function.bluetooth_set_callback_classic_client_connected} receives it, until the connect fails,
 * ${function.bluetooth_classic_disconnect} is called, the callback set with
 * ${function.bluetooth_set_callback_classic_disconnected} is called for it, or
 * ${function.bluetooth_shutdown} is called. Handles are never reused.
 * @param {Real} connection The connection handle to check.
 * @returns {Bool} `true` if the handle is valid, otherwise `false`.
 * @example
 * ```gml
 * if (!bluetooth_classic_connection_is_valid(connection))
 * {
 *     connection = 0;
 * }
 * ```
 * The above code clears the instance's `connection` variable when the handle it holds is no longer
 * valid.
 * @function_end
 */

/**
 * @function bluetooth_classic_connection_is_connected
 * @desc This function checks whether the given RFCOMM connection is currently connected. It returns
 * `false` while the connection is still being made, after it has ended, and for a handle that is not
 * valid.
 * @param {Real} connection The connection handle to check.
 * @returns {Bool} `true` if the connection is connected, otherwise `false`.
 * @example
 * ```gml
 * if (bluetooth_classic_connection_is_connected(connection))
 * {
 *     bluetooth_classic_send(connection, message_buffer, 0, message_size);
 * }
 * ```
 * The above code sends the bytes held in the instance's `message_buffer` only when the connection
 * stored in `connection` is connected.
 * @function_end
 */

/**
 * @function bluetooth_classic_connection_get_device
 * @desc This function returns the handle of the device at the other end of the given RFCOMM
 * connection. For a connection accepted by the server it is the device that connected. It returns 0
 * for a handle that is not valid.
 * @param {Real} connection The connection handle to query.
 * @returns {Real} The device handle, or 0 if the connection handle is not valid.
 * @example
 * ```gml
 * var _device = bluetooth_classic_connection_get_device(connection);
 * if (_device != 0)
 * {
 *     show_debug_message($"Connected to {bluetooth_device_get_name(_device)}");
 * }
 * ```
 * The above code gets the device of the connection stored in `connection` and shows its name with
 * ${function.bluetooth_device_get_name}.
 * @function_end
 */

/**
 * @function bluetooth_classic_send
 * @desc This function queues the given number of bytes from the given buffer, starting at the given
 * offset, to be sent over the given RFCOMM connection. The bytes are copied when you call the
 * function, so you can change or delete the buffer straight away. The function returns as soon as
 * the bytes are queued, without waiting for them to go out, and there is no callback: a send that
 * later fails ends the connection, and the callback set with
 * ${function.bluetooth_set_callback_classic_disconnected} reports the error.
 *
 * At most 1 MiB may wait to be sent on a connection. A send that would take the queue past that
 * returns ${constant.BluetoothError}.Busy and queues nothing, so send more than that in parts and
 * try again once the queue drains. A size of 0 returns ${constant.BluetoothError}.Ok and sends
 * nothing.
 *
 * An RFCOMM connection is a stream and has no message boundaries: the device may receive the bytes
 * of one send in several chunks, or the bytes of several sends in one. If your data is made of
 * messages, add your own framing, such as a length prefix or a line ending.
 *
 * The function returns a ${constant.BluetoothError}. When it is not
 * ${constant.BluetoothError}.Ok nothing was queued, and the reason is in
 * ${function.bluetooth_last_error_code} and ${function.bluetooth_last_error_message}:
 * NotInitialized, InvalidArgument when the offset and size go past the end of the buffer,
 * InvalidHandle for a handle that is not valid, Disconnected when the connection is still
 * connecting, closing or has ended, Busy when the queue is full, or NotSupported where Classic is not
 * available.
 * @param {Real} connection The connection handle to send on.
 * @param {Buffer} data The buffer that holds the bytes to send.
 * @param {Real} offset The position in the buffer of the first byte to send (in bytes).
 * @param {Real} size The number of bytes to send.
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the bytes were queued, otherwise the
 * reason they were not.
 * @example
 * ```gml
 * var _buffer = buffer_create(64, buffer_grow, 1);
 * buffer_write(_buffer, buffer_text, "PING\n");
 * var _size = buffer_tell(_buffer);
 *
 * var _result = bluetooth_classic_send(connection, _buffer, 0, _size);
 * buffer_delete(_buffer);
 *
 * if (_result == BluetoothError.Busy)
 * {
 *     show_debug_message("Send queue is full, try again later");
 * }
 * else if (_result != BluetoothError.Ok)
 * {
 *     show_debug_message($"Send failed: {bluetooth_last_error_message()}");
 * }
 * ```
 * The above code writes the text "PING" and a line ending into a new buffer and queues those bytes
 * on the connection stored in `connection`. The buffer is deleted straight after the call, since the
 * bytes have already been copied. A full queue and any other failure are reported with a debug
 * message. The line ending marks the end of the message for the receiving side, since the stream
 * itself has no message boundaries.
 * @function_end
 */

/**
 * @function bluetooth_classic_server_start
 * @desc This function starts an RFCOMM server that offers a service with the given name and UUID to
 * other devices. The service is published in the SDP records of this device, so a remote device can
 * connect to it by its UUID. Every incoming connection calls the callback set with
 * ${function.bluetooth_set_callback_classic_client_connected} with its new connection handle; set
 * that callback before you start the server. Only one server can run at a time.
 *
 * An unpaired device can only find this device with a Classic scan while it is discoverable: see
 * ${function.bluetooth_classic_discoverable_start}.
 *
 * The function returns a ${constant.BluetoothError}. When it is not
 * ${constant.BluetoothError}.Ok the server did not start, and the reason is in
 * ${function.bluetooth_last_error_code} and ${function.bluetooth_last_error_message}:
 * NotInitialized, InvalidArgument for a service UUID that is not valid, Busy while a server is
 * already running (stop it first to change its name or UUID), PermissionDenied when the Bluetooth
 * permission is missing or was refused, BluetoothDisabled when the radio is off, NotSupported where
 * the Classic server is not available, or OperationFailed when the system refused to publish the
 * service.
 *
 * [[Note: On Windows and Android an empty name is published as "GMBluetooth RFCOMM", and incoming
 * links are secure (authenticated and encrypted) RFCOMM links. On Android the permission is the
 * Bluetooth connect permission. macOS has no API to require a secure link, so incoming links there
 * use the system's default security, and it returns PermissionDenied until ${function.bluetooth_permission_request}
 * has been called.]]
 * @param {String} name The name of the service, as other devices see it.
 * @param {String} service_uuid The UUID of the service, in the full 36-character form or as 4 or 8
 * hex digits.
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the server started, otherwise the
 * reason it did not.
 * @example
 * ```gml
 * bluetooth_set_callback_classic_client_connected(function(_connection, _device)
 * {
 *     show_debug_message($"Client connected: {bluetooth_device_get_name(_device)}");
 *     client_connection = _connection;
 * });
 *
 * var _result = bluetooth_classic_server_start("My Game", "6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
 * if (_result != BluetoothError.Ok)
 * {
 *     show_debug_message($"Server did not start: {bluetooth_last_error_message()}");
 * }
 * ```
 * The above code first sets the callback that is called for each incoming connection, which keeps
 * the new connection handle in the instance's `client_connection` variable. It then starts a server
 * named "My Game" with a custom service UUID, and shows the reason in a debug message when the
 * server could not start.
 * @function_end
 */

/**
 * @function bluetooth_classic_server_stop
 * @desc This function stops the running RFCOMM server: the service is removed from the SDP records
 * and no more incoming connections are accepted. Connections the server has already accepted stay
 * open; end them with ${function.bluetooth_classic_disconnect}. Stopping when no server is running
 * returns ${constant.BluetoothError}.Ok.
 *
 * The function returns ${constant.BluetoothError}.NotInitialized before
 * ${function.bluetooth_initialize}, with the reason in ${function.bluetooth_last_error_message}.
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the server is stopped, otherwise
 * the reason.
 * @example
 * ```gml
 * bluetooth_classic_server_stop();
 * if (bluetooth_classic_connection_is_valid(client_connection))
 * {
 *     bluetooth_classic_disconnect(client_connection);
 * }
 * ```
 * The above code stops the server and then ends the connection it accepted, which is stored in the
 * instance's `client_connection` variable.
 * @function_end
 */

/**
 * @function bluetooth_classic_server_is_running
 * @desc This function checks whether the RFCOMM server started with
 * ${function.bluetooth_classic_server_start} is running.
 * @returns {Bool} `true` if the server is running, otherwise `false`.
 * @example
 * ```gml
 * if (!bluetooth_classic_server_is_running())
 * {
 *     bluetooth_classic_server_start("My Game", "6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
 * }
 * ```
 * The above code starts the server only when it is not already running.
 * @function_end
 */

/**
 * @function bluetooth_classic_discoverable_start
 * @desc This function makes this device discoverable to the Classic scans of other devices for the
 * given number of seconds, so that an unpaired device can find it and connect to the service started
 * with ${function.bluetooth_classic_server_start}. Use ${function.bluetooth_feature_is_supported}
 * with ${constant.BluetoothFeature}.ClassicDiscoverable to check whether the platform can do this.
 *
 * The function returns a ${constant.BluetoothError}. When it is not
 * ${constant.BluetoothError}.Ok the reason is in ${function.bluetooth_last_error_code} and
 * ${function.bluetooth_last_error_message}: NotInitialized, PermissionDenied when the Bluetooth
 * permission is missing or was refused, BluetoothDisabled when the radio is off, NotSupported where
 * discoverability cannot be controlled, or OperationFailed when the system refused.
 *
 * [[Note: On Windows the device becomes discoverable at once and stays so for the given duration,
 * or until ${function.bluetooth_classic_discoverable_stop} when the duration is 0 or less. Incoming
 * connections are also turned on if they were off, and turned back off when discoverability ends.
 * Calling this function again replaces the previous duration. It returns NotSupported when no
 * Bluetooth radio is found.]]
 *
 * [[Note: On Android the user is shown a system prompt asking to make the device discoverable, and
 * ${constant.BluetoothError}.Ok only means that the prompt was shown, not that the user accepted:
 * check ${function.bluetooth_classic_discoverable_is_running}. The duration is clamped to 1 to 3600
 * seconds, and 0 or less asks for the Android default of 120 seconds; there is no indefinite
 * duration. The permission is the Bluetooth advertise permission.]]
 *
 * [[Note: macOS and iOS have no public API for this, and the function returns NotSupported.]]
 * @param {Real} duration_seconds How long the device stays discoverable (in seconds), or 0 for no
 * limit where the platform allows it.
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the device was made discoverable
 * (or, on Android, the prompt was shown), otherwise the reason.
 * @example
 * ```gml
 * if (bluetooth_feature_is_supported(BluetoothFeature.ClassicDiscoverable))
 * {
 *     var _result = bluetooth_classic_discoverable_start(120);
 *     if (_result != BluetoothError.Ok)
 *     {
 *         show_debug_message($"Could not become discoverable: {bluetooth_last_error_message()}");
 *     }
 * }
 * ```
 * The above code asks for the device to be discoverable for two minutes when the platform supports
 * it, and shows the reason in a debug message when the request failed.
 * @function_end
 */

/**
 * @function bluetooth_classic_discoverable_stop
 * @desc This function ends discoverability early, before the duration given to
 * ${function.bluetooth_classic_discoverable_start} has passed. Use
 * ${function.bluetooth_feature_is_supported} with ${constant.BluetoothFeature}.ClassicDiscoverableStop
 * to check whether the platform can do this.
 *
 * The function returns a ${constant.BluetoothError}: NotInitialized before
 * ${function.bluetooth_initialize}, or NotSupported where discoverability cannot be ended early,
 * with the reason in ${function.bluetooth_last_error_message}.
 *
 * [[Note: Only Windows can end discoverability early; it also turns incoming connections back off
 * if ${function.bluetooth_classic_discoverable_start} turned them on. Android has no API for this
 * and returns NotSupported, and discoverability ends on its own when its duration expires. macOS
 * and iOS return NotSupported.]]
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if discoverability was ended,
 * otherwise the reason.
 * @example
 * ```gml
 * if (bluetooth_feature_is_supported(BluetoothFeature.ClassicDiscoverableStop))
 * {
 *     bluetooth_classic_discoverable_stop();
 * }
 * ```
 * The above code ends discoverability only on a platform that can end it early.
 * @function_end
 */

/**
 * @function bluetooth_classic_discoverable_is_running
 * @desc This function checks whether this device is currently discoverable to Classic scans. It reads
 * the state from the system, so it also reflects a change the user made outside the game.
 *
 * [[Note: On Android this function needs the Bluetooth scan permission on Android 12 and later;
 * without it the function returns `false` and sets ${function.bluetooth_last_error_code} to
 * PermissionDenied. On macOS and iOS it always returns `false`.]]
 * @returns {Bool} `true` if the device is discoverable, otherwise `false`.
 * @example
 * ```gml
 * discoverable_text = bluetooth_classic_discoverable_is_running() ? "Visible to other devices" : "Hidden";
 * ```
 * The above code sets the instance's `discoverable_text` variable to a status string that depends on
 * whether the device is discoverable.
 * @function_end
 */

/**
 * @function bluetooth_set_callback_classic_client_connected
 * @desc This function sets the function to call when a remote device connects to the RFCOMM server
 * started with ${function.bluetooth_classic_server_start}. The callback receives the handle of the
 * new connection, which you use with ${function.bluetooth_classic_send} and
 * ${function.bluetooth_classic_disconnect}, and the handle of the device that connected. Setting a
 * new function replaces the previous one.
 *
 * You can set the callback before ${function.bluetooth_initialize}, and it stays set across
 * ${function.bluetooth_shutdown}. Set it before you start the server: a connection accepted while
 * no callback is set is still made, and its data arrives through the callback set with
 * ${function.bluetooth_set_callback_classic_data}.
 * @param {Function} callback The function to call for each incoming connection.
 * @returns {Bool} Always `true`.
 * @event callback
 * @desc Called once for each connection the server accepts.
 * @member {Real} connection The handle of the new connection.
 * @member {Real} device The handle of the device that connected.
 * @event_end
 * @example
 * ```gml
 * bluetooth_set_callback_classic_client_connected(function(_connection, _device)
 * {
 *     show_debug_message($"{bluetooth_device_get_name(_device)} connected");
 *     array_push(clients, _connection);
 * });
 * ```
 * The above code sets the callback for incoming server connections. It shows the name of each
 * device that connects and adds the new connection handle to the instance's `clients` array.
 * @function_end
 */

/**
 * @function bluetooth_remove_callback_classic_client_connected
 * @desc This function removes the function set with
 * ${function.bluetooth_set_callback_classic_client_connected}. The server keeps accepting
 * connections, but they are no longer reported.
 * @returns {Bool} Always `true`.
 * @example
 * ```gml
 * bluetooth_remove_callback_classic_client_connected();
 * ```
 * The above code stops incoming server connections from being reported.
 * @function_end
 */

/**
 * @function bluetooth_set_callback_classic_data
 * @desc This function sets the function to call for every chunk of bytes an RFCOMM connection
 * receives, in the order they arrive. The callback receives the connection, a value handle that
 * holds the bytes, and their number. You copy the bytes into a buffer of your own with
 * ${function.bluetooth_value_fetch}, which also frees the value, or free it without copying with
 * ${function.bluetooth_value_release}. Setting a new function replaces the previous one, and you can
 * set it before ${function.bluetooth_initialize}.
 *
 * An RFCOMM connection is a stream and has no message boundaries: the bytes of one send from the
 * device may arrive in several chunks, and the bytes of several sends may arrive in one. Collect the
 * chunks in order and split them with your own framing.
 *
 * The values a connection has not fetched or released count toward its receive budget of 1 MiB.
 * While they hold more than that, Windows and Android stop reading from the device until some are
 * fetched or released, and RFCOMM flow control holds the device back. A value that is left
 * unfetched for 30 seconds expires, and the connection is then ended with
 * ${constant.BluetoothError}.Disconnected and the message "receive data expired", rather than leave
 * a gap in the stream. For a connection the device ends, the callback set with
 * ${function.bluetooth_set_callback_classic_disconnected} is called after its last chunk.
 *
 * [[Warning: While no callback is set, the bytes every connection receives are discarded.]]
 *
 * [[Note: macOS cannot pause an RFCOMM channel, so a chunk that would take the unfetched bytes past
 * 1 MiB ends the connection with ${constant.BluetoothError}.Disconnected and the message "receive
 * buffer full", after the part that fits is delivered.]]
 * @param {Function} callback The function to call for each chunk of received bytes.
 * @returns {Bool} Always `true`.
 * @event callback
 * @desc Called once for every chunk of bytes a connection receives, in order.
 * @member {Real} connection The handle of the connection that received the bytes.
 * @member {Real} value The value handle that holds the bytes, for ${function.bluetooth_value_fetch}.
 * @member {Real} size The number of bytes the value holds.
 * @event_end
 * @example
 * ```gml
 * bluetooth_set_callback_classic_data(function(_connection, _value, _size)
 * {
 *     if (receive_length + _size > buffer_get_size(receive_buffer))
 *     {
 *         buffer_resize(receive_buffer, (receive_length + _size) * 2);
 *     }
 *
 *     var _copied = bluetooth_value_fetch(_value, receive_buffer, receive_length);
 *     if (_copied < 0)
 *     {
 *         show_debug_message($"Fetch failed: {bluetooth_last_error_message()}");
 *         exit;
 *     }
 *     receive_length += _copied;
 * });
 * ```
 * The above code sets the callback for received Classic data. The instance keeps a buffer in its
 * `receive_buffer` variable and the number of bytes it holds in `receive_length`. For each chunk the
 * buffer is first made large enough with `buffer_resize`, since ${function.bluetooth_value_fetch}
 * writes straight into the buffer's memory and never grows it. The bytes are then copied to the end
 * of the data received so far and the count is advanced by the number of bytes copied. The game can
 * then look for complete messages in the first `receive_length` bytes of the buffer.
 * @function_end
 */

/**
 * @function bluetooth_remove_callback_classic_data
 * @desc This function removes the function set with
 * ${function.bluetooth_set_callback_classic_data}. Note that while no callback is set, the bytes
 * every connection receives are discarded.
 * @returns {Bool} Always `true`.
 * @example
 * ```gml
 * bluetooth_remove_callback_classic_data();
 * ```
 * The above code removes the callback for received Classic data.
 * @function_end
 */

/**
 * @function bluetooth_set_callback_classic_disconnected
 * @desc This function sets the function to call when an established RFCOMM connection ends without
 * ${function.bluetooth_classic_disconnect}: the device closed it, the link failed, a send failed, or
 * the connection was ended because its received data expired or, on macOS, filled the receive
 * budget (see ${function.bluetooth_set_callback_classic_data}). The callback receives the reason and
 * the connection. Setting a new function replaces the previous one, and you can set it before
 * ${function.bluetooth_initialize}.
 *
 * The callback is called after the last chunk of data the connection received, and the connection
 * handle is no longer valid once it is called; the values already delivered can still be fetched. A
 * disconnect you ask for with ${function.bluetooth_classic_disconnect} never calls it, and neither
 * does a connect that fails, which reports through the callback of
 * ${function.bluetooth_classic_connect}.
 * @param {Function} callback The function to call when a connection ends.
 * @returns {Bool} Always `true`.
 * @event callback
 * @desc Called once when an established connection ends without
 * ${function.bluetooth_classic_disconnect}.
 * @member {Enum.BluetoothError} error_code ${constant.BluetoothError}.Disconnected when the device
 * closed the connection or its received data could not be held, otherwise the error that ended it.
 * @member {String} message A description of why the connection ended.
 * @member {Real} connection The handle of the connection that ended.
 * @event_end
 * @example
 * ```gml
 * bluetooth_set_callback_classic_disconnected(function(_error_code, _message, _connection)
 * {
 *     show_debug_message($"Connection {_connection} ended: {_message}");
 *     if (_connection == connection)
 *     {
 *         connection = 0;
 *     }
 * });
 * ```
 * The above code sets the callback for connections that end on their own. It shows why the
 * connection ended and clears the instance's `connection` variable when it held that handle.
 * @function_end
 */

/**
 * @function bluetooth_remove_callback_classic_disconnected
 * @desc This function removes the function set with
 * ${function.bluetooth_set_callback_classic_disconnected}. Connections that end are then no longer
 * reported, but their handles still stop being valid.
 * @returns {Bool} Always `true`.
 * @example
 * ```gml
 * bluetooth_remove_callback_classic_disconnected();
 * ```
 * The above code removes the callback for ended Classic connections.
 * @function_end
 */

/**
 * @module classic
 * @title Classic
 * @desc The functions to discover Bluetooth Classic devices, open RFCOMM connections to them, run an
 * RFCOMM server, make this device discoverable, and send and receive the bytes of a connection.
 *
 * Bluetooth Classic is available on Windows, Android and macOS. On iOS the support queries return
 * `false` and the calls that start Classic work return ${constant.BluetoothError}.NotSupported. On
 * macOS Classic goes through IOBluetooth.
 *
 * An RFCOMM connection is a byte stream with no message boundaries. Received bytes arrive as values
 * through the callback set with ${function.bluetooth_set_callback_classic_data} and are copied with
 * ${function.bluetooth_value_fetch}; the failure codes are those of ${constant.BluetoothError}.
 *
 * @section_func
 * @desc These are the functions of Bluetooth Classic.
 * @ref bluetooth_classic_is_supported
 * @ref bluetooth_classic_server_is_supported
 * @ref bluetooth_classic_scan_start
 * @ref bluetooth_classic_scan_stop
 * @ref bluetooth_classic_scan_is_running
 * @ref bluetooth_classic_connect
 * @ref bluetooth_classic_disconnect
 * @ref bluetooth_classic_connection_is_valid
 * @ref bluetooth_classic_connection_is_connected
 * @ref bluetooth_classic_connection_get_device
 * @ref bluetooth_classic_send
 * @ref bluetooth_classic_server_start
 * @ref bluetooth_classic_server_stop
 * @ref bluetooth_classic_server_is_running
 * @ref bluetooth_classic_discoverable_start
 * @ref bluetooth_classic_discoverable_stop
 * @ref bluetooth_classic_discoverable_is_running
 * @ref bluetooth_set_callback_classic_client_connected
 * @ref bluetooth_remove_callback_classic_client_connected
 * @ref bluetooth_set_callback_classic_data
 * @ref bluetooth_remove_callback_classic_data
 * @ref bluetooth_set_callback_classic_disconnected
 * @ref bluetooth_remove_callback_classic_disconnected
 * @section_end
 *
 * @module_end
 */
