bluetooth_remove_callback_state_changed();

if (bluetooth_is_initialized())
{
    bluetooth_shutdown();
}
