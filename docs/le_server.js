// Functions

/**
 * @function bluetooth_le_server_start
 * @desc This function starts the local GATT server, so that this device can act as an LE peripheral and remote centrals can connect to it. There is one server: calling this function while it is already running returns ${constant.BluetoothError}.Ok and starts nothing new. Once the server runs, you add its services with ${function.bluetooth_le_server_add_service}.
 *
 * The function returns ${constant.BluetoothError}.Ok when the server is running. Otherwise it returns the reason, which is also in ${function.bluetooth_last_error_code} and ${function.bluetooth_last_error_message}: NotInitialized before ${function.bluetooth_initialize}, NotSupported when the device cannot act as a peripheral (see ${function.bluetooth_le_server_is_supported}), PermissionDenied when the Bluetooth permission is missing or was refused, or BluetoothDisabled when the radio is off.
 *
 * [[Note: On Android 12 and later the server needs the `BLUETOOTH_CONNECT` permission, and the call returns PermissionDenied without it.]]
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the server is running, otherwise the reason it could not start.
 * @example
 * ```gml
 * var _result = bluetooth_le_server_start();
 * if (_result != BluetoothError.Ok)
 * {
 *     show_debug_message($"Server did not start: {bluetooth_last_error_message()}");
 * }
 * else
 * {
 *     show_debug_message("Server running, adding the services");
 *     add_services();
 * }
 * ```
 * The above code starts the GATT server. If the call fails, the reason is read with ${function.bluetooth_last_error_message}; otherwise the instance's own `add_services` method is called to add the services with ${function.bluetooth_le_server_add_service}.
 * @function_end
 */

/**
 * @function bluetooth_le_server_stop
 * @desc This function stops the GATT server and removes every service it holds.
 *
 * Before the server stops, every read and write request still waiting for an answer is answered with ${constant.BluetoothAttError}.UnlikelyError, and every ${function.bluetooth_le_server_add_service} still in progress calls its callback with ${constant.BluetoothError}.OperationFailed. The server connection callback (see ${function.bluetooth_set_callback_le_server_connection_state_changed}) is then called with a disconnect for each central still connected, and every service, characteristic and descriptor handle stops being valid.
 *
 * Stopping a server that is not running returns ${constant.BluetoothError}.Ok and does nothing. The function returns NotInitialized before ${function.bluetooth_initialize}, with the reason in ${function.bluetooth_last_error_message}.
 *
 * [[Note: On macOS and iOS stopping the server also stops advertising.]]
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the server is stopped, otherwise the reason it could not stop.
 * @example
 * ```gml
 * bluetooth_le_server_stop();
 * service = 0;
 * characteristic_status = 0;
 * ```
 * The above code stops the GATT server and clears the instance variables that held its service and characteristic handles, since they are no longer valid.
 * @function_end
 */

/**
 * @function bluetooth_le_server_add_service
 * @desc This function adds a primary GATT service, described by the given ${struct.BluetoothLeServiceDefinition}, to the running GATT server. The service is added in the background, and the callback is called once when it has been added or has failed.
 *
 * The function returns ${constant.BluetoothError}.Ok when the add has started. Otherwise it returns the reason, which is also in ${function.bluetooth_last_error_code} and ${function.bluetooth_last_error_message}, and the callback is never called:
 *
 * * NotInitialized before ${function.bluetooth_initialize}.
 * * InvalidArgument for a UUID that is not valid, characteristic `properties` with bits that are not ${constant.BluetoothLeCharacteristicProperty} flags, `permissions` with bits that are not ${constant.BluetoothLeAttributePermission} flags, or an initial `value` longer than 512 bytes.
 * * OperationFailed when the server is not running: call ${function.bluetooth_le_server_start} first.
 * * Busy when a service with the same UUID has already been added.
 * * NotSupported for a permission or a descriptor the platform cannot host (see the notes below).
 *
 * On success the callback receives the handle of the new server service. You walk it with the LE client getters: ${function.bluetooth_le_characteristic_get_count} and ${function.bluetooth_le_characteristic_get_at} list its characteristics, and ${function.bluetooth_le_descriptor_get_count} and ${function.bluetooth_le_descriptor_get_at} the descriptors of each characteristic, while ${function.bluetooth_le_characteristic_get_uuid}, ${function.bluetooth_le_characteristic_get_properties} and ${function.bluetooth_le_descriptor_get_uuid} read them. They are listed in definition order, so the characteristic at index 0 is the first entry of the definition's `characteristics` array, and two attributes with the same UUID each get their own handle. A Client Characteristic Configuration descriptor (0x2902) is managed by the platform: it gets no handle and is left out of the list, even when the definition names it, so the descriptor indices count only the other descriptors.
 *
 * These are the handles that ${function.bluetooth_le_server_notify_value} takes and that the read and write request callbacks report. They stay valid until ${function.bluetooth_le_server_clear_services}, ${function.bluetooth_le_server_stop} or ${function.bluetooth_shutdown}, and a client call (read, write, subscribe or discover) given one returns InvalidHandle.
 *
 * [[Note: On Windows the system publishes the Characteristic User Description descriptor (0x2901) itself, and a definition that names it makes the whole add fail in the callback with ${constant.BluetoothError}.OperationFailed ("The provided descriptor uuid is reserved and will be automatically published by the system."). Use a custom 128-bit descriptor UUID there instead.]]
 *
 * [[Note: On macOS and iOS the only descriptor a server can host is the Characteristic User Description (0x2901), which is served with an empty value; any other descriptor returns NotSupported. Windows, macOS and iOS have no signed write permission, so ${constant.BluetoothLeAttributePermission}.WriteSigned and WriteSignedMitm return NotSupported there.]]
 *
 * [[Note: On macOS and iOS the platform drops every service when the Bluetooth radio turns off. Their handles stop being valid, and you must add the services again once the state callback (see ${function.bluetooth_set_callback_state_changed}) reports ${constant.BluetoothState}.PoweredOn.]]
 * @param {Struct.BluetoothLeServiceDefinition} service The definition of the service to add.
 * @param {Function} callback The function to call once the service has been added or has failed.
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the add has started, otherwise the reason it did not.
 * @event callback
 * @desc Called once, when the service has been added or the add has failed.
 * @member {Enum.BluetoothError} error_code ${constant.BluetoothError}.Ok if the service was added, otherwise the reason it was not.
 * @member {String} message A description of the failure, or an empty string on success.
 * @member {Real} service The handle of the new server service, or 0 on failure.
 * @event_end
 * @example
 * ```gml
 * var _characteristic = new BluetoothLeCharacteristicDefinition();
 * _characteristic.uuid = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";
 * _characteristic.properties = BluetoothLeCharacteristicProperty.Read | BluetoothLeCharacteristicProperty.Notify;
 * _characteristic.permissions = BluetoothLeAttributePermission.Read;
 * _characteristic.value = [];
 * _characteristic.descriptors = [];
 *
 * var _definition = new BluetoothLeServiceDefinition();
 * _definition.uuid = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
 * _definition.characteristics = [_characteristic];
 *
 * var _result = bluetooth_le_server_add_service(_definition, function(_error_code, _message, _service)
 * {
 *     if (_error_code != BluetoothError.Ok)
 *     {
 *         show_debug_message($"Service not added: {_message}");
 *         exit;
 *     }
 *     service = _service;
 *     characteristic_status = bluetooth_le_characteristic_get_at(_service, 0);
 *     show_debug_message($"Service added with {bluetooth_le_characteristic_get_count(_service)} characteristic(s)");
 * });
 *
 * if (_result != BluetoothError.Ok)
 * {
 *     show_debug_message($"Add did not start: {bluetooth_last_error_message()}");
 * }
 * ```
 * The above code defines a service with one characteristic that can be read and notified, and has no initial value, so its reads reach the read request callback. The definition is passed to ${function.bluetooth_le_server_add_service}. When the callback reports success, the service handle is kept in `service` and the handle of its first (and only) characteristic, read with ${function.bluetooth_le_characteristic_get_at}, in `characteristic_status`. If the add could not start, the reason is read with ${function.bluetooth_last_error_message}.
 * @function_end
 */

