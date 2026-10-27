// Core module: the extension's lifecycle, its error state, capabilities and permissions, the value
// store every inbound byte payload goes through (and the GML helpers over it), the connect options
// and the adapter state callback.

/**
 * @function bluetooth_initialize
 * @desc This function starts the extension and its platform backend. You must call it before any other Bluetooth function: until it succeeds, every call that uses Bluetooth returns ${constant.BluetoothError}.NotInitialized.
 *
 * The function returns `true` once the extension is running, and also when it is already running. It also returns `true` on a device that has no Bluetooth hardware: the extension then reports ${constant.BluetoothState}.Unsupported, ${function.bluetooth_feature_is_supported} returns `false` and every Bluetooth call returns ${constant.BluetoothError}.NotSupported.
 *
 * If the backend cannot be started, the function returns `false` and the reason is in ${function.bluetooth_last_error_code} and ${function.bluetooth_last_error_message}, normally ${constant.BluetoothError}.OperationFailed. The extension is then not initialised, so ${function.bluetooth_is_initialized} returns `false` and you can call this function again to retry.
 *
 * [[Note: On Windows the function waits up to 3 seconds for the Bluetooth adapter to answer, because the capabilities are read from it. An adapter that answers later still sets them and reports its state to the callback set with ${function.bluetooth_set_callback_state_changed}.]]
 *
 * [[Note: On iOS and macOS the function does not show the Bluetooth permission prompt. While the user has not answered it, Bluetooth stays unavailable until you call ${function.bluetooth_permission_request}.]]
 * @returns {Bool} `true` if the extension is running, otherwise `false`.
 * @example
 * ```gml
 * if (!bluetooth_initialize())
 * {
 *     show_debug_message($"Bluetooth could not start: {bluetooth_last_error_message()}");
 *     exit;
 * }
 *
 * if (!bluetooth_feature_is_supported(BluetoothFeature.LeCentral))
 * {
 *     show_debug_message("This device cannot act as an LE client");
 * }
 * ```
 * The above code initialises the extension in the Create Event of a controller object. If the backend could not be started, the reason is read with ${function.bluetooth_last_error_message} and the event ends there. Otherwise ${function.bluetooth_feature_is_supported} is used to check whether the device can scan for and connect to LE devices.
 * @function_end
 */

/**
 * @function bluetooth_shutdown
 * @desc This function stops the extension. It ends every connection, scan, advertisement and server, and frees every value still held for ${function.bluetooth_value_fetch}. Every device, connection and value handle the extension gave out is no longer valid afterwards.
 *
 * Every callback that is still waiting for the outcome of an operation (a connect, a read, a pairing, a permission or enable request, and so on) is called once with ${constant.BluetoothError}.NotInitialized. Every event callback you passed to a call that starts something, such as the `on_found` of a scan or the `on_disconnected` of a connection, is dropped without being called. The callback set with ${function.bluetooth_set_callback_state_changed} stays registered, so it is used again after the next call to ${function.bluetooth_initialize}.
 *
 * You should call this function when the game no longer needs Bluetooth, for example in the Clean Up Event of the object that initialised it. Calling it when the extension is not running does nothing.
 * @example
 * ```gml
 * if (bluetooth_is_initialized())
 * {
 *     bluetooth_shutdown();
 * }
 * ```
 * The above code, placed in the Clean Up Event of the controller object, stops the extension if it is running. Any operation still in progress reports ${constant.BluetoothError}.NotInitialized to its callback, and no event callback is called.
 * @function_end
 */

/**
 * @function bluetooth_is_initialized
 * @desc This function checks whether the extension is running. It returns `true` after a successful call to ${function.bluetooth_initialize} and until ${function.bluetooth_shutdown} is called, and `false` before that, after it, or when ${function.bluetooth_initialize} failed.
 * @returns {Bool} `true` if the extension is running, otherwise `false`.
 * @example
 * ```gml
 * if (!bluetooth_is_initialized())
 * {
 *     bluetooth_initialize();
 * }
 * ```
 * The above code initialises the extension only when it is not already running.
 * @function_end
 */

/**
 * @function bluetooth_last_error_code
 * @desc This function returns the error code of the most recent call that failed. Every function that can fail sets it when it fails: a function that returns a ${constant.BluetoothError} returns the same code, ${function.bluetooth_le_connect}, ${function.bluetooth_classic_connect}, ${function.bluetooth_device_from_id}, ${function.bluetooth_le_service_find}, ${function.bluetooth_le_characteristic_find} and ${function.bluetooth_le_descriptor_find} return 0, and ${function.bluetooth_initialize} returns `false`.
 *
 * A call that succeeds leaves the last error as it was, so you should read it straight after the call that failed, not to check whether a call succeeded. It is ${constant.BluetoothError}.Ok until the first failure. The outcome of an operation that completes later is reported to its callback in the callback's own `error_code` argument, not here.
 *
 * Use ${function.bluetooth_last_error_message} to get a description of the same failure.
 * @returns {Enum.BluetoothError} The error code of the most recent failed call.
 * @example
 * ```gml
 * var _error = bluetooth_request_enable(on_enable);
 * if (_error != BluetoothError.Ok)
 * {
 *     var _code = bluetooth_last_error_code();
 *     show_debug_message($"Request failed with code {_code}: {bluetooth_last_error_message()}");
 * }
 * ```
 * The above code asks the user to turn the Bluetooth radio on and, when the request cannot start, reads the code and the description of the failure.
 * @function_end
 */

