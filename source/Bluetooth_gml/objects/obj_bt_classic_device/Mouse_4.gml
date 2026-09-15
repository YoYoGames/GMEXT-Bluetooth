
bluetooth_classic_scan_stop();
bluetooth_classic_connect(device, DEMO_CLASSIC_SERVICE_UUID,
    function(_error_code, _message, _connection, _device) {
        show_debug_message("[GML] classic connect " + string(_error_code) + " " + _message);
        if (_error_code == BluetoothError.Ok) {
            global.classic_conn = _connection;
            global.classic_conn_inst = instance_create_depth(0, 0, 0, obj_bt_classic_connection, {connection: _connection});
        }
    });

