if (!bt_ready) exit;

var _permission = bluetooth_permission_get_status();
if (auto_scan_after_permission && _permission == BluetoothPermissionStatus.Granted) {
    auto_scan_after_permission = false;
    bluetooth_start_le_scan();
}
last_permission_status = _permission;
