// characteristic, service_uuid and properties arrive through the creation struct.

uuid = bluetooth_le_characteristic_get_uuid(characteristic);

if (!variable_instance_exists(id, "properties"))
{
    properties = bluetooth_le_characteristic_get_properties(characteristic);
}

value_text = "";
status_text = "Ready";

name = "Characteristic";

if (string_lower(uuid) == string_lower(DEMO_CHAR_RX_UUID))
    name = "RX (client -> server)";
else if (string_lower(uuid) == string_lower(DEMO_CHAR_TX_UUID))
    name = "TX (server -> client)";
else if (string_lower(uuid) == string_lower(DEMO_CHAR_INFO_UUID))
    name = "INFO";

function short_uuid(_uuid)
{
    return (string_length(_uuid) > 13)
        ? string_copy(_uuid, 1, 8) + "..."
        : _uuid;
}

function property_names(_properties)
{
    var _s = "";

    if (_properties & BluetoothLeCharacteristicProperty.Read)
        _s += (_s == "" ? "" : " | ") + "READ";

    if (_properties & BluetoothLeCharacteristicProperty.Write)
        _s += (_s == "" ? "" : " | ") + "WRITE";

    if (_properties & BluetoothLeCharacteristicProperty.WriteWithoutResponse)
        _s += (_s == "" ? "" : " | ") + "WRITE NO RESP";

    if (_properties & BluetoothLeCharacteristicProperty.Notify)
        _s += (_s == "" ? "" : " | ") + "NOTIFY";

    if (_properties & BluetoothLeCharacteristicProperty.Indicate)
        _s += (_s == "" ? "" : " | ") + "INDICATE";

    return (_s == "") ? "NONE" : _s;
}

label = short_uuid(service_uuid) + " / " + short_uuid(uuid);
properties_text = property_names(properties);
