
/// obj_bt_classic_server : Draw GUI

var _x = 16;
var _y = 400;
var _line = 17;


// State: Step takes it (refresh_status); Draw only draws.
var _permission = obj_bt_init.bt_permission_status;
var _running = status_running;
var _discoverable = status_discoverable;

var _conn = conn;
var _connected = status_connected;
var _device_name = status_device_name;
var _address = status_device_address;


// Permission text
var _permission_text = "Unknown";

if (_permission == BluetoothPermissionStatus.Granted)
{
    _permission_text = "Granted";
}
else if (_permission == BluetoothPermissionStatus.Denied)
{
    _permission_text = "Denied";
}


// Peer data
var _peer_position = "-";
var _buffered_bytes = 0;

if (instance_exists(conn_inst))
{
    _buffered_bytes = buffer_get_used_size(conn_inst.recv_buffer);

    if (conn_inst.has_remote)
    {
        _peer_position =
            string(conn_inst.remote_x)
            + ", "
            + string(conn_inst.remote_y);
    }
}


// Draw
draw_set_font(fnt_gm_15);
draw_set_valign(fa_top)
draw_set_halign(fa_left)
draw_set_color(c_white);

draw_text(_x, _y, "CLASSIC RFCOMM SERVER");
_y += _line * 2;

if (unavailable_reason != "")
{
    draw_text(_x, _y, unavailable_reason);
    _y += _line * 2;
}

draw_text(_x, _y, "Permission: " + _permission_text);
_y += _line;

draw_text(_x, _y, "Server: " + (_running ? "RUNNING" : "STOPPED"));
_y += _line;

draw_text(_x, _y, "Discoverable: " + (_discoverable ? "YES" : "NO"));
_y += _line;

draw_text(_x, _y, "Service: " + DEMO_CLASSIC_SERVICE_NAME);
_y += _line;

draw_text(_x, _y, "UUID: " + DEMO_CLASSIC_SERVICE_UUID);
_y += _line * 2;

draw_text(_x, _y, "Connection: " + string(_conn));
_y += _line;

draw_text(_x, _y, "Connected: " + (_connected ? "YES" : "NO"));
_y += _line;


if (_connected)
{
    _y += _line;

    draw_text(_x, _y, "Client: " + _device_name);
    _y += _line;

    draw_text(_x, _y, "Address: " + _address);
    _y += _line;

    draw_text(_x, _y, "Peer pointer: " + _peer_position);
    _y += _line;

    draw_text(_x, _y, "Buffered bytes: " + string(_buffered_bytes));
}