/**
 * @function bluetooth_last_error_message
 * @desc This function returns a description of the most recent call that failed, the same failure ${function.bluetooth_last_error_code} returns the code of. The text comes from the extension or the platform and is meant for logs and debugging, not for the player; it is not localised and can differ between platforms for the same code.
 *
 * Like the code, the message is set only when a call fails, so it still describes an earlier failure after a call that succeeds. It is an empty string until the first failure.
 * @returns {String} The description of the most recent failed call.
 * @example
 * ```gml
 * if (!bluetooth_initialize())
 * {
 *     show_debug_message($"Bluetooth could not start: {bluetooth_last_error_message()}");
 * }
 * ```
 * The above code writes the reason to the output log when the extension could not be initialised.
 * @function_end
 */

/**
 * @function bluetooth_feature_is_supported
 * @desc This function checks whether the current platform and Bluetooth adapter can do the given feature. It is the one capability query of the extension: use it to decide which parts of your game's Bluetooth support to offer before you call the functions that need them. A function that uses a feature the platform cannot do returns ${constant.BluetoothError}.NotSupported.
 *
 * The function returns `false` before ${function.bluetooth_initialize} has been called, on a device without Bluetooth, and for a value that is not a ${constant.BluetoothFeature} member.
 *
 * The table below shows what each platform can do when its hardware allows it. "Adapter" means the answer depends on the Bluetooth adapter or the OS version of the device the game runs on.
 *
 * | Feature | Windows | macOS | iOS | Android |
 * | --- | --- | --- | --- | --- |
 * | `LeCentral` | Adapter | Yes | Yes | Adapter |
 * | `LePassiveScan` | Adapter | No | No | No |
 * | `LeScanMode` | No | No | No | Yes |
 * | `LeAdvertise`, `LeAdvertiseName`, `LeAdvertiseServiceUuids` | Adapter | Yes | Yes | Adapter |
 * | `LeAdvertiseServiceData`, `LeAdvertiseManufacturerData`, `LeAdvertiseTxPower`, `LeAdvertiseIncludeTxPower`, `LeAdvertiseNonConnectable` | Adapter | No | No | Adapter |
 * | `LeServer` | Adapter | Yes | Yes | Adapter |
 * | `LeServerDescriptorRequests` | Adapter | No | No | Adapter |
 * | `LeServerSignedWrite`, `LeServerConnectionEvents`, `LePairing` | No | No | No | Adapter |
 * | `Classic`, `ClassicServer`, `ClassicPairing` | Adapter | Yes | No | Adapter |
 * | `ClassicDiscoverable` | Adapter | No | No | Adapter |
 * | `ClassicDiscoverableStop` | Adapter | No | No | No |
 * | `PermissionRequest` | No | Yes | Yes | Android 6+ |
 * | `LeMtuRequest` | No | No | No | Adapter |
 * | `LeReadRssi` | No | Yes | Yes | Adapter |
 * | `LeConnectionPriority` | Adapter | No | No | Adapter |
 * | `RequestEnable` | Adapter | No | No | Adapter |
 * | `PairedDevicesQuery` | Adapter | Yes | No | Adapter |
 *
 * [[Note: On Windows the answers come from the adapter, which ${function.bluetooth_initialize} queries. Ask after it has been called, and ask again when the state callback reports a change, since an adapter that is added or removed changes them.]]
 * @param {Enum.BluetoothFeature} feature The feature to check.
 * @returns {Bool} `true` if the platform and adapter can do the feature, otherwise `false`.
 * @example
 * ```gml
 * can_advertise = bluetooth_feature_is_supported(BluetoothFeature.LeAdvertise);
 * can_send_manufacturer_data = bluetooth_feature_is_supported(BluetoothFeature.LeAdvertiseManufacturerData);
 *
 * if (can_advertise && !can_send_manufacturer_data)
 * {
 *     show_debug_message("Advertising without manufacturer data on this platform");
 * }
 * ```
 * The above code checks whether the game can advertise and whether the advertisement can carry manufacturer data, and stores both answers in instance variables. On a platform that can advertise but cannot send manufacturer data, such as iOS, a message is written to the output log.
 * @function_end
 */

/**
 * @function bluetooth_permission_get_status
 * @desc This function returns whether the game has the Bluetooth permissions the platform needs. It does not ask the user; use ${function.bluetooth_permission_request} for that.
 *
 * The function returns ${constant.BluetoothPermissionStatus}.Unknown before ${function.bluetooth_initialize} has been called.
 *
 * [[Note: On Windows the function always returns ${constant.BluetoothPermissionStatus}.Granted, since a desktop app needs no Bluetooth permission.]]
 *
 * [[Note: On Android the function returns ${constant.BluetoothPermissionStatus}.Granted when every runtime permission the current Android version needs is granted: `BLUETOOTH_SCAN`, `BLUETOOTH_CONNECT` and `BLUETOOTH_ADVERTISE` from Android 12, `ACCESS_FINE_LOCATION` on Android 6 to 11. Android cannot tell a permission that was never asked for from one that was refused, so the extension remembers that it asked: the status is ${constant.BluetoothPermissionStatus}.Unknown until ${function.bluetooth_permission_request} has been called once, and ${constant.BluetoothPermissionStatus}.Denied after that while a permission is missing. Clearing the app's data resets it.]]
 *
 * [[Note: On iOS and macOS the function reports the app's Bluetooth authorisation: ${constant.BluetoothPermissionStatus}.Unknown until the user has answered the prompt that ${function.bluetooth_permission_request} shows, and ${constant.BluetoothPermissionStatus}.Denied when the user refused it or it is restricted on the device.]]
 * @returns {Enum.BluetoothPermissionStatus} The game's Bluetooth permission status.
 * @example
 * ```gml
 * var _status = bluetooth_permission_get_status();
 * if (_status == BluetoothPermissionStatus.Denied)
 * {
 *     show_debug_message("Bluetooth permission was refused, enable it in the system settings");
 * }
 * ```
 * The above code reads the permission status and writes a message to the output log when the permission was refused.
 * @function_end
 */