/**
 * @function bluetooth_le_server_clear_services
 * @desc This function removes every service from the running GATT server, which keeps running.
 *
 * Every read and write request still waiting for an answer is answered with ${constant.BluetoothAttError}.UnlikelyError, every ${function.bluetooth_le_server_add_service} still in progress calls its callback with ${constant.BluetoothError}.OperationFailed, and every service, characteristic and descriptor handle stops being valid. A service UUID is free to be added again afterwards.
 *
 * Clearing a server that is not running returns ${constant.BluetoothError}.Ok and does nothing. The function returns NotInitialized before ${function.bluetooth_initialize}, with the reason in ${function.bluetooth_last_error_message}.
 *
 * [[Note: On Windows a service UUID that was advertised through one of the cleared services is no longer advertised.]]
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the services were cleared, otherwise the reason they were not.
 * @example
 * ```gml
 * if (bluetooth_le_server_clear_services() == BluetoothError.Ok)
 * {
 *     service = 0;
 *     add_services();
 * }
 * ```
 * The above code removes every service from the server, clears the stored service handle and calls the instance's own `add_services` method to add the services again.
 * @function_end
 */

/**
 * @function bluetooth_le_server_respond_read
 * @desc This function answers a read request that the read request callback (see ${function.bluetooth_set_callback_le_server_read_request}) reported. With ${constant.BluetoothAttError}.Success it sends `size` bytes of the given buffer, starting at `offset`, as the value read; any other code refuses the read, and the buffer, offset and size are then ignored. Answer with the bytes of the value from the request's own offset onwards, or with ${constant.BluetoothAttError}.InvalidOffset when that offset is past the end of the value.
 *
 * You must answer every read request, once. The request is checked before anything is sent, so a call that fails can be retried. The function returns ${constant.BluetoothError}.Ok when the answer was sent, otherwise the reason, which is also in ${function.bluetooth_last_error_code} and ${function.bluetooth_last_error_message}: NotInitialized before ${function.bluetooth_initialize}, InvalidHandle for a request id that is unknown, already answered or expired, InvalidArgument for an `error_code` that is not a ${constant.BluetoothAttError} member, for the id of a write request (answer it with ${function.bluetooth_le_server_respond_write}), or for an offset and size outside the buffer when sending a value, and OperationFailed when the platform could not send the answer.
 *
 * A request you do not answer expires 30 seconds after it arrived, which is the ATT transaction timeout: by then the central has usually given up on the link, and a call with its id returns InvalidHandle. Expired requests are swept when the next request arrives. Stopping the server or clearing its services answers every waiting request with ${constant.BluetoothAttError}.UnlikelyError.
 *
 * [[Note: On Windows, macOS and iOS the extension answers an expired request with ${constant.BluetoothAttError}.UnlikelyError when it sweeps it. On Android an expired request is dropped without an answer, and so is every request of a central that disconnects.]]
 *
 * [[Note: The `data` argument must be an existing buffer even when you refuse the read.]]
 * @param {Real} request_id The id of the request, as the read request callback reported it.
 * @param {Enum.BluetoothAttError} error_code ${constant.BluetoothAttError}.Success to send the value, or the ATT error to refuse the read with.
 * @param {Buffer} data The buffer holding the value to send.
 * @param {Real} offset The position in the buffer of the first byte to send (in bytes).
 * @param {Real} size The number of bytes to send.
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the answer was sent, otherwise the reason it was not.
 * @example
 * ```gml
 * bluetooth_set_callback_le_server_read_request(function(_request_id, _connection, _characteristic, _descriptor, _offset)
 * {
 *     if (_characteristic != characteristic_status || _descriptor != 0)
 *     {
 *         bluetooth_le_server_respond_read(_request_id, BluetoothAttError.ReadNotPermitted, reply_buffer, 0, 0);
 *         exit;
 *     }
 *
 *     buffer_seek(reply_buffer, buffer_seek_start, 0);
 *     buffer_write(reply_buffer, buffer_text, status_text);
 *     var _length = buffer_tell(reply_buffer);
 *     if (_offset > _length)
 *     {
 *         bluetooth_le_server_respond_read(_request_id, BluetoothAttError.InvalidOffset, reply_buffer, 0, 0);
 *         exit;
 *     }
 *
 *     bluetooth_le_server_respond_read(_request_id, BluetoothAttError.Success, reply_buffer, _offset, _length - _offset);
 * });
 * ```
 * The above code answers read requests. A request for any attribute other than the characteristic stored in `characteristic_status` is refused with ${constant.BluetoothAttError}.ReadNotPermitted. Otherwise the text in `status_text` is written to `reply_buffer`, a fixed buffer the instance created earlier, and the bytes from the requested offset onwards are sent; an offset past the end of the text is refused with ${constant.BluetoothAttError}.InvalidOffset.
 * @function_end
 */

