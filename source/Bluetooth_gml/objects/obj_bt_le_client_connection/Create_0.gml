// connection arrives via the instance_create_depth struct argument.

rows = [];
buttons = [];
log_lines = [];
next_row_y = 220;

function log_msg(_s)
{
    show_debug_message("[GML] " + _s);
    array_push(log_lines, _s);
    if (array_length(log_lines) > 8) array_delete(log_lines, 0, 1);
}

function bytes_to_string(_buf, _n)
{
    var _s = "";
    for (var i = 0; i < _n; i++)
    {
        _s += chr(buffer_peek(_buf, i, buffer_u8));
    }
    return _s;
}

function property_names(_properties)
{
    var _names = [];

    if (_properties & BluetoothLeCharacteristicProperty.Read)
        array_push(_names, "READ");

    if (_properties & BluetoothLeCharacteristicProperty.Write)
        array_push(_names, "WRITE");

    if (_properties & BluetoothLeCharacteristicProperty.WriteWithoutResponse)
        array_push(_names, "WRITE_NO_RESPONSE");

    if (_properties & BluetoothLeCharacteristicProperty.Notify)
        array_push(_names, "NOTIFY");

    if (_properties & BluetoothLeCharacteristicProperty.Indicate)
        array_push(_names, "INDICATE");

    var _s = "";
    for (var i = 0; i < array_length(_names); i++)
    {
        if (i > 0) _s += ", ";
        _s += _names[i];
    }

    return (_s == "") ? "NONE" : _s;
}

// One row per discovered characteristic: a label plus action buttons gated on
// that characteristic's actual cross-platform GATT properties.
function add_characteristic_row(_characteristic, _service_uuid)
{
    var _y = next_row_y;
    next_row_y += 112;

    var _properties = bluetooth_le_characteristic_get_properties(_characteristic);
    var _uuid = bluetooth_le_characteristic_get_uuid(_characteristic);

    log_msg(
        "characteristic " + _uuid
        + " properties=" + property_names(_properties)
        + " (" + string(_properties) + ")"
    );

    var _row = instance_create_depth(16, _y, 0, obj_bt_le_characteristic, {
        characteristic: _characteristic,
        service_uuid: _service_uuid,
        properties: _properties
    });
    array_push(rows, _row);

    var _bx = 500;

    if (_properties & BluetoothLeCharacteristicProperty.Read)
    {
        array_push(
            buttons,
            instance_create_depth(_bx, _y + 28, 0, obj_bt_le_char_button, {
                owner: id,
                row: _row,
                characteristic: _characteristic,
                action: "read"
            })
        );
        _bx += 112;
    }

    if (_properties & (
        BluetoothLeCharacteristicProperty.Write
        | BluetoothLeCharacteristicProperty.WriteWithoutResponse
    ))
    {
        var _write_type = (_properties & BluetoothLeCharacteristicProperty.Write) ? 0 : 1;

        array_push(
            buttons,
            instance_create_depth(_bx, _y + 28, 0, obj_bt_le_char_button, {
                owner: id,
                row: _row,
                characteristic: _characteristic,
                action: "write",
                write_type: _write_type
            })
        );
        _bx += 112;
    }

    if (_properties & (
        BluetoothLeCharacteristicProperty.Notify
        | BluetoothLeCharacteristicProperty.Indicate
    ))
    {
        var _sub_mode = (_properties & BluetoothLeCharacteristicProperty.Notify)
            ? BluetoothLeSubscribeMode.Notify
            : BluetoothLeSubscribeMode.Indicate;

        array_push(
            buttons,
            instance_create_depth(_bx, _y + 28, 0, obj_bt_le_char_button, {
                owner: id,
                row: _row,
                characteristic: _characteristic,
                action: "subscribe",
                subscribe_mode: _sub_mode
            })
        );
    }
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

            if (_error_code != BluetoothError.Ok) return;

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
        })
    );

    if (_r != BluetoothError.Ok)
    {
        log_msg("characteristics_discover failed to start: " + bluetooth_last_error_message());
    }
}

function discover_all()
{
    log_msg("discovering services...");

    var _r = bluetooth_le_services_discover(
        connection,
        function(_error_code, _message, _connection)
        {
            if (!instance_exists(id)) return;

            log_msg("services: " + string(_error_code) + " " + _message);
            if (_error_code != BluetoothError.Ok) return;

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
                return;
            }

            log_msg("demo service found: " + DEMO_SERVICE_UUID);
            discover_characteristics(_demo_service);
        }
    );

    if (_r != BluetoothError.Ok)
    {
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
        var _text = bytes_to_string(_buf, _n);
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
discover_all();
