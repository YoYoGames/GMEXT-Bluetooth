
if (async_load[? "id"] == send_dialog_id) {
    if (async_load[? "status"]) {
        var _text = async_load[? "result"];
        if (string_length(_text) > 0 && conn != 0 && instance_exists(conn_inst)) {
            conn_inst.send_text(_text);
        }
    }
    send_dialog_id = -1;
}
