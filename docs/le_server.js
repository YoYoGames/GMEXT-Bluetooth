// Functions

/**
 * @function bluetooth_le_server_start
 * @desc This function starts the local GATT server, so that this device can act as an LE peripheral and remote centrals can connect to it. There is one server, and it keeps the given callbacks until it stops. Once the server runs, you add its services with ${function.bluetooth_le_server_add_service}.
 *
 * The function returns ${constant.BluetoothError}.Ok when the server is running. Otherwise it returns the reason, which is also in ${function.bluetooth_last_error_code} and ${function.bluetooth_last_error_message}, and no callback is called: NotInitialized before ${function.bluetooth_initialize}, Busy while the server is already running, NotSupported when the device cannot act as a peripheral (see ${constant.BluetoothFeature}.LeServer), PermissionDenied when the Bluetooth permission is missing or was refused, or BluetoothDisabled when the radio is off.
 *
 * ### Centrals
 *
 * `on_connected` is called when a central connects, with a new server connection handle and the central's device handle. The device enters the device cache without calling any scan callback. The connection handle is what the central's read and write requests carry, what ${function.bluetooth_le_server_notify_value} takes to send to that central only, and what ${function.bluetooth_le_connection_is_valid}, ${function.bluetooth_le_connection_get_device} and ${function.bluetooth_le_connection_get_mtu} accept. A server connection is not a client connection: ${function.bluetooth_le_disconnect} given one returns InvalidHandle, since it ends when the central disconnects.
 *
 * `on_disconnected` is called when the central leaves, and retires its connection handle; a central that comes back gets a new one.
 *
 * [[Note: Only Android reports a central's connect and disconnect as they happen (${constant.BluetoothFeature}.LeServerConnectionEvents). Windows reports a central on its first request or subscription, and its disconnect when its session closes. macOS and iOS have no peripheral-side connection event: they report a central on its first request or subscription, and its disconnect when its last subscription ends, so a central that never subscribes is not reported gone while the server runs: only the radio turning off reports it, with ${constant.BluetoothError}.BluetoothDisabled.]]
 *
 * ### Requests
 *
 * `on_read` is called for each read of an attribute that has no value of its own. It reports the server characteristic read, or the one the descriptor belongs to; `descriptor` is 0 for a read of the characteristic's value. `offset` is where in the value the central reads from: a central reading a long value asks again at later offsets. Answer each request once, with ${function.bluetooth_le_server_respond_read} and the bytes from its offset onwards, or with ${function.bluetooth_le_server_respond_read_error}, for example ${constant.BluetoothAttError}.InvalidOffset for an offset past the end of the value. An attribute defined with a static or initial `value` is read without reaching `on_read`, on every platform, and so is the Client Characteristic Configuration descriptor (0x2902).
 *
 * `on_write` is called for each write. It reports the attribute written in the same way; `offset` is where the bytes go in the attribute's value, and a long write that the central sends in parts arrives assembled, at the offset of its first part. `response_needed` is `false` for a write without response. `value` is a value handle holding the `size` bytes written: copy them with ${function.bluetooth_value_fetch}, then answer the request with ${function.bluetooth_le_server_respond_write}, which frees the value. You must answer every write request, a write without response included. A write of the Client Characteristic Configuration descriptor is a subscription, which the platform handles, and never reaches `on_write`.
 *
 * A request you do not answer expires: see ${function.bluetooth_le_server_respond_read} and ${function.bluetooth_le_server_respond_write}.
 *
 * [[Note: macOS and iOS never report a descriptor read or write (${constant.BluetoothFeature}.LeServerDescriptorRequests), and do not say whether the central asked for a response, so `response_needed` is always `true` there.]]
 *
 * ### Stopping
 *
 * `on_stopped` is called when the server ends without ${function.bluetooth_le_server_stop}, after `on_disconnected` for each central. When the radio turns off, for example, each central reports ${constant.BluetoothError}.BluetoothDisabled, then the server does; its services and attribute handles are gone, and nothing restarts when the radio comes back, so start the server and add its services again once the state is ${constant.BluetoothState}.PoweredOn. A server you stop yourself calls neither.
 *
 * [[Note: On Android 12 and later the server needs the `BLUETOOTH_CONNECT` permission, and the call returns PermissionDenied without it.]]
 * @param {Function} on_connected The function to call when a central connects.
 * @param {Function} on_disconnected The function to call when a central disconnects.
 * @param {Function} on_read The function to call for each read request.
 * @param {Function} on_write The function to call for each write request.
 * @param {Function} [on_stopped] The function to call when the server ends without ${function.bluetooth_le_server_stop}, or `undefined`.
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the server is running, otherwise the reason it could not start.
 * @event callback:on_connected
 * @desc Called each time a central connects to the server.
 * @member {Real} connection The new server connection handle of the central.
 * @member {Real} device The device handle of the central.
 * @event_end
 * @event callback:on_disconnected
 * @desc Called each time a central disconnects from the server, other than through ${function.bluetooth_le_server_stop}. It is the last callback for that central's connection.
 * @member {Enum.BluetoothError} error_code The reason the central left, such as ${constant.BluetoothError}.BluetoothDisabled when the radio was turned off.
 * @member {String} message A description of why the central left.
 * @member {Real} connection The server connection handle of the central, which is no longer valid.
 * @event_end
 * @event callback:on_read
 * @desc Called once for each read request a central makes for an attribute with no value of its own.
 * @member {Real} request_id The id to answer the request with, using ${function.bluetooth_le_server_respond_read} or ${function.bluetooth_le_server_respond_read_error}.
 * @member {Real} connection The server connection handle of the central.
 * @member {Real} characteristic The handle of the server characteristic read, or of the one the descriptor belongs to.
 * @member {Real} descriptor The handle of the server descriptor read, or 0 for a read of the characteristic.
 * @member {Real} offset The position in the value that the central reads from (in bytes).
 * @event_end
 * @event callback:on_write
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
 * @event callback:on_stopped
 * @desc Called once, when the server ends without ${function.bluetooth_le_server_stop}, after `on_disconnected` for each central. It is the last callback of the server.
 * @member {Enum.BluetoothError} error_code The reason the server ended, such as ${constant.BluetoothError}.BluetoothDisabled when the radio was turned off.
 * @member {String} message A description of why the server ended.
 * @event_end
 * @example
 * ```gml
 * var _error = bluetooth_le_server_start(
 *     function(_connection, _device)
 *     {
 *         show_debug_message($"Central {bluetooth_device_get_id(_device)} connected");
 *         array_push(centrals, _connection);
 *     },
 *     function(_error_code, _message, _connection)
 *     {
 *         var _index = array_get_index(centrals, _connection);
 *         if (_index >= 0)
 *         {
 *             array_delete(centrals, _index, 1);
 *         }
 *     },
 *     on_read_request,
 *     on_write_request,
 *     function(_error_code, _message)
 *     {
 *         show_debug_message($"Server ended: {_message}");
 *         centrals = [];
 *         service = 0;
 *     });
 *
 * if (_error != BluetoothError.Ok)
 * {
 *     show_debug_message($"Server did not start: {bluetooth_last_error_message()}");
 * }
 * else
 * {
 *     add_services();
 * }
 * ```
 * The above code starts the GATT server. The `on_connected` callback logs each central with its device id, read with ${function.bluetooth_device_get_id}, and adds its connection to the instance's `centrals` array; `on_disconnected` removes it again. Read and write requests go to the instance's own `on_read_request` and `on_write_request` methods, shown in the examples of ${function.bluetooth_le_server_respond_read} and ${function.bluetooth_le_server_respond_write}. When the server ends on its own, `on_stopped` clears the list of centrals and the stored service handle, which are no longer valid. If the call fails, the reason is read with ${function.bluetooth_last_error_message}; otherwise the instance's `add_services` method adds the services with ${function.bluetooth_le_server_add_service}.
 * @function_end
 */

