/**
 * @function bluetooth_le_scan_start
 * @desc This function starts scanning for Bluetooth LE devices. Every device the scan sees enters the shared device cache, and the scan calls its own callbacks for it: `on_found` the first time this scan sees the device, also when an earlier scan already put it in the cache, and `on_updated` for each later advertisement in the same scan, at most once per `update_interval_ms` per device. Each advertisement updates the device's name, signal strength and advertisement, which ${function.bluetooth_device_get_name}, ${function.bluetooth_device_get_rssi} and ${function.bluetooth_device_get_advertisement} return.
 *
 * The scan keeps its callbacks until it ends. When it ends without ${function.bluetooth_le_scan_stop}, for example with ${constant.BluetoothError}.BluetoothDisabled when the radio is turned off, `on_stopped` is called once with the reason; a scan you stop yourself calls nothing. Only an LE scan reports LE devices, and a device that enters the cache in another way, such as through ${function.bluetooth_device_from_id}, calls no scan callback.
 *
 * The given `settings` is a ${struct.BluetoothLeScanSettings}, or `undefined` for an active scan in ${constant.BluetoothLeScanMode}.LowLatency mode with an update interval of 1000 milliseconds. The given `filters` array holds ${struct.BluetoothLeScanFilter} structs; `undefined` or an empty array reports every device. Otherwise the scan reports a device once its advertisement record, merged over every advertisement and scan response it has sent, matches any one of the filters.
 *
 * Only one LE scan runs at a time: a start while a scan is running returns ${constant.BluetoothError}.Busy, so stop the scan first to change its settings, filters or callbacks. While the Bluetooth state is still ${constant.BluetoothState}.Unknown the scan starts once the radio is ready.
 *
 * If the scan cannot start, the function returns a ${constant.BluetoothError} other than `Ok`: NotInitialized, InvalidArgument for a bad setting or filter (a filter that sets no field, a malformed UUID, a `company_id` outside 0-65535, data without its key, a mask of another length than its data, an `update_interval_ms` outside 0-60000, a `mode` that is not a ${constant.BluetoothLeScanMode}), Busy while a scan runs, PermissionDenied when the Bluetooth permission is missing or was refused, BluetoothDisabled when the radio is off or resetting, NotSupported when there is no Bluetooth LE hardware, or OperationFailed when the platform refused to start. The reason is in ${function.bluetooth_last_error_code} and ${function.bluetooth_last_error_message}, and no scan callback is called.
 *
 * [[Note: Android filters in the Bluetooth stack and allows 5 scan starts per 30 seconds: a sixth starts but finds nothing. It also pauses unfiltered scans while the screen is off, and on Android 6 to 11 a scan returns PermissionDenied while the system Location setting is off. With the manifest's `neverForLocation` flag it also leaves out some beacon advertisements. Only Android uses the `mode` of the settings (${constant.BluetoothFeature}.LeScanMode), and it reads a `mode` that is not a ${constant.BluetoothLeScanMode} as `undefined`, that is LowLatency.]]
 *
 * [[Note: Apple and Android always scan actively; only Windows can scan passively (${constant.BluetoothFeature}.LePassiveScan). iOS needs every filter to set `service_uuid` to scan in the background, and never reports iBeacon advertisements.]]
 *
 * [[Note: Windows 10 version 2004 and later, and Android 8 and later on a Bluetooth controller that supports extended advertising, also receive extended advertisements, such as long-range Coded PHY ones. Earlier Windows and Android versions receive legacy advertisements only, so a device that sends only extended advertisements is not found there.]]
 * @param {Struct.BluetoothLeScanSettings} [settings] How the scan runs, or `undefined` for the defaults.
 * @param {Array[Struct.BluetoothLeScanFilter]} [filters] The filters a device must match one of, or `undefined` or an empty array for every device.
 * @param {Function} on_found The function to call the first time this scan sees a device.
 * @param {Function} [on_updated] The function to call for a later advertisement of a device this scan has already reported, or `undefined`.
 * @param {Function} [on_stopped] The function to call when the scan ends without ${function.bluetooth_le_scan_stop}, or `undefined`.
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the scan started, otherwise the reason it did not.
 * @event callback:on_found
 * @desc Called once for each device, the first time this scan sees it.
 * @member {Real} device The handle of the device.
 * @event_end
 * @event callback:on_updated
 * @desc Called for a later advertisement of a device this scan has already reported, at most once per `update_interval_ms` per device.
 * @member {Real} device The handle of the device.
 * @event_end
 * @event callback:on_stopped
 * @desc Called once, when the scan ends without ${function.bluetooth_le_scan_stop}. It is the last callback of the scan.
 * @member {Enum.BluetoothError} error_code The reason the scan ended, such as ${constant.BluetoothError}.BluetoothDisabled when the radio was turned off.
 * @member {String} message A description of why the scan ended.
 * @event_end
 * @example
 * ```gml
 * var _settings = new BluetoothLeScanSettings();
 * _settings.update_interval_ms = 2000;
 *
 * var _filter = new BluetoothLeScanFilter();
 * _filter.service_uuid = "180d";
 *
 * var _error = bluetooth_le_scan_start(_settings, [_filter],
 *     function(_device)
 *     {
 *         show_debug_message($"Found {bluetooth_device_get_name(_device)}");
 *         array_push(devices, _device);
 *     },
 *     function(_device)
 *     {
 *         var _rssi = bluetooth_device_get_rssi(_device);
 *         if (_rssi != undefined)
 *         {
 *             show_debug_message($"{bluetooth_device_get_name(_device)}: {_rssi} dBm");
 *         }
 *     },
 *     function(_error_code, _message)
 *     {
 *         show_debug_message($"Scan ended: {_message}");
 *     });
 *
 * if (_error != BluetoothError.Ok)
 * {
 *     show_debug_message($"Scan did not start: {bluetooth_last_error_message()}");
 * }
 * ```
 * The above code creates a ${struct.BluetoothLeScanSettings} that reports a device's later advertisements at most every 2 seconds, and a ${struct.BluetoothLeScanFilter} that matches devices advertising the Heart Rate service (0x180D). It then starts the scan with three callbacks: `on_found` writes the name of each new device to the output log and adds its handle to the instance's `devices` array, `on_updated` writes the device's latest signal strength when it has one, and `on_stopped` writes why the scan ended on its own. If the scan could not start, the reason is read with ${function.bluetooth_last_error_message}.
 * @function_end
 */

/**
 * @function bluetooth_le_scan_stop
 * @desc This function stops the running Bluetooth LE scan. The scan's `on_stopped` callback is not called for a stop you ask for, and the scan calls none of its callbacks after this function returns. A stop while no scan is running returns ${constant.BluetoothError}.Ok and does nothing.
 *
 * The function returns ${constant.BluetoothError}.NotInitialized before ${function.bluetooth_initialize}, and on Windows OperationFailed when the platform could not stop its scanner; the scan counts as stopped in that case too. The reason is in ${function.bluetooth_last_error_message}.
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the scan is stopped or none was running, otherwise the reason.
 * @example
 * ```gml
 * if (bluetooth_le_scan_is_running())
 * {
 *     bluetooth_le_scan_stop();
 * }
 * ```
 * The above code stops the Bluetooth LE scan if one is running.
 * @function_end
 */

/**
 * @function bluetooth_le_scan_is_running
 * @desc This function checks whether a Bluetooth LE scan is running. It returns `false` before ${function.bluetooth_initialize}.
 * @returns {Bool} `true` if a Bluetooth LE scan is running, otherwise `false`.
 * @example
 * ```gml
 * scan_label = bluetooth_le_scan_is_running() ? "Scanning..." : "Idle";
 * ```
 * The above code sets the instance variable `scan_label` to a text that says whether a Bluetooth LE scan is running.
 * @function_end
 */

/**
 * @function bluetooth_le_connected_devices_query
 * @desc This function lists the Bluetooth LE devices that are connected to this system, by this game or by another app, and that expose one of the given services, without a scan. The callback is called once with an array of device handles. A device that is not in the device cache yet enters it without calling any scan callback. You can pass a handle from the list to ${function.bluetooth_le_connect}.
 *
 * Every UUID in the given `service_uuids` array is checked first. If the query cannot start, the function returns a ${constant.BluetoothError} other than `Ok`: NotInitialized, InvalidArgument for an empty array or a malformed UUID, PermissionDenied, BluetoothDisabled or NotSupported as for ${function.bluetooth_le_scan_start}, and the callback is never called. The reason is in ${function.bluetooth_last_error_message}.
 *
 * [[Note: Android and Windows cannot filter by service, so they list every connected LE device. Apple lists only the devices that expose one of the given services, and the radio must already be on (BluetoothDisabled otherwise).]]
 * @param {Array[String]} service_uuids The service UUIDs a device must expose one of; it must not be empty.
 * @param {Function} callback The function to call once with the list of connected devices.
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the query started, otherwise the reason it did not.
 * @event callback
 * @desc Called once, with the connected devices or the reason the query failed.
 * @member {Enum.BluetoothError} error_code ${constant.BluetoothError}.Ok if the query succeeded, otherwise the reason it failed.
 * @member {String} message A description of the failure, or an empty string on success.
 * @member {Array[Real]} devices The handles of the connected devices, empty on failure.
 * @event_end
 * @example
 * ```gml
 * var _error = bluetooth_le_connected_devices_query(["180d"], function(_error_code, _message, _devices)
 * {
 *     if (_error_code != BluetoothError.Ok)
 *     {
 *         show_debug_message($"Query failed: {_message}");
 *         return;
 *     }
 *     for (var _i = 0; _i < array_length(_devices); _i++)
 *     {
 *         show_debug_message($"Connected: {bluetooth_device_get_name(_devices[_i])}");
 *     }
 * });
 *
 * if (_error != BluetoothError.Ok)
 * {
 *     show_debug_message($"Query did not start: {bluetooth_last_error_message()}");
 * }
 * ```
 * The above code asks for the LE devices connected to the system that expose the Heart Rate service. The callback writes the reason to the output log if the query failed, otherwise the name of each device it found. If the query could not start at all, the reason is read with ${function.bluetooth_last_error_message} and the callback is never called.
 * @function_end
 */

