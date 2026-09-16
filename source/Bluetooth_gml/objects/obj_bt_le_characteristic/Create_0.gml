// characteristic, service_uuid arrive via the instance_create_depth struct argument.

uuid = bluetooth_le_characteristic_get_uuid(characteristic);
value_text = "";

function short_uuid(_uuid) {
    return (string_length(_uuid) > 8) ? string_copy(_uuid, 1, 8) + "..." : _uuid;
}

label = short_uuid(service_uuid) + " / " + short_uuid(uuid);