/**
 * @function bluetooth_le_server_stop
 * @desc This function stops the GATT server, removes every service it holds, retires every server connection and ends advertising. A server you stop yourself calls no callback: no `on_disconnected` for its centrals, no `on_stopped` for the server, and no `on_stopped` for the advertisement it ends.
 *
 * Every read and write request still waiting for an answer is answered with ${constant.BluetoothAttError}.UnlikelyError, every ${function.bluetooth_le_server_add_service} still in progress calls its callback with ${constant.BluetoothError}.OperationFailed, and every service, characteristic and descriptor handle stops being valid.
 *
 * Stopping a server that is not running returns ${constant.BluetoothError}.Ok and does nothing. The function returns NotInitialized before ${function.bluetooth_initialize}, with the reason in ${function.bluetooth_last_error_message}.
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the server is stopped or was not running, otherwise the reason it could not stop.
 * @example
 * ```gml
 * bluetooth_le_server_stop();
 * centrals = [];
 * service = 0;
 * characteristic_status = 0;
 * ```
 * The above code stops the GATT server and clears the instance variables that held its connections and its service and characteristic handles, since they are no longer valid and no callback reports them gone.
 * @function_end
 */

/**
 * @function bluetooth_le_server_is_running
 * @desc This function checks whether the GATT server is running, from a successful ${function.bluetooth_le_server_start} until ${function.bluetooth_le_server_stop}, ${function.bluetooth_shutdown}, or the server ending on its own.
 * @returns {Bool} `true` if the server is running, otherwise `false`.
 * @example
 * ```gml
 * server_label = bluetooth_le_server_is_running() ? "Server running" : "Server stopped";
 * ```
 * The above code sets the instance variable `server_label` to a text that says whether the GATT server is running.
 * @function_end
 */

