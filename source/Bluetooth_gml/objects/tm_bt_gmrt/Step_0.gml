// ============================================================
// Bluetooth mobile test - Step Event
// ============================================================

if (!bt_ready)
    exit;


// ------------------------------------------------------------
// Dispatch Bluetooth events to GML
// ------------------------------------------------------------

var _events_processed = bluetooth_update();
show_debug_message($"[GML] bluetooth_update() called - events processed: {_events_processed}");


// ------------------------------------------------------------
// Permission state
// ------------------------------------------------------------

var _permission =
    bluetooth_permission_get_status();

if (_permission != last_permission_status)
{
    show_debug_message($"[GML] *** PERMISSION CHANGED ***");
    show_debug_message(
        $"[GML] Permission changed: " +
        $"{bluetooth_permission_to_string(last_permission_status)} -> " +
        $"{bluetooth_permission_to_string(_permission)}"
    );

    last_permission_status =
        _permission;
}


// The Android permission dialog is asynchronous.
// Start scanning only after the user actually granted permission.
if (
    auto_scan_after_permission &&
    _permission == BluetoothPermissionStatus.Granted)
{
    show_debug_message("[GML] Auto-starting BLE scan after permission granted");
    auto_scan_after_permission = false;
    permission_request_sent = false;

    bluetooth_start_ble_scan();
}


// ------------------------------------------------------------
// Touch UI
//
// GameMaker maps the first touchscreen contact to device mouse 0.
// No keyboard is required.
// ------------------------------------------------------------

if (device_mouse_check_button_pressed(0, mb_left))
{
    var _mx =
        device_mouse_x_to_gui(0);

    var _my =
        device_mouse_y_to_gui(0);

    var _gui_w =
        display_get_gui_width();

    var _button_w =
        (_gui_w - ui_margin * 2 - ui_gap) * 0.5;

    var _x1 =
        ui_margin;

    var _x2 =
        ui_margin + _button_w + ui_gap;

    var _row1_y =
        186;

    var _row2_y =
        _row1_y + ui_button_h + ui_gap;


    // REQUEST PERMISSION
    if (
        point_in_rectangle(
            _mx,
            _my,
            _x1,
            _row1_y,
            _x1 + _button_w,
            _row1_y + ui_button_h))
    {
        bluetooth_request_permissions();
    }


    // START BLE
    if (
        point_in_rectangle(
            _mx,
            _my,
            _x2,
            _row1_y,
            _x2 + _button_w,
            _row1_y + ui_button_h))
    {
        if (_permission == BluetoothPermissionStatus.Granted)
        {
            bluetooth_start_ble_scan();
        }
        else
        {
            auto_scan_after_permission = true;
            bluetooth_request_permissions();
        }
    }


    // STOP BLE
    if (
        point_in_rectangle(
            _mx,
            _my,
            _x1,
            _row2_y,
            _x1 + _button_w,
            _row2_y + ui_button_h))
    {
        if (bluetooth_le_scan_is_running())
        {
            show_debug_message("Stopping BLE scan...");
            bluetooth_le_scan_stop();
        }
    }


    // CLEAR DEVICES
    if (
        point_in_rectangle(
            _mx,
            _my,
            _x2,
            _row2_y,
            _x2 + _button_w,
            _row2_y + ui_button_h))
    {
        bluetooth_device_clear();

        show_debug_message(
            "Bluetooth device cache cleared."
        );
    }
}
