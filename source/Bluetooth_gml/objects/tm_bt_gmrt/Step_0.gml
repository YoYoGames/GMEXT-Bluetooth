// ============================================================
// Bluetooth test - Step Event
// ============================================================

// Dispatch native Bluetooth events to GML.
var _events =
    bluetooth_update();


// ------------------------------------------------------------
// SPACE = start/restart active BLE scan
// ------------------------------------------------------------

if (keyboard_check_pressed(vk_space))
{
    if (!bluetooth_le_scan_is_running())
    {
        show_debug_message("");
        show_debug_message("Starting BLE scan...");

        var _error =
            bluetooth_le_scan_start(true);

        if (_error != BluetoothError.Ok)
        {
            show_debug_message(
                $"ERROR: {bluetooth_last_error_message()}"
            );
        }
    }
}


// ------------------------------------------------------------
// S = stop scan
// ------------------------------------------------------------

if (keyboard_check_pressed(ord("S")))
{
    if (bluetooth_le_scan_is_running())
    {
        show_debug_message("Stopping BLE scan...");

        bluetooth_le_scan_stop();
    }
}


// ------------------------------------------------------------
// P = print all discovered devices
// ------------------------------------------------------------

if (keyboard_check_pressed(ord("P")))
{
    bluetooth_print_all_devices();
}


// ------------------------------------------------------------
// C = clear discovered-device cache
// ------------------------------------------------------------

if (keyboard_check_pressed(ord("C")))
{
    bluetooth_device_clear();

    show_debug_message(
        "Bluetooth device cache cleared."
    );
}