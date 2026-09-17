if (!bt_ready) exit;

if (bluetooth_classic_discoverable_is_running())
{
    bluetooth_classic_discoverable_stop();
}

if (server_started)
{
    bluetooth_classic_server_stop();
}

bluetooth_remove_callback_classic_client_connected();
bluetooth_remove_callback_classic_data();
bluetooth_remove_callback_classic_disconnected();

global.classic_conn = 0;
global.classic_conn_inst = noone;
