/// obj_bt_init : Draw

var _x = 16;
var _y = 100;
var _line = 17;


// -----------------------------------------------------------------------------
// General Bluetooth state only.
// Feature-specific state belongs to the BLE/Classic client/server objects.
// Step takes the state (refresh_status); Draw only draws it.
// -----------------------------------------------------------------------------
// Draw
// -----------------------------------------------------------------------------

draw_set_font(fnt_gm_15);
draw_set_color(c_white);
draw_set_halign(fa_left);
draw_set_valign(fa_top);


draw_text(_x, _y, "BLUETOOTH");
_y += _line * 2;

draw_text(_x, _y, "Initialized: " + bt_yes_no(status_initialized));
_y += _line;

draw_text(_x, _y, "Permission: " + bt_permission_name(global.bt_permission_status));
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


draw_text(_x, _y, "Last Error Code: " + string(status_error_code));
_y += _line;

draw_text(_x, _y, "Last Error: " + status_error_message);
