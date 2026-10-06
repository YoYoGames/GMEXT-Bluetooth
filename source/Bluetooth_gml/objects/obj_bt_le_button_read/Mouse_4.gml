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
    method(_ctx, function(_error_code, _message, _characteristic, _value, _size)
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

        var _buf = buffer_create(max(_size, 1), buffer_fixed, 1);
        var _copy = bluetooth_le_value_copy(_value, _buf, 0);

        if (_copy >= 0)
        {
            var _text = ble_bytes_to_string(_buf, _size);

            row_inst.value_text = _text;
            row_inst.status_text = "Read OK (" + string(_size) + " bytes)";
            conn.log_msg("READ value: " + _text);
        }
        else
        {
            row_inst.status_text = "Read OK, but the value could not be copied";
            conn.log_msg("READ completed but value_copy returned " + string(_copy));
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