/**
 * @function bluetooth_le_server_add_service
 * @desc This function adds a primary GATT service, described by the given ${struct.BluetoothLeServiceDefinition}, to the running GATT server. The service is added in the background, and the callback is called once when it has been added or has failed.
 *
 * The function returns ${constant.BluetoothError}.Ok when the add has started. Otherwise it returns the reason, which is also in ${function.bluetooth_last_error_code} and ${function.bluetooth_last_error_message}, and the callback is never called:
 *
 * * NotInitialized before ${function.bluetooth_initialize}.
 * * InvalidArgument for a UUID that is not valid, characteristic `properties` with bits that are not ${constant.BluetoothLeCharacteristicProperty} flags, `permissions` with bits that are not ${constant.BluetoothLeAttributePermission} flags, or a characteristic or descriptor `value` longer than 512 bytes.
 * * OperationFailed when the server is not running: call ${function.bluetooth_le_server_start} first.
 * * Busy when a service with the same UUID has already been added.
 * * NotSupported for a permission or a descriptor the platform cannot host (see the notes below).
 *
 * On success the callback receives the handle of the new server service. You walk it with the LE client getters: ${function.bluetooth_le_characteristic_get_count}, ${function.bluetooth_le_characteristic_get_at} and ${function.bluetooth_le_characteristic_find} list and find its characteristics, and ${function.bluetooth_le_descriptor_get_count}, ${function.bluetooth_le_descriptor_get_at} and ${function.bluetooth_le_descriptor_find} the descriptors of each characteristic, while ${function.bluetooth_le_characteristic_get_uuid}, ${function.bluetooth_le_characteristic_get_properties} and ${function.bluetooth_le_descriptor_get_uuid} read them. They are listed in definition order, so the characteristic at index 0 is the first entry of the definition's `characteristics` array, and two attributes with the same UUID each get their own handle. A Client Characteristic Configuration descriptor (0x2902) is managed by the platform: it gets no handle and is left out of the list, even when the definition names it, so the descriptor indices count only the other descriptors.
 *
 * These are the handles that ${function.bluetooth_le_server_notify_value} takes and that the server's `on_read` and `on_write` callbacks report. They stay valid until ${function.bluetooth_le_server_clear_services}, ${function.bluetooth_le_server_stop}, ${function.bluetooth_shutdown} or the server ending on its own, and a client call (read, write, subscribe or discover) given one returns InvalidHandle.
 *
 * [[Note: On macOS and iOS a server can host only the Characteristic User Description (0x2901) and Characteristic Presentation Format (0x2904) descriptors, and only with a static `value`; any other descriptor returns NotSupported. Windows, macOS and iOS have no signed write permission, so ${constant.BluetoothLeAttributePermission}.WriteSigned and WriteSignedMitm return NotSupported there.]]
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
 * var _description = new BluetoothLeDescriptorDefinition();
 * _description.uuid = "2901";
 * _description.value = [83, 99, 111, 114, 101];
 *
 * var _characteristic = new BluetoothLeCharacteristicDefinition();
 * _characteristic.uuid = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";
 * _characteristic.properties = BluetoothLeCharacteristicProperty.Read | BluetoothLeCharacteristicProperty.Notify;
 * _characteristic.permissions = BluetoothLeAttributePermission.Read;
 * _characteristic.descriptors = [_description];
 *
 * var _definition = new BluetoothLeServiceDefinition();
 * _definition.uuid = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
 * _definition.characteristics = [_characteristic];
 *
 * var _error = bluetooth_le_server_add_service(_definition, function(_error_code, _message, _service)
 * {
 *     if (_error_code != BluetoothError.Ok)
 *     {
 *         show_debug_message($"Service not added: {_message}");
 *         exit;
 *     }
 *     service = _service;
 *     characteristic_status = bluetooth_le_characteristic_find(_service, "6e400003-b5a3-f393-e0a9-e50e24dcca9e");
 * });
 *
 * if (_error != BluetoothError.Ok)
 * {
 *     show_debug_message($"Add did not start: {bluetooth_last_error_message()}");
 * }
 * ```
 * The above code defines a service with one characteristic that can be read and notified. The characteristic's `value` is left `undefined`, so it has no initial value and its reads reach the server's `on_read` callback. It has one descriptor, a Characteristic User Description (0x2901) whose static value is the UTF-8 text "Score", which the platform serves without reaching `on_read`. The definition is passed to ${function.bluetooth_le_server_add_service}. When the callback reports success, the service handle is kept in `service` and the handle of its characteristic, found with ${function.bluetooth_le_characteristic_find}, in `characteristic_status`. If the add could not start, the reason is read with ${function.bluetooth_last_error_message}.
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
 * @desc This function answers a read request that the server's `on_read` callback (see ${function.bluetooth_le_server_start}) reported, with ${constant.BluetoothAttError}.Success and `size` bytes of the given buffer, starting at `offset`, as the value read. Send the bytes of the value from the request's own offset onwards. To refuse a read, use ${function.bluetooth_le_server_respond_read_error} instead.
 *
 * You must answer every read request, once, with one of the two functions. The request is checked before anything is sent, so a call that fails can be retried. The function returns ${constant.BluetoothError}.Ok when the answer was sent, otherwise the reason, which is also in ${function.bluetooth_last_error_code} and ${function.bluetooth_last_error_message}: NotInitialized before ${function.bluetooth_initialize}, InvalidHandle for a request id that is unknown, already answered or expired, InvalidArgument for the id of a write request (answer it with ${function.bluetooth_le_server_respond_write}) or for an offset and size outside the buffer, and OperationFailed when the platform could not send the answer.
 *
 * A request you do not answer expires 30 seconds after it arrived, which is the ATT transaction timeout: by then the central has usually given up on the link, and a call with its id returns InvalidHandle. Expired requests are swept when the next request arrives, and one the central still waits on is answered with ${constant.BluetoothAttError}.UnlikelyError. Stopping the server or clearing its services answers every waiting request with ${constant.BluetoothAttError}.UnlikelyError.
 *
 * [[Note: On Android the requests of a central that disconnects are dropped at once, since nothing can reach it, and a call with their ids returns InvalidHandle. On Windows, macOS and iOS they wait until they expire.]]
 * @param {Real} request_id The id of the request, as the server's `on_read` callback reported it.
 * @param {Buffer} data The buffer holding the value to send.
 * @param {Real} offset The position in the buffer of the first byte to send (in bytes).
 * @param {Real} size The number of bytes to send.
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the answer was sent, otherwise the reason it was not.
 * @example
 * ```gml
 * reply_buffer = buffer_create(64, buffer_grow, 1);
 *
 * on_read_request = function(_request_id, _connection, _characteristic, _descriptor, _offset)
 * {
 *     if (_characteristic != characteristic_status || _descriptor != 0)
 *     {
 *         bluetooth_le_server_respond_read_error(_request_id, BluetoothAttError.ReadNotPermitted);
 *         return;
 *     }
 *
 *     buffer_seek(reply_buffer, buffer_seek_start, 0);
 *     buffer_write(reply_buffer, buffer_text, status_text);
 *     var _length = buffer_tell(reply_buffer);
 *     if (_offset > _length)
 *     {
 *         bluetooth_le_server_respond_read_error(_request_id, BluetoothAttError.InvalidOffset);
 *         return;
 *     }
 *
 *     bluetooth_le_server_respond_read(_request_id, reply_buffer, _offset, _length - _offset);
 * };
 * ```
 * The above code creates a buffer for the answers and the method `on_read_request`, which is passed as the `on_read` callback of ${function.bluetooth_le_server_start}. A request for any attribute other than the characteristic stored in `characteristic_status` is refused with ${constant.BluetoothAttError}.ReadNotPermitted, using ${function.bluetooth_le_server_respond_read_error}. Otherwise the text in `status_text` is written to `reply_buffer` and the bytes from the requested offset onwards are sent with ${function.bluetooth_le_server_respond_read}; an offset past the end of the text is refused with ${constant.BluetoothAttError}.InvalidOffset. Every request is answered once.
 * @function_end
 */

