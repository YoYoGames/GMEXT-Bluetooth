/// obj_bt_le_server : Draw GUI

var _x = 16;
var _y = 16;
var _w = 430;
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


var _server_running = false;
var _advertising = false;

if (bt_ready)
{
    _server_running = bluetooth_le_server_is_running();
    _advertising = bluetooth_le_advertise_is_running();
}


var _peer = global.ble_server_conn;

var _last_peer_message = "-";

if (instance_exists(global.ble_server_conn_inst))
{
    var _logs = global.ble_server_conn_inst.log_lines;

    if (array_length(_logs) > 0)
    {
        _last_peer_message = _logs[array_length(_logs) - 1];
    }
}


draw_set_alpha(0.80);
draw_set_color(c_black);
draw_rectangle(_x, _y, _x + _w, _y + 285, false);
draw_set_alpha(1);

draw_set_font(fnt_gm_20);
draw_set_halign(fa_left);
draw_set_valign(fa_top);

draw_set_color(c_aqua);
draw_text(_x + 10, _y + 8, "BLE GATT SERVER");

draw_set_color(c_white);

var _yy = _y + 36;

draw_text(_x + 10, _yy, "Permission: " + _permission_text);
_yy += _line;

draw_text(
    _x + 10,
    _yy,
    "Server: " + (_server_running ? "RUNNING" : "STOPPED")
);
_yy += _line;

draw_text(
    _x + 10,
    _yy,
    "Advertising: " + (_advertising ? "YES" : "NO")
);
_yy += _line;

draw_text(
    _x + 10,
    _yy,
    "Peer connection: " + string(_peer)
);
_yy += _line + 4;


draw_set_color(c_yellow);
draw_text(_x + 10, _yy, "Demo GATT Service");
_yy += _line;

draw_set_color(c_white);
draw_text(_x + 10, _yy, "Service:");
_yy += _line;

draw_text(_x + 18, _yy, DEMO_SERVICE_UUID);
_yy += _line + 2;


draw_set_color(c_lime);
draw_text(_x + 10, _yy, "RX  WRITE");
draw_set_color(c_white);
draw_text(_x + 145, _yy, DEMO_CHAR_RX_UUID);
_yy += _line;


draw_set_color(c_lime);
draw_text(_x + 10, _yy, "TX  NOTIFY");
draw_set_color(c_white);
draw_text(_x + 145, _yy, DEMO_CHAR_TX_UUID);
_yy += _line;


draw_set_color(c_lime);
draw_text(_x + 10, _yy, "INFO READ");
draw_set_color(c_white);
draw_text(
    _x + 145,
    _yy,
    "6e400004-b5a3-f393-e0a9-e50e24dcca9e"
);
_yy += _line + 4;


draw_set_color(c_ltgray);
draw_text(
    _x + 10,
    _yy,
    "READ returns: GMBluetooth BLE Demo - READ characteristic OK"
);

_yy += _line;

draw_text(
    _x + 10,
    _yy,
    "Last peer event: " + _last_peer_message
);