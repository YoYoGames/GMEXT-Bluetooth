if (!bt_ready) exit;

// What the game stops fires no event, and a callback already queued finds
// callback_ctx.owner gone and does nothing (a value it carries is released).
// The accepted link stays open after a server stop; its connection object
// closes it in its own Clean Up.
if (bluetooth_classic_discoverable_is_running() && bluetooth_feature_is_supported(BluetoothFeature.ClassicDiscoverableStop)) {
	bluetooth_classic_discoverable_stop();
}

if (server_started) {
	bluetooth_classic_server_stop();
}
