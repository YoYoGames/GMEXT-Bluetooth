if (!bt_ready) exit;

// What the game stops fires no event, and a callback already queued finds
// callback_ctx.owner gone and does nothing (a value it carries is released).
bluetooth_classic_scan_stop();

// A connect still in progress is cancelled, so its callback cannot spawn a
// connection object in the next room.
if (pending_conn != 0) {
	bluetooth_classic_disconnect(pending_conn);
	pending_conn = 0;
}

if (conn != 0) {
	bluetooth_classic_disconnect(conn);
}
