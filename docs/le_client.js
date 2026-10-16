/**
 * @function bluetooth_le_is_supported
 * @desc This function checks whether this platform and its Bluetooth adapter can act as a Bluetooth LE central: scan for LE devices and connect to them. It returns `false` before ${function.bluetooth_initialize} has been called and on a device with no Bluetooth LE hardware, and gives the same answer as ${function.bluetooth_feature_is_supported} with ${constant.BluetoothFeature}.LeCentral.
 *
 * [[Note: On Windows the answer comes from the adapter, so call this function after ${function.bluetooth_initialize}.]]
 * @returns {Bool} `true` if Bluetooth LE central is supported, otherwise `false`.
 * @example
 * ```gml
 * if (!bluetooth_le_is_supported())
 * {
 *     show_debug_message("Bluetooth LE is not available on this device");
 * }
 * ```
 * The above code checks for Bluetooth LE support and writes a message to the output log when it is not available.
 * @function_end
 */

/**
 * @function bluetooth_le_scan_start
 * @desc This function starts scanning for Bluetooth LE devices. Every device the scan finds enters the shared device cache and fires the callback set with ${function.bluetooth_set_callback_device_found} once; later advertisements from the same device update its name, RSSI and advertisement, which ${function.bluetooth_device_get_name}, ${function.bluetooth_device_get_rssi} and ${function.bluetooth_device_get_advertisement} return. When the scan stops, whether you stopped it or it failed, the callback set with ${function.bluetooth_set_callback_scan_stopped} is called.
 *
 * The given `filters` array holds ${struct.BluetoothLeScanFilter} structs. An empty array reports every device; otherwise a device enters the cache once its advertisement matches any one of the filters. A filter matches when every field it sets matches. The given `active` argument asks for scan-response packets as well as advertisements; `false` scans passively only where ${constant.BluetoothFeature}.LePassiveScan is supported (Windows), and is ignored elsewhere.
 *
 * A start while a scan is already running returns ${constant.BluetoothError}.Ok, starts nothing new and keeps the running scan's filters: stop the scan first to change them. While the Bluetooth state is still ${constant.BluetoothState}.Unknown the scan starts once the radio is ready.
 *
 * If the scan cannot start, the function returns a ${constant.BluetoothError} other than `Ok`: NotInitialized, InvalidArgument for a filter that sets no field, a malformed UUID or a `company_id` outside 0-65535, PermissionDenied when the Bluetooth permission is missing or was refused, BluetoothDisabled when the radio is off, NotSupported when there is no Bluetooth LE hardware, or OperationFailed when the platform refused to start. The reason is in ${function.bluetooth_last_error_code} and ${function.bluetooth_last_error_message}, and no scan callback is called.
 *
 * [[Note: Android filters in the Bluetooth stack, scans in low-latency mode and allows 5 scan starts per 30 seconds: a sixth starts but finds nothing. It also pauses unfiltered scans while the screen is off, and on Android 6 to 11 a scan returns PermissionDenied while the system Location setting is off.]]
 *
 * [[Note: Apple and Android always scan actively. iOS needs every filter to set `service_uuid` to scan in the background.]]
 * @param {Bool} active Whether to request scan-response packets (an active scan).
 * @param {Array[Struct.BluetoothLeScanFilter]} filters The filters a device must match one of, or an empty array for every device.
 * @returns {Enum.BluetoothError}
 * @example
 * ```gml
 * bluetooth_set_callback_device_found(function(_device)
 * {
 *     show_debug_message($"Found {bluetooth_device_get_name(_device)}");
 * });
 *
 * var _filter = new BluetoothLeScanFilter();
 * _filter.service_uuid = "180d";
 *
 * var _error = bluetooth_le_scan_start(true, [_filter]);
 * if (_error != BluetoothError.Ok)
 * {
 *     show_debug_message($"Scan did not start: {bluetooth_last_error_message()}");
 * }
 * ```
 * The above code first sets the callback that is called for each device the scan finds, which writes the device's name to the output log. It then creates a ${struct.BluetoothLeScanFilter} that matches devices advertising the Heart Rate service (0x180D) and starts an active scan with it. If the scan could not start, the reason is read with ${function.bluetooth_last_error_message}.
 * @function_end
 */

