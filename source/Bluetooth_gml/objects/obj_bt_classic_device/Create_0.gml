// Inherit the parent event
event_inherited();

address = bluetooth_device_has_address(device)
    ? bluetooth_device_get_address(device)
    : "<no address>";

pairing_supported = bluetooth_pairing_is_supported(device);
paired = bluetooth_device_is_paired(device);

function update_text()
{
    var _suffix = "";

    if (paired)
    {
        _suffix = " [paired]";
    }
    else if (pairing_supported)
    {
        _suffix = " (right-click to pair)";
    }

    text = bluetooth_device_get_name(device) + " - " + address + _suffix;
}

update_text();
