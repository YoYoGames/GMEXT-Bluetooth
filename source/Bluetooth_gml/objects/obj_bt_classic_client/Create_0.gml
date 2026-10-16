show_debug_message("========== BLUETOOTH CLASSIC CLIENT (DEMO) ==========");

bt_ready = bluetooth_is_initialized();

// Why this room cannot run on this platform, or "" when it can.
unavailable_reason = "";

if (!bt_ready)
{
    unavailable_reason = "Bluetooth is not initialized";
}
else if (!bluetooth_classic_is_supported())
{
    bt_ready = false;
    unavailable_reason = "Bluetooth Classic is not supported on this platform";
}

last_permission_status = BluetoothPermissionStatus.Unknown;
auto_scan_after_permission = true;

global.classic_conn = 0;
global.classic_conn_inst = noone;
send_dialog_id = -1;

// The handle of a connect that has not completed yet (0 when none), owned
// here so Clean Up can cancel it when the room is left mid-connect.
pending_conn = 0;

// True from bluetooth_pair() to its callback; the device list locks meanwhile.
pairing = false;

devices = [];

ui_margin = 24;
ui_gap = 12;
ui_row_h = 56;

_x = room_width/2;
_y = 100;

// The methods are defined before the room gives up on an unsupported
// platform, so Step and the buttons can always call them.
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

    var _error = bluetooth_permission_request(function(_error_code, _message, _status)
    {
        show_debug_message(
            $"[GML] permission request: {_error_code} {_message}, status: {_status}"
        );

        if (auto_scan_after_permission && _status == BluetoothPermissionStatus.Granted)
        {
            auto_scan_after_permission = false;
            bluetooth_start_classic_scan();
        }
    });
    show_debug_message($"[GML] bluetooth_permission_request() = {_error}");

    if (_error != BluetoothError.Ok)
    {
        show_debug_message($"[GML] permission request failed: {bluetooth_last_error_message()}");
    }
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

    // Remove buttons from a previous scan before rebuilding the list.
    with (obj_bt_classic_device)
    {
        instance_destroy();
    }

    _y = 100;

    var _error = bluetooth_classic_scan_start();
    show_debug_message($"[GML] bluetooth_classic_scan_start() = {_error}");

    if (_error != BluetoothError.Ok)
    {
        show_debug_message($"[GML] classic scan failed to start: {bluetooth_last_error_message()}");
    }
};

// device_found can report a device more than once, so a device already
// listed (by id, as the LE client does) gets no second button.
add_device_button = function(_device)
{
    var _device_id = bluetooth_device_get_id(_device);

    for (var i = 0; i < array_length(devices); i++)
    {
        if (devices[i].id == _device_id) return;
    }

    bluetooth_print_device(_device);

    instance_create_depth(_x, _y, 0, obj_bt_classic_device, {device: _device, owner: id});
    _y += 100;

    array_push(devices, {
        device: _device,
        id: _device_id,
        name: bluetooth_device_get_name(_device),
        address: bluetooth_device_has_address(_device)
            ? bluetooth_device_get_address(_device)
            : "<no address>"
    });
};

// One connection at a time: a connect to another device closes the current
// one first, and a connect to the device already connected does nothing.
connect_device = function(_device)
{
    if (!bt_ready || pending_conn != 0) return;

    if (global.classic_conn != 0)
    {
        if (bluetooth_classic_connection_get_device(global.classic_conn) == _device) return;

        var _closed = bluetooth_classic_disconnect(global.classic_conn);

        if (_closed != BluetoothError.Ok)
        {
            show_debug_message("[GML] classic disconnect failed: " + bluetooth_last_error_message());
        }

        if (instance_exists(global.classic_conn_inst))
        {
            instance_destroy(global.classic_conn_inst);
        }

        global.classic_conn = 0;
        global.classic_conn_inst = noone;
    }

    // Bound to a struct, not to this instance: the callback can arrive after
    // the room is left, and must then close the link instead of spawning.
    var _ctx = {
        client: id
    };

    pending_conn = bluetooth_classic_connect(
        _device,
        DEMO_CLASSIC_SERVICE_UUID,
        method(_ctx, function(_error_code, _message, _connection, _device)
        {
            show_debug_message(
                "[GML] classic connect "
                + string(_error_code)
                + " "
                + _message
            );

            if (!instance_exists(client))
            {
                if (_error_code == BluetoothError.Ok) bluetooth_classic_disconnect(_connection);
                return;
            }

            client.pending_conn = 0;

            if (_error_code != BluetoothError.Ok) return;

            global.classic_conn = _connection;

            global.classic_conn_inst = instance_create_depth(
                0,
                0,
                0,
                obj_bt_classic_connection,
                {connection: _connection}
            );
        })
    );

    if (pending_conn == 0)
    {
        show_debug_message(
            "[GML] classic connect failed to start: "
            + string(bluetooth_last_error_code())
            + " "
            + bluetooth_last_error_message()
        );
    }
};

// Pairs first when the device needs it, then connects.
pair_device = function(_device, _connect_after)
{
    if (!bt_ready || pairing) return;

    var _ctx = {
        client: id,
        connect_after: _connect_after
    };

    pairing = true;

    var _error = bluetooth_pair(
        _device,
        method(_ctx, function(_error_code, _message, _device)
        {
            show_debug_message(
                "[GML] pair "
                + string(_error_code)
                + " "
                + _message
            );

            if (!instance_exists(client)) return;

            client.pairing = false;

            with (obj_bt_classic_device)
            {
                if (device == _device)
                {
                    paired = bluetooth_device_is_paired(_device);
                    update_text();
                }
            }

            if (connect_after && _error_code == BluetoothError.Ok)
            {
                client.connect_device(_device);
            }
        })
    );

    if (_error != BluetoothError.Ok)
    {
        pairing = false;
        show_debug_message(
            "[GML] pair failed to start: "
            + string(_error)
            + " "
            + bluetooth_last_error_message()
        );
    }
};

if (!bt_ready)
{
    show_debug_message("[GML] " + unavailable_reason);
    exit;
}

bluetooth_set_callback_device_found(function(_device)
{
    if (bluetooth_device_get_transport(_device) != BluetoothTransport.Classic) return;

    add_device_button(_device);
});

bluetooth_set_callback_scan_stopped(function(_error, _message, _transport)
{
    if (_transport != BluetoothTransport.Classic) return;

    show_debug_message(
        $"[GML] classic scan stopped: {_error} {_message}, devices found: {array_length(devices)}"
    );
});

bluetooth_set_callback_classic_data(function(_connection, _value, _size)
{
    if (!instance_exists(global.classic_conn_inst))
    {
        bluetooth_value_release(_value);
        return;
    }

    var _buf = buffer_create(max(_size, 1), buffer_fixed, 1);
    var _n = bluetooth_value_fetch(_value, _buf, 0);

    if (_n > 0)
    {
        global.classic_conn_inst.on_receive(_buf, _n);
    }

    buffer_delete(_buf);
});

bluetooth_set_callback_classic_disconnected(
    function(_error_code, _message, _connection)
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
