if (!bt_ready || server_started) exit;

if (bluetooth_permission_get_status() == BluetoothPermissionStatus.Granted)
{
    start_classic_server_demo();
}
