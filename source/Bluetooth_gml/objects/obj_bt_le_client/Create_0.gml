
show_debug_message("========== BLUETOOTH LE CLIENT (DEMO) ==========");

bt_ready = bluetooth_is_initialized();

// Why this room cannot run on this platform, or "" when it can.
unavailable_reason = "";

if (!bt_ready)
{
    unavailable_reason = "Bluetooth is not initialized";
}
else if (!bluetooth_le_is_supported())
{
    bt_ready = false;
    unavailable_reason = "Bluetooth LE is not supported on this platform";
}

auto_scan_after_permission = true;

// The handle of a connect that has not completed yet (0 when none), owned
// here so Clean Up can cancel it when the room is left mid-connect.
pending_conn = 0;

// The open connection's handle (0 when none) and the object showing it; that
// object clears both in its Clean Up when it is still the current one.
conn = 0;
conn_inst = noone;

// devices[] is a fresh list rebuilt for each scan.
devices = [];
device_instances = [];

// device_found fires once per device, so one first seen without a name waits
// here until a later advertisement or scan response names it (see Step).
unnamed_devices = [];

list_x = max(650, room_width * 0.55);
list_y = 100;

// The methods are defined before the room gives up on an unsupported
// platform, so Step and the buttons can always call them.
bluetooth_request_permissions = function()
{
    if (!bt_ready) return;
    if (bluetooth_permission_get_status() == BluetoothPermissionStatus.Granted) return;

    var _error = bluetooth_permission_request(function(_error_code, _message, _status)
    {
        obj_bt_init.bt_permission_status = _status;

        show_debug_message(
            $"[GML] permission request: {_error_code} {_message}, status: {_status}"
        );

        if (auto_scan_after_permission && _status == BluetoothPermissionStatus.Granted)
        {
            auto_scan_after_permission = false;
            bluetooth_start_le_scan();
        }
    });
    show_debug_message($"[GML] bluetooth_permission_request() = {_error}");

    if (_error != BluetoothError.Ok)
    {
        show_debug_message($"[GML] permission request failed: {bluetooth_last_error_message()}");
    }
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
    unnamed_devices = [];

    // Remove buttons from a previous scan before rebuilding the list.
    with (obj_bt_le_device)
    {
        instance_destroy();
    }

    device_instances = [];
    list_y = 100;

    var _error = bluetooth_le_scan_start(true, []);
    show_debug_message($"[GML] bluetooth_le_scan_start() = {_error}");

    if (_error != BluetoothError.Ok)
    {
        show_debug_message($"[GML] le scan failed to start: {bluetooth_last_error_message()}");
    }
};

add_device_button = function(_device)
{
    var _name = bluetooth_device_get_name(_device);
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
        list_x,
        list_y,
        0,
        obj_bt_le_device,
        {device: _device, owner: id}
    );

    array_push(device_instances, _inst);
    array_push(devices, {
        device: _device,
        id: _device_id,
        name: _name,
        address: _address
    });

    list_y += 60;
};

// One connection at a time; the device buttons lock while one is pending
// or open (see obj_bt_le_device Step).
connect_device = function(_device)
{
    if (!bt_ready || pending_conn != 0 || conn != 0) return;

    // Bound to a struct, not to this instance: the callback can arrive after
    // the room is left, and must then close the link instead of spawning.
    var _ctx = {
        client: id
    };

    pending_conn = bluetooth_le_connect(
        _device,
        method(_ctx, function(_error_code, _message, _connection, _device)
        {
            show_debug_message("[GML] le connect " + string(_error_code) + " " + _message);

            if (!instance_exists(client))
            {
                if (_error_code == BluetoothError.Ok) bluetooth_le_disconnect(_connection);
                return;
            }

            client.pending_conn = 0;

            if (_error_code != BluetoothError.Ok) return;

            client.conn = _connection;
            client.conn_inst = instance_create_depth(
                0,
                0,
                0,
                obj_bt_le_client_connection,
                {connection: _connection, owner: client}
            );
        })
    );

    if (pending_conn == 0)
    {
        show_debug_message(
            "[GML] le connect failed to start: "
            + string(bluetooth_last_error_code())
            + " "
            + bluetooth_last_error_message()
        );
    }
};

// Draw only draws: Step calls this every frame to take what Draw shows.
refresh_status = function()
{
    status_scanning = bt_ready && bluetooth_le_scan_is_running();
    status_connected = false;
    status_device_name = "-";
    status_device_address = "-";
    status_device_id = "-";
    status_rssi = "-";

    var _conn = conn;

    if (_conn == 0 || !bluetooth_le_connection_is_valid(_conn)) return;

    status_connected = bluetooth_le_connection_is_connected(_conn);

    var _device = bluetooth_le_connection_get_device(_conn);

    if (!bluetooth_device_is_valid(_device)) return;

    status_device_name = bluetooth_device_get_name(_device);
    status_device_id = bluetooth_device_get_id(_device);

    if (bluetooth_device_has_address(_device))
    {
        status_device_address = bluetooth_device_get_address(_device);
    }

    if (bluetooth_device_has_rssi(_device))
    {
        status_rssi = string(bluetooth_device_get_rssi(_device)) + " dBm";
    }
};

refresh_status();

if (!bt_ready)
{
    show_debug_message("[GML] " + unavailable_reason);
    exit;
}

bluetooth_set_callback_device_found(function(_device)
{
    // The public discovery callback is shared by BLE and Classic.
    if (bluetooth_device_get_transport(_device) != BluetoothTransport.LowEnergy) return;

    // Keep the demo scan focused: only show BLE devices with a visible name.
    // An unnamed one is kept aside; Step adds it once its scan response names it.
    if (bluetooth_device_get_name(_device) == "")
    {
        array_push(unnamed_devices, _device);
        return;
    }

    add_device_button(_device);
});

bluetooth_set_callback_scan_stopped(function(_error, _message, _transport)
{
    if (_transport != BluetoothTransport.LowEnergy) return;

    show_debug_message(
        $"[GML] le scan stopped: {_error} {_message}, devices found: {array_length(devices)}"
    );
});

bluetooth_set_callback_le_characteristic_value_changed(
    function(_characteristic, _connection, _value, _size)
    {
        if (instance_exists(conn_inst))
        {
            conn_inst.on_value_changed(_characteristic, _connection, _value, _size);
        }
        else
        {
            bluetooth_value_release(_value);
        }
    }
);

bluetooth_set_callback_le_disconnected(
    function(_error_code, _message, _connection)
    {
        show_debug_message("[GML] le disconnected: " + _message);

        if (instance_exists(conn_inst))
        {
            instance_destroy(conn_inst);
        }

        conn = 0;
        conn_inst = noone;
    }
);

bluetooth_device_clear();

if (bluetooth_permission_get_status() == BluetoothPermissionStatus.Granted)
{
    auto_scan_after_permission = false;
    bluetooth_start_le_scan();
}
else
{
    bluetooth_request_permissions();
}
