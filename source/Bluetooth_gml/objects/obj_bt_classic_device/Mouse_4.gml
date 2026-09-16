
show_debug_message("Connecting")

bluetooth_classic_scan_stop();

function do_classic_connect() {
    bluetooth_classic_connect(device, DEMO_CLASSIC_SERVICE_UUID,
        function(_error_code, _message, _connection, _device) {
            show_debug_message("[GML] classic connect " + string(_error_code) + " " + _message);
            if (_error_code == BluetoothError.Ok) {
                global.classic_conn = _connection;
                global.classic_conn_inst = instance_create_depth(0, 0, 0, obj_bt_classic_connection, {connection: _connection});
            }
        });
}

if (bluetooth_device_is_paired(device)) {
    do_classic_connect();
} else {
    // Some Classic connections only succeed once the devices are bonded - pair first,
    // then attempt to connect either way (the peer may still accept an RFCOMM
    // connection even if pairing itself failed or was already in progress).
    bluetooth_pair(device,
        function(_error_code, _message, _device) {
            show_debug_message("[GML] pair " + string(_error_code) + " " + _message);
            if (!instance_exists(id)) return;
            paired = bluetooth_device_is_paired(_device);
            update_text();
            do_classic_connect();
        });
}