/**
 * @function bluetooth_le_connect
 * @desc This function starts a GATT connection to the given Bluetooth LE device, which must have been found by a scan, listed by ${function.bluetooth_le_connected_devices_query} or looked up with ${function.bluetooth_device_from_id}. It returns a connection handle straight away, before the device has answered, and `callback` is called once when the connection is made or fails.
 *
 * The function returns 0 when the connect could not be started, and the reason is then in ${function.bluetooth_last_error_code} and ${function.bluetooth_last_error_message}: NotInitialized, InvalidHandle for a device handle that is not valid, InvalidArgument for a Classic device, one with no usable address or options that are not valid, PermissionDenied, BluetoothDisabled, NotSupported, or Busy when the device already has a connection. In that case no callback is ever called.
 *
 * The given `options` is a ${struct.BluetoothConnectOptions}, or `undefined` for the defaults. A device that does not answer within its `timeout_ms`, 15 seconds by default, fails with ${constant.BluetoothError}.Timeout, and one that refuses or whose link fails with ConnectionFailed. You can cancel a connect that is still in progress by passing the returned handle to ${function.bluetooth_le_disconnect}, and `callback` is then called once with ${constant.BluetoothError}.ConnectionFailed. ${function.bluetooth_shutdown} fails a connect still in progress with NotInitialized, and the radio turning off fails it with BluetoothDisabled. A connect that fails retires its handle.
 *
 * Once the connection is open, `on_disconnected` is called once if the link ends without ${function.bluetooth_le_disconnect}: the device dropped it, the link failed, or the radio was turned off (${constant.BluetoothError}.BluetoothDisabled). When it is called, the connection handle has already been retired, together with every service, characteristic and descriptor handle discovered on it, and every operation that was waiting on the connection has been failed with ${constant.BluetoothError}.Disconnected. A disconnect you ask for does not call it, and neither does a connect that fails, which reports through `callback`.
 *
 * [[Note: A device can only have one client connection at a time, whether it is still connecting or already connected.]]
 *
 * [[Note: On Apple the connect also returns 0 with NotFound for a device that CoreBluetooth no longer knows (scan for it again), and with PermissionDenied until ${function.bluetooth_permission_request} has been answered.]]
 * @param {Real} device The handle of the LE device to connect to.
 * @param {Struct.BluetoothConnectOptions} [options] How the connect runs, or `undefined` for the defaults.
 * @param {Function} on_disconnected The function to call once if the open connection ends without ${function.bluetooth_le_disconnect}.
 * @param {Function} callback The function to call once the connection is made or fails.
 * @returns {Real} The new connection handle, or 0 if the connect did not start.
 * @event callback:on_disconnected
 * @desc Called once, when the open connection ends without ${function.bluetooth_le_disconnect}. It is the last callback of the connection.
 * @member {Enum.BluetoothError} error_code The reason the connection ended, such as ${constant.BluetoothError}.Disconnected when the device dropped it or ${constant.BluetoothError}.BluetoothDisabled when the radio was turned off.
 * @member {String} message A description of why the connection ended.
 * @member {Real} connection The handle of the connection that ended, which is no longer valid.
 * @event_end
 * @event callback:callback
 * @desc Called once, when the connection is made, fails or is cancelled.
 * @member {Enum.BluetoothError} error_code ${constant.BluetoothError}.Ok if the device is connected, otherwise the reason it is not.
 * @member {String} message A description of the failure, or an empty string on success.
 * @member {Real} connection The connection handle that ${function.bluetooth_le_connect} returned.
 * @member {Real} device The handle of the device.
 * @event_end
 * @example
 * ```gml
 * var _options = new BluetoothConnectOptions();
 * _options.timeout_ms = 10000;
 *
 * connection = bluetooth_le_connect(device, _options,
 *     function(_error_code, _message, _connection)
 *     {
 *         if (_connection != connection) return;
 *         show_debug_message($"Connection lost: {_message}");
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
 *         show_debug_message("Connected, discovering services");
 *         bluetooth_le_services_discover(_connection, undefined, on_services);
 *     });
 *
 * if (connection == 0)
 * {
 *     show_debug_message($"Connect did not start: {bluetooth_last_error_message()}");
 * }
 * ```
 * The above code connects to the device stored in the instance's `device` variable, giving up after 10 seconds, and keeps the returned handle in `connection`. Both callbacks first check that the connection they report is still the one stored in `connection`, so a callback for an older connection the instance has since replaced is ignored. When the connect fails, or the open connection is lost later, the reason is written to the output log and the handle is cleared; on success the services of the device are discovered next with ${function.bluetooth_le_services_discover}, which calls the instance's `on_services` method. If the connect could not be started at all, the returned handle is 0 and the reason is read with ${function.bluetooth_last_error_message}.
 * @function_end
 */

/**
 * @function bluetooth_le_disconnect
 * @desc This function closes a client connection made by ${function.bluetooth_le_connect}, or cancels it while it is still connecting. The disconnect is synchronous: the handle is retired by the call, together with every service, characteristic and descriptor handle discovered on it and every subscription on it, and the connection's `on_disconnected` callback is not called.
 *
 * Every operation still waiting on the connection (a discovery, a read, a write, a subscription, an RSSI or MTU request) is failed once with ${constant.BluetoothError}.Disconnected, and a connect still in progress calls its callback once with ${constant.BluetoothError}.ConnectionFailed.
 *
 * The function returns ${constant.BluetoothError}.NotInitialized before ${function.bluetooth_initialize}, and InvalidHandle for a handle that is not a live client connection: one already disconnected, dropped or failed, or a server connection, which ends when the central disconnects.
 * @param {Real} connection The client connection handle to close.
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the connection was closed or the connect cancelled, otherwise the reason.
 * @example
 * ```gml
 * if (bluetooth_le_connection_is_valid(connection))
 * {
 *     bluetooth_le_disconnect(connection);
 * }
 * connection = 0;
 * ```
 * The above code closes the connection stored in `connection` if its handle is still valid, then clears the variable.
 * @function_end
 */

/**
 * @function bluetooth_le_connection_is_valid
 * @desc This function checks whether the given handle names a live LE connection. A client connection is valid from the moment ${function.bluetooth_le_connect} returns it, while it is still connecting, until it is disconnected, drops or fails to connect. A server connection, which the LE server's `on_connected` callback reports, is valid until its central disconnects or the server stops.
 * @param {Real} connection The connection handle to check.
 * @returns {Bool} `true` if the handle is a live client or server connection, otherwise `false`.
 * @example
 * ```gml
 * if (!bluetooth_le_connection_is_valid(connection))
 * {
 *     connection = 0;
 * }
 * ```
 * The above code clears the instance variable `connection` when the handle it holds is no longer valid.
 * @function_end
 */

/**
 * @function bluetooth_le_connection_is_connected
 * @desc This function checks whether the given connection is currently connected: a client connection whose link is open, or a server connection while its central is connected. It returns `false` while a client connection is still being made, after it has dropped, and for a handle that is not valid.
 * @param {Real} connection The connection handle to check.
 * @returns {Bool} `true` if the connection is connected, otherwise `false`.
 * @example
 * ```gml
 * if (bluetooth_le_connection_is_connected(connection))
 * {
 *     bluetooth_le_characteristic_read(characteristic, on_read);
 * }
 * ```
 * The above code reads a characteristic only when the connection stored in `connection` is connected.
 * @function_end
 */

/**
 * @function bluetooth_le_connection_get_device
 * @desc This function returns the handle of the device the given client connection was made to, or of the central of the given server connection. It returns 0 for a handle that is not a live connection.
 * @param {Real} connection The client or server connection handle.
 * @returns {Real} The device handle, or 0 if the connection is not valid.
 * @example
 * ```gml
 * var _device = bluetooth_le_connection_get_device(connection);
 * show_debug_message($"Connected to {bluetooth_device_get_name(_device)}");
 * ```
 * The above code gets the device of the connection stored in `connection` and writes its name to the output log.
 * @function_end
 */

/**
 * @function bluetooth_le_connection_get_mtu
 * @desc This function returns the ATT MTU (Maximum Transmission Unit) of a connected client or server connection. A notification or a write without response carries at most `mtu - 3` bytes. The MTU is 23 until a larger one is negotiated, and the function returns 0 for a handle that is not a connected LE connection.
 *
 * For a server connection it is the MTU of the central's link, which tells you how much of a value ${function.bluetooth_le_server_notify_value} can send to that central in one notification.
 *
 * [[Note: Windows and Apple negotiate the MTU of a client connection themselves when the link opens, so the value is the negotiated one from the start. On Android the value changes when ${function.bluetooth_le_connection_request_mtu} completes, or when the device or the system negotiates it, and a server connection reports 23 until the central negotiates a larger MTU.]]
 * @param {Real} connection The client or server connection handle.
 * @returns {Real} The ATT MTU (in bytes), or 0 if the connection is not connected.
 * @example
 * ```gml
 * var _max_write = bluetooth_le_connection_get_mtu(connection) - 3;
 * show_debug_message($"A write without response can carry {_max_write} bytes");
 * ```
 * The above code works out the largest write without response the connection can carry and writes it to the output log.
 * @function_end
 */