/**
 * @function bluetooth_le_server_respond_read_error
 * @desc This function refuses a read request that the server's `on_read` callback (see ${function.bluetooth_le_server_start}) reported, with the given ATT error. Use ${function.bluetooth_le_server_respond_read} to answer with a value instead.
 *
 * You must answer every read request, once, with one of the two functions; a request you do not answer expires, as described for ${function.bluetooth_le_server_respond_read}. The function returns ${constant.BluetoothError}.Ok when the answer was sent, otherwise the reason, which is also in ${function.bluetooth_last_error_code} and ${function.bluetooth_last_error_message}: NotInitialized before ${function.bluetooth_initialize}, InvalidHandle for a request id that is unknown, already answered or expired, InvalidArgument for ${constant.BluetoothAttError}.Success, for a code that is not a ${constant.BluetoothAttError} member or for the id of a write request, and OperationFailed when the platform could not send the answer. A call that fails a check can be retried.
 * @param {Real} request_id The id of the request, as the server's `on_read` callback reported it.
 * @param {Enum.BluetoothAttError} error_code The ATT error to refuse the read with; any member except Success.
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the answer was sent, otherwise the reason it was not.
 * @example
 * ```gml
 * var _error = bluetooth_le_server_respond_read_error(_request_id, BluetoothAttError.InsufficientAuthorization);
 * if (_error != BluetoothError.Ok)
 * {
 *     show_debug_message($"Read not answered: {bluetooth_last_error_message()}");
 * }
 * ```
 * The above code, run inside the server's `on_read` callback, refuses the read with the given request id because the central is not authorised, and reports the reason if the answer could not be sent.
 * @function_end
 */

