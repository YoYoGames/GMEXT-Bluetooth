
if (locked) exit;

bt_le_characteristic_indicate(global.address, global.serviceUuid, global.characteristic.uuid);