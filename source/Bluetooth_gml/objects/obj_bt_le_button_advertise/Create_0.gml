event_inherited();

// The controller (obj_bt_le_server) is found in Room Start, once every
// instance in the room exists, so the room's instance order does not matter.
owner = noone;

// True between bluetooth_le_advertise_start() and its completion callback.
starting = false;

text = "START ADVERTISING";

// Callbacks reach GML after the call that queued them, so each one is bound
// to this struct and checks that this button still exists first.
callback_ctx = { owner: id };

start_advertising = function() {
	if (!instance_exists(owner)) return;
	if (starting || bluetooth_le_advertise_is_running()) return;

	// No name: next to a 128-bit service UUID it overflows Android's 31-byte
	// advertisement. Clients find the server by the UUID.
	var _data = new BluetoothLeAdvertiseData();
	_data.include_name = false;
	_data.service_uuids = [DEMO_SERVICE_UUID];

	starting = true;

	// No settings: connectable, at the platform's default power.
	var _r = bluetooth_le_advertise_start(
		undefined,
		_data,
		method(callback_ctx, function(_error_code, _message) {
			if (instance_exists(owner)) {
				owner.on_advertise_stopped(_error_code, _message);
			}
		}),
		method(callback_ctx, function(_error_code, _message) {
			if (instance_exists(owner)) {
				owner.on_advertise_started(_error_code, _message);
			}
		})
	);

	if (_r != BluetoothError.Ok) {
		starting = false;
		owner.last_server_event = "Advertise failed to start";
		show_debug_message($"[GML] le_advertise_start failed to start: {bluetooth_last_error_code()} {bluetooth_last_error_message()}");
	}
};

on_advertise_started = function(_error_code, _message) {
	starting = false;
	show_debug_message($"[GML] le_advertise_start {_error_code} {_message}");

	if (!instance_exists(owner)) return;

	owner.last_server_event = (_error_code == BluetoothError.Ok) ? "Advertising started" : $"Advertise failed: {_message}";
};

// Only for an end the game did not ask for, such as the radio turning off.
on_advertise_stopped = function(_error_code, _message) {
	show_debug_message($"[GML] le advertising stopped: {_error_code} {_message}");

	if (!instance_exists(owner)) return;

	owner.last_server_event = $"Advertising stopped: {_message}";
};

stop_advertising = function() {
	if (!bluetooth_le_advertise_is_running()) return;

	var _r = bluetooth_le_advertise_stop();
	show_debug_message($"[GML] bluetooth_le_advertise_stop() = {_r}");

	if (_r != BluetoothError.Ok) {
		owner.last_server_event = $"Advertise stop failed: {bluetooth_last_error_message()}";
		show_debug_message($"[GML] le advertise stop failed: {bluetooth_last_error_message()}");
		return;
	}

	owner.last_server_event = "Advertising stopped";
};
