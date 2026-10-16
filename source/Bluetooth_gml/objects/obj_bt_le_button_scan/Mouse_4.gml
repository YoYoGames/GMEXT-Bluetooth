if (locked) exit;
if (!instance_exists(owner)) exit;

if (bluetooth_le_scan_is_running())
{
    var _r = bluetooth_le_scan_stop();
    show_debug_message($"[GML] bluetooth_le_scan_stop() = {_r}");
    if (_r != BluetoothError.Ok)
    {
        show_debug_message($"[GML] le scan stop failed: {bluetooth_last_error_message()}");
    }
}
else
{
    owner.bluetooth_start_le_scan();
}