/**
 * @function bluetooth_le_connection_request_mtu
 * @desc This function asks for an ATT MTU of the given number of bytes on a connected client connection. The callback is called once with the MTU now in effect, which the device may have lowered. ${function.bluetooth_feature_is_supported} with ${constant.BluetoothFeature}.LeMtuRequest says whether the platform really negotiates the size you ask for.
 *
 * If the request cannot start, the function returns a ${constant.BluetoothError} other than `Ok`: NotInitialized, InvalidHandle for a handle that is not a client connection, InvalidArgument for an `mtu` outside 23-517, or Disconnected while the connection is not connected. The callback is then never called, and the reason is in ${function.bluetooth_last_error_message}.
 *
 * [[Note: Only Android negotiates the MTU on request. Windows and Apple negotiate it themselves when the link opens: the callback is called at once with ${constant.BluetoothError}.Ok and that MTU.]]
 * @param {Real} connection The client connection handle.
 * @param {Real} mtu The ATT MTU to ask for (23-517, in bytes).
 * @param {Function} callback The function to call once with the MTU in effect.
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the request started, otherwise the reason it did not.
 * @event callback
 * @desc Called once, when the MTU request completes or fails.
 * @member {Enum.BluetoothError} error_code ${constant.BluetoothError}.Ok if the request completed, otherwise the reason it failed.
 * @member {String} message A description of the failure, or an empty string on success.
 * @member {Real} connection The connection handle.
 * @member {Real} mtu The ATT MTU now in effect (in bytes), or 0 on failure.
 * @event_end
 * @example
 * ```gml
 * var _error = bluetooth_le_connection_request_mtu(connection, 247, function(_error_code, _message, _connection, _mtu)
 * {
 *     if (_error_code != BluetoothError.Ok)
 *     {
 *         show_debug_message($"MTU request failed: {_message}");
 *         return;
 *     }
 *     show_debug_message($"MTU is now {_mtu} bytes");
 * });
 *
 * if (_error != BluetoothError.Ok)
 * {
 *     show_debug_message($"MTU request did not start: {bluetooth_last_error_message()}");
 * }
 * ```
 * The above code asks for an MTU of 247 bytes on the connection stored in `connection`. The callback writes the MTU in effect to the output log, or the reason the request failed. If the request could not start, the reason is read with ${function.bluetooth_last_error_message}.
 * @function_end
 */

/**
 * @function bluetooth_le_connection_read_rssi
 * @desc This function reads the signal strength (RSSI) of a connected client connection. The callback is called once with the value in dBm. ${function.bluetooth_feature_is_supported} with ${constant.BluetoothFeature}.LeReadRssi says whether the platform can do it.
 *
 * If the read cannot start, the function returns a ${constant.BluetoothError} other than `Ok`: NotInitialized, InvalidHandle for a handle that is not a client connection, NotSupported where the platform cannot read the RSSI of a connection, or Disconnected while the connection is not connected. The callback is then never called, and the reason is in ${function.bluetooth_last_error_message}.
 *
 * [[Note: Windows has no API to read the RSSI of a connection and always returns NotSupported. Android, macOS and iOS support it.]]
 * @param {Real} connection The client connection handle.
 * @param {Function} callback The function to call once with the signal strength.
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the read started, otherwise the reason it did not.
 * @event callback
 * @desc Called once, when the RSSI has been read or the read failed.
 * @member {Enum.BluetoothError} error_code ${constant.BluetoothError}.Ok if the RSSI was read, otherwise the reason it was not.
 * @member {String} message A description of the failure, or an empty string on success.
 * @member {Real} connection The connection handle.
 * @member {Real} rssi The signal strength (in dBm), or 0 on failure.
 * @event_end
 * @example
 * ```gml
 * if (bluetooth_feature_is_supported(BluetoothFeature.LeReadRssi))
 * {
 *     var _error = bluetooth_le_connection_read_rssi(connection, function(_error_code, _message, _connection, _rssi)
 *     {
 *         if (_error_code != BluetoothError.Ok)
 *         {
 *             show_debug_message($"RSSI read failed: {_message}");
 *             return;
 *         }
 *         signal_strength = _rssi;
 *     });
 *
 *     if (_error != BluetoothError.Ok)
 *     {
 *         show_debug_message($"RSSI read did not start: {bluetooth_last_error_message()}");
 *     }
 * }
 * ```
 * The above code first checks that the platform can read the RSSI of a connection. It then reads the RSSI of the connection stored in `connection`, and the callback stores the value in the instance variable `signal_strength`, or writes the reason to the output log if the read failed.
 * @function_end
 */

/**
 * @function bluetooth_le_connection_request_priority
 * @desc This function asks the platform to run a client connection at the given priority, trading speed against power. The request is synchronous: ${constant.BluetoothError}.Ok means the request was made, and no platform reports what the link settled on. ${function.bluetooth_feature_is_supported} with ${constant.BluetoothFeature}.LeConnectionPriority says whether the platform can do it.
 *
 * The function returns a ${constant.BluetoothError} other than `Ok` when the request is not made: NotInitialized, InvalidHandle for a handle that is not a client connection, InvalidArgument for a value that is not a ${constant.BluetoothLeConnectionPriority}, NotSupported where the platform cannot set it, Disconnected while the connection is not connected, or OperationFailed when the system refused the request. The reason is in ${function.bluetooth_last_error_message}.
 *
 * [[Note: Android supports connection priority. Windows 11 sets the preferred connection parameters, held for the life of the link; Windows 10 returns NotSupported. Apple always returns NotSupported.]]
 * @param {Real} connection The client connection handle.
 * @param {Enum.BluetoothLeConnectionPriority} priority The priority to ask for.
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the request was made, otherwise the reason it was not.
 * @example
 * ```gml
 * if (bluetooth_feature_is_supported(BluetoothFeature.LeConnectionPriority))
 * {
 *     var _error = bluetooth_le_connection_request_priority(connection, BluetoothLeConnectionPriority.High);
 *     if (_error != BluetoothError.Ok)
 *     {
 *         show_debug_message($"Priority request failed: {bluetooth_last_error_message()}");
 *     }
 * }
 * ```
 * The above code asks for the high connection priority on the connection stored in `connection` when the platform supports it, and writes the reason to the output log if the request was not made.
 * @function_end
 */

/**
 * @function bluetooth_le_services_discover
 * @desc This function starts discovering the GATT services of the device on a connected client connection. When the callback reports ${constant.BluetoothError}.Ok, the services are listed by ${function.bluetooth_le_service_get_count} and ${function.bluetooth_le_service_get_at}, and ${function.bluetooth_le_service_find} looks one up by UUID.
 *
 * The services, characteristics and descriptors of a device are addressed by handles, not by UUID, so two attributes that share a UUID get their own handles. Without options, discovery finds the services only, and you go one level at a time: ${function.bluetooth_le_characteristics_discover} on each service you need, then ${function.bluetooth_le_descriptors_discover} on each characteristic whose descriptors you need. With a ${struct.BluetoothLeDiscoverOptions}, the one callback is called after every level the options ask for has been discovered, in the services its `service_uuids` names (every service when it is `undefined`); the first failure is the callback's error, and what was found before it stays listed. Every service is listed either way.
 *
 * The getters list each level in the order discovery found it. Running discovery again keeps the handles of the attributes that are still there and retires the handles of those that are gone, with every handle under them; a subscription on a characteristic that is gone ends. Every handle discovered on a connection is retired when the connection ends.
 *
 * If discovery cannot start, the function returns a ${constant.BluetoothError} other than `Ok`: NotInitialized, InvalidHandle for a handle that is not a client connection, InvalidArgument for options that are not valid, or Disconnected while the connection is not connected. The callback is then never called, and the reason is in ${function.bluetooth_last_error_message}. A connection that ends while discovery is running fails it with Disconnected.
 *
 * [[Note: Android downloads the whole GATT database in this one step, so on Android the characteristic and descriptor discoveries that follow complete at once.]]
 * @param {Real} connection The client connection handle.
 * @param {Struct.BluetoothLeDiscoverOptions} [options] How deep discovery goes, or `undefined` for the services only.
 * @param {Function} callback The function to call once discovery completes or fails.
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if discovery started, otherwise the reason it did not.
 * @event callback
 * @desc Called once, when every level the options ask for has been discovered, or discovery fails.
 * @member {Enum.BluetoothError} error_code ${constant.BluetoothError}.Ok if everything asked for was discovered, otherwise the first failure.
 * @member {String} message A description of the failure, or an empty string on success.
 * @member {Real} connection The connection handle.
 * @event_end
 * @example
 * ```gml
 * var _options = new BluetoothLeDiscoverOptions();
 * _options.service_uuids = ["0000180d-0000-1000-8000-00805f9b34fb"];
 * _options.characteristics = true;
 *
 * var _error = bluetooth_le_services_discover(connection, _options, function(_error_code, _message, _connection)
 * {
 *     if (_connection != connection) return;
 *     if (_error_code != BluetoothError.Ok)
 *     {
 *         show_debug_message($"Discovery failed: {_message}");
 *         return;
 *     }
 *
 *     var _service = bluetooth_le_service_find(_connection, "180d");
 *     measurement = bluetooth_le_characteristic_find(_service, "2a37");
 *     if (measurement == 0)
 *     {
 *         show_debug_message($"No heart rate measurement: {bluetooth_last_error_message()}");
 *     }
 * });
 *
 * if (_error != BluetoothError.Ok)
 * {
 *     show_debug_message($"Discovery did not start: {bluetooth_last_error_message()}");
 * }
 * ```
 * The above code discovers the services of the device on the connection stored in `connection`, and the characteristics of its Heart Rate service, in one step. On success the callback finds the Heart Rate service with ${function.bluetooth_le_service_find} and its Heart Rate Measurement characteristic (0x2A37) with ${function.bluetooth_le_characteristic_find}, and keeps the characteristic's handle in the instance variable `measurement`. A find that matches nothing returns 0, and a find given 0 does too, so one check covers a device without the service or the characteristic. If discovery failed, or could not start at all, the reason is written to the output log instead.
 * @function_end
 */

