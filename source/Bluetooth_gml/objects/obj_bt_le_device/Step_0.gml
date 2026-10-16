// Only one connection at a time: lock every device while connecting/connected,
// and hide the list once connected so the GATT view has the screen.
connecting = instance_exists(owner) && owner.pending_conn != 0;
locked = connecting || (instance_exists(owner) && owner.conn != 0);
visible = connecting || !instance_exists(owner) || owner.conn == 0;

event_inherited();

text = connecting ? "CONNECTING..." : label;
