
// Close last connection this will help with cleaning resources
if (variable_global_exists("address")) {
	bt_le_peripheral_close(global.address);
}

global.address = "";
bt_le_scan_start();

x_offset = room_width * .5;
y_offset = 200;
