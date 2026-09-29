// connection arrives via the instance_create_depth struct argument.

log_lines = [];

function log_msg(_s) {
    show_debug_message("[GML] " + _s);
    array_push(log_lines, _s);
    if (array_length(log_lines) > 8) array_delete(log_lines, 0, 1);
}

// Forwarded from obj_bt_le_server's write-request callback.
on_receive = function(_characteristic_uuid, _buf, _n) {
    log_msg("RX write: " + ble_bytes_to_string(_buf, _n));
};

log_msg("client connected");
