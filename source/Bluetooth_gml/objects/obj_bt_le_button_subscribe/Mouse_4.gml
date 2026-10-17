if (locked) exit;
if (!instance_exists(owner)) exit;
if (!instance_exists(row)) exit;

locked = true;
var _uuid = bluetooth_le_characteristic_get_uuid(characteristic);

if (subscribed) {
	row.status_text = "Unsubscribing...";
	owner.log_msg($"UNSUBSCRIBE start: {_uuid}");

	var _unsubscribe = bluetooth_le_characteristic_unsubscribe(characteristic, method(callback_ctx, function(_error_code, _message, _characteristic) {
		if (instance_exists(owner)) {
			owner.on_unsubscribe_complete(_error_code, _message);
		}
	}));

	if (_unsubscribe != BluetoothError.Ok) {
		locked = false;
		row.status_text = "Unsubscribe failed to start";
		owner.log_msg($"UNSUBSCRIBE failed to start: {bluetooth_last_error_code()} {bluetooth_last_error_message()}");
	}
	exit;
}

row.status_text = "Subscribing...";
owner.log_msg($"SUBSCRIBE start: {_uuid}");

// The values go to the connection object, which shows them; one that arrives
// after it is gone is released rather than left held until it expires.
var _value_ctx = { owner: owner };

var _r = bluetooth_le_characteristic_subscribe(
	characteristic,
	subscribe_mode,
	method(_value_ctx, function(_characteristic, _connection, _value, _size) {
		if (instance_exists(owner)) {
			owner.on_value_changed(_characteristic, _connection, _value, _size);
		} else {
			bluetooth_value_release(_value);
		}
	}),
	method(callback_ctx, function(_error_code, _message, _characteristic) {
		if (instance_exists(owner)) {
			owner.on_subscribe_complete(_error_code, _message);
		}
	})
);

if (_r != BluetoothError.Ok) {
	locked = false;
	row.status_text = "Subscribe failed to start";
	owner.log_msg($"SUBSCRIBE failed to start: {bluetooth_last_error_code()} {bluetooth_last_error_message()}");
}
