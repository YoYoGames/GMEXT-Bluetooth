/// obj_bt_init : Draw GUI

var _x = 16;
var _y = 100;
var _line_h = 17;


// -----------------------------------------------------------------------------
// Runtime values
// -----------------------------------------------------------------------------

var _initialized = bluetooth_is_initialized();

var _permission = BluetoothPermissionStatus.Unknown;
var _device_count = 0;

var _le_scan = false;
var _le_advertising = false;
var _le_server_running = false;

var _classic_scan = false;
var _classic_server_running = false;
var _classic_discoverable = false;


if (_initialized)
{
    _permission = bluetooth_permission_get_status();
    global.bt_permission_status = _permission;

    _device_count = bluetooth_device_get_count();

    if (global.bt_capabilities.le)
    {
        _le_scan = bluetooth_le_scan_is_running();
    }

    if (global.bt_capabilities.le_advertise)
    {
        _le_advertising = bluetooth_le_advertise_is_running();
    }

    if (global.bt_capabilities.le_server)
    {
        _le_server_running = bluetooth_le_server_is_running();
    }

    if (global.bt_capabilities.classic)
    {
        _classic_scan = bluetooth_classic_scan_is_running();
        _classic_discoverable = bluetooth_classic_discoverable_is_running();
    }

    if (global.bt_capabilities.classic_server)
    {
        _classic_server_running = bluetooth_classic_server_is_running();
    }
}


// -----------------------------------------------------------------------------
// Connection handles
// -----------------------------------------------------------------------------

var _ble_conn = variable_global_exists("ble_conn")
    ? global.ble_conn
    : 0;

var _ble_server_conn = variable_global_exists("ble_server_conn")
    ? global.ble_server_conn
    : 0;

var _classic_conn = variable_global_exists("classic_conn")
    ? global.classic_conn
    : 0;


// -----------------------------------------------------------------------------
// Connection states
// -----------------------------------------------------------------------------

var _ble_valid = false;
var _ble_connected = false;

if (_initialized && _ble_conn != 0)
{
    _ble_valid = bluetooth_le_connection_is_valid(_ble_conn);

    if (_ble_valid)
    {
        _ble_connected = bluetooth_le_connection_is_connected(_ble_conn);
    }
}


var _classic_valid = false;
var _classic_connected = false;

if (_initialized && _classic_conn != 0)
{
    _classic_valid = bluetooth_classic_connection_is_valid(_classic_conn);

    if (_classic_valid)
    {
        _classic_connected =
            bluetooth_classic_connection_is_connected(_classic_conn);
    }
}


// -----------------------------------------------------------------------------
// Error
// -----------------------------------------------------------------------------

var _error_code = 0;
var _error_message = "";

if (_initialized)
{
    _error_code = bluetooth_last_error_code();
    _error_message = bluetooth_last_error_message();
}


// -----------------------------------------------------------------------------
// Draw
// -----------------------------------------------------------------------------

draw_set_font(fnt_gm_15);
draw_set_color(c_white);
draw_set_halign(fa_left);
draw_set_valign(fa_top);



draw_text(_x, _y, "Initialized: " + bt_yes_no(_initialized));
_y += _line_h;

draw_text(_x, _y, "Permission: " + bt_permission_name(_permission));
_y += _line_h;

draw_text(
    _x,
    _y,
    "Bluetooth State: "
        + bt_state_name(global.bt_state)
        + " ("
        + string(global.bt_state)
        + ")"
);
_y += _line_h * 2;


// BLE

draw_text(_x, _y, "BLE: " + bt_yes_no(global.bt_capabilities.le));
_y += _line_h;

draw_text(
    _x,
    _y,
    "BLE Advertising Support: "
        + bt_yes_no(global.bt_capabilities.le_advertise)
);
_y += _line_h;

draw_text(
    _x,
    _y,
    "BLE Server Support: "
        + bt_yes_no(global.bt_capabilities.le_server)
);
_y += _line_h;

draw_text(_x, _y, "LE Scan Running: " + bt_yes_no(_le_scan));
_y += _line_h;

draw_text(_x, _y, "LE Advertising: " + bt_yes_no(_le_advertising));
_y += _line_h;

draw_text(_x, _y, "LE Server Running: " + bt_yes_no(_le_server_running));
_y += _line_h;

draw_text(_x, _y, "BLE Client Handle: " + string(_ble_conn));
_y += _line_h;

draw_text(_x, _y, "BLE Client Valid: " + bt_yes_no(_ble_valid));
_y += _line_h;

draw_text(_x, _y, "BLE Client Connected: " + bt_yes_no(_ble_connected));
_y += _line_h;

draw_text(_x, _y, "BLE Server Peer Handle: " + string(_ble_server_conn));
_y += _line_h * 2;


// Classic

draw_text(
    _x,
    _y,
    "Classic: " + bt_yes_no(global.bt_capabilities.classic)
);
_y += _line_h;

draw_text(
    _x,
    _y,
    "Classic Server: "
        + bt_yes_no(global.bt_capabilities.classic_server)
);
_y += _line_h;

draw_text(_x, _y, "Classic Scan Running: " + bt_yes_no(_classic_scan));
_y += _line_h;

draw_text(
    _x,
    _y,
    "Classic Server Running: "
        + bt_yes_no(_classic_server_running)
);
_y += _line_h;

draw_text(
    _x,
    _y,
    "Classic Discoverable: "
        + bt_yes_no(_classic_discoverable)
);
_y += _line_h;

draw_text(_x, _y, "Classic Handle: " + string(_classic_conn));
_y += _line_h;

draw_text(_x, _y, "Classic Valid: " + bt_yes_no(_classic_valid));
_y += _line_h;

draw_text(_x, _y, "Classic Connected: " + bt_yes_no(_classic_connected));
_y += _line_h * 2;


// Misc

draw_text(_x, _y, "Cached Devices: " + string(_device_count));
_y += _line_h;

draw_text(_x, _y, "Last Error Code: " + string(_error_code));
_y += _line_h;

draw_text(_x, _y, "Last Error: " + _error_message);
_y += _line_h;



