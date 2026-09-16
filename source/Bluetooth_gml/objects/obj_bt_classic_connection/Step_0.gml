if (!bluetooth_classic_connection_is_connected(connection)) exit;

var _buf = buffer_create(8, buffer_fixed, 1);
buffer_write(_buf, buffer_s32, mouse_x);
buffer_write(_buf, buffer_s32, mouse_y);
var _result = bluetooth_classic_send(connection, _buf, 0, buffer_get_size(_buf));
buffer_delete(_buf);

send_log_counter++;
if (send_log_counter mod 30 == 0) {
    show_debug_message("[GML] classic send x=" + string(mouse_x) + " y=" + string(mouse_y) + " result=" + string(_result));
}
