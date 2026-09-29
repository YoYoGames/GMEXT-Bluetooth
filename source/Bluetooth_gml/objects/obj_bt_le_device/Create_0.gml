// device arrives via the creation struct.

event_inherited();

// True between bluetooth_le_connect() and its completion callback.
connecting = false;

var _name = bluetooth_device_get_name(device);

address = bluetooth_device_has_address(device)
    ? bluetooth_device_get_address(device)
    : "<no address>";

label = _name + " - " + address;
text = label;
