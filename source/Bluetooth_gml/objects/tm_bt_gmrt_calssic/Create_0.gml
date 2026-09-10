show_debug_message("========== BLUETOOTH CLASSIC TEST - CREATE ==========");

// Request Android 12+ runtime permissions
if (!os_check_permission("android.permission.ACCESS_FINE_LOCATION") || !os_check_permission("android.permission.BLUETOOTH_SCAN") || !os_check_permission("android.permission.BLUETOOTH_CONNECT") || !os_check_permission("android.permission.BLUETOOTH_ADVERTISE")) {
	show_debug_message("[GML] Requesting runtime permissions...");
	os_request_permission("android.permission.ACCESS_FINE_LOCATION", "android.permission.BLUETOOTH_SCAN", "android.permission.BLUETOOTH_CONNECT", "android.permission.BLUETOOTH_ADVERTISE");
}

bt_ready = false;
last_permission_status = BluetoothPermissionStatus.Unknown;
auto_scan_after_permission = true;
permission_request_sent = false;

ui_margin = 24;
ui_gap = 12;
ui_button_h = 64;

// Helper: permission status to string
bluetooth_permission_to_string = function(_status) {
    switch (_status) {
        case BluetoothPermissionStatus.Granted: return "Granted";
        case BluetoothPermissionStatus.Denied: return "Denied";
        default: return "Unknown";
    }
};

// Helper: transport to string
bluetooth_transport_to_string = function(_transport) {
    switch (_transport) {
        case BluetoothTransport.Classic: return "Classic";
        case BluetoothTransport.LowEnergy: return "BLE";
        default: return "Unknown";
    }
};

// Helper: print device info
bluetooth_print_device = function(_device) {
    if (!bluetooth_device_is_valid(_device)) {
        show_debug_message("Invalid device");
        return;
    }
    var _transport = bluetooth_device_get_transport(_device);
    var _id = bluetooth_device_get_id(_device);
    var _name = bluetooth_device_get_name(_device);
    var _connectable = bluetooth_device_is_connectable(_device);
    var _address = "<not available>";
    if (bluetooth_device_has_address(_device)) {
        _address = bluetooth_device_get_address(_device);
    }
    var _rssi = "<not available>";
    if (bluetooth_device_has_rssi(_device)) {
        _rssi = string(bluetooth_device_get_rssi(_device)) + " dBm";
    }
    show_debug_message("----------------------------------------");
    show_debug_message($"Handle:      {_device}");
    show_debug_message($"Transport:   {bluetooth_transport_to_string(_transport)}");
    show_debug_message($"ID:          {_id}");
    show_debug_message($"Name:        {_name}");
    show_debug_message($"Address:     {_address}");
    show_debug_message($"RSSI:        {_rssi}");
    show_debug_message($"Connectable: {_connectable}");
};

// Helper: print all devices
bluetooth_print_all_devices = function() {
    var _count = bluetooth_device_get_count();
    show_debug_message("");
    show_debug_message("========================================");
    show_debug_message($"BLUETOOTH DEVICES: {_count}");
    show_debug_message("========================================");
    for (var i = 0; i < _count; ++i) {
        bluetooth_print_device(bluetooth_device_get_at(i));
    }
    show_debug_message("========================================");
};

// Helper: request permissions
bluetooth_request_permissions = function() {
    if (!bt_ready) return;
    var _status = bluetooth_permission_get_status();
    if (_status == BluetoothPermissionStatus.Granted) {
        permission_request_sent = false;
        return;
    }
    var _error = bluetooth_permission_request();
    permission_request_sent = (_error == BluetoothError.Ok);
    show_debug_message($"[GML] bluetooth_permission_request() = {_error}");
    if (_error != BluetoothError.Ok) {
        show_debug_message($"[GML] Permission request error: {bluetooth_last_error_code()} - {bluetooth_last_error_message()}");
    }
};

// Helper: start classic scan
bluetooth_start_classic_scan = function() {
    show_debug_message("[GML] bluetooth_start_classic_scan() called");
    if (!bt_ready) {
        show_debug_message("[GML] ERROR: Bluetooth not ready");
        return;
    }
    var _perm_status = bluetooth_permission_get_status();
    show_debug_message($"[GML] Current permission status: {bluetooth_permission_to_string(_perm_status)}");
    if (_perm_status != BluetoothPermissionStatus.Granted) {
        show_debug_message("[GML] Permissions not granted, requesting...");
        bluetooth_request_permissions();
        return;
    }
    if (bluetooth_classic_scan_is_running()) {
        show_debug_message("[GML] Classic scan already running");
        return;
    }
    show_debug_message("[GML] Clearing device cache...");
    bluetooth_device_clear();
    show_debug_message("[GML] Calling bluetooth_classic_scan_start()...");
    var _error = bluetooth_classic_scan_start();
    show_debug_message($"[GML] bluetooth_classic_scan_start() returned: {_error}");
    if (_error != BluetoothError.Ok) {
        show_debug_message("[GML] *** ERROR: Classic scan failed ***");
        show_debug_message($"[GML] Error Code: {bluetooth_last_error_code()}");
        show_debug_message($"[GML] Error Message: {bluetooth_last_error_message()}");
    } else {
        show_debug_message("[GML] *** CLASSIC BLUETOOTH SCANNING STARTED SUCCESSFULLY ***");
    }
};

