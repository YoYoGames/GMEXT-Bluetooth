show_debug_message("========== BLUETOOTH CLASSIC CLIENT (DEMO) ==========");

bt_ready = bluetooth_is_initialized();

// Why this room cannot run on this platform, or "" when it can.
unavailable_reason = "";

if (!bt_ready) {
	unavailable_reason = "Bluetooth is not initialized";
} else if (!bluetooth_feature_is_supported(BluetoothFeature.Classic)) {
	bt_ready = false;
	unavailable_reason = "Bluetooth Classic is not supported on this platform";
}

auto_scan_after_permission = true;

// The open connection's handle (0 when none) and the object showing it; that
// object clears both in its Clean Up when it is still the current one.
conn = 0;
conn_inst = noone;
send_dialog_id = -1;

// The handle of a connect that has not completed yet (0 when none), owned
// here so Clean Up can cancel it when the room is left mid-connect.
pending_conn = 0;

// True from bluetooth_pair() to its callback; the device list locks meanwhile.
pairing = false;

// The device handles listed by the current scan, one button each.
devices = [];

list_x = room_width / 2;
list_y = 100;

// Callbacks reach GML after the call that queued them, even after this room
// is left, so each one is bound to this struct and checks the owner first.
callback_ctx = { owner: id };

// The methods are defined before the room gives up on an unsupported
// platform, so Step and the buttons can always call them.
device_address = function(_device) {
	var _address = bluetooth_device_get_address(_device);
	return is_undefined(_address) ? "<no address>" : _address;
};

// Draw only draws: Step calls this every frame to take what Draw shows.
refresh_status = function() {
	status_scanning = bt_ready && bluetooth_classic_scan_is_running();
	status_valid = false;
	status_connected = false;
	status_device_name = "-";
	status_device_address = "-";
	status_pairing_supported = false;
	status_paired = false;

	if (conn == 0) return;

	status_valid = bluetooth_classic_connection_is_valid(conn);

	if (!status_valid) return;

	status_connected = bluetooth_classic_connection_is_connected(conn);

	var _device = bluetooth_classic_connection_get_device(conn);

	if (!bluetooth_device_is_valid(_device)) return;

	status_device_name = bluetooth_device_get_name(_device);
	status_device_address = device_address(_device);
	status_pairing_supported = bluetooth_pairing_is_supported(_device);
	status_paired = bluetooth_device_is_paired(_device);
};

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
		bluetooth_start_classic_scan();
	}
};

bluetooth_start_classic_scan = function() {
	if (!bt_ready) return;

	if (bluetooth_permission_get_status() != BluetoothPermissionStatus.Granted) {
		bluetooth_request_permissions();
		return;
	}

	if (bluetooth_classic_scan_is_running()) return;

	// on_found fires the first time each scan sees a device, also one an
	// earlier scan cached, so the list is rebuilt without clearing the cache
	// (which would also invalidate a connected device's handle).
	devices = [];

	// Remove buttons from a previous scan before rebuilding the list.
	with (obj_bt_classic_device) {
		instance_destroy();
	}

	list_y = 100;

	var _error = bluetooth_classic_scan_start(
		method(callback_ctx, function(_device) {
			if (instance_exists(owner)) {
				owner.on_device_found(_device);
			}
		}),
		method(callback_ctx, function(_device) {
			if (instance_exists(owner)) {
				owner.on_device_updated(_device);
			}
		}),
		method(callback_ctx, function(_error_code, _message) {
			if (instance_exists(owner)) {
				owner.on_scan_stopped(_error_code, _message);
			}
		})
	);
	show_debug_message($"[GML] bluetooth_classic_scan_start() = {_error}");

	if (_error != BluetoothError.Ok) {
		show_debug_message($"[GML] classic scan failed to start: {bluetooth_last_error_message()}");
	}
};

on_device_found = function(_device) {
	if (array_contains(devices, _device)) return;

	show_debug_message($"[GML] classic device found: handle={_device} name={bluetooth_device_get_name(_device)} address={device_address(_device)}");

	instance_create_depth(list_x, list_y, 0, obj_bt_classic_device, { device: _device, owner: id });
	array_push(devices, _device);
	list_y += 100;
};

// A device answered again or had its name resolved: refresh its button.
on_device_updated = function(_device) {
	if (!array_contains(devices, _device)) {
		on_device_found(_device);
		return;
	}

	with (obj_bt_classic_device) {
		if (device == _device) {
			update_text();
		}
	}
};

