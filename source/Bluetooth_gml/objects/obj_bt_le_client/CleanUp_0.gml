if (bluetooth_le_scan_is_running()) bluetooth_le_scan_stop();
if (global.ble_conn != -1) {
    bluetooth_le_disconnect(global.ble_conn);
    global.ble_conn = -1;
}
