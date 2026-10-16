if (!bt_ready) exit;

if (bluetooth_le_scan_is_running())
{
    bluetooth_le_scan_stop();
}

// A connect still in progress is cancelled, so its callback cannot spawn a
// connection object in the next room.
if (pending_conn != 0)
{
    bluetooth_le_disconnect(pending_conn);
    pending_conn = 0;
}

if (conn != 0)
{
    bluetooth_le_disconnect(conn);
}

bluetooth_remove_callback_device_found();
bluetooth_remove_callback_scan_stopped();
bluetooth_remove_callback_le_characteristic_value_changed();
bluetooth_remove_callback_le_disconnected();
