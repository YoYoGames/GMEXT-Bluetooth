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

if (instance_exists(disconnect_button))
{
    instance_destroy(disconnect_button);
}

if (bluetooth_le_connection_is_connected(connection))
{
    bluetooth_le_disconnect(connection);
}

if (instance_exists(owner) && owner.conn_inst == id)
{
    owner.conn_inst = noone;
    owner.conn = 0;
}
