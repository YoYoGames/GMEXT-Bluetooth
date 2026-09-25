
/// obj_bt_classic_server : Draw GUI

var _x = 16;
var _y = 400;
var _line = 17;


// State
var _permission = bluetooth_permission_get_status();
var _running = bt_ready && bluetooth_classic_server_is_running();
var _discoverable = bt_ready && bluetooth_classic_discoverable_is_running();

var _conn = global.classic_conn;
var _connected = false;
var _device_name = "-";
var _address = "-";


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


// Connection
if (_conn != 0 && bluetooth_classic_connection_is_valid(_conn))
{
    _connected = bluetooth_classic_connection_is_connected(_conn);

    if (_connected)
    {
        var _device = bluetooth_classic_connection_get_device(_conn);

        if (bluetooth_device_is_valid(_device))
        {
            _device_name = bluetooth_device_get_name(_device);

            if (bluetooth_device_has_address(_device))
            {
                _address = bluetooth_device_get_address(_device);
            }
        }
    }
}


// Peer data
var _peer_position = "-";
var _buffered_bytes = 0;

if (instance_exists(global.classic_conn_inst))
{
    _buffered_bytes = global.classic_conn_inst.recv_buffer_size;

    if (global.classic_conn_inst.has_remote)
    {
        _peer_position =
            string(global.classic_conn_inst.remote_x)
            + ", "
            + string(global.classic_conn_inst.remote_y);
    }
}


// Draw
draw_set_font(fnt_gm_15);
draw_set_valign(fa_top)
draw_set_halign(fa_left)
draw_set_color(c_white);

draw_text(_x, _y, "CLASSIC RFCOMM SERVER");
_y += _line * 2;

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
