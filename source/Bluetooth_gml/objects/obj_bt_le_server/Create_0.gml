show_debug_message("========== BLUETOOTH LE SERVER (DEMO) ==========");

bt_ready = bluetooth_is_initialized();
server_started = false;
service_ready = false;
startup_requested = false;
permission_request_sent = false;

// The open connection's handle (0 when none) and the object showing it; that
// object clears both in its Clean Up when it is still the current one.
conn = 0;
conn_inst = noone;

server_info_text = "GMBluetooth BLE Demo - READ characteristic OK";
info_version = 0;
notify_counter = 0;
last_rx_text = "-";
last_notify_text = "-";
last_server_event = "-";

// Server attribute handles, filled in by the add_service callback.
server_service = 0;
char_tx = 0;
char_info = 0;

// Why this room cannot run on this platform, or "" when it can.
unavailable_reason = "";

if (!bt_ready)
{
    unavailable_reason = "Bluetooth is not initialized";
}
else
{
    show_debug_message("[GML] BLE supported: " + string(bluetooth_le_is_supported()));
    show_debug_message("[GML] BLE advertising supported: " + string(bluetooth_le_advertise_is_supported()));
    show_debug_message("[GML] BLE GATT server supported: " + string(bluetooth_le_server_is_supported()));

    if (!bluetooth_le_is_supported())
    {
        unavailable_reason = "Bluetooth LE is not supported on this platform";
    }
    else if (!bluetooth_le_server_is_supported())
    {
        unavailable_reason = "Bluetooth LE server is not supported on this platform";
    }
    else if (!bluetooth_le_advertise_is_supported())
    {
        unavailable_reason = "Bluetooth LE advertising is not supported on this platform";
    }

    bt_ready = (unavailable_reason == "");
}

// The methods are defined before the room gives up on an unsupported
// platform, so Step and the buttons can always call them.
change_info_value = function()
{
    if (!bt_ready) return;

    info_version++;
    server_info_text = "GMBluetooth BLE Demo INFO #" + string(info_version);
    show_debug_message("[GML] INFO value changed: " + server_info_text);
    last_server_event = "INFO changed";

    if (instance_exists(conn_inst))
    {
        conn_inst.log_msg("INFO changed: " + server_info_text);
    }
};

