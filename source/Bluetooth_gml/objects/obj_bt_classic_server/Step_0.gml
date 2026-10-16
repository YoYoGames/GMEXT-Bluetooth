refresh_status();

if (!bt_ready || server_started) exit;

if (global.bt_permission_status == BluetoothPermissionStatus.Granted)
{
    start_classic_server_demo();
}
