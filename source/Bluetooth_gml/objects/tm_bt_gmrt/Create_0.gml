// ============================================================
// Bluetooth mobile test - Create Event
// ============================================================

show_debug_message("========================================");
show_debug_message("GMBluetooth MOBILE TEST - CREATE EVENT");
show_debug_message("========================================");
show_debug_message("Platform: " + os_get_config());

// Request Android 12+ runtime permissions
if (!os_check_permission("android.permission.ACCESS_FINE_LOCATION") || !os_check_permission("android.permission.BLUETOOTH_SCAN") || !os_check_permission("android.permission.BLUETOOTH_CONNECT") || !os_check_permission("android.permission.BLUETOOTH_ADVERTISE")) {
	show_debug_message("[GML] Requesting runtime permissions...");
	os_request_permission("android.permission.ACCESS_FINE_LOCATION", "android.permission.BLUETOOTH_SCAN", "android.permission.BLUETOOTH_CONNECT", "android.permission.BLUETOOTH_ADVERTISE");
}

// ------------------------------------------------------------
// UI / runtime state
// ------------------------------------------------------------

bt_ready = false;
permission_request_sent = false;
auto_scan_after_permission = true;
last_permission_status = BluetoothPermissionStatus.Unknown;

// Polled by Step so it can log transitions instead of every frame.
last_device_count = 0;
last_scanning = false;
log_tick = 0;

ui_margin = 24;
ui_gap = 12;
ui_button_h = 64;


// ------------------------------------------------------------
// Helper: transport -> string
// ------------------------------------------------------------

bluetooth_transport_to_string = function(_transport)
{
    switch (_transport)
    {
        case BluetoothTransport.Classic:
            return "Classic";

        case BluetoothTransport.LowEnergy:
            return "BLE";

        default:
            return "Unknown";
    }
};


// ------------------------------------------------------------
// Helper: permission -> string
// ------------------------------------------------------------

bluetooth_permission_to_string = function(_status)
{
    switch (_status)
    {
        case BluetoothPermissionStatus.Granted:
            return "Granted";

        case BluetoothPermissionStatus.Denied:
            return "Denied";

        default:
            return "Unknown";
    }
};


// ------------------------------------------------------------
// Helper: print all information about one device
// ------------------------------------------------------------

