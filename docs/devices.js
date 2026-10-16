// Devices: the shared device cache that LE and Classic scans fill, device ids, pairing, and the
// device_found and scan_stopped callbacks.

/**
 * @function bluetooth_device_clear
 * @desc This function empties the device cache. Every device handle the cache has handed out becomes
 * invalid, and ${function.bluetooth_device_get_count} returns 0 afterwards.
 *
 * The cache keeps every device seen since the last clear, whichever scan found it, so you should
 * call this function from time to time during a long scan in a busy place. Device handles are never
 * reused: a device found again after the clear enters the cache under a new handle, and the
 * device_found callback set with ${function.bluetooth_set_callback_device_found} is called for it
 * again. An old handle can therefore never name a different device.
 *
 * Clearing the cache does not close anything. A connection made to a device stays open, and a
 * pairing still in progress still calls its callback with the old handle, but the device getters
 * return their empty values for that handle from now on.
 *
 * [[Note: ${function.bluetooth_shutdown} also empties the cache.]]
 * @example
 * ```gml
 * bluetooth_device_clear();
 * devices = [];
 * bluetooth_le_scan_start(true, []);
 * ```
 * The above code empties the device cache and the instance's own `devices` array before it starts a
 * new LE scan with ${function.bluetooth_le_scan_start}, so every device the scan reports gets a new
 * handle.
 * @function_end
 */

/**
 * @function bluetooth_device_get_count
 * @desc This function returns the number of devices in the device cache. The count includes every
 * device found by an LE or a Classic scan since the last call to ${function.bluetooth_device_clear},
 * as well as the devices added by ${function.bluetooth_device_from_id}, by
 * ${function.bluetooth_paired_devices_query} and ${function.bluetooth_le_connected_devices_query},
 * and the centrals that connect to the LE server.
 * @returns {Real} The number of devices in the cache.
 * @example
 * ```gml
 * show_debug_message($"{bluetooth_device_get_count()} devices found so far");
 * ```
 * The above code shows the number of devices in the cache in the output log.
 * @function_end
 */

/**
 * @function bluetooth_device_get_at
 * @desc This function returns the handle of the device at the given index in the device cache.
 * Devices are kept in the order they entered the cache, from 0 to
 * ${function.bluetooth_device_get_count} - 1. The function returns 0 for an index outside that
 * range.
 * @param {Real} index The index of the device in the cache, starting at 0.
 * @returns {Real} The handle of the device, or 0 if the index is out of range.
 * @example
 * ```gml
 * var _count = bluetooth_device_get_count();
 * for (var i = 0; i < _count; i++)
 * {
 *     var _device = bluetooth_device_get_at(i);
 *     show_debug_message($"{i}: {bluetooth_device_get_name(_device)}");
 * }
 * ```
 * The above code loops through every device in the cache and shows the name of each one, read with
 * ${function.bluetooth_device_get_name}, in the output log.
 * @function_end
 */

/**
 * @function bluetooth_device_is_valid
 * @desc This function checks whether the given device handle names a device in the device cache. It
 * returns `false` for a handle that ${function.bluetooth_device_clear} or
 * ${function.bluetooth_shutdown} invalidated, for 0, and for a handle of another kind, such as a
 * connection handle.
 * @param {Real} device The device handle to check.
 * @returns {Bool} `true` if the handle names a device in the cache, otherwise `false`.
 * @example
 * ```gml
 * if (!bluetooth_device_is_valid(device))
 * {
 *     device = 0;
 * }
 * ```
 * The above code forgets the handle stored in the instance's `device` variable when it no longer
 * names a cached device.
 * @function_end
 */

