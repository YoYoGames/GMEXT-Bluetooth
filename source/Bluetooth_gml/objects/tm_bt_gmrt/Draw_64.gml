// ============================================================
// Bluetooth test - Draw GUI Event
// ============================================================

var _x = 32;
var _y = 32;

draw_text(_x, _y, "GMBluetooth - Windows BLE Test");
_y += 28;

draw_text(
    _x,
    _y,
    $"Initialized: {bluetooth_is_initialized()}"
);
_y += 22;

draw_text(
    _x,
    _y,
    $"BLE supported: {bluetooth_le_is_supported()}"
);
_y += 22;

draw_text(
    _x,
    _y,
    $"Classic supported: {bluetooth_classic_is_supported()}"
);
_y += 22;

draw_text(
    _x,
    _y,
    $"Scanning: {bluetooth_le_scan_is_running()}"
);
_y += 22;

var _count =
    bluetooth_device_get_count();

draw_text(
    _x,
    _y,
    $"Devices: {_count}"
);
_y += 32;


draw_text(
    _x,
    _y,
    "SPACE = Start scan | S = Stop | P = Print | C = Clear"
);

_y += 38;


// ------------------------------------------------------------
// Draw device list
// ------------------------------------------------------------

for (var i = 0; i < _count; ++i)
{
    var _device =
        bluetooth_device_get_at(i);

    if (!bluetooth_device_is_valid(_device))
        continue;


    var _name =
        bluetooth_device_get_name(_device);

    if (_name == "")
        _name = "<unnamed>";


    var _address = "";

    if (bluetooth_device_has_address(_device))
    {
        _address =
            bluetooth_device_get_address(_device);
    }


    var _rssi = "";

    if (bluetooth_device_has_rssi(_device))
    {
        _rssi =
            string(bluetooth_device_get_rssi(_device)) + " dBm";
    }


    var _connectable =
        bluetooth_device_is_connectable(_device);


    draw_text(
        _x,
        _y,
        $"{i}: {_name} | {_address} | {_rssi} | Connectable={_connectable}"
    );

    _y += 22;
}