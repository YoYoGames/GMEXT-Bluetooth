// Bluetooth Classic (RFCOMM): discovery, client connections, sending and receiving, the server and
// discoverability, each with the event callbacks of the call that starts it.

/**
 * @function bluetooth_classic_scan_start
 * @desc This function starts a Bluetooth Classic discovery (an inquiry scan) for nearby Classic
 * devices. Every device the scan sees enters the device cache, with the transport
 * ${constant.BluetoothTransport}.Classic, and the scan calls its own callbacks for it: `on_found`
 * the first time this scan sees the device, also when an earlier scan already put it in the cache,
 * and `on_updated` each time the device answers the inquiry again or its name is resolved later.
 *
 * The scan keeps its callbacks until it ends. It ends on its own after one inquiry, and `on_stopped`
 * is then called once; it is also called when the inquiry could not run or the radio is turned off,
 * with the reason. A scan you stop with ${function.bluetooth_classic_scan_stop} calls nothing. Only
 * a Classic scan reports Classic devices, and a device that enters the cache in another way calls no
 * scan callback.
 *
 * Only one Classic scan runs at a time: a start while one is running returns
 * ${constant.BluetoothError}.Busy.
 *
 * The function returns a ${constant.BluetoothError}. When it is not
 * ${constant.BluetoothError}.Ok the scan did not start, no callback is called, and the reason is in
 * ${function.bluetooth_last_error_code} and ${function.bluetooth_last_error_message}:
 * NotInitialized before ${function.bluetooth_initialize}, Busy while a Classic scan runs,
 * PermissionDenied when the Bluetooth permission is missing or was refused, BluetoothDisabled when
 * the radio is off, NotSupported where Classic is not available, or OperationFailed when the system
 * could not start the inquiry.
 *
 * [[Note: An inquiry runs for about 5 seconds on Windows, which reports every device at the end of
 * it, and for about 10 seconds on macOS. Android and macOS report devices as they answer, and on
 * Android the inquiry lasts as long as the system's discovery.]]
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
 * @param {Function} on_found The function to call the first time this scan sees a device.
 * @param {Function} [on_updated] The function to call when a device this scan has reported answers
 * again or its name is resolved, or `undefined`.
 * @param {Function} [on_stopped] The function to call when the scan ends without
 * ${function.bluetooth_classic_scan_stop}, or `undefined`.
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the scan started, otherwise the
 * reason it did not.
 * @event callback:on_found
 * @desc Called once for each device, the first time this scan sees it.
 * @member {Real} device The handle of the device.
 * @event_end
 * @event callback:on_updated
 * @desc Called each time a device this scan has already reported answers the inquiry again or has
 * its name resolved.
 * @member {Real} device The handle of the device.
 * @event_end
 * @event callback:on_stopped
 * @desc Called once, when the scan ends without ${function.bluetooth_classic_scan_stop}, also when
 * the inquiry has run its course. It is the last callback of the scan.
 * @member {Enum.BluetoothError} error_code ${constant.BluetoothError}.Ok when the inquiry ran its
 * course, otherwise the reason the scan ended, such as ${constant.BluetoothError}.BluetoothDisabled
 * when the radio was turned off.
 * @member {String} message A description of why the scan ended, or an empty string when the inquiry
 * ran its course.
 * @event_end
 * @example
 * ```gml
 * var _error = bluetooth_classic_scan_start(
 *     function(_device)
 *     {
 *         array_push(devices, _device);
 *     },
 *     function(_device)
 *     {
 *         show_debug_message($"Updated: {bluetooth_device_get_name(_device)}");
 *     },
 *     function(_error_code, _message)
 *     {
 *         if (_error_code != BluetoothError.Ok)
 *         {
 *             show_debug_message($"Classic scan ended: {_message}");
 *         }
 *         scanning = false;
 *     });
 *
 * scanning = (_error == BluetoothError.Ok);
 * if (!scanning)
 * {
 *     show_debug_message($"Classic scan did not start: {bluetooth_last_error_message()}");
 * }
 * ```
 * The above code starts a Classic scan. Each device it finds is added to the instance's `devices`
 * array, and a device whose name arrives later is shown in a debug message. When the inquiry ends,
 * the instance's `scanning` flag is cleared, and an error is shown when the scan ended for another
 * reason. If the scan could not start, the reason is shown in a debug message.
 * @function_end
 */

