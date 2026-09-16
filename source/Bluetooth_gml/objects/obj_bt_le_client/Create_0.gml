show_debug_message("========== BLUETOOTH LE CLIENT (DEMO) ==========");

// Android 12+ runtime permissions (same set the Classic client requests)
if (!os_check_permission("android.permission.ACCESS_FINE_LOCATION") || !os_check_permission("android.permission.BLUETOOTH_SCAN") || !os_check_permission("android.permission.BLUETOOTH_CONNECT")) {
    os_request_permission("android.permission.ACCESS_FINE_LOCATION", "android.permission.BLUETOOTH_SCAN", "android.permission.BLUETOOTH_CONNECT");
}

bt_ready = bluetooth_initialize();
last_permission_status = BluetoothPermissionStatus.Unknown;
auto_scan_after_permission = true;

global.ble_conn = -1;
global.ble_conn_inst = noone;

// devices[] is a fresh list rebuilt each time device_found fires, kept for
// parity/debug logging (click-to-connect is per-button, via obj_bt_le_device).
devices = [];

bluetooth_request_permissions = function() {
    if (!bt_ready) return;
    if (bluetooth_permission_get_status() == BluetoothPermissionStatus.Granted) return;
    bluetooth_permission_request();
};

bluetooth_start_le_scan = function() {
    if (!bt_ready) return;
    if (bluetooth_permission_get_status() != BluetoothPermissionStatus.Granted) {
        bluetooth_request_permissions();
        return;
    }
    if (bluetooth_le_scan_is_running()) return;
    bluetooth_device_clear();
    devices = [];
    var _error = bluetooth_le_scan_start(true);
    show_debug_message($"[GML] bluetooth_le_scan_start() = {_error}");
};

if (bt_ready) {
    _x = 300;
    _y = 100;
    bluetooth_set_callback_device_found(function(_device) {
        // The Classic and LE demos share one scan-result stream on some
        // platforms - only spawn buttons for LE devices on this screen.
        if (bluetooth_device_get_transport(_device) != BluetoothTransport.LowEnergy) return;

        instance_create_depth(_x, _y, 0, obj_bt_le_device, {device: _device});
        _y += 100;

        array_push(devices, {
            device: _device,
            name: bluetooth_device_get_name(_device),
            address: bluetooth_device_has_address(_device) ? bluetooth_device_get_address(_device) : "<no address>"
        });
    });

    bluetooth_set_callback_scan_stopped(function(_error, _message) {
        show_debug_message($"[GML] le scan stopped: {_error} {_message}, devices found: {array_length(devices)}");
    });

    bluetooth_set_callback_le_characteristic_value_changed(function(_characteristic, _connection) {
        if (instance_exists(global.ble_conn_inst)) global.ble_conn_inst.on_value_changed(_characteristic, _connection);
    });

    bluetooth_set_callback_le_disconnected(function(_connection, _error_code, _message) {
        show_debug_message("[GML] le disconnected: " + _message);
        if (instance_exists(global.ble_conn_inst)) instance_destroy(global.ble_conn_inst);
    });

    bluetooth_device_clear();
    last_permission_status = bluetooth_permission_get_status();
    if (last_permission_status == BluetoothPermissionStatus.Granted) {
        auto_scan_after_permission = false;
        bluetooth_start_le_scan();
    } else {
        bluetooth_request_permissions();
    }
}
