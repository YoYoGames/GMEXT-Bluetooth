// A duplicate destroyed in Create must not shut the extension down for the survivor.
if (!owns_extension) exit;

bluetooth_remove_callback_state_changed();

if (bluetooth_is_initialized()) {
	bluetooth_shutdown();
}