// Initialize Bluetooth
show_debug_message("[GML] ========== BLUETOOTH INITIALIZATION ==========");
show_debug_message("[GML] Calling bluetooth_initialize()...");
bt_ready = bluetooth_initialize();
show_debug_message($"[GML] bluetooth_initialize() returned: {bt_ready}");
show_debug_message($"[GML] Last error code: {bluetooth_last_error_code()}");
show_debug_message($"[GML] Last error message: {bluetooth_last_error_message()}");

if (!bt_ready) {
    show_debug_message("[GML] *** ERROR: INITIALIZATION FAILED ***");
} else {
    show_debug_message("[GML] *** INITIALIZATION SUCCESSFUL ***");
    show_debug_message($"[GML] BLE supported: {bluetooth_le_is_supported()}");
    show_debug_message($"[GML] Classic supported: {bluetooth_classic_is_supported()}");

    // Set up callbacks
    bluetooth_set_callback_device_found(function(_device) {
        show_debug_message("");
        show_debug_message("[GML] *** CALLBACK: NEW BLUETOOTH DEVICE FOUND ***");
        show_debug_message($"[GML] Device handle: {_device}");
        var _name = bluetooth_device_get_name(_device);
        var _addr = "";
        if (bluetooth_device_has_address(_device)) {
            _addr = bluetooth_device_get_address(_device);
        }
        show_debug_message($"[GML] *** DEVICE: {_name} ({_addr}) ***");
        bluetooth_print_device(_device);
    });
    show_debug_message("[GML] Device found callback registered");

    bluetooth_set_callback_scan_stopped(function(_error, _message) {
        show_debug_message("");
        show_debug_message("[GML] *** CALLBACK: SCAN STOPPED (TIMEOUT - NO DEVICES FOUND) ***");
        show_debug_message($"[GML] Error code: {_error}");
        show_debug_message($"[GML] Message: {_message}");
        show_debug_message($"[GML] Is still running: {bluetooth_classic_scan_is_running()}");
        bluetooth_print_all_devices();
        show_debug_message("[GML] ** Make sure you have a Bluetooth device that is DISCOVERABLE nearby! **");
    });
    show_debug_message("[GML] Scan stopped callback registered");

    bluetooth_set_callback_classic_client_connected(function(_connection, _device) {
        show_debug_message("");
        show_debug_message("[GML] *** CALLBACK: CLASSIC CLIENT CONNECTED ***");
        show_debug_message($"[GML] Connection handle: {_connection}");
        show_debug_message($"[GML] Device handle: {_device}");
    });
    show_debug_message("[GML] Classic client connected callback registered");

    bluetooth_set_callback_classic_data(function(_connection, _available) {
        show_debug_message($"[GML] *** CALLBACK: CLASSIC DATA AVAILABLE ***");
        show_debug_message($"[GML] Connection: {_connection}");
        show_debug_message($"[GML] Bytes available: {_available}");
    });
    show_debug_message("[GML] Classic data callback registered");

    bluetooth_set_callback_classic_disconnected(function(_connection, _error, _message) {
        show_debug_message("");
        show_debug_message("[GML] *** CALLBACK: CLASSIC DISCONNECTED ***");
        show_debug_message($"[GML] Connection: {_connection}");
        show_debug_message($"[GML] Error: {_error}");
        show_debug_message($"[GML] Message: {_message}");
    });
    show_debug_message("[GML] Classic disconnected callback registered");

    bluetooth_device_clear();

    show_debug_message("[GML] ========== CHECKING PERMISSIONS ==========");
    last_permission_status = bluetooth_permission_get_status();
    show_debug_message($"[GML] Permission status: {bluetooth_permission_to_string(last_permission_status)}");

    if (last_permission_status == BluetoothPermissionStatus.Granted) {
        show_debug_message("[GML] Permissions already granted, starting Classic scan...");
        bluetooth_start_classic_scan();
        auto_scan_after_permission = false;
    } else {
        show_debug_message("[GML] Permissions not granted, requesting permissions...");
        bluetooth_request_permissions();
    }
}