/**
 * @function bluetooth_le_service_get_count
 * @desc This function returns how many services ${function.bluetooth_le_services_discover} has found on the given connection, or 0 before discovery and for a handle that is not valid.
 * @param {Real} connection The client connection handle.
 * @returns {Real} The number of discovered services.
 * @example
 * ```gml
 * var _count = bluetooth_le_service_get_count(connection);
 * show_debug_message($"The device has {_count} services");
 * ```
 * The above code gets the number of services discovered on the connection stored in `connection` and writes it to the output log.
 * @function_end
 */

/**
 * @function bluetooth_le_service_get_at
 * @desc This function returns the handle of the service at the given index on the given connection, in the order discovery found the services. It returns 0 for an index out of range or a connection that is not valid.
 * @param {Real} connection The client connection handle.
 * @param {Real} index The index of the service, from 0 to ${function.bluetooth_le_service_get_count} - 1.
 * @returns {Real} The service handle, or 0 if there is none at that index.
 * @example
 * ```gml
 * var _service = bluetooth_le_service_get_at(connection, 0);
 * ```
 * The above code gets the handle of the first service discovered on the connection stored in `connection`.
 * @function_end
 */

/**
 * @function bluetooth_le_service_get_uuid
 * @desc This function returns the UUID of the given service, as a lowercase 128-bit UUID string, the form every `*_get_uuid` function returns. It returns an empty string for a handle that is not valid. It also takes a service handle from ${function.bluetooth_le_server_add_service}.
 * @param {Real} service The service handle.
 * @returns {String} The service UUID, or an empty string if the handle is not valid.
 * @example
 * ```gml
 * var _count = bluetooth_le_service_get_count(connection);
 * for (var _i = 0; _i < _count; _i++)
 * {
 *     var _service = bluetooth_le_service_get_at(connection, _i);
 *     show_debug_message($"Service {bluetooth_le_service_get_uuid(_service)}");
 * }
 * ```
 * The above code writes the UUID of every service discovered on the connection stored in `connection` to the output log. Each UUID is in its lowercase 128-bit form, such as "0000180d-0000-1000-8000-00805f9b34fb" for the Heart Rate service.
 * @function_end
 */

/**
 * @function bluetooth_le_service_find
 * @desc This function returns the first service discovered on the given connection whose UUID is the given one, in the order discovery found the services. The UUID can be written in any form: the 16-bit "180D", the 32-bit form or the full 128-bit form, in upper or lower case, all match.
 *
 * The function returns 0 when no discovered service has that UUID, and also when it fails, and the reason is then in ${function.bluetooth_last_error_code} and ${function.bluetooth_last_error_message}: NotFound when nothing matches, InvalidArgument for a malformed UUID, and InvalidHandle for a connection that is not live.
 * @param {Real} connection The client connection handle.
 * @param {String} uuid The UUID of the service to find.
 * @returns {Real} The service handle, or 0 if none has that UUID or the call fails.
 * @example
 * ```gml
 * heart_rate_service = bluetooth_le_service_find(connection, "180d");
 * if (heart_rate_service == 0)
 * {
 *     show_debug_message($"No Heart Rate service: {bluetooth_last_error_message()}");
 * }
 * ```
 * The above code looks up the Heart Rate service (0x180D) among the services discovered on the connection stored in `connection` and keeps its handle in the instance variable `heart_rate_service`, or writes the reason to the output log when there is none.
 * @function_end
 */

/**
 * @function bluetooth_le_characteristics_discover
 * @desc This function starts discovering the characteristics of the given service, which must come from ${function.bluetooth_le_services_discover}. When the callback reports ${constant.BluetoothError}.Ok, the characteristics are listed by ${function.bluetooth_le_characteristic_get_count} and ${function.bluetooth_le_characteristic_get_at}, in the order discovery found them, and ${function.bluetooth_le_characteristic_find} looks one up by UUID. A ${struct.BluetoothLeDiscoverOptions} passed to ${function.bluetooth_le_services_discover} can do this step for you.
 *
 * If discovery cannot start, the function returns a ${constant.BluetoothError} other than `Ok`: NotInitialized, InvalidHandle for a handle that is not a discovered service (also a service of a connection that has ended), or Disconnected while the connection is not connected or is closing. The callback is then never called, and the reason is in ${function.bluetooth_last_error_message}.
 *
 * [[Note: On Android the characteristics were already downloaded by the service discovery, so the callback reports them without asking the device again.]]
 * @param {Real} service The service handle.
 * @param {Function} callback The function to call once discovery completes or fails.
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if discovery started, otherwise the reason it did not.
 * @event callback
 * @desc Called once, when characteristic discovery completes or fails.
 * @member {Enum.BluetoothError} error_code ${constant.BluetoothError}.Ok if the characteristics were discovered, otherwise the reason they were not.
 * @member {String} message A description of the failure, or an empty string on success.
 * @member {Real} service The service handle.
 * @event_end
 * @example
 * ```gml
 * var _error = bluetooth_le_characteristics_discover(heart_rate_service, function(_error_code, _message, _service)
 * {
 *     if (_error_code != BluetoothError.Ok)
 *     {
 *         show_debug_message($"Characteristic discovery failed: {_message}");
 *         return;
 *     }
 *     var _count = bluetooth_le_characteristic_get_count(_service);
 *     for (var _i = 0; _i < _count; _i++)
 *     {
 *         var _characteristic = bluetooth_le_characteristic_get_at(_service, _i);
 *         show_debug_message($"Characteristic {bluetooth_le_characteristic_get_uuid(_characteristic)}");
 *     }
 * });
 *
 * if (_error != BluetoothError.Ok)
 * {
 *     show_debug_message($"Characteristic discovery did not start: {bluetooth_last_error_message()}");
 * }
 * ```
 * The above code discovers the characteristics of the service stored in `heart_rate_service`. On success the callback writes the UUID of each characteristic to the output log; if discovery failed, or could not start, the reason is written instead.
 * @function_end
 */

/**
 * @function bluetooth_le_characteristic_get_count
 * @desc This function returns how many characteristics ${function.bluetooth_le_characteristics_discover} has found in the given service, or 0 before discovery and for a handle that is not valid. It also walks a service handle from ${function.bluetooth_le_server_add_service}, listing its characteristics in definition order.
 * @param {Real} service The service handle.
 * @returns {Real} The number of characteristics.
 * @example
 * ```gml
 * var _count = bluetooth_le_characteristic_get_count(heart_rate_service);
 * show_debug_message($"The service has {_count} characteristics");
 * ```
 * The above code gets the number of characteristics discovered in the service stored in `heart_rate_service` and writes it to the output log.
 * @function_end
 */

/**
 * @function bluetooth_le_characteristic_get_at
 * @desc This function returns the handle of the characteristic at the given index in the given service, in the order discovery found the characteristics. It returns 0 for an index out of range or a service that is not valid.
 * @param {Real} service The service handle.
 * @param {Real} index The index of the characteristic, from 0 to ${function.bluetooth_le_characteristic_get_count} - 1.
 * @returns {Real} The characteristic handle, or 0 if there is none at that index.
 * @example
 * ```gml
 * var _characteristic = bluetooth_le_characteristic_get_at(heart_rate_service, 0);
 * ```
 * The above code gets the handle of the first characteristic discovered in the service stored in `heart_rate_service`.
 * @function_end
 */

/**
 * @function bluetooth_le_characteristic_get_uuid
 * @desc This function returns the UUID of the given characteristic, as a lowercase 128-bit UUID string. It returns an empty string for a handle that is not valid.
 * @param {Real} characteristic The characteristic handle.
 * @returns {String} The characteristic UUID, or an empty string if the handle is not valid.
 * @example
 * ```gml
 * var _count = bluetooth_le_characteristic_get_count(heart_rate_service);
 * for (var _i = 0; _i < _count; _i++)
 * {
 *     var _characteristic = bluetooth_le_characteristic_get_at(heart_rate_service, _i);
 *     show_debug_message($"Characteristic {bluetooth_le_characteristic_get_uuid(_characteristic)}");
 * }
 * ```
 * The above code writes the UUID of every characteristic of the service stored in `heart_rate_service` to the output log, each in its lowercase 128-bit form.
 * @function_end
 */

