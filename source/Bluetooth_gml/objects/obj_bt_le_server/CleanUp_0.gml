for (var i = 0; i < array_length(control_buttons); i++)
{
    if (instance_exists(control_buttons[i])) instance_destroy(control_buttons[i]);
}

if (!bt_ready) exit;

if (bluetooth_le_advertise_is_running())
{
    bluetooth_le_advertise_stop();
}

if (server_started)
{
    bluetooth_le_server_clear_services();
    bluetooth_le_server_stop();
}

bluetooth_remove_callback_le_server_connection_state_changed();
bluetooth_remove_callback_le_server_write_request();
bluetooth_remove_callback_le_server_read_request();

global.ble_server_conn = 0;
global.ble_server_conn_inst = noone;
