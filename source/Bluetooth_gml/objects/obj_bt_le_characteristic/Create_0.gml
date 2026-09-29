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

properties_text = ble_property_names(properties);
