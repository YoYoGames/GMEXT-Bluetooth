if (locked) exit;
if (!instance_exists(owner)) exit;
if (!instance_exists(row)) exit;

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
