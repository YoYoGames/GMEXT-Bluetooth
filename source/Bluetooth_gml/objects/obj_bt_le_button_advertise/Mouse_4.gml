if (locked) exit;
if (!instance_exists(owner)) exit;

if (bluetooth_le_advertise_is_running())
{
    stop_advertising();
}
else
{
    start_advertising();
}
