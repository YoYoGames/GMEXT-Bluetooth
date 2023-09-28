
// feather ignore GM1063
// feather ignore GM2017

service_data = {
	uuid: get_service_uuid(),
	characteristics:
	[
		{ 
			uuid: get_characteristic_write_uuid(), 
			properties: BT_PROPERTY_WRITE, 
			permissions: BT_PERMISSION_WRITE,
		},
		{ 
			uuid: get_characteristic_notify_uuid(),
			properties: BT_PROPERTY_READ | BT_PROPERTY_NOTIFY,
			permissions: BT_PERMISSION_READ,				
		}
	]
}

advertise_settings = {
	advertiseMode: BT_ADVERTISE_MODE_BALANCED,
	connectable: true,
	discoverable: true,
	txPowerLevel: BT_ADVERTISE_TX_POWER_MEDIUM
};

advertise_data = {
	includeName: true,
	includePowerLevel: true
};

isMacOs = os_type == os_macosx;

// If we are on Android device we need to stringify the struct
var _service = (platform_mobile() || isMacOs) ? json_stringify(service_data) : service_data;

bt_le_server_open();

bt_le_server_add_service(_service);