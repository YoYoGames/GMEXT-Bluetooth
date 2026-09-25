show_debug_message("========== BLUETOOTH LE SERVER (DEMO) ==========");

bt_ready = bluetooth_is_initialized();
server_started = false;
startup_requested = false;
permission_request_sent = false;

global.ble_server_conn = 0;
global.ble_server_conn_inst = noone;

server_info_text = "GMBluetooth BLE Demo - READ characteristic OK";
info_version = 0;
notify_counter = 0;
last_rx_text = "-";
last_notify_text = "-";
last_server_event = "-";
control_buttons = [];

if (!bt_ready)
{
    show_debug_message("[GML] Bluetooth is not initialized");
    exit;
}

show_debug_message("[GML] BLE supported: " + string(bluetooth_le_is_supported()));
show_debug_message("[GML] BLE advertising supported: " + string(bluetooth_le_advertise_is_supported()));
show_debug_message("[GML] BLE GATT server supported: " + string(bluetooth_le_server_is_supported()));

if (!bluetooth_le_is_supported())
{
    show_debug_message("[GML] Bluetooth LE is not supported on this platform");
    exit;
}

if (!bluetooth_le_server_is_supported())
{
    show_debug_message("[GML] Bluetooth LE server is not supported on this platform");
    exit;
}

if (!bluetooth_le_advertise_is_supported())
{
    show_debug_message("[GML] Bluetooth LE advertising is not supported on this platform");
    exit;
}

function bytes_to_string(_buf, _n)
{
    var _s = "";
    for (var i = 0; i < _n; i++)
    {
        _s += chr(buffer_peek(_buf, i, buffer_u8));
    }
    return _s;
}

change_info_value = function()
{
    info_version++;
    server_info_text = "GMBluetooth BLE Demo INFO #" + string(info_version);
    show_debug_message("[GML] INFO value changed: " + server_info_text);
    last_server_event = "INFO changed";

    if (instance_exists(global.ble_server_conn_inst))
    {
        global.ble_server_conn_inst.log_msg("INFO changed: " + server_info_text);
    }
};

send_demo_notification = function()
{
    // A connection handle of 0 means broadcast to current subscribers.
    // This is required on platforms such as Windows where the local GATT
    // server does not expose a per-peer connection handle to the GML layer.
    notify_counter++;
    var _text = "Hello from BLE server #" + string(notify_counter);
    var _buf = buffer_create(string_byte_length(_text) + 1, buffer_grow, 1);
    buffer_write(_buf, buffer_text, _text);
    var _size = buffer_tell(_buf);

    var _r = bluetooth_le_server_notify_value(
        DEMO_SERVICE_UUID,
        DEMO_CHAR_TX_UUID,
        0,
        _buf,
        0,
        _size
    );

    buffer_delete(_buf);

    if (_r == BluetoothError.Ok)
    {
        last_notify_text = _text;
        last_server_event = "TX notification broadcast";
        show_debug_message("[GML] NOTIFY broadcast: \"" + _text + "\"");
        if (instance_exists(global.ble_server_conn_inst))
        {
            global.ble_server_conn_inst.log_msg("TX notify: " + _text);
        }
    }
    else
    {
        var _message = bluetooth_last_error_message();
        last_server_event = "NOTIFY failed: " + _message;
        show_debug_message("[GML] NOTIFY failed: " + _message);
        if (instance_exists(global.ble_server_conn_inst))
        {
            global.ble_server_conn_inst.log_msg("notify failed: " + _message);
        }
    }
};

bluetooth_set_callback_le_server_connection_state_changed(
    function(_connection, _connected, _device)
    {
        show_debug_message(
            $"[GML] LE server connection state: connection={_connection}, connected={_connected}, device={_device}"
        );

        if (_connected)
        {
            global.ble_server_conn = _connection;

            if (instance_exists(global.ble_server_conn_inst))
            {
                instance_destroy(global.ble_server_conn_inst);
            }

            global.ble_server_conn_inst = instance_create_depth(
                0,
                0,
                0,
                obj_bt_le_server_connection,
                {connection: _connection}
            );
        }
        else
        {
            if (instance_exists(global.ble_server_conn_inst))
            {
                instance_destroy(global.ble_server_conn_inst);
            }

            global.ble_server_conn = 0;
            global.ble_server_conn_inst = noone;
        }
    }
);

bluetooth_set_callback_le_server_write_request(
    function(
        _request_id,
        _connection,
        _service_uuid,
        _characteristic_uuid,
        _descriptor_uuid
    )
    {
        show_debug_message(
            "[GML] LE WRITE request: service=" + _service_uuid
            + " characteristic=" + _characteristic_uuid
            + " descriptor=" + _descriptor_uuid
        );

        var _buf = buffer_create(512, buffer_grow, 1);

        var _n = bluetooth_le_server_write_request_get_value(
            _request_id,
            _buf,
            0,
            512
        );

        show_debug_message("[GML] LE WRITE bytes received: " + string(_n));

        if (_n > 0)
        {
            last_rx_text = bytes_to_string(_buf, _n);
            last_server_event = "RX write received";

            if (instance_exists(global.ble_server_conn_inst))
            {
                global.ble_server_conn_inst.on_receive(
                    _characteristic_uuid,
                    _buf,
                    _n
                );
            }
        }

        buffer_delete(_buf);

        bluetooth_le_server_respond_write(
            _request_id,
            BluetoothError.Ok
        );
    }
);

