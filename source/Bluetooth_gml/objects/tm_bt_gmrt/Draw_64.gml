// ============================================================
// Bluetooth mobile test - Draw GUI Event
// ============================================================

var _gui_w =
    display_get_gui_width();

var _gui_h =
    display_get_gui_height();

var _x =
    ui_margin;

var _y =
    24;


// ------------------------------------------------------------
// Header / status
// ------------------------------------------------------------

draw_text(
    _x,
    _y,
    "GMBluetooth - Mobile BLE Test"
);

_y += 30;


draw_text(
    _x,
    _y,
    $"Initialized: {bluetooth_is_initialized()}"
);

_y += 24;


var _permission =
    bluetooth_permission_get_status();

draw_text(
    _x,
    _y,
    $"Permission: {bluetooth_permission_to_string(_permission)}"
);

_y += 24;


draw_text(
    _x,
    _y,
    $"BLE supported: {bluetooth_le_is_supported()}"
);

_y += 24;


draw_text(
    _x,
    _y,
    $"Scanning: {bluetooth_le_scan_is_running()}"
);

_y += 24;


var _count =
    bluetooth_device_get_count();

draw_text(
    _x,
    _y,
    $"Devices: {_count}"
);


// ------------------------------------------------------------
// Touch buttons
// ------------------------------------------------------------

var _button_w =
    (_gui_w - ui_margin * 2 - ui_gap) * 0.5;

var _x1 =
    ui_margin;

var _x2 =
    ui_margin + _button_w + ui_gap;

var _row1_y =
    186;

var _row2_y =
    _row1_y + ui_button_h + ui_gap;


var _draw_button = function(
    _bx,
    _by,
    _bw,
    _bh,
    _text,
    _enabled)
{
    draw_set_alpha(_enabled ? 1.0 : 0.45);

    draw_rectangle(
        _bx,
        _by,
        _bx + _bw,
        _by + _bh,
        false
    );

    draw_text(
        _bx + 16,
        _by + 20,
        _text
    );

    draw_set_alpha(1.0);
};


_draw_button(
    _x1,
    _row1_y,
    _button_w,
    ui_button_h,
    "REQUEST PERMISSION",
    _permission != BluetoothPermissionStatus.Granted
);

_draw_button(
    _x2,
    _row1_y,
    _button_w,
    ui_button_h,
    "START BLE",
    _permission == BluetoothPermissionStatus.Granted &&
    !bluetooth_le_scan_is_running()
);

_draw_button(
    _x1,
    _row2_y,
    _button_w,
    ui_button_h,
    "STOP BLE",
    bluetooth_le_scan_is_running()
);

_draw_button(
    _x2,
    _row2_y,
    _button_w,
    ui_button_h,
    "CLEAR DEVICES",
    true
);


// ------------------------------------------------------------
// Device list
// ------------------------------------------------------------

_y =
    _row2_y + ui_button_h + 28;

draw_text(
    _x,
    _y,
    "Discovered devices:"
);

_y += 28;


// Keep the mobile debug screen readable.
// The full list is still available in the debug output.
var _max_visible =
    max(0, floor((_gui_h - _y - 16) / 44));

var _visible =
    min(_count, _max_visible);


for (var i = 0; i < _visible; ++i)
{
    var _device =
        bluetooth_device_get_at(i);

    if (!bluetooth_device_is_valid(_device))
        continue;


    var _name =
        bluetooth_device_get_name(_device);

    if (_name == "")
        _name = "<unnamed>";


    var _transport =
        bluetooth_transport_to_string(
            bluetooth_device_get_transport(_device)
        );


    var _rssi = "";

    if (bluetooth_device_has_rssi(_device))
    {
        _rssi =
            string(
                bluetooth_device_get_rssi(_device)
            ) + " dBm";
    }


    draw_text(
        _x,
        _y,
        $"{i}: {_name}"
    );

    _y += 20;

    draw_text(
        _x + 16,
        _y,
        $"{_transport} | {_rssi}"
    );

    _y += 24;
}


if (_visible < _count)
{
    draw_text(
        _x,
        _y,
        $"... +{_count - _visible} more"
    );
}
