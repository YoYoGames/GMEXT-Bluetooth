/// obj_bt_init : Step

refresh_status();

if (!bt_ready) exit;

// The permission changes only through the prompt, whose callback sets it, or
// the system settings, so it is polled once a second rather than every frame
// (each call is a JNI call on Android). The demo rooms read the global.
permission_poll_frames--;

if (permission_poll_frames <= 0)
{
    permission_poll_frames = game_get_speed(gamespeed_fps);
    global.bt_permission_status = bluetooth_permission_get_status();
}
