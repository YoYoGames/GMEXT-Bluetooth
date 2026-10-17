// device arrives via the creation struct.

event_inherited();

// True between bluetooth_le_connect() and its completion callback.
connecting = false;

// Undefined when the platform does not expose it (every LE device on Apple).
address = bluetooth_device_get_address(device);
if (is_undefined(address)) {
	address = "<no address>";
}

label = $"{bluetooth_device_get_name(device)} - {address}";
text = label;