/**
 * @function bluetooth_device_from_id
 * @desc This function returns the handle of the device that the given id names. The id is one that
 * ${function.bluetooth_device_get_id} returned, possibly in an earlier run of the game, so you can
 * store it and use this function to reconnect to the same device later without scanning for it.
 *
 * A device already in the cache keeps its handle and that handle is returned. Otherwise the device
 * enters the cache under a new handle without calling the device_found callback, and its name is an
 * empty string until the platform knows it.
 *
 * The function returns 0 when it cannot make a handle, and the reason is then in
 * ${function.bluetooth_last_error_code} and ${function.bluetooth_last_error_message}:
 * NotInitialized, or InvalidArgument for a malformed id or an id issued by another platform (a
 * Windows id on Android, for example).
 *
 * [[Note: On Apple platforms an LE id names a peripheral that CoreBluetooth must already know: the
 * function fails with BluetoothDisabled while the radio is off, NotFound for an id the system does not
 * know, and PermissionDenied until ${function.bluetooth_permission_request} has been called.]]
 *
 * [[Note: On Android the function fails with NotSupported on a device with no Bluetooth adapter,
 * and with OperationFailed when the device cannot be added to the cache.]]
 *
 * [[Note: On Windows the device is built from the address in the id, so the function succeeds
 * without contacting the device.]]
 * @param {String} id The device id, as returned by ${function.bluetooth_device_get_id}.
 * @returns {Real} The handle of the device, or 0 if it could not be made.
 * @example
 * ```gml
 * var _device = bluetooth_device_from_id(saved_id);
 * if (_device == 0)
 * {
 *     show_debug_message($"Unknown device: {bluetooth_last_error_message()}");
 *     exit;
 * }
 * connection = bluetooth_le_connect(_device, on_connected);
 * ```
 * The above code turns the id stored in the instance's `saved_id` variable back into a device handle
 * and connects to it with ${function.bluetooth_le_connect}, without a scan. If the id cannot be used,
 * the reason is shown with ${function.bluetooth_last_error_message}.
 * @function_end
 */

/**
 * @function bluetooth_device_get_transport
 * @desc This function returns the transport of the given device: Classic for a device found by a
 * Classic scan, LowEnergy for an LE device. It returns ${constant.BluetoothTransport}.Unknown for a
 * handle that is not valid.
 *
 * [[Note: A device that supports both transports is two separate devices in the cache, one per
 * transport, each with its own handle and id.]]
 * @param {Real} device The handle of the device.
 * @returns {Enum.BluetoothTransport} The transport of the device.
 * @example
 * ```gml
 * if (bluetooth_device_get_transport(device) == BluetoothTransport.Classic)
 * {
 *     bluetooth_classic_connect(device, service_uuid, on_connected);
 * }
 * ```
 * The above code connects with ${function.bluetooth_classic_connect} only when the device is a
 * Classic device.
 * @function_end
 */

/**
 * @function bluetooth_device_get_id
 * @desc This function returns the id of the given device, a string you can store and later pass to
 * ${function.bluetooth_device_from_id} to get the device back without a scan. It returns an empty
 * string for a handle that is not valid.
 *
 * The id is opaque and specific to the platform: you should not parse it or assume it holds a MAC
 * address. Each platform prefixes its ids with its own name and the transport, so an id from one
 * platform is refused on another:
 *
 * * Windows: `win:ble:` or `win:classic:` followed by the device address.
 * * Android: `android:ble:` or `android:classic:` followed by the device address.
 * * macOS and iOS: `apple:ble:` followed by the identifier CoreBluetooth assigned to the peripheral,
 *   or on macOS `apple:classic:` followed by the device address.
 *
 * [[Note: CoreBluetooth makes up the identifier of an LE peripheral itself, so an Apple LE id only
 * names the device on the system that issued it.]]
 * @param {Real} device The handle of the device.
 * @returns {String} The id of the device, or an empty string if the handle is not valid.
 * @example
 * ```gml
 * saved_id = bluetooth_device_get_id(device);
 * ```
 * The above code stores the id of the device in the instance's `saved_id` variable, so the game can
 * save it and reconnect to the device later with ${function.bluetooth_device_from_id}.
 * @function_end
 */

/**
 * @function bluetooth_device_get_name
 * @desc This function returns the name of the given device. The name is updated whenever a later
 * advertisement or inquiry result carries one, so it can be an empty string when the device is first
 * found and filled in later. It is also an empty string for a device that has never reported a name
 * and for a handle that is not valid.
 * @param {Real} device The handle of the device.
 * @returns {String} The name of the device, or an empty string if it is not known.
 * @example
 * ```gml
 * var _name = bluetooth_device_get_name(device);
 * if (_name == "")
 * {
 *     _name = "(unnamed)";
 * }
 * show_debug_message($"Found {_name}");
 * ```
 * The above code shows the name of the device in the output log, or "(unnamed)" when it has none.
 * @function_end
 */

