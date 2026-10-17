show_debug_message("========== BLUETOOTH CLASSIC SERVER (DEMO) ==========");

bt_ready = bluetooth_is_initialized();
server_started = false;
permission_request_sent = false;

// The open connection's handle (0 when none) and the object showing it; that
// object clears both in its Clean Up when it is still the current one.
conn = 0;
conn_inst = noone;

// Callbacks reach GML after the call that queued them, even after this room
// is left, so each one is bound to this struct and checks the owner first.
callback_ctx = { owner: id };

// Why this room cannot run on this platform, or "" when it can.
unavailable_reason = "";

if (!bt_ready) {
	unavailable_reason = "Bluetooth is not initialized";
} else if (!bluetooth_feature_is_supported(BluetoothFeature.Classic)) {
	bt_ready = false;
	unavailable_reason = "Bluetooth Classic is not supported on this platform";
} else if (!bluetooth_feature_is_supported(BluetoothFeature.ClassicServer)) {
	bt_ready = false;
	unavailable_reason = "Bluetooth Classic server is not supported on this platform";
}

// The methods are defined before the room gives up on an unsupported
// platform, so Step can always call them.
on_permission_result = function(_error_code, _message, _status) {
	obj_bt_init.bt_permission_status = _status;
	show_debug_message($"[GML] permission request: {_error_code} {_message}, status: {_status}");

	if (_status == BluetoothPermissionStatus.Granted) {
		start_classic_server_demo();
	}
};

start_classic_server_demo = function() {
	if (!bt_ready || server_started) return;

	if (bluetooth_permission_get_status() != BluetoothPermissionStatus.Granted) {
		if (!permission_request_sent) {
			permission_request_sent = true;

			var _error = bluetooth_permission_request(method(callback_ctx, function(_error_code, _message, _status) {
				if (instance_exists(owner)) {
					owner.on_permission_result(_error_code, _message, _status);
				}
			}));

			show_debug_message($"[GML] bluetooth_permission_request = {_error}");

			if (_error != BluetoothError.Ok) {
				show_debug_message($"[GML] permission request failed: {bluetooth_last_error_message()}");
			}
		}

		return;
	}

	var _server_error = bluetooth_classic_server_start(
		DEMO_CLASSIC_SERVICE_NAME,
		DEMO_CLASSIC_SERVICE_UUID,
		method(callback_ctx, function(_connection, _device) {
			if (instance_exists(owner)) {
				owner.on_client_connected(_connection, _device);
			} else {
				// The room was left: close a link it accepted meanwhile.
				bluetooth_classic_disconnect(_connection);
			}
		}),
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
		method(callback_ctx, function(_error_code, _message) {
			if (instance_exists(owner)) {
				owner.on_server_stopped(_error_code, _message);
			}
		})
	);

	show_debug_message($"[GML] classic_server_start = {_server_error}");

	if (_server_error == BluetoothError.Ok) {
		server_started = true;
	} else {
		show_debug_message($"[GML] classic server failed to start: {bluetooth_last_error_message()}");
	}
};

on_client_connected = function(_connection, _device) {
	show_debug_message($"[GML] classic client connected conn={_connection}");

	// Destroyed before conn is assigned: its Clean Up clears conn while it is still current.
	if (instance_exists(conn_inst)) {
		instance_destroy(conn_inst);
	}

	conn = _connection;
	conn_inst = instance_create_depth(0, 0, 0, obj_bt_classic_connection, { connection: _connection, owner: id });
};

// The server's on_data serves every inbound link; only the one shown is fed
// to its connection object, any other chunk is released.
on_data = function(_connection, _value, _size) {
	if (_connection != conn || !instance_exists(conn_inst)) {
		bluetooth_value_release(_value);
		return;
	}

	conn_inst.on_receive_value(_value, _size);
};

// Also fires with BluetoothDisabled when the radio turns off.
on_disconnected = function(_error_code, _message, _connection) {
	show_debug_message($"[GML] classic client disconnected: {_error_code} {_message}");

	if (_connection != conn) return;

	if (instance_exists(conn_inst)) {
		instance_destroy(conn_inst);
	}

	conn = 0;
	conn_inst = noone;
};

// Only for an end the game did not ask for, such as the radio turning off
// (BluetoothDisabled); nothing restarts it, so the room has to be entered again.
on_server_stopped = function(_error_code, _message) {
	show_debug_message($"[GML] classic server stopped: {_error_code} {_message}");
	server_started = false;
};

// Draw only draws: Step calls this every frame to take what Draw shows.
refresh_status = function() {
	status_running = bt_ready && bluetooth_classic_server_is_running();
	status_discoverable = bt_ready && bluetooth_classic_discoverable_is_running();
	status_connected = false;
	status_device_name = "-";
	status_device_address = "-";

	if (conn == 0 || !bluetooth_classic_connection_is_valid(conn)) return;

	status_connected = bluetooth_classic_connection_is_connected(conn);

	if (!status_connected) return;

	var _device = bluetooth_classic_connection_get_device(conn);

	if (!bluetooth_device_is_valid(_device)) return;

	status_device_name = bluetooth_device_get_name(_device);

	var _address = bluetooth_device_get_address(_device);
	if (!is_undefined(_address)) {
		status_device_address = _address;
	}
};

refresh_status();

if (!bt_ready) {
	show_debug_message($"[GML] {unavailable_reason}");
	exit;
}

start_classic_server_demo();
