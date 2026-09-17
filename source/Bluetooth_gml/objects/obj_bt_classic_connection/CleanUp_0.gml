if (bluetooth_classic_connection_is_connected(connection))
{
    bluetooth_classic_disconnect(connection);
}

buffer_delete(recv_buffer);

if (global.classic_conn_inst == id)
{
    global.classic_conn_inst = noone;
    global.classic_conn = 0;
}
