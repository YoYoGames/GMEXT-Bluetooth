
/// obj_bt_classic_client : Draw GUI

var _x = 16;
var _y = 400;
var _line = 17;


// -----------------------------------------------------------------------------
// Permission
// -----------------------------------------------------------------------------

// Step takes the scan and connection state (refresh_status); Draw only draws.
var _permission_text = "Unknown";

switch (global.bt_permission_status)
{
    case BluetoothPermissionStatus.Granted:
        _permission_text = "Granted";
        break;

    case BluetoothPermissionStatus.Denied:
        _permission_text = "Denied";
        break;
}

var _conn = global.classic_conn;
var _scanning = status_scanning;
var _valid = status_valid;
var _connected = status_connected;
var _name = status_device_name;
var _address = status_device_address;
var _paired = status_paired;
var _pairing_supported = status_pairing_supported;


// -----------------------------------------------------------------------------
// Remote peer state
// -----------------------------------------------------------------------------

var _remote_text = "-";

if (instance_exists(global.classic_conn_inst))
{
    if (global.classic_conn_inst.has_remote)
    {
        _remote_text =
            string(global.classic_conn_inst.remote_x)
            + ", "
            + string(global.classic_conn_inst.remote_y);
    }
}


// -----------------------------------------------------------------------------
// Draw
// -----------------------------------------------------------------------------

draw_set_font(fnt_gm_15);
draw_set_color(c_white);
draw_set_halign(fa_left);
draw_set_valign(fa_top);


draw_text(_x, _y, "CLASSIC CLIENT");
_y += _line * 2;

if (unavailable_reason != "")
{
    draw_text(_x, _y, unavailable_reason);
    _y += _line * 2;
}

draw_text(_x, _y, "Permission: " + _permission_text);
_y += _line;

draw_text(
    _x,
    _y,
    "Scan: " + (_scanning ? "RUNNING" : "STOPPED")
);
_y += _line;

draw_text(
    _x,
    _y,
    "Devices: " + string(array_length(devices))
);
_y += _line;

draw_text(
    _x,
    _y,
    "Connection: " + string(_conn)
);
_y += _line;

draw_text(
    _x,
    _y,
    "Valid: " + (_valid ? "YES" : "NO")
);
_y += _line;

draw_text(
    _x,
    _y,
    "Connected: " + (_connected ? "YES" : "NO")
);
_y += _line;


if (_connected)
{
    _y += _line;

    draw_text(_x, _y, "Device: " + _name);
    _y += _line;

    draw_text(_x, _y, "Address: " + _address);
    _y += _line;

    draw_text(
        _x,
        _y,
        "Pairing supported: "
        + (_pairing_supported ? "YES" : "NO")
    );
    _y += _line;

    draw_text(
        _x,
        _y,
        "Paired: " + (_paired ? "YES" : "NO")
    );
    _y += _line;

    draw_text(
        _x,
        _y,
        "Peer pointer: " + _remote_text
    );
}