/**
 * @function bluetooth_permission_request
 * @desc This function asks for the Bluetooth permissions the platform needs, showing the system prompt when the user has not answered it yet. The callback is called once with the answer: when the user answers the prompt, or without a prompt when the answer is already known.
 *
 * The callback's `error_code` is ${constant.BluetoothError}.Ok whenever the request was answered, also when the user refused it, so check its `status` argument to see whether the permission was granted. If you call this function again while the prompt is still up, no second prompt is shown and both callbacks get the same answer. If ${function.bluetooth_shutdown} is called before the answer, the callback is called with ${constant.BluetoothError}.NotInitialized and ${constant.BluetoothPermissionStatus}.Unknown.
 *
 * The function returns a ${constant.BluetoothError}. It returns ${constant.BluetoothError}.NotInitialized before ${function.bluetooth_initialize} has been called, and on Android ${constant.BluetoothError}.OperationFailed when the prompt cannot be shown. The reason is then in ${function.bluetooth_last_error_code} and ${function.bluetooth_last_error_message}, and the callback is never called.
 *
 * After the user answers, the callback set with ${function.bluetooth_set_callback_state_changed} reports the new adapter state on Android, iOS and macOS.
 *
 * [[Note: On Windows no permission is needed: the callback is called with ${constant.BluetoothPermissionStatus}.Granted without a prompt.]]
 *
 * [[Note: On Android 12 and later the function asks for `BLUETOOTH_SCAN`, `BLUETOOTH_CONNECT` and `BLUETOOTH_ADVERTISE`; on Android 6 to 11 it asks for `ACCESS_FINE_LOCATION` and `ACCESS_COARSE_LOCATION`. Before Android 6 the permissions are granted at install time and the callback is called without a prompt. The permissions must be declared in the game's manifest, see ${page.platform_setup}.]]
 *
 * [[Important: On iOS and macOS the system prompt is shown only by this function. Until the user has answered it, every function that uses Bluetooth returns ${constant.BluetoothError}.PermissionDenied and the adapter state is ${constant.BluetoothState}.Unauthorized, as on Android without the runtime permissions. Once it has been answered, later calls report the answer without a prompt.]]
 * @param {Function} callback The function to call once with the answer.
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the request was made, otherwise the reason it was not.
 * @event callback
 * @desc Called once, when the request is answered or Bluetooth is shut down before the answer.
 * @member {Enum.BluetoothError} error_code ${constant.BluetoothError}.Ok if the request was answered, otherwise ${constant.BluetoothError}.NotInitialized.
 * @member {String} message A description of the failure, or an empty string when the request was answered.
 * @member {Enum.BluetoothPermissionStatus} status The permission status after the answer.
 * @event_end
 * @example
 * ```gml
 * var _error = bluetooth_permission_request(function(_error_code, _message, _status)
 * {
 *     if (_error_code != BluetoothError.Ok)
 *     {
 *         show_debug_message($"Permission request failed: {_message}");
 *         return;
 *     }
 *
 *     bluetooth_allowed = (_status == BluetoothPermissionStatus.Granted);
 *     show_debug_message($"Bluetooth permission granted: {bluetooth_allowed}");
 * });
 *
 * if (_error != BluetoothError.Ok)
 * {
 *     show_debug_message($"Permission request did not start: {bluetooth_last_error_message()}");
 * }
 * ```
 * The above code asks for the Bluetooth permissions. When the request is answered, the instance variable `bluetooth_allowed` is set to whether the permission was granted; when Bluetooth was shut down before the answer, the message is written to the output log instead. If the request could not be made at all, the reason is read with ${function.bluetooth_last_error_message} and the callback is never called.
 * @function_end
 */

