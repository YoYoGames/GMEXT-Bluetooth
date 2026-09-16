show_debug_message("========== BLUETOOTH LE SERVER (DEMO) ==========");

if (!os_check_permission("android.permission.BLUETOOTH_ADVERTISE") || !os_check_permission("android.permission.BLUETOOTH_CONNECT")) {
    os_request_permission("android.permission.BLUETOOTH_ADVERTISE", "android.permission.BLUETOOTH_CONNECT");
}

bt_ready = bluetooth_initialize();

global.ble_server_conn = -1;
global.ble_server_conn_inst = noone;

if (!bt_ready) exit;

var _server_start = bluetooth_le_server_start();
show_debug_message($"[GML] bluetooth_le_server_start() = {_server_start}");

var _service_json = json_stringify({
    uuid: DEMO_SERVICE_UUID,
    characteristics: [
        {
            uuid: DEMO_CHAR_RX_UUID,
            properties: GATT_PROPERTY_WRITE | GATT_PROPERTY_WRITE_NO_RESPONSE,
            permissions: GATT_PERMISSION_WRITE
        },
        {
            uuid: DEMO_CHAR_TX_UUID,
            properties: GATT_PROPERTY_NOTIFY,
            permissions: 0
        }
    ]
});

bluetooth_set_callback_le_server_connection_state_changed(function(_connection, _connected, _device) {
    show_debug_message($"[GML] le server connection state: connected={_connected}");
    if (_connected) {
        global.ble_server_conn = _connection;
        global.ble_server_conn_inst = instance_create_depth(0, 0, 0, obj_bt_le_server_connection, {connection: _connection});
    } else {
        if (instance_exists(global.ble_server_conn_inst)) instance_destroy(global.ble_server_conn_inst);
    }
});

bluetooth_set_callback_le_server_write_request(function(_request_id, _connection, _service_uuid, _characteristic_uuid, _descriptor_uuid) {
    var _buf = buffer_create(512, buffer_grow, 1);
    var _n = bluetooth_le_server_write_request_get_value(_request_id, _buf, 0, 512);
    if (instance_exists(global.ble_server_conn_inst)) global.ble_server_conn_inst.on_receive(_characteristic_uuid, _buf, max(_n, 0));
    buffer_delete(_buf);
    bluetooth_le_server_respond_write(_request_id, BluetoothError.Ok);
});

bluetooth_set_callback_le_server_read_request(function(_request_id, _connection, _service_uuid, _characteristic_uuid, _descriptor_uuid, _offset) {
    // Nothing in the demo profile is readable - respond with an empty value.
    var _buf = buffer_create(1, buffer_fixed, 1);
    bluetooth_le_server_respond_read(_request_id, BluetoothError.Ok, _buf, 0, 0);
    buffer_delete(_buf);
});

var _add_service = bluetooth_le_server_add_service(_service_json, function(_error_code, _message) {
    show_debug_message("[GML] le_server_add_service " + string(_error_code) + " " + _message);
    if (_error_code != BluetoothError.Ok) return;

    var _settings_json = json_stringify({txPowerLevel: 0});
    var _data_json = json_stringify({includeName: true, services: [{uuid: DEMO_SERVICE_UUID}]});
    var _r = bluetooth_le_advertise_start(_settings_json, _data_json, function(_error_code, _message) {
        show_debug_message("[GML] le_advertise_start " + string(_error_code) + " " + _message);
    });
    if (_r != BluetoothError.Ok) {
        show_debug_message("[GML] le_advertise_start failed to start: " + string(bluetooth_last_error_code()) + " " + bluetooth_last_error_message());
    }
});
if (_add_service != BluetoothError.Ok) {
    show_debug_message("[GML] le_server_add_service failed to start: " + string(bluetooth_last_error_code()) + " " + bluetooth_last_error_message());
}
