event_inherited();

// Poll the real scan state so the label also follows stops that did not come
// from this button (discovery timeout, connecting to a device, etc).
text = bluetooth_classic_scan_is_running() ? "STOP SCAN" : "START SCAN";
