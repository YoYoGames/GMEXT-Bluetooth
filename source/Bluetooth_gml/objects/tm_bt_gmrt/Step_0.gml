// ============================================================
// Bluetooth mobile test - Step Event
// ============================================================

if (!bt_ready)
    exit;


// ------------------------------------------------------------
// Dispatch Bluetooth events to GML
// ------------------------------------------------------------

// bluetooth_update() runs 60x/second, so logging every call buries everything
// else in the output. Report only what actually changed since last frame.

var _events_processed =
    bluetooth_update();

if (_events_processed != 0)
{
    show_debug_message(
        $"[GML] bluetooth_update() processed {_events_processed} event(s)"
    );
}

log_tick += 1;


// ------------------------------------------------------------
// Device cache
//
// The device_found callback cannot fire while the native event
// queue is unimplemented, so polling the cache is currently the
// only evidence that discovery is working at all.
// ------------------------------------------------------------

var _device_count =
    bluetooth_device_get_count();

if (_device_count != last_device_count)
{
    show_debug_message(
        $"[GML] *** DEVICE COUNT CHANGED: " +
        $"{last_device_count} -> {_device_count} ***"
    );

    for (var i = last_device_count; i < _device_count; ++i)
    {
        bluetooth_print_device(
            bluetooth_device_get_at(i)
        );
    }

    last_device_count = _device_count;
}


// ------------------------------------------------------------
// Scan state
// ------------------------------------------------------------

var _scanning =
    bluetooth_le_scan_is_running();

if (_scanning != last_scanning)
{
    show_debug_message(
        $"[GML] *** SCAN STATE CHANGED: {last_scanning} -> {_scanning} ***"
    );

    last_scanning = _scanning;
}


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
// Heartbeat
//
// Once a second, so an otherwise silent log can be told apart
// from a hung or crashed extension.
// ------------------------------------------------------------

if (log_tick >= game_get_speed(gamespeed_fps))
{
    log_tick = 0;

    show_debug_message(
        $"[GML] heartbeat - " +
        $"init: {bluetooth_is_initialized()}, " +
        $"perm: {bluetooth_permission_to_string(_permission)}, " +
        $"scanning: {_scanning}, " +
        $"devices: {_device_count}"
    );
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
        show_debug_message("[GML] BUTTON: REQUEST PERMISSION");
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
        show_debug_message(
            $"[GML] BUTTON: START BLE (permission = " +
            $"{bluetooth_permission_to_string(_permission)})"
        );

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
        show_debug_message(
            $"[GML] BUTTON: STOP BLE (running = {_scanning})"
        );

        if (_scanning)
        {
            var _error =
                bluetooth_le_scan_stop();

            show_debug_message(
                $"[GML] bluetooth_le_scan_stop() returned: {_error}"
            );

            if (_error != BluetoothError.Ok)
            {
                show_debug_message(
                    $"[GML] Error: {bluetooth_last_error_code()} - " +
                    $"{bluetooth_last_error_message()}"
                );
            }
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
        show_debug_message(
            $"[GML] BUTTON: CLEAR DEVICES ({_device_count} cached)"
        );

        bluetooth_device_clear();
        last_device_count = 0;
    }
}
