
bt_le_scan_stop();

if (bt_le_peripheral_is_open(address)) {
	global.address = address;
	room_goto(rm_bt_le_services);
}
else bt_le_peripheral_open(address);
