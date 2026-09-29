if (locked) exit;
if (!instance_exists(owner)) exit;

owner.log_msg("disconnecting...");

var _r = bluetooth_le_disconnect(owner.connection);

if (_r != BluetoothError.Ok)
{
    owner.log_msg("disconnect failed: " + bluetooth_last_error_message());
    exit;
}

// Not every backend fires le_disconnected for a disconnect we asked for, so
// tear the connection UI down here. Its CleanUp resets global.ble_conn, which
// unlocks the device list again. A late le_disconnected is then a no-op.
show_debug_message("[GML] disconnected by user");
instance_destroy(owner);
