draw_set_color(c_white);
if (!bt_ready) {
    draw_text(16, 16, "Bluetooth not ready");
} else if (global.ble_conn == -1) {
    draw_text(16, 16, "Scanning for LE devices - click one to connect");
}
