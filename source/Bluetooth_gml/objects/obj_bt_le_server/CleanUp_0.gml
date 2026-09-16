if (!bt_ready) exit;

bluetooth_le_advertise_stop();
bluetooth_le_server_clear_services();
bluetooth_le_server_stop();
