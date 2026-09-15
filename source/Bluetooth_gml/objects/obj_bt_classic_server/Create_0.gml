
show_debug_message("Bluetooth Classic Server")

bluetooth_classic_server_start(DEMO_CLASSIC_SERVICE_NAME, DEMO_CLASSIC_SERVICE_UUID);

bluetooth_classic_discoverable_start()

bluetooth_set_callback_classic_client_connected(function(_connection, _device) {
    global.classic_conn = _connection;
    show_debug_message("[GML] classic client connected conn=" + string(_connection));
});

bluetooth_set_callback_classic_data(function(_connection, _available_bytes) {
    var _buf = buffer_create(_available_bytes, buffer_grow, 1);
    var _n = bluetooth_classic_receive(_connection, _buf, 0, _available_bytes);
    var _text = buffer_peek(_buf, 0, buffer_string);
    buffer_delete(_buf);
    show_debug_message("[GML] classic RX (" + string(_n) + " bytes): " + _text);
});

bluetooth_set_callback_classic_disconnected(function(_connection, _error_code, _message) {
    show_debug_message("[GML] classic client disconnected: " + _message);
});



