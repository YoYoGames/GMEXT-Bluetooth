if (locked) exit;

if (bluetooth_classic_discoverable_is_running())
{
    var _stop = bluetooth_classic_discoverable_stop();
    show_debug_message($"[GML] bluetooth_classic_discoverable_stop() = {_stop}");
    exit;
}

var _r = bluetooth_classic_discoverable_start(duration);
show_debug_message($"[GML] bluetooth_classic_discoverable_start({duration}) = {_r}");

if (_r == BluetoothError.Ok)
{
    discoverable_until = current_time + duration * 1000;
}
else if (_r == BluetoothError.NotSupported)
{
    not_supported = true;
}
else
{
    show_debug_message("[GML] discoverable failed: " + bluetooth_last_error_message());
}
