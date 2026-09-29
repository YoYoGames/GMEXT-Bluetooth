event_inherited();

// Placed in the room; obj_bt_le_server is created before this button
// (see the room's instance creation order).
owner = instance_find(obj_bt_le_server, 0);

// True between bluetooth_le_advertise_start() and its completion callback.
starting = false;

text = "START ADVERTISING";

start_advertising = function()
{
    if (starting || bluetooth_le_advertise_is_running()) return;

    var _settings_json = json_stringify({
        txPowerLevel: 0
    });

    var _data_json = json_stringify({
        includeName: true,
        services: [
            {uuid: DEMO_SERVICE_UUID}
        ]
    });

    starting = true;

    var _ctx = {
        server: owner,
        button_inst: id
    };

    var _r = bluetooth_le_advertise_start(
        _settings_json,
        _data_json,
        method(_ctx, function(_error_code, _message)
        {
            if (instance_exists(button_inst)) button_inst.starting = false;

            show_debug_message(
                "[GML] le_advertise_start "
                + string(_error_code)
                + " "
                + _message
            );

            if (!instance_exists(server)) return;

            server.last_server_event = (_error_code == BluetoothError.Ok)
                ? "Advertising started"
                : "Advertise failed: " + _message;
        })
    );

    if (_r != BluetoothError.Ok)
    {
        starting = false;
        owner.last_server_event = "Advertise failed to start";
        show_debug_message(
            "[GML] le_advertise_start failed to start: "
            + string(bluetooth_last_error_code())
            + " "
            + bluetooth_last_error_message()
        );
    }
};

stop_advertising = function()
{
    if (!bluetooth_le_advertise_is_running()) return;

    var _r = bluetooth_le_advertise_stop();
    show_debug_message($"[GML] bluetooth_le_advertise_stop() = {_r}");
    owner.last_server_event = "Advertising stopped";
};