/**
 * @function bluetooth_le_scan_stop
 * @desc This function stops the running Bluetooth LE scan. When a scan was running, the callback set with ${function.bluetooth_set_callback_scan_stopped} is called once with ${constant.BluetoothError}.Ok. A stop while no scan is running returns `Ok` and calls nothing.
 *
 * The function returns ${constant.BluetoothError}.NotInitialized before ${function.bluetooth_initialize}, and on Windows OperationFailed when the platform could not stop its scanner; the scan counts as stopped in that case too. The reason is in ${function.bluetooth_last_error_message}.
 * @returns {Enum.BluetoothError}
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
 * @desc This function lists the Bluetooth LE devices that are connected to this system, by this game or by another app, without a scan. The callback is called once with an array of device handles. A device that is not in the device cache yet enters it without firing the callback set with ${function.bluetooth_set_callback_device_found}. You can pass a handle from the list to ${function.bluetooth_le_connect}.
 *
 * Every UUID in the given `service_uuids` array is checked first. If the query cannot start, the function returns a ${constant.BluetoothError} other than `Ok`: NotInitialized, InvalidArgument for a malformed UUID, PermissionDenied, BluetoothDisabled or NotSupported as for ${function.bluetooth_le_scan_start}, and the callback is never called. The reason is in ${function.bluetooth_last_error_message}.
 *
 * [[Note: Android and Windows list every connected LE device and ignore `service_uuids`. Apple can only list the devices that expose one of the given services: an empty array is InvalidArgument there, and the radio must already be on (BluetoothDisabled otherwise).]]
 * @param {Array[String]} service_uuids The service UUIDs a device must expose one of (used on Apple only).
 * @param {Function} callback The function to call once with the list of connected devices.
 * @returns {Enum.BluetoothError}
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
 * @desc This function starts a GATT connection to the given Bluetooth LE device, which must have been found by a scan, listed by ${function.bluetooth_le_connected_devices_query} or looked up with ${function.bluetooth_device_from_id}. It returns a connection handle straight away, before the device has answered, and the callback is called once when the connection is made or fails.
 *
 * The function returns 0 when the connect could not be started, and the reason is then in ${function.bluetooth_last_error_code} and ${function.bluetooth_last_error_message}: NotInitialized, InvalidHandle for a device handle that is not valid, InvalidArgument for a Classic device or one with no usable address, PermissionDenied, BluetoothDisabled, NotSupported, or Busy when the device already has a connection. In that case the callback is never called.
 *
 * A device that does not answer within 15 seconds fails with ${constant.BluetoothError}.Timeout, and one that refuses or whose link fails with ConnectionFailed. You can cancel a connect that is still in progress by passing the returned handle to ${function.bluetooth_le_disconnect}, and the callback is then called once with ${constant.BluetoothError}.ConnectionFailed. ${function.bluetooth_shutdown} fails a connect still in progress with NotInitialized. A connect that fails retires its handle.
 *
 * [[Note: A device can only have one client connection at a time, whether it is still connecting or already connected.]]
 *
 * [[Note: On Apple the connect also returns 0 with NotFound for a device that CoreBluetooth no longer knows (scan for it again), and with PermissionDenied until ${function.bluetooth_permission_request} has been answered.]]
 * @param {Real} device The handle of the LE device to connect to.
 * @param {Function} callback The function to call once the connection is made or fails.
 * @returns {Real} The new connection handle, or 0 if the connect did not start.
 * @event callback
 * @desc Called once, when the connection is made, fails or is cancelled.
 * @member {Enum.BluetoothError} error_code ${constant.BluetoothError}.Ok if the device is connected, otherwise the reason it is not.
 * @member {String} message A description of the failure, or an empty string on success.
 * @member {Real} connection The connection handle that ${function.bluetooth_le_connect} returned.
 * @member {Real} device The handle of the device.
 * @event_end
 * @example
 * ```gml
 * connection = bluetooth_le_connect(device, function(_error_code, _message, _connection, _device)
 * {
 *     if (_error_code != BluetoothError.Ok)
 *     {
 *         show_debug_message($"Connect failed: {_message}");
 *         connection = 0;
 *         return;
 *     }
 *     show_debug_message("Connected, discovering services");
 *     bluetooth_le_services_discover(_connection, on_services);
 * });
 *
 * if (connection == 0)
 * {
 *     show_debug_message($"Connect did not start: {bluetooth_last_error_message()}");
 * }
 * ```
 * The above code starts a connection to the device stored in the instance's `device` variable and keeps the returned handle in `connection`. When the callback reports a failure the handle is cleared; on success the services of the device are discovered next with ${function.bluetooth_le_services_discover}, which calls the instance's `on_services` function. If the connect could not be started at all, the returned handle is 0 and the reason is read with ${function.bluetooth_last_error_message}.
 * @function_end
 */

