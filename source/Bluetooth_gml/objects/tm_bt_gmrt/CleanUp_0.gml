// ============================================================
// Bluetooth test - Clean Up Event
// ============================================================

if (bluetooth_le_scan_is_running())
{
    bluetooth_le_scan_stop();
}

bluetooth_remove_callback_device_found();
bluetooth_remove_callback_scan_stopped();

bluetooth_shutdown();

show_debug_message("GMBluetooth shutdown.");