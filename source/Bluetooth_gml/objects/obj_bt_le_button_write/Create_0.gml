// owner (obj_bt_le_client_connection), row, characteristic and write_type arrive via the creation struct.

event_inherited();

// Size and position are set by obj_bt_le_client_connection.add_characteristic_row().
font = fnt_gm_15;

text = "WRITE";

// Callbacks reach GML after the call that queued them, so each one is bound
// to this struct and checks that this button still exists first.
callback_ctx = { owner: id };

on_write_complete = function(_error_code, _message) {
	locked = false;

	if (!instance_exists(owner)) return;

	owner.log_msg($"WRITE complete: {_error_code} {_message}");

	if (!instance_exists(row)) return;

	row.status_text = (_error_code == BluetoothError.Ok) ? "Write OK" : $"Write failed: {_message}";
};
