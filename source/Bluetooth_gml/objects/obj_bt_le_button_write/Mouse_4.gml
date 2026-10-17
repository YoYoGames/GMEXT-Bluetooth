if (locked) exit;
if (!instance_exists(owner)) exit;
if (!instance_exists(row)) exit;

locked = true;
var _text = "Hello from GMBluetooth BLE client";
var _buf = buffer_create(string_byte_length(_text) + 1, buffer_grow, 1);
buffer_write(_buf, buffer_text, _text);
var _size = buffer_tell(_buf);

row.status_text = "Writing...";
owner.log_msg($"WRITE start: {bluetooth_le_characteristic_get_uuid(characteristic)}");

var _r = bluetooth_le_characteristic_write(characteristic, _buf, 0, _size, write_type, method(callback_ctx, function(_error_code, _message, _characteristic) {
	if (instance_exists(owner)) {
		owner.on_write_complete(_error_code, _message);
	}
}));

buffer_delete(_buf);

if (_r != BluetoothError.Ok) {
	locked = false;
	row.status_text = "Write failed to start";
	owner.log_msg($"WRITE failed to start: {bluetooth_last_error_code()} {bluetooth_last_error_message()}");
}