/**
 * @function bluetooth_request_enable
 * @desc This function asks the user to turn the Bluetooth radio on. The callback is called once with the outcome: ${constant.BluetoothError}.Ok when the radio is on, ${constant.BluetoothError}.BluetoothDisabled when the user declined or the radio stayed off, ${constant.BluetoothError}.PermissionDenied when the system refused the request, and ${constant.BluetoothError}.NotInitialized when ${function.bluetooth_shutdown} is called before the answer. When the radio is already on, the callback is called with ${constant.BluetoothError}.Ok without asking the user.
 *
 * If you call this function again while a request is still waiting, no second request is made and both callbacks get the same outcome.
 *
 * The function returns a ${constant.BluetoothError}: ${constant.BluetoothError}.NotInitialized before ${function.bluetooth_initialize} has been called, ${constant.BluetoothError}.NotSupported when the platform cannot turn the radio on (see ${constant.BluetoothFeature}.RequestEnable), ${constant.BluetoothError}.PermissionDenied on Android 12 and later without the `BLUETOOTH_CONNECT` permission, and ${constant.BluetoothError}.OperationFailed when the request cannot be made. The reason is then in ${function.bluetooth_last_error_code} and ${function.bluetooth_last_error_message}, and the callback is never called.
 *
 * [[Note: On Android the function shows the system dialog that asks the user to turn Bluetooth on. From Android 12 it needs the `BLUETOOTH_CONNECT` permission, so call ${function.bluetooth_permission_request} first.]]
 *
 * [[Note: On Windows the function asks for access to the radio and switches it on, without a dialog of its own. The callback can also report ${constant.BluetoothError}.OperationFailed when Windows fails the request, and the function returns ${constant.BluetoothError}.NotSupported when there is no Bluetooth radio.]]
 *
 * [[Note: iOS and macOS have no way for an app to turn the radio on, so the function always returns ${constant.BluetoothError}.NotSupported there. Ask the player to turn Bluetooth on in the system settings instead.]]
 * @param {Function} callback The function to call once with the outcome.
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the request was made, otherwise the reason it was not.
 * @event callback
 * @desc Called once, when the radio is on, the request is declined or fails, or Bluetooth is shut down before the answer.
 * @member {Enum.BluetoothError} error_code ${constant.BluetoothError}.Ok if the radio is on, otherwise the reason it is not.
 * @member {String} message A description of the failure, or an empty string on success.
 * @event_end
 * @example
 * ```gml
 * if (bluetooth_feature_is_supported(BluetoothFeature.RequestEnable))
 * {
 *     var _error = bluetooth_request_enable(function(_error_code, _message)
 *     {
 *         if (_error_code != BluetoothError.Ok)
 *         {
 *             show_debug_message($"Bluetooth is still off: {_message}");
 *             return;
 *         }
 *         show_debug_message("Bluetooth is on");
 *     });
 *
 *     if (_error != BluetoothError.Ok)
 *     {
 *         show_debug_message($"Enable request did not start: {bluetooth_last_error_message()}");
 *     }
 * }
 * else
 * {
 *     show_debug_message("Please turn Bluetooth on in the system settings");
 * }
 * ```
 * The above code first checks with ${function.bluetooth_feature_is_supported} whether the platform can turn the radio on. If it can, the user is asked to turn it on and the callback writes the outcome to the output log; if the request could not be made, the reason is read with ${function.bluetooth_last_error_message}. On a platform that cannot do it, such as iOS, the player is told to use the system settings.
 * @function_end
 */

/**
 * @function bluetooth_value_fetch
 * @desc This function copies the bytes of a value into the given buffer, and frees the value. It is the one way to read every byte payload the extension delivers: an LE characteristic or descriptor read, a notification or indication, a write request to your LE server, and a chunk of data received on a Classic connection.
 *
 * The callbacks that deliver bytes do not pass them directly. They pass a value handle and the number of bytes it holds, `value` and `size`, and you call this function with that handle to get the bytes. A handle can be fetched only once: the value is freed when it has been copied.
 *
 * The buffer must have room for all `size` bytes at the given offset, that is, ${function.buffer_get_size} of the buffer must be at least `offset + size`; the whole value is copied, never a part of it. A `buffer_fixed` buffer of `size` bytes is the usual choice.
 *
 * The function returns ${constant.BluetoothError}.Ok when the bytes were copied. Otherwise it returns the reason, which is also in ${function.bluetooth_last_error_code} and ${function.bluetooth_last_error_message}:
 *
 * - ${constant.BluetoothError}.InvalidHandle for a value that is unknown, has already been fetched or released, or has expired. A failed read passes the value 0, which is always unknown.
 * - ${constant.BluetoothError}.InvalidArgument for a buffer that is too small. Nothing is copied and the value is kept, so you can call the function again with a larger buffer, or free it with ${function.bluetooth_value_release}.
 *
 * The bytes are written straight into the buffer's memory, so neither the buffer's used size nor its seek position changes. Read them with ${function.buffer_peek}, or with ${function.buffer_read} after ${function.buffer_seek} to `offset`. Call ${function.buffer_set_used_size} before you pass the buffer to a function that only works on its used size, such as ${function.buffer_copy} or ${function.buffer_save}, or use ${function.bluetooth_value_to_buffer} and ${function.bluetooth_value_append}, which set the used size for you.
 *
 * ### Usage Notes
 *
 * The extension holds each value until you fetch or release it, within these limits:
 *
 * - A value expires 30 seconds after its callback was queued. Fetch it in the callback that delivers it.
 * - At most 256 values from LE reads and notifications are held. Past that, the oldest of them is dropped.
 * - The value of a write request to your LE server is freed with its request, when you answer it with ${function.bluetooth_le_server_respond_write}, so fetch it before you answer.
 * - The values of a Classic connection count toward a limit of 1 MiB per connection (see ${function.bluetooth_classic_connect}). While its unfetched values hold more than that, Windows and Android stop reading from the peer until some are fetched or released; macOS ends the connection with ${constant.BluetoothError}.Disconnected. A Classic value that expires unfetched also ends its connection with ${constant.BluetoothError}.Disconnected, rather than leave a gap in the data.
 * - A value stays available after its connection ends, so bytes delivered just before a disconnect can still be fetched.
 * - ${function.bluetooth_shutdown} frees every value.
 *
 * Use ${function.bluetooth_value_release} to free a value whose bytes you do not need.
 * @param {Real} value The value handle a callback delivered.
 * @param {Buffer} out_data The buffer to copy the bytes into.
 * @param {Real} offset The position in the buffer to copy the bytes to (in bytes).
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the bytes were copied, otherwise the reason they were not.
 * @example
 * ```gml
 * var _error = bluetooth_le_characteristic_read(characteristic, function(_error_code, _message, _characteristic, _value, _size)
 * {
 *     if (_error_code != BluetoothError.Ok)
 *     {
 *         show_debug_message($"Read failed: {_message}");
 *         return;
 *     }
 *
 *     var _buffer = buffer_create(max(_size, 1), buffer_fixed, 1);
 *     if (bluetooth_value_fetch(_value, _buffer, 0) != BluetoothError.Ok)
 *     {
 *         show_debug_message($"Fetch failed: {bluetooth_last_error_message()}");
 *     }
 *     else if (_size > 0)
 *     {
 *         var _first = buffer_peek(_buffer, 0, buffer_u8);
 *         show_debug_message($"Read {_size} bytes, the first is {_first}");
 *     }
 *     buffer_delete(_buffer);
 * });
 *
 * if (_error != BluetoothError.Ok)
 * {
 *     show_debug_message($"Read did not start: {bluetooth_last_error_message()}");
 * }
 * ```
 * The above code reads the characteristic stored in the instance variable `characteristic` with ${function.bluetooth_le_characteristic_read}. When the read succeeds, a fixed buffer the size of the value is created (at least 1 byte, since a value can be empty) and ${function.bluetooth_value_fetch} copies the bytes into it at offset 0, freeing the value. Since the buffer holds the whole value, the fetch can only fail for a value that has expired. The first byte is then read with ${function.buffer_peek}, which reads the buffer's memory directly, and the buffer is deleted. If the read could not start, the reason is read with ${function.bluetooth_last_error_message} and the callback is never called.
 * @function_end
 */

