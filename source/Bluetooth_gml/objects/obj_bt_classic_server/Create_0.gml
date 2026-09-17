show_debug_message("========== BLUETOOTH CLASSIC SERVER (DEMO) ==========");

bt_ready = bluetooth_is_initialized();
server_started = false;
permission_request_sent = false;

global.classic_conn = 0;
global.classic_conn_inst = noone;

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

if (!bluetooth_classic_server_is_supported())
{
    show_debug_message("[GML] Bluetooth Classic server is not supported on this platform");
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
        show_debug_message("[GML] classic client disconnected: " + _message);

        if (instance_exists(global.classic_conn_inst))
        {
            instance_destroy(global.classic_conn_inst);
        }

        global.classic_conn = 0;
        global.classic_conn_inst = noone;
    }
);

start_classic_server_demo = function()
{
    if (!bt_ready || server_started) return;

    if (bluetooth_permission_get_status() != BluetoothPermissionStatus.Granted)
    {
        if (!permission_request_sent)
        {
            permission_request_sent = true;
            bluetooth_permission_request();
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
};

start_classic_server_demo();