/**
 * @function bluetooth_le_server_respond_write
 * @desc This function answers a write request that the write request callback (see ${function.bluetooth_set_callback_le_server_write_request}) reported. ${constant.BluetoothAttError}.Success accepts the write; any other code refuses it with that ATT error.
 *
 * You must call this function for every write request, once, including a write without response (`response_needed` is `false`): nothing is sent to the central then, but the request is released. The request's value is freed with it, so you must copy the value with ${function.bluetooth_value_fetch} before you answer. A write without response that you do not answer is dropped 5 seconds after it arrived, and any other write request expires after 30 seconds, as described for ${function.bluetooth_le_server_respond_read}.
 *
 * The function returns ${constant.BluetoothError}.Ok when the answer was sent or the request released, otherwise the reason, which is also in ${function.bluetooth_last_error_code} and ${function.bluetooth_last_error_message}: NotInitialized before ${function.bluetooth_initialize}, InvalidHandle for a request id that is unknown, already answered or expired, InvalidArgument for an `error_code` that is not a ${constant.BluetoothAttError} member or for the id of a read request (answer it with ${function.bluetooth_le_server_respond_read}), and OperationFailed when the platform could not send the answer. A call that fails a check can be retried.
 *
 * [[Note: A long write, or a central writing several attributes in one go, is answered once on Android, macOS and iOS: when the last of its requests has been answered, with the first error code given, or Success.]]
 * @param {Real} request_id The id of the request, as the write request callback reported it.
 * @param {Enum.BluetoothAttError} error_code ${constant.BluetoothAttError}.Success to accept the write, or the ATT error to refuse it with.
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the answer was sent or the request released, otherwise the reason it was not.
 * @example
 * ```gml
 * var _result = bluetooth_le_server_respond_write(_request_id, BluetoothAttError.Success);
 * if (_result != BluetoothError.Ok)
 * {
 *     show_debug_message($"Write not answered: {bluetooth_last_error_message()}");
 * }
 * ```
 * The above code, run inside the write request callback after the value has been fetched, accepts the write with the given request id and reports the reason if the answer could not be sent.
 * @function_end
 */

/**
 * @function bluetooth_le_server_notify_value
 * @desc This function pushes a new value of a server characteristic to the centrals subscribed to it, as a notification or an indication, whichever each central subscribed with. The characteristic is a handle from a service that ${function.bluetooth_le_server_add_service} added. With a connection of 0 the value goes to every subscriber; with a server connection handle (from the read, write or connection callbacks) it goes to that central only. A call with no subscriber to send to returns ${constant.BluetoothError}.Ok and sends nothing.
 *
 * The value is copied, so you can free the buffer as soon as the function returns. It is queued and sent as the link allows: while 64 values are waiting to be sent the function returns Busy, and you should try again later, for example on the next step.
 *
 * The function returns ${constant.BluetoothError}.Ok when the value was queued, otherwise the reason, which is also in ${function.bluetooth_last_error_code} and ${function.bluetooth_last_error_message}: NotInitialized before ${function.bluetooth_initialize}, InvalidHandle for a characteristic that is not a live server characteristic or a connection that is not a connected server connection, InvalidArgument for an offset and size outside the buffer, NotFound when the given central is not subscribed to the characteristic, or Busy.
 *
 * [[Note: On Android the limit of 64 counts one value per subscriber, so a value sent to every subscriber takes one place for each of them.]]
 * @param {Real} characteristic The handle of the server characteristic whose value changed.
 * @param {Real} connection The server connection handle of the central to send to, or 0 for every subscriber.
 * @param {Buffer} data The buffer holding the new value.
 * @param {Real} offset The position in the buffer of the first byte to send (in bytes).
 * @param {Real} size The number of bytes to send.
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the value was queued, otherwise the reason it was not.
 * @example
 * ```gml
 * var _buffer = buffer_create(4, buffer_fixed, 1);
 * buffer_write(_buffer, buffer_u32, score);
 * var _result = bluetooth_le_server_notify_value(characteristic_status, 0, _buffer, 0, 4);
 * buffer_delete(_buffer);
 *
 * if (_result == BluetoothError.Busy)
 * {
 *     notify_pending = true;
 * }
 * else if (_result != BluetoothError.Ok)
 * {
 *     show_debug_message($"Notify failed: {bluetooth_last_error_message()}");
 * }
 * ```
 * The above code writes the instance's `score` to a new buffer as a 32-bit value and sends it to every central subscribed to the characteristic in `characteristic_status`. The buffer is deleted straight away, since the value has been copied. When too many values are waiting the instance sets `notify_pending` to try again later; any other failure is reported with ${function.bluetooth_last_error_message}.
 * @function_end
 */

