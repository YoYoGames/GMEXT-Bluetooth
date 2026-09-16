show_debug_message("Connecting");

bluetooth_le_scan_stop();

var _connection = bluetooth_le_connect(device,
    function(_error_code, _message, _connection, _device) {
        show_debug_message("[GML] le connect " + string(_error_code) + " " + _message);
        if (_error_code != BluetoothError.Ok) return;
        global.ble_conn = _connection;
        global.ble_conn_inst = instance_create_depth(0, 0, 0, obj_bt_le_client_connection, {connection: _connection});
    });
if (_connection == 0) {
    show_debug_message("[GML] le connect failed to start: " + string(bluetooth_last_error_code()) + " " + bluetooth_last_error_message());
}
