if (bluetooth_classic_connection_is_connected(connection))
{
    bluetooth_classic_disconnect(connection);
}

buffer_delete(recv_buffer);

if (instance_exists(owner) && owner.conn_inst == id)
{
    owner.conn_inst = noone;
    owner.conn = 0;
}
