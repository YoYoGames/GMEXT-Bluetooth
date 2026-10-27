if (locked) exit;

if (bluetooth_classic_discoverable_is_running()) {
	var _stop = bluetooth_classic_discoverable_stop();
	show_debug_message($"[GML] bluetooth_classic_discoverable_stop() = {_stop}");
	if (_stop != BluetoothError.Ok) {
		show_debug_message($"[GML] discoverable stop failed: {bluetooth_last_error_message()}");
	}
	exit;
}

starting = true;

var _r = bluetooth_classic_discoverable_start(duration, method(callback_ctx, function(_error_code, _message, _duration_seconds) {
	if (instance_exists(owner)) {
		owner.on_discoverable_result(_error_code, _message, _duration_seconds);
	}
}), method(callback_ctx, function(_error_code, _message) {
	if (instance_exists(owner)) {
		owner.on_discoverable_stopped(_error_code, _message);
	}
}));
show_debug_message($"[GML] bluetooth_classic_discoverable_start({duration}) = {_r}");

if (_r != BluetoothError.Ok) {
	starting = false;
	show_debug_message($"[GML] discoverable failed to start: {bluetooth_last_error_message()}");
}