/**
 * @function bluetooth_le_advertise_start
 * @desc This function starts advertising this device, with the given settings and data, so that scanning centrals can find it. It returns straight away, and the callback is called once when the advertisement has started or has failed. Only one advertisement runs at a time: stop it with ${function.bluetooth_le_advertise_stop} before you start another one. Stopping it before it has started makes the start fail with ${constant.BluetoothError}.OperationFailed.
 *
 * The function returns ${constant.BluetoothError}.Ok when the start has begun. Otherwise it returns the reason, which is also in ${function.bluetooth_last_error_code} and ${function.bluetooth_last_error_message}, and the callback is never called:
 *
 * * NotInitialized before ${function.bluetooth_initialize}.
 * * InvalidArgument for a `tx_power` that is not a ${constant.BluetoothLeAdvertiseTxPower} member, a UUID that is not valid, service data whose UUID is not also in `service_uuids`, or a `company_id` outside 0-65535.
 * * PermissionDenied when the Bluetooth permission is missing or was refused, or BluetoothDisabled when the radio is off.
 * * Busy while an advertisement is starting or running.
 * * NotSupported for a part of the settings or data that the platform cannot send. You can ask ${function.bluetooth_feature_is_supported} about each part beforehand: ${constant.BluetoothFeature}.LeAdvertiseName, LeAdvertiseServiceUuids, LeAdvertiseServiceData, LeAdvertiseManufacturerData, LeAdvertiseTxPower, LeAdvertiseIncludeTxPower and LeAdvertiseNonConnectable.
 *
 * Advertising differs a lot between platforms:
 *
 * [[Note: On macOS and iOS the advertisement carries only the local name and the service UUIDs, and is always connectable. `connectable` set to `false`, a `tx_power`, `include_tx_power` set to `true`, service data and manufacturer data each return NotSupported. The name is the computer's name on macOS and the device's name on iOS.]]
 *
 * [[Note: On Windows a service UUID, its service data and the name are advertised only through a GATT service that the server already holds, so every UUID in `service_uuids` must be a service added with ${function.bluetooth_le_server_add_service}, and `include_name` needs at least one such UUID; otherwise the call returns NotSupported. Manufacturer data, `include_tx_power` and `tx_power` go out in a separate advertisement. An advertisement with neither a service UUID nor one of these returns NotSupported.]]
 *
 * [[Note: On Android the advertisement is one legacy advertising packet, with no scan response, and must fit in its 31 bytes: data that does not fit fails in the callback with ${constant.BluetoothError}.InvalidArgument. Android 12 and later need the `BLUETOOTH_ADVERTISE` permission.]]
 * @param {Struct.BluetoothLeAdvertiseSettings} settings How the advertisement is sent.
 * @param {Struct.BluetoothLeAdvertiseData} data What the advertisement carries.
 * @param {Function} callback The function to call once the advertisement has started or has failed.
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the start has begun, otherwise the reason it did not.
 * @event callback
 * @desc Called once, when the advertisement has started, has failed or was stopped before it started.
 * @member {Enum.BluetoothError} error_code ${constant.BluetoothError}.Ok if the device is advertising, otherwise the reason it is not.
 * @member {String} message A description of the failure, or an empty string on success.
 * @event_end
 * @example
 * ```gml
 * var _settings = new BluetoothLeAdvertiseSettings();
 * _settings.connectable = true;
 *
 * var _data = new BluetoothLeAdvertiseData();
 * _data.include_name = false;
 * _data.include_tx_power = false;
 * _data.service_uuids = ["6e400001-b5a3-f393-e0a9-e50e24dcca9e"];
 * _data.service_data = [];
 * _data.manufacturer_data = [];
 *
 * var _result = bluetooth_le_advertise_start(_settings, _data, function(_error_code, _message)
 * {
 *     if (_error_code != BluetoothError.Ok)
 *     {
 *         show_debug_message($"Advertising failed: {_message}");
 *         exit;
 *     }
 *     show_debug_message("Advertising");
 * });
 *
 * if (_result != BluetoothError.Ok)
 * {
 *     show_debug_message($"Advertising did not start: {bluetooth_last_error_message()}");
 * }
 * ```
 * The above code advertises the UUID of the service that the server holds, as a connectable advertisement at the platform's default power, since `tx_power` is left undefined. The name is left out, which keeps the advertisement within the 31 bytes Android allows, and the service has been added first, as Windows requires, so this advertisement can be sent on every platform. The callback reports whether the advertisement started; if the call itself fails, the reason is read with ${function.bluetooth_last_error_message} and the callback is never called.
 * @function_end
 */

/**
 * @function bluetooth_le_advertise_stop
 * @desc This function stops advertising. A start that is still in progress fails, and its callback is called with ${constant.BluetoothError}.OperationFailed. Stopping when nothing is advertising returns ${constant.BluetoothError}.Ok and does nothing.
 *
 * The function returns NotInitialized before ${function.bluetooth_initialize}, and OperationFailed when the platform could not stop the advertisement, with the reason in ${function.bluetooth_last_error_message}.
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if advertising has stopped, otherwise the reason it has not.
 * @example
 * ```gml
 * if (bluetooth_le_advertise_is_running())
 * {
 *     bluetooth_le_advertise_stop();
 * }
 * ```
 * The above code stops advertising when an advertisement is starting or running.
 * @function_end
 */

