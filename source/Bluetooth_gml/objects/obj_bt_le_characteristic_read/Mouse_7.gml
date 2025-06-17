
if (locked) exit;

bt_le_characteristic_read(global.address, global.serviceUuid, string(global.characteristic.uuid));