/**
 * @function bluetooth_le_disconnect
 * @desc This function closes a client connection made by ${function.bluetooth_le_connect}, or cancels it while it is still connecting. The disconnect is synchronous: the handle is retired by the call, together with every service, characteristic and descriptor handle discovered on it, and the callback set with ${function.bluetooth_set_callback_le_disconnected} is not called for it.
 *
 * Every operation still waiting on the connection (a discovery, a read, a write, a subscription, an RSSI or MTU request) is failed once with ${constant.BluetoothError}.Disconnected, and a connect still in progress calls its callback once with ${constant.BluetoothError}.ConnectionFailed.
 *
 * The function returns ${constant.BluetoothError}.NotInitialized before ${function.bluetooth_initialize}, and InvalidHandle for a handle that is not a live client connection: one already disconnected, dropped or failed, or a server connection, which ends when the central disconnects.
 * @param {Real} connection The client connection handle to close.
 * @returns {Enum.BluetoothError}
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
 * @desc This function checks whether the given handle names a live client connection. A handle is valid from the moment ${function.bluetooth_le_connect} returns it, while it is still connecting, until it is disconnected, drops or fails to connect.
 *
 * [[Note: On Android this function also returns `true` for a server connection handle, which the other platforms report as not valid.]]
 * @param {Real} connection The connection handle to check.
 * @returns {Bool} `true` if the handle is a live client connection, otherwise `false`.
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
 * @desc This function checks whether the given connection is currently connected. It returns `false` while the connection is still being made, after it has dropped, and for a handle that is not valid.
 *
 * [[Note: On Android this function also returns `true` for a connected server connection handle.]]
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
 * @desc This function returns the handle of the device the given connection was made to, or 0 for a handle that is not a live client connection.
 *
 * [[Note: On Android this function also returns the central's device handle for a server connection.]]
 * @param {Real} connection The connection handle.
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
 * @desc This function returns the ATT MTU (Maximum Transmission Unit) of a connected client connection. A notification or a write without response carries at most `mtu - 3` bytes. The MTU is 23 until a larger one is negotiated, and the function returns 0 for a handle that is not a connected client connection.
 *
 * [[Note: Windows and Apple negotiate the MTU themselves when the link opens, so the value is the negotiated one from the start. On Android the value changes when ${function.bluetooth_le_connection_request_mtu} completes, or when the device or the system negotiates it.]]
 * @param {Real} connection The client connection handle.
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
 * @returns {Enum.BluetoothError}
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
 * @returns {Enum.BluetoothError}
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
 * @returns {Enum.BluetoothError}
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
 * @desc This function starts discovering the GATT services of the device on a connected client connection. When the callback reports ${constant.BluetoothError}.Ok, the services are listed by ${function.bluetooth_le_service_get_count} and ${function.bluetooth_le_service_get_at}.
 *
 * The services, characteristics and descriptors of a device are addressed by handles, not by UUID, so two attributes that share a UUID get their own handles. Discovery goes one level at a time: services first, then ${function.bluetooth_le_characteristics_discover} on each service you need, then ${function.bluetooth_le_descriptors_discover} on each characteristic whose descriptors you need. The getters list each level in the order discovery found it. Running discovery again keeps the handles of the attributes that are still there. Every handle discovered on a connection is retired when the connection ends.
 *
 * If discovery cannot start, the function returns a ${constant.BluetoothError} other than `Ok`: NotInitialized, InvalidHandle for a handle that is not a client connection, or Disconnected while the connection is not connected (OperationFailed on Apple). The callback is then never called, and the reason is in ${function.bluetooth_last_error_message}. A connection that ends while discovery is running fails it with Disconnected.
 *
 * [[Note: Android downloads the whole GATT database in this one step, so on Android the characteristic and descriptor discoveries that follow complete at once.]]
 * @param {Real} connection The client connection handle.
 * @param {Function} callback The function to call once discovery completes or fails.
 * @returns {Enum.BluetoothError}
 * @event callback
 * @desc Called once, when service discovery completes or fails.
 * @member {Enum.BluetoothError} error_code ${constant.BluetoothError}.Ok if the services were discovered, otherwise the reason they were not.
 * @member {String} message A description of the failure, or an empty string on success.
 * @member {Real} connection The connection handle.
 * @event_end
 * @example
 * ```gml
 * var _error = bluetooth_le_services_discover(connection, function(_error_code, _message, _connection)
 * {
 *     if (_error_code != BluetoothError.Ok)
 *     {
 *         show_debug_message($"Service discovery failed: {_message}");
 *         return;
 *     }
 *     var _count = bluetooth_le_service_get_count(_connection);
 *     for (var _i = 0; _i < _count; _i++)
 *     {
 *         var _service = bluetooth_le_service_get_at(_connection, _i);
 *         show_debug_message($"Service {bluetooth_le_service_get_uuid(_service)}");
 *     }
 * });
 *
 * if (_error != BluetoothError.Ok)
 * {
 *     show_debug_message($"Service discovery did not start: {bluetooth_last_error_message()}");
 * }
 * ```
 * The above code discovers the services of the device on the connection stored in `connection`. On success the callback walks the services with ${function.bluetooth_le_service_get_count} and ${function.bluetooth_le_service_get_at} and writes the UUID of each one to the output log. If discovery failed, or could not start at all, the reason is written to the output log instead.
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
 * @desc This function returns the UUID of the given service, as a lowercase 128-bit UUID string. It returns an empty string for a handle that is not valid. It also takes a service handle from ${function.bluetooth_le_server_add_service}.
 * @param {Real} service The service handle.
 * @returns {String} The service UUID, or an empty string if the handle is not valid.
 * @example
 * ```gml
 * var _count = bluetooth_le_service_get_count(connection);
 * for (var _i = 0; _i < _count; _i++)
 * {
 *     var _service = bluetooth_le_service_get_at(connection, _i);
 *     if (bluetooth_le_service_get_uuid(_service) == "0000180d-0000-1000-8000-00805f9b34fb")
 *     {
 *         heart_rate_service = _service;
 *     }
 * }
 * ```
 * The above code looks through the services discovered on the connection stored in `connection` and keeps the handle of the Heart Rate service in the instance variable `heart_rate_service`. Note that the UUID is compared in its lowercase 128-bit form, which is the form this function returns.
 * @function_end
 */

