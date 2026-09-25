// Inherit the parent event.
event_inherited();

var _name = bluetooth_device_get_name(device);

address = bluetooth_device_has_address(device)
    ? bluetooth_device_get_address(device)
    : "<no address>";

text = _name + " - " + address;
