if (!bt_ready) exit;

if (bluetooth_classic_scan_is_running())
{
    bluetooth_classic_scan_stop();
}

if (global.classic_conn != 0)
{
    bluetooth_classic_disconnect(global.classic_conn);
}

bluetooth_remove_callback_device_found();
bluetooth_remove_callback_scan_stopped();
bluetooth_remove_callback_classic_data();
bluetooth_remove_callback_classic_disconnected();

global.classic_conn = 0;
global.classic_conn_inst = noone;