/**
 * @function bluetooth_classic_scan_stop
 * @desc This function stops a running Bluetooth Classic scan. The scan's `on_stopped` callback is not
 * called for a stop you ask for. Stopping when no scan is running returns
 * ${constant.BluetoothError}.Ok and does nothing.
 *
 * The function returns ${constant.BluetoothError}.NotInitialized before
 * ${function.bluetooth_initialize}, with the reason in ${function.bluetooth_last_error_message}.
 *
 * [[Note: Windows cannot cut an inquiry short. The scan is stopped at once and nothing the inquiry
 * still finds is reported.]]
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the scan is stopped or none was
 * running, otherwise the reason.
 * @example
 * ```gml
 * if (bluetooth_classic_scan_is_running())
 * {
 *     bluetooth_classic_scan_stop();
 *     scanning = false;
 * }
 * ```
 * The above code stops the Classic scan if one is running and clears the instance's `scanning`
 * flag, since no callback reports the stop.
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
 * away, before the device has answered, and `callback` is called once when the connection is made
 * or fails.
 *
 * The function returns 0 when the connect could not be started, and the reason is then in
 * ${function.bluetooth_last_error_code} and ${function.bluetooth_last_error_message}:
 * NotInitialized, InvalidHandle for a device handle that is not valid, InvalidArgument for an LE
 * device, a service UUID that is not valid or options that are not valid, BluetoothDisabled,
 * PermissionDenied, or NotSupported where Classic is not available. In that case no callback is
 * ever called.
 *
 * The given `options` is a ${struct.BluetoothConnectOptions}, or `undefined` for the defaults. Its
 * `timeout_ms`, 15 seconds by default, is one window from the call to an open link, a pairing prompt
 * included: a connect that is not open by then fails with ${constant.BluetoothError}.Timeout on
 * every platform. One that is refused or fails reports ${constant.BluetoothError}.ConnectionFailed.
 * You can cancel a connect that is still in progress by passing the returned handle to
 * ${function.bluetooth_classic_disconnect}, and `callback` is then called once with
 * ${constant.BluetoothError}.ConnectionFailed. If ${function.bluetooth_shutdown} is called first, it
 * is called with ${constant.BluetoothError}.NotInitialized, and if the radio turns off first, with
 * ${constant.BluetoothError}.BluetoothDisabled.
 *
 * ### Receiving
 *
 * Once connected, `on_data` is called for every chunk of bytes the connection receives, in the order
 * they arrive, with a value handle that holds the bytes and their number. Copy the bytes into a
 * buffer of your own with ${function.bluetooth_value_append} or ${function.bluetooth_value_fetch},
 * which also free the value, or free it without copying with ${function.bluetooth_value_release}. An
 * RFCOMM connection is a stream and has no message boundaries: the bytes of one send from the device
 * may arrive in several chunks, and the bytes of several sends may arrive in one, so collect the
 * chunks in order and split them with your own framing.
 *
 * The values a connection has not fetched or released count toward its receive budget of 1 MiB.
 * While they hold more than that, Windows and Android stop reading from the device until some are
 * fetched or released, and RFCOMM flow control holds the device back. A value that is left
 * unfetched for 30 seconds expires, and the connection is then ended with
 * ${constant.BluetoothError}.Disconnected and the message "receive data expired", rather than leave
 * a gap in the stream.
 *
 * `on_disconnected` is called once when the open link ends without
 * ${function.bluetooth_classic_disconnect}: the device closed it, the link failed, a send failed,
 * the radio was turned off, or the connection was ended because its received data expired or, on
 * macOS, filled the receive budget. It is called after the last `on_data` of the connection, and the
 * connection handle is no longer valid once it is called; the values already delivered can still be
 * fetched. A disconnect you ask for never calls it, and neither does a connect that fails, which
 * reports through `callback`.
 *
 * [[Note: macOS cannot pause an RFCOMM channel, so a chunk that would take the unfetched bytes past
 * 1 MiB ends the connection with ${constant.BluetoothError}.Disconnected and the message "receive
 * buffer full", after the part that fits is delivered.]]
 *
 * [[Note: On macOS the RFCOMM channel is found by looking up the service UUID in the device's SDP
 * records, through IOBluetooth: the records cached from pairing when they hold the service,
 * otherwise an SDP query. A device that does not offer the service fails with
 * ${constant.BluetoothError}.ConnectionFailed, as on every platform. The call itself returns 0 with
 * NotFound when macOS cannot resolve the device's address, and with PermissionDenied until
 * ${function.bluetooth_permission_request} has been called.]]
 *
 * [[Note: The connection is a secure (authenticated and encrypted) RFCOMM link, so an unpaired device
 * may show a pairing prompt, which counts toward the timeout. On Android a running Classic scan is
 * cancelled before the connect.]]
 * @param {Real} device The handle of the Classic device to connect to.
 * @param {String} service_uuid The UUID of the RFCOMM service to connect to.
 * @param {Struct.BluetoothConnectOptions} [options] How the connect runs, or `undefined` for the
 * defaults.
 * @param {Function} on_data The function to call for each chunk of bytes the connection receives.
 * @param {Function} on_disconnected The function to call once if the open connection ends without
 * ${function.bluetooth_classic_disconnect}.
 * @param {Function} callback The function to call once the connection is made or fails.
 * @returns {Real} The new connection handle, or 0 if the connect did not start.
 * @event callback:on_data
 * @desc Called once for every chunk of bytes the connection receives, in order.
 * @member {Real} connection The handle of the connection that received the bytes.
 * @member {Real} value The value handle that holds the bytes, for ${function.bluetooth_value_fetch}.
 * @member {Real} size The number of bytes the value holds.
 * @event_end
 * @event callback:on_disconnected
 * @desc Called once, when the open connection ends without
 * ${function.bluetooth_classic_disconnect}. It is the last callback of the connection.
 * @member {Enum.BluetoothError} error_code ${constant.BluetoothError}.Disconnected when the device
 * closed the connection or its received data could not be held, ${constant.BluetoothError}.BluetoothDisabled
 * when the radio was turned off, otherwise the error that ended it.
 * @member {String} message A description of why the connection ended.
 * @member {Real} connection The handle of the connection that ended.
 * @event_end
 * @event callback:callback
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
 * receive_buffer = buffer_create(1024, buffer_grow, 1);
 *
 * connection = bluetooth_classic_connect(device, "00001101-0000-1000-8000-00805F9B34FB", undefined,
 *     function(_connection, _value, _size)
 *     {
 *         if (_connection != connection)
 *         {
 *             bluetooth_value_release(_value);
 *             return;
 *         }
 *         if (bluetooth_value_append(_value, _size, receive_buffer) != BluetoothError.Ok)
 *         {
 *             show_debug_message($"Receive failed: {bluetooth_last_error_message()}");
 *         }
 *     },
 *     function(_error_code, _message, _connection)
 *     {
 *         if (_connection != connection) return;
 *         show_debug_message($"Connection ended: {_message}");
 *         connection = 0;
 *     },
 *     function(_error_code, _message, _connection, _device)
 *     {
 *         if (_connection != connection) return;
 *         if (_error_code != BluetoothError.Ok)
 *         {
 *             show_debug_message($"Connect failed: {_message}");
 *             connection = 0;
 *             return;
 *         }
 *         show_debug_message($"Connected to {bluetooth_device_get_name(_device)}");
 *     });
 *
 * if (connection == 0)
 * {
 *     show_debug_message($"Connect did not start: {bluetooth_last_error_message()}");
 * }
 * ```
 * The above code creates a grow buffer for the received bytes and starts a connection to the Serial
 * Port Profile service of the device stored in the instance's `device` variable, with the default
 * options, keeping the returned handle in `connection`. Every callback first checks that the
 * connection it reports is still the one stored in `connection`, so a callback for an older
 * connection the instance has since replaced is ignored, and a chunk for such a connection is freed
 * with ${function.bluetooth_value_release}. Each chunk for the current connection is added to the
 * end of `receive_buffer` with ${function.bluetooth_value_append}, which grows the buffer and moves
 * its used size. When the connect fails, or the open connection ends later, the reason is shown and
 * the handle is cleared; on success the name of the device is shown with
 * ${function.bluetooth_device_get_name}. If the connect could not be started at all, the returned
 * handle is 0 and the reason is read with ${function.bluetooth_last_error_message}.
 * @function_end
 */