/**
 * @function bluetooth_le_server_is_supported
 * @desc This function checks whether this device can run a GATT server. It returns `false` before ${function.bluetooth_initialize} and on a device with no Bluetooth LE.
 *
 * [[Note: On Windows the answer comes from the adapter, which must support the peripheral role.]]
 * @returns {Bool} `true` if the device can run a GATT server, otherwise `false`.
 * @example
 * ```gml
 * if (!bluetooth_le_server_is_supported())
 * {
 *     show_debug_message("This device cannot act as a peripheral");
 * }
 * ```
 * The above code reports when the device cannot run a GATT server.
 * @function_end
 */

/**
 * @function bluetooth_le_server_is_running
 * @desc This function checks whether the GATT server is running, from a successful ${function.bluetooth_le_server_start} until ${function.bluetooth_le_server_stop} or ${function.bluetooth_shutdown}.
 * @returns {Bool} `true` if the server is running, otherwise `false`.
 * @example
 * ```gml
 * if (!bluetooth_le_server_is_running())
 * {
 *     bluetooth_le_server_start();
 * }
 * ```
 * The above code starts the GATT server when it is not running yet.
 * @function_end
 */

/**
 * @function bluetooth_le_advertise_is_supported
 * @desc This function checks whether this device can advertise. It returns `false` before ${function.bluetooth_initialize} and on a device with no Bluetooth LE. Which parts of an advertisement the device can send is a separate question for ${function.bluetooth_feature_is_supported} (see ${function.bluetooth_le_advertise_start}).
 *
 * [[Note: On Windows the adapter must support the peripheral role, and on Android it must support multiple advertisement.]]
 * @returns {Bool} `true` if the device can advertise, otherwise `false`.
 * @example
 * ```gml
 * can_advertise = bluetooth_le_advertise_is_supported();
 * ```
 * The above code stores whether the device can advertise in the instance variable `can_advertise`.
 * @function_end
 */

/**
 * @function bluetooth_le_advertise_is_running
 * @desc This function checks whether an advertisement is starting or running. It returns `true` from a successful ${function.bluetooth_le_advertise_start}, while the start is still in progress, until the advertisement fails to start or is stopped.
 * @returns {Bool} `true` if an advertisement is starting or running, otherwise `false`.
 * @example
 * ```gml
 * draw_text(16, 16, bluetooth_le_advertise_is_running() ? "Advertising" : "Not advertising");
 * ```
 * The above code, in a Draw Event, shows whether the device is advertising.
 * @function_end
 */

/**
 * @function bluetooth_set_callback_le_server_connection_state_changed
 * @desc This function sets the function to call when a remote central connects to or disconnects from the GATT server. Setting a callback replaces the one set before, and it stays set across ${function.bluetooth_shutdown}.
 *
 * The connection handle it reports is the server connection that the read and write request callbacks carry and that ${function.bluetooth_le_server_notify_value} takes to send to that central only. It is retired on the disconnect, and a central that comes back gets a new one. A server connection is not a client connection: ${function.bluetooth_le_disconnect} given one returns InvalidHandle, since it ends when the central disconnects. ${function.bluetooth_le_server_stop} reports a disconnect for each central still connected.
 *
 * The device is the central as a device handle. It enters the device cache without the device found callback being called (see ${function.bluetooth_set_callback_device_found}).
 *
 * [[Note: Only Android reports a central's connect and disconnect as they happen (${constant.BluetoothFeature}.LeServerConnectionEvents). Windows reports a central on its first request or subscription, and when its session closes. macOS and iOS have no peripheral-side connection event: they report a central on its first request or subscription, and its disconnect when its last subscription ends, so a central that never subscribes is never reported disconnected.]]
 * @param {Function} callback The function to call when a central connects or disconnects.
 * @returns {Bool} `true` once the callback is set.
 * @event callback
 * @desc Called each time a central connects to or disconnects from the GATT server.
 * @member {Real} connection The server connection handle of the central.
 * @member {Bool} connected `true` if the central connected, `false` if it disconnected.
 * @member {Real} device The device handle of the central.
 * @event_end
 * @example
 * ```gml
 * bluetooth_set_callback_le_server_connection_state_changed(function(_connection, _connected, _device)
 * {
 *     if (_connected)
 *     {
 *         show_debug_message($"Central {bluetooth_device_get_id(_device)} connected");
 *         array_push(centrals, _connection);
 *     }
 *     else
 *     {
 *         var _index = array_get_index(centrals, _connection);
 *         if (_index >= 0)
 *         {
 *             array_delete(centrals, _index, 1);
 *         }
 *     }
 * });
 * ```
 * The above code keeps the server connections of the connected centrals in the instance's `centrals` array. A central that connects is logged with its device id, read with ${function.bluetooth_device_get_id}, and its connection is added to the array; one that disconnects is removed from it.
 * @function_end
 */

/**
 * @function bluetooth_remove_callback_le_server_connection_state_changed
 * @desc This function removes the function set with ${function.bluetooth_set_callback_le_server_connection_state_changed}. Centrals keep connecting and disconnecting, but the game is no longer told.
 * @returns {Bool} `true` once the callback is removed.
 * @example
 * ```gml
 * bluetooth_remove_callback_le_server_connection_state_changed();
 * ```
 * The above code stops the server connection callback from being called.
 * @function_end
 */

