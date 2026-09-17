/// obj_bt_le_client : Draw GUI

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


var _scan_running = false;

if (bt_ready)
{
    _scan_running = bluetooth_le_scan_is_running();
}


var _conn = global.ble_conn;

var _connected = false;
var _device_name = "-";
var _device_address = "-";
var _device_id = "-";
var _rssi = "-";
var _service_count = 0;

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

    _service_count = bluetooth_le_service_get_count(_conn);
}


var _rows_count = 0;
var _button_count = 0;

if (instance_exists(global.ble_conn_inst))
{
    _rows_count = array_length(global.ble_conn_inst.rows);
    _button_count = array_length(global.ble_conn_inst.buttons);
}


draw_set_alpha(0.78);
draw_set_color(c_black);
draw_rectangle(_x, _y, _x + _w, _y + 190, false);
draw_set_alpha(1);

draw_set_font(fnt_gm_20);
draw_set_halign(fa_left);
draw_set_valign(fa_top);

draw_set_color(c_aqua);
draw_text(_x + 10, _y + 8, "BLE CLIENT");

draw_set_color(c_white);

var _yy = _y + 34;

draw_text(_x + 10, _yy, "Permission: " + _permission_text);
_yy += _line;

draw_text(
    _x + 10,
    _yy,
    "Scan: " + (_scan_running ? "RUNNING" : "STOPPED")
);
_yy += _line;

draw_text(
    _x + 10,
    _yy,
    "Devices: " + string(array_length(devices))
);
_yy += _line;

draw_text(
    _x + 10,
    _yy,
    "Connection: " + string(_conn)
);
_yy += _line;

draw_text(
    _x + 10,
    _yy,
    "Connected: " + (_connected ? "YES" : "NO")
);
_yy += _line;

if (_connected)
{
    draw_text(_x + 10, _yy, "Device: " + _device_name);
    _yy += _line;

    draw_text(_x + 10, _yy, "RSSI: " + _rssi);
    _yy += _line;

    draw_text(
        _x + 10,
        _yy,
        "GATT: "
        + string(_service_count)
        + " services / "
        + string(_rows_count)
        + " characteristics"
    );
}