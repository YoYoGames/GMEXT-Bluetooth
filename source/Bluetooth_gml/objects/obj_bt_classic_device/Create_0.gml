// Inherit the parent event
event_inherited();


address = bluetooth_device_has_address(device) ? bluetooth_device_get_address(device) : "<no address>"

paired = bluetooth_device_is_paired(device);

function update_text() {
    text = bluetooth_device_get_name(device) + " - " + address + (paired ? " [paired]" : " (right-click to pair)")
}

update_text();
