if (locked) exit;
if (!instance_exists(owner)) exit;

if (!bluetooth_pairing_is_supported(device))
{
    show_debug_message("[GML] Explicit pairing is not supported for this device/platform");
    exit;
}

owner.pair_device(device, false);
