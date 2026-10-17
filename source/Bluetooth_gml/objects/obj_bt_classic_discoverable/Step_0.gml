var _running = bluetooth_classic_discoverable_is_running();

locked = !can_start || starting || (_running && !can_stop);

event_inherited();

if (!can_start) {
	text = "DISCOVERABLE: NOT SUPPORTED";
} else if (starting) {
	text = "STARTING...";
} else if (_running) {
	var _seconds_left = max(0, ceil((discoverable_until - current_time) / 1000));
	var _label = can_stop ? "STOP DISCOVERABLE" : "DISCOVERABLE";
	text = $"{_label} ({_seconds_left}s)";
} else {
	text = "MAKE DISCOVERABLE";
}
