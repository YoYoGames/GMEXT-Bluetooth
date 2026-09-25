/// obj_bt_classic_server : Draw GUI

var _x = room_width-16;
var _y = 16;
var _w = 370;
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


var _running = false;
var _discoverable = false;

if (bt_ready)
{
    _running =
        bluetooth_classic_server_is_running();

    _discoverable =
        bluetooth_classic_discoverable_is_running();
}


var _conn = global.classic_conn;

var _connected = false;
var _device_name = "-";
var _address = "-";

if (_conn != 0)
{
    if (bluetooth_classic_connection_is_valid(_conn))
    {
        _connected =
            bluetooth_classic_connection_is_connected(_conn);

        var _device =
            bluetooth_classic_connection_get_device(_conn);

        if (bluetooth_device_is_valid(_device))
        {
            _device_name =
                bluetooth_device_get_name(_device);

            if (bluetooth_device_has_address(_device))
            {
                _address =
                    bluetooth_device_get_address(_device);
            }
        }
    }
}


var _peer_position = "-";
var _buffered_bytes = 0;

if (instance_exists(global.classic_conn_inst))
{
    _buffered_bytes =
        global.classic_conn_inst.recv_buffer_size;

    if (global.classic_conn_inst.has_remote)
    {
        _peer_position =
            string(global.classic_conn_inst.remote_x)
            + ", "
            + string(global.classic_conn_inst.remote_y);
    }
}


draw_set_alpha(0.80);
draw_set_color(c_black);
draw_rectangle(_x, _y, _x + _w, _y + 265, false);
draw_set_alpha(1);

draw_set_font(fnt_gm_20);
draw_set_halign(fa_right);
draw_set_valign(fa_top);

draw_set_color(c_aqua);
draw_text(_x + 10, _y + 8, "CLASSIC RFCOMM SERVER");

draw_set_color(c_white);

var _yy = _y + 36;

draw_text(_x + 10, _yy, "Permission: " + _permission_text);
_yy += _line;

draw_text(
    _x + 10,
    _yy,
    "Server: " + (_running ? "RUNNING" : "STOPPED")
);
_yy += _line;

draw_text(
    _x + 10,
    _yy,
    "Discoverable: " + (_discoverable ? "YES" : "NO")
);
_yy += _line;

draw_text(
    _x + 10,
    _yy,
    "Service: " + DEMO_CLASSIC_SERVICE_NAME
);
_yy += _line;

draw_text(
    _x + 10,
    _yy,
    "UUID: " + DEMO_CLASSIC_SERVICE_UUID
);
_yy += _line + 4;

draw_text(
    _x + 10,
    _yy,
    "Connection: " + string(_conn)
);
_yy += _line;

draw_text(
    _x + 10,
    _yy,
    "Client connected: " + (_connected ? "YES" : "NO")
);
_yy += _line;

if (_connected)
{
    draw_text(
        _x + 10,
        _yy,
        "Client: " + _device_name
    );
    _yy += _line;

    draw_text(
        _x + 10,
        _yy,
        "Address: " + _address
    );
    _yy += _line;

    draw_text(
        _x + 10,
        _yy,
        "Peer pointer: " + _peer_position
    );
    _yy += _line;

    draw_text(
        _x + 10,
        _yy,
        "Buffered RFCOMM bytes: "
        + string(_buffered_bytes)
    );
}