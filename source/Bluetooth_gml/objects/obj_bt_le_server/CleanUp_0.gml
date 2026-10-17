if (!bt_ready) exit;

// Stopping the server also ends advertising and retires every central. What
// the game stops fires no event, and a callback already queued finds
// callback_ctx.owner gone and does nothing.
if (server_started) {
	bluetooth_le_server_stop();
}
