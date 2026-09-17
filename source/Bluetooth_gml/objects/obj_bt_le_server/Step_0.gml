if (!bt_ready || startup_requested) exit;

if (bluetooth_permission_get_status() == BluetoothPermissionStatus.Granted)
{
    start_le_server_demo();
}