/**
 * @function bluetooth_classic_disconnect
 * @desc This function ends the given RFCOMM connection, whether it is still connecting or already
 * connected. The disconnect is synchronous: the handle is no longer valid when the function returns,
 * and the connection's `on_disconnected` callback is not called. A connect that is still in progress
 * is cancelled, and its callback is called once with ${constant.BluetoothError}.ConnectionFailed.
 *
 * Values that the connection's `on_data` callback has already received can still be fetched with
 * ${function.bluetooth_value_fetch} after the disconnect.
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
 * from the moment ${function.bluetooth_classic_connect} returns it, or the server's `on_connected`
 * callback receives it, until the connect fails, ${function.bluetooth_classic_disconnect} is called,
 * the connection's `on_disconnected` callback is called, or ${function.bluetooth_shutdown} is
 * called. Handles are never reused.
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
 * later fails ends the connection, and the connection's `on_disconnected` callback reports the
 * error.
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
 * var _error = bluetooth_classic_send(connection, _buffer, 0, _size);
 * buffer_delete(_buffer);
 *
 * if (_error == BluetoothError.Busy)
 * {
 *     show_debug_message("Send queue is full, try again later");
 * }
 * else if (_error != BluetoothError.Ok)
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
 * connect to it by its UUID. Only one server can run at a time. It keeps `on_connected` and
 * `on_stopped` until it stops, and `on_data` and `on_disconnected` for as long as a connection it
 * accepted stays open, also after ${function.bluetooth_classic_server_stop}.
 *
 * `on_connected` is called for every incoming connection, with its new connection handle and the
 * device that connected; the device enters the device cache without calling any scan callback. The
 * server's `on_data` and `on_disconnected` serve every connection it accepts, as they do for
 * ${function.bluetooth_classic_connect}: you send on such a connection with
 * ${function.bluetooth_classic_send} and end it with ${function.bluetooth_classic_disconnect}.
 * `on_stopped` is called once if the server ends without ${function.bluetooth_classic_server_stop},
 * for example with ${constant.BluetoothError}.BluetoothDisabled when the radio is turned off, after
 * each connection it accepted has called `on_disconnected` with
 * ${constant.BluetoothError}.BluetoothDisabled. Stopping the server yourself leaves the connections it
 * accepted open, with their callbacks.
 *
 * An unpaired device can only find this device with a Classic scan while it is discoverable: see
 * ${function.bluetooth_classic_discoverable_start}.
 *
 * The function returns a ${constant.BluetoothError}. When it is not
 * ${constant.BluetoothError}.Ok the server did not start, no callback is called, and the reason is
 * in ${function.bluetooth_last_error_code} and ${function.bluetooth_last_error_message}:
 * NotInitialized, InvalidArgument for a service UUID that is not valid, Busy while a server is
 * already running (stop it first to change its name, UUID or callbacks), PermissionDenied when the
 * Bluetooth permission is missing or was refused, BluetoothDisabled when the radio is off,
 * NotSupported where the Classic server is not available, or OperationFailed when the system refused
 * to publish the service.
 *
 * [[Note: On Windows and Android an empty name is published as "GMBluetooth RFCOMM", and incoming
 * links are secure (authenticated and encrypted) RFCOMM links. On Android the permission is the
 * Bluetooth connect permission. macOS has no API to require a secure link, so incoming links there
 * use the system's default security, and it returns PermissionDenied until
 * ${function.bluetooth_permission_request} has been called.]]
 * @param {String} name The name of the service, as other devices see it.
 * @param {String} service_uuid The UUID of the service, in the full 36-character form or as 4 or 8
 * hex digits.
 * @param {Function} on_connected The function to call for each incoming connection.
 * @param {Function} on_data The function to call for each chunk of bytes an accepted connection
 * receives.
 * @param {Function} on_disconnected The function to call when an accepted connection ends without
 * ${function.bluetooth_classic_disconnect}.
 * @param {Function} [on_stopped] The function to call when the server ends without
 * ${function.bluetooth_classic_server_stop}, or `undefined`.
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the server started, otherwise the
 * reason it did not.
 * @event callback:on_connected
 * @desc Called once for each connection the server accepts.
 * @member {Real} connection The handle of the new connection.
 * @member {Real} device The handle of the device that connected.
 * @event_end
 * @event callback:on_data
 * @desc Called once for every chunk of bytes an accepted connection receives, in order.
 * @member {Real} connection The handle of the connection that received the bytes.
 * @member {Real} value The value handle that holds the bytes, for ${function.bluetooth_value_fetch}.
 * @member {Real} size The number of bytes the value holds.
 * @event_end
 * @event callback:on_disconnected
 * @desc Called once for each accepted connection that ends without
 * ${function.bluetooth_classic_disconnect}, after its last `on_data`.
 * @member {Enum.BluetoothError} error_code ${constant.BluetoothError}.Disconnected when the device
 * closed the connection or its received data could not be held, ${constant.BluetoothError}.BluetoothDisabled
 * when the radio was turned off, otherwise the error that ended it.
 * @member {String} message A description of why the connection ended.
 * @member {Real} connection The handle of the connection that ended.
 * @event_end
 * @event callback:on_stopped
 * @desc Called once, when the server ends without ${function.bluetooth_classic_server_stop}. It is
 * the last callback of the server itself.
 * @member {Enum.BluetoothError} error_code The reason the server ended, such as
 * ${constant.BluetoothError}.BluetoothDisabled when the radio was turned off.
 * @member {String} message A description of why the server ended.
 * @event_end
 * @example
 * ```gml
 * var _error = bluetooth_classic_server_start("My Game", "6E400001-B5A3-F393-E0A9-E50E24DCCA9E",
 *     function(_connection, _device)
 *     {
 *         show_debug_message($"Client connected: {bluetooth_device_get_name(_device)}");
 *         array_push(clients, _connection);
 *     },
 *     function(_connection, _value, _size)
 *     {
 *         if (array_get_index(clients, _connection) < 0)
 *         {
 *             bluetooth_value_release(_value);
 *             return;
 *         }
 *         if (bluetooth_value_append(_value, _size, receive_buffer) != BluetoothError.Ok)
 *         {
 *             show_debug_message($"Receive failed: {bluetooth_last_error_message()}");
 *         }
 *     },
 *     function(_error_code, _message, _connection)
 *     {
 *         var _index = array_get_index(clients, _connection);
 *         if (_index >= 0)
 *         {
 *             array_delete(clients, _index, 1);
 *         }
 *     },
 *     function(_error_code, _message)
 *     {
 *         show_debug_message($"Server ended: {_message}");
 *     });
 *
 * if (_error != BluetoothError.Ok)
 * {
 *     show_debug_message($"Server did not start: {bluetooth_last_error_message()}");
 * }
 * ```
 * The above code starts a server named "My Game" with a custom service UUID. Each incoming
 * connection is added to the instance's `clients` array, and removed again when it ends. The bytes
 * of a connection in the array are added to the instance's `receive_buffer` with
 * ${function.bluetooth_value_append}, and a chunk for any other connection is freed with
 * ${function.bluetooth_value_release}. When the server ends on its own, or could not start, the
 * reason is shown in a debug message.
 * @function_end
 */