/**
 * @function bluetooth_value_release
 * @desc This function frees a value without copying its bytes. Use it for a value whose bytes you do not need, so that it does not count toward the limits described in ${function.bluetooth_value_fetch} until it expires: on a Classic connection, this lets the connection keep receiving. Every callback that delivers a value should either fetch it or release it, including on an early return.
 *
 * The function returns ${constant.BluetoothError}.Ok when the value was freed, and ${constant.BluetoothError}.InvalidHandle for a value that is unknown, has already been fetched or released, or has expired; the reason is then in ${function.bluetooth_last_error_message}.
 * @param {Real} value The value handle a callback delivered.
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the value was freed, otherwise ${constant.BluetoothError}.InvalidHandle.
 * @example
 * ```gml
 * measurement_buffer = buffer_create(64, buffer_fixed, 1);
 *
 * on_measurement = function(_characteristic, _connection, _value, _size)
 * {
 *     if (_connection != connection || paused || _size > buffer_get_size(measurement_buffer))
 *     {
 *         bluetooth_value_release(_value);
 *         return;
 *     }
 *
 *     if (bluetooth_value_fetch(_value, measurement_buffer, 0) != BluetoothError.Ok)
 *     {
 *         show_debug_message($"Fetch failed: {bluetooth_last_error_message()}");
 *         return;
 *     }
 *     measurement_size = _size;
 * };
 * ```
 * The above code creates a 64-byte buffer and the method `on_measurement`, which is passed as the `on_value` callback of ${function.bluetooth_le_characteristic_subscribe}. A value that arrives for a connection other than the one stored in `connection`, while the instance's `paused` flag is set, or that is too large for the buffer is freed with ${function.bluetooth_value_release} without being read. Any other value is copied into the buffer with ${function.bluetooth_value_fetch}; since the buffer is known to be large enough, the fetch can only fail for a value that has expired, and the failure is written to the output log. On success the number of bytes is kept in `measurement_size`.
 * @function_end
 */

/**
 * @function bluetooth_value_to_buffer
 * @desc This function copies a value into a new buffer. It fetches the value with ${function.bluetooth_value_fetch}, which frees it, and sets the used size of the new buffer to the size of the value, so the buffer can be passed straight to functions that work on the used size, such as ${function.buffer_copy} or ${function.buffer_save}. You must delete the buffer when you no longer need it.
 *
 * If the value cannot be fetched, the function returns -1 and leaves nothing for you to free; the reason, normally ${constant.BluetoothError}.InvalidHandle for a value that has expired or was already fetched, is in ${function.bluetooth_last_error_code} and ${function.bluetooth_last_error_message}.
 *
 * [[Note: This function is a GML helper written in the `GMBluetooth_helpers` script that ships with the extension, not a function of the native extension. It calls only the public functions documented here.]]
 * @param {Real} value The value handle a callback delivered.
 * @param {Real} size The size of the value (in bytes), as the callback reported it.
 * @returns {Buffer|Real} The new buffer holding the value, or -1 if the value could not be fetched.
 * @example
 * ```gml
 * var _error = bluetooth_le_characteristic_read(battery_level, function(_error_code, _message, _characteristic, _value, _size)
 * {
 *     if (_error_code != BluetoothError.Ok)
 *     {
 *         show_debug_message($"Read failed: {_message}");
 *         return;
 *     }
 *
 *     var _buffer = bluetooth_value_to_buffer(_value, _size);
 *     if (_buffer == -1)
 *     {
 *         show_debug_message($"Fetch failed: {bluetooth_last_error_message()}");
 *         return;
 *     }
 *     buffer_save(_buffer, "battery_level.bin");
 *     buffer_delete(_buffer);
 * });
 *
 * if (_error != BluetoothError.Ok)
 * {
 *     show_debug_message($"Read did not start: {bluetooth_last_error_message()}");
 * }
 * ```
 * The above code reads the characteristic stored in `battery_level`. On success the callback copies the value into a new buffer with ${function.bluetooth_value_to_buffer}, saves it to a file with ${function.buffer_save}, which writes the buffer's used size, and deletes the buffer. If the value could not be fetched, or the read failed or could not start, the reason is written to the output log instead.
 * @function_end
 */

