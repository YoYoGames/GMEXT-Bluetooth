show_debug_message("========== BLUETOOTH LE SERVER (DEMO) ==========");

bt_ready = bluetooth_is_initialized();
server_started = false;
service_ready = false;
startup_requested = false;
permission_request_sent = false;

// The open connection's handle (0 when none) and the object showing it; that
// object clears both in its Clean Up when it is still the current one.
conn = 0;
conn_inst = noone;

server_info_text = "GMBluetooth BLE Demo - READ characteristic OK";
info_version = 0;
notify_counter = 0;
last_rx_text = "-";
last_notify_text = "-";
last_server_event = "-";

// Server attribute handles, filled in by the add_service callback.
server_service = 0;
char_tx = 0;
char_info = 0;

// Callbacks reach GML after the call that queued them, even after this room
// is left, so each one is bound to this struct and checks the owner first.
callback_ctx = { owner: id };

// Why this room cannot run on this platform, or "" when it can.
unavailable_reason = "";

if (!bt_ready) {
	unavailable_reason = "Bluetooth is not initialized";
} else {
	var _server = bluetooth_feature_is_supported(BluetoothFeature.LeServer);
	var _advertise = bluetooth_feature_is_supported(BluetoothFeature.LeAdvertise);

	show_debug_message($"[GML] BLE GATT server supported: {_server}");
	show_debug_message($"[GML] BLE advertising supported: {_advertise}");

	if (!_server) {
		unavailable_reason = "Bluetooth LE server is not supported on this platform";
	} else if (!_advertise) {
		unavailable_reason = "Bluetooth LE advertising is not supported on this platform";
	}

	bt_ready = (unavailable_reason == "");
}

// The methods are defined before the room gives up on an unsupported
// platform, so Step and the buttons can always call them.
change_info_value = function() {
	if (!bt_ready) return;

	info_version++;
	server_info_text = $"GMBluetooth BLE Demo INFO #{info_version}";
	show_debug_message($"[GML] INFO value changed: {server_info_text}");
	last_server_event = "INFO changed";

	if (instance_exists(conn_inst)) {
		conn_inst.log_msg($"INFO changed: {server_info_text}");
	}
};

send_demo_notification = function() {
	if (!bt_ready) return;

	notify_counter++;
	var _text = $"Hello from BLE server #{notify_counter}";
	var _buf = buffer_create(string_byte_length(_text) + 1, buffer_grow, 1);
	buffer_write(_buf, buffer_text, _text);
	var _size = buffer_tell(_buf);

	// 0 broadcasts to every subscribed central, by choice; a server connection
	// handle such as conn would send to that one central only.
	var _r = bluetooth_le_server_notify_value(char_tx, 0, _buf, 0, _size);

	buffer_delete(_buf);

	if (_r == BluetoothError.Ok) {
		last_notify_text = _text;
		last_server_event = "TX notification broadcast";
		show_debug_message($"[GML] NOTIFY broadcast: {_text}");
		if (instance_exists(conn_inst)) {
			conn_inst.log_msg($"TX notify: {_text}");
		}
	} else {
		var _message = bluetooth_last_error_message();
		last_server_event = $"NOTIFY failed: {_message}";
		show_debug_message($"[GML] NOTIFY failed: {_message}");
		if (instance_exists(conn_inst)) {
			conn_inst.log_msg($"notify failed: {_message}");
		}
	}
};

on_permission_result = function(_error_code, _message, _status) {
	obj_bt_init.bt_permission_status = _status;
	show_debug_message($"[GML] permission request: {_error_code} {_message}, status: {_status}");

	if (_status == BluetoothPermissionStatus.Granted) {
		start_le_server_demo();
	}
};

