owner = instance_find(obj_bt_classic_client, 0);

// Stay locked if the controller bailed out early (Bluetooth unavailable).
locked = !instance_exists(owner) || !owner.bt_ready;
