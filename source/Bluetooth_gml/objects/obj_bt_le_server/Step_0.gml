refresh_status();

if (!bt_ready || startup_requested) exit;

if (obj_bt_init.bt_permission_status == BluetoothPermissionStatus.Granted)
{
    start_le_server_demo();
}
