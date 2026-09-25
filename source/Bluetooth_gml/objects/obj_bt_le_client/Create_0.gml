
show_debug_message("========== BLUETOOTH LE CLIENT (DEMO) ==========");

bt_ready = bluetooth_is_initialized();

last_permission_status = BluetoothPermissionStatus.Unknown;
auto_scan_after_permission = true;

global.ble_conn = 0;
global.ble_conn_inst = noone;

// devices[] is a fresh list rebuilt for each scan.
devices = [];
device_instances = [];

if (!bt_ready)
{
    show_debug_message("[GML] Bluetooth is not initialized");
    exit;
}

if (!bluetooth_le_is_supported())
{
    show_debug_message("[GML] Bluetooth LE is not supported on this platform");
    exit;
}

bluetooth_request_permissions = function()
{
    if (!bt_ready) return;
    if (bluetooth_permission_get_status() == BluetoothPermissionStatus.Granted) return;
    bluetooth_permission_request();
};

bluetooth_start_le_scan = function()
{
    if (!bt_ready) return;

    if (bluetooth_permission_get_status() != BluetoothPermissionStatus.Granted)
    {
        bluetooth_request_permissions();
        return;
    }

    if (bluetooth_le_scan_is_running()) return;

    bluetooth_device_clear();
    devices = [];

    // Remove buttons from a previous scan before rebuilding the list.
    with (obj_bt_le_device)
    {
        instance_destroy();
    }

    device_instances = [];
    _y = 100;

    var _error = bluetooth_le_scan_start(true);
    show_debug_message($"[GML] bluetooth_le_scan_start() = {_error}");
};

_x = max(650, room_width * 0.55);
_y = 100;

bluetooth_set_callback_device_found(function(_device)
{
    // The public discovery callback is shared by BLE and Classic.
    if (bluetooth_device_get_transport(_device) != BluetoothTransport.LowEnergy) return;

    // Keep the demo scan focused: only show BLE devices with a visible name.
    // An unnamed advertisement may be reported again later with its scan-response
    // name, at which point it will be accepted.
    var _name = bluetooth_device_get_name(_device);
    if (_name == "") return;

    var _device_id = bluetooth_device_get_id(_device);

    // Avoid duplicate buttons when the same peripheral advertises repeatedly.
    for (var i = 0; i < array_length(devices); i++)
    {
        if (devices[i].id == _device_id) return;
    }

    var _address = bluetooth_device_has_address(_device)
        ? bluetooth_device_get_address(_device)
        : "<no address>";

    var _inst = instance_create_depth(
        _x,
        _y,
        0,
        obj_bt_le_device,
        {device: _device}
    );

    _inst.text = _name + " - " + _address;

    array_push(device_instances, _inst);
    array_push(devices, {
        device: _device,
        id: _device_id,
        name: _name,
        address: _address
    });

    _y += 60;
});

bluetooth_set_callback_scan_stopped(function(_error, _message)
{
    show_debug_message(
        $"[GML] le scan stopped: {_error} {_message}, devices found: {array_length(devices)}"
    );
});

bluetooth_set_callback_le_characteristic_value_changed(
    function(_characteristic, _connection)
    {
        if (instance_exists(global.ble_conn_inst))
        {
            global.ble_conn_inst.on_value_changed(_characteristic, _connection);
        }
    }
);

bluetooth_set_callback_le_disconnected(
    function(_connection, _error_code, _message)
    {
        show_debug_message("[GML] le disconnected: " + _message);

        if (instance_exists(global.ble_conn_inst))
        {
            instance_destroy(global.ble_conn_inst);
        }

        global.ble_conn = 0;
        global.ble_conn_inst = noone;
    }
);

bluetooth_device_clear();

last_permission_status = bluetooth_permission_get_status();

if (last_permission_status == BluetoothPermissionStatus.Granted)
{
    auto_scan_after_permission = false;
    bluetooth_start_le_scan();
}
else
{
    bluetooth_request_permissions();
}
