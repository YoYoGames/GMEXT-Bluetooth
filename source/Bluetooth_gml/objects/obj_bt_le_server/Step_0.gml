refresh_status();

if (!bt_ready || startup_requested) exit;

if (global.bt_permission_status == BluetoothPermissionStatus.Granted)
{
    start_le_server_demo();
}