bluetooth_print_device = function(_device)
{
    if (!bluetooth_device_is_valid(_device))
    {
        show_debug_message("Invalid Bluetooth device.");
        return;
    }

    var _transport =
        bluetooth_device_get_transport(_device);

    var _id =
        bluetooth_device_get_id(_device);

    var _name =
        bluetooth_device_get_name(_device);

    var _connectable =
        bluetooth_device_is_connectable(_device);


    var _address = "<not available>";

    if (bluetooth_device_has_address(_device))
    {
        _address =
            bluetooth_device_get_address(_device);
    }


    var _rssi = "<not available>";

    if (bluetooth_device_has_rssi(_device))
    {
        _rssi =
            string(bluetooth_device_get_rssi(_device)) + " dBm";
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


// ------------------------------------------------------------
// Helper: print every cached device
// ------------------------------------------------------------

bluetooth_print_all_devices = function()
{
    var _count =
        bluetooth_device_get_count();

    show_debug_message("");
    show_debug_message("========================================");
    show_debug_message($"BLUETOOTH DEVICES: {_count}");
    show_debug_message("========================================");

    for (var i = 0; i < _count; ++i)
    {
        var _device =
            bluetooth_device_get_at(i);

        bluetooth_print_device(_device);
    }

    show_debug_message("========================================");
};


// ------------------------------------------------------------
// Helper: request Android Bluetooth permissions
// ------------------------------------------------------------

bluetooth_request_permissions = function()
{
    if (!bt_ready)
        return;

    var _status =
        bluetooth_permission_get_status();

    if (_status == BluetoothPermissionStatus.Granted)
    {
        permission_request_sent = false;
        return;
    }

    var _error =
        bluetooth_permission_request();

    permission_request_sent =
        (_error == BluetoothError.Ok);

    show_debug_message(
        $"bluetooth_permission_request() = {_error}"
    );

    if (_error != BluetoothError.Ok)
    {
        show_debug_message(
            $"Permission request error: " +
            $"{bluetooth_last_error_code()} - " +
            $"{bluetooth_last_error_message()}"
        );
    }
};


// ------------------------------------------------------------
// Helper: start BLE scan
// ------------------------------------------------------------

bluetooth_start_ble_scan = function()
{
    show_debug_message("[GML] bluetooth_start_ble_scan() called");

    if (!bt_ready)
    {
        show_debug_message("[GML] ERROR: Bluetooth not ready");
        return;
    }

    var _perm_status = bluetooth_permission_get_status();
    show_debug_message($"[GML] Current permission status: {bluetooth_permission_to_string(_perm_status)}");

    if (_perm_status != BluetoothPermissionStatus.Granted)
    {
        show_debug_message("[GML] Permissions not granted, requesting...");
        bluetooth_request_permissions();
        return;
    }

    if (bluetooth_le_scan_is_running())
    {
        show_debug_message("[GML] BLE scan already running");
        return;
    }

    show_debug_message("[GML] Clearing device cache...");
    bluetooth_device_clear();

    show_debug_message("[GML] Calling bluetooth_le_scan_start(true)...");
    var _error =
        bluetooth_le_scan_start(true);

    show_debug_message(
        $"[GML] bluetooth_le_scan_start() returned: {_error}"
    );

    if (_error != BluetoothError.Ok)
    {
        show_debug_message("[GML] *** ERROR: BLE scan failed ***");
        show_debug_message(
            $"[GML] Error Code: {bluetooth_last_error_code()}"
        );
        show_debug_message(
            $"[GML] Error Message: {bluetooth_last_error_message()}"
        );
    }
    else
    {
        show_debug_message("[GML] *** BLE SCANNING STARTED SUCCESSFULLY ***");

        // On iOS the call above reports Ok even when CoreBluetooth silently
        // dropped it because the central had not powered on yet. The only way
        // to catch that from GML is to ask whether a scan is really running.
        if (!bluetooth_le_scan_is_running())
        {
            show_debug_message(
                "[GML] *** WARNING: start reported Ok but no scan is running - " +
                "check the native log for the CBCentralManager state ***"
            );
        }
    }
};


// ------------------------------------------------------------
// Initialize
// ------------------------------------------------------------

show_debug_message("[GML] ========== BLUETOOTH INITIALIZATION ==========");
show_debug_message("[GML] Calling bluetooth_initialize()...");
bt_ready =
    bluetooth_initialize();

show_debug_message($"[GML] bluetooth_initialize() returned: {bt_ready}");
show_debug_message($"[GML] Last error code: {bluetooth_last_error_code()}");
show_debug_message($"[GML] Last error message: {bluetooth_last_error_message()}");

if (!bt_ready)
{
    show_debug_message("[GML] *** ERROR: INITIALIZATION FAILED ***");
    show_debug_message(
        $"[GML] Details: " +
        $"{bluetooth_last_error_code()} - " +
        $"{bluetooth_last_error_message()}"
    );
}
else
{
    show_debug_message("[GML] *** INITIALIZATION SUCCESSFUL ***");
    show_debug_message(
        $"[GML] BLE supported: {bluetooth_le_is_supported()}"
    );

    show_debug_message(
        $"[GML] Classic supported: {bluetooth_classic_is_supported()}"
    );


    // --------------------------------------------------------
    // Device discovered callback
    // --------------------------------------------------------

    bluetooth_set_callback_device_found(
        function(_device)
        {
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
        }
    );
    show_debug_message("[GML] Device found callback registered");


    // --------------------------------------------------------
    // Scan stopped callback
    // --------------------------------------------------------

    bluetooth_set_callback_scan_stopped(
        function(_error, _message)
        {
            show_debug_message("");
            show_debug_message("[GML] *** CALLBACK: SCAN STOPPED ***");
            show_debug_message($"[GML] Error code: {_error}");
            show_debug_message($"[GML] Message: {_message}");

            bluetooth_print_all_devices();
        }
    );
    show_debug_message("[GML] Scan stopped callback registered");


    bluetooth_device_clear();

    show_debug_message("[GML] ========== CHECKING PERMISSIONS ==========");
    last_permission_status =
        bluetooth_permission_get_status();

    show_debug_message(
        $"[GML] Permission status: " +
        $"{bluetooth_permission_to_string(last_permission_status)}"
    );


    // Mobile: request permissions immediately.
    // The Android system dialog is asynchronous, so Step waits until
    // bluetooth_permission_get_status() becomes Granted before scanning.
    if (last_permission_status == BluetoothPermissionStatus.Granted)
    {
        show_debug_message("[GML] Permissions already granted, starting BLE scan...");
        bluetooth_start_ble_scan();
        auto_scan_after_permission = false;
    }
    else
    {
        show_debug_message("[GML] Permissions not granted, requesting permissions...");
        bluetooth_request_permissions();
    }
}