start_le_server_demo = function() {
	if (!bt_ready || startup_requested) return;

	var _permission = bluetooth_permission_get_status();

	if (_permission != BluetoothPermissionStatus.Granted) {
		show_debug_message($"[GML] LE server waiting for Bluetooth permission. status={_permission}");

		if (!permission_request_sent) {
			permission_request_sent = true;

			var _error = bluetooth_permission_request(method(callback_ctx, function(_error_code, _message, _status) {
				if (instance_exists(owner)) {
					owner.on_permission_result(_error_code, _message, _status);
				}
			}));

			show_debug_message($"[GML] bluetooth_permission_request() = {_error}");

			if (_error != BluetoothError.Ok) {
				show_debug_message($"[GML] permission request failed: {bluetooth_last_error_message()}");
			}
		}

		return;
	}

	startup_requested = true;

	var _server_start = bluetooth_le_server_start(
		method(callback_ctx, function(_connection, _device) {
			if (instance_exists(owner)) {
				owner.on_central_connected(_connection, _device);
			}
		}),
		method(callback_ctx, function(_error_code, _message, _connection) {
			if (instance_exists(owner)) {
				owner.on_central_disconnected(_error_code, _message, _connection);
			}
		}),
		method(callback_ctx, function(_request_id, _connection, _characteristic, _descriptor, _offset) {
			if (instance_exists(owner)) {
				owner.on_read_request(_request_id, _connection, _characteristic, _descriptor, _offset);
			}
		}),
		method(callback_ctx, function(_request_id, _connection, _characteristic, _descriptor, _offset, _response_needed, _value, _size) {
			// The server was stopped with the room, which dropped the request and its value.
			if (instance_exists(owner)) {
				owner.on_write_request(_request_id, _connection, _characteristic, _descriptor, _offset, _response_needed, _value, _size);
			}
		}),
		method(callback_ctx, function(_error_code, _message) {
			if (instance_exists(owner)) {
				owner.on_server_stopped(_error_code, _message);
			}
		})
	);

	show_debug_message($"[GML] bluetooth_le_server_start() = {_server_start}");

	if (_server_start != BluetoothError.Ok) {
		show_debug_message($"[GML] bluetooth_le_server_start failed: {bluetooth_last_error_code()} {bluetooth_last_error_message()}");
		return;
	}

	server_started = true;

	var _add_service = bluetooth_le_server_add_service(service_definition, method(callback_ctx, function(_error_code, _message, _service) {
		if (instance_exists(owner)) {
			owner.on_service_added(_error_code, _message, _service);
		}
	}));

	if (_add_service != BluetoothError.Ok) {
		show_debug_message($"[GML] le_server_add_service failed to start: {bluetooth_last_error_code()} {bluetooth_last_error_message()}");
	}
};

on_service_added = function(_error_code, _message, _service) {
	show_debug_message($"[GML] le_server_add_service {_error_code} {_message}");

	if (_error_code != BluetoothError.Ok) return;

	server_service = _service;
	char_tx = bluetooth_le_characteristic_find(_service, DEMO_CHAR_TX_UUID);
	char_info = bluetooth_le_characteristic_find(_service, DEMO_CHAR_INFO_UUID);

	service_ready = true;

	// Advertise straight away; the button can stop/restart it later.
	with (obj_bt_le_button_advertise) {
		start_advertising();
	}
};

on_central_connected = function(_connection, _device) {
	show_debug_message($"[GML] LE server central connected: connection={_connection}, device={_device}");

	// Destroyed before conn is assigned: its Clean Up clears conn while it is still current.
	if (instance_exists(conn_inst)) {
		instance_destroy(conn_inst);
	}

	conn = _connection;
	conn_inst = instance_create_depth(0, 0, 0, obj_bt_le_server_connection, { connection: _connection, owner: id });
};

on_central_disconnected = function(_error_code, _message, _connection) {
	show_debug_message($"[GML] LE server central disconnected: connection={_connection}, {_error_code} {_message}");

	// Another central than the one shown leaving does not touch its UI.
	if (_connection != conn) return;

	if (instance_exists(conn_inst)) {
		instance_destroy(conn_inst);
	}

	conn = 0;
	conn_inst = noone;
};

// Only for an end the game did not ask for, such as the radio turning off;
// nothing restarts it, so the room has to be entered again.
on_server_stopped = function(_error_code, _message) {
	show_debug_message($"[GML] LE server stopped: {_error_code} {_message}");

	server_started = false;
	service_ready = false;
	server_service = 0;
	char_tx = 0;
	char_info = 0;
	last_server_event = $"Server stopped: {_message}";
};

