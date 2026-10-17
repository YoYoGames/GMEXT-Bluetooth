if (locked) exit;
if (!instance_exists(owner)) exit;
if (!instance_exists(row)) exit;

locked = true;
row.status_text = "Reading...";
owner.log_msg($"READ start: {bluetooth_le_characteristic_get_uuid(characteristic)}");

var _r = bluetooth_le_characteristic_read(characteristic, method(callback_ctx, function(_error_code, _message, _characteristic, _value, _size) {
	if (instance_exists(owner)) {
		owner.on_read_complete(_error_code, _message, _value, _size);
	} else if (_error_code == BluetoothError.Ok) {
		bluetooth_value_release(_value);
	}
}));

if (_r != BluetoothError.Ok) {
	locked = false;
	row.status_text = "Read failed to start";
	owner.log_msg($"READ failed to start: {bluetooth_last_error_code()} {bluetooth_last_error_message()}");
}
