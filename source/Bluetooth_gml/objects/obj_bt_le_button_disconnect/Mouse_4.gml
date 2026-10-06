if (locked) exit;
if (!instance_exists(owner)) exit;

owner.log_msg("disconnecting...");

var _r = bluetooth_le_disconnect(owner.connection);

if (_r != BluetoothError.Ok)
{
    owner.log_msg("disconnect failed: " + bluetooth_last_error_message());
    exit;
}

// A disconnect we asked for fires no le_disconnected, so tear the connection
// UI down here. Its CleanUp resets global.ble_conn, which unlocks the device
// list again.
show_debug_message("[GML] disconnected by user");
instance_destroy(owner);
