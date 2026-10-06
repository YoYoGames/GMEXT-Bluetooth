if (!bt_ready) exit;

var _permission = bluetooth_permission_get_status();
if (auto_scan_after_permission && _permission == BluetoothPermissionStatus.Granted) {
    auto_scan_after_permission = false;
    bluetooth_start_le_scan();
}

// A device first seen without a name gets its button once a later
// advertisement or scan response names it (device_found fires only once).
for (var _i = array_length(unnamed_devices) - 1; _i >= 0; _i--)
{
    var _device = unnamed_devices[_i];
    if (!bluetooth_device_is_valid(_device))
    {
        array_delete(unnamed_devices, _i, 1);
    }
    else if (bluetooth_device_get_name(_device) != "")
    {
        array_delete(unnamed_devices, _i, 1);
        add_device_button(_device);
    }
}
