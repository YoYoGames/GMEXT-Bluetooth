// ============================================================
// Bluetooth test - Create Event
// ============================================================

show_debug_message("========================================");
show_debug_message("GMBluetooth TEST");
show_debug_message("========================================");


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
// Initialize
// ------------------------------------------------------------

var _initialized =
    bluetooth_initialize();

show_debug_message($"Initialized: {_initialized}");

if (!_initialized)
{
    show_debug_message(
        $"Bluetooth initialization failed: " +
        $"{bluetooth_last_error_code()} - " +
        $"{bluetooth_last_error_message()}"
    );

    exit;
}


// ------------------------------------------------------------
// Capabilities
// ------------------------------------------------------------

show_debug_message(
    $"BLE supported: {bluetooth_le_is_supported()}"
);

show_debug_message(
    $"Classic supported: {bluetooth_classic_is_supported()}"
);


// ------------------------------------------------------------
// Device discovered callback
// ------------------------------------------------------------

bluetooth_set_callback_device_found(
    function(_device)
    {
        show_debug_message("");
        show_debug_message(">>> NEW BLUETOOTH DEVICE <<<");

        bluetooth_print_device(_device);
    }
);


// ------------------------------------------------------------
// Scan stopped callback
// ------------------------------------------------------------

bluetooth_set_callback_scan_stopped(
    function(_error, _message)
    {
        show_debug_message("");
        show_debug_message(">>> BLE SCAN STOPPED <<<");

        show_debug_message($"Error:   {_error}");
        show_debug_message($"Message: {_message}");

        // Print final state of every discovered device.
        bluetooth_print_all_devices();
    }
);


// ------------------------------------------------------------
// Clear devices from previous scan
// ------------------------------------------------------------

bluetooth_device_clear();


// ------------------------------------------------------------
// Start ACTIVE BLE scan
// ------------------------------------------------------------

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