/**
 * @function bluetooth_classic_server_stop
 * @desc This function stops the running RFCOMM server: the service is removed from the SDP records
 * and no more incoming connections are accepted. The server's `on_stopped` callback is not called
 * for a stop you ask for. Connections the server has already accepted stay open and keep their
 * `on_data` and `on_disconnected` callbacks; end them with ${function.bluetooth_classic_disconnect}.
 * Stopping when no server is running returns ${constant.BluetoothError}.Ok.
 *
 * The function returns ${constant.BluetoothError}.NotInitialized before
 * ${function.bluetooth_initialize}, with the reason in ${function.bluetooth_last_error_message}.
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the server is stopped or none was
 * running, otherwise the reason.
 * @example
 * ```gml
 * bluetooth_classic_server_stop();
 * for (var i = 0; i < array_length(clients); i++)
 * {
 *     bluetooth_classic_disconnect(clients[i]);
 * }
 * clients = [];
 * ```
 * The above code stops the server and then ends every connection it accepted, which are stored in
 * the instance's `clients` array, before emptying the array.
 * @function_end
 */

/**
 * @function bluetooth_classic_server_is_running
 * @desc This function checks whether the RFCOMM server started with
 * ${function.bluetooth_classic_server_start} is running.
 * @returns {Bool} `true` if the server is running, otherwise `false`.
 * @example
 * ```gml
 * server_text = bluetooth_classic_server_is_running() ? "Waiting for players" : "Server stopped";
 * ```
 * The above code sets the instance's `server_text` variable to a status string that depends on
 * whether the server is running.
 * @function_end
 */

