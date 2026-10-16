
if (async_load[? "id"] == send_dialog_id) {
    if (async_load[? "status"]) {
        var _text = async_load[? "result"];
        if (string_length(_text) > 0 && global.classic_conn != 0 && instance_exists(global.classic_conn_inst)) {
            global.classic_conn_inst.send_text(_text);
        }
    }
    send_dialog_id = -1;
}
