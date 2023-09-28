
if (locked) exit;

bt_le_characteristic_read(global.address, global.serviceUuid, global.characteristic.uuid);