show_debug_message("========== BLUETOOTH LE CLIENT (DEMO) ==========");

bt_ready = bluetooth_is_initialized();

// Why this room cannot run on this platform, or "" when it can.
unavailable_reason = "";

if (!bt_ready) {
	unavailable_reason = "Bluetooth is not initialized";
} else if (!bluetooth_feature_is_supported(BluetoothFeature.LeCentral)) {
	bt_ready = false;
	unavailable_reason = "Bluetooth LE is not supported on this platform";
}

auto_scan_after_permission = true;

// The handle of a connect that has not completed yet (0 when none), owned
// here so Clean Up can cancel it when the room is left mid-connect.
pending_conn = 0;

// The open connection's handle (0 when none) and the object showing it; that
// object clears both in its Clean Up when it is still the current one.
conn = 0;
conn_inst = noone;

// The device handles listed by the current scan, one button each.
devices = [];

list_x = max(650, room_width * 0.55);
list_y = 100;

// Callbacks reach GML after the call that queued them, even after this room
// is left, so each one is bound to this struct and checks the owner first.
callback_ctx = { owner: id };

// The methods are defined before the room gives up on an unsupported
// platform, so Step and the buttons can always call them.
bluetooth_request_permissions = function() {
	if (!bt_ready) return;
	if (bluetooth_permission_get_status() == BluetoothPermissionStatus.Granted) return;

	var _error = bluetooth_permission_request(method(callback_ctx, function(_error_code, _message, _status) {
		if (instance_exists(owner)) {
			owner.on_permission_result(_error_code, _message, _status);
		}
	}));
	show_debug_message($"[GML] bluetooth_permission_request() = {_error}");

	if (_error != BluetoothError.Ok) {
		show_debug_message($"[GML] permission request failed: {bluetooth_last_error_message()}");
	}
};

on_permission_result = function(_error_code, _message, _status) {
	obj_bt_init.bt_permission_status = _status;
	show_debug_message($"[GML] permission request: {_error_code} {_message}, status: {_status}");

	if (auto_scan_after_permission && _status == BluetoothPermissionStatus.Granted) {
		auto_scan_after_permission = false;
		bluetooth_start_le_scan();
	}
};

bluetooth_start_le_scan = function() {
	if (!bt_ready) return;

	if (bluetooth_permission_get_status() != BluetoothPermissionStatus.Granted) {
		bluetooth_request_permissions();
		return;
	}

	if (bluetooth_le_scan_is_running()) return;

	// on_found fires the first time each scan sees a device, also one an
	// earlier scan cached, so the list is rebuilt without clearing the cache
	// (which would also invalidate a connected device's handle).
	devices = [];

	// Remove buttons from a previous scan before rebuilding the list.
	with (obj_bt_le_device) {
		instance_destroy();
	}

	list_y = 100;

	// No settings or filters: an active, low-latency scan that reports every device.
	var _error = bluetooth_le_scan_start(
		undefined,
		undefined,
		method(callback_ctx, function(_device) {
			if (instance_exists(owner)) {
				owner.on_device_seen(_device);
			}
		}),
		method(callback_ctx, function(_device) {
			if (instance_exists(owner)) {
				owner.on_device_seen(_device);
			}
		}),
		method(callback_ctx, function(_error_code, _message) {
			if (instance_exists(owner)) {
				owner.on_scan_stopped(_error_code, _message);
			}
		})
	);
	show_debug_message($"[GML] bluetooth_le_scan_start() = {_error}");

	if (_error != BluetoothError.Ok) {
		show_debug_message($"[GML] le scan failed to start: {bluetooth_last_error_message()}");
	}
};

// on_found and on_updated both land here. The demo only lists devices with a
// name, so one first found unnamed gets its button once an update names it.
on_device_seen = function(_device) {
	if (bluetooth_device_get_name(_device) == "") return;
	if (array_contains(devices, _device)) return;

	add_device_button(_device);
};

on_scan_stopped = function(_error_code, _message) {
	show_debug_message($"[GML] le scan stopped: {_error_code} {_message}, devices found: {array_length(devices)}");
};

add_device_button = function(_device) {
	instance_create_depth(list_x, list_y, 0, obj_bt_le_device, { device: _device, owner: id });
	array_push(devices, _device);
	list_y += 60;
};

// One connection at a time; the device buttons lock while one is pending
// or open (see obj_bt_le_device Step).
connect_device = function(_device) {
	if (!bt_ready || pending_conn != 0 || conn != 0) return;

	pending_conn = bluetooth_le_connect(
		_device,
		undefined,
		method(callback_ctx, function(_error_code, _message, _connection) {
			if (instance_exists(owner)) {
				owner.on_disconnected(_error_code, _message, _connection);
			}
		}),
		method(callback_ctx, function(_error_code, _message, _connection, _device) {
			if (instance_exists(owner)) {
				owner.on_connect_result(_error_code, _message, _connection);
			} else if (_error_code == BluetoothError.Ok) {
				// The room was left: close the link instead of spawning a connection object.
				bluetooth_le_disconnect(_connection);
			}
		})
	);

	if (pending_conn == 0) {
		show_debug_message($"[GML] le connect failed to start: {bluetooth_last_error_code()} {bluetooth_last_error_message()}");
	}
};

on_connect_result = function(_error_code, _message, _connection) {
	show_debug_message($"[GML] le connect {_error_code} {_message}");

	pending_conn = 0;

	if (_error_code != BluetoothError.Ok) return;

	// Destroyed before conn is assigned: its Clean Up clears conn while it is still current.
	if (instance_exists(conn_inst)) {
		instance_destroy(conn_inst);
	}

	conn = _connection;
	conn_inst = instance_create_depth(0, 0, 0, obj_bt_le_client_connection, { connection: _connection, owner: id });
};

on_disconnected = function(_error_code, _message, _connection) {
	show_debug_message($"[GML] le disconnected: {_error_code} {_message}");

	if (_connection != conn) return;

	if (instance_exists(conn_inst)) {
		instance_destroy(conn_inst);
	}

	conn = 0;
	conn_inst = noone;
};

// Draw only draws: Step calls this every frame to take what Draw shows.
refresh_status = function() {
	status_scanning = bt_ready && bluetooth_le_scan_is_running();
	status_connected = false;
	status_device_name = "-";
	status_device_address = "-";
	status_device_id = "-";
	status_rssi = "-";

	if (conn == 0 || !bluetooth_le_connection_is_valid(conn)) return;

	status_connected = bluetooth_le_connection_is_connected(conn);

	var _device = bluetooth_le_connection_get_device(conn);

	if (!bluetooth_device_is_valid(_device)) return;

	status_device_name = bluetooth_device_get_name(_device);
	status_device_id = bluetooth_device_get_id(_device);

	// Both are undefined when the platform has none (an LE address on Apple).
	var _address = bluetooth_device_get_address(_device);
	if (!is_undefined(_address)) {
		status_device_address = _address;
	}

	var _rssi = bluetooth_device_get_rssi(_device);
	if (!is_undefined(_rssi)) {
		status_rssi = $"{_rssi} dBm";
	}
};

refresh_status();

if (!bt_ready) {
	show_debug_message($"[GML] {unavailable_reason}");
	exit;
}

if (bluetooth_permission_get_status() == BluetoothPermissionStatus.Granted) {
	auto_scan_after_permission = false;
	bluetooth_start_le_scan();
} else {
	bluetooth_request_permissions();
}
