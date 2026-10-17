refresh_status();

if (!bt_ready) exit;

var _permission = obj_bt_init.bt_permission_status;
if (auto_scan_after_permission && _permission == BluetoothPermissionStatus.Granted) {
	auto_scan_after_permission = false;
	bluetooth_start_le_scan();
}