on_scan_stopped = function(_error_code, _message) {
	show_debug_message($"[GML] classic scan stopped: {_error_code} {_message}, devices found: {array_length(devices)}");
};

// One connection at a time: a connect to another device closes the current
// one first, and a connect to the device already connected does nothing.
connect_device = function(_device) {
	if (!bt_ready || pending_conn != 0) return;

	if (conn != 0) {
		if (bluetooth_classic_connection_get_device(conn) == _device) return;

		var _closed = bluetooth_classic_disconnect(conn);

		if (_closed != BluetoothError.Ok) {
			show_debug_message($"[GML] classic disconnect failed: {bluetooth_last_error_message()}");
		}

		if (instance_exists(conn_inst)) {
			instance_destroy(conn_inst);
		}

		conn = 0;
		conn_inst = noone;
	}

	show_debug_message($"[GML] classic connect to {bluetooth_device_get_name(_device)}");

	pending_conn = bluetooth_classic_connect(
		_device,
		DEMO_CLASSIC_SERVICE_UUID,
		undefined,
		method(callback_ctx, function(_connection, _value, _size) {
			if (instance_exists(owner)) {
				owner.on_data(_connection, _value, _size);
			} else {
				bluetooth_value_release(_value);
			}
		}),
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
				bluetooth_classic_disconnect(_connection);
			}
		})
	);

	if (pending_conn == 0) {
		show_debug_message($"[GML] classic connect failed to start: {bluetooth_last_error_code()} {bluetooth_last_error_message()}");
	}
};

on_connect_result = function(_error_code, _message, _connection) {
	show_debug_message($"[GML] classic connect {_error_code} {_message}");

	pending_conn = 0;

	if (_error_code != BluetoothError.Ok) return;

	// Destroyed before conn is assigned: its Clean Up clears conn while it is still current.
	if (instance_exists(conn_inst)) {
		instance_destroy(conn_inst);
	}

	conn = _connection;
	conn_inst = instance_create_depth(0, 0, 0, obj_bt_classic_connection, { connection: _connection, owner: id });
};

// A chunk of an earlier link can still arrive after a switch; it is released,
// not fed into the current link's framing.
on_data = function(_connection, _value, _size) {
	if (_connection != conn || !instance_exists(conn_inst)) {
		bluetooth_value_release(_value);
		return;
	}

	conn_inst.on_receive_value(_value, _size);
};

// Also fires with BluetoothDisabled when the radio turns off.
on_disconnected = function(_error_code, _message, _connection) {
	show_debug_message($"[GML] classic disconnected: {_error_code} {_message}");

	if (_connection != conn) return;

	if (instance_exists(conn_inst)) {
		instance_destroy(conn_inst);
	}

	conn = 0;
	conn_inst = noone;
};

// Pairs first when the device needs it, then connects.
pair_device = function(_device, _connect_after) {
	if (!bt_ready || pairing) return;

	var _ctx = {
		owner: id,
		connect_after: _connect_after
	};

	pairing = true;

	show_debug_message($"[GML] pair {bluetooth_device_get_name(_device)}");

	var _error = bluetooth_pair(_device, method(_ctx, function(_error_code, _message, _device) {
		if (instance_exists(owner)) {
			owner.on_pair_result(_error_code, _message, _device, connect_after);
		}
	}));

	if (_error != BluetoothError.Ok) {
		pairing = false;
		show_debug_message($"[GML] pair failed to start: {_error} {bluetooth_last_error_message()}");
	}
};

on_pair_result = function(_error_code, _message, _device, _connect_after) {
	show_debug_message($"[GML] pair {_error_code} {_message}");

	pairing = false;

	with (obj_bt_classic_device) {
		if (device == _device) {
			paired = bluetooth_device_is_paired(_device);
			update_text();
		}
	}

	if (_connect_after && _error_code == BluetoothError.Ok) {
		connect_device(_device);
	}
};

// Forwarded from the Async - Dialog event with async_load.
handle_send_dialog = function(_data) {
	if (_data[? "id"] != send_dialog_id) return;

	send_dialog_id = -1;

	if (!_data[? "status"]) return;

	var _text = _data[? "result"];
	if (string_length(_text) > 0 && conn != 0 && instance_exists(conn_inst)) {
		conn_inst.send_text(_text);
	}
};

refresh_status();

if (!bt_ready) {
	show_debug_message($"[GML] {unavailable_reason}");
	exit;
}

if (bluetooth_permission_get_status() == BluetoothPermissionStatus.Granted) {
	auto_scan_after_permission = false;
	bluetooth_start_classic_scan();
} else {
	bluetooth_request_permissions();
}