/**
 * @function bluetooth_device_has_address
 * @desc This function checks whether the address of the given device is known. You should call this
 * function before you use ${function.bluetooth_device_get_address}.
 *
 * [[Note: Apple platforms never expose the address of an LE device, so this function returns
 * `false` for every LE device on macOS and iOS. It returns `true` for a Classic device on macOS.]]
 * @param {Real} device The handle of the device.
 * @returns {Bool} `true` if the address of the device is known, otherwise `false`.
 * @example
 * ```gml
 * if (bluetooth_device_has_address(device))
 * {
 *     show_debug_message($"Address: {bluetooth_device_get_address(device)}");
 * }
 * ```
 * The above code shows the address of the device, read with ${function.bluetooth_device_get_address},
 * only when it is known.
 * @function_end
 */

/**
 * @function bluetooth_device_get_address
 * @desc This function returns the Bluetooth address of the given device, in the form
 * `AA:BB:CC:DD:EE:FF`. It returns an empty string when the address is not known (see
 * ${function.bluetooth_device_has_address}) and for a handle that is not valid.
 *
 * To remember a device between runs of the game, store its id from
 * ${function.bluetooth_device_get_id} instead: not every platform exposes an address.
 * @param {Real} device The handle of the device.
 * @returns {String} The address of the device, or an empty string if it is not known.
 * @example
 * ```gml
 * var _address = bluetooth_device_get_address(device);
 * if (_address != "")
 * {
 *     show_debug_message($"Address: {_address}");
 * }
 * ```
 * The above code shows the address of the device in the output log when it is known.
 * @function_end
 */

/**
 * @function bluetooth_device_has_rssi
 * @desc This function checks whether a signal strength (RSSI) has been reported for the given
 * device. You should call this function before you use ${function.bluetooth_device_get_rssi}.
 *
 * Scans report the signal strength. A device that entered the cache without a scan (through
 * ${function.bluetooth_device_from_id} or a device query, for example) has none until a scan sees
 * it.
 *
 * [[Note: Classic scans on Windows and macOS report no signal strength, so this function returns
 * `false` for those devices.]]
 * @param {Real} device The handle of the device.
 * @returns {Bool} `true` if a signal strength has been reported for the device, otherwise `false`.
 * @example
 * ```gml
 * if (bluetooth_device_has_rssi(device))
 * {
 *     show_debug_message($"RSSI: {bluetooth_device_get_rssi(device)} dBm");
 * }
 * ```
 * The above code shows the signal strength of the device, read with
 * ${function.bluetooth_device_get_rssi}, only when one has been reported.
 * @function_end
 */

/**
 * @function bluetooth_device_get_rssi
 * @desc This function returns the most recent signal strength (RSSI) reported for the given device,
 * in dBm. Values are negative, and a value closer to 0 means a stronger signal. The value is updated
 * by every later advertisement or inquiry result the device sends while a scan runs.
 *
 * The function returns 0 when no signal strength has been reported (see
 * ${function.bluetooth_device_has_rssi}) and for a handle that is not valid.
 * @param {Real} device The handle of the device.
 * @returns {Real} The signal strength of the device (in dBm), or 0 if none has been reported.
 * @example
 * ```gml
 * var _closest = 0;
 * var _best = -1000;
 * var _count = bluetooth_device_get_count();
 * for (var i = 0; i < _count; i++)
 * {
 *     var _device = bluetooth_device_get_at(i);
 *     if (bluetooth_device_has_rssi(_device) && bluetooth_device_get_rssi(_device) > _best)
 *     {
 *         _best = bluetooth_device_get_rssi(_device);
 *         _closest = _device;
 *     }
 * }
 * ```
 * The above code goes through every device in the cache and keeps the handle of the one with the
 * strongest signal in the local variable `_closest`.
 * @function_end
 */

/**
 * @function bluetooth_device_is_connectable
 * @desc This function checks whether the given device accepts connections. For an LE device this
 * comes from its advertisements: once any advertisement has said the device is connectable, this
 * function keeps returning `true`. Classic devices are reported as connectable. The function returns
 * `false` for a handle that is not valid.
 *
 * [[Note: On Windows, a device added with ${function.bluetooth_device_from_id} reports `false`
 * until a scan sees it send a connectable advertisement.]]
 * @param {Real} device The handle of the device.
 * @returns {Bool} `true` if the device accepts connections, otherwise `false`.
 * @example
 * ```gml
 * if (bluetooth_device_is_connectable(device))
 * {
 *     connection = bluetooth_le_connect(device, on_connected);
 * }
 * ```
 * The above code connects to the device with ${function.bluetooth_le_connect} only when it
 * accepts connections.
 * @function_end
 */

