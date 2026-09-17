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

enum BluetoothLeSubscribeMode
{
    Unsubscribe = 0,
    Notify = 1,
    Indicate = 2
}

enum BluetoothLeCharacteristicProperty
{
    None = 0,
    Broadcast = 1,
    Read = 2,
    WriteWithoutResponse = 4,
    Write = 8,
    Notify = 16,
    Indicate = 32,
    AuthenticatedSignedWrites = 64,
    ExtendedProperties = 128
}

enum BluetoothState
{
    Unknown = 0,
    Resetting = 1,
    Unsupported = 2,
    Unauthorized = 3,
    PoweredOff = 4,
    PoweredOn = 5
}

// #####################################################################
// # Constructors
// #####################################################################

/**
 * @returns {Struct.BluetoothLeDescriptorDefinition}
 */
function BluetoothLeDescriptorDefinition() constructor
{
    /**
     * Internally generated hash for quick validation
     * @ignore
     */
    static __uid = 976365866;

    self.uuid = undefined;

}

/**
 * @returns {Struct.BluetoothLeCharacteristicDefinition}
 */
function BluetoothLeCharacteristicDefinition() constructor
{
    /**
     * Internally generated hash for quick validation
     * @ignore
     */
    static __uid = 25225386;

    self.uuid = undefined;
    self.properties = undefined;
    self.permissions = undefined;
    self.value = undefined;
    self.descriptors = undefined;

}

/**
 * @returns {Struct.BluetoothLeServiceDefinition}
 */
function BluetoothLeServiceDefinition() constructor
{
    /**
     * Internally generated hash for quick validation
     * @ignore
     */
    static __uid = 2776316700;

    self.uuid = undefined;
    self.characteristics = undefined;

}

// #####################################################################
// # Codecs
// #####################################################################

/**
 * @func __BluetoothLeDescriptorDefinition_encode(_inst, _buffer, _offset, _where)
 * @param {Struct.BluetoothLeDescriptorDefinition} _inst
 * @param {Id.Buffer} _buffer
 * @param {Real} _offset
 * @param {String} _where
 * @ignore
 */
function __BluetoothLeDescriptorDefinition_encode(_inst, _buffer, _offset, _where = _GMFUNCTION_)
{
    buffer_seek(_buffer, buffer_seek_start, _offset);
    with (_inst)
    {
        // field: uuid, type: String
        if (!is_string(self.uuid)) show_error($"{_where} :: self.uuid expected string", true);
        buffer_write(_buffer, buffer_u32, string_byte_length(self.uuid));
        buffer_write(_buffer, buffer_string, self.uuid);

    }
}

/**
 * @func __BluetoothLeDescriptorDefinition_decode(_buffer, _offset)
 * @param {Id.Buffer} _buffer
 * @param {Real} _offset
 * @returns {Struct.BluetoothLeDescriptorDefinition}
 * @ignore
 */
function __BluetoothLeDescriptorDefinition_decode(_buffer, _offset)
{
    buffer_seek(_buffer, buffer_seek_start, _offset);

    _inst = new BluetoothLeDescriptorDefinition();
    with (_inst)
    {
        // field: uuid, type: String
        buffer_read(_buffer, buffer_u32);
        self.uuid = buffer_read(_buffer, buffer_string);

    }

    return _inst;
}

/**
 * @func __BluetoothLeCharacteristicDefinition_encode(_inst, _buffer, _offset, _where)
 * @param {Struct.BluetoothLeCharacteristicDefinition} _inst
 * @param {Id.Buffer} _buffer
 * @param {Real} _offset
 * @param {String} _where
 * @ignore
 */
function __BluetoothLeCharacteristicDefinition_encode(_inst, _buffer, _offset, _where = _GMFUNCTION_)
{
    buffer_seek(_buffer, buffer_seek_start, _offset);
    with (_inst)
    {
        // field: uuid, type: String
        if (!is_string(self.uuid)) show_error($"{_where} :: self.uuid expected string", true);
        buffer_write(_buffer, buffer_u32, string_byte_length(self.uuid));
        buffer_write(_buffer, buffer_string, self.uuid);

        // field: properties, type: Int32
        if (!is_numeric(self.properties)) show_error($"{_where} :: self.properties expected number", true);
        buffer_write(_buffer, buffer_s32, self.properties);

        // field: permissions, type: Int32
        if (!is_numeric(self.permissions)) show_error($"{_where} :: self.permissions expected number", true);
        buffer_write(_buffer, buffer_s32, self.permissions);

        // field: value, type: optional<String>
        if (is_undefined(self.value))
        {
            buffer_write(_buffer, buffer_bool, false);
        }
        else
        {
            buffer_write(_buffer, buffer_bool, true);
            if (!is_string(self.value)) show_error($"{_where} :: self.value expected string", true);
            buffer_write(_buffer, buffer_u32, string_byte_length(self.value));
            buffer_write(_buffer, buffer_string, self.value);
        }

        // field: descriptors, type: struct BluetoothLeDescriptorDefinition[]
        if (!is_array(self.descriptors)) show_error($"{_where} :: self.descriptors expected array", true);
        var __length__ = array_length(self.descriptors);
        buffer_write(_buffer, buffer_u32, __length__);
        for (var _i = 0; _i < __length__; ++_i)
        {
            if (self.descriptors[_i].__uid != 976365866) show_error($"{_where} :: self.descriptors[_i] expected BluetoothLeDescriptorDefinition", true);
            __BluetoothLeDescriptorDefinition_encode(self.descriptors[_i], _buffer, buffer_tell(_buffer), _where);
        }

    }
}

