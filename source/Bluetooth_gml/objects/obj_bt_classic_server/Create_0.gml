show_debug_message("========== BLUETOOTH CLASSIC SERVER (DEMO) ==========");

bt_ready = bluetooth_is_initialized();
server_started = false;
permission_request_sent = false;

global.classic_conn = 0;
global.classic_conn_inst = noone;

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
else if (!bluetooth_classic_server_is_supported())
{
    bt_ready = false;
    unavailable_reason = "Bluetooth Classic server is not supported on this platform";
}

// Defined before the room gives up on an unsupported platform, so Step can
// always call it.
start_classic_server_demo = function()
{
    if (!bt_ready || server_started) return;

    if (bluetooth_permission_get_status() != BluetoothPermissionStatus.Granted)
    {
        if (!permission_request_sent)
        {
            permission_request_sent = true;

            var _error = bluetooth_permission_request(function(_error_code, _message, _status)
            {
                global.bt_permission_status = _status;

                show_debug_message(
                    "[GML] permission request: " + string(_error_code)
                    + " " + _message
                    + ", status: " + string(_status)
                );

                if (_status == BluetoothPermissionStatus.Granted)
                {
                    start_classic_server_demo();
                }
            });

            show_debug_message(
                "[GML] bluetooth_permission_request = " + string(_error)
            );

            if (_error != BluetoothError.Ok)
            {
                show_debug_message(
                    "[GML] permission request failed: " + bluetooth_last_error_message()
                );
            }
        }

        return;
    }

    var _server_error = bluetooth_classic_server_start(
        DEMO_CLASSIC_SERVICE_NAME,
        DEMO_CLASSIC_SERVICE_UUID
    );

    show_debug_message(
        "[GML] classic_server_start = " + string(_server_error)
    );

    if (_server_error == BluetoothError.Ok)
    {
        server_started = true;
    }
    else
    {
        show_debug_message(
            "[GML] classic server failed to start: " + bluetooth_last_error_message()
        );
    }
};

// Draw only draws: Step calls this every frame to take what Draw shows.
refresh_status = function()
{
    status_running = bt_ready && bluetooth_classic_server_is_running();
    status_discoverable = bt_ready && bluetooth_classic_discoverable_is_running();
    status_connected = false;
    status_device_name = "-";
    status_device_address = "-";

    var _conn = global.classic_conn;

    if (_conn == 0 || !bluetooth_classic_connection_is_valid(_conn)) return;

    status_connected = bluetooth_classic_connection_is_connected(_conn);

    if (!status_connected) return;

    var _device = bluetooth_classic_connection_get_device(_conn);

    if (!bluetooth_device_is_valid(_device)) return;

    status_device_name = bluetooth_device_get_name(_device);

    if (bluetooth_device_has_address(_device))
    {
        status_device_address = bluetooth_device_get_address(_device);
    }
};

refresh_status();

if (!bt_ready)
{
    show_debug_message("[GML] " + unavailable_reason);
    exit;
}

bluetooth_set_callback_classic_client_connected(
    function(_connection, _device)
    {
        global.classic_conn = _connection;

        if (instance_exists(global.classic_conn_inst))
        {
            instance_destroy(global.classic_conn_inst);
        }

        global.classic_conn_inst = instance_create_depth(
            0,
            0,
            0,
            obj_bt_classic_connection,
            {connection: _connection}
        );

        show_debug_message(
            "[GML] classic client connected conn=" + string(_connection)
        );
    }
);

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
        show_debug_message("[GML] classic client disconnected: " + _message);

        if (instance_exists(global.classic_conn_inst))
        {
            instance_destroy(global.classic_conn_inst);
        }

        global.classic_conn = 0;
        global.classic_conn_inst = noone;
    }
);

start_classic_server_demo();