/**
 * @function bluetooth_set_callback_le_server_read_request
 * @desc This function sets the function to call when a remote central reads an attribute of the GATT server. Setting a callback replaces the one set before, and it stays set across ${function.bluetooth_shutdown}.
 *
 * The callback reports the server characteristic read, or the one the descriptor belongs to; `descriptor` is 0 for a read of the characteristic's value. `offset` is where in the value the central reads from: a central reading a long value asks again at later offsets, and you answer each request with the bytes from its offset onwards. You must answer every request with ${function.bluetooth_le_server_respond_read}; a request that is not answered expires after 30 seconds, as described there.
 *
 * Some reads never reach the callback. A characteristic defined with an initial `value` is answered by the platform, on every platform. The Client Characteristic Configuration descriptor (0x2902) is answered by the platform too. While no callback is set, every read request is refused at once with ${constant.BluetoothAttError}.RequestNotSupported.
 *
 * [[Note: macOS and iOS never report a descriptor read (${constant.BluetoothFeature}.LeServerDescriptorRequests), and serve the User Description descriptor they can host with an empty value.]]
 * @param {Function} callback The function to call for each read request.
 * @returns {Bool} `true` once the callback is set.
 * @event callback
 * @desc Called once for each read request a central makes.
 * @member {Real} request_id The id to answer the request with, using ${function.bluetooth_le_server_respond_read}.
 * @member {Real} connection The server connection handle of the central.
 * @member {Real} characteristic The handle of the server characteristic read, or of the one the descriptor belongs to.
 * @member {Real} descriptor The handle of the server descriptor read, or 0 for a read of the characteristic.
 * @member {Real} offset The position in the value that the central reads from (in bytes).
 * @event_end
 * @example
 * ```gml
 * bluetooth_set_callback_le_server_read_request(function(_request_id, _connection, _characteristic, _descriptor, _offset)
 * {
 *     buffer_seek(reply_buffer, buffer_seek_start, 0);
 *     buffer_write(reply_buffer, buffer_u32, score);
 *     if (_offset > 4)
 *     {
 *         bluetooth_le_server_respond_read(_request_id, BluetoothAttError.InvalidOffset, reply_buffer, 0, 0);
 *         exit;
 *     }
 *     bluetooth_le_server_respond_read(_request_id, BluetoothAttError.Success, reply_buffer, _offset, 4 - _offset);
 * });
 * ```
 * The above code answers every read request with the instance's `score` as a 32-bit value. The value is written to `reply_buffer`, a fixed buffer the instance created earlier, and the bytes from the requested offset onwards are sent with ${function.bluetooth_le_server_respond_read}; an offset past the end of the value is refused with ${constant.BluetoothAttError}.InvalidOffset.
 * @function_end
 */

/**
 * @function bluetooth_remove_callback_le_server_read_request
 * @desc This function removes the function set with ${function.bluetooth_set_callback_le_server_read_request}. From then on every read request that reaches the game is refused at once with ${constant.BluetoothAttError}.RequestNotSupported.
 * @returns {Bool} `true` once the callback is removed.
 * @example
 * ```gml
 * bluetooth_remove_callback_le_server_read_request();
 * ```
 * The above code stops the read request callback from being called.
 * @function_end
 */

/**
 * @function bluetooth_set_callback_le_server_write_request
 * @desc This function sets the function to call when a remote central writes an attribute of the GATT server. Setting a callback replaces the one set before, and it stays set across ${function.bluetooth_shutdown}.
 *
 * The callback reports the attribute written as the read request callback does (see ${function.bluetooth_set_callback_le_server_read_request}): `descriptor` is 0 for a write of the characteristic's value. `offset` is where the bytes go in the attribute's value; a long write that the central sends in parts arrives assembled, at the offset of its first part. `response_needed` is `false` for a write without response.
 *
 * `value` is a value handle holding the `size` bytes written. You copy them into a buffer with ${function.bluetooth_value_fetch}, then answer the request with ${function.bluetooth_le_server_respond_write}, which frees the value. You must answer every request, a write without response included; one that is not answered is dropped after 5 seconds for a write without response and expires after 30 seconds otherwise, and its value is freed with it.
 *
 * A write of the Client Characteristic Configuration descriptor (0x2902) is a subscription, which the platform handles, and never reaches the callback. While no callback is set, every write request that needs a response is refused at once with ${constant.BluetoothAttError}.RequestNotSupported.
 *
 * [[Note: macOS and iOS never report a descriptor write, and do not say whether the central asked for a response, so `response_needed` is always `true` there.]]
 * @param {Function} callback The function to call for each write request.
 * @returns {Bool} `true` once the callback is set.
 * @event callback
 * @desc Called once for each write request a central makes.
 * @member {Real} request_id The id to answer the request with, using ${function.bluetooth_le_server_respond_write}.
 * @member {Real} connection The server connection handle of the central.
 * @member {Real} characteristic The handle of the server characteristic written, or of the one the descriptor belongs to.
 * @member {Real} descriptor The handle of the server descriptor written, or 0 for a write of the characteristic.
 * @member {Real} offset The position in the attribute's value where the bytes go (in bytes).
 * @member {Bool} response_needed `true` if the central waits for an answer, `false` for a write without response.
 * @member {Real} value The value handle holding the bytes written, to copy with ${function.bluetooth_value_fetch}.
 * @member {Real} size The number of bytes written.
 * @event_end
 * @example
 * ```gml
 * bluetooth_set_callback_le_server_write_request(function(_request_id, _connection, _characteristic, _descriptor, _offset, _response_needed, _value, _size)
 * {
 *     var _buffer = buffer_create(max(_size, 1), buffer_fixed, 1);
 *     var _copied = bluetooth_value_fetch(_value, _buffer, 0);
 *
 *     if (_copied < 1 || _characteristic != characteristic_command)
 *     {
 *         bluetooth_le_server_respond_write(_request_id, BluetoothAttError.WriteNotPermitted);
 *     }
 *     else
 *     {
 *         var _command = buffer_peek(_buffer, 0, buffer_u8);
 *         show_debug_message($"Command {_command} from connection {_connection}");
 *         bluetooth_le_server_respond_write(_request_id, BluetoothAttError.Success);
 *     }
 *
 *     buffer_delete(_buffer);
 * });
 * ```
 * The above code handles write requests. The bytes written are copied with ${function.bluetooth_value_fetch} into a new buffer of the reported size. A write that holds no bytes, or that is not for the characteristic stored in `characteristic_command`, is refused with ${constant.BluetoothAttError}.WriteNotPermitted; otherwise its first byte is read as a command and the write is accepted with ${function.bluetooth_le_server_respond_write}. Every request is answered, and the buffer is deleted at the end.
 * @function_end
 */

