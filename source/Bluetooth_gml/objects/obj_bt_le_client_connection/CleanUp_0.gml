for (var i = 0; i < array_length(rows); i++)
{
    if (instance_exists(rows[i])) instance_destroy(rows[i]);
}

for (var i = 0; i < array_length(buttons); i++)
{
    if (instance_exists(buttons[i])) instance_destroy(buttons[i]);
}

if (instance_exists(discover_button))
{
    instance_destroy(discover_button);
}

if (bluetooth_le_connection_is_connected(connection))
{
    bluetooth_le_disconnect(connection);
}

if (global.ble_conn_inst == id)
{
    global.ble_conn_inst = noone;
    global.ble_conn = 0;
}
