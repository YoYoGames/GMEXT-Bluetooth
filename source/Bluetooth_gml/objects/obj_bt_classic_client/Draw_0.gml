
/// obj_bt_classic_client : Draw GUI

var _x = 16;
var _y = 400;
var _line = 17;


// -----------------------------------------------------------------------------
// Permission
// -----------------------------------------------------------------------------

var _permission = bluetooth_permission_get_status();

var _permission_text = "Unknown";

switch (_permission)
{
    case BluetoothPermissionStatus.Granted:
        _permission_text = "Granted";
        break;

    case BluetoothPermissionStatus.Denied:
        _permission_text = "Denied";
        break;
}


// -----------------------------------------------------------------------------
// Scan state
// -----------------------------------------------------------------------------

var _scanning = false;

if (bt_ready)
{
    _scanning = bluetooth_classic_scan_is_running();
}


// -----------------------------------------------------------------------------
// Connection state
// -----------------------------------------------------------------------------

var _conn = global.classic_conn;

var _valid = false;
var _connected = false;

var _name = "-";
var _address = "-";
var _paired = false;
var _pairing_supported = false;

if (_conn != 0)
{
    _valid = bluetooth_classic_connection_is_valid(_conn);

    if (_valid)
    {
        _connected =
            bluetooth_classic_connection_is_connected(_conn);

        var _device =
            bluetooth_classic_connection_get_device(_conn);

        if (bluetooth_device_is_valid(_device))
        {
            _name = bluetooth_device_get_name(_device);

            if (bluetooth_device_has_address(_device))
            {
                _address =
                    bluetooth_device_get_address(_device);
            }

            _pairing_supported =
                bluetooth_pairing_is_supported(_device);

            _paired =
                bluetooth_device_is_paired(_device);
        }
    }
}


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
