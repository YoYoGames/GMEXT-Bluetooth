// ##### extgen :: Auto-generated file do not edit!! #####

// #####################################################################
// # Macros
// #####################################################################

// #####################################################################
// # Enums
// #####################################################################

enum BluetoothError
{
    Ok = 0,
    Unknown = 1,
    NotSupported = 2,
    NotInitialized = 3,
    BluetoothDisabled = 4,
    PermissionDenied = 5,
    InvalidArgument = 6,
    InvalidHandle = 7,
    Busy = 8,
    Timeout = 9,
    NotFound = 10,
    ConnectionFailed = 11,
    Disconnected = 12,
    OperationFailed = 13
}

enum BluetoothTransport
{
    Unknown = 0,
    Classic = 1,
    LowEnergy = 2
}

enum BluetoothPermissionStatus
{
    Unknown = 0,
    Granted = 1,
    Denied = 2
}

// #####################################################################
// # Constructors
// #####################################################################

// #####################################################################
// # Codecs
// #####################################################################

// #####################################################################
// # Functions
// #####################################################################

// Skipping function bluetooth_initialize (no wrapper is required)


// Skipping function bluetooth_shutdown (no wrapper is required)


// Skipping function bluetooth_update (no wrapper is required)


// Skipping function bluetooth_is_initialized (no wrapper is required)


// Skipping function bluetooth_last_error_code (no wrapper is required)


// Skipping function bluetooth_last_error_message (no wrapper is required)


// Skipping function bluetooth_le_is_supported (no wrapper is required)


// Skipping function bluetooth_classic_is_supported (no wrapper is required)


// Skipping function bluetooth_classic_server_is_supported (no wrapper is required)


// Skipping function bluetooth_permission_get_status (no wrapper is required)


// Skipping function bluetooth_permission_request (no wrapper is required)


// Skipping function bluetooth_le_scan_start (no wrapper is required)


// Skipping function bluetooth_le_scan_stop (no wrapper is required)


// Skipping function bluetooth_le_scan_is_running (no wrapper is required)


// Skipping function bluetooth_classic_scan_start (no wrapper is required)


// Skipping function bluetooth_classic_scan_stop (no wrapper is required)


// Skipping function bluetooth_classic_scan_is_running (no wrapper is required)


// Skipping function bluetooth_device_clear (no wrapper is required)


// Skipping function bluetooth_device_get_count (no wrapper is required)


/**
 * @param {Real} _index
 * @returns {Real}
 */
function bluetooth_device_get_at(_index)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __ret_buffer__ = __ext_core_get_ret_buffer();

    var __return_value__ = __bluetooth_device_get_at(_index, buffer_get_address(__ret_buffer__), buffer_get_size(__ret_buffer__));

    var __result__ = undefined;
    __result__ = buffer_read(__ret_buffer__, buffer_u64);
    return __result__;
}

/**
 * @param {Real} _device
 * @returns {Bool}
 */
