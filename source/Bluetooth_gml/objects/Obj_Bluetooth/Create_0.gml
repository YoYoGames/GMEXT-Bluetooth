
#macro ANDROID_BLUETOOTH_UUID "7CBCDDF1-21B8-4F3F-A37B-4B8E044DC1D5"
#macro MICROCONTROLLER_BLUETOOTH_UUID "00001101-0000-1000-8000-00805F9B34FB"

if (os_type != os_android) {
	instance_destroy();
	return;
}

os_powersave_enable(false);
draw_set_color(c_black)

// If bluetooth is supported
if(bt_classic_is_supported())
{
	// Check if bluetooth is enable
	if(!bt_is_enabled()) {
		// If not request to enable it
		bt_request_enable()
	}
	
	// Check if we need to request dynamic permissions
	if (!os_check_permission("android.permission.ACCESS_FINE_LOCATION") || !os_check_permission("android.permission.BLUETOOTH_SCAN") || !os_check_permission("android.permission.BLUETOOTH_CONNECT") || !os_check_permission("android.permission.BLUETOOTH_ADVERTISE")) {
		return os_request_permission("android.permission.ACCESS_FINE_LOCATION", "android.permission.BLUETOOTH_SCAN", "android.permission.BLUETOOTH_CONNECT", "android.permission.BLUETOOTH_ADVERTISE");
	}
	
} else show_message_async("Bluetooth Not Supported")