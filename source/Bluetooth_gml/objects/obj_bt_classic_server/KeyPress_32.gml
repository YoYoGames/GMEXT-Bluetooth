 
var _text = "Helloo"
 
var _buf = buffer_create(string_byte_length(_text) + 1, buffer_fixed, 1);
buffer_write(_buf, buffer_string, _text);
bluetooth_classic_send(global.classic_conn, _buf, 0, buffer_get_size(_buf));
buffer_delete(_buf);