/**
 * @function bluetooth_le_characteristic_find
 * @desc This function returns the first characteristic of the given service whose UUID is the given one, in the order the characteristics are listed. The UUID can be written in any form, as for ${function.bluetooth_le_service_find}. The service can also be a server service from ${function.bluetooth_le_server_add_service}.
 *
 * The function returns 0 when no characteristic of the service has that UUID, and also when it fails, and the reason is then in ${function.bluetooth_last_error_code} and ${function.bluetooth_last_error_message}: NotFound when nothing matches, InvalidArgument for a malformed UUID, and InvalidHandle for a service that is not valid.
 * @param {Real} service The service handle.
 * @param {String} uuid The UUID of the characteristic to find.
 * @returns {Real} The characteristic handle, or 0 if none has that UUID or the call fails.
 * @example
 * ```gml
 * measurement = bluetooth_le_characteristic_find(heart_rate_service, "2a37");
 * if (measurement == 0)
 * {
 *     show_debug_message($"No Heart Rate Measurement: {bluetooth_last_error_message()}");
 * }
 * ```
 * The above code looks up the Heart Rate Measurement characteristic (0x2A37) in the service stored in `heart_rate_service` and keeps its handle in the instance variable `measurement`, or writes the reason to the output log when there is none.
 * @function_end
 */

/**
 * @function bluetooth_le_characteristic_get_properties
 * @desc This function returns what the given characteristic supports, as a combination of ${constant.BluetoothLeCharacteristicProperty} flags. Test a flag with the `&` operator. It returns 0 for a handle that is not valid.
 *
 * Note that only the GATT core properties byte is returned: platform bits above 0x80 are masked off.
 * @param {Real} characteristic The characteristic handle.
 * @returns {Real} A bitmask of ${constant.BluetoothLeCharacteristicProperty} flags.
 * @example
 * ```gml
 * var _properties = bluetooth_le_characteristic_get_properties(measurement);
 * if ((_properties & BluetoothLeCharacteristicProperty.Notify) != 0)
 * {
 *     bluetooth_le_characteristic_subscribe(measurement, BluetoothLeSubscribeMode.Notify, on_measurement, on_subscribed);
 * }
 * ```
 * The above code gets the properties of the characteristic stored in `measurement` and subscribes to its notifications only when it has the Notify property.
 * @function_end
 */

/**
 * @function bluetooth_le_descriptors_discover
 * @desc This function starts discovering the descriptors of the given characteristic, which must come from ${function.bluetooth_le_characteristics_discover}. When the callback reports ${constant.BluetoothError}.Ok, the descriptors are listed by ${function.bluetooth_le_descriptor_get_count} and ${function.bluetooth_le_descriptor_get_at}, in the order discovery found them, and ${function.bluetooth_le_descriptor_find} looks one up by UUID. A ${struct.BluetoothLeDiscoverOptions} passed to ${function.bluetooth_le_services_discover} can do this step for you.
 *
 * If discovery cannot start, the function returns a ${constant.BluetoothError} other than `Ok`: NotInitialized, InvalidHandle for a handle that is not a discovered characteristic, or Disconnected while the connection is not connected or is closing. The callback is then never called, and the reason is in ${function.bluetooth_last_error_message}.
 *
 * [[Note: On Android the descriptors were already downloaded by the service discovery, so the callback reports them without asking the device again.]]
 * @param {Real} characteristic The characteristic handle.
 * @param {Function} callback The function to call once discovery completes or fails.
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if discovery started, otherwise the reason it did not.
 * @event callback
 * @desc Called once, when descriptor discovery completes or fails.
 * @member {Enum.BluetoothError} error_code ${constant.BluetoothError}.Ok if the descriptors were discovered, otherwise the reason they were not.
 * @member {String} message A description of the failure, or an empty string on success.
 * @member {Real} characteristic The characteristic handle.
 * @event_end
 * @example
 * ```gml
 * var _error = bluetooth_le_descriptors_discover(measurement, function(_error_code, _message, _characteristic)
 * {
 *     if (_error_code != BluetoothError.Ok)
 *     {
 *         show_debug_message($"Descriptor discovery failed: {_message}");
 *         return;
 *     }
 *     show_debug_message($"Found {bluetooth_le_descriptor_get_count(_characteristic)} descriptors");
 * });
 *
 * if (_error != BluetoothError.Ok)
 * {
 *     show_debug_message($"Descriptor discovery did not start: {bluetooth_last_error_message()}");
 * }
 * ```
 * The above code discovers the descriptors of the characteristic stored in `measurement`. On success the callback writes the number of descriptors to the output log; if discovery failed, or could not start, the reason is written instead.
 * @function_end
 */

/**
 * @function bluetooth_le_descriptor_get_count
 * @desc This function returns how many descriptors ${function.bluetooth_le_descriptors_discover} has found on the given characteristic, or 0 before discovery and for a handle that is not valid. It also walks a characteristic handle of a server service from ${function.bluetooth_le_server_add_service}.
 * @param {Real} characteristic The characteristic handle.
 * @returns {Real} The number of descriptors.
 * @example
 * ```gml
 * var _count = bluetooth_le_descriptor_get_count(measurement);
 * show_debug_message($"The characteristic has {_count} descriptors");
 * ```
 * The above code gets the number of descriptors discovered on the characteristic stored in `measurement` and writes it to the output log.
 * @function_end
 */

/**
 * @function bluetooth_le_descriptor_get_at
 * @desc This function returns the handle of the descriptor at the given index on the given characteristic, in the order discovery found the descriptors. It returns 0 for an index out of range or a characteristic that is not valid.
 * @param {Real} characteristic The characteristic handle.
 * @param {Real} index The index of the descriptor, from 0 to ${function.bluetooth_le_descriptor_get_count} - 1.
 * @returns {Real} The descriptor handle, or 0 if there is none at that index.
 * @example
 * ```gml
 * var _descriptor = bluetooth_le_descriptor_get_at(measurement, 0);
 * ```
 * The above code gets the handle of the first descriptor discovered on the characteristic stored in `measurement`.
 * @function_end
 */

/**
 * @function bluetooth_le_descriptor_get_uuid
 * @desc This function returns the UUID of the given descriptor, as a lowercase 128-bit UUID string. It returns an empty string for a handle that is not valid.
 * @param {Real} descriptor The descriptor handle.
 * @returns {String} The descriptor UUID, or an empty string if the handle is not valid.
 * @example
 * ```gml
 * var _count = bluetooth_le_descriptor_get_count(measurement);
 * for (var _i = 0; _i < _count; _i++)
 * {
 *     var _descriptor = bluetooth_le_descriptor_get_at(measurement, _i);
 *     show_debug_message($"Descriptor {bluetooth_le_descriptor_get_uuid(_descriptor)}");
 * }
 * ```
 * The above code writes the UUID of every descriptor of the characteristic stored in `measurement` to the output log, each in its lowercase 128-bit form.
 * @function_end
 */

/**
 * @function bluetooth_le_descriptor_find
 * @desc This function returns the first descriptor of the given characteristic whose UUID is the given one, in the order the descriptors are listed. The UUID can be written in any form, as for ${function.bluetooth_le_service_find}. The characteristic can also be a server characteristic from a service that ${function.bluetooth_le_server_add_service} added.
 *
 * The function returns 0 when no descriptor of the characteristic has that UUID, and also when it fails, and the reason is then in ${function.bluetooth_last_error_code} and ${function.bluetooth_last_error_message}: NotFound when nothing matches, InvalidArgument for a malformed UUID, and InvalidHandle for a characteristic that is not valid.
 * @param {Real} characteristic The characteristic handle.
 * @param {String} uuid The UUID of the descriptor to find.
 * @returns {Real} The descriptor handle, or 0 if none has that UUID or the call fails.
 * @example
 * ```gml
 * user_description = bluetooth_le_descriptor_find(measurement, "2901");
 * ```
 * The above code looks up the Characteristic User Description descriptor (0x2901) of the characteristic stored in `measurement` and keeps its handle, or 0 when it has none, in the instance variable `user_description`.
 * @function_end
 */

