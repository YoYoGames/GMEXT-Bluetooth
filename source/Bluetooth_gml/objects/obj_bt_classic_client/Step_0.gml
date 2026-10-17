refresh_status();

if (!bt_ready) exit;

var _permission = obj_bt_init.bt_permission_status;

if (auto_scan_after_permission && _permission == BluetoothPermissionStatus.Granted) {
	auto_scan_after_permission = false;
	bluetooth_start_classic_scan();
}

// Space = send (desktop convenience only; mobile uses the on-screen controls).
if (keyboard_check_pressed(vk_space) && conn != 0) {
	send_dialog_id = get_string_async("Message to send", "");
}
