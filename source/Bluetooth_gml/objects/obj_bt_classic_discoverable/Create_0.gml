// Placed in rm_bt_classic_server. Toggles Classic discoverability so an
// unpaired client's scan can find this device and its RFCOMM service.

event_inherited();

// How long each request asks to keep the radio discoverable, in seconds.
duration = 120;

// current_time (ms) at which the current discoverable window ends.
discoverable_until = 0;

// True between bluetooth_classic_discoverable_start() and its callback (on
// Android, while the user answers the system prompt).
starting = false;

// Asked of the platform rather than read from os_type: Apple cannot start it,
// and Android cannot end it early, so there the button stays locked until the
// window expires on its own.
can_start = bluetooth_is_initialized() && bluetooth_feature_is_supported(BluetoothFeature.ClassicDiscoverable);
can_stop = bluetooth_is_initialized() && bluetooth_feature_is_supported(BluetoothFeature.ClassicDiscoverableStop);

text = "MAKE DISCOVERABLE";

// Callbacks reach GML after the call that queued them, so each one is bound
// to this struct and checks that this button still exists first.
callback_ctx = { owner: id };

on_discoverable_result = function(_error_code, _message, _duration_seconds) {
	starting = false;
	show_debug_message($"[GML] discoverable start: {_error_code} {_message}, duration={_duration_seconds}");

	if (_error_code == BluetoothError.Ok) {
		discoverable_until = current_time + _duration_seconds * 1000;
	}
};
