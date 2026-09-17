if (!bt_ready) exit;

var _permission = bluetooth_permission_get_status();

if (auto_scan_after_permission && _permission == BluetoothPermissionStatus.Granted)
{
    auto_scan_after_permission = false;
    bluetooth_start_classic_scan();
}

last_permission_status = _permission;

// Space = send (desktop convenience only; mobile uses the on-screen controls).
if (keyboard_check_pressed(vk_space) && global.classic_conn != 0)
{
    send_dialog_id = get_string_async("Message to send", "");
}
