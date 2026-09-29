if (locked) exit;
if (!instance_exists(owner)) exit;
if (!instance_exists(row)) exit;

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
            var _text = ble_bytes_to_string(_buf, _n);

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
