/// obj_bt_le_client : Draw

var _x = 16;
var _y = 400;
var _line = 17;


// -----------------------------------------------------------------------------
// Permission
// -----------------------------------------------------------------------------

// Step takes the scan and connection state (refresh_status); Draw only draws.
var _permission_text = "Unknown";

switch (obj_bt_init.bt_permission_status)
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

var _scan_running = status_scanning;


// -----------------------------------------------------------------------------
// Connection state
// -----------------------------------------------------------------------------

var _conn = conn;

var _connected = status_connected;
var _device_name = status_device_name;
var _device_address = status_device_address;
var _device_id = status_device_id;
var _rssi = status_rssi;


// -----------------------------------------------------------------------------
// GATT UI state
// -----------------------------------------------------------------------------

var _rows_count = 0;
var _button_count = 0;

if (instance_exists(conn_inst))
{
    _rows_count = array_length(conn_inst.rows);
    _button_count = array_length(conn_inst.buttons);
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

if (unavailable_reason != "")
{
    draw_text(_x, _y, unavailable_reason);
    _y += _line * 2;
}

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