
var _service = get_service_uuid();
var _characteristic = get_characteristic_notify_uuid();
var _value = base64_encode("This is a new value!!");

bt_le_server_notify_value(_service, _characteristic, _value);