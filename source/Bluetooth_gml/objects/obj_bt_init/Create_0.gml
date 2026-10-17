show_debug_message("========== BLUETOOTH MANAGER ==========");

// Persistent owner of the extension lifetime.
// Demo rooms must not call bluetooth_initialize()/bluetooth_shutdown().
// Only the first instance owns the extension; a duplicate leaves it alone in Clean Up.
owns_extension = false;

if (instance_number(obj_bt_init) > 1) {
	instance_destroy();
	exit;
}

owns_extension = true;

function bt_state_name(_state) {
	switch (_state) {
		case BluetoothState.Unknown:      return "Unknown";
		case BluetoothState.Resetting:    return "Resetting";
		case BluetoothState.Unsupported:  return "Unsupported";
		case BluetoothState.Unauthorized: return "Unauthorized";
		case BluetoothState.PoweredOff:   return "PoweredOff";
		case BluetoothState.PoweredOn:    return "PoweredOn";
	}

	return $"Unknown({_state})";
}

function bt_permission_name(_status) {
	switch (_status) {
		case BluetoothPermissionStatus.Unknown: return "Unknown";
		case BluetoothPermissionStatus.Granted: return "Granted";
		case BluetoothPermissionStatus.Denied:  return "Denied";
	}

	return $"Unknown({_status})";
}

function bt_yes_no(_value) {
	return _value ? "YES" : "NO";
}

bt_ready = bluetooth_initialize();

bt_state = BluetoothState.Unknown;
bt_permission_status = BluetoothPermissionStatus.Unknown;
bt_capabilities = {
	le: false,
	le_advertise: false,
	le_server: false,
	classic: false,
	classic_server: false
};

// Draw only draws: Step calls this every frame to take what Draw shows.
refresh_status = function() {
	status_initialized = bluetooth_is_initialized();
	status_error_code = 0;
	status_error_message = "";

	if (status_initialized) {
		status_error_code = bluetooth_last_error_code();
		status_error_message = bluetooth_last_error_message();
	}
};

on_state_changed = function(_state) {
	bt_state = _state;
	show_debug_message($"[GML] Bluetooth state changed: {bt_state_name(_state)} ({_state})");
};

on_permission_result = function(_error_code, _message, _status) {
	bt_permission_status = _status;
	show_debug_message($"[GML] Bluetooth permission request: {_error_code} {_message}, status: {bt_permission_name(_status)}");
};

// Step polls the permission once a second; see Step.
permission_poll_frames = game_get_speed(gamespeed_fps);

refresh_status();

if (!bt_ready) {
	show_debug_message($"[GML] bluetooth_initialize failed: {bluetooth_last_error_code()} {bluetooth_last_error_message()}");
	room_goto(rm_bt_menu);
	exit;
}

// Cache the capability matrix once so demo rooms can also use it for UI.
bt_capabilities.le = bluetooth_feature_is_supported(BluetoothFeature.LeCentral);
bt_capabilities.le_advertise = bluetooth_feature_is_supported(BluetoothFeature.LeAdvertise);
bt_capabilities.le_server = bluetooth_feature_is_supported(BluetoothFeature.LeServer);
bt_capabilities.classic = bluetooth_feature_is_supported(BluetoothFeature.Classic);
bt_capabilities.classic_server = bluetooth_feature_is_supported(BluetoothFeature.ClassicServer);

bt_permission_status = bluetooth_permission_get_status();

show_debug_message("----------------------------------------");
show_debug_message($"[GML] Initialized: {bt_yes_no(bluetooth_is_initialized())}");
show_debug_message($"[GML] Permission status: {bt_permission_name(bt_permission_status)}");
show_debug_message($"[GML] BLE supported: {bt_yes_no(bt_capabilities.le)}");
show_debug_message($"[GML] BLE advertising supported: {bt_yes_no(bt_capabilities.le_advertise)}");
show_debug_message($"[GML] BLE GATT server supported: {bt_yes_no(bt_capabilities.le_server)}");
show_debug_message($"[GML] Classic supported: {bt_yes_no(bt_capabilities.classic)}");
show_debug_message($"[GML] Classic server supported: {bt_yes_no(bt_capabilities.classic_server)}");
show_debug_message("[GML] Explicit pairing support is device-specific.");
show_debug_message("[GML] After discovery use bluetooth_pairing_is_supported(device).");
show_debug_message("----------------------------------------");

// Callbacks reach GML after the call that queued them, so each is bound to a
// struct and checks that this instance still exists before forwarding.
var _ctx = { owner: id };

bluetooth_set_callback_state_changed(method(_ctx, function(_state) {
	if (instance_exists(owner)) {
		owner.on_state_changed(_state);
	}
}));

if (bt_permission_status != BluetoothPermissionStatus.Granted) {
	show_debug_message("[GML] Requesting Bluetooth permission...");
	var _error = bluetooth_permission_request(method(_ctx, function(_error_code, _message, _status) {
		if (instance_exists(owner)) {
			owner.on_permission_result(_error_code, _message, _status);
		}
	}));

	show_debug_message($"[GML] bluetooth_permission_request = {_error}");

	if (_error != BluetoothError.Ok) {
		show_debug_message($"[GML] permission request failed: {bluetooth_last_error_message()}");
	}
}

room_goto(rm_bt_menu);