/**
 * @function bluetooth_le_characteristics_discover
 * @desc This function starts discovering the characteristics of the given service, which must come from ${function.bluetooth_le_services_discover}. When the callback reports ${constant.BluetoothError}.Ok, the characteristics are listed by ${function.bluetooth_le_characteristic_get_count} and ${function.bluetooth_le_characteristic_get_at}, in the order discovery found them.
 *
 * If discovery cannot start, the function returns a ${constant.BluetoothError} other than `Ok`: NotInitialized, InvalidHandle for a handle that is not a discovered service (also a service of a connection that has ended), or Disconnected when the connection is closing (OperationFailed on Apple). The callback is then never called, and the reason is in ${function.bluetooth_last_error_message}.
 *
 * [[Note: On Android the characteristics were already downloaded by the service discovery, so the callback is called at once with ${constant.BluetoothError}.Ok.]]
 * @param {Real} service The service handle.
 * @param {Function} callback The function to call once discovery completes or fails.
 * @returns {Enum.BluetoothError}
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
 *     if (bluetooth_le_characteristic_get_uuid(_characteristic) == "00002a37-0000-1000-8000-00805f9b34fb")
 *     {
 *         measurement = _characteristic;
 *     }
 * }
 * ```
 * The above code looks through the characteristics of the service stored in `heart_rate_service` and keeps the handle of the Heart Rate Measurement characteristic (0x2A37) in the instance variable `measurement`.
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
 *     bluetooth_le_characteristic_subscribe(measurement, BluetoothLeSubscribeMode.Notify, on_subscribed);
 * }
 * ```
 * The above code gets the properties of the characteristic stored in `measurement` and subscribes to its notifications only when it has the Notify property.
 * @function_end
 */

/**
 * @function bluetooth_le_descriptors_discover
 * @desc This function starts discovering the descriptors of the given characteristic, which must come from ${function.bluetooth_le_characteristics_discover}. When the callback reports ${constant.BluetoothError}.Ok, the descriptors are listed by ${function.bluetooth_le_descriptor_get_count} and ${function.bluetooth_le_descriptor_get_at}, in the order discovery found them.
 *
 * If discovery cannot start, the function returns a ${constant.BluetoothError} other than `Ok`: NotInitialized, InvalidHandle for a handle that is not a discovered characteristic, or Disconnected when the connection is closing (OperationFailed on Apple). The callback is then never called, and the reason is in ${function.bluetooth_last_error_message}.
 *
 * [[Note: On Android the descriptors were already downloaded by the service discovery, so the callback is called at once with ${constant.BluetoothError}.Ok.]]
 * @param {Real} characteristic The characteristic handle.
 * @param {Function} callback The function to call once discovery completes or fails.
 * @returns {Enum.BluetoothError}
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
 *     if (bluetooth_le_descriptor_get_uuid(_descriptor) == "00002901-0000-1000-8000-00805f9b34fb")
 *     {
 *         user_description = _descriptor;
 *     }
 * }
 * ```
 * The above code looks through the descriptors of the characteristic stored in `measurement` and keeps the handle of its Characteristic User Description descriptor (0x2901) in the instance variable `user_description`.
 * @function_end
 */

