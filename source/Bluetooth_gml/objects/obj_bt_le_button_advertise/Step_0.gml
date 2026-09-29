// Advertising only makes sense once the demo GATT service is registered.
locked = starting || !instance_exists(owner) || !owner.service_ready;

event_inherited();

if (starting)
{
    text = "STARTING...";
}
else if (bluetooth_le_advertise_is_running())
{
    text = "STOP ADVERTISING";
}
else
{
    text = "START ADVERTISING";
}
