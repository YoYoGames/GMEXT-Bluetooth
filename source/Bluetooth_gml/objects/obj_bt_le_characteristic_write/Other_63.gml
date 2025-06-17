/// @description Insert description here
// You can write your code in this editor

if (async_load[? "id"] != request_id) exit;
if (async_load[? "status"] != true) exit;

var _data = base64_encode(async_load[? "result"]);

bt_le_characteristic_write_request(global.address, global.serviceUuid, string(global.characteristic.uuid), _data);