/**
 * @function bluetooth_value_append
 * @desc This function adds a value to the end of the data in the given buffer. It grows the buffer when the value does not fit after the buffer's used size, fetches the value there with ${function.bluetooth_value_fetch}, which frees it, and moves the used size past the new bytes. This makes it the simplest way to collect the chunks of a Classic connection, which arrive in order but with no message boundaries.
 *
 * The function returns the ${constant.BluetoothError} of the fetch: ${constant.BluetoothError}.Ok when the bytes were added, otherwise the reason, which is also in ${function.bluetooth_last_error_code} and ${function.bluetooth_last_error_message}, normally ${constant.BluetoothError}.InvalidHandle for a value that has expired or was already fetched.
 *
 * [[Note: This function is a GML helper written in the `GMBluetooth_helpers` script that ships with the extension, not a function of the native extension. It calls only the public functions documented here.]]
 * @param {Real} value The value handle a callback delivered.
 * @param {Real} size The size of the value (in bytes), as the callback reported it.
 * @param {Buffer} buffer The buffer to add the bytes to, at its used size.
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the bytes were added, otherwise the reason they were not.
 * @example
 * ```gml
 * receive_buffer = buffer_create(1024, buffer_grow, 1);
 *
 * on_data = function(_connection, _value, _size)
 * {
 *     if (_connection != connection)
 *     {
 *         bluetooth_value_release(_value);
 *         return;
 *     }
 *
 *     if (bluetooth_value_append(_value, _size, receive_buffer) != BluetoothError.Ok)
 *     {
 *         show_debug_message($"Receive failed: {bluetooth_last_error_message()}");
 *         return;
 *     }
 *     show_debug_message($"{buffer_get_used_size(receive_buffer)} bytes received so far");
 * };
 * ```
 * The above code creates a buffer for the bytes a Classic connection receives and the method `on_data`, which is passed as the `on_data` callback of ${function.bluetooth_classic_connect}. A chunk for a connection other than the one stored in `connection` is freed with ${function.bluetooth_value_release}. Any other chunk is added to the end of `receive_buffer` with ${function.bluetooth_value_append}, which grows the buffer when needed and moves its used size, so the game can look for complete messages in the buffer's used bytes.
 * @function_end
 */

/**
 * @function bluetooth_set_callback_state_changed
 * @desc This function sets the function to call when the state of the local Bluetooth adapter changes, for example when the user turns Bluetooth on or off. It replaces the callback set by an earlier call. It is the one global callback of the extension: every other event is passed to the call that starts its subject, such as a scan or a connection.
 *
 * The callback is called with the current state when you set it, again when ${function.bluetooth_initialize} succeeds, and then whenever the state changes. Before ${function.bluetooth_initialize} the current state is ${constant.BluetoothState}.Unknown. The callback stays set across ${function.bluetooth_shutdown} and the next ${function.bluetooth_initialize}, until you remove it with ${function.bluetooth_remove_callback_state_changed}.
 *
 * When the state becomes anything but ${constant.BluetoothState}.PoweredOn or ${constant.BluetoothState}.Unknown, everything that was running ends before the callback is called: see ${constant.BluetoothState}.
 *
 * The function always returns `true`.
 *
 * [[Note: On Android the callback is also called after the user answers ${function.bluetooth_permission_request}. From Android 12, a radio that is on reports ${constant.BluetoothState}.Unauthorized while the game does not have the `BLUETOOTH_SCAN` and `BLUETOOTH_CONNECT` permissions.]]
 *
 * [[Note: On iOS and macOS the state is ${constant.BluetoothState}.Unauthorized until the user has answered the prompt of ${function.bluetooth_permission_request}, and the callback reports the state again once the user has answered.]]
 * @param {Function} callback The function to call with the adapter state.
 * @returns {Bool} Always `true`.
 * @event callback
 * @desc Called once when the callback is set, and then whenever the state of the local Bluetooth adapter changes.
 * @member {Enum.BluetoothState} state The new state of the local Bluetooth adapter.
 * @event_end
 * @example
 * ```gml
 * bluetooth_set_callback_state_changed(function(_state)
 * {
 *     bluetooth_ready = (_state == BluetoothState.PoweredOn);
 *     if (_state == BluetoothState.PoweredOff)
 *     {
 *         show_debug_message("Bluetooth is off");
 *     }
 * });
 * ```
 * The above code sets a callback that keeps the instance variable `bluetooth_ready` up to date with whether the adapter is on and usable, and writes a message to the output log when it is turned off.
 * @function_end
 */

/**
 * @function bluetooth_remove_callback_state_changed
 * @desc This function removes the callback set with ${function.bluetooth_set_callback_state_changed}, so adapter state changes are no longer reported. It always returns `true`, also when no callback was set.
 * @returns {Bool} Always `true`.
 * @example
 * ```gml
 * bluetooth_remove_callback_state_changed();
 * ```
 * The above code stops the reporting of adapter state changes.
 * @function_end
 */

/**
 * @struct BluetoothConnectOptions
 * @desc This struct says how ${function.bluetooth_le_connect} and ${function.bluetooth_classic_connect} run a connect. You create it with `new BluetoothConnectOptions()`; a variable left `undefined` takes its default, and you can pass `undefined` instead of the struct to use the defaults for everything.
 * @member {Real} [timeout_ms] The whole time from the call to an open link (in milliseconds), a pairing prompt included: 1000 to 300000, or `undefined` for 15000. A connect that is still not open when it runs out fails with ${constant.BluetoothError}.Timeout on every platform; a value outside the range makes the connect return 0 with ${constant.BluetoothError}.InvalidArgument.
 * @struct_end
 */

