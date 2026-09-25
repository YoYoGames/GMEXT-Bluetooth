show_debug_message("========== BLUETOOTH CLASSIC CLIENT (DEMO) ==========");

bt_ready = bluetooth_is_initialized();

last_permission_status = BluetoothPermissionStatus.Unknown;
auto_scan_after_permission = true;

global.classic_conn = 0;
global.classic_conn_inst = noone;
send_dialog_id = -1;

devices = [];

ui_margin = 24;
ui_gap = 12;
ui_row_h = 56;

if (!bt_ready)
{
    show_debug_message("[GML] Bluetooth is not initialized");
    exit;
}

if (!bluetooth_classic_is_supported())
{
    show_debug_message("[GML] Bluetooth Classic is not supported on this platform");
    exit;
}

bluetooth_print_device = function(_device)
{
    var _name = bluetooth_device_get_name(_device);
    var _addr = bluetooth_device_has_address(_device)
        ? bluetooth_device_get_address(_device)
        : "<no address>";

    show_debug_message("----------------------------------------");
    show_debug_message($"Handle:  {_device}");
    show_debug_message($"Name:    {_name}");
    show_debug_message($"Address: {_addr}");
};

bluetooth_request_permissions = function()
{
    if (!bt_ready) return;
    if (bluetooth_permission_get_status() == BluetoothPermissionStatus.Granted) return;
    bluetooth_permission_request();
};

bluetooth_start_classic_scan = function()
{
    if (!bt_ready) return;

    if (bluetooth_permission_get_status() != BluetoothPermissionStatus.Granted)
    {
        bluetooth_request_permissions();
        return;
    }

    if (bluetooth_classic_scan_is_running()) return;

    bluetooth_device_clear();
    devices = [];

    var _error = bluetooth_classic_scan_start();
    show_debug_message($"[GML] bluetooth_classic_scan_start() = {_error}");
};

_x = room_width/2;
_y = 100;

bluetooth_set_callback_device_found(function(_device)
{
    if (bluetooth_device_get_transport(_device) != BluetoothTransport.Classic) return;

    bluetooth_print_device(_device);

    instance_create_depth(_x, _y, 0, obj_bt_classic_device, {device: _device});
    _y += 100;

    array_push(devices, {
        device: _device,
        name: bluetooth_device_get_name(_device),
        address: bluetooth_device_has_address(_device)
            ? bluetooth_device_get_address(_device)
            : "<no address>"
    });
});

bluetooth_set_callback_scan_stopped(function(_error, _message)
{
    show_debug_message(
        $"[GML] classic scan stopped: {_error} {_message}, devices found: {array_length(devices)}"
    );
});

bluetooth_set_callback_classic_data(function(_connection, _available_bytes)
{
    var _buf = buffer_create(_available_bytes, buffer_grow, 1);
    var _n = bluetooth_classic_receive(_connection, _buf, 0, _available_bytes);

    if (instance_exists(global.classic_conn_inst))
    {
        global.classic_conn_inst.on_receive(_buf, _n);
    }

    buffer_delete(_buf);
});

bluetooth_set_callback_classic_disconnected(
    function(_connection, _error_code, _message)
    {
        show_debug_message("[GML] classic disconnected: " + _message);

        if (instance_exists(global.classic_conn_inst))
        {
            instance_destroy(global.classic_conn_inst);
        }

        global.classic_conn = 0;
        global.classic_conn_inst = noone;
    }
);

bluetooth_device_clear();

last_permission_status = bluetooth_permission_get_status();

if (last_permission_status == BluetoothPermissionStatus.Granted)
{
    auto_scan_after_permission = false;
    bluetooth_start_classic_scan();
}
else
{
    bluetooth_request_permissions();
}
