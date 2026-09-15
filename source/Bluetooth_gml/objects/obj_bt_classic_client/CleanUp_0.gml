if (bluetooth_classic_scan_is_running()) bluetooth_classic_scan_stop();
if (global.classic_conn != -1) {
    bluetooth_classic_disconnect(global.classic_conn);
    global.classic_conn = -1;
}
