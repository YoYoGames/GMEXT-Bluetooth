// Placed in rm_bt_classic_server. Toggles Classic discoverability so an
// unpaired client's scan can find this device and its RFCOMM service.

event_inherited();

// How long each request keeps the radio discoverable, in seconds.
duration = 120;

// current_time (ms) at which the current discoverable window ends.
discoverable_until = 0;

// macOS/iOS have no public API; the first start attempt reports NotSupported.
not_supported = false;

// Android cannot end discoverability early, so the button stays locked until
// the window expires on its own.
can_stop = (os_type != os_android);

text = "MAKE DISCOVERABLE";