/**
 * @function bluetooth_device_get_advertisement
 * @desc This function returns what the given device has advertised, as a
 * ${struct.BluetoothLeAdvertisement} struct. The struct merges every advertisement and scan response
 * received from the device since it entered the cache, not only the latest one.
 *
 * Every array in the struct is empty, and `tx_power` is `undefined`, for a Classic device, for a
 * device that has never been seen advertising (one added with ${function.bluetooth_device_from_id},
 * for example) and for a handle that is not valid.
 * @param {Real} device The handle of the device.
 * @returns {Struct.BluetoothLeAdvertisement} What the device has advertised.
 * @example
 * ```gml
 * var _advertisement = bluetooth_device_get_advertisement(device);
 * for (var i = 0; i < array_length(_advertisement.manufacturer_data); i++)
 * {
 *     var _entry = _advertisement.manufacturer_data[i];
 *     show_debug_message($"Company {_entry.company_id}: {array_length(_entry.data)} bytes");
 * }
 * ```
 * The above code reads the advertisement of the device and shows the company identifier and the
 * length of each manufacturer data entry in the output log.
 * @function_end
 */

/**
 * @function bluetooth_paired_devices_query
 * @desc This function lists the devices that are paired (bonded) with this system, without a scan.
 * The callback is called once with the handles of the paired devices. A paired device that is not
 * yet in the cache enters it without calling the device_found callback.
 *
 * The function returns a ${constant.BluetoothError}, and the callback is never called when it is
 * not Ok: NotInitialized, NotSupported where the platform cannot list paired devices or there is no
 * Bluetooth hardware, PermissionDenied, or BluetoothDisabled. The reason is also in
 * ${function.bluetooth_last_error_code} and ${function.bluetooth_last_error_message}. If you call
 * ${function.bluetooth_shutdown} before the answer arrives, the callback is called with
 * NotInitialized.
 *
 * Use ${function.bluetooth_feature_is_supported} with ${constant.BluetoothFeature}.PairedDevicesQuery
 * to check whether the platform can list paired devices.
 *
 * [[Note: Android lists Classic and LE bonds, with a dual-mode device as one entry of each transport.
 * Windows lists paired Classic and LE devices. macOS lists paired Classic devices only. iOS returns
 * NotSupported.]]
 *
 * [[Note: A Classic scan started with ${function.bluetooth_classic_scan_start} reports only the
 * devices that answer its inquiry, never the paired list, so use this function to find a paired
 * device that is out of range or not discoverable.]]
 * @param {Function} callback The function to call once with the list of paired devices.
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the query started, otherwise the
 * reason it did not.
 * @event callback
 * @desc Called once, with the paired devices or the reason the list could not be read.
 * @member {Enum.BluetoothError} error_code ${constant.BluetoothError}.Ok if the list was read,
 * otherwise the reason it was not.
 * @member {String} message A description of the failure, or an empty string on success.
 * @member {Array[Real]} devices The handles of the paired devices, or an empty array on failure.
 * @event_end
 * @example
 * ```gml
 * var _error = bluetooth_paired_devices_query(function(_error_code, _message, _devices)
 * {
 *     if (_error_code != BluetoothError.Ok)
 *     {
 *         show_debug_message($"Could not list paired devices: {_message}");
 *         exit;
 *     }
 *     for (var i = 0; i < array_length(_devices); i++)
 *     {
 *         show_debug_message($"Paired: {bluetooth_device_get_name(_devices[i])}");
 *     }
 * });
 *
 * if (_error != BluetoothError.Ok)
 * {
 *     show_debug_message($"Query did not start: {bluetooth_last_error_message()}");
 * }
 * ```
 * The above code asks for the list of paired devices. When the callback reports a failure the
 * message is shown; on success the name of every paired device is shown in the output log. If the
 * query could not be started at all, the reason is read with
 * ${function.bluetooth_last_error_message} and the callback is never called.
 * @function_end
 */