send_demo_notification = function()
{
    if (!bt_ready) return;

    // A connection handle of 0 means broadcast to current subscribers.
    // This is required on platforms such as Windows where the local GATT
    // server does not expose a per-peer connection handle to the GML layer.
    notify_counter++;
    var _text = "Hello from BLE server #" + string(notify_counter);
    var _buf = buffer_create(string_byte_length(_text) + 1, buffer_grow, 1);
    buffer_write(_buf, buffer_text, _text);
    var _size = buffer_tell(_buf);

    var _r = bluetooth_le_server_notify_value(
        char_tx,
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
        if (instance_exists(conn_inst))
        {
            conn_inst.log_msg("TX notify: " + _text);
        }
    }
    else
    {
        var _message = bluetooth_last_error_message();
        last_server_event = "NOTIFY failed: " + _message;
        show_debug_message("[GML] NOTIFY failed: " + _message);
        if (instance_exists(conn_inst))
        {
            conn_inst.log_msg("notify failed: " + _message);
        }
    }
};

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

            var _error = bluetooth_permission_request(function(_error_code, _message, _status)
            {
                obj_bt_init.bt_permission_status = _status;

                show_debug_message(
                    "[GML] permission request: " + string(_error_code)
                    + " " + _message
                    + ", status: " + string(_status)
                );

                if (_status == BluetoothPermissionStatus.Granted)
                {
                    start_le_server_demo();
                }
            });

            show_debug_message(
                $"[GML] bluetooth_permission_request() = {_error}"
            );

            if (_error != BluetoothError.Ok)
            {
                show_debug_message(
                    "[GML] permission request failed: " + bluetooth_last_error_message()
                );
            }
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
        function(_error_code, _message, _service)
        {
            show_debug_message(
                "[GML] le_server_add_service "
                + string(_error_code)
                + " "
                + _message
            );

            if (_error_code != BluetoothError.Ok) return;

            // The getters walk the service in definition order; match by UUID
            // so the order of service_definition.characteristics can change.
            server_service = _service;
            var _count = bluetooth_le_characteristic_get_count(_service);
            for (var _i = 0; _i < _count; _i++)
            {
                var _characteristic = bluetooth_le_characteristic_get_at(_service, _i);
                var _uuid = string_lower(bluetooth_le_characteristic_get_uuid(_characteristic));

                if (_uuid == string_lower(DEMO_CHAR_TX_UUID))
                {
                    char_tx = _characteristic;
                }
                else if (_uuid == string_lower(DEMO_CHAR_INFO_UUID))
                {
                    char_info = _characteristic;
                }
            }

            service_ready = true;

            // Advertise straight away; the button can stop/restart it later.
            with (obj_bt_le_button_advertise)
            {
                start_advertising();
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

// Draw only draws: Step calls this every frame to take what Draw shows.
refresh_status = function()
{
    status_server_running = bt_ready && bluetooth_le_server_is_running();
    status_advertising = bt_ready && bluetooth_le_advertise_is_running();
};

refresh_status();

if (!bt_ready)
{
    show_debug_message("[GML] " + unavailable_reason);
    exit;
}

bluetooth_set_callback_le_server_connection_state_changed(
    function(_connection, _connected, _device)
    {
        show_debug_message(
            $"[GML] LE server connection state: connection={_connection}, connected={_connected}, device={_device}"
        );

        if (_connected)
        {
            conn = _connection;

            if (instance_exists(conn_inst))
            {
                instance_destroy(conn_inst);
            }

            conn_inst = instance_create_depth(
                0,
                0,
                0,
                obj_bt_le_server_connection,
                {connection: _connection, owner: id}
            );
        }
        else
        {
            if (instance_exists(conn_inst))
            {
                instance_destroy(conn_inst);
            }

            conn = 0;
            conn_inst = noone;
        }
    }
);

bluetooth_set_callback_le_server_write_request(
    function(
        _request_id,
        _connection,
        _characteristic,
        _descriptor,
        _offset,
        _response_needed,
        _value,
        _size
    )
    {
        show_debug_message(
            "[GML] LE WRITE request: characteristic=" + string(_characteristic)
            + " descriptor=" + string(_descriptor)
            + " offset=" + string(_offset)
            + " response_needed=" + string(_response_needed)
            + " size=" + string(_size)
        );

        // The value belongs to the request: fetch it before responding.
        var _buf = buffer_create(max(_size, 1), buffer_fixed, 1);
        var _n = bluetooth_value_fetch(_value, _buf, 0);

        show_debug_message("[GML] LE WRITE bytes received: " + string(_n));

        if (_n > 0)
        {
            last_rx_text = ble_bytes_to_string(_buf, _n);
            last_server_event = "RX write received";

            if (instance_exists(conn_inst))
            {
                conn_inst.on_receive(
                    _characteristic,
                    _buf,
                    _n
                );
            }
        }

        buffer_delete(_buf);

        var _respond = bluetooth_le_server_respond_write(
            _request_id,
            BluetoothAttError.Success
        );

        if (_respond != BluetoothError.Ok)
        {
            show_debug_message("[GML] LE WRITE respond failed: " + bluetooth_last_error_message());
        }
    }
);

bluetooth_set_callback_le_server_read_request(
    function(
        _request_id,
        _connection,
        _characteristic,
        _descriptor,
        _offset
    )
    {
        show_debug_message(
            "[GML] LE READ request: characteristic=" + string(_characteristic)
            + " descriptor=" + string(_descriptor)
            + " offset=" + string(_offset)
        );

        // Only the INFO characteristic is readable in this demo profile.
        if (_characteristic != char_info || _descriptor != 0)
        {
            var _empty = buffer_create(1, buffer_fixed, 1);
            var _refused = bluetooth_le_server_respond_read(
                _request_id,
                BluetoothAttError.ReadNotPermitted,
                _empty,
                0,
                0
            );
            buffer_delete(_empty);

            if (_refused != BluetoothError.Ok)
            {
                show_debug_message("[GML] LE READ respond failed: " + bluetooth_last_error_message());
            }
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

        var _respond = bluetooth_le_server_respond_read(
            _request_id,
            BluetoothAttError.Success,
            _buf,
            _read_offset,
            _read_size
        );

        buffer_delete(_buf);

        if (_respond != BluetoothError.Ok)
        {
            show_debug_message("[GML] LE READ respond failed: " + bluetooth_last_error_message());
        }
    }
);

// Typed GATT service definition generated from spec.gmidl.
var _rx = new BluetoothLeCharacteristicDefinition();
_rx.uuid = DEMO_CHAR_RX_UUID;
_rx.properties =
    BluetoothLeCharacteristicProperty.Write
    | BluetoothLeCharacteristicProperty.WriteWithoutResponse;
_rx.permissions = BluetoothLeAttributePermission.Write;
_rx.value = [];
_rx.descriptors = [];

var _tx = new BluetoothLeCharacteristicDefinition();
_tx.uuid = DEMO_CHAR_TX_UUID;
_tx.properties = BluetoothLeCharacteristicProperty.Notify;
_tx.permissions = BluetoothLeAttributePermission.None;
_tx.value = [];
_tx.descriptors = [];

var _info = new BluetoothLeCharacteristicDefinition();
_info.uuid = DEMO_CHAR_INFO_UUID;
_info.properties = BluetoothLeCharacteristicProperty.Read;
_info.permissions = BluetoothLeAttributePermission.Read;
_info.value = [];
_info.descriptors = [];

service_definition = new BluetoothLeServiceDefinition();
service_definition.uuid = DEMO_SERVICE_UUID;
service_definition.characteristics = [_rx, _tx, _info];

show_debug_message("[GML] Demo service: " + DEMO_SERVICE_UUID);
show_debug_message("[GML]   RX   " + DEMO_CHAR_RX_UUID + " [WRITE, WRITE_NO_RESPONSE]");
show_debug_message("[GML]   TX   " + DEMO_CHAR_TX_UUID + " [NOTIFY]");
show_debug_message("[GML]   INFO " + DEMO_CHAR_INFO_UUID + " [READ]");

start_le_server_demo();