/**
 * @function bluetooth_remove_callback_le_server_write_request
 * @desc This function removes the function set with ${function.bluetooth_set_callback_le_server_write_request}. From then on every write request that needs a response is refused at once with ${constant.BluetoothAttError}.RequestNotSupported, and a write without response is dropped.
 * @returns {Bool} `true` once the callback is removed.
 * @example
 * ```gml
 * bluetooth_remove_callback_le_server_write_request();
 * ```
 * The above code stops the write request callback from being called.
 * @function_end
 */

// Structs

/**
 * @struct BluetoothLeServiceDefinition
 * @desc This struct describes a primary GATT service for ${function.bluetooth_le_server_add_service}. You create it with `new BluetoothLeServiceDefinition()` and must set every variable: one left undefined is a runtime error when the struct is passed.
 * @member {String} uuid The UUID of the service, 16-bit or 128-bit.
 * @member {Array[Struct.BluetoothLeCharacteristicDefinition]} characteristics The characteristics of the service, in the order their handles are listed. An empty array is a service with none.
 * @struct_end
 */

/**
 * @struct BluetoothLeCharacteristicDefinition
 * @desc This struct describes one characteristic of a ${struct.BluetoothLeServiceDefinition}. You create it with `new BluetoothLeCharacteristicDefinition()` and must set every variable: one left undefined is a runtime error when the struct is passed. A characteristic with the Notify or Indicate property gets its Client Characteristic Configuration descriptor (0x2902) from the platform, so you do not list it.
 *
 * [[Note: Windows ignores the Broadcast property of a server characteristic, and macOS and iOS ignore both Broadcast and ExtendedProperties.]]
 * @member {String} uuid The UUID of the characteristic, 16-bit or 128-bit.
 * @member {Real} properties The ${constant.BluetoothLeCharacteristicProperty} flags of the characteristic, combined with `|`.
 * @member {Real} permissions The ${constant.BluetoothLeAttributePermission} flags of the characteristic, combined with `|`.
 * @member {Array[Real]} value The initial value, each entry a byte (0-255), at most 512 bytes, or an empty array for none. A characteristic with an initial value is read from it by the platform, and its reads never reach the read request callback.
 * @member {Array[Struct.BluetoothLeDescriptorDefinition]} descriptors The descriptors of the characteristic, or an empty array for none.
 * @struct_end
 */

/**
 * @struct BluetoothLeDescriptorDefinition
 * @desc This struct describes one descriptor of a ${struct.BluetoothLeCharacteristicDefinition}. You create it with `new BluetoothLeDescriptorDefinition()`. The descriptor has no initial value: its reads and writes reach the request callbacks, on the platforms that report them (${constant.BluetoothFeature}.LeServerDescriptorRequests). A Client Characteristic Configuration descriptor (0x2902) in a definition is left out, since the platform manages it.
 *
 * [[Note: Windows refuses the User Description descriptor (0x2901), while macOS and iOS can host only that one (see ${function.bluetooth_le_server_add_service}).]]
 * @member {String} uuid The UUID of the descriptor, 16-bit or 128-bit.
 * @struct_end
 */

/**
 * @struct BluetoothLeAdvertiseSettings
 * @desc This struct says how ${function.bluetooth_le_advertise_start} sends the advertisement. You create it with `new BluetoothLeAdvertiseSettings()`.
 * @member {Bool} connectable Whether centrals can connect to the device from the advertisement. macOS and iOS always advertise as connectable.
 * @member {Enum.BluetoothLeAdvertiseTxPower} [tx_power] The transmit power to advertise at. Leave it undefined for the platform's default.
 * @struct_end
 */

/**
 * @struct BluetoothLeAdvertiseData
 * @desc This struct says what the advertisement of ${function.bluetooth_le_advertise_start} carries. You create it with `new BluetoothLeAdvertiseData()` and must set every variable: one left undefined is a runtime error when the struct is passed. A platform that cannot send a part you ask for makes the call return NotSupported (see ${function.bluetooth_le_advertise_start}).
 * @member {Bool} include_name Whether the advertisement carries the device's name.
 * @member {Bool} include_tx_power Whether the advertisement carries the transmit power level.
 * @member {Array[String]} service_uuids The UUIDs of the services to advertise, or an empty array for none.
 * @member {Array[Struct.BluetoothLeAdvertiseServiceData]} service_data The service data to advertise, or an empty array for none. Each entry's UUID must also be in `service_uuids`.
 * @member {Array[Struct.BluetoothLeAdvertiseManufacturerData]} manufacturer_data The manufacturer-specific data to advertise, or an empty array for none.
 * @struct_end
 */

/**
 * @struct BluetoothLeAdvertiseServiceData
 * @desc This struct holds the data advertised for one service. You create it with `new BluetoothLeAdvertiseServiceData()` to advertise it in a ${struct.BluetoothLeAdvertiseData}, and ${function.bluetooth_device_get_advertisement} returns it as part of what a scanned device advertised.
 * @member {String} uuid The UUID of the service the data belongs to. When advertising, it must also be in the data's `service_uuids`.
 * @member {Array[Real]} data The service data, each entry a byte (0-255).
 * @struct_end
 */