/**
 * @function bluetooth_pairing_is_supported
 * @desc This function checks whether ${function.bluetooth_pair} can pair with the given device. The
 * answer depends on the device and its transport, not only on the platform. It returns `false` for
 * a handle that is not valid and when Bluetooth is not initialised.
 *
 * [[Note: Android can pair Classic and LE devices. Windows can pair Classic devices only. macOS can
 * pair Classic devices whose address is known. On Apple platforms LE pairing happens on its own when
 * a protected characteristic is used, so this function returns `false` for every LE device there,
 * and for every device on iOS.]]
 * @param {Real} device The handle of the device.
 * @returns {Bool} `true` if the device can be paired with ${function.bluetooth_pair}, otherwise
 * `false`.
 * @example
 * ```gml
 * if (bluetooth_pairing_is_supported(device) && !bluetooth_device_is_paired(device))
 * {
 *     bluetooth_pair(device, on_paired);
 * }
 * ```
 * The above code pairs with the device using ${function.bluetooth_pair} only when the platform can
 * pair it and it is not paired already.
 * @function_end
 */

/**
 * @function bluetooth_pair
 * @desc This function asks the system to pair (bond) with the given device. Pairing is separate
 * from connecting with ${function.bluetooth_classic_connect} or ${function.bluetooth_le_connect}:
 * some devices only accept a connection once they are paired, and the system remembers the pairing
 * across runs of the game. The user may be shown a system pairing prompt.
 *
 * The function returns straight away, and the callback is called once, later, when the pairing
 * succeeds or fails. On Windows and Android, a device that is already paired completes at once with
 * ${constant.BluetoothError}.Ok.
 *
 * The function returns a ${constant.BluetoothError}, and the callback is never called when it is
 * not Ok: NotInitialized, InvalidHandle for a device handle that is not valid, PermissionDenied,
 * BluetoothDisabled, NotSupported for a device the platform cannot pair (see
 * ${function.bluetooth_pairing_is_supported}), InvalidArgument for a device with no usable address,
 * Busy while a pairing of the same device is already in progress, or OperationFailed when the
 * system could not start the pairing. The reason is also in ${function.bluetooth_last_error_code}
 * and ${function.bluetooth_last_error_message}.
 *
 * When the pairing fails, is rejected or is cancelled by the user, the callback reports
 * ${constant.BluetoothError}.OperationFailed. If you call ${function.bluetooth_shutdown} before the
 * pairing completes, the callback is called with NotInitialized.
 *
 * [[Note: Android pairs Classic and LE devices. Windows and macOS pair Classic devices only and
 * return NotSupported for an LE device. iOS always returns NotSupported.]]
 *
 * [[Note: On macOS the function returns NotFound when the system cannot find the device, and a
 * pairing that has not completed after 60 seconds fails with ${constant.BluetoothError}.Timeout.]]
 * @param {Real} device The handle of the device to pair with.
 * @param {Function} callback The function to call once the pairing succeeds or fails.
 * @returns {Enum.BluetoothError} ${constant.BluetoothError}.Ok if the pairing started, otherwise the
 * reason it did not.
 * @event callback
 * @desc Called once, when the pairing succeeds, fails or is cancelled.
 * @member {Enum.BluetoothError} error_code ${constant.BluetoothError}.Ok if the device is paired,
 * otherwise the reason it is not.
 * @member {String} message A description of the failure, or an empty string on success.
 * @member {Real} device The handle of the device passed to ${function.bluetooth_pair}.
 * @event_end
 * @example
 * ```gml
 * var _error = bluetooth_pair(device, function(_error_code, _message, _device)
 * {
 *     if (_error_code != BluetoothError.Ok)
 *     {
 *         show_debug_message($"Pairing failed: {_message}");
 *         exit;
 *     }
 *     show_debug_message($"Paired with {bluetooth_device_get_name(_device)}");
 * });
 *
 * if (_error != BluetoothError.Ok)
 * {
 *     show_debug_message($"Pairing did not start: {bluetooth_last_error_message()}");
 * }
 * ```
 * The above code asks the system to pair with the device stored in the instance's `device`
 * variable. The callback shows the reason when the pairing fails, or the name of the paired device
 * on success. If the pairing could not be started at all, the reason is read with
 * ${function.bluetooth_last_error_message} and the callback is never called.
 * @function_end
 */

