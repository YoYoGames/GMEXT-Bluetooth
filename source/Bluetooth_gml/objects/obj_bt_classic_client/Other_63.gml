if (async_load[? "id"] == send_dialog_id) {
    if (async_load[? "status"]) {
        var _text = async_load[? "result"];
        if (string_length(_text) > 0 && global.classic_conn != -1) {
            var _buf = buffer_create(string_byte_length(_text) + 1, buffer_fixed, 1);
            buffer_write(_buf, buffer_string, _text);
            bluetooth_classic_send(global.classic_conn, _buf, 0, buffer_get_size(_buf));
            buffer_delete(_buf);
            show_debug_message("[GML] classic TX: " + _text);
        }
    }
    send_dialog_id = -1;
}
