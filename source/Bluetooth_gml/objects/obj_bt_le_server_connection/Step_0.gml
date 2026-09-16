// Nothing in the demo profile is readable/writable from this side, but TX
// only means something to a client that subscribed - push a value on a timer
// so the client's Subscribe button has something to see.
tick++;
if (tick % 60 == 0) {
    var _text = "server tick " + string(tick div 60);
    var _buf = buffer_create(string_byte_length(_text) + 1, buffer_fixed, 1);
    buffer_write(_buf, buffer_string, _text);
    var _r = bluetooth_le_server_notify_value(DEMO_SERVICE_UUID, DEMO_CHAR_TX_UUID, connection, _buf, 0, buffer_get_size(_buf));
    if (_r != BluetoothError.Ok) log_msg("notify failed: " + bluetooth_last_error_message());
    buffer_delete(_buf);
}