/**
 * @const BluetoothError
 * @desc This enum holds the result codes of the extension. Every function that can fail returns one of them, except those that return a handle: ${function.bluetooth_le_connect} and ${function.bluetooth_classic_connect} (so that a disconnect can cancel a connect still in progress), ${function.bluetooth_device_from_id}, ${function.bluetooth_le_service_find}, ${function.bluetooth_le_characteristic_find} and ${function.bluetooth_le_descriptor_find}, which return 0 when they fail. After a failed call, ${function.bluetooth_last_error_code} returns the code and ${function.bluetooth_last_error_message} describes it. A call that fails never calls its callbacks.
 *
 * A callback that reports the outcome of an operation receives one of these codes as its first argument, `error_code`, followed by a `message` that describes a failure. An event that cannot fail, such as a device found by a scan, carries data only.
 *
 * The same condition fails with the same code on every platform, and these conditions are checked before anything starts, so they come back from the call itself:
 *
 * | Condition | Code |
 * | --- | --- |
 * | A handle that is not, or is no longer, valid | `InvalidHandle` |
 * | A device of the wrong transport, or another bad argument | `InvalidArgument` |
 * | The radio is off or resetting | `BluetoothDisabled` |
 * | The permission is missing or was refused | `PermissionDenied` |
 * | No Bluetooth hardware, or a feature the platform cannot do (see ${function.bluetooth_feature_is_supported}) | `NotSupported` |
 * | The LE server is not running | `OperationFailed` |
 * | A start while it already runs, a subscribe while subscribed, a duplicate service, or a full send or notify queue | `Busy` |
 *
 * A stop while nothing runs returns `Ok` and calls nothing. What the game ends fires no event: a stop or a disconnect that you ask for takes effect when the call returns, and no `on_stopped` or `on_disconnected` callback is called for it, nor for the centrals and the advertisement that ${function.bluetooth_le_server_stop} ends.
 *
 * An outcome the extension recognises reads the same on every platform: when the other device closes the link, the message is `Remote device disconnected`, and a Classic connect to a service the device does not offer fails with `ConnectionFailed` and `The device does not offer service <uuid>, or nothing is listening on it`. A failure the extension cannot classify carries the platform's own text, as `<operation> failed (<code>): <system text>`.
 * @member Ok The call or operation succeeded.
 * @member Unknown The platform reported an error that matches no other code; the message holds its description.
 * @member NotSupported The device has no Bluetooth hardware, or the platform or adapter cannot do what was asked.
 * @member NotInitialized The extension is not running: ${function.bluetooth_initialize} has not been called, or ${function.bluetooth_shutdown} ended the operation.
 * @member BluetoothDisabled The Bluetooth radio is off or resetting, or the user declined to turn it on. Also the reason a scan, connection, advertisement or server ended when the radio turned off.
 * @member PermissionDenied The game does not have the Bluetooth permission, or the user or the system refused it.
 * @member InvalidArgument An argument is not valid, such as a device of the wrong transport or a buffer that is too small.
 * @member InvalidHandle A handle that is not, or is no longer, valid.
 * @member Busy The operation cannot start while it, or another one, is in progress: a start while the scan, server or advertisement already runs, a subscribe while subscribed, a duplicate service or a full send or notify queue.
 * @member Timeout The remote device did not answer in time.
 * @member NotFound What was asked for does not exist, such as a device id the system does not know.
 * @member ConnectionFailed The connection could not be made: the device refused it, the link failed or the connect was cancelled.
 * @member Disconnected The connection ended before the operation completed.
 * @member OperationFailed The platform could not carry out the operation, or the LE server is not running.
 * @member NotPermitted The remote device refused the attribute operation, because the read or write is not permitted or the request is not supported; the message names the ATT error.
 * @member InsufficientSecurity The remote device requires pairing, encryption or authorisation before the attribute operation; the message names the ATT error.
 * @const_end
 */

/**
 * @const BluetoothState
 * @desc This enum holds the states of the local Bluetooth adapter, which the callback set with ${function.bluetooth_set_callback_state_changed} receives.
 *
 * When the state becomes anything but PoweredOn or Unknown, everything running ends, before the state callback is called: each LE and Classic scan and the advertisement call their `on_stopped` with ${constant.BluetoothError}.BluetoothDisabled, each connection, LE or Classic, and each central of the LE server call their `on_disconnected` with ${constant.BluetoothError}.BluetoothDisabled, and a connect in progress, LE or Classic, fails with ${constant.BluetoothError}.BluetoothDisabled. Each server, LE and Classic, then calls its `on_stopped` with ${constant.BluetoothError}.BluetoothDisabled, after the `on_disconnected` of each central or connection it served; the LE server's services and attribute handles are gone. Discoverability ends too, and its `on_stopped` is called with ${constant.BluetoothError}.BluetoothDisabled (see ${function.bluetooth_classic_discoverable_start}). Nothing restarts when the radio comes back: start what you need again once the state is PoweredOn.
 * @member Unknown The state is not known yet, for example before ${function.bluetooth_initialize} or while the platform is starting. A scan started now begins once the radio is ready.
 * @member Resetting The adapter is changing state, for example turning on or off. Calls that use the radio return ${constant.BluetoothError}.BluetoothDisabled while it lasts.
 * @member Unsupported The device has no Bluetooth hardware the extension can use.
 * @member Unauthorized The game is not allowed to use Bluetooth: the permission has not been granted yet or was refused.
 * @member PoweredOff The Bluetooth radio is off or disabled.
 * @member PoweredOn The Bluetooth radio is on and the game can use it.
 * @const_end
 */

