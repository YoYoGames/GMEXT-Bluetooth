
show_debug_message("Bluetooth Classic Server")

global.classic_conn = -1;
global.classic_conn_inst = noone;

var _server_error = bluetooth_classic_server_start(DEMO_CLASSIC_SERVICE_NAME, DEMO_CLASSIC_SERVICE_UUID);
show_debug_message("[GML] classic_server_start = " + string(_server_error));


bluetooth_set_callback_classic_client_connected(function(_connection, _device) {
    global.classic_conn = _connection;
    global.classic_conn_inst = instance_create_depth(0, 0, 0, obj_bt_classic_connection, {connection: _connection});
    show_debug_message("[GML] classic client connected conn=" + string(_connection));
});

bluetooth_set_callback_classic_data(function(_connection, _available_bytes) {
    var _buf = buffer_create(_available_bytes, buffer_grow, 1);
    var _n = bluetooth_classic_receive(_connection, _buf, 0, _available_bytes);
    if (instance_exists(global.classic_conn_inst)) global.classic_conn_inst.on_receive(_buf, _n);
    buffer_delete(_buf);
});

bluetooth_set_callback_classic_disconnected(function(_connection, _error_code, _message) {
    show_debug_message("[GML] classic client disconnected: " + _message);
    if (instance_exists(global.classic_conn_inst)) instance_destroy(global.classic_conn_inst);
});



