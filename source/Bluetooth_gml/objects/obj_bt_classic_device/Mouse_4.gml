if (locked) exit;
if (!instance_exists(owner)) exit;

show_debug_message("Connecting");

var _stop = bluetooth_classic_scan_stop();

if (_stop != BluetoothError.Ok)
{
    show_debug_message("[GML] classic scan stop failed: " + bluetooth_last_error_message());
}

// The controller owns the connection: it closes the current one first and
// keeps the pending handle, so leaving the room cancels the connect.
if (bluetooth_device_is_paired(device) || !bluetooth_pairing_is_supported(device))
{
    owner.connect_device(device);
}
else
{
    owner.pair_device(device, true);
}
