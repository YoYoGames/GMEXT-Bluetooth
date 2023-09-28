/// @description Insert description here
// You can write your code in this editor

with (obj_bt_le_characteristic) 
{
	instance_destroy();
}

global.characteristic = undefined;

bt_le_service_get_characteristics(global.address, global.serviceUuid);

x_offset = room_width * .5;
y_offset = 280;