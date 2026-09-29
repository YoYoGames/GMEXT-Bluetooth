// Only one connection at a time: lock every device while connecting/connected,
// and hide the list once connected so the GATT view has the screen.
locked = connecting || global.ble_conn != 0;
visible = connecting || global.ble_conn == 0;

event_inherited();

text = connecting ? "CONNECTING..." : label;
