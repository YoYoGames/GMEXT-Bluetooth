
if (locked) exit;

bt_le_characteristic_unsubscribe(global.address, global.serviceUuid, global.characteristic.uuid);