/**
 * @function bluetooth_le_server_respond_write
 * @desc This function answers a write request that the server's `on_write` callback (see ${function.bluetooth_le_server_start}) reported. ${constant.BluetoothAttError}.Success accepts the write; any other code refuses it with that ATT error.
 *
 * You must call this function for every write request, once, including a write without response (`response_needed` is `false`): nothing is sent to the central then, but the request is released. The request's value is freed with it, so you must copy the value with ${function.bluetooth_value_fetch} before you answer, and you need not release a value you answer without reading. A write without response that you do not answer is dropped 5 seconds after it arrived, and any other write request expires after 30 seconds, as described for ${function.bluetooth_le_server_respond_read}.
 *
 * The function returns ${constant.BluetoothError}.Ok when the answer was sent or the request released, otherwise the reason, which is also in ${function.bluetooth_last_error_code} and ${function.bluetooth_last_error_message}: NotInitialized before ${function.bluetooth_initialize}, InvalidHandle for a request id that is unknown, already answered or expired, InvalidArgument for an `error_code` that is not a ${constant.BluetoothAttError} member or for the id of a read request (answer it with ${function.bluetooth_le_server_respond_read} or ${function.bluetooth_le_server_respond_read_error}), and OperationFailed when the platform could not send the answer. A call that fails a check can be retried.
 *
 * [[Note: A long write, or a central writing several attributes in one go, is answered once on Android, macOS and iOS: when the last of its requests has been answered, with the first error code given, or Success.]]
 * @param {Real} request_id The id of the request, as the server's `on_write` callback reported it.
 * @param {Enum.BluetoothAttError} error_code ${constant.BluetoothAttError}.Success to accept the write, or the ATT error to refuse it with.
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the answer was sent or the request released, otherwise the reason it was not.
 * @example
 * ```gml
 * on_write_request = function(_request_id, _connection, _characteristic, _descriptor, _offset, _response_needed, _value, _size)
 * {
 *     if (_characteristic != characteristic_command || _descriptor != 0 || _offset != 0 || _size < 1)
 *     {
 *         bluetooth_le_server_respond_write(_request_id, BluetoothAttError.WriteNotPermitted);
 *         return;
 *     }
 *
 *     var _buffer = bluetooth_value_to_buffer(_value, _size);
 *     if (_buffer == -1)
 *     {
 *         bluetooth_le_server_respond_write(_request_id, BluetoothAttError.UnlikelyError);
 *         return;
 *     }
 *
 *     var _command = buffer_peek(_buffer, 0, buffer_u8);
 *     buffer_delete(_buffer);
 *     show_debug_message($"Command {_command} from connection {_connection}");
 *     bluetooth_le_server_respond_write(_request_id, BluetoothAttError.Success);
 * };
 * ```
 * The above code creates the method `on_write_request`, which is passed as the `on_write` callback of ${function.bluetooth_le_server_start}. A write that is not to the start of the characteristic stored in `characteristic_command`, or that holds no bytes, is refused with ${constant.BluetoothAttError}.WriteNotPermitted; its value is freed with the request, so it needs no release. Otherwise the bytes are copied into a new buffer with ${function.bluetooth_value_to_buffer}, the first byte is read as a command and the write is accepted with ${function.bluetooth_le_server_respond_write}. A value that could not be fetched is refused with ${constant.BluetoothAttError}.UnlikelyError. Every request is answered once, a write without response included.
 * @function_end
 */

/**
 * @function bluetooth_le_server_notify_value
 * @desc This function pushes a new value of a server characteristic to the centrals subscribed to it, as a notification or an indication, whichever each central subscribed with. The characteristic is a handle from a service that ${function.bluetooth_le_server_add_service} added. With a connection of 0 the value goes to every subscriber; with a server connection handle (from the server's `on_connected`, `on_read` or `on_write` callbacks) it goes to that central only. A call with no subscriber to send to returns ${constant.BluetoothError}.Ok and sends nothing.
 *
 * The value is copied, so you can free the buffer as soon as the function returns. It is queued and sent as the link allows: while 64 values are waiting to be sent the function returns Busy, and you should try again later, for example on the next step. A notification carries at most `mtu - 3` bytes of the central's link, which ${function.bluetooth_le_connection_get_mtu} returns for its server connection.
 *
 * The function returns ${constant.BluetoothError}.Ok when the value was queued, otherwise the reason, which is also in ${function.bluetooth_last_error_code} and ${function.bluetooth_last_error_message}: NotInitialized before ${function.bluetooth_initialize}, InvalidHandle for a characteristic that is not a live server characteristic or a connection that is not a connected server connection, InvalidArgument for an offset and size outside the buffer, NotFound when the given central is not subscribed to the characteristic, or Busy.
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
 * var _error = bluetooth_le_server_notify_value(characteristic_status, 0, _buffer, 0, 4);
 * buffer_delete(_buffer);
 *
 * if (_error == BluetoothError.Busy)
 * {
 *     notify_pending = true;
 * }
 * else if (_error != BluetoothError.Ok)
 * {
 *     show_debug_message($"Notify failed: {bluetooth_last_error_message()}");
 * }
 * ```
 * The above code writes the instance's `score` to a new buffer as a 32-bit value and sends it to every central subscribed to the characteristic in `characteristic_status`. The buffer is deleted straight away, since the value has been copied. When too many values are waiting the instance sets `notify_pending` to try again later; any other failure is reported with ${function.bluetooth_last_error_message}.
 * @function_end
 */

