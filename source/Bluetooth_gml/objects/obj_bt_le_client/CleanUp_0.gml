if (!bt_ready) exit;

if (bluetooth_le_scan_is_running())
{
    bluetooth_le_scan_stop();
}

if (global.ble_conn != 0)
{
    bluetooth_le_disconnect(global.ble_conn);
}

bluetooth_remove_callback_device_found();
bluetooth_remove_callback_scan_stopped();
bluetooth_remove_callback_le_characteristic_value_changed();
bluetooth_remove_callback_le_disconnected();

global.ble_conn = 0;
global.ble_conn_inst = noone;
