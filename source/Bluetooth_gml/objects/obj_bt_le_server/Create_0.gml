show_debug_message("========== BLUETOOTH LE SERVER (DEMO) ==========");

//
// Bluetooth is initialized by the persistent Bluetooth manager.
// This object only checks that it is ready.
//
bt_ready = bluetooth_is_initialized();

global.ble_server_conn = 0;
global.ble_server_conn_inst = noone;

if (!bt_ready)
{
    show_debug_message("[GML] Bluetooth is not initialized");
    exit;
}


//
// Permissions are handled through the extension.
// Do not reference Android permissions directly from GML.
//
var _permission_status = bluetooth_permission_get_status();

if (_permission_status != BluetoothPermissionStatus.Granted)
{
    show_debug_message("[GML] Bluetooth permissions are not granted - requesting");
    bluetooth_permission_request();
    exit;
}


//
// Check the capabilities required by this demo.
//
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


//
// Server callbacks
//
bluetooth_set_callback_le_server_connection_state_changed(
    function(_connection, _connected, _device)
    {
        show_debug_message(
            $"[GML] le server connection state: connection={_connection}, connected={_connected}"
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
                {
                    connection: _connection
                }
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
        var _buf = buffer_create(512, buffer_grow, 1);

        var _n = bluetooth_le_server_write_request_get_value(
            _request_id,
            _buf,
            0,
            512
        );

        if (_n > 0 && instance_exists(global.ble_server_conn_inst))
        {
            global.ble_server_conn_inst.on_receive(
                _characteristic_uuid,
                _buf,
                _n
            );
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
        //
        // Nothing in the demo profile is readable.
        // Respond successfully with an empty value.
        //
        var _buf = buffer_create(1, buffer_fixed, 1);

        bluetooth_le_server_respond_read(
            _request_id,
            BluetoothError.Ok,
            _buf,
            0,
            0
        );

        buffer_delete(_buf);
    }
);


// -----------------------------------------------------------------------------
// Build the GATT service using the typed ExtGen classes.
// -----------------------------------------------------------------------------

var _rx = new BluetoothLeCharacteristicDefinition();

_rx.uuid = DEMO_CHAR_RX_UUID;

_rx.properties =
    BluetoothLeCharacteristicProperty.Write
    | BluetoothLeCharacteristicProperty.WriteWithoutResponse;

_rx.permissions = GATT_PERMISSION_WRITE;

_rx.descriptors = [];


var _tx = new BluetoothLeCharacteristicDefinition();

_tx.uuid = DEMO_CHAR_TX_UUID;

_tx.properties =
    BluetoothLeCharacteristicProperty.Notify;

_tx.permissions = 0;

_tx.descriptors = [];


var _service = new BluetoothLeServiceDefinition();

_service.uuid = DEMO_SERVICE_UUID;

_service.characteristics = [
    _rx,
    _tx
];


// -----------------------------------------------------------------------------
// Start the BLE GATT server.
// -----------------------------------------------------------------------------

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

    exit;
}


// -----------------------------------------------------------------------------
// Add our service.
// -----------------------------------------------------------------------------

var _add_service = bluetooth_le_server_add_service(
    _service,

    function(_error_code, _message)
    {
        show_debug_message(
            "[GML] le_server_add_service "
            + string(_error_code)
            + " "
            + _message
        );

        if (_error_code != BluetoothError.Ok)
        {
            return;
        }


        //
        // Advertising is still configured using the advertising JSON API.
        // Only the GATT service definition was converted to typed classes.
        //
        var _settings_json = json_stringify({
            txPowerLevel: 0
        });

        var _data_json = json_stringify({
            includeName: true,

            services: [
                {
                    uuid: DEMO_SERVICE_UUID
                }
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