bluetooth_set_callback_le_server_read_request(
    function(
        _request_id,
        _connection,
        _service_uuid,
        _characteristic_uuid,
        _descriptor_uuid,
        _offset
    )
    {
        show_debug_message(
            "[GML] LE READ request: service=" + _service_uuid
            + " characteristic=" + _characteristic_uuid
            + " descriptor=" + _descriptor_uuid
            + " offset=" + string(_offset)
        );

        // Only the INFO characteristic is readable in this demo profile.
        if (string_lower(_characteristic_uuid) != string_lower(DEMO_CHAR_INFO_UUID))
        {
            var _empty = buffer_create(1, buffer_fixed, 1);
            bluetooth_le_server_respond_read(
                _request_id,
                BluetoothError.NotSupported,
                _empty,
                0,
                0
            );
            buffer_delete(_empty);
            exit;
        }

        var _info_text = server_info_text;
        var _buf = buffer_create(256, buffer_grow, 1);
        buffer_seek(_buf, buffer_seek_start, 0);
        buffer_write(_buf, buffer_text, _info_text);

        var _total_size = buffer_tell(_buf);
        var _read_offset = clamp(_offset, 0, _total_size);
        var _read_size = max(0, _total_size - _read_offset);

        last_server_event = "INFO read request";

        show_debug_message(
            "[GML] LE READ response: \""
            + _info_text
            + "\" bytes="
            + string(_read_size)
        );

        bluetooth_le_server_respond_read(
            _request_id,
            BluetoothError.Ok,
            _buf,
            _read_offset,
            _read_size
        );

        buffer_delete(_buf);
    }
);

// Typed GATT service definition generated from spec.gmidl.
var _rx = new BluetoothLeCharacteristicDefinition();
_rx.uuid = DEMO_CHAR_RX_UUID;
_rx.properties =
    BluetoothLeCharacteristicProperty.Write
    | BluetoothLeCharacteristicProperty.WriteWithoutResponse;
_rx.permissions = GATT_PERMISSION_WRITE;
_rx.descriptors = [];

var _tx = new BluetoothLeCharacteristicDefinition();
_tx.uuid = DEMO_CHAR_TX_UUID;
_tx.properties = BluetoothLeCharacteristicProperty.Notify;
_tx.permissions = 0;
_tx.descriptors = [];

var _info = new BluetoothLeCharacteristicDefinition();
_info.uuid = DEMO_CHAR_INFO_UUID;
_info.properties = BluetoothLeCharacteristicProperty.Read;
_info.permissions = GATT_PERMISSION_READ;
_info.descriptors = [];

service_definition = new BluetoothLeServiceDefinition();
service_definition.uuid = DEMO_SERVICE_UUID;
service_definition.characteristics = [_rx, _tx, _info];

show_debug_message("[GML] Demo service: " + DEMO_SERVICE_UUID);
show_debug_message("[GML]   RX   " + DEMO_CHAR_RX_UUID + " [WRITE, WRITE_NO_RESPONSE]");
show_debug_message("[GML]   TX   " + DEMO_CHAR_TX_UUID + " [NOTIFY]");
show_debug_message("[GML]   INFO " + DEMO_CHAR_INFO_UUID + " [READ]");

start_le_server_demo = function()
{
    if (!bt_ready || startup_requested) return;

    var _permission = bluetooth_permission_get_status();

    if (_permission != BluetoothPermissionStatus.Granted)
    {
        show_debug_message("[GML] LE server waiting for Bluetooth permission. status=" + string(_permission));

        if (!permission_request_sent)
        {
            permission_request_sent = true;
            bluetooth_permission_request();
        }

        return;
    }

    startup_requested = true;

    var _server_start = bluetooth_le_server_start();

    show_debug_message(
        $"[GML] bluetooth_le_server_start() = {_server_start}"
    );

    if (_server_start != BluetoothError.Ok)
    {
        show_debug_message(
            "[GML] bluetooth_le_server_start failed: "
            + string(bluetooth_last_error_code())
            + " "
            + bluetooth_last_error_message()
        );

        return;
    }

    server_started = true;

    var _add_service = bluetooth_le_server_add_service(
        service_definition,
        function(_error_code, _message)
        {
            show_debug_message(
                "[GML] le_server_add_service "
                + string(_error_code)
                + " "
                + _message
            );

            if (_error_code != BluetoothError.Ok) return;

            var _settings_json = json_stringify({
                txPowerLevel: 0
            });

            var _data_json = json_stringify({
                includeName: true,
                services: [
                    {uuid: DEMO_SERVICE_UUID}
                ]
            });

            var _r = bluetooth_le_advertise_start(
                _settings_json,
                _data_json,
                function(_error_code, _message)
                {
                    show_debug_message(
                        "[GML] le_advertise_start "
                        + string(_error_code)
                        + " "
                        + _message
                    );
                }
            );

            if (_r != BluetoothError.Ok)
            {
                show_debug_message(
                    "[GML] le_advertise_start failed to start: "
                    + string(bluetooth_last_error_code())
                    + " "
                    + bluetooth_last_error_message()
                );
            }
        }
    );

    if (_add_service != BluetoothError.Ok)
    {
        show_debug_message(
            "[GML] le_server_add_service failed to start: "
            + string(bluetooth_last_error_code())
            + " "
            + bluetooth_last_error_message()
        );
    }
};

array_push(control_buttons, instance_create_depth(760, 300, 0, obj_bt_le_char_button, {
    owner: id,
    row: noone,
    characteristic: 0,
    action: "server_info"
}));

array_push(control_buttons, instance_create_depth(1010, 300, 0, obj_bt_le_char_button, {
    owner: id,
    row: noone,
    characteristic: 0,
    action: "server_notify"
}));

start_le_server_demo();
