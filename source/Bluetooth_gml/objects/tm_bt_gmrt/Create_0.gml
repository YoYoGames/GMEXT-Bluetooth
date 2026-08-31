// ============================================================
// Bluetooth mobile test - Create Event
// ============================================================

show_debug_message("========================================");
show_debug_message("GMBluetooth MOBILE TEST");
show_debug_message("========================================");


// ------------------------------------------------------------
// UI / runtime state
// ------------------------------------------------------------

bt_ready = false;
permission_request_sent = false;
auto_scan_after_permission = true;
last_permission_status = BluetoothPermissionStatus.Unknown;

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
    if (!bt_ready)
        return;

    if (bluetooth_permission_get_status() != BluetoothPermissionStatus.Granted)
    {
        bluetooth_request_permissions();
        return;
    }

    if (bluetooth_le_scan_is_running())
        return;

    bluetooth_device_clear();

    var _error =
        bluetooth_le_scan_start(true);

    show_debug_message(
        $"bluetooth_le_scan_start() = {_error}"
    );

    if (_error != BluetoothError.Ok)
    {
        show_debug_message(
            $"Scan error: " +
            $"{bluetooth_last_error_code()} - " +
            $"{bluetooth_last_error_message()}"
        );
    }
    else
    {
        show_debug_message("BLE scanning started...");
    }
};


// ------------------------------------------------------------
// Initialize
// ------------------------------------------------------------

bt_ready =
    bluetooth_initialize();

show_debug_message($"Initialized: {bt_ready}");

if (!bt_ready)
{
    show_debug_message(
        $"Bluetooth initialization failed: " +
        $"{bluetooth_last_error_code()} - " +
        $"{bluetooth_last_error_message()}"
    );
}
else
{
    show_debug_message(
        $"BLE supported: {bluetooth_le_is_supported()}"
    );

    show_debug_message(
        $"Classic supported: {bluetooth_classic_is_supported()}"
    );


    // --------------------------------------------------------
    // Device discovered callback
    // --------------------------------------------------------

    bluetooth_set_callback_device_found(
        function(_device)
        {
            show_debug_message("");
            show_debug_message(">>> NEW BLUETOOTH DEVICE <<<");

            bluetooth_print_device(_device);
        }
    );


    // --------------------------------------------------------
    // Scan stopped callback
    // --------------------------------------------------------

    bluetooth_set_callback_scan_stopped(
        function(_error, _message)
        {
            show_debug_message("");
            show_debug_message(">>> BLUETOOTH SCAN STOPPED <<<");
            show_debug_message($"Error:   {_error}");
            show_debug_message($"Message: {_message}");

            bluetooth_print_all_devices();
        }
    );


    bluetooth_device_clear();

    last_permission_status =
        bluetooth_permission_get_status();

    show_debug_message(
        $"Bluetooth permission: " +
        $"{bluetooth_permission_to_string(last_permission_status)}"
    );


    // Mobile: request permissions immediately.
    // The Android system dialog is asynchronous, so Step waits until
    // bluetooth_permission_get_status() becomes Granted before scanning.
    if (last_permission_status == BluetoothPermissionStatus.Granted)
    {
        bluetooth_start_ble_scan();
        auto_scan_after_permission = false;
    }
    else
    {
        bluetooth_request_permissions();
    }
}
