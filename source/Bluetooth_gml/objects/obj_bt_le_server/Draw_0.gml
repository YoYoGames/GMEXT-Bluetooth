
/// obj_bt_le_server : Draw GUI

var _x = 16;
var _y = 400;
var _line = 17;


// State
var _permission = bluetooth_permission_get_status();
var _server_running = bt_ready && bluetooth_le_server_is_running();
var _advertising = bt_ready && bluetooth_le_advertise_is_running();


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


// Last peer event
var _last_peer_message = "-";

if (instance_exists(global.ble_server_conn_inst))
{
    var _logs = global.ble_server_conn_inst.log_lines;

    if (array_length(_logs) > 0)
    {
        _last_peer_message = _logs[array_length(_logs) - 1];
    }
}


// Draw
draw_set_font(fnt_gm_15);
draw_set_valign(fa_top)
draw_set_halign(fa_left)
draw_set_color(c_white);

draw_text(_x, _y, "BLE GATT SERVER");
_y += _line * 2;

draw_text(_x, _y, "Permission: " + _permission_text);
_y += _line;

draw_text(_x, _y, "Server: " + (_server_running ? "RUNNING" : "STOPPED"));
_y += _line;

draw_text(_x, _y, "Advertising: " + (_advertising ? "YES" : "NO"));
_y += _line;

draw_text(_x, _y, "Notify target: all subscribed clients");
_y += _line * 2;

draw_text(_x, _y, "Service: " + DEMO_SERVICE_UUID);
_y += _line;

draw_text(_x, _y, "RX WRITE: " + DEMO_CHAR_RX_UUID);
_y += _line;

draw_text(_x, _y, "TX NOTIFY: " + DEMO_CHAR_TX_UUID);
_y += _line;

draw_text(_x, _y, "INFO READ: " + DEMO_CHAR_INFO_UUID);
_y += _line * 2;

draw_text(_x, _y, "INFO value: " + server_info_text);
_y += _line;

draw_text(_x, _y, "Last RX write: " + last_rx_text);
_y += _line;

draw_text(_x, _y, "Last TX notify: " + last_notify_text);
_y += _line;

draw_text(_x, _y, "Last event: " + last_server_event);