/**
 * @function bluetooth_le_advertise_start
 * @desc This function starts advertising this device, with the given settings and data, so that scanning centrals can find it. It returns straight away, and `callback` is called once when the advertisement has started or has failed. Only one advertisement runs at a time: a start while one is starting or running returns ${constant.BluetoothError}.Busy, so stop it with ${function.bluetooth_le_advertise_stop} before you start another one. Stopping it before it has started makes the start fail with ${constant.BluetoothError}.OperationFailed. ${function.bluetooth_le_server_stop} also ends it.
 *
 * Once the advertisement runs, `on_stopped` is called once if it ends without ${function.bluetooth_le_advertise_stop} or ${function.bluetooth_le_server_stop}, for example with ${constant.BluetoothError}.BluetoothDisabled when the radio is turned off. An advertisement you end yourself calls nothing.
 *
 * The function returns ${constant.BluetoothError}.Ok when the start has begun. Otherwise it returns the reason, which is also in ${function.bluetooth_last_error_code} and ${function.bluetooth_last_error_message}, and no callback is called:
 *
 * * NotInitialized before ${function.bluetooth_initialize}.
 * * InvalidArgument for a `tx_power` that is not a ${constant.BluetoothLeAdvertiseTxPower} member, a UUID that is not valid, service data whose UUID is not also in `service_uuids`, or a `company_id` outside 0-65535.
 * * PermissionDenied when the Bluetooth permission is missing or was refused, or BluetoothDisabled when the radio is off.
 * * Busy while an advertisement is starting or running.
 * * NotSupported for a part of the settings or data that the platform cannot send. You can ask ${function.bluetooth_feature_is_supported} about each part beforehand: ${constant.BluetoothFeature}.LeAdvertiseName, LeAdvertiseServiceUuids, LeAdvertiseServiceData, LeAdvertiseManufacturerData, LeAdvertiseTxPower, LeAdvertiseIncludeTxPower and LeAdvertiseNonConnectable.
 *
 * Advertising differs a lot between platforms:
 *
 * [[Note: On macOS and iOS the advertisement carries only the local name and the service UUIDs, and is always connectable. `connectable` set to `false`, a `tx_power`, `include_tx_power` set to `true`, service data and manufacturer data each return NotSupported. The name is the computer's name on macOS; on iOS 16 and later it is the model name, such as "iPhone", not the name the user gave the device.]]
 *
 * [[Note: On Windows a service UUID, its service data and the name are advertised only through a GATT service that the server already holds, so every UUID in `service_uuids` must be a service added with ${function.bluetooth_le_server_add_service}, and `include_name` needs at least one such UUID; otherwise the call returns NotSupported. Manufacturer data, `include_tx_power` and `tx_power` go out in a separate advertisement. An advertisement with neither a service UUID nor one of these returns NotSupported.]]
 *
 * [[Note: On Android the advertisement is one legacy advertising packet, with no scan response, and must fit in its 31 bytes: data that does not fit fails in the callback with ${constant.BluetoothError}.InvalidArgument. Android 12 and later need the `BLUETOOTH_ADVERTISE` permission.]]
 * @param {Struct.BluetoothLeAdvertiseSettings} [settings] How the advertisement is sent, or `undefined` for a connectable advertisement at the platform's default power.
 * @param {Struct.BluetoothLeAdvertiseData} data What the advertisement carries.
 * @param {Function} [on_stopped] The function to call when the advertisement ends without ${function.bluetooth_le_advertise_stop} or ${function.bluetooth_le_server_stop}, or `undefined`.
 * @param {Function} callback The function to call once the advertisement has started or has failed.
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the start has begun, otherwise the reason it did not.
 * @event callback:on_stopped
 * @desc Called once, when the running advertisement ends without ${function.bluetooth_le_advertise_stop} or ${function.bluetooth_le_server_stop}. It is the last callback of the advertisement.
 * @member {Enum.BluetoothError} error_code The reason the advertisement ended, such as ${constant.BluetoothError}.BluetoothDisabled when the radio was turned off.
 * @member {String} message A description of why the advertisement ended.
 * @event_end
 * @event callback:callback
 * @desc Called once, when the advertisement has started, has failed or was stopped before it started.
 * @member {Enum.BluetoothError} error_code ${constant.BluetoothError}.Ok if the device is advertising, otherwise the reason it is not.
 * @member {String} message A description of the failure, or an empty string on success.
 * @event_end
 * @example
 * ```gml
 * var _data = new BluetoothLeAdvertiseData();
 * _data.service_uuids = ["6e400001-b5a3-f393-e0a9-e50e24dcca9e"];
 *
 * var _error = bluetooth_le_advertise_start(undefined, _data,
 *     function(_error_code, _message)
 *     {
 *         show_debug_message($"Advertising ended: {_message}");
 *     },
 *     function(_error_code, _message)
 *     {
 *         if (_error_code != BluetoothError.Ok)
 *         {
 *             show_debug_message($"Advertising failed: {_message}");
 *             exit;
 *         }
 *         show_debug_message("Advertising");
 *     });
 *
 * if (_error != BluetoothError.Ok)
 * {
 *     show_debug_message($"Advertising did not start: {bluetooth_last_error_message()}");
 * }
 * ```
 * The above code advertises the UUID of the service that the server holds. The settings are `undefined`, so the advertisement is connectable and sent at the platform's default power, and every other variable of the data is left `undefined`, which leaves the name, the transmit power level, service data and manufacturer data out. This keeps the advertisement within the 31 bytes Android allows, and the service has been added first, as Windows requires, so this advertisement can be sent on every platform. The `callback` reports whether the advertisement started, and `on_stopped` reports an advertisement that ends on its own later; if the call itself fails, the reason is read with ${function.bluetooth_last_error_message} and no callback is called.
 * @function_end
 */

