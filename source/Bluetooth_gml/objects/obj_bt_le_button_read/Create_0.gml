// owner (obj_bt_le_client_connection), row and characteristic arrive via the creation struct.

event_inherited();

// Size and position are set by obj_bt_le_client_connection.add_characteristic_row().
font = fnt_gm_15;

text = "READ";

// Callbacks reach GML after the call that queued them, so each one is bound
// to this struct and checks that this button still exists first.
callback_ctx = { owner: id };

// A value the read delivered is released on every path that does not fetch it.
on_read_complete = function(_error_code, _message, _value, _size) {
	locked = false;

	if (!instance_exists(owner) || !instance_exists(row)) {
		if (_error_code == BluetoothError.Ok) {
			bluetooth_value_release(_value);
		}
		return;
	}

	owner.log_msg($"READ complete: {_error_code} {_message}");

	if (_error_code != BluetoothError.Ok) {
		row.status_text = $"Read failed: {_message}";
		return;
	}

	var _buf = bluetooth_value_to_buffer(_value, _size);

	if (_buf == -1) {
		row.status_text = "Read OK, but the value could not be fetched";
		owner.log_msg($"READ value could not be fetched: {bluetooth_last_error_message()}");
		return;
	}

	var _text = ble_bytes_to_string(_buf, _size);
	buffer_delete(_buf);

	row.value_text = _text;
	row.status_text = $"Read OK ({_size} bytes)";
	owner.log_msg($"READ value: {_text}");
};
