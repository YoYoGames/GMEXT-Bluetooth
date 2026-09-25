if (locked) exit;
if (!instance_exists(owner)) exit;

// Actions that do not target a remote characteristic row.
switch (action)
{
    case "discover":
        owner.discover_all();
        exit;

    case "server_info":
        owner.change_info_value();
        exit;

    case "server_notify":
        owner.send_demo_notification();
        exit;
}

if (!instance_exists(row)) exit;

switch (action)
{
    case "read":
    {
        locked = true;
        row.status_text = "Reading...";
        owner.log_msg("READ start: " + bluetooth_le_characteristic_get_uuid(characteristic));

        var _ctx = {
            conn: owner,
            row_inst: row,
            button_inst: id
        };

        var _r = bluetooth_le_characteristic_read(
            characteristic,
            method(_ctx, function(_error_code, _message, _characteristic)
            {
                if (instance_exists(button_inst)) button_inst.locked = false;
                if (!instance_exists(conn)) return;

                conn.log_msg(
                    "READ complete: "
                    + string(_error_code)
                    + " "
                    + _message
                );

                if (!instance_exists(row_inst)) return;

                if (_error_code != BluetoothError.Ok)
                {
                    row_inst.status_text = "Read failed: " + _message;
                    return;
                }

                var _buf = buffer_create(512, buffer_grow, 1);
                var _n = bluetooth_le_characteristic_get_value(
                    _characteristic,
                    _buf,
                    0,
                    512
                );

                if (_n >= 0)
                {
                    var _text = conn.bytes_to_string(_buf, _n);

                    row_inst.value_text = _text;
                    row_inst.status_text = "Read OK (" + string(_n) + " bytes)";
                    conn.log_msg("READ value: " + _text);
                }
                else
                {
                    row_inst.status_text = "Read OK, but no cached value";
                    conn.log_msg("READ completed but get_value returned " + string(_n));
                }

                buffer_delete(_buf);
            })
        );

        if (_r != BluetoothError.Ok)
        {
            locked = false;
            row.status_text = "Read failed to start";
            owner.log_msg(
                "READ failed to start: "
                + string(bluetooth_last_error_code())
                + " "
                + bluetooth_last_error_message()
            );
        }

        break;
    }

    case "write":
    {
        locked = true;
        var _text = "Hello from GMBluetooth BLE client";
        var _buf = buffer_create(string_byte_length(_text) + 1, buffer_grow, 1);
        buffer_write(_buf, buffer_text, _text);
        var _size = buffer_tell(_buf);

        row.status_text = "Writing...";
        owner.log_msg("WRITE start: " + bluetooth_le_characteristic_get_uuid(characteristic));

        var _ctx = {
            conn: owner,
            row_inst: row,
            button_inst: id
        };

        var _r = bluetooth_le_characteristic_write(
            characteristic,
            _buf,
            0,
            _size,
            write_type,
            method(_ctx, function(_error_code, _message, _characteristic)
            {
                if (instance_exists(button_inst)) button_inst.locked = false;
                if (!instance_exists(conn)) return;

                conn.log_msg(
                    "WRITE complete: "
                    + string(_error_code)
                    + " "
                    + _message
                );

                if (!instance_exists(row_inst)) return;

                if (_error_code == BluetoothError.Ok)
                {
                    row_inst.status_text = "Write OK";
                }
                else
                {
                    row_inst.status_text = "Write failed: " + _message;
                }
            })
        );

        buffer_delete(_buf);

        if (_r != BluetoothError.Ok)
        {
            locked = false;
            row.status_text = "Write failed to start";
            owner.log_msg(
                "WRITE failed to start: "
                + string(bluetooth_last_error_code())
                + " "
                + bluetooth_last_error_message()
            );
        }

        break;
    }

    case "subscribe":
    {
        locked = true;
        row.status_text = "Subscribing...";
        owner.log_msg("SUBSCRIBE start: " + bluetooth_le_characteristic_get_uuid(characteristic));

        var _ctx = {
            conn: owner,
            row_inst: row,
            button_inst: id
        };

        var _r = bluetooth_le_characteristic_subscribe(
            characteristic,
            subscribe_mode,
            method(_ctx, function(_error_code, _message, _characteristic)
            {
                if (_error_code != BluetoothError.Ok && instance_exists(button_inst))
                {
                    button_inst.locked = false;
                }

                if (!instance_exists(conn)) return;

                conn.log_msg(
                    "SUBSCRIBE complete: "
                    + string(_error_code)
                    + " "
                    + _message
                );

                if (instance_exists(row_inst))
                {
                    if (_error_code == BluetoothError.Ok)
                    {
                        row_inst.status_text = "Subscribed - waiting for notifications";
                    }
                    else
                    {
                        row_inst.status_text = "Subscribe failed: " + _message;
                    }
                }

                if (_error_code == BluetoothError.Ok && instance_exists(button_inst))
                {
                    button_inst.text = "SUBSCRIBED";
                    button_inst.locked = true;
                }
            })
        );

        if (_r != BluetoothError.Ok)
        {
            locked = false;
            row.status_text = "Subscribe failed to start";
            owner.log_msg(
                "SUBSCRIBE failed to start: "
                + string(bluetooth_last_error_code())
                + " "
                + bluetooth_last_error_message()
            );
        }

        break;
    }
}