/**
 * @function bluetooth_le_characteristic_read
 * @desc This function starts reading the value of the given characteristic. The callback is called once with a value handle and the size of the value in bytes. Copy the bytes into a buffer with ${function.bluetooth_value_fetch}, which also frees the value; if you do not need them, free the value with ${function.bluetooth_value_release}. A value that is neither fetched nor released is dropped 30 seconds after its callback was queued, and at most 256 LE read and notification values are held at a time (the oldest is dropped past that), so fetch or release each value in the callback. A successful read of 0 bytes still delivers a value handle; a failed read delivers 0 and needs nothing freed.
 *
 * If the read cannot start, the function returns a ${constant.BluetoothError} other than `Ok`: NotInitialized, InvalidHandle for a handle that is not a discovered characteristic, or Disconnected while the connection is not connected (OperationFailed on Apple). The callback is then never called, and the reason is in ${function.bluetooth_last_error_message}. A device that refuses the read reports NotPermitted, or InsufficientSecurity when it needs pairing or encryption first, through the callback.
 *
 * [[Note: Apple reports reads and notifications through one system call, so on a subscribed characteristic a notification that arrives while the read is in flight can be what the read returns.]]
 * @param {Real} characteristic The characteristic handle.
 * @param {Function} callback The function to call once with the value read.
 * @returns {Enum.BluetoothError}
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
 *     var _buffer = buffer_create(max(_size, 1), buffer_fixed, 1);
 *     var _copied = bluetooth_value_fetch(_value, _buffer, 0);
 *     if (_copied > 0)
 *     {
 *         show_debug_message($"Battery level: {buffer_peek(_buffer, 0, buffer_u8)}%");
 *     }
 *     buffer_delete(_buffer);
 * });
 *
 * if (_error != BluetoothError.Ok)
 * {
 *     show_debug_message($"Read did not start: {bluetooth_last_error_message()}");
 * }
 * ```
 * The above code reads the characteristic stored in `battery_level`. On success the callback creates a buffer large enough for the value, copies the bytes into it with ${function.bluetooth_value_fetch}, which frees the value, and writes the first byte to the output log as the battery level before deleting the buffer. If the read failed, or could not start, the reason is written to the output log instead.
 * @function_end
 */

/**
 * @function bluetooth_le_characteristic_write
 * @desc This function writes the given number of bytes from the given buffer, starting at the given offset, to a characteristic. The bytes are copied when you call the function, so you can change or delete the buffer as soon as it returns. The callback is called once: for ${constant.BluetoothLeWriteType}.WithResponse when the device has confirmed or refused the write, and for WithoutResponse when the platform has taken the bytes.
 *
 * A write without response carries at most `mtu - 3` bytes, where `mtu` is the value of ${function.bluetooth_le_connection_get_mtu}, so you should keep such writes within that length on every platform.
 *
 * If the write cannot start, the function returns a ${constant.BluetoothError} other than `Ok`: NotInitialized, InvalidHandle for a handle that is not a discovered characteristic, InvalidArgument for a `write_type` that is not a ${constant.BluetoothLeWriteType} or an `offset` and `size` outside the buffer, or Disconnected while the connection is not connected (OperationFailed on Apple). The callback is then never called, and the reason is in ${function.bluetooth_last_error_message}. A device that refuses the write reports NotPermitted, or InsufficientSecurity when it needs pairing or encryption first, through the callback.
 *
 * [[Note: On Apple a write without response is NotSupported on a characteristic without the WriteWithoutResponse property, InvalidArgument past the link's maximum write length, and Busy while 64 of them wait to be sent.]]
 * @param {Real} characteristic The characteristic handle.
 * @param {Buffer} data The buffer holding the bytes to write.
 * @param {Real} offset The position in the buffer of the first byte to write (in bytes).
 * @param {Real} size The number of bytes to write.
 * @param {Enum.BluetoothLeWriteType} write_type Whether the device confirms the write.
 * @param {Function} callback The function to call once the write completes or fails.
 * @returns {Enum.BluetoothError}
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
 * @desc This function subscribes to or unsubscribes from the value updates of the given characteristic, by writing its Client Characteristic Configuration descriptor (0x2902) on the device. The callback is called once when the device has accepted or refused the change. While subscribed, every notification or indication is delivered to the callback set with ${function.bluetooth_set_callback_le_characteristic_value_changed}.
 *
 * If the change cannot start, the function returns a ${constant.BluetoothError} other than `Ok`: NotInitialized, InvalidHandle for a handle that is not a discovered characteristic, InvalidArgument for a `mode` that is not a ${constant.BluetoothLeSubscribeMode}, or Disconnected while the connection is not connected (OperationFailed on Apple). The callback is then never called, and the reason is in ${function.bluetooth_last_error_message}.
 *
 * [[Note: On Android, Notify on a characteristic without the Notify property, Indicate on one without the Indicate property, or a characteristic with no Client Characteristic Configuration descriptor is NotSupported from the call itself. Windows and Apple send the request to the device, and the callback reports the failure.]]
 *
 * [[Note: Apple turns on whichever of notifications or indications the characteristic supports, so Notify and Indicate behave the same there.]]
 * @param {Real} characteristic The characteristic handle.
 * @param {Enum.BluetoothLeSubscribeMode} mode Whether to unsubscribe, or subscribe to notifications or indications.
 * @param {Function} callback The function to call once the change completes or fails.
 * @returns {Enum.BluetoothError}
 * @event callback
 * @desc Called once, when the device has accepted or refused the change.
 * @member {Enum.BluetoothError} error_code ${constant.BluetoothError}.Ok if the change was made, otherwise the reason it was not.
 * @member {String} message A description of the failure, or an empty string on success.
 * @member {Real} characteristic The characteristic handle.
 * @event_end
 * @example
 * ```gml
 * var _error = bluetooth_le_characteristic_subscribe(measurement, BluetoothLeSubscribeMode.Notify, function(_error_code, _message, _characteristic)
 * {
 *     if (_error_code != BluetoothError.Ok)
 *     {
 *         show_debug_message($"Subscribe failed: {_message}");
 *         return;
 *     }
 *     show_debug_message("Subscribed to heart rate measurements");
 * });
 *
 * if (_error != BluetoothError.Ok)
 * {
 *     show_debug_message($"Subscribe did not start: {bluetooth_last_error_message()}");
 * }
 * ```
 * The above code subscribes to the notifications of the characteristic stored in `measurement`. The callback writes to the output log whether the device accepted the subscription; if the request could not start, the reason is read with ${function.bluetooth_last_error_message}. The notifications themselves arrive through the callback set with ${function.bluetooth_set_callback_le_characteristic_value_changed}.
 * @function_end
 */

