if (instance_exists(owner) && owner.conn_inst == id)
{
    owner.conn_inst = noone;
    owner.conn = 0;
}
