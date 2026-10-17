// connection and owner (obj_bt_le_client) arrive via the creation struct.

rows = [];
buttons = [];
log_lines = [];

// Right half of the room; the left half belongs to the Bluetooth/BLE status panels.
ui_x = 560;
rows_top = 230;
row_h = 76;
next_row_y = rows_top;

// Characteristic action buttons: small, laid out left-to-right, text to their right.
action_w = 128;
action_h = 38;
action_gap = 8;

discovery_in_progress = false;
discover_button = noone;
disconnect_button = noone;

// Callbacks reach GML after the call that queued them, so each one is bound
// to this struct and checks that this instance still exists first.
callback_ctx = { owner: id };

function log_msg(_s) {
	show_debug_message($"[GML] {_s}");
	array_push(log_lines, _s);
	if (array_length(log_lines) > 8) array_delete(log_lines, 0, 1);
}

function clear_gatt_ui() {
	for (var _i = 0; _i < array_length(rows); _i++) {
		if (instance_exists(rows[_i])) instance_destroy(rows[_i]);
	}

	for (var _i = 0; _i < array_length(buttons); _i++) {
		if (instance_exists(buttons[_i])) instance_destroy(buttons[_i]);
	}

	rows = [];
	buttons = [];
	next_row_y = rows_top;
}

// One row per demo characteristic: a label plus action buttons gated on
// that characteristic's actual cross-platform GATT properties.
function add_characteristic_row(_characteristic) {
	var _y = next_row_y;
	next_row_y += row_h;

	var _properties = bluetooth_le_characteristic_get_properties(_characteristic);
	var _uuid = bluetooth_le_characteristic_get_uuid(_characteristic);

	log_msg($"characteristic {_uuid} properties={ble_property_names(_properties)} ({_properties})");

	var _row = instance_create_depth(ui_x, _y, 0, obj_bt_le_characteristic, {
		characteristic: _characteristic,
		properties: _properties
	});
	array_push(rows, _row);

	// Buttons from left to right; the row's text is drawn after the last one.
	var _bx = ui_x;
	var _by = _y + action_h / 2;

	var _add_button = function(_object, _bx, _by, _extra) {
		_extra.owner = id;
		var _inst = instance_create_depth(_bx + action_w / 2, _by, 0, _object, _extra);
		_inst.image_xscale = action_w / sprite_get_width(spr_gm_button);
		_inst.image_yscale = action_h / sprite_get_height(spr_gm_button);
		array_push(buttons, _inst);
		return _bx + action_w + action_gap;
	};

	if (_properties & BluetoothLeCharacteristicProperty.Read) {
		_bx = _add_button(obj_bt_le_button_read, _bx, _by, {
			row: _row,
			characteristic: _characteristic
		});
	}

	if (_properties & (BluetoothLeCharacteristicProperty.Write | BluetoothLeCharacteristicProperty.WriteWithoutResponse)) {
		var _write_type = (_properties & BluetoothLeCharacteristicProperty.Write)
			? BluetoothLeWriteType.WithResponse
			: BluetoothLeWriteType.WithoutResponse;

		_bx = _add_button(obj_bt_le_button_write, _bx, _by, {
			row: _row,
			characteristic: _characteristic,
			write_type: _write_type
		});
	}

	if (_properties & (BluetoothLeCharacteristicProperty.Notify | BluetoothLeCharacteristicProperty.Indicate)) {
		var _sub_mode = (_properties & BluetoothLeCharacteristicProperty.Notify)
			? BluetoothLeSubscribeMode.Notify
			: BluetoothLeSubscribeMode.Indicate;

		_bx = _add_button(obj_bt_le_button_subscribe, _bx, _by, {
			row: _row,
			characteristic: _characteristic,
			subscribe_mode: _sub_mode
		});
	}

	_row.x = _bx + action_gap;
}

function find_row_for_characteristic(_characteristic) {
	var _index = array_find_index(rows, method({ characteristic: _characteristic }, function(_row) {
		return instance_exists(_row) && _row.characteristic == characteristic;
	}));

	return (_index >= 0) ? rows[_index] : noone;
}

// One discovery pass: the demo service's characteristics come with it, so
// the callback can look the three demo characteristics up by UUID.
function discover_all() {
	if (discovery_in_progress) return;

	clear_gatt_ui();
	discovery_in_progress = true;
	log_msg("discovering the demo service...");

	var _options = new BluetoothLeDiscoverOptions();
	_options.service_uuids = [DEMO_SERVICE_UUID];
	_options.characteristics = true;

	var _r = bluetooth_le_services_discover(connection, _options, method(callback_ctx, function(_error_code, _message, _connection) {
		if (instance_exists(owner)) {
			owner.on_discovered(_error_code, _message, _connection);
		}
	}));

	if (_r != BluetoothError.Ok) {
		discovery_in_progress = false;
		log_msg($"services_discover failed to start: {bluetooth_last_error_message()}");
	}
}

function on_discovered(_error_code, _message, _connection) {
	discovery_in_progress = false;
	log_msg($"discovery: {_error_code} {_message}");

	if (_connection != connection || _error_code != BluetoothError.Ok) return;

	var _service = bluetooth_le_service_find(connection, DEMO_SERVICE_UUID);

	if (_service == 0) {
		log_msg($"demo service not found: {DEMO_SERVICE_UUID}");
		return;
	}

	log_msg($"demo service found: {DEMO_SERVICE_UUID}");

	// The demo only shows RX, TX and INFO, whatever else the service lists.
	var _uuids = [DEMO_CHAR_RX_UUID, DEMO_CHAR_TX_UUID, DEMO_CHAR_INFO_UUID];
	for (var _i = 0; _i < array_length(_uuids); _i++) {
		var _characteristic = bluetooth_le_characteristic_find(_service, _uuids[_i]);
		if (_characteristic == 0) {
			log_msg($"characteristic not found: {_uuids[_i]}");
			continue;
		}
		add_characteristic_row(_characteristic);
	}
}

// The subscribe button passes this as its on_value; the value is released
// whenever it is not shown, so it does not stay held until it expires.
on_value_changed = function(_characteristic, _connection, _value, _size) {
	if (_connection != connection) {
		bluetooth_value_release(_value);
		return;
	}

	var _buf = bluetooth_value_to_buffer(_value, _size);

	if (_buf == -1) {
		log_msg($"notification could not be fetched: {bluetooth_last_error_message()}");
		return;
	}

	var _text = ble_bytes_to_string(_buf, _size);
	buffer_delete(_buf);

	log_msg($"notification: {_text}");

	var _row = find_row_for_characteristic(_characteristic);
	if (_row != noone) {
		_row.value_text = _text;
		_row.status_text = $"Notification received ({_size} bytes)";
	}
};

log_msg($"connected handle={connection}");
log_msg("press DISCOVER GATT to find the demo service");

discover_button = instance_create_depth(872, 600, 0, obj_bt_le_button_discover, { owner: id });
disconnect_button = instance_create_depth(872, 672, 0, obj_bt_le_button_disconnect, { owner: id });
