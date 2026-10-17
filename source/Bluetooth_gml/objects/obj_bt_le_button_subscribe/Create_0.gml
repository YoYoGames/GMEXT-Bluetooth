// owner (obj_bt_le_client_connection), row, characteristic and subscribe_mode arrive via the creation struct.

event_inherited();

// Size and position are set by obj_bt_le_client_connection.add_characteristic_row().
font = fnt_gm_15;

text = "SUBSCRIBE";

// Toggles: SUBSCRIBE while not subscribed, UNSUBSCRIBE once it is.
subscribed = false;

// Callbacks reach GML after the call that queued them, so each one is bound
// to this struct and checks that this button still exists first.
callback_ctx = { owner: id };

on_subscribe_complete = function(_error_code, _message) {
	locked = false;
	subscribed = (_error_code == BluetoothError.Ok);
	text = subscribed ? "UNSUBSCRIBE" : "SUBSCRIBE";

	if (!instance_exists(owner)) return;

	owner.log_msg($"SUBSCRIBE complete: {_error_code} {_message}");

	if (!instance_exists(row)) return;

	row.status_text = subscribed ? "Subscribed - waiting for notifications" : $"Subscribe failed: {_message}";
};

on_unsubscribe_complete = function(_error_code, _message) {
	locked = false;

	if (_error_code == BluetoothError.Ok) {
		subscribed = false;
		text = "SUBSCRIBE";
	}

	if (!instance_exists(owner)) return;

	owner.log_msg($"UNSUBSCRIBE complete: {_error_code} {_message}");

	if (!instance_exists(row)) return;

	row.status_text = (_error_code == BluetoothError.Ok) ? "Unsubscribed" : $"Unsubscribe failed: {_message}";
};