on_write_request = function(_request_id, _connection, _characteristic, _descriptor, _offset, _response_needed, _value, _size) {
	show_debug_message($"[GML] LE WRITE request: connection={_connection} characteristic={_characteristic} descriptor={_descriptor} offset={_offset} response_needed={_response_needed} size={_size}");

	// The value belongs to the request: fetch it before responding, which
	// frees it. A write from a central the room does not show is answered
	// without fetching it.
	if (_connection == conn) {
		var _buf = bluetooth_value_to_buffer(_value, _size);

		if (_buf != -1) {
			last_rx_text = ble_bytes_to_string(_buf, _size);
			last_server_event = "RX write received";

			if (instance_exists(conn_inst)) {
				conn_inst.on_receive(_characteristic, _buf, _size);
			}

			buffer_delete(_buf);
		} else {
			show_debug_message($"[GML] LE WRITE value could not be fetched: {bluetooth_last_error_message()}");
		}
	}

	var _respond = bluetooth_le_server_respond_write(_request_id, BluetoothAttError.Success);

	if (_respond != BluetoothError.Ok) {
		show_debug_message($"[GML] LE WRITE respond failed: {bluetooth_last_error_message()}");
	}
};

on_read_request = function(_request_id, _connection, _characteristic, _descriptor, _offset) {
	show_debug_message($"[GML] LE READ request: connection={_connection} characteristic={_characteristic} descriptor={_descriptor} offset={_offset}");

	// Only the INFO characteristic is readable in this demo profile; any
	// central may read it, so the request is not matched against conn.
	if (_characteristic != char_info || _descriptor != 0) {
		respond_read_error(_request_id, BluetoothAttError.ReadNotPermitted);
		return;
	}

	var _info_text = server_info_text;
	var _buf = buffer_create(256, buffer_grow, 1);
	buffer_write(_buf, buffer_text, _info_text);
	var _total_size = buffer_tell(_buf);

	// A long read asks again from later offsets; past the end is an error.
	if (_offset > _total_size) {
		buffer_delete(_buf);
		respond_read_error(_request_id, BluetoothAttError.InvalidOffset);
		return;
	}

	var _read_size = _total_size - _offset;

	last_server_event = "INFO read request";
	show_debug_message($"[GML] LE READ response: {_info_text}, bytes={_read_size}");

	var _respond = bluetooth_le_server_respond_read(_request_id, _buf, _offset, _read_size);

	buffer_delete(_buf);

	if (_respond != BluetoothError.Ok) {
		show_debug_message($"[GML] LE READ respond failed: {bluetooth_last_error_message()}");
	}
};

respond_read_error = function(_request_id, _att_error) {
	var _respond = bluetooth_le_server_respond_read_error(_request_id, _att_error);

	if (_respond != BluetoothError.Ok) {
		show_debug_message($"[GML] LE READ respond failed: {bluetooth_last_error_message()}");
	}
};

// Draw only draws: Step calls this every frame to take what Draw shows.
refresh_status = function() {
	status_server_running = bt_ready && bluetooth_le_server_is_running();
	status_advertising = bt_ready && bluetooth_le_advertise_is_running();
};

refresh_status();

if (!bt_ready) {
	show_debug_message($"[GML] {unavailable_reason}");
	exit;
}

// Typed GATT service definition generated from spec.gmidl; a field left
// undefined (an initial value, descriptors) means none.
var _rx = new BluetoothLeCharacteristicDefinition();
_rx.uuid = DEMO_CHAR_RX_UUID;
_rx.properties = BluetoothLeCharacteristicProperty.Write | BluetoothLeCharacteristicProperty.WriteWithoutResponse;
_rx.permissions = BluetoothLeAttributePermission.Write;

var _tx = new BluetoothLeCharacteristicDefinition();
_tx.uuid = DEMO_CHAR_TX_UUID;
_tx.properties = BluetoothLeCharacteristicProperty.Notify;
_tx.permissions = BluetoothLeAttributePermission.None;

var _info = new BluetoothLeCharacteristicDefinition();
_info.uuid = DEMO_CHAR_INFO_UUID;
_info.properties = BluetoothLeCharacteristicProperty.Read;
_info.permissions = BluetoothLeAttributePermission.Read;

service_definition = new BluetoothLeServiceDefinition();
service_definition.uuid = DEMO_SERVICE_UUID;
service_definition.characteristics = [_rx, _tx, _info];

show_debug_message($"[GML] Demo service: {DEMO_SERVICE_UUID}");
show_debug_message($"[GML]   RX   {DEMO_CHAR_RX_UUID} [WRITE, WRITE_NO_RESPONSE]");
show_debug_message($"[GML]   TX   {DEMO_CHAR_TX_UUID} [NOTIFY]");
show_debug_message($"[GML]   INFO {DEMO_CHAR_INFO_UUID} [READ]");

start_le_server_demo();
