/// obj_bt_init : Draw GUI

var _x = 16;
var _y = 100;
var _line = 17;


// -----------------------------------------------------------------------------
// General Bluetooth state only.
// Feature-specific state belongs to the BLE/Classic client/server objects.
// -----------------------------------------------------------------------------

var _initialized = bluetooth_is_initialized();
var _permission = BluetoothPermissionStatus.Unknown;
var _error_code = 0;
var _error_message = "";

if (_initialized)
{
    _permission = bluetooth_permission_get_status();
    global.bt_permission_status = _permission;

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


draw_text(_x, _y, "BLUETOOTH");
_y += _line * 2;

draw_text(_x, _y, "Initialized: " + bt_yes_no(_initialized));
_y += _line;

draw_text(_x, _y, "Permission: " + bt_permission_name(_permission));
_y += _line;

draw_text(
    _x,
    _y,
    "State: "
        + bt_state_name(global.bt_state)
        + " ("
        + string(global.bt_state)
        + ")"
);
_y += _line * 2;


draw_text(_x, _y, "BLE: " + bt_yes_no(global.bt_capabilities.le));
_y += _line;

draw_text(
    _x,
    _y,
    "BLE Advertising: " + bt_yes_no(global.bt_capabilities.le_advertise)
);
_y += _line;

draw_text(
    _x,
    _y,
    "BLE Server: " + bt_yes_no(global.bt_capabilities.le_server)
);
_y += _line;

draw_text(_x, _y, "Classic: " + bt_yes_no(global.bt_capabilities.classic));
_y += _line;

draw_text(
    _x,
    _y,
    "Classic Server: " + bt_yes_no(global.bt_capabilities.classic_server)
);
_y += _line * 2;


draw_text(_x, _y, "Last Error Code: " + string(_error_code));
_y += _line;

draw_text(_x, _y, "Last Error: " + _error_message);