/**
 * @function bluetooth_le_characteristic_read
 * @desc This function starts reading the value of the given characteristic. The callback is called once with a value handle and the size of the value in bytes. Copy the bytes into a buffer with ${function.bluetooth_value_fetch} or ${function.bluetooth_value_to_buffer}, which also free the value; if you do not need them, free the value with ${function.bluetooth_value_release}. A value that is neither fetched nor released is dropped 30 seconds after its callback was queued, and at most 256 LE read and notification values are held at a time (the oldest is dropped past that), so fetch or release each value in the callback. A successful read of 0 bytes still delivers a value handle; a failed read delivers 0 and needs nothing freed.
 *
 * If the read cannot start, the function returns a ${constant.BluetoothError} other than `Ok`: NotInitialized, InvalidHandle for a handle that is not a discovered characteristic, or Disconnected while the connection is not connected. The callback is then never called, and the reason is in ${function.bluetooth_last_error_message}. A device that refuses the read reports NotPermitted, or InsufficientSecurity when it needs pairing or encryption first, through the callback.
 *
 * [[Note: Apple reports reads and notifications through one system call, so on a subscribed characteristic a notification that arrives while the read is in flight can be what the read returns.]]
 * @param {Real} characteristic The characteristic handle.
 * @param {Function} callback The function to call once with the value read.
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the read started, otherwise the reason it did not.
 * @event callback
 * @desc Called once, when the read completes or fails.
 * @member {Enum.BluetoothError} error_code ${constant.BluetoothError}.Ok if the value was read, otherwise the reason it was not.
 * @member {String} message A description of the failure, or an empty string on success.
 * @member {Real} characteristic The characteristic handle.
 * @member {Real} value The value handle holding the bytes read, to pass to ${function.bluetooth_value_fetch}, or 0 on failure.
 * @member {Real} size The size of the value (in bytes), or 0 on failure.
 * @event_end
 * @example
 * ```gml
 * var _error = bluetooth_le_characteristic_read(battery_level, function(_error_code, _message, _characteristic, _value, _size)
 * {
 *     if (_error_code != BluetoothError.Ok)
 *     {
 *         show_debug_message($"Read failed: {_message}");
 *         return;
 *     }
 *     if (_size < 1)
 *     {
 *         bluetooth_value_release(_value);
 *         return;
 *     }
 *
 *     var _buffer = bluetooth_value_to_buffer(_value, _size);
 *     if (_buffer == -1)
 *     {
 *         show_debug_message($"Fetch failed: {bluetooth_last_error_message()}");
 *         return;
 *     }
 *     show_debug_message($"Battery level: {buffer_peek(_buffer, 0, buffer_u8)}%");
 *     buffer_delete(_buffer);
 * });
 *
 * if (_error != BluetoothError.Ok)
 * {
 *     show_debug_message($"Read did not start: {bluetooth_last_error_message()}");
 * }
 * ```
 * The above code reads the characteristic stored in `battery_level`. An empty value is released with ${function.bluetooth_value_release}, since there is nothing to read. Otherwise the callback copies the value into a new buffer with ${function.bluetooth_value_to_buffer}, which frees the value, and writes the first byte to the output log as the battery level before deleting the buffer. If the read failed, could not start, or the value could not be fetched, the reason is written to the output log instead.
 * @function_end
 */

/**
 * @function bluetooth_le_characteristic_write
 * @desc This function writes the given number of bytes from the given buffer, starting at the given offset, to a characteristic. The bytes are copied when you call the function, so you can change or delete the buffer as soon as it returns. The callback is called once: for ${constant.BluetoothLeWriteType}.WithResponse when the device has confirmed or refused the write, and for WithoutResponse when the platform has taken the bytes.
 *
 * A write without response carries at most `mtu - 3` bytes, where `mtu` is the value of ${function.bluetooth_le_connection_get_mtu}, so you should keep such writes within that length on every platform.
 *
 * If the write cannot start, the function returns a ${constant.BluetoothError} other than `Ok`: NotInitialized, InvalidHandle for a handle that is not a discovered characteristic, InvalidArgument for a `write_type` that is not a ${constant.BluetoothLeWriteType} or an `offset` and `size` outside the buffer, NotSupported for a write without response on a characteristic without the WriteWithoutResponse property, Busy while 64 writes without response wait to be sent on the connection, or Disconnected while the connection is not connected. The callback is then never called, and the reason is in ${function.bluetooth_last_error_message}. A device that refuses the write reports NotPermitted, or InsufficientSecurity when it needs pairing or encryption first, through the callback.
 *
 * [[Note: On Apple a write without response is also InvalidArgument past the link's maximum write length.]]
 * @param {Real} characteristic The characteristic handle.
 * @param {Buffer} data The buffer holding the bytes to write.
 * @param {Real} offset The position in the buffer of the first byte to write (in bytes).
 * @param {Real} size The number of bytes to write.
 * @param {Enum.BluetoothLeWriteType} write_type Whether the device confirms the write.
 * @param {Function} callback The function to call once the write completes or fails.
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the write started, otherwise the reason it did not.
 * @event callback
 * @desc Called once, when the write completes or fails.
 * @member {Enum.BluetoothError} error_code ${constant.BluetoothError}.Ok if the write succeeded, otherwise the reason it did not.
 * @member {String} message A description of the failure, or an empty string on success.
 * @member {Real} characteristic The characteristic handle.
 * @event_end
 * @example
 * ```gml
 * var _buffer = buffer_create(1, buffer_fixed, 1);
 * buffer_write(_buffer, buffer_u8, 1);
 *
 * var _error = bluetooth_le_characteristic_write(control_point, _buffer, 0, 1, BluetoothLeWriteType.WithResponse, function(_error_code, _message, _characteristic)
 * {
 *     if (_error_code != BluetoothError.Ok)
 *     {
 *         show_debug_message($"Write failed: {_message}");
 *         return;
 *     }
 *     show_debug_message("Write confirmed");
 * });
 * buffer_delete(_buffer);
 *
 * if (_error != BluetoothError.Ok)
 * {
 *     show_debug_message($"Write did not start: {bluetooth_last_error_message()}");
 * }
 * ```
 * The above code creates a one-byte buffer holding the value 1 and writes it to the characteristic stored in `control_point`, asking the device to confirm the write. The buffer is deleted straight after the call, since the bytes were copied when the function was called. The callback writes to the output log whether the device confirmed the write; if the write could not start, the reason is read with ${function.bluetooth_last_error_message}.
 * @function_end
 */

/**
 * @function bluetooth_le_characteristic_subscribe
 * @desc This function subscribes to the notifications or indications of the given characteristic, by writing its Client Characteristic Configuration descriptor (0x2902) on the device. `callback` is called once when the device has accepted or refused the subscription.
 *
 * Every value the device then sends is passed to `on_value`, until the subscription ends: you call ${function.bluetooth_le_characteristic_unsubscribe}, the connection ends, or a new discovery drops the characteristic. The callback receives a value handle and the size of the value in bytes: copy the bytes into a buffer with ${function.bluetooth_value_fetch}, which also frees the value, or free it with ${function.bluetooth_value_release} if you do not need them. A value that is neither fetched nor released is dropped 30 seconds after its callback was queued, and at most 256 LE read and notification values are held at a time (the oldest is dropped past that), so fetch or release each value in the callback.
 *
 * If the subscription cannot start, the function returns a ${constant.BluetoothError} other than `Ok`: NotInitialized, InvalidHandle for a handle that is not a discovered characteristic, InvalidArgument for a `mode` that is not a ${constant.BluetoothLeSubscribeMode}, Busy while the characteristic is subscribed or subscribing, NotSupported for Notify on a characteristic without the Notify property or Indicate on one without the Indicate property, or Disconnected while the connection is not connected. No callback is then called, and the reason is in ${function.bluetooth_last_error_message}.
 *
 * [[Note: On Android a characteristic with no Client Characteristic Configuration descriptor is NotSupported from the call itself. Windows and Apple send the request to the device, and the callback reports the failure.]]
 *
 * [[Note: On a characteristic with both the Notify and Indicate properties, Apple turns on notifications for either mode.]]
 * @param {Real} characteristic The characteristic handle.
 * @param {Enum.BluetoothLeSubscribeMode} mode Whether to subscribe to notifications or to indications.
 * @param {Function} on_value The function to call for every value the device sends while subscribed.
 * @param {Function} callback The function to call once the subscription is accepted or refused.
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the subscription started, otherwise the reason it did not.
 * @event callback:on_value
 * @desc Called for every notification or indication the characteristic sends while subscribed.
 * @member {Real} characteristic The handle of the characteristic that sent the value.
 * @member {Real} connection The handle of the client connection the value arrived on.
 * @member {Real} value The value handle holding the bytes, to pass to ${function.bluetooth_value_fetch}.
 * @member {Real} size The size of the value (in bytes).
 * @event_end
 * @event callback:callback
 * @desc Called once, when the device has accepted or refused the subscription.
 * @member {Enum.BluetoothError} error_code ${constant.BluetoothError}.Ok if the characteristic is subscribed, otherwise the reason it is not.
 * @member {String} message A description of the failure, or an empty string on success.
 * @member {Real} characteristic The characteristic handle.
 * @event_end
 * @example
 * ```gml
 * measurement_buffer = buffer_create(512, buffer_fixed, 1);
 *
 * var _error = bluetooth_le_characteristic_subscribe(measurement, BluetoothLeSubscribeMode.Notify,
 *     function(_characteristic, _connection, _value, _size)
 *     {
 *         if (_connection != connection || _size < 2 || _size > buffer_get_size(measurement_buffer))
 *         {
 *             bluetooth_value_release(_value);
 *             return;
 *         }
 *         if (bluetooth_value_fetch(_value, measurement_buffer, 0) != BluetoothError.Ok)
 *         {
 *             show_debug_message($"Fetch failed: {bluetooth_last_error_message()}");
 *             return;
 *         }
 *
 *         var _flags = buffer_peek(measurement_buffer, 0, buffer_u8);
 *         var _rate;
 *         if ((_flags & 1) != 0)
 *         {
 *             if (_size < 3) return;
 *             _rate = buffer_peek(measurement_buffer, 1, buffer_u16);
 *         }
 *         else
 *         {
 *             _rate = buffer_peek(measurement_buffer, 1, buffer_u8);
 *         }
 *         show_debug_message($"Heart rate: {_rate} bpm");
 *     },
 *     function(_error_code, _message, _characteristic)
 *     {
 *         if (_error_code != BluetoothError.Ok)
 *         {
 *             show_debug_message($"Subscribe failed: {_message}");
 *         }
 *     });
 *
 * if (_error != BluetoothError.Ok)
 * {
 *     show_debug_message($"Subscribe did not start: {bluetooth_last_error_message()}");
 * }
 * ```
 * The above code creates a 512-byte buffer, large enough for any characteristic value, and subscribes to the notifications of the Heart Rate Measurement characteristic stored in `measurement`. The `on_value` callback releases, with ${function.bluetooth_value_release}, a value that arrives for a connection other than the one stored in `connection` or that is too short to hold a heart rate. Otherwise the bytes are copied into the buffer with ${function.bluetooth_value_fetch}, and the fetch result is checked. The first bit of the flags byte says whether the heart rate is an 8-bit or a 16-bit value; the 16-bit form needs 3 bytes, so a shorter value is skipped rather than read past its end. The heart rate is then written to the output log. The second callback reports a subscription the device refused, and if the subscription could not start, the reason is read with ${function.bluetooth_last_error_message}.
 * @function_end
 */