/**
 * @function bluetooth_le_advertise_stop
 * @desc This function stops advertising. The advertisement's `on_stopped` callback is not called for a stop you ask for. A start that is still in progress fails, and its callback is called with ${constant.BluetoothError}.OperationFailed. Stopping when nothing is advertising returns ${constant.BluetoothError}.Ok and does nothing.
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
 * @function bluetooth_le_advertise_is_running
 * @desc This function checks whether an advertisement is starting or running. It returns `true` from a successful ${function.bluetooth_le_advertise_start}, while the start is still in progress, until the advertisement fails to start, is stopped or ends on its own.
 * @returns {Bool} `true` if an advertisement is starting or running, otherwise `false`.
 * @example
 * ```gml
 * draw_text(16, 16, bluetooth_le_advertise_is_running() ? "Advertising" : "Not advertising");
 * ```
 * The above code, in a Draw Event, shows whether the device is advertising.
 * @function_end
 */

// Structs

/**
 * @struct BluetoothLeServiceDefinition
 * @desc This struct describes a primary GATT service for ${function.bluetooth_le_server_add_service}. You create it with `new BluetoothLeServiceDefinition()`. You must set `uuid`; `characteristics` is optional and can stay `undefined` for a service with none.
 * @member {String} uuid The UUID of the service, 16-bit or 128-bit.
 * @member {Array[Struct.BluetoothLeCharacteristicDefinition]} [characteristics] The characteristics of the service, in the order their handles are listed, or `undefined` for none.
 * @struct_end
 */

/**
 * @struct BluetoothLeCharacteristicDefinition
 * @desc This struct describes one characteristic of a ${struct.BluetoothLeServiceDefinition}. You create it with `new BluetoothLeCharacteristicDefinition()`. You must set `uuid`, `properties` and `permissions`; `value` and `descriptors` are optional and can stay `undefined`. A characteristic with the Notify or Indicate property gets its Client Characteristic Configuration descriptor (0x2902) from the platform, so you do not list it.
 *
 * [[Note: Windows ignores the Broadcast property of a server characteristic, and macOS and iOS ignore both Broadcast and ExtendedProperties.]]
 * @member {String} uuid The UUID of the characteristic, 16-bit or 128-bit.
 * @member {Real} properties The ${constant.BluetoothLeCharacteristicProperty} flags of the characteristic, combined with `|`.
 * @member {Real} permissions The ${constant.BluetoothLeAttributePermission} flags of the characteristic, combined with `|`.
 * @member {Array[Real]} [value] The initial value, each entry a byte (0-255), at most 512 bytes, or `undefined` or an empty array for none. A characteristic with an initial value is read from it by the platform, and its reads never reach the server's `on_read` callback.
 * @member {Array[Struct.BluetoothLeDescriptorDefinition]} [descriptors] The descriptors of the characteristic, or `undefined` for none.
 * @struct_end
 */