/**
 * @function bluetooth_device_is_paired
 * @desc This function checks whether the given device is currently paired (bonded) with this system.
 * It returns `false` for a handle that is not valid and when Bluetooth is not initialised.
 *
 * [[Note: Android reports pairing for Classic and LE devices, and returns `false` without the
 * Bluetooth connect permission. Windows and macOS report it for Classic devices only and return
 * `false` for every LE device. iOS always returns `false`.]]
 * @param {Real} device The handle of the device.
 * @returns {Bool} `true` if the device is paired, otherwise `false`.
 * @example
 * ```gml
 * if (bluetooth_device_is_paired(device))
 * {
 *     show_debug_message("Already paired");
 * }
 * ```
 * The above code shows a message in the output log when the device is already paired.
 * @function_end
 */

/**
 * @function bluetooth_set_callback_device_found
 * @desc This function sets the function to call when a scan adds a new device to the device cache.
 * Both LE scans (${function.bluetooth_le_scan_start}) and Classic scans
 * (${function.bluetooth_classic_scan_start}) call it, so use
 * ${function.bluetooth_device_get_transport} to tell them apart. Setting a new function replaces the
 * previous one.
 *
 * The callback is called once per device, not once per advertisement: later advertisements and
 * inquiry results from the same device update its name, signal strength and advertisement in the
 * cache, which the device getters return, without calling the callback again. After
 * ${function.bluetooth_device_clear} a device found again is a new device and the callback is called
 * for it again. When an LE scan has filters, a device is added, and the callback called, once its
 * merged advertisement first matches one of them.
 *
 * A device that entered the cache without a scan is never reported here, even when a scan sees it
 * later: one added with ${function.bluetooth_device_from_id}, ${function.bluetooth_paired_devices_query}
 * or ${function.bluetooth_le_connected_devices_query}, or a central that connects to the LE server.
 *
 * The callback stays set across ${function.bluetooth_shutdown} and ${function.bluetooth_initialize},
 * until you call ${function.bluetooth_remove_callback_device_found}.
 * @param {Function} callback The function to call for each new device.
 * @returns {Bool} Always `true`.
 * @event callback
 * @desc Called once for each device a scan adds to the cache.
 * @member {Real} device The handle of the new device.
 * @event_end
 * @example
 * ```gml
 * bluetooth_set_callback_device_found(function(_device)
 * {
 *     var _kind = (bluetooth_device_get_transport(_device) == BluetoothTransport.LowEnergy) ? "LE" : "Classic";
 *     show_debug_message($"Found {_kind} device {bluetooth_device_get_name(_device)}");
 *     array_push(devices, _device);
 * });
 * ```
 * The above code sets a function that shows the transport and name of every new device in the
 * output log and adds its handle to the instance's `devices` array.
 * @function_end
 */

/**
 * @function bluetooth_remove_callback_device_found
 * @desc This function removes the function set with ${function.bluetooth_set_callback_device_found}.
 * Scans keep adding devices to the cache, but nothing is called for them.
 * @returns {Bool} Always `true`.
 * @example
 * ```gml
 * bluetooth_remove_callback_device_found();
 * ```
 * The above code stops the game from being told about new devices.
 * @function_end
 */

/**
 * @function bluetooth_set_callback_scan_stopped
 * @desc This function sets the function to call when an LE or a Classic scan stops. Setting a new
 * function replaces the previous one.
 *
 * The callback is called once per scan, whether the game stopped the scan with
 * ${function.bluetooth_le_scan_stop} or ${function.bluetooth_classic_scan_stop}, the scan ended on
 * its own (a Classic inquiry lasts a limited time), or the system ended it, for example because the
 * radio was turned off. A stop while no scan is running calls nothing.
 *
 * The callback stays set across ${function.bluetooth_shutdown} and ${function.bluetooth_initialize},
 * until you call ${function.bluetooth_remove_callback_scan_stopped}.
 * @param {Function} callback The function to call when a scan stops.
 * @returns {Bool} Always `true`.
 * @event callback
 * @desc Called once each time an LE or a Classic scan stops.
 * @member {Enum.BluetoothError} error_code ${constant.BluetoothError}.Ok if the scan ended without
 * an error, otherwise the reason it ended, such as BluetoothDisabled when the radio was turned off.
 * @member {String} message A description of the failure, or an empty string on success.
 * @member {Enum.BluetoothTransport} transport The transport of the scan that stopped.
 * @event_end
 * @example
 * ```gml
 * bluetooth_set_callback_scan_stopped(function(_error_code, _message, _transport)
 * {
 *     var _kind = (_transport == BluetoothTransport.Classic) ? "Classic" : "LE";
 *     if (_error_code != BluetoothError.Ok)
 *     {
 *         show_debug_message($"{_kind} scan ended: {_message}");
 *         exit;
 *     }
 *     show_debug_message($"{_kind} scan stopped, {bluetooth_device_get_count()} devices cached");
 * });
 * ```
 * The above code sets a function that shows why a scan ended when it ended with an error, or
 * otherwise shows how many devices are in the cache, read with
 * ${function.bluetooth_device_get_count}.
 * @function_end
 */

