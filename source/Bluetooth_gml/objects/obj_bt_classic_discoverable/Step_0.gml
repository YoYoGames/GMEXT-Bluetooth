var _running = bluetooth_classic_discoverable_is_running();

locked = not_supported || (_running && !can_stop);

event_inherited();

if (not_supported)
{
    text = "DISCOVERABLE: NOT SUPPORTED";
}
else if (_running)
{
    var _seconds_left = max(0, ceil((discoverable_until - current_time) / 1000));
    text = (can_stop ? "STOP DISCOVERABLE" : "DISCOVERABLE") + " (" + string(_seconds_left) + "s)";
}
else
{
    text = "MAKE DISCOVERABLE";
}