/**
 * @struct BluetoothLeAdvertiseManufacturerData
 * @desc This struct holds manufacturer-specific data, under a company identifier assigned by the Bluetooth SIG. You create it with `new BluetoothLeAdvertiseManufacturerData()` to advertise it in a ${struct.BluetoothLeAdvertiseData}, and ${function.bluetooth_device_get_advertisement} returns it as part of what a scanned device advertised.
 * @member {Real} company_id The Bluetooth SIG company identifier (0-65535).
 * @member {Array[Real]} data The manufacturer-specific data, each entry a byte (0-255).
 * @struct_end
 */

// Constants

/**
 * @const BluetoothAttError
 * @desc This enum holds the ATT error codes (Bluetooth Core Specification, Vol 3, Part F, 3.4.1.1) that answer a read request with ${function.bluetooth_le_server_respond_read} or a write request with ${function.bluetooth_le_server_respond_write}. Each code is sent to the central as it is.
 * @member Success The request succeeded (0x00).
 * @member InvalidHandle The attribute handle is not valid (0x01).
 * @member ReadNotPermitted The attribute cannot be read (0x02).
 * @member WriteNotPermitted The attribute cannot be written (0x03).
 * @member InvalidPdu The request is malformed (0x04).
 * @member InsufficientAuthentication The link must be authenticated (paired) first (0x05).
 * @member RequestNotSupported The server does not support the request (0x06).
 * @member InvalidOffset The offset is past the end of the value (0x07).
 * @member InsufficientAuthorization The central is not authorised to access the attribute (0x08).
 * @member PrepareQueueFull Too many writes are prepared (0x09).
 * @member AttributeNotFound No attribute was found (0x0A).
 * @member AttributeNotLong The attribute cannot be read or written in parts (0x0B).
 * @member InsufficientEncryptionKeySize The encryption key of the link is too short (0x0C).
 * @member InvalidAttributeValueLength The value has a length the attribute does not accept (0x0D).
 * @member UnlikelyError The request failed for a reason no other code covers (0x0E).
 * @member InsufficientEncryption The link must be encrypted first (0x0F).
 * @member UnsupportedGroupType The attribute type is not a supported grouping type (0x10).
 * @member InsufficientResources The server does not have the resources to complete the request (0x11).
 * @const_end
 */

/**
 * @const BluetoothLeAttributePermission
 * @desc This enum holds the permission flags of a server characteristic, set in the `permissions` of a ${struct.BluetoothLeCharacteristicDefinition} and combined with `|`. The values are those of Android's attribute permissions.
 *
 * [[Note: Windows, macOS and iOS have no signed write permission: WriteSigned and WriteSignedMitm make ${function.bluetooth_le_server_add_service} return NotSupported there. macOS and iOS also have no separate MITM protection, so ReadEncryptedMitm and WriteEncryptedMitm only require encryption there.]]
 * @member None No access.
 * @member Read The attribute can be read.
 * @member ReadEncrypted The attribute can be read over an encrypted link.
 * @member ReadEncryptedMitm The attribute can be read over an encrypted link with MITM protection (an authenticated pairing).
 * @member Write The attribute can be written.
 * @member WriteEncrypted The attribute can be written over an encrypted link.
 * @member WriteEncryptedMitm The attribute can be written over an encrypted link with MITM protection (an authenticated pairing).
 * @member WriteSigned The attribute can be written with a signed write.
 * @member WriteSignedMitm The attribute can be written with a signed write with MITM protection.
 * @const_end
 */

/**
 * @const BluetoothLeAdvertiseTxPower
 * @desc This enum holds the transmit power levels that the `tx_power` of a ${struct.BluetoothLeAdvertiseSettings} can ask for. They are Android's advertising power levels; Windows sends them as -21, -15, -7 and 1 dBm, and macOS and iOS cannot set the power at all.
 * @member UltraLow The lowest power and the shortest range.
 * @member Low A low power.
 * @member Medium A medium power, the default on Android.
 * @member High The highest power and the longest range.
 * @const_end
 */

/**
 * @module le_server
 * @title LE Server
 * @desc The functions to run a GATT server, so that this device acts as an LE peripheral that centrals connect to, read and write, and to advertise it.
 *
 * @section_func
 * @desc These are the functions of the GATT server and of advertising.
 * @ref bluetooth_le_server_start
 * @ref bluetooth_le_server_stop
 * @ref bluetooth_le_server_add_service
 * @ref bluetooth_le_server_clear_services
 * @ref bluetooth_le_server_respond_read
 * @ref bluetooth_le_server_respond_write
 * @ref bluetooth_le_server_notify_value
 * @ref bluetooth_le_advertise_start
 * @ref bluetooth_le_advertise_stop
 * @ref bluetooth_le_server_is_supported
 * @ref bluetooth_le_server_is_running
 * @ref bluetooth_le_advertise_is_supported
 * @ref bluetooth_le_advertise_is_running
 * @ref bluetooth_set_callback_le_server_connection_state_changed
 * @ref bluetooth_remove_callback_le_server_connection_state_changed
 * @ref bluetooth_set_callback_le_server_read_request
 * @ref bluetooth_remove_callback_le_server_read_request
 * @ref bluetooth_set_callback_le_server_write_request
 * @ref bluetooth_remove_callback_le_server_write_request
 * @section_end
 *
 * @section_struct
 * @desc These are the structs that define the services of the GATT server and the advertisement.
 * @ref BluetoothLeServiceDefinition
 * @ref BluetoothLeCharacteristicDefinition
 * @ref BluetoothLeDescriptorDefinition
 * @ref BluetoothLeAdvertiseSettings
 * @ref BluetoothLeAdvertiseData
 * @ref BluetoothLeAdvertiseServiceData
 * @ref BluetoothLeAdvertiseManufacturerData
 * @section_end
 *
 * @section_const
 * @desc These are the constants used by the GATT server and by advertising.
 * @ref BluetoothAttError
 * @ref BluetoothLeAttributePermission
 * @ref BluetoothLeAdvertiseTxPower
 * @section_end
 *
 * @module_end
 */
