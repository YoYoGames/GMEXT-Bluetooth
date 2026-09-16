// No server-side "kick client" call exists in the API surface - just clear
// the globals if this is still the live instance.
if (global.ble_server_conn_inst == id) {
    global.ble_server_conn_inst = noone;
    global.ble_server_conn = -1;
}
