
if (locked) exit;

bt_le_characteristic_unsubscribe(global.address, global.serviceUuid, string(global.characteristic.uuid));