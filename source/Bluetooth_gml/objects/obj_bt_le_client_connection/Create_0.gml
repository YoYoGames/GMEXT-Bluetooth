// connection arrives via the instance_create_depth struct argument.

rows = [];
buttons = [];
log_lines = [];
next_row_y = 96;

function log_msg(_s) {
    show_debug_message("[GML] " + _s);
    array_push(log_lines, _s);
    if (array_length(log_lines) > 8) array_delete(log_lines, 0, 1);
}

function bytes_to_string(_buf, _n) {
    var _s = "";
    for (var i = 0; i < _n; i++) {
        _s += chr(buffer_peek(_buf, i, buffer_u8));
    }
    return _s;
}

// One row per discovered characteristic: a label (obj_bt_le_characteristic)
// plus up to 3 action buttons (obj_bt_le_char_button), gated on that
// characteristic's actual GATT properties.
function add_characteristic_row(_characteristic, _service_uuid) {
    var _y = next_row_y;
    next_row_y += 100;

    var _row = instance_create_depth(0, _y, 0, obj_bt_le_characteristic, {
        characteristic: _characteristic,
        service_uuid: _service_uuid
    });
    array_push(rows, _row);

    var _properties = bluetooth_le_characteristic_get_properties(_characteristic);
    var _bx = 420;
    if (_properties & GATT_PROPERTY_READ) {
        array_push(buttons, instance_create_depth(_bx, _y, 0, obj_bt_le_char_button, {owner: id, row: _row, characteristic: _characteristic, action: "read"}));
        _bx += 90;
    }
    if (_properties & (GATT_PROPERTY_WRITE | GATT_PROPERTY_WRITE_NO_RESPONSE)) {
        var _write_type = (_properties & GATT_PROPERTY_WRITE) ? 0 : 1;
        array_push(buttons, instance_create_depth(_bx, _y, 0, obj_bt_le_char_button, {owner: id, row: _row, characteristic: _characteristic, action: "write", write_type: _write_type}));
        _bx += 90;
    }
    if (_properties & (GATT_PROPERTY_NOTIFY | GATT_PROPERTY_INDICATE)) {
        var _sub_mode = (_properties & GATT_PROPERTY_NOTIFY) ? BluetoothLeSubscribeMode.Notify : BluetoothLeSubscribeMode.Indicate;
        array_push(buttons, instance_create_depth(_bx, _y, 0, obj_bt_le_char_button, {owner: id, row: _row, characteristic: _characteristic, action: "subscribe", subscribe_mode: _sub_mode}));
        _bx += 90;
    }
}

function find_row_for_characteristic(_characteristic) {
    for (var i = 0; i < array_length(rows); i++) {
        if (instance_exists(rows[i]) && rows[i].characteristic == _characteristic) return rows[i];
    }
    return noone;
}

function discover_characteristics(_service) {
    // Anonymous functions don't capture `var` locals in GML, so the service
    // uuid is bound into the callback's `self` via method() instead.
    var _ctx = {uuid: bluetooth_le_service_get_uuid(_service), conn: id};
    var _r = bluetooth_le_characteristics_discover(_service, method(_ctx, function(_error_code, _message, _service) {
        if (!instance_exists(conn)) return;
        conn.log_msg("characteristics " + uuid + ": " + string(_error_code) + " " + _message);
        if (_error_code != BluetoothError.Ok) return;
        var _char_count = bluetooth_le_characteristic_get_count(_service);
        for (var j = 0; j < _char_count; j++) {
            conn.add_characteristic_row(bluetooth_le_characteristic_get_at(_service, j), uuid);
        }
    }));
    if (_r != BluetoothError.Ok) log_msg("characteristics_discover failed to start: " + bluetooth_last_error_message());
}

function discover_all() {
    log_msg("discovering services...");
    var _r = bluetooth_le_services_discover(connection, function(_error_code, _message, _connection) {
        if (!instance_exists(id)) return;
        log_msg("services: " + string(_error_code) + " " + _message);
        if (_error_code != BluetoothError.Ok) return;
        var _service_count = bluetooth_le_service_get_count(connection);
        for (var i = 0; i < _service_count; i++) {
            discover_characteristics(bluetooth_le_service_get_at(connection, i));
        }
    });
    if (_r != BluetoothError.Ok) log_msg("services_discover failed to start: " + bluetooth_last_error_message());
}

// Forwarded from obj_bt_le_client's le_characteristic_value_changed callback.
on_value_changed = function(_characteristic, _connection) {
    var _buf = buffer_create(512, buffer_grow, 1);
    var _n = bluetooth_le_characteristic_get_value(_characteristic, _buf, 0, 512);
    if (_n > 0) {
        var _text = bytes_to_string(_buf, _n);
        log_msg("notify: " + _text);
        var _row = find_row_for_characteristic(_characteristic);
        if (_row != noone) _row.value_text = _text;
    }
    buffer_delete(_buf);
};

discover_all();
