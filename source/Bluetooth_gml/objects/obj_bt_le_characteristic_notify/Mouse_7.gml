
if (locked) exit;

bt_le_characteristic_notify(global.address, global.serviceUuid, global.characteristic.uuid);