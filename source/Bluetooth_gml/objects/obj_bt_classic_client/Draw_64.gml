/// obj_bt_classic_client : Draw GUI

var _x = 16;
var _y = 16;
var _w = 270;
var _line = 22;

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


var _scanning = false;

if (bt_ready)
{
    _scanning = bluetooth_classic_scan_is_running();
}


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


draw_set_alpha(0.78);
draw_set_color(c_black);
draw_rectangle(_x, _y, _x + _w, _y + 245, false);
draw_set_alpha(1);

draw_set_font(fnt_gm_20);
draw_set_halign(fa_left);
draw_set_valign(fa_top);

draw_set_color(c_aqua);
draw_text(_x + 10, _y + 8, "CLASSIC CLIENT");

draw_set_color(c_white);

var _yy = _y + 36;

draw_text(_x + 10, _yy, "Permission: " + _permission_text);
_yy += _line;

draw_text(
    _x + 10,
    _yy,
    "Scan: " + (_scanning ? "RUNNING" : "STOPPED")
);
_yy += _line;

draw_text(
    _x + 10,
    _yy,
    "Devices: " + string(array_length(devices))
);
_yy += _line;

draw_text(_x + 10, _yy, "Connection: " + string(_conn));
_yy += _line;

draw_text(
    _x + 10,
    _yy,
    "Connected: " + (_connected ? "YES" : "NO")
);
_yy += _line;

if (_connected)
{
    draw_text(_x + 10, _yy, "Device: " + _name);
    _yy += _line;

    draw_text(_x + 10, _yy, "Address: " + _address);
    _yy += _line;

    draw_text(
        _x + 10,
        _yy,
        "Pairing supported: "
        + (_pairing_supported ? "YES" : "NO")
    );
    _yy += _line;

    draw_text(
        _x + 10,
        _yy,
        "Paired: " + (_paired ? "YES" : "NO")
    );
    _yy += _line;

    draw_text(
        _x + 10,
        _yy,
        "Peer pointer: " + _remote_text
    );
}