/// obj_bt_le_server : Draw GUI

var _x = room_width-16;
var _y = 100;
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
// Server state
// -----------------------------------------------------------------------------

var _server_running = false;
var _advertising = false;

if (bt_ready)
{
    _server_running = bluetooth_le_server_is_running();
    _advertising = bluetooth_le_advertise_is_running();
}


// -----------------------------------------------------------------------------
// Peer state
// -----------------------------------------------------------------------------

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


// -----------------------------------------------------------------------------
// Draw
// -----------------------------------------------------------------------------

draw_set_font(fnt_gm_15);
draw_set_color(c_white);
draw_set_halign(fa_right);
draw_set_valign(fa_top);


draw_text(_x, _y, "BLE GATT SERVER");
_y += _line * 2;

draw_text(_x, _y, "Permission: " + _permission_text);
_y += _line;

draw_text(
    _x,
    _y,
    "Server: " + (_server_running ? "RUNNING" : "STOPPED")
);
_y += _line;

draw_text(
    _x,
    _y,
    "Advertising: " + (_advertising ? "YES" : "NO")
);
_y += _line;

draw_text(
    _x,
    _y,
    "Peer connection: " + string(_peer)
);
_y += _line * 2;


// -----------------------------------------------------------------------------
// Demo GATT service
// -----------------------------------------------------------------------------

draw_text(_x, _y, "Demo GATT Service");
_y += _line;

draw_text(_x, _y, "Service: " + DEMO_SERVICE_UUID);
_y += _line * 2;

draw_text(
    _x,
    _y,
    "RX WRITE: " + DEMO_CHAR_RX_UUID
);
_y += _line;

draw_text(
    _x,
    _y,
    "TX NOTIFY: " + DEMO_CHAR_TX_UUID
);
_y += _line;

draw_text(
    _x,
    _y,
    "INFO READ: 6e400004-b5a3-f393-e0a9-e50e24dcca9e"
);
_y += _line * 2;

draw_text(
    _x,
    _y,
    "READ returns: GMBluetooth BLE Demo - READ characteristic OK"
);
_y += _line;

draw_text(
    _x,
    _y,
    "Last peer event: " + _last_peer_message
);