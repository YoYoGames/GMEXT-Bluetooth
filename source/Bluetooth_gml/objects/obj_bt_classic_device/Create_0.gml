// Inherit the parent event
event_inherited();


address = bluetooth_device_has_address(device) ? bluetooth_device_get_address(device) : "<no address>"

text = bluetooth_device_get_name(device) + " - " + address

