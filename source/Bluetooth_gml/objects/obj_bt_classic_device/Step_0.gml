// One connect or pairing at a time: lock every device while one is in
// progress. A connected device stays clickable, to switch to another one.
locked = !instance_exists(owner) || owner.pending_conn != 0 || owner.pairing;

event_inherited();