/**
 * @function bluetooth_le_characteristic_unsubscribe
 * @desc This function ends the subscription to the given characteristic that ${function.bluetooth_le_characteristic_subscribe} made. The subscription's `on_value` stops at once, though a value that was already queued may still reach it, so keep fetching or releasing what it receives. The callback is called once when the device has taken the change. For a characteristic that is not subscribed, nothing is sent to the device and the callback is called with ${constant.BluetoothError}.Ok.
 *
 * If the request cannot start, the function returns a ${constant.BluetoothError} other than `Ok`, such as NotInitialized, or InvalidHandle for a handle that is not a discovered characteristic. The callback is then never called, and the reason is in ${function.bluetooth_last_error_message}.
 * @param {Real} characteristic The characteristic handle.
 * @param {Function} callback The function to call once the subscription has ended.
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the request started, otherwise the reason it did not.
 * @event callback
 * @desc Called once, when the subscription has ended or the request failed.
 * @member {Enum.BluetoothError} error_code ${constant.BluetoothError}.Ok if the characteristic is no longer subscribed, otherwise the reason the request failed.
 * @member {String} message A description of the failure, or an empty string on success.
 * @member {Real} characteristic The characteristic handle.
 * @event_end
 * @example
 * ```gml
 * var _error = bluetooth_le_characteristic_unsubscribe(measurement, function(_error_code, _message, _characteristic)
 * {
 *     if (_error_code != BluetoothError.Ok)
 *     {
 *         show_debug_message($"Unsubscribe failed: {_message}");
 *     }
 * });
 *
 * if (_error != BluetoothError.Ok)
 * {
 *     show_debug_message($"Unsubscribe did not start: {bluetooth_last_error_message()}");
 * }
 * ```
 * The above code ends the subscription to the characteristic stored in `measurement`. The callback writes the reason to the output log if the request failed; if it could not start, the reason is read with ${function.bluetooth_last_error_message}.
 * @function_end
 */

/**
 * @function bluetooth_le_descriptor_read
 * @desc This function starts reading the value of the given descriptor. The callback is called once with a value handle and the size of the value in bytes, as for ${function.bluetooth_le_characteristic_read}: copy the bytes with ${function.bluetooth_value_fetch}, or free the value with ${function.bluetooth_value_release} if you do not need them. A value that is neither fetched nor released is dropped after 30 seconds.
 *
 * If the read cannot start, the function returns a ${constant.BluetoothError} other than `Ok`: NotInitialized, InvalidHandle for a handle that is not a discovered descriptor, or Disconnected while the connection is not connected. The callback is then never called, and the reason is in ${function.bluetooth_last_error_message}.
 * @param {Real} descriptor The descriptor handle.
 * @param {Function} callback The function to call once with the value read.
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the read started, otherwise the reason it did not.
 * @event callback
 * @desc Called once, when the read completes or fails.
 * @member {Enum.BluetoothError} error_code ${constant.BluetoothError}.Ok if the value was read, otherwise the reason it was not.
 * @member {String} message A description of the failure, or an empty string on success.
 * @member {Real} descriptor The descriptor handle.
 * @member {Real} value The value handle holding the bytes read, to pass to ${function.bluetooth_value_fetch}, or 0 on failure.
 * @member {Real} size The size of the value (in bytes), or 0 on failure.
 * @event_end
 * @example
 * ```gml
 * var _error = bluetooth_le_descriptor_read(user_description, function(_error_code, _message, _descriptor, _value, _size)
 * {
 *     if (_error_code != BluetoothError.Ok)
 *     {
 *         show_debug_message($"Descriptor read failed: {_message}");
 *         return;
 *     }
 *
 *     var _buffer = buffer_create(_size + 1, buffer_fixed, 1);
 *     if (bluetooth_value_fetch(_value, _buffer, 0) == BluetoothError.Ok)
 *     {
 *         buffer_poke(_buffer, _size, buffer_u8, 0);
 *         show_debug_message($"Description: {buffer_peek(_buffer, 0, buffer_string)}");
 *     }
 *     else
 *     {
 *         show_debug_message($"Fetch failed: {bluetooth_last_error_message()}");
 *     }
 *     buffer_delete(_buffer);
 * });
 *
 * if (_error != BluetoothError.Ok)
 * {
 *     show_debug_message($"Descriptor read did not start: {bluetooth_last_error_message()}");
 * }
 * ```
 * The above code reads the descriptor stored in `user_description`. On success the callback copies the value into a buffer one byte larger than the value with ${function.bluetooth_value_fetch}, adds a terminating zero byte, and writes the text to the output log before deleting the buffer. If the read failed, could not start, or the value could not be fetched, the reason is written to the output log instead.
 * @function_end
 */

/**
 * @function bluetooth_le_descriptor_write
 * @desc This function writes the given number of bytes from the given buffer, starting at the given offset, to a descriptor. The bytes are copied when you call the function, so you can change or delete the buffer as soon as it returns. The callback is called once when the device has confirmed or refused the write.
 *
 * The Client Characteristic Configuration descriptor (0x2902) cannot be written with this function: use ${function.bluetooth_le_characteristic_subscribe} and ${function.bluetooth_le_characteristic_unsubscribe} instead.
 *
 * If the write cannot start, the function returns a ${constant.BluetoothError} other than `Ok`: NotInitialized, InvalidHandle for a handle that is not a discovered descriptor, InvalidArgument for the Client Characteristic Configuration descriptor or an `offset` and `size` outside the buffer, or Disconnected while the connection is not connected. The callback is then never called, and the reason is in ${function.bluetooth_last_error_message}.
 * @param {Real} descriptor The descriptor handle.
 * @param {Buffer} data The buffer holding the bytes to write.
 * @param {Real} offset The position in the buffer of the first byte to write (in bytes).
 * @param {Real} size The number of bytes to write.
 * @param {Function} callback The function to call once the write completes or fails.
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the write started, otherwise the reason it did not.
 * @event callback
 * @desc Called once, when the write completes or fails.
 * @member {Enum.BluetoothError} error_code ${constant.BluetoothError}.Ok if the write succeeded, otherwise the reason it did not.
 * @member {String} message A description of the failure, or an empty string on success.
 * @member {Real} descriptor The descriptor handle.
 * @event_end
 * @example
 * ```gml
 * var _buffer = buffer_create(2, buffer_fixed, 1);
 * buffer_write(_buffer, buffer_u16, 500);
 *
 * var _error = bluetooth_le_descriptor_write(descriptor, _buffer, 0, 2, function(_error_code, _message, _descriptor)
 * {
 *     if (_error_code != BluetoothError.Ok)
 *     {
 *         show_debug_message($"Descriptor write failed: {_message}");
 *     }
 * });
 * buffer_delete(_buffer);
 *
 * if (_error != BluetoothError.Ok)
 * {
 *     show_debug_message($"Descriptor write did not start: {bluetooth_last_error_message()}");
 * }
 * ```
 * The above code writes a 16-bit value of 500 to the descriptor stored in the instance variable `descriptor`, then deletes the buffer, since the bytes were copied when the function was called. The callback writes the reason to the output log if the device refused the write; if the write could not start, the reason is read with ${function.bluetooth_last_error_message}.
 * @function_end
 */

/**
 * @struct BluetoothLeScanFilter
 * @desc This struct is one filter for ${function.bluetooth_le_scan_start}. You create it with `new BluetoothLeScanFilter()` and set the variables you want to filter on; every variable is optional, and one left `undefined` is not checked. A device matches when every variable the filter sets matches its advertisement record, merged over every advertisement and scan response it has sent (see ${struct.BluetoothLeAdvertisement}). A filter belongs to the scan it was passed to.
 *
 * The data variables match the start of an advertised entry: an entry matches when its first bytes, as many as `manufacturer_data` or `service_data` holds, equal those bytes under the mask, compared bit by bit where the mask has a 1. A mask must have the same length as its data, and a mask left `undefined` compares every bit.
 *
 * A filter must set at least one variable; `manufacturer_data` needs `company_id`, `service_data` needs `service_data_uuid`, and a mask needs its data. `name` and `name_prefix` can both be set, and must then both match. Anything else makes the scan start return InvalidArgument.
 *
 * [[Note: On Android, while a filtered scan runs, the Bluetooth stack delivers only the advertisements that carry a field one of the filters sets, so a found device's other advertisements do not update its signal strength or advertisement until the scan stops.]]
 * @member {String} [service_uuid] The service UUID the device must advertise, in 4-digit, 8-digit or 36-character form.
 * @member {String} [name] The advertised local name the device must have, matched exactly.
 * @member {String} [name_prefix] The text the device's advertised local name must start with.
 * @member {Real} [company_id] The Bluetooth SIG company identifier (0-65535) the device must advertise manufacturer data for.
 * @member {Array[Real]} [manufacturer_data] The bytes the manufacturer data of `company_id` must start with, each entry a byte (0-255).
 * @member {Array[Real]} [manufacturer_data_mask] The bits of `manufacturer_data` to compare, the same length as it, each entry a byte (0-255).
 * @member {String} [service_data_uuid] The service UUID the device must advertise service data for. Set alone, any service data for it matches.
 * @member {Array[Real]} [service_data] The bytes the service data of `service_data_uuid` must start with, each entry a byte (0-255).
 * @member {Array[Real]} [service_data_mask] The bits of `service_data` to compare, the same length as it, each entry a byte (0-255).
 * @struct_end
 */