/**
 * @function bluetooth_le_descriptor_read
 * @desc This function starts reading the value of the given descriptor. The callback is called once with a value handle and the size of the value in bytes, as for ${function.bluetooth_le_characteristic_read}: copy the bytes with ${function.bluetooth_value_fetch}, or free the value with ${function.bluetooth_value_release} if you do not need them. A value that is neither fetched nor released is dropped after 30 seconds.
 *
 * If the read cannot start, the function returns a ${constant.BluetoothError} other than `Ok`: NotInitialized, InvalidHandle for a handle that is not a discovered descriptor, or Disconnected while the connection is not connected (OperationFailed on Apple). The callback is then never called, and the reason is in ${function.bluetooth_last_error_message}.
 * @param {Real} descriptor The descriptor handle.
 * @param {Function} callback The function to call once with the value read.
 * @returns {Enum.BluetoothError}
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
 *     var _buffer = buffer_create(_size + 1, buffer_fixed, 1);
 *     bluetooth_value_fetch(_value, _buffer, 0);
 *     buffer_poke(_buffer, _size, buffer_u8, 0);
 *     show_debug_message($"Description: {buffer_peek(_buffer, 0, buffer_string)}");
 *     buffer_delete(_buffer);
 * });
 *
 * if (_error != BluetoothError.Ok)
 * {
 *     show_debug_message($"Descriptor read did not start: {bluetooth_last_error_message()}");
 * }
 * ```
 * The above code reads the descriptor stored in `user_description`. On success the callback copies the value into a buffer one byte larger than the value with ${function.bluetooth_value_fetch}, adds a terminating zero byte, and writes the text to the output log before deleting the buffer. If the read failed, or could not start, the reason is written to the output log instead.
 * @function_end
 */

