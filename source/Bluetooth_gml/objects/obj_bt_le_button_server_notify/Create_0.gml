event_inherited();

// Placed in the room; the demo controller is created before this button
// (see the room's instance creation order).
owner = instance_find(obj_bt_le_server, 0);

// Stay locked if the controller bailed out early (Bluetooth unavailable).
locked = !instance_exists(owner) || !variable_instance_exists(owner, "send_demo_notification");

text = "NOTIFY CLIENT";