/**
 * @struct BluetoothLeScanSettings
 * @desc This struct says how ${function.bluetooth_le_scan_start} runs the scan. You create it with `new BluetoothLeScanSettings()`; every variable is optional and takes its default when left `undefined`. A value outside its range makes the scan start return InvalidArgument.
 * @member {Bool} [active] Whether the scan asks for scan-response packets as well as advertisements, `true` when `undefined`. `false` scans passively only where ${constant.BluetoothFeature}.LePassiveScan is supported, and is ignored elsewhere.
 * @member {Enum.BluetoothLeScanMode} [mode] How hard the scan works, traded against power, ${constant.BluetoothLeScanMode}.LowLatency when `undefined`. Only Android uses it (${constant.BluetoothFeature}.LeScanMode).
 * @member {Real} [update_interval_ms] The least time between two calls of `on_updated` for one device (in milliseconds): 0 to 60000, 0 for every advertisement, or `undefined` for 1000.
 * @struct_end
 */

/**
 * @struct BluetoothLeDiscoverOptions
 * @desc This struct says how far ${function.bluetooth_le_services_discover} goes before it calls its callback. You create it with `new BluetoothLeDiscoverOptions()`; every variable is optional, and one left `undefined` is off or empty. Every service of the device is listed whatever the options say.
 * @member {Array[String]} [service_uuids] The services whose characteristics and descriptors are discovered, or `undefined` for every service.
 * @member {Bool} [characteristics] Whether the characteristics of each service are discovered as well.
 * @member {Bool} [descriptors] Whether the descriptors of each characteristic are discovered as well. It implies `characteristics`.
 * @struct_end
 */

/**
 * @const BluetoothLeSubscribeMode
 * @desc This enum holds the subscription modes of ${function.bluetooth_le_characteristic_subscribe}. To end a subscription, call ${function.bluetooth_le_characteristic_unsubscribe}.
 * @member Notify Turns on notifications: the device sends each new value without waiting for an acknowledgement.
 * @member Indicate Turns on indications: the device sends each new value and waits for the client to acknowledge it.
 * @const_end
 */

/**
 * @const BluetoothLeScanMode
 * @desc This enum holds the scan modes that the `mode` of a ${struct.BluetoothLeScanSettings} can ask for, trading how quickly devices are found against power. They are Android's scan modes; the other platforms ignore the mode (${constant.BluetoothFeature}.LeScanMode).
 * @member LowPower The scan uses the least power and finds devices the slowest.
 * @member Balanced A balance between power and how quickly devices are found.
 * @member LowLatency The scan finds devices the quickest and uses the most power. This is the default.
 * @const_end
 */

/**
 * @const BluetoothLeCharacteristicProperty
 * @desc This enum holds the flags of the GATT characteristic properties byte, which ${function.bluetooth_le_characteristic_get_properties} returns combined. Test a flag with the `&` operator.
 * @member None The characteristic has no properties.
 * @member Broadcast The value can be broadcast in a server characteristic configuration.
 * @member Read The value can be read.
 * @member WriteWithoutResponse The value can be written without a confirmation from the device.
 * @member Write The value can be written with a confirmation from the device.
 * @member Notify The characteristic can send notifications.
 * @member Indicate The characteristic can send indications.
 * @member AuthenticatedSignedWrites The value can be written with a signed write.
 * @member ExtendedProperties The characteristic has an Extended Properties descriptor.
 * @const_end
 */

/**
 * @const BluetoothLeWriteType
 * @desc This enum holds the ways a characteristic value can be written with ${function.bluetooth_le_characteristic_write}.
 * @member WithResponse The device confirms the write, and the callback reports its answer.
 * @member WithoutResponse The value is sent without a confirmation, and the callback reports only that the platform has taken the bytes.
 * @const_end
 */

/**
 * @const BluetoothLeConnectionPriority
 * @desc This enum holds the connection priorities of ${function.bluetooth_le_connection_request_priority}, which trade the speed of a client connection against power. The members are Android's connection priority levels; Windows 11 maps Balanced to its Balanced connection parameters, High to ThroughputOptimized and LowPower to PowerOptimized.
 * @member Balanced The default balance between speed and power.
 * @member High A faster connection with lower latency, which uses more power.
 * @member LowPower A slower connection, which saves power.
 * @const_end
 */

/**
 * @module le_client
 * @title LE Client
 * @desc The functions to scan for Bluetooth LE devices, connect to them as a GATT client, and discover, read, write and subscribe to their services, characteristics and descriptors.
 *
 * Each call that starts something takes the functions to call for its events and keeps them until it ends: a scan its `on_found`, `on_updated` and `on_stopped`, a connection its `on_disconnected`, a subscription its `on_value`. The event that ends it, `on_stopped` or `on_disconnected`, is the last one, and it is not called when you end it yourself with ${function.bluetooth_le_scan_stop} or ${function.bluetooth_le_disconnect}. Starting something that is already running returns ${constant.BluetoothError}.Busy.
 *
 * A connection, and every service, characteristic and descriptor on it, is addressed by an opaque handle, so two attributes that share a UUID get their own handles. ${function.bluetooth_le_services_discover} discovers the services of a connected device, and with a ${struct.BluetoothLeDiscoverOptions} their characteristics and descriptors too; the getters, such as ${function.bluetooth_le_service_get_count} and ${function.bluetooth_le_service_get_at}, list each level in the order discovery found it, and the find functions, such as ${function.bluetooth_le_service_find}, look one up by UUID. Every handle discovered on a connection is retired when the connection ends.
 *
 * The bytes of a read or a notification arrive as a value handle and a size, and are copied into a buffer with ${function.bluetooth_value_fetch} or ${function.bluetooth_value_to_buffer}. A call that fails before it starts returns a ${constant.BluetoothError} (or 0 for ${function.bluetooth_le_connect} and the find functions) and never calls its callbacks; ${function.bluetooth_last_error_message} gives the reason.
 *
 * @section_func
 * @desc These are the functions of the LE client.
 * @ref bluetooth_le_scan_start
 * @ref bluetooth_le_scan_stop
 * @ref bluetooth_le_scan_is_running
 * @ref bluetooth_le_connected_devices_query
 * @ref bluetooth_le_connect
 * @ref bluetooth_le_disconnect
 * @ref bluetooth_le_connection_is_valid
 * @ref bluetooth_le_connection_is_connected
 * @ref bluetooth_le_connection_get_device
 * @ref bluetooth_le_connection_get_mtu
 * @ref bluetooth_le_connection_request_mtu
 * @ref bluetooth_le_connection_read_rssi
 * @ref bluetooth_le_connection_request_priority
 * @ref bluetooth_le_services_discover
 * @ref bluetooth_le_service_get_count
 * @ref bluetooth_le_service_get_at
 * @ref bluetooth_le_service_get_uuid
 * @ref bluetooth_le_service_find
 * @ref bluetooth_le_characteristics_discover
 * @ref bluetooth_le_characteristic_get_count
 * @ref bluetooth_le_characteristic_get_at
 * @ref bluetooth_le_characteristic_get_uuid
 * @ref bluetooth_le_characteristic_find
 * @ref bluetooth_le_characteristic_get_properties
 * @ref bluetooth_le_descriptors_discover
 * @ref bluetooth_le_descriptor_get_count
 * @ref bluetooth_le_descriptor_get_at
 * @ref bluetooth_le_descriptor_get_uuid
 * @ref bluetooth_le_descriptor_find
 * @ref bluetooth_le_characteristic_read
 * @ref bluetooth_le_characteristic_write
 * @ref bluetooth_le_characteristic_subscribe
 * @ref bluetooth_le_characteristic_unsubscribe
 * @ref bluetooth_le_descriptor_read
 * @ref bluetooth_le_descriptor_write
 * @section_end
 *
 * @section_struct
 * @desc These are the structs used by the LE client.
 * @ref BluetoothLeScanSettings
 * @ref BluetoothLeScanFilter
 * @ref BluetoothLeDiscoverOptions
 * @section_end
 *
 * @section_const
 * @desc These are the constants used by the LE client.
 * @ref BluetoothLeSubscribeMode
 * @ref BluetoothLeScanMode
 * @ref BluetoothLeCharacteristicProperty
 * @ref BluetoothLeWriteType
 * @ref BluetoothLeConnectionPriority
 * @section_end
 *
 * @module_end
 */