/**
 * @const BluetoothPermissionStatus
 * @desc This enum holds the permission states that ${function.bluetooth_permission_get_status} returns and the callback of ${function.bluetooth_permission_request} receives.
 * @member Unknown The user has not been asked yet, or the extension is not running.
 * @member Granted The game has every Bluetooth permission the platform needs.
 * @member Denied The user refused a permission the game needs, or it is restricted on the device.
 * @const_end
 */

/**
 * @const BluetoothFeature
 * @desc This enum holds the features that ${function.bluetooth_feature_is_supported} can be asked about. See that function for which platforms support each one.
 * @member LeCentral The game can scan for LE devices and connect to them as a client.
 * @member LePassiveScan An LE scan whose ${struct.BluetoothLeScanSettings} sets `active` to `false` really scans passively; elsewhere `active` is ignored.
 * @member LeAdvertise The game can advertise.
 * @member LeAdvertiseName An advertisement can carry the device name.
 * @member LeAdvertiseServiceUuids An advertisement can carry service UUIDs.
 * @member LeAdvertiseServiceData An advertisement can carry service data.
 * @member LeAdvertiseManufacturerData An advertisement can carry manufacturer data.
 * @member LeAdvertiseTxPower The advertising transmit power can be chosen.
 * @member LeAdvertiseIncludeTxPower An advertisement can carry its transmit power level.
 * @member LeAdvertiseNonConnectable An advertisement can be non-connectable.
 * @member LeServer The game can run an LE GATT server.
 * @member LeServerDescriptorRequests Reads and writes of a server descriptor with no static value reach the server's `on_read` and `on_write`.
 * @member LeServerSignedWrite A server attribute can use the signed write permissions.
 * @member LeServerConnectionEvents The server's `on_connected` and `on_disconnected` report a central as it connects and disconnects, not only around its requests and subscriptions.
 * @member LePairing ${function.bluetooth_pair} can pair an LE device.
 * @member Classic The game can scan for and connect to Classic devices.
 * @member ClassicServer The game can run a Classic server.
 * @member ClassicPairing ${function.bluetooth_pair} can pair a Classic device.
 * @member ClassicDiscoverable ${function.bluetooth_classic_discoverable_start} can make the device discoverable.
 * @member ClassicDiscoverableStop ${function.bluetooth_classic_discoverable_stop} can end discoverability early.
 * @member PermissionRequest ${function.bluetooth_permission_request} shows a system prompt.
 * @member LeMtuRequest ${function.bluetooth_le_connection_request_mtu} negotiates the MTU asked for; elsewhere the platform negotiates it and the request reports that value.
 * @member LeReadRssi ${function.bluetooth_le_connection_read_rssi} can read the signal strength of a connection.
 * @member LeConnectionPriority ${function.bluetooth_le_connection_request_priority} can change a connection's priority.
 * @member RequestEnable ${function.bluetooth_request_enable} can turn the Bluetooth radio on.
 * @member PairedDevicesQuery ${function.bluetooth_paired_devices_query} can list the paired devices.
 * @member LeScanMode The `mode` of a ${struct.BluetoothLeScanSettings} changes how the scan runs; elsewhere it is ignored.
 * @const_end
 */

/**
 * @const BluetoothTransport
 * @desc This enum holds the Bluetooth transports a device or a scan can use.
 * @member Unknown The transport is not known, for example for a device handle that is not valid.
 * @member Classic Bluetooth Classic (BR/EDR), which the `bluetooth_classic_*` functions use.
 * @member LowEnergy Bluetooth Low Energy, which the `bluetooth_le_*` functions use.
 * @const_end
 */

/**
 * @module core
 * @title Core
 * @desc The functions to start and stop the extension, read the error of a failed call, check the platform's capabilities and permissions, turn the radio on, read the bytes that callbacks deliver, and follow the state of the local Bluetooth adapter.
 *
 * Two of the value functions, ${function.bluetooth_value_to_buffer} and ${function.bluetooth_value_append}, are GML helpers written in the `GMBluetooth_helpers` script that ships with the extension, built on ${function.bluetooth_value_fetch}.
 *
 * @section_func
 * @desc These are the core functions of the extension.
 * @ref bluetooth_initialize
 * @ref bluetooth_shutdown
 * @ref bluetooth_is_initialized
 * @ref bluetooth_last_error_code
 * @ref bluetooth_last_error_message
 * @ref bluetooth_feature_is_supported
 * @ref bluetooth_permission_get_status
 * @ref bluetooth_permission_request
 * @ref bluetooth_request_enable
 * @ref bluetooth_value_fetch
 * @ref bluetooth_value_release
 * @ref bluetooth_value_to_buffer
 * @ref bluetooth_value_append
 * @ref bluetooth_set_callback_state_changed
 * @ref bluetooth_remove_callback_state_changed
 * @section_end
 *
 * @section_struct
 * @desc This is the struct shared by the LE and Classic connect functions.
 * @ref BluetoothConnectOptions
 * @section_end
 *
 * @section_const
 * @desc These are the constants used across the extension.
 * @ref BluetoothError
 * @ref BluetoothState
 * @ref BluetoothPermissionStatus
 * @ref BluetoothFeature
 * @ref BluetoothTransport
 * @section_end
 *
 * @module_end
 */
