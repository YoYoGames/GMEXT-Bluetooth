
if (bluetooth_classic_discoverable_is_running())
	bluetooth_classic_discoverable_stop()
else
	bluetooth_classic_discoverable_start(120)

