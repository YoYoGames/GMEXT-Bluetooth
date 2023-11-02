/// @description Insert description here
// You can write your code in this editor

with (obj_bt_le_descriptor)
{
	instance_destroy();
}

bt_le_characteristic_get_descriptors(global.address, global.serviceUuid, global.characteristic.uuid);

x_offset = room_width * .5;
y_offset = 430;