/**
 * @function bluetooth_classic_discoverable_start
 * @desc This function makes this device discoverable to the Classic scans of other devices for the
 * given number of seconds, so that an unpaired device can find it and connect to the service started
 * with ${function.bluetooth_classic_server_start}. Use ${function.bluetooth_feature_is_supported}
 * with ${constant.BluetoothFeature}.ClassicDiscoverable to check whether the platform can do this.
 *
 * The duration is clamped to 1 to 3600 seconds, and 0 or less asks for 120 seconds; there is no
 * indefinite duration. The callback is called once: with ${constant.BluetoothError}.Ok and the
 * duration granted, with ${constant.BluetoothError}.PermissionDenied when the user declined, or with
 * ${constant.BluetoothError}.NotInitialized when ${function.bluetooth_shutdown} is called first.
 *
 * Once the device is discoverable, `on_stopped` is called when discoverability ends without
 * ${function.bluetooth_classic_discoverable_stop}: with ${constant.BluetoothError}.Ok when its time
 * runs out or the system ends it early, and with ${constant.BluetoothError}.BluetoothDisabled when
 * the radio is turned off.
 *
 * The function returns a ${constant.BluetoothError}. When it is not
 * ${constant.BluetoothError}.Ok the callback is never called, and the reason is in
 * ${function.bluetooth_last_error_code} and ${function.bluetooth_last_error_message}:
 * NotInitialized, Busy while the device is already discoverable, PermissionDenied when the Bluetooth
 * permission is missing or was refused, BluetoothDisabled when the radio is off, NotSupported where
 * discoverability cannot be controlled, or OperationFailed when the system refused.
 *
 * [[Note: On Windows the device becomes discoverable at once, the callback is called straight away
 * with the duration, and discoverability ends when it runs out or on
 * ${function.bluetooth_classic_discoverable_stop}. Incoming connections are also turned on if they
 * were off, and turned back off when discoverability ends. Windows can also end it early, for
 * example when the Bluetooth settings page is closed, and `on_stopped` hears it within a second. It
 * returns NotSupported when no Bluetooth radio is found.]]
 *
 * [[Note: On Android the user is shown a system prompt asking to make the device discoverable, and
 * the callback reports the answer. The permission is the Bluetooth advertise permission. Some
 * Android builds end discoverability again right after granting it (ColorOS did within 50 ms), so
 * the callback reports Ok and `on_stopped` follows at once. Without the Bluetooth scan permission
 * the end cannot be watched, and `on_stopped` is called when the granted time runs out.]]
 *
 * [[Note: macOS and iOS have no public API for this, and the function returns NotSupported.]]
 * @param {Real} duration_seconds How long the device stays discoverable (1-3600, in seconds), or 0
 * for 120 seconds.
 * @param {Function} callback The function to call once with the outcome.
 * @param {Function} [on_stopped] The function to call when discoverability ends without
 * ${function.bluetooth_classic_discoverable_stop}, or `undefined`.
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the request was made, otherwise the
 * reason it was not.
 * @event callback
 * @desc Called once, when the device is discoverable, the user declined, or Bluetooth is shut down
 * before the answer.
 * @member {Enum.BluetoothError} error_code ${constant.BluetoothError}.Ok if the device is
 * discoverable, otherwise the reason it is not.
 * @member {String} message A description of the failure, or an empty string on success.
 * @member {Real} duration_seconds The number of seconds the device stays discoverable.
 * @event_end
 * @event callback:on_stopped
 * @desc Called once, when discoverability that was granted ends without
 * ${function.bluetooth_classic_discoverable_stop}. It is the last callback of the request.
 * @member {Enum.BluetoothError} error_code ${constant.BluetoothError}.Ok when the time ran out or the
 * system ended it, ${constant.BluetoothError}.BluetoothDisabled when the radio was turned off.
 * @member {String} message A description of why discoverability ended.
 * @event_end
 * @example
 * ```gml
 * if (bluetooth_feature_is_supported(BluetoothFeature.ClassicDiscoverable))
 * {
 *     var _error = bluetooth_classic_discoverable_start(120, function(_error_code, _message, _duration_seconds)
 *     {
 *         if (_error_code != BluetoothError.Ok)
 *         {
 *             show_debug_message($"Not discoverable: {_message}");
 *             return;
 *         }
 *         show_debug_message($"Discoverable for {_duration_seconds} seconds");
 *     });
 *
 *     if (_error != BluetoothError.Ok)
 *     {
 *         show_debug_message($"Could not ask to become discoverable: {bluetooth_last_error_message()}");
 *     }
 * }
 * ```
 * The above code asks for the device to be discoverable for two minutes when the platform supports
 * it. The callback shows how long the device is discoverable for, or why it is not, for example when
 * the user declined the prompt on Android. If the request could not be made, the reason is shown in
 * a debug message.
 * @function_end
 */

/**
 * @function bluetooth_classic_discoverable_stop
 * @desc This function ends discoverability early, before the duration given to
 * ${function.bluetooth_classic_discoverable_start} has passed. Use
 * ${function.bluetooth_feature_is_supported} with ${constant.BluetoothFeature}.ClassicDiscoverableStop
 * to check whether the platform can do this. The `on_stopped` callback is not called for a stop you
 * ask for.
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
 * @desc This function checks whether the discoverable window this game started is still open. It reads
 * the state from the system, so it turns `false` as soon as the window ends, early or not, and a window
 * another app or the system settings opened does not count.
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
 * @module classic
 * @title Classic
 * @desc The functions to discover Bluetooth Classic devices, open RFCOMM connections to them, run an
 * RFCOMM server, make this device discoverable, and send and receive the bytes of a connection.
 *
 * Bluetooth Classic is available on Windows, Android and macOS. On iOS
 * ${function.bluetooth_feature_is_supported} returns `false` for ${constant.BluetoothFeature}.Classic
 * and ${constant.BluetoothFeature}.ClassicServer, and the calls that start Classic work return
 * ${constant.BluetoothError}.NotSupported. On macOS Classic goes through IOBluetooth.
 *
 * Each call that starts something takes the functions to call for its events and keeps them until it
 * ends: a scan its `on_found`, `on_updated` and `on_stopped`, a connection its `on_data` and
 * `on_disconnected`, the server its `on_connected` and `on_stopped`, and its `on_data` and
 * `on_disconnected` for as long as a connection it accepted stays open, also after the server stops.
 * Nothing is called for what you end yourself with a stop or a disconnect, and starting what is
 * already running returns ${constant.BluetoothError}.Busy.
 *
 * An RFCOMM connection is a byte stream with no message boundaries. Received bytes arrive as values
 * through `on_data` and are copied with ${function.bluetooth_value_append} or
 * ${function.bluetooth_value_fetch}; the failure codes are those of ${constant.BluetoothError}.
 *
 * @section_func
 * @desc These are the functions of Bluetooth Classic.
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
 * @section_end
 *
 * @module_end
 */
