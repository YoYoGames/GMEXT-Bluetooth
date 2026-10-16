if (locked) exit;
if (!instance_exists(owner)) exit;

show_debug_message("[GML] connecting to " + label);

var _stop = bluetooth_le_scan_stop();

if (_stop != BluetoothError.Ok)
{
    show_debug_message("[GML] le scan stop failed: " + bluetooth_last_error_message());
}

// The controller owns the pending handle, so leaving the room cancels the
// connect instead of letting its callback spawn a connection elsewhere.
owner.connect_device(device);
