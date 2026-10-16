refresh_status();

if (!bt_ready || server_started) exit;

if (obj_bt_init.bt_permission_status == BluetoothPermissionStatus.Granted)
{
    start_classic_server_demo();
}