function bluetooth_device_is_valid(_device)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _device, type: UInt64
    if (!is_numeric(_device)) show_error($"{_GMFUNCTION_} :: _device expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _device);

    var __return_value__ = __bluetooth_device_is_valid(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _device
 * @returns {Real}
 */
function bluetooth_device_get_transport(_device)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _device, type: UInt64
    if (!is_numeric(_device)) show_error($"{_GMFUNCTION_} :: _device expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _device);

    var __return_value__ = __bluetooth_device_get_transport(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _device
 * @returns {String}
 */
function bluetooth_device_get_id(_device)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _device, type: UInt64
    if (!is_numeric(_device)) show_error($"{_GMFUNCTION_} :: _device expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _device);

    var __return_value__ = __bluetooth_device_get_id(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _device
 * @returns {String}
 */
function bluetooth_device_get_name(_device)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _device, type: UInt64
    if (!is_numeric(_device)) show_error($"{_GMFUNCTION_} :: _device expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _device);

    var __return_value__ = __bluetooth_device_get_name(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _device
 * @returns {Bool}
 */
function bluetooth_device_has_address(_device)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _device, type: UInt64
    if (!is_numeric(_device)) show_error($"{_GMFUNCTION_} :: _device expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _device);

    var __return_value__ = __bluetooth_device_has_address(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _device
 * @returns {String}
 */
function bluetooth_device_get_address(_device)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _device, type: UInt64
    if (!is_numeric(_device)) show_error($"{_GMFUNCTION_} :: _device expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _device);

    var __return_value__ = __bluetooth_device_get_address(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _device
 * @returns {Bool}
 */
function bluetooth_device_has_rssi(_device)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _device, type: UInt64
    if (!is_numeric(_device)) show_error($"{_GMFUNCTION_} :: _device expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _device);

    var __return_value__ = __bluetooth_device_has_rssi(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _device
 * @returns {Real}
 */
function bluetooth_device_get_rssi(_device)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _device, type: UInt64
    if (!is_numeric(_device)) show_error($"{_GMFUNCTION_} :: _device expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _device);

    var __return_value__ = __bluetooth_device_get_rssi(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _device
 * @returns {Bool}
 */
function bluetooth_device_is_connectable(_device)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _device, type: UInt64
    if (!is_numeric(_device)) show_error($"{_GMFUNCTION_} :: _device expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _device);

    var __return_value__ = __bluetooth_device_is_connectable(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _device
 * @param {String} _service_uuid
 * @param {Function} _callback
 * @returns {Real}
 */
function bluetooth_classic_connect(_device, _service_uuid, _callback)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __dispatcher__ = __GMBluetooth_get_dispatcher();

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _device, type: UInt64
    if (!is_numeric(_device)) show_error($"{_GMFUNCTION_} :: _device expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _device);

    // param: _service_uuid, type: String
    if (!is_string(_service_uuid)) show_error($"{_GMFUNCTION_} :: _service_uuid expected string", true);
    buffer_write(__args_buffer__, buffer_u32, string_byte_length(_service_uuid));
    buffer_write(__args_buffer__, buffer_string, _service_uuid);

    // param: _callback, type: Function
    if (!is_callable(_callback)) show_error($"{_GMFUNCTION_} :: _callback expected callable type", true);
    var _callback_handle = __ext_core_function_register(_callback, __dispatcher__);
    buffer_write(__args_buffer__, buffer_u64, _callback_handle);

    var __ret_buffer__ = __ext_core_get_ret_buffer();

    var __return_value__ = __bluetooth_classic_connect(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__), buffer_get_address(__ret_buffer__), buffer_get_size(__ret_buffer__));

    var __result__ = undefined;
    __result__ = buffer_read(__ret_buffer__, buffer_u64);
    return __result__;
}

/**
 * @param {Real} _connection
 * @returns {Real}
 */
function bluetooth_classic_disconnect(_connection)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _connection, type: UInt64
    if (!is_numeric(_connection)) show_error($"{_GMFUNCTION_} :: _connection expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _connection);

    var __return_value__ = __bluetooth_classic_disconnect(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _connection
 * @returns {Bool}
 */
function bluetooth_classic_connection_is_valid(_connection)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _connection, type: UInt64
    if (!is_numeric(_connection)) show_error($"{_GMFUNCTION_} :: _connection expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _connection);

    var __return_value__ = __bluetooth_classic_connection_is_valid(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _connection
 * @returns {Bool}
 */
function bluetooth_classic_connection_is_connected(_connection)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _connection, type: UInt64
    if (!is_numeric(_connection)) show_error($"{_GMFUNCTION_} :: _connection expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _connection);

    var __return_value__ = __bluetooth_classic_connection_is_connected(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _connection
 * @returns {Real}
 */
function bluetooth_classic_connection_get_device(_connection)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _connection, type: UInt64
    if (!is_numeric(_connection)) show_error($"{_GMFUNCTION_} :: _connection expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _connection);

    var __ret_buffer__ = __ext_core_get_ret_buffer();

    var __return_value__ = __bluetooth_classic_connection_get_device(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__), buffer_get_address(__ret_buffer__), buffer_get_size(__ret_buffer__));

    var __result__ = undefined;
    __result__ = buffer_read(__ret_buffer__, buffer_u64);
    return __result__;
}

/**
 * @param {Real} _connection
 * @returns {Real}
 */
function bluetooth_classic_receive_available(_connection)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _connection, type: UInt64
    if (!is_numeric(_connection)) show_error($"{_GMFUNCTION_} :: _connection expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _connection);

    var __return_value__ = __bluetooth_classic_receive_available(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _connection
 * @param {Id.Buffer} _data
 * @param {Real} _offset
 * @param {Real} _size
 * @returns {Real}
 */
function bluetooth_classic_send(_connection, _data, _offset, _size)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _connection, type: UInt64
    if (!is_numeric(_connection)) show_error($"{_GMFUNCTION_} :: _connection expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _connection);

    // param: _data, type: Buffer
    if (!buffer_exists(_data)) show_error($"{_GMFUNCTION_} :: _data expected Id.Buffer", true);
    __GMBluetooth_queue_buffer(buffer_get_address(_data), buffer_get_size(_data));

    // param: _offset, type: UInt32
    if (!is_numeric(_offset)) show_error($"{_GMFUNCTION_} :: _offset expected number", true);
    buffer_write(__args_buffer__, buffer_u32, _offset);

    // param: _size, type: UInt32
    if (!is_numeric(_size)) show_error($"{_GMFUNCTION_} :: _size expected number", true);
    buffer_write(__args_buffer__, buffer_u32, _size);

    var __return_value__ = __bluetooth_classic_send(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _connection
 * @param {Id.Buffer} _out_data
 * @param {Real} _offset
 * @param {Real} _max_size
 * @returns {Real}
 */
function bluetooth_classic_receive(_connection, _out_data, _offset, _max_size)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _connection, type: UInt64
    if (!is_numeric(_connection)) show_error($"{_GMFUNCTION_} :: _connection expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _connection);

    // param: _out_data, type: Buffer
    if (!buffer_exists(_out_data)) show_error($"{_GMFUNCTION_} :: _out_data expected Id.Buffer", true);
    __GMBluetooth_queue_buffer(buffer_get_address(_out_data), buffer_get_size(_out_data));

    // param: _offset, type: UInt32
    if (!is_numeric(_offset)) show_error($"{_GMFUNCTION_} :: _offset expected number", true);
    buffer_write(__args_buffer__, buffer_u32, _offset);

    // param: _max_size, type: UInt32
    if (!is_numeric(_max_size)) show_error($"{_GMFUNCTION_} :: _max_size expected number", true);
    buffer_write(__args_buffer__, buffer_u32, _max_size);

    var __return_value__ = __bluetooth_classic_receive(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

// Skipping function bluetooth_classic_server_start (no wrapper is required)


// Skipping function bluetooth_classic_server_stop (no wrapper is required)


// Skipping function bluetooth_classic_server_is_running (no wrapper is required)


/**
 * @param {Function} _callback
 * @returns {Bool}
 */
function bluetooth_set_callback_device_found(_callback)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __dispatcher__ = __GMBluetooth_get_dispatcher();

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _callback, type: Function
    if (!is_callable(_callback)) show_error($"{_GMFUNCTION_} :: _callback expected callable type", true);
    var _callback_handle = __ext_core_function_register(_callback, __dispatcher__);
    buffer_write(__args_buffer__, buffer_u64, _callback_handle);

    var __return_value__ = __bluetooth_set_callback_device_found(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

// Skipping function bluetooth_remove_callback_device_found (no wrapper is required)


/**
 * @param {Function} _callback
 * @returns {Bool}
 */
function bluetooth_set_callback_scan_stopped(_callback)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __dispatcher__ = __GMBluetooth_get_dispatcher();

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _callback, type: Function
    if (!is_callable(_callback)) show_error($"{_GMFUNCTION_} :: _callback expected callable type", true);
    var _callback_handle = __ext_core_function_register(_callback, __dispatcher__);
    buffer_write(__args_buffer__, buffer_u64, _callback_handle);

    var __return_value__ = __bluetooth_set_callback_scan_stopped(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

// Skipping function bluetooth_remove_callback_scan_stopped (no wrapper is required)


/**
 * @param {Function} _callback
 * @returns {Bool}
 */
function bluetooth_set_callback_classic_client_connected(_callback)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __dispatcher__ = __GMBluetooth_get_dispatcher();

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _callback, type: Function
    if (!is_callable(_callback)) show_error($"{_GMFUNCTION_} :: _callback expected callable type", true);
    var _callback_handle = __ext_core_function_register(_callback, __dispatcher__);
    buffer_write(__args_buffer__, buffer_u64, _callback_handle);

    var __return_value__ = __bluetooth_set_callback_classic_client_connected(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

// Skipping function bluetooth_remove_callback_classic_client_connected (no wrapper is required)


/**
 * @param {Function} _callback
 * @returns {Bool}
 */
function bluetooth_set_callback_classic_data(_callback)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __dispatcher__ = __GMBluetooth_get_dispatcher();

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _callback, type: Function
    if (!is_callable(_callback)) show_error($"{_GMFUNCTION_} :: _callback expected callable type", true);
    var _callback_handle = __ext_core_function_register(_callback, __dispatcher__);
    buffer_write(__args_buffer__, buffer_u64, _callback_handle);

    var __return_value__ = __bluetooth_set_callback_classic_data(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

// Skipping function bluetooth_remove_callback_classic_data (no wrapper is required)


/**
 * @param {Function} _callback
 * @returns {Bool}
 */
function bluetooth_set_callback_classic_disconnected(_callback)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __dispatcher__ = __GMBluetooth_get_dispatcher();

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _callback, type: Function
    if (!is_callable(_callback)) show_error($"{_GMFUNCTION_} :: _callback expected callable type", true);
    var _callback_handle = __ext_core_function_register(_callback, __dispatcher__);
    buffer_write(__args_buffer__, buffer_u64, _callback_handle);

    var __return_value__ = __bluetooth_set_callback_classic_disconnected(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

// Skipping function bluetooth_remove_callback_classic_disconnected (no wrapper is required)


/// @ignore
function __GMBluetooth_get_decoders()
{
    static __decoders__ = [];
    return __decoders__;
}
/// @ignore
function __GMBluetooth_get_dispatcher()
{
    static __dispatcher__ = new __GMNativeFunctionDispatcher(__GMBluetooth_invocation_handler, __GMBluetooth_get_decoders());
    return __dispatcher__;
}
/// @ignore
function __GMBluetooth_is_available()
{
    static __available__ = extension_exists("GMBluetooth");
    return __available__;
}