/**
 * @function bluetooth_le_descriptor_write
 * @desc This function writes the given number of bytes from the given buffer, starting at the given offset, to a descriptor. The bytes are copied when you call the function, so you can change or delete the buffer as soon as it returns. The callback is called once when the device has confirmed or refused the write.
 *
 * The Client Characteristic Configuration descriptor (0x2902) cannot be written with this function: subscribe with ${function.bluetooth_le_characteristic_subscribe} instead.
 *
 * If the write cannot start, the function returns a ${constant.BluetoothError} other than `Ok`: NotInitialized, InvalidHandle for a handle that is not a discovered descriptor, InvalidArgument for the Client Characteristic Configuration descriptor or an `offset` and `size` outside the buffer, or Disconnected while the connection is not connected (OperationFailed on Apple). The callback is then never called, and the reason is in ${function.bluetooth_last_error_message}.
 * @param {Real} descriptor The descriptor handle.
 * @param {Buffer} data The buffer holding the bytes to write.
 * @param {Real} offset The position in the buffer of the first byte to write (in bytes).
 * @param {Real} size The number of bytes to write.
 * @param {Function} callback The function to call once the write completes or fails.
 * @returns {Enum.BluetoothError}
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
 * @function bluetooth_set_callback_le_disconnected
 * @desc This function sets the function that is called when an established client connection ends without ${function.bluetooth_le_disconnect}: the device dropped it, the link failed or the radio was turned off. When it is called, the connection handle has already been retired, together with every service, characteristic and descriptor handle discovered on it, and every operation that was waiting on the connection has been failed with ${constant.BluetoothError}.Disconnected. A disconnect you asked for with ${function.bluetooth_le_disconnect} does not call it, and neither does a connect that fails, which reports through its own callback.
 *
 * Setting a new function replaces the previous one. The function stays set across ${function.bluetooth_shutdown} and ${function.bluetooth_initialize}. The function always returns `true`.
 * @param {Function} callback The function to call when a client connection drops.
 * @returns {Bool} Always `true`.
 * @event callback
 * @desc Called each time an established client connection ends without ${function.bluetooth_le_disconnect}.
 * @member {Enum.BluetoothError} error_code ${constant.BluetoothError}.Disconnected.
 * @member {String} message A description of why the connection ended.
 * @member {Real} connection The handle of the connection that ended, which is no longer valid.
 * @event_end
 * @example
 * ```gml
 * bluetooth_set_callback_le_disconnected(function(_error_code, _message, _connection)
 * {
 *     if (_connection == connection)
 *     {
 *         show_debug_message($"Connection lost: {_message}");
 *         connection = 0;
 *     }
 * });
 * ```
 * The above code sets the function that is called when a client connection drops. When the connection that dropped is the one stored in `connection`, it writes the reason to the output log and clears the variable.
 * @function_end
 */

/**
 * @function bluetooth_remove_callback_le_disconnected
 * @desc This function removes the function set with ${function.bluetooth_set_callback_le_disconnected}, so dropped client connections no longer call it. The function always returns `true`, also when no function was set.
 * @returns {Bool} Always `true`.
 * @example
 * ```gml
 * bluetooth_remove_callback_le_disconnected();
 * ```
 * The above code removes the function that is called when a client connection drops.
 * @function_end
 */

/**
 * @function bluetooth_set_callback_le_characteristic_value_changed
 * @desc This function sets the function that is called for every notification or indication a subscribed characteristic sends (see ${function.bluetooth_le_characteristic_subscribe}). The callback receives a value handle and the size of the value in bytes: copy the bytes into a buffer with ${function.bluetooth_value_fetch}, which also frees the value, or free it with ${function.bluetooth_value_release} if you do not need them. A value that is neither fetched nor released is dropped 30 seconds after its callback was queued, and at most 256 LE read and notification values are held at a time (the oldest is dropped past that), so fetch or release each value in the callback.
 *
 * Only characteristics discovered with ${function.bluetooth_le_characteristics_discover} on the connection are reported. While no function is set, notifications are dropped and no value is kept for them.
 *
 * Setting a new function replaces the previous one. The function stays set across ${function.bluetooth_shutdown} and ${function.bluetooth_initialize}. The function always returns `true`.
 * @param {Function} callback The function to call for every notification or indication.
 * @returns {Bool} Always `true`.
 * @event callback
 * @desc Called once for every notification or indication a subscribed characteristic sends.
 * @member {Real} characteristic The handle of the characteristic that sent the value.
 * @member {Real} connection The handle of the client connection the value arrived on.
 * @member {Real} value The value handle holding the bytes, to pass to ${function.bluetooth_value_fetch}.
 * @member {Real} size The size of the value (in bytes).
 * @event_end
 * @example
 * ```gml
 * notify_buffer = buffer_create(512, buffer_fixed, 1);
 *
 * bluetooth_set_callback_le_characteristic_value_changed(function(_characteristic, _connection, _value, _size)
 * {
 *     if (_characteristic != measurement || _size < 2)
 *     {
 *         bluetooth_value_release(_value);
 *         return;
 *     }
 *     bluetooth_value_fetch(_value, notify_buffer, 0);
 *     var _flags = buffer_peek(notify_buffer, 0, buffer_u8);
 *     var _rate = ((_flags & 1) != 0) ? buffer_peek(notify_buffer, 1, buffer_u16) : buffer_peek(notify_buffer, 1, buffer_u8);
 *     show_debug_message($"Heart rate: {_rate} bpm");
 * });
 * ```
 * The above code creates a 512-byte buffer, large enough for any characteristic value, and sets the function that is called for every notification. A value from any characteristic other than the one stored in `measurement`, or one too short to read, is freed with ${function.bluetooth_value_release}. Otherwise the bytes are copied into the buffer with ${function.bluetooth_value_fetch} and the heart rate is read from them, as an 8-bit or a 16-bit value depending on the first bit of the flags byte, and written to the output log.
 * @function_end
 */