/**
 * @function bluetooth_remove_callback_scan_stopped
 * @desc This function removes the function set with ${function.bluetooth_set_callback_scan_stopped}.
 * Scans still stop as before, but nothing is called when they do.
 * @returns {Bool} Always `true`.
 * @example
 * ```gml
 * bluetooth_remove_callback_scan_stopped();
 * ```
 * The above code stops the game from being told when a scan stops.
 * @function_end
 */

/**
 * @struct BluetoothLeAdvertisement
 * @desc This struct holds what an LE device has advertised, as returned by
 * ${function.bluetooth_device_get_advertisement}. It merges every advertisement and scan response
 * received from the device since it entered the cache. Every array is empty for a Classic device and
 * for a device never seen advertising. Each array holds at most 32 entries.
 * @member {Array[String]} service_uuids Every service UUID the device has advertised, as lowercase
 * 128-bit UUID strings.
 * @member {Array[Struct.BluetoothLeAdvertiseServiceData]} service_data The service data the device
 * has advertised, the latest entry for each service UUID.
 * @member {Array[Struct.BluetoothLeAdvertiseManufacturerData]} manufacturer_data The manufacturer
 * data the device has advertised, the latest entry for each company identifier.
 * @member {Real} [tx_power] The latest TX Power Level the device advertised (in dBm), or
 * `undefined` if it has not advertised one.
 * @struct_end
 */

/**
 * @module devices
 * @title Devices
 * @desc The device cache that LE and Classic scans fill, and the functions to read the devices in
 * it, get a device back from a saved id, list and pair devices, and hear about new devices and
 * stopped scans.
 *
 * Scans are started with ${function.bluetooth_le_scan_start} and
 * ${function.bluetooth_classic_scan_start}. Every device they find is added to one shared cache and
 * given a device handle, a number that stays valid until ${function.bluetooth_device_clear} or
 * ${function.bluetooth_shutdown} empties the cache. Handles are never reused. To remember a device
 * between runs of the game, store its id from ${function.bluetooth_device_get_id} and get a handle
 * back with ${function.bluetooth_device_from_id}.
 *
 * @section_func
 * @desc These are the functions to manage and read the device cache, to pair devices, and to be told
 * about new devices and stopped scans.
 * @ref bluetooth_device_clear
 * @ref bluetooth_device_get_count
 * @ref bluetooth_device_get_at
 * @ref bluetooth_device_is_valid
 * @ref bluetooth_device_from_id
 * @ref bluetooth_device_get_transport
 * @ref bluetooth_device_get_id
 * @ref bluetooth_device_get_name
 * @ref bluetooth_device_has_address
 * @ref bluetooth_device_get_address
 * @ref bluetooth_device_has_rssi
 * @ref bluetooth_device_get_rssi
 * @ref bluetooth_device_is_connectable
 * @ref bluetooth_device_get_advertisement
 * @ref bluetooth_paired_devices_query
 * @ref bluetooth_pairing_is_supported
 * @ref bluetooth_pair
 * @ref bluetooth_device_is_paired
 * @ref bluetooth_set_callback_device_found
 * @ref bluetooth_remove_callback_device_found
 * @ref bluetooth_set_callback_scan_stopped
 * @ref bluetooth_remove_callback_scan_stopped
 * @section_end
 *
 * @section_struct
 * @desc This is the struct used by the device cache.
 * @ref BluetoothLeAdvertisement
 * @section_end
 *
 * @module_end
 */
