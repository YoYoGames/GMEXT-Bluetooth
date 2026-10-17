// device and owner (obj_bt_classic_client) arrive via the creation struct.
event_inherited();

pairing_supported = bluetooth_pairing_is_supported(device);
paired = bluetooth_device_is_paired(device);

// Reads the name and address each time: a scan can resolve the name later.
function update_text() {
	var _address = bluetooth_device_get_address(device);
	if (is_undefined(_address)) {
		_address = "<no address>";
	}

	var _suffix = "";

	if (paired) {
		_suffix = " [paired]";
	} else if (pairing_supported) {
		_suffix = " (right-click to pair)";
	}

	text = $"{bluetooth_device_get_name(device)} - {_address}{_suffix}";
}

update_text();