/**
 * @struct BluetoothLeDescriptorDefinition
 * @desc This struct describes one descriptor of a ${struct.BluetoothLeCharacteristicDefinition}. You create it with `new BluetoothLeDescriptorDefinition()`. You must set `uuid`; `value` is optional.
 *
 * A descriptor with a `value` is served by the platform with that static value, and its reads never reach the server's `on_read` callback. A descriptor without one has its reads and writes reported to `on_read` and `on_write`, on the platforms that report them (${constant.BluetoothFeature}.LeServerDescriptorRequests). A Client Characteristic Configuration descriptor (0x2902) in a definition is left out, since the platform manages it.
 *
 * [[Note: macOS and iOS can host only a Characteristic User Description (0x2901) and a Characteristic Presentation Format (0x2904) descriptor, and only with a `value`; any other descriptor makes ${function.bluetooth_le_server_add_service} return NotSupported there.]]
 * @member {String} uuid The UUID of the descriptor, 16-bit or 128-bit.
 * @member {Array[Real]} [value] The static value of the descriptor, each entry a byte (0-255), at most 512 bytes, or `undefined` for none. The value of a Characteristic User Description (0x2901) is UTF-8 text.
 * @struct_end
 */

/**
 * @struct BluetoothLeAdvertiseSettings
 * @desc This struct says how ${function.bluetooth_le_advertise_start} sends the advertisement. You create it with `new BluetoothLeAdvertiseSettings()`; both variables are optional and take their default when left `undefined`.
 * @member {Bool} [connectable] Whether centrals can connect to the device from the advertisement, `true` when `undefined`. macOS and iOS always advertise as connectable.
 * @member {Enum.BluetoothLeAdvertiseTxPower} [tx_power] The transmit power to advertise at, or `undefined` for the platform's default.
 * @struct_end
 */

/**
 * @struct BluetoothLeAdvertiseData
 * @desc This struct says what the advertisement of ${function.bluetooth_le_advertise_start} carries. You create it with `new BluetoothLeAdvertiseData()` and set the parts you want to send; every variable is optional, and one left `undefined` is off or empty. A platform that cannot send a part you ask for makes the call return NotSupported (see ${function.bluetooth_le_advertise_start}).
 * @member {Bool} [include_name] Whether the advertisement carries the device's name. On iOS 16 and later that is the model name, such as "iPhone", not the name the user gave the device.
 * @member {Bool} [include_tx_power] Whether the advertisement carries the transmit power level.
 * @member {Array[String]} [service_uuids] The UUIDs of the services to advertise, or `undefined` for none.
 * @member {Array[Struct.BluetoothLeAdvertiseServiceData]} [service_data] The service data to advertise, or `undefined` for none. Each entry's UUID must also be in `service_uuids`.
 * @member {Array[Struct.BluetoothLeAdvertiseManufacturerData]} [manufacturer_data] The manufacturer-specific data to advertise, or `undefined` for none.
 * @struct_end
 */

/**
 * @struct BluetoothLeAdvertiseServiceData
 * @desc This struct holds the data advertised for one service. You create it with `new BluetoothLeAdvertiseServiceData()` to advertise it in a ${struct.BluetoothLeAdvertiseData}, and ${function.bluetooth_device_get_advertisement} returns it as part of what a scanned device advertised. You must set both variables.
 * @member {String} uuid The UUID of the service the data belongs to. When advertising, it must also be in the data's `service_uuids`.
 * @member {Array[Real]} data The service data, each entry a byte (0-255).
 * @struct_end
 */

/**
 * @struct BluetoothLeAdvertiseManufacturerData
 * @desc This struct holds manufacturer-specific data, under a company identifier assigned by the Bluetooth SIG. You create it with `new BluetoothLeAdvertiseManufacturerData()` to advertise it in a ${struct.BluetoothLeAdvertiseData}, and ${function.bluetooth_device_get_advertisement} returns it as part of what a scanned device advertised. You must set both variables.
 * @member {Real} company_id The Bluetooth SIG company identifier (0-65535).
 * @member {Array[Real]} data The manufacturer-specific data, each entry a byte (0-255).
 * @struct_end
 */

// Constants

/**
 * @const BluetoothAttError
 * @desc This enum holds the ATT error codes (Bluetooth Core Specification, Vol 3, Part F, 3.4.1.1) that answer a read request with ${function.bluetooth_le_server_respond_read_error} or a write request with ${function.bluetooth_le_server_respond_write}. Each code is sent to the central as it is.
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
 * ${function.bluetooth_le_server_start} takes the functions to call for the server's events, from centrals connecting and disconnecting to their read and write requests, and ${function.bluetooth_le_advertise_start} the function to call when the advertisement ends on its own. Both keep them until they end. Neither calls anything when you end it yourself, and stopping the server also ends the advertisement and retires every central.
 *
 * @section_func
 * @desc These are the functions of the GATT server and of advertising.
 * @ref bluetooth_le_server_start
 * @ref bluetooth_le_server_stop
 * @ref bluetooth_le_server_is_running
 * @ref bluetooth_le_server_add_service
 * @ref bluetooth_le_server_clear_services
 * @ref bluetooth_le_server_respond_read
 * @ref bluetooth_le_server_respond_read_error
 * @ref bluetooth_le_server_respond_write
 * @ref bluetooth_le_server_notify_value
 * @ref bluetooth_le_advertise_start
 * @ref bluetooth_le_advertise_stop
 * @ref bluetooth_le_advertise_is_running
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