/**
 * @function bluetooth_remove_callback_le_characteristic_value_changed
 * @desc This function removes the function set with ${function.bluetooth_set_callback_le_characteristic_value_changed}. Notifications and indications that arrive afterwards are dropped and no value is kept for them; the subscriptions on the device stay as they are. The function always returns `true`, also when no function was set.
 * @returns {Bool} Always `true`.
 * @example
 * ```gml
 * bluetooth_remove_callback_le_characteristic_value_changed();
 * ```
 * The above code removes the function that is called for every notification or indication.
 * @function_end
 */

/**
 * @struct BluetoothLeScanFilter
 * @desc This struct is one filter for ${function.bluetooth_le_scan_start}. A device matches when every field the filter sets matches what the device has advertised, merged over every advertisement and scan response it has sent since it entered the device cache (see ${struct.BluetoothLeAdvertisement}). A field left `undefined` is not checked, and a filter must set at least one field, otherwise the scan start is InvalidArgument.
 *
 * [[Note: On Android the filters are applied by the Bluetooth stack to each scan result.]]
 * @member {String} [service_uuid] The service UUID the device must advertise, in 4-digit, 8-digit or 36-character form.
 * @member {String} [name] The advertised local name the device must have, matched exactly.
 * @member {Real} [company_id] The Bluetooth SIG company identifier (0-65535) the device must advertise manufacturer data for.
 * @struct_end
 */

/**
 * @const BluetoothLeSubscribeMode
 * @desc This enum holds the subscription modes of ${function.bluetooth_le_characteristic_subscribe}.
 * @member Unsubscribe Turns off the notifications or indications of the characteristic.
 * @member Notify Turns on notifications: the device sends each new value without waiting for an acknowledgement.
 * @member Indicate Turns on indications: the device sends each new value and waits for the client to acknowledge it.
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
 * @desc This enum holds the connection priorities of ${function.bluetooth_le_connection_request_priority}, which trade the speed of a client connection against power. The members are Android's connection priority levels; Windows 11 maps them to its Balanced, ThroughputOptimized and PowerOptimized connection parameters.
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
 * A connection, and every service, characteristic and descriptor on it, is addressed by an opaque handle, never by its UUID, so two attributes that share a UUID get their own handles. Discovery goes one level at a time: ${function.bluetooth_le_services_discover} on a connected connection, then ${function.bluetooth_le_characteristics_discover} on a service, then ${function.bluetooth_le_descriptors_discover} on a characteristic. The getters, such as ${function.bluetooth_le_service_get_count} and ${function.bluetooth_le_service_get_at}, list each level in the order discovery found it. Every handle discovered on a connection is retired when the connection ends.
 *
 * The bytes of a read or a notification arrive as a value handle and a size, and are copied into a buffer with ${function.bluetooth_value_fetch}. A call that fails before it starts returns a ${constant.BluetoothError} (or 0 for ${function.bluetooth_le_connect}) and never calls its callback; ${function.bluetooth_last_error_message} gives the reason.
 *
 * @section_func
 * @desc These are the functions of the LE client.
 * @ref bluetooth_le_is_supported
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
 * @ref bluetooth_le_characteristics_discover
 * @ref bluetooth_le_characteristic_get_count
 * @ref bluetooth_le_characteristic_get_at
 * @ref bluetooth_le_characteristic_get_uuid
 * @ref bluetooth_le_characteristic_get_properties
 * @ref bluetooth_le_descriptors_discover
 * @ref bluetooth_le_descriptor_get_count
 * @ref bluetooth_le_descriptor_get_at
 * @ref bluetooth_le_descriptor_get_uuid
 * @ref bluetooth_le_characteristic_read
 * @ref bluetooth_le_characteristic_write
 * @ref bluetooth_le_characteristic_subscribe
 * @ref bluetooth_le_descriptor_read
 * @ref bluetooth_le_descriptor_write
 * @ref bluetooth_set_callback_le_disconnected
 * @ref bluetooth_remove_callback_le_disconnected
 * @ref bluetooth_set_callback_le_characteristic_value_changed
 * @ref bluetooth_remove_callback_le_characteristic_value_changed
 * @section_end
 *
 * @section_struct
 * @desc These are the structs used by the LE client.
 * @ref BluetoothLeScanFilter
 * @section_end
 *
 * @section_const
 * @desc These are the constants used by the LE client.
 * @ref BluetoothLeSubscribeMode
 * @ref BluetoothLeCharacteristicProperty
 * @ref BluetoothLeWriteType
 * @ref BluetoothLeConnectionPriority
 * @section_end
 *
 * @module_end
 */