/**
 * @func __BluetoothLeCharacteristicDefinition_decode(_buffer, _offset)
 * @param {Id.Buffer} _buffer
 * @param {Real} _offset
 * @returns {Struct.BluetoothLeCharacteristicDefinition}
 * @ignore
 */
function __BluetoothLeCharacteristicDefinition_decode(_buffer, _offset)
{
    buffer_seek(_buffer, buffer_seek_start, _offset);

    _inst = new BluetoothLeCharacteristicDefinition();
    with (_inst)
    {
        // field: uuid, type: String
        buffer_read(_buffer, buffer_u32);
        self.uuid = buffer_read(_buffer, buffer_string);

        // field: properties, type: Int32
        self.properties = buffer_read(_buffer, buffer_s32);

        // field: permissions, type: Int32
        self.permissions = buffer_read(_buffer, buffer_s32);

        // field: value, type: optional<String>
        if (buffer_read(_buffer, buffer_bool))
        {
            buffer_read(_buffer, buffer_u32);
            self.value = buffer_read(_buffer, buffer_string);
        }
        else
        {
            self.value = undefined;
        }

        // field: descriptors, type: struct BluetoothLeDescriptorDefinition[]
        var __length__ = buffer_read(_buffer, buffer_u32);
        self.descriptors = array_create(__length__);
        for (var _i = 0; _i < __length__; ++_i)
        {
            self.descriptors[_i] = __BluetoothLeDescriptorDefinition_decode(_buffer, buffer_tell(_buffer));
        }

    }

    return _inst;
}

/**
 * @func __BluetoothLeServiceDefinition_encode(_inst, _buffer, _offset, _where)
 * @param {Struct.BluetoothLeServiceDefinition} _inst
 * @param {Id.Buffer} _buffer
 * @param {Real} _offset
 * @param {String} _where
 * @ignore
 */
function __BluetoothLeServiceDefinition_encode(_inst, _buffer, _offset, _where = _GMFUNCTION_)
{
    buffer_seek(_buffer, buffer_seek_start, _offset);
    with (_inst)
    {
        // field: uuid, type: String
        if (!is_string(self.uuid)) show_error($"{_where} :: self.uuid expected string", true);
        buffer_write(_buffer, buffer_u32, string_byte_length(self.uuid));
        buffer_write(_buffer, buffer_string, self.uuid);

        // field: characteristics, type: struct BluetoothLeCharacteristicDefinition[]
        if (!is_array(self.characteristics)) show_error($"{_where} :: self.characteristics expected array", true);
        var __length__ = array_length(self.characteristics);
        buffer_write(_buffer, buffer_u32, __length__);
        for (var _i = 0; _i < __length__; ++_i)
        {
            if (self.characteristics[_i].__uid != 25225386) show_error($"{_where} :: self.characteristics[_i] expected BluetoothLeCharacteristicDefinition", true);
            __BluetoothLeCharacteristicDefinition_encode(self.characteristics[_i], _buffer, buffer_tell(_buffer), _where);
        }

    }
}

/**
 * @func __BluetoothLeServiceDefinition_decode(_buffer, _offset)
 * @param {Id.Buffer} _buffer
 * @param {Real} _offset
 * @returns {Struct.BluetoothLeServiceDefinition}
 * @ignore
 */
function __BluetoothLeServiceDefinition_decode(_buffer, _offset)
{
    buffer_seek(_buffer, buffer_seek_start, _offset);

    _inst = new BluetoothLeServiceDefinition();
    with (_inst)
    {
        // field: uuid, type: String
        buffer_read(_buffer, buffer_u32);
        self.uuid = buffer_read(_buffer, buffer_string);

        // field: characteristics, type: struct BluetoothLeCharacteristicDefinition[]
        var __length__ = buffer_read(_buffer, buffer_u32);
        self.characteristics = array_create(__length__);
        for (var _i = 0; _i < __length__; ++_i)
        {
            self.characteristics[_i] = __BluetoothLeCharacteristicDefinition_decode(_buffer, buffer_tell(_buffer));
        }

    }

    return _inst;
}

// #####################################################################
// # Functions
// #####################################################################

// Skipping function bluetooth_initialize (no wrapper is required)


// Skipping function bluetooth_shutdown (no wrapper is required)


// Skipping function bluetooth_is_initialized (no wrapper is required)


// Skipping function bluetooth_last_error_code (no wrapper is required)


// Skipping function bluetooth_last_error_message (no wrapper is required)


// Skipping function bluetooth_le_is_supported (no wrapper is required)


