/// obj_bt_le_client : Draw GUI

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

var _scan_running = false;

if (bt_ready)
{
    _scan_running = bluetooth_le_scan_is_running();
}


// -----------------------------------------------------------------------------
// Connection state
// -----------------------------------------------------------------------------

var _conn = global.ble_conn;

var _connected = false;
var _device_name = "-";
var _device_address = "-";
var _device_id = "-";
var _rssi = "-";

if (_conn != 0 && bluetooth_le_connection_is_valid(_conn))
{
    _connected = bluetooth_le_connection_is_connected(_conn);

    var _device = bluetooth_le_connection_get_device(_conn);

    if (bluetooth_device_is_valid(_device))
    {
        _device_name = bluetooth_device_get_name(_device);
        _device_id = bluetooth_device_get_id(_device);

        if (bluetooth_device_has_address(_device))
        {
            _device_address = bluetooth_device_get_address(_device);
        }

        if (bluetooth_device_has_rssi(_device))
        {
            _rssi = string(bluetooth_device_get_rssi(_device)) + " dBm";
        }
    }

}


// -----------------------------------------------------------------------------
// GATT UI state
// -----------------------------------------------------------------------------

var _rows_count = 0;
var _button_count = 0;

if (instance_exists(global.ble_conn_inst))
{
    _rows_count = array_length(global.ble_conn_inst.rows);
    _button_count = array_length(global.ble_conn_inst.buttons);
}


// -----------------------------------------------------------------------------
// Draw
// -----------------------------------------------------------------------------

draw_set_font(fnt_gm_15);
draw_set_color(c_white);
draw_set_halign(fa_left);
draw_set_valign(fa_top);

draw_text(_x, _y, "BLE CLIENT");
_y += _line * 2;

draw_text(_x, _y, "Permission: " + _permission_text);
_y += _line;

draw_text(
    _x,
    _y,
    "Scan: " + (_scan_running ? "RUNNING" : "STOPPED")
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
    "Connected: " + (_connected ? "YES" : "NO")
);
_y += _line;


if (_connected)
{
    _y += _line;

    draw_text(_x, _y, "Device: " + _device_name);
    _y += _line;

    draw_text(_x, _y, "Device ID: " + _device_id);
    _y += _line;

    draw_text(_x, _y, "Address: " + _device_address);
    _y += _line;

    draw_text(_x, _y, "RSSI: " + _rssi);
    _y += _line;

    draw_text(
        _x,
        _y,
        "Demo characteristics: "
        + string(_rows_count)
    );
}