// connection arrives via the instance_create_depth struct argument.

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

function log_msg(_s)
{
    show_debug_message("[GML] " + _s);
    array_push(log_lines, _s);
    if (array_length(log_lines) > 8) array_delete(log_lines, 0, 1);
}

function clear_gatt_ui()
{
    for (var i = 0; i < array_length(rows); i++)
    {
        if (instance_exists(rows[i])) instance_destroy(rows[i]);
    }

    for (var i = 0; i < array_length(buttons); i++)
    {
        if (instance_exists(buttons[i])) instance_destroy(buttons[i]);
    }

    rows = [];
    buttons = [];
    next_row_y = rows_top;
}

// One row per discovered characteristic: a label plus action buttons gated on
// that characteristic's actual cross-platform GATT properties.
function add_characteristic_row(_characteristic, _service_uuid)
{
    var _y = next_row_y;
    next_row_y += row_h;

    var _properties = bluetooth_le_characteristic_get_properties(_characteristic);
    var _uuid = bluetooth_le_characteristic_get_uuid(_characteristic);

    log_msg(
        "characteristic " + _uuid
        + " properties=" + ble_property_names(_properties)
        + " (" + string(_properties) + ")"
    );

    var _row = instance_create_depth(ui_x, _y, 0, obj_bt_le_characteristic, {
        characteristic: _characteristic,
        service_uuid: _service_uuid,
        properties: _properties
    });
    array_push(rows, _row);

    // Buttons from left to right; the row's text is drawn after the last one.
    var _bx = ui_x;
    var _by = _y + action_h / 2;

    var _add_button = function(_object, _bx, _by, _extra)
    {
        _extra.owner = id;
        var _inst = instance_create_depth(_bx + action_w / 2, _by, 0, _object, _extra);
        _inst.image_xscale = action_w / sprite_get_width(spr_gm_button);
        _inst.image_yscale = action_h / sprite_get_height(spr_gm_button);
        array_push(buttons, _inst);
        return _bx + action_w + action_gap;
    };

    if (_properties & BluetoothLeCharacteristicProperty.Read)
    {
        _bx = _add_button(obj_bt_le_button_read, _bx, _by, {
            row: _row,
            characteristic: _characteristic
        });
    }

    if (_properties & (
        BluetoothLeCharacteristicProperty.Write
        | BluetoothLeCharacteristicProperty.WriteWithoutResponse
    ))
    {
        var _write_type = (_properties & BluetoothLeCharacteristicProperty.Write)
            ? BLE_WRITE_WITH_RESPONSE
            : BLE_WRITE_WITHOUT_RESPONSE;

        _bx = _add_button(obj_bt_le_button_write, _bx, _by, {
            row: _row,
            characteristic: _characteristic,
            write_type: _write_type
        });
    }

    if (_properties & (
        BluetoothLeCharacteristicProperty.Notify
        | BluetoothLeCharacteristicProperty.Indicate
    ))
    {
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


function is_demo_characteristic_uuid(_uuid)
{
    var _u = string_lower(_uuid);

    return _u == string_lower(DEMO_CHAR_RX_UUID)
        || _u == string_lower(DEMO_CHAR_TX_UUID)
        || _u == string_lower(DEMO_CHAR_INFO_UUID);
}

function find_row_for_characteristic(_characteristic)
{
    for (var i = 0; i < array_length(rows); i++)
    {
        if (instance_exists(rows[i]) && rows[i].characteristic == _characteristic)
            return rows[i];
    }

    return noone;
}

function discover_characteristics(_service)
{
    // Anonymous functions don't capture `var` locals in GML, so the service
    // UUID is bound into the callback's self via method().
    var _ctx = {
        uuid: bluetooth_le_service_get_uuid(_service),
        conn: id
    };

    log_msg("discovering characteristics for service " + _ctx.uuid + "...");

    var _r = bluetooth_le_characteristics_discover(
        _service,
        method(_ctx, function(_error_code, _message, _service)
        {
            if (!instance_exists(conn)) return;

            conn.log_msg(
                "characteristics " + uuid + ": "
                + string(_error_code) + " " + _message
            );

            if (_error_code != BluetoothError.Ok)
            {
                conn.discovery_in_progress = false;
                return;
            }

            var _char_count = bluetooth_le_characteristic_get_count(_service);
            conn.log_msg("characteristics found: " + string(_char_count));

            for (var j = 0; j < _char_count; j++)
            {
                var _characteristic = bluetooth_le_characteristic_get_at(_service, j);
                var _char_uuid = bluetooth_le_characteristic_get_uuid(_characteristic);

                // The demo only exposes RX, TX and INFO. Ignore any unrelated
                // characteristics a platform/device may report.
                if (!conn.is_demo_characteristic_uuid(_char_uuid)) continue;

                conn.add_characteristic_row(_characteristic, uuid);
            }

            conn.discovery_in_progress = false;
        })
    );

    if (_r != BluetoothError.Ok)
    {
        discovery_in_progress = false;
        log_msg("characteristics_discover failed to start: " + bluetooth_last_error_message());
    }
}

function discover_all()
{
    if (discovery_in_progress) return;

    clear_gatt_ui();
    discovery_in_progress = true;
    log_msg("discovering services...");

    var _r = bluetooth_le_services_discover(
        connection,
        function(_error_code, _message, _connection)
        {
            if (!instance_exists(id)) return;

            log_msg("services: " + string(_error_code) + " " + _message);
            if (_error_code != BluetoothError.Ok)
            {
                discovery_in_progress = false;
                return;
            }

            var _service_count = bluetooth_le_service_get_count(connection);
            log_msg("services found: " + string(_service_count));

            var _demo_service = 0;

            for (var i = 0; i < _service_count; i++)
            {
                var _service = bluetooth_le_service_get_at(connection, i);
                var _uuid = bluetooth_le_service_get_uuid(_service);

                if (string_lower(_uuid) == string_lower(DEMO_SERVICE_UUID))
                {
                    _demo_service = _service;
                    break;
                }
            }

            if (_demo_service == 0)
            {
                log_msg("demo service not found: " + DEMO_SERVICE_UUID);
                discovery_in_progress = false;
                return;
            }

            log_msg("demo service found: " + DEMO_SERVICE_UUID);
            discover_characteristics(_demo_service);
        }
    );

    if (_r != BluetoothError.Ok)
    {
        discovery_in_progress = false;
        log_msg("services_discover failed to start: " + bluetooth_last_error_message());
    }
}

// Forwarded from obj_bt_le_client's le_characteristic_value_changed callback.
on_value_changed = function(_characteristic, _connection)
{
    var _buf = buffer_create(512, buffer_grow, 1);
    var _n = bluetooth_le_characteristic_get_value(_characteristic, _buf, 0, 512);

    if (_n > 0)
    {
        var _text = ble_bytes_to_string(_buf, _n);
        log_msg("notification: " + _text);

        var _row = find_row_for_characteristic(_characteristic);
        if (_row != noone)
        {
            _row.value_text = _text;
            _row.status_text = "Notification received (" + string(_n) + " bytes)";
        }
    }

    buffer_delete(_buf);
};

log_msg("connected handle=" + string(connection));
log_msg("press DISCOVER GATT to find the demo service");

discover_button = instance_create_depth(872, 600, 0, obj_bt_le_button_discover, {
    owner: id
});

disconnect_button = instance_create_depth(872, 672, 0, obj_bt_le_button_disconnect, {
    owner: id
});