// Skipping function bluetooth_le_advertise_is_supported (no wrapper is required)


// Skipping function bluetooth_le_server_is_supported (no wrapper is required)


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


// Skipping function bluetooth_classic_discoverable_start (no wrapper is required)


// Skipping function bluetooth_classic_discoverable_stop (no wrapper is required)


// Skipping function bluetooth_classic_discoverable_is_running (no wrapper is required)


/**
 * @param {Real} _device
 * @returns {Bool}
 */
function bluetooth_pairing_is_supported(_device)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _device, type: UInt64
    if (!is_numeric(_device)) show_error($"{_GMFUNCTION_} :: _device expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _device);

    var __return_value__ = __bluetooth_pairing_is_supported(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _device
 * @param {Function} _callback
 * @returns {Real}
 */
function bluetooth_pair(_device, _callback)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __dispatcher__ = __GMBluetooth_get_dispatcher();

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _device, type: UInt64
    if (!is_numeric(_device)) show_error($"{_GMFUNCTION_} :: _device expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _device);

    // param: _callback, type: Function
    if (!is_callable(_callback)) show_error($"{_GMFUNCTION_} :: _callback expected callable type", true);
    var _callback_handle = __ext_core_function_register(_callback, __dispatcher__);
    buffer_write(__args_buffer__, buffer_u64, _callback_handle);

    var __return_value__ = __bluetooth_pair(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _device
 * @returns {Bool}
 */
function bluetooth_device_is_paired(_device)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _device, type: UInt64
    if (!is_numeric(_device)) show_error($"{_GMFUNCTION_} :: _device expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _device);

    var __return_value__ = __bluetooth_device_is_paired(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _device
 * @param {Function} _callback
 * @returns {Real}
 */
function bluetooth_le_connect(_device, _callback)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __dispatcher__ = __GMBluetooth_get_dispatcher();

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _device, type: UInt64
    if (!is_numeric(_device)) show_error($"{_GMFUNCTION_} :: _device expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _device);

    // param: _callback, type: Function
    if (!is_callable(_callback)) show_error($"{_GMFUNCTION_} :: _callback expected callable type", true);
    var _callback_handle = __ext_core_function_register(_callback, __dispatcher__);
    buffer_write(__args_buffer__, buffer_u64, _callback_handle);

    var __ret_buffer__ = __ext_core_get_ret_buffer();

    var __return_value__ = __bluetooth_le_connect(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__), buffer_get_address(__ret_buffer__), buffer_get_size(__ret_buffer__));

    var __result__ = undefined;
    __result__ = buffer_read(__ret_buffer__, buffer_u64);
    return __result__;
}

/**
 * @param {Real} _connection
 * @returns {Real}
 */
function bluetooth_le_disconnect(_connection)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _connection, type: UInt64
    if (!is_numeric(_connection)) show_error($"{_GMFUNCTION_} :: _connection expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _connection);

    var __return_value__ = __bluetooth_le_disconnect(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _connection
 * @returns {Bool}
 */
function bluetooth_le_connection_is_valid(_connection)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _connection, type: UInt64
    if (!is_numeric(_connection)) show_error($"{_GMFUNCTION_} :: _connection expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _connection);

    var __return_value__ = __bluetooth_le_connection_is_valid(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _connection
 * @returns {Bool}
 */
function bluetooth_le_connection_is_connected(_connection)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _connection, type: UInt64
    if (!is_numeric(_connection)) show_error($"{_GMFUNCTION_} :: _connection expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _connection);

    var __return_value__ = __bluetooth_le_connection_is_connected(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _connection
 * @returns {Real}
 */
function bluetooth_le_connection_get_device(_connection)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _connection, type: UInt64
    if (!is_numeric(_connection)) show_error($"{_GMFUNCTION_} :: _connection expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _connection);

    var __ret_buffer__ = __ext_core_get_ret_buffer();

    var __return_value__ = __bluetooth_le_connection_get_device(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__), buffer_get_address(__ret_buffer__), buffer_get_size(__ret_buffer__));

    var __result__ = undefined;
    __result__ = buffer_read(__ret_buffer__, buffer_u64);
    return __result__;
}

/**
 * @param {Real} _connection
 * @param {Function} _callback
 * @returns {Real}
 */
function bluetooth_le_services_discover(_connection, _callback)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __dispatcher__ = __GMBluetooth_get_dispatcher();

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _connection, type: UInt64
    if (!is_numeric(_connection)) show_error($"{_GMFUNCTION_} :: _connection expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _connection);

    // param: _callback, type: Function
    if (!is_callable(_callback)) show_error($"{_GMFUNCTION_} :: _callback expected callable type", true);
    var _callback_handle = __ext_core_function_register(_callback, __dispatcher__);
    buffer_write(__args_buffer__, buffer_u64, _callback_handle);

    var __return_value__ = __bluetooth_le_services_discover(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _connection
 * @returns {Real}
 */
function bluetooth_le_service_get_count(_connection)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _connection, type: UInt64
    if (!is_numeric(_connection)) show_error($"{_GMFUNCTION_} :: _connection expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _connection);

    var __return_value__ = __bluetooth_le_service_get_count(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _connection
 * @param {Real} _index
 * @returns {Real}
 */
function bluetooth_le_service_get_at(_connection, _index)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _connection, type: UInt64
    if (!is_numeric(_connection)) show_error($"{_GMFUNCTION_} :: _connection expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _connection);

    // param: _index, type: Int32
    if (!is_numeric(_index)) show_error($"{_GMFUNCTION_} :: _index expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _index);

    var __ret_buffer__ = __ext_core_get_ret_buffer();

    var __return_value__ = __bluetooth_le_service_get_at(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__), buffer_get_address(__ret_buffer__), buffer_get_size(__ret_buffer__));

    var __result__ = undefined;
    __result__ = buffer_read(__ret_buffer__, buffer_u64);
    return __result__;
}

/**
 * @param {Real} _service
 * @returns {String}
 */
function bluetooth_le_service_get_uuid(_service)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _service, type: UInt64
    if (!is_numeric(_service)) show_error($"{_GMFUNCTION_} :: _service expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _service);

    var __return_value__ = __bluetooth_le_service_get_uuid(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _service
 * @param {Function} _callback
 * @returns {Real}
 */
function bluetooth_le_characteristics_discover(_service, _callback)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __dispatcher__ = __GMBluetooth_get_dispatcher();

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _service, type: UInt64
    if (!is_numeric(_service)) show_error($"{_GMFUNCTION_} :: _service expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _service);

    // param: _callback, type: Function
    if (!is_callable(_callback)) show_error($"{_GMFUNCTION_} :: _callback expected callable type", true);
    var _callback_handle = __ext_core_function_register(_callback, __dispatcher__);
    buffer_write(__args_buffer__, buffer_u64, _callback_handle);

    var __return_value__ = __bluetooth_le_characteristics_discover(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _service
 * @returns {Real}
 */
function bluetooth_le_characteristic_get_count(_service)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _service, type: UInt64
    if (!is_numeric(_service)) show_error($"{_GMFUNCTION_} :: _service expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _service);

    var __return_value__ = __bluetooth_le_characteristic_get_count(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _service
 * @param {Real} _index
 * @returns {Real}
 */
function bluetooth_le_characteristic_get_at(_service, _index)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _service, type: UInt64
    if (!is_numeric(_service)) show_error($"{_GMFUNCTION_} :: _service expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _service);

    // param: _index, type: Int32
    if (!is_numeric(_index)) show_error($"{_GMFUNCTION_} :: _index expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _index);

    var __ret_buffer__ = __ext_core_get_ret_buffer();

    var __return_value__ = __bluetooth_le_characteristic_get_at(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__), buffer_get_address(__ret_buffer__), buffer_get_size(__ret_buffer__));

    var __result__ = undefined;
    __result__ = buffer_read(__ret_buffer__, buffer_u64);
    return __result__;
}

/**
 * @param {Real} _characteristic
 * @returns {String}
 */
function bluetooth_le_characteristic_get_uuid(_characteristic)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _characteristic, type: UInt64
    if (!is_numeric(_characteristic)) show_error($"{_GMFUNCTION_} :: _characteristic expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _characteristic);

    var __return_value__ = __bluetooth_le_characteristic_get_uuid(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _characteristic
 * @returns {Real}
 */
function bluetooth_le_characteristic_get_properties(_characteristic)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _characteristic, type: UInt64
    if (!is_numeric(_characteristic)) show_error($"{_GMFUNCTION_} :: _characteristic expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _characteristic);

    var __return_value__ = __bluetooth_le_characteristic_get_properties(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _characteristic
 * @param {Function} _callback
 * @returns {Real}
 */
function bluetooth_le_descriptors_discover(_characteristic, _callback)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __dispatcher__ = __GMBluetooth_get_dispatcher();

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _characteristic, type: UInt64
    if (!is_numeric(_characteristic)) show_error($"{_GMFUNCTION_} :: _characteristic expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _characteristic);

    // param: _callback, type: Function
    if (!is_callable(_callback)) show_error($"{_GMFUNCTION_} :: _callback expected callable type", true);
    var _callback_handle = __ext_core_function_register(_callback, __dispatcher__);
    buffer_write(__args_buffer__, buffer_u64, _callback_handle);

    var __return_value__ = __bluetooth_le_descriptors_discover(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _characteristic
 * @returns {Real}
 */
function bluetooth_le_descriptor_get_count(_characteristic)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _characteristic, type: UInt64
    if (!is_numeric(_characteristic)) show_error($"{_GMFUNCTION_} :: _characteristic expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _characteristic);

    var __return_value__ = __bluetooth_le_descriptor_get_count(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _characteristic
 * @param {Real} _index
 * @returns {Real}
 */
function bluetooth_le_descriptor_get_at(_characteristic, _index)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _characteristic, type: UInt64
    if (!is_numeric(_characteristic)) show_error($"{_GMFUNCTION_} :: _characteristic expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _characteristic);

    // param: _index, type: Int32
    if (!is_numeric(_index)) show_error($"{_GMFUNCTION_} :: _index expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _index);

    var __ret_buffer__ = __ext_core_get_ret_buffer();

    var __return_value__ = __bluetooth_le_descriptor_get_at(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__), buffer_get_address(__ret_buffer__), buffer_get_size(__ret_buffer__));

    var __result__ = undefined;
    __result__ = buffer_read(__ret_buffer__, buffer_u64);
    return __result__;
}

/**
 * @param {Real} _descriptor
 * @returns {String}
 */
function bluetooth_le_descriptor_get_uuid(_descriptor)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _descriptor, type: UInt64
    if (!is_numeric(_descriptor)) show_error($"{_GMFUNCTION_} :: _descriptor expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _descriptor);

    var __return_value__ = __bluetooth_le_descriptor_get_uuid(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _characteristic
 * @param {Function} _callback
 * @returns {Real}
 */
function bluetooth_le_characteristic_read(_characteristic, _callback)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __dispatcher__ = __GMBluetooth_get_dispatcher();

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _characteristic, type: UInt64
    if (!is_numeric(_characteristic)) show_error($"{_GMFUNCTION_} :: _characteristic expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _characteristic);

    // param: _callback, type: Function
    if (!is_callable(_callback)) show_error($"{_GMFUNCTION_} :: _callback expected callable type", true);
    var _callback_handle = __ext_core_function_register(_callback, __dispatcher__);
    buffer_write(__args_buffer__, buffer_u64, _callback_handle);

    var __return_value__ = __bluetooth_le_characteristic_read(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _characteristic
 * @param {Id.Buffer} _out_data
 * @param {Real} _offset
 * @param {Real} _max_size
 * @returns {Real}
 */
function bluetooth_le_characteristic_get_value(_characteristic, _out_data, _offset, _max_size)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _characteristic, type: UInt64
    if (!is_numeric(_characteristic)) show_error($"{_GMFUNCTION_} :: _characteristic expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _characteristic);

    // param: _out_data, type: Buffer
    if (!buffer_exists(_out_data)) show_error($"{_GMFUNCTION_} :: _out_data expected Id.Buffer", true);
    __GMBluetooth_queue_buffer(buffer_get_address(_out_data), buffer_get_size(_out_data));

    // param: _offset, type: UInt32
    if (!is_numeric(_offset)) show_error($"{_GMFUNCTION_} :: _offset expected number", true);
    buffer_write(__args_buffer__, buffer_u32, _offset);

    // param: _max_size, type: UInt32
    if (!is_numeric(_max_size)) show_error($"{_GMFUNCTION_} :: _max_size expected number", true);
    buffer_write(__args_buffer__, buffer_u32, _max_size);

    var __return_value__ = __bluetooth_le_characteristic_get_value(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _characteristic
 * @param {Id.Buffer} _data
 * @param {Real} _offset
 * @param {Real} _size
 * @param {Real} _write_type
 * @param {Function} _callback
 * @returns {Real}
 */
function bluetooth_le_characteristic_write(_characteristic, _data, _offset, _size, _write_type, _callback)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __dispatcher__ = __GMBluetooth_get_dispatcher();

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _characteristic, type: UInt64
    if (!is_numeric(_characteristic)) show_error($"{_GMFUNCTION_} :: _characteristic expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _characteristic);

    // param: _data, type: Buffer
    if (!buffer_exists(_data)) show_error($"{_GMFUNCTION_} :: _data expected Id.Buffer", true);
    __GMBluetooth_queue_buffer(buffer_get_address(_data), buffer_get_size(_data));

    // param: _offset, type: UInt32
    if (!is_numeric(_offset)) show_error($"{_GMFUNCTION_} :: _offset expected number", true);
    buffer_write(__args_buffer__, buffer_u32, _offset);

    // param: _size, type: UInt32
    if (!is_numeric(_size)) show_error($"{_GMFUNCTION_} :: _size expected number", true);
    buffer_write(__args_buffer__, buffer_u32, _size);

    // param: _write_type, type: Int32
    if (!is_numeric(_write_type)) show_error($"{_GMFUNCTION_} :: _write_type expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _write_type);

    // param: _callback, type: Function
    if (!is_callable(_callback)) show_error($"{_GMFUNCTION_} :: _callback expected callable type", true);
    var _callback_handle = __ext_core_function_register(_callback, __dispatcher__);
    buffer_write(__args_buffer__, buffer_u64, _callback_handle);

    var __return_value__ = __bluetooth_le_characteristic_write(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _characteristic
 * @param {Real} _mode
 * @param {Function} _callback
 * @returns {Real}
 */
function bluetooth_le_characteristic_subscribe(_characteristic, _mode, _callback)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __dispatcher__ = __GMBluetooth_get_dispatcher();

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _characteristic, type: UInt64
    if (!is_numeric(_characteristic)) show_error($"{_GMFUNCTION_} :: _characteristic expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _characteristic);

    // param: _mode, type: Int32
    if (!is_numeric(_mode)) show_error($"{_GMFUNCTION_} :: _mode expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _mode);

    // param: _callback, type: Function
    if (!is_callable(_callback)) show_error($"{_GMFUNCTION_} :: _callback expected callable type", true);
    var _callback_handle = __ext_core_function_register(_callback, __dispatcher__);
    buffer_write(__args_buffer__, buffer_u64, _callback_handle);

    var __return_value__ = __bluetooth_le_characteristic_subscribe(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _descriptor
 * @param {Function} _callback
 * @returns {Real}
 */
function bluetooth_le_descriptor_read(_descriptor, _callback)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __dispatcher__ = __GMBluetooth_get_dispatcher();

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _descriptor, type: UInt64
    if (!is_numeric(_descriptor)) show_error($"{_GMFUNCTION_} :: _descriptor expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _descriptor);

    // param: _callback, type: Function
    if (!is_callable(_callback)) show_error($"{_GMFUNCTION_} :: _callback expected callable type", true);
    var _callback_handle = __ext_core_function_register(_callback, __dispatcher__);
    buffer_write(__args_buffer__, buffer_u64, _callback_handle);

    var __return_value__ = __bluetooth_le_descriptor_read(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _descriptor
 * @param {Id.Buffer} _out_data
 * @param {Real} _offset
 * @param {Real} _max_size
 * @returns {Real}
 */
function bluetooth_le_descriptor_get_value(_descriptor, _out_data, _offset, _max_size)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _descriptor, type: UInt64
    if (!is_numeric(_descriptor)) show_error($"{_GMFUNCTION_} :: _descriptor expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _descriptor);

    // param: _out_data, type: Buffer
    if (!buffer_exists(_out_data)) show_error($"{_GMFUNCTION_} :: _out_data expected Id.Buffer", true);
    __GMBluetooth_queue_buffer(buffer_get_address(_out_data), buffer_get_size(_out_data));

    // param: _offset, type: UInt32
    if (!is_numeric(_offset)) show_error($"{_GMFUNCTION_} :: _offset expected number", true);
    buffer_write(__args_buffer__, buffer_u32, _offset);

    // param: _max_size, type: UInt32
    if (!is_numeric(_max_size)) show_error($"{_GMFUNCTION_} :: _max_size expected number", true);
    buffer_write(__args_buffer__, buffer_u32, _max_size);

    var __return_value__ = __bluetooth_le_descriptor_get_value(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Real} _descriptor
 * @param {Id.Buffer} _data
 * @param {Real} _offset
 * @param {Real} _size
 * @param {Function} _callback
 * @returns {Real}
 */
function bluetooth_le_descriptor_write(_descriptor, _data, _offset, _size, _callback)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __dispatcher__ = __GMBluetooth_get_dispatcher();

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _descriptor, type: UInt64
    if (!is_numeric(_descriptor)) show_error($"{_GMFUNCTION_} :: _descriptor expected number", true);
    buffer_write(__args_buffer__, buffer_u64, _descriptor);

    // param: _data, type: Buffer
    if (!buffer_exists(_data)) show_error($"{_GMFUNCTION_} :: _data expected Id.Buffer", true);
    __GMBluetooth_queue_buffer(buffer_get_address(_data), buffer_get_size(_data));

    // param: _offset, type: UInt32
    if (!is_numeric(_offset)) show_error($"{_GMFUNCTION_} :: _offset expected number", true);
    buffer_write(__args_buffer__, buffer_u32, _offset);

    // param: _size, type: UInt32
    if (!is_numeric(_size)) show_error($"{_GMFUNCTION_} :: _size expected number", true);
    buffer_write(__args_buffer__, buffer_u32, _size);

    // param: _callback, type: Function
    if (!is_callable(_callback)) show_error($"{_GMFUNCTION_} :: _callback expected callable type", true);
    var _callback_handle = __ext_core_function_register(_callback, __dispatcher__);
    buffer_write(__args_buffer__, buffer_u64, _callback_handle);

    var __return_value__ = __bluetooth_le_descriptor_write(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {String} _settings_json
 * @param {String} _data_json
 * @param {Function} _callback
 * @returns {Real}
 */
function bluetooth_le_advertise_start(_settings_json, _data_json, _callback)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __dispatcher__ = __GMBluetooth_get_dispatcher();

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _settings_json, type: String
    if (!is_string(_settings_json)) show_error($"{_GMFUNCTION_} :: _settings_json expected string", true);
    buffer_write(__args_buffer__, buffer_u32, string_byte_length(_settings_json));
    buffer_write(__args_buffer__, buffer_string, _settings_json);

    // param: _data_json, type: String
    if (!is_string(_data_json)) show_error($"{_GMFUNCTION_} :: _data_json expected string", true);
    buffer_write(__args_buffer__, buffer_u32, string_byte_length(_data_json));
    buffer_write(__args_buffer__, buffer_string, _data_json);

    // param: _callback, type: Function
    if (!is_callable(_callback)) show_error($"{_GMFUNCTION_} :: _callback expected callable type", true);
    var _callback_handle = __ext_core_function_register(_callback, __dispatcher__);
    buffer_write(__args_buffer__, buffer_u64, _callback_handle);

    var __return_value__ = __bluetooth_le_advertise_start(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

// Skipping function bluetooth_le_advertise_stop (no wrapper is required)


// Skipping function bluetooth_le_advertise_is_running (no wrapper is required)


// Skipping function bluetooth_le_server_start (no wrapper is required)


// Skipping function bluetooth_le_server_stop (no wrapper is required)


// Skipping function bluetooth_le_server_is_running (no wrapper is required)


/**
 * @param {Struct.BluetoothLeServiceDefinition} _service
 * @param {Function} _callback
 * @returns {Real}
 */
function bluetooth_le_server_add_service(_service, _callback)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __dispatcher__ = __GMBluetooth_get_dispatcher();

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _service, type: struct BluetoothLeServiceDefinition
    if (_service.__uid != 2776316700) show_error($"{_GMFUNCTION_} :: _service expected BluetoothLeServiceDefinition", true);
    __BluetoothLeServiceDefinition_encode(_service, __args_buffer__, buffer_tell(__args_buffer__), _GMFUNCTION_);

    // param: _callback, type: Function
    if (!is_callable(_callback)) show_error($"{_GMFUNCTION_} :: _callback expected callable type", true);
    var _callback_handle = __ext_core_function_register(_callback, __dispatcher__);
    buffer_write(__args_buffer__, buffer_u64, _callback_handle);

    var __return_value__ = __bluetooth_le_server_add_service(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

// Skipping function bluetooth_le_server_clear_services (no wrapper is required)


/**
 * @param {Real} _request_id
 * @param {Real} _error_code
 * @param {Id.Buffer} _data
 * @param {Real} _offset
 * @param {Real} _size
 * @returns {Real}
 */
function bluetooth_le_server_respond_read(_request_id, _error_code, _data, _offset, _size)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _request_id, type: Int32
    if (!is_numeric(_request_id)) show_error($"{_GMFUNCTION_} :: _request_id expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _request_id);

    // param: _error_code, type: Int32
    if (!is_numeric(_error_code)) show_error($"{_GMFUNCTION_} :: _error_code expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _error_code);

    // param: _data, type: Buffer
    if (!buffer_exists(_data)) show_error($"{_GMFUNCTION_} :: _data expected Id.Buffer", true);
    __GMBluetooth_queue_buffer(buffer_get_address(_data), buffer_get_size(_data));

    // param: _offset, type: UInt32
    if (!is_numeric(_offset)) show_error($"{_GMFUNCTION_} :: _offset expected number", true);
    buffer_write(__args_buffer__, buffer_u32, _offset);

    // param: _size, type: UInt32
    if (!is_numeric(_size)) show_error($"{_GMFUNCTION_} :: _size expected number", true);
    buffer_write(__args_buffer__, buffer_u32, _size);

    var __return_value__ = __bluetooth_le_server_respond_read(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

// Skipping function bluetooth_le_server_respond_write (no wrapper is required)


/**
 * @param {Real} _request_id
 * @param {Id.Buffer} _out_data
 * @param {Real} _offset
 * @param {Real} _max_size
 * @returns {Real}
 */
function bluetooth_le_server_write_request_get_value(_request_id, _out_data, _offset, _max_size)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _request_id, type: Int32
    if (!is_numeric(_request_id)) show_error($"{_GMFUNCTION_} :: _request_id expected number", true);
    buffer_write(__args_buffer__, buffer_s32, _request_id);

    // param: _out_data, type: Buffer
    if (!buffer_exists(_out_data)) show_error($"{_GMFUNCTION_} :: _out_data expected Id.Buffer", true);
    __GMBluetooth_queue_buffer(buffer_get_address(_out_data), buffer_get_size(_out_data));

    // param: _offset, type: UInt32
    if (!is_numeric(_offset)) show_error($"{_GMFUNCTION_} :: _offset expected number", true);
    buffer_write(__args_buffer__, buffer_u32, _offset);

    // param: _max_size, type: UInt32
    if (!is_numeric(_max_size)) show_error($"{_GMFUNCTION_} :: _max_size expected number", true);
    buffer_write(__args_buffer__, buffer_u32, _max_size);

    var __return_value__ = __bluetooth_le_server_write_request_get_value(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {String} _service_uuid
 * @param {String} _characteristic_uuid
 * @param {Real} _connection
 * @param {Id.Buffer} _data
 * @param {Real} _offset
 * @param {Real} _size
 * @returns {Real}
 */
function bluetooth_le_server_notify_value(_service_uuid, _characteristic_uuid, _connection, _data, _offset, _size)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _service_uuid, type: String
    if (!is_string(_service_uuid)) show_error($"{_GMFUNCTION_} :: _service_uuid expected string", true);
    buffer_write(__args_buffer__, buffer_u32, string_byte_length(_service_uuid));
    buffer_write(__args_buffer__, buffer_string, _service_uuid);

    // param: _characteristic_uuid, type: String
    if (!is_string(_characteristic_uuid)) show_error($"{_GMFUNCTION_} :: _characteristic_uuid expected string", true);
    buffer_write(__args_buffer__, buffer_u32, string_byte_length(_characteristic_uuid));
    buffer_write(__args_buffer__, buffer_string, _characteristic_uuid);

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

    var __return_value__ = __bluetooth_le_server_notify_value(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

/**
 * @param {Function} _callback
 * @returns {Bool}
 */
function bluetooth_set_callback_state_changed(_callback)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __dispatcher__ = __GMBluetooth_get_dispatcher();

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _callback, type: Function
    if (!is_callable(_callback)) show_error($"{_GMFUNCTION_} :: _callback expected callable type", true);
    var _callback_handle = __ext_core_function_register(_callback, __dispatcher__);
    buffer_write(__args_buffer__, buffer_u64, _callback_handle);

    var __return_value__ = __bluetooth_set_callback_state_changed(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

// Skipping function bluetooth_remove_callback_state_changed (no wrapper is required)


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


/**
 * @param {Function} _callback
 * @returns {Bool}
 */
function bluetooth_set_callback_le_disconnected(_callback)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __dispatcher__ = __GMBluetooth_get_dispatcher();

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _callback, type: Function
    if (!is_callable(_callback)) show_error($"{_GMFUNCTION_} :: _callback expected callable type", true);
    var _callback_handle = __ext_core_function_register(_callback, __dispatcher__);
    buffer_write(__args_buffer__, buffer_u64, _callback_handle);

    var __return_value__ = __bluetooth_set_callback_le_disconnected(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

// Skipping function bluetooth_remove_callback_le_disconnected (no wrapper is required)


/**
 * @param {Function} _callback
 * @returns {Bool}
 */
function bluetooth_set_callback_le_characteristic_value_changed(_callback)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __dispatcher__ = __GMBluetooth_get_dispatcher();

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _callback, type: Function
    if (!is_callable(_callback)) show_error($"{_GMFUNCTION_} :: _callback expected callable type", true);
    var _callback_handle = __ext_core_function_register(_callback, __dispatcher__);
    buffer_write(__args_buffer__, buffer_u64, _callback_handle);

    var __return_value__ = __bluetooth_set_callback_le_characteristic_value_changed(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

// Skipping function bluetooth_remove_callback_le_characteristic_value_changed (no wrapper is required)


/**
 * @param {Function} _callback
 * @returns {Bool}
 */
function bluetooth_set_callback_le_server_connection_state_changed(_callback)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __dispatcher__ = __GMBluetooth_get_dispatcher();

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _callback, type: Function
    if (!is_callable(_callback)) show_error($"{_GMFUNCTION_} :: _callback expected callable type", true);
    var _callback_handle = __ext_core_function_register(_callback, __dispatcher__);
    buffer_write(__args_buffer__, buffer_u64, _callback_handle);

    var __return_value__ = __bluetooth_set_callback_le_server_connection_state_changed(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

// Skipping function bluetooth_remove_callback_le_server_connection_state_changed (no wrapper is required)


/**
 * @param {Function} _callback
 * @returns {Bool}
 */
function bluetooth_set_callback_le_server_read_request(_callback)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __dispatcher__ = __GMBluetooth_get_dispatcher();

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _callback, type: Function
    if (!is_callable(_callback)) show_error($"{_GMFUNCTION_} :: _callback expected callable type", true);
    var _callback_handle = __ext_core_function_register(_callback, __dispatcher__);
    buffer_write(__args_buffer__, buffer_u64, _callback_handle);

    var __return_value__ = __bluetooth_set_callback_le_server_read_request(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

// Skipping function bluetooth_remove_callback_le_server_read_request (no wrapper is required)


/**
 * @param {Function} _callback
 * @returns {Bool}
 */
function bluetooth_set_callback_le_server_write_request(_callback)
{
    var __available__ = __GMBluetooth_is_available();
    if (!__available__) return;

    var __dispatcher__ = __GMBluetooth_get_dispatcher();

    var __args_buffer__ = __ext_core_get_args_buffer();

    // param: _callback, type: Function
    if (!is_callable(_callback)) show_error($"{_GMFUNCTION_} :: _callback expected callable type", true);
    var _callback_handle = __ext_core_function_register(_callback, __dispatcher__);
    buffer_write(__args_buffer__, buffer_u64, _callback_handle);

    var __return_value__ = __bluetooth_set_callback_le_server_write_request(buffer_get_address(__args_buffer__), buffer_tell(__args_buffer__));

    return __return_value__;
}

// Skipping function bluetooth_remove_callback_le_server_write_request (no wrapper is required)


/// @ignore
function __GMBluetooth_get_decoders()
{
    static __decoders__ = [
        __BluetoothLeDescriptorDefinition_decode,
        __BluetoothLeCharacteristicDefinition_decode,
        __BluetoothLeServiceDefinition_decode
    ];
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
