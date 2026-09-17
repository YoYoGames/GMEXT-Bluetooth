/// obj_bt_init : Draw GUI

var _gw = display_get_gui_width();
var _gh = display_get_gui_height();

var _panel_w = min(520, floor(_gw * 0.48));
var _x = _gw - _panel_w - 16;
var _y = 16;
var _line_h = 18;
var _pad = 12;
var _scale = 0.72;


// -----------------------------------------------------------------------------
// Refresh values that can change while the game is running.
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
// Optional active handles.
// -----------------------------------------------------------------------------

var _ble_conn = 0;
var _ble_server_conn = 0;
var _classic_conn = 0;

if (variable_global_exists("ble_conn"))
{
    _ble_conn = global.ble_conn;
}

if (variable_global_exists("ble_server_conn"))
{
    _ble_server_conn = global.ble_server_conn;
}

if (variable_global_exists("classic_conn"))
{
    _classic_conn = global.classic_conn;
}


// -----------------------------------------------------------------------------
// Client connection state.
// -----------------------------------------------------------------------------

var _ble_conn_valid = false;
var _ble_connected = false;

if (_initialized && _ble_conn != 0)
{
    _ble_conn_valid = bluetooth_le_connection_is_valid(_ble_conn);

    if (_ble_conn_valid)
    {
        _ble_connected = bluetooth_le_connection_is_connected(_ble_conn);
    }
}


var _classic_conn_valid = false;
var _classic_connected = false;

if (_initialized && _classic_conn != 0)
{
    _classic_conn_valid = bluetooth_classic_connection_is_valid(_classic_conn);

    if (_classic_conn_valid)
    {
        _classic_connected =
            bluetooth_classic_connection_is_connected(_classic_conn);
    }
}


// -----------------------------------------------------------------------------
// Last extension error.
// -----------------------------------------------------------------------------

var _last_error_code = 0;
var _last_error_message = "";

if (_initialized)
{
    _last_error_code = bluetooth_last_error_code();
    _last_error_message = bluetooth_last_error_message();
}


// -----------------------------------------------------------------------------
// Human-readable values.
// -----------------------------------------------------------------------------

var _permission_text = bt_permission_name(_permission);

var _state_text = bt_state_name(global.bt_state);

var _room_name = room_get_name(room);


// -----------------------------------------------------------------------------
// Build information table.
// -----------------------------------------------------------------------------

var _rows = [
    ["Room", _room_name],
    ["Platform", bt_platform_name()],
    ["Initialized", bt_yes_no(_initialized)],
    ["Permission", _permission_text],
    ["Bluetooth State", _state_text + " (" + string(global.bt_state) + ")"],

    ["", ""],

    ["BLE", bt_yes_no(global.bt_capabilities.le)],
    ["BLE Advertising", bt_yes_no(global.bt_capabilities.le_advertise)],
    ["BLE GATT Server", bt_yes_no(global.bt_capabilities.le_server)],

    ["LE Scan Running", bt_yes_no(_le_scan)],
    ["LE Advertising", bt_yes_no(_le_advertising)],
    ["LE Server Running", bt_yes_no(_le_server_running)],

    ["BLE Client Handle", string(_ble_conn)],
    ["BLE Client Valid", bt_yes_no(_ble_conn_valid)],
    ["BLE Client Connected", bt_yes_no(_ble_connected)],
    ["BLE Server Peer Handle", string(_ble_server_conn)],

    ["", ""],

    ["Classic", bt_yes_no(global.bt_capabilities.classic)],
    ["Classic Server", bt_yes_no(global.bt_capabilities.classic_server)],

    ["Classic Scan Running", bt_yes_no(_classic_scan)],
    ["Classic Server Running", bt_yes_no(_classic_server_running)],
    ["Classic Discoverable", bt_yes_no(_classic_discoverable)],

    ["Classic Conn Handle", string(_classic_conn)],
    ["Classic Conn Valid", bt_yes_no(_classic_conn_valid)],
    ["Classic Connected", bt_yes_no(_classic_connected)],

    ["", ""],

    ["Cached Devices", string(_device_count)],
    ["Pairing", "device-specific"],
    ["Last Error Code", string(_last_error_code)],
    ["Last Error", _last_error_message],
    ["FPS", string(floor(fps_real))],
    ["GUI", string(_gw) + " x " + string(_gh)]
];


// -----------------------------------------------------------------------------
// Panel
// -----------------------------------------------------------------------------

var _panel_h =
    44
    + array_length(_rows) * _line_h;

draw_set_alpha(0.82);
draw_set_color(c_black);

draw_rectangle(
    _x,
    _y,
    _x + _panel_w,
    min(_gh - 8, _y + _panel_h),
    false
);

draw_set_alpha(1);

draw_set_font(fnt_gm_20);
draw_set_halign(fa_left);
draw_set_valign(fa_top);


// Title

draw_set_color(c_aqua);

draw_text_transformed(
    _x + _pad,
    _y + 8,
    "GMBluetooth Runtime",
    _scale,
    _scale,
    0
);


// Rows

var _yy = _y + 34;

for (var i = 0; i < array_length(_rows); i++)
{
    var _label = _rows[i][0];
    var _value = _rows[i][1];

    if (_label == "")
    {
        _yy += 6;
        continue;
    }

    draw_set_color(c_ltgray);

    draw_text_transformed(
        _x + _pad,
        _yy,
        _label + ":",
        _scale,
        _scale,
        0
    );

    draw_set_color(c_white);

    draw_text_transformed(
        _x + 180,
        _yy,
        _value,
        _scale,
        _scale,
        0
    );

    _yy += _line_h;
}