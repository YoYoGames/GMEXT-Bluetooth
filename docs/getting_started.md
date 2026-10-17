@title Getting Started

# Getting Started

This guide takes you through a first Bluetooth Low Energy session: initialising the extension, asking for permission, scanning for a device, connecting to it, reading a characteristic and subscribing to its notifications. The same pattern applies to the rest of the API: a call returns an error code straight away, and the functions you pass to it are called later with the outcome and with the events of what it started.

Before you start, read ${page.platform_setup} for what your target platform needs from the project, and set the ${page.extension_options} if you are building for iOS.

## 1. Initialise the extension

You must call ${function.bluetooth_initialize} once, before any other Bluetooth function. It returns `true` once the extension is running, even on a device with no Bluetooth hardware, which then reports ${constant.BluetoothState}.Unsupported. The state of the radio is the one event that is not tied to a call, so register its callback straight after:

```gml
bluetooth_initialize();

bluetooth_set_callback_state_changed(function(_state)
{
    show_debug_message($"Bluetooth state: {_state}");
});
```

The above code initialises the extension and registers a callback that is called with a ${constant.BluetoothState} whenever the local adapter changes state. It is also called once, straight away, with the current state. When the radio is turned off, everything that was running ends first, and nothing restarts on its own when it comes back on.

## 2. Ask for permission

On Android and on Apple platforms the player must give your game permission to use Bluetooth. Call ${function.bluetooth_permission_request} before you scan, connect or advertise; on Apple platforms the system prompt only appears when you call it. On Windows the callback reports ${constant.BluetoothPermissionStatus}.Granted at once.

```gml
var _error = bluetooth_permission_request(function(_error_code, _message, _status)
{
    if (_status == BluetoothPermissionStatus.Granted)
    {
        start_scan();
    }
    else
    {
        show_debug_message($"Bluetooth permission not granted: {_message}");
    }
});

if (_error != BluetoothError.Ok)
{
    show_debug_message($"Permission request failed: {bluetooth_last_error_message()}");
}
```

The above code asks for permission and starts a scan only once it has been granted. If the request itself cannot be made, the function returns an error and the callback is never called.

## 3. Scan for devices

A scan takes the functions to call for its events: `on_found` the first time it sees a device, `on_updated` for the device's later advertisements, and `on_stopped` if it ends on its own. Every device it sees goes into the extension's device cache and is identified by a handle, which you pass to the `bluetooth_device_*` functions in ${module.devices}:

```gml
connection = 0;

check_device = function(_device)
{
    if (connection != 0) return;
    if (bluetooth_device_get_name(_device) != "My Sensor") return;

    bluetooth_le_scan_stop();
    connect_to(_device);
};

start_scan = function()
{
    var _error = bluetooth_le_scan_start(undefined, undefined, check_device, check_device,
        function(_error_code, _message)
        {
            show_debug_message($"Scan ended: {_message}");
        });

    if (_error != BluetoothError.Ok)
    {
        show_debug_message($"Scan did not start: {bluetooth_last_error_message()}");
    }
};
```

The above code starts an LE scan with the default settings and no filters, which reports every device in range. The same method, `check_device`, is passed as `on_found` and `on_updated`, because a device's name can arrive in a later advertisement than the first one. When a device with the expected name is seen, the scan is stopped and a connection is made; the check of `connection` makes sure that happens only once. Stopping the scan yourself calls nothing, so `on_stopped` only reports a scan that ended on its own, for example because the radio was turned off. Pass a ${struct.BluetoothLeScanSettings} or an array of ${struct.BluetoothLeScanFilter} structs instead of `undefined` to change how the scan runs or to report only the devices you are interested in.

## 4. Connect and discover

${function.bluetooth_le_connect} returns a connection handle straight away and calls its callback once the device has answered. It also takes `on_disconnected`, which is called if the connection is lost later. ${function.bluetooth_le_services_discover} then discovers the device's services, and with a ${struct.BluetoothLeDiscoverOptions} their characteristics too, which the find functions in ${module.le_client} look up by UUID:

```gml
connect_to = function(_device)
{
    connection = bluetooth_le_connect(_device, undefined,
        function(_error_code, _message, _connection)
        {
            if (_connection != connection) return;
            show_debug_message($"Connection lost: {_message}");
            connection = 0;
        },
        function(_error_code, _message, _connection, _device)
        {
            if (_connection != connection) return;
            if (_error_code != BluetoothError.Ok)
            {
                show_debug_message($"Connect failed: {_message}");
                connection = 0;
                return;
            }

            var _options = new BluetoothLeDiscoverOptions();
            _options.characteristics = true;
            bluetooth_le_services_discover(_connection, _options, function(_error_code, _message, _connection)
            {
                if (_connection != connection || _error_code != BluetoothError.Ok) return;

                var _service = bluetooth_le_service_find(_connection, "180f");
                battery_level = bluetooth_le_characteristic_find(_service, "2a19");
                if (battery_level != 0)
                {
                    read_battery();
                }
            });
        });

    if (connection == 0)
    {
        show_debug_message($"Connect did not start: {bluetooth_last_error_message()}");
    }
};
```

The above code connects to the given device with the default connect options. Every callback first checks that the connection it reports is still the one stored in `connection`, so a callback for an older connection is ignored. Once connected, the services of the device and their characteristics are discovered in one step, and the Battery Level characteristic (0x2A19) of the Battery service (0x180F) is looked up with ${function.bluetooth_le_service_find} and ${function.bluetooth_le_characteristic_find}. A find that matches nothing returns 0, and so does a find given 0, so one check covers a device without the service or the characteristic.

## 5. Read a value

Byte data that arrives from a device, whether from a read, a notification or a Classic connection, is held by the extension and handed to your callback as a value handle and a size. You copy it into a buffer of your own with ${function.bluetooth_value_fetch}, or with the GML helper ${function.bluetooth_value_to_buffer}, which creates the buffer for you; both free the value:

```gml
read_battery = function()
{
    bluetooth_le_characteristic_read(battery_level, function(_error_code, _message, _characteristic, _value, _size)
    {
        if (_error_code != BluetoothError.Ok)
        {
            show_debug_message($"Read failed: {_message}");
            return;
        }
        if (_size < 1)
        {
            bluetooth_value_release(_value);
            return;
        }

        var _buffer = bluetooth_value_to_buffer(_value, _size);
        if (_buffer == -1)
        {
            show_debug_message($"Fetch failed: {bluetooth_last_error_message()}");
            return;
        }
        show_debug_message($"Battery level: {buffer_peek(_buffer, 0, buffer_u8)}%");
        buffer_delete(_buffer);
    });
};
```

The above code reads the characteristic and copies the value into a new buffer before printing its first byte. A value you do not want, like the empty one here, must be freed with ${function.bluetooth_value_release}, on every early return; one that is never fetched or released is freed by the extension after 30 seconds.

## 6. Subscribe to notifications

${function.bluetooth_le_characteristic_subscribe} takes `on_value`, which is called for every value the characteristic sends until you call ${function.bluetooth_le_characteristic_unsubscribe} or the connection ends:

```gml
bluetooth_le_characteristic_subscribe(battery_level, BluetoothLeSubscribeMode.Notify,
    function(_characteristic, _connection, _value, _size)
    {
        if (_connection != connection || _size < 1)
        {
            bluetooth_value_release(_value);
            return;
        }
        var _buffer = bluetooth_value_to_buffer(_value, _size);
        if (_buffer != -1)
        {
            show_debug_message($"Battery level: {buffer_peek(_buffer, 0, buffer_u8)}%");
            buffer_delete(_buffer);
        }
    },
    function(_error_code, _message, _characteristic)
    {
        if (_error_code != BluetoothError.Ok)
        {
            show_debug_message($"Subscribe failed: {_message}");
        }
    });
```

The above code subscribes to the notifications of the Battery Level characteristic. Each value is checked against the current connection and copied with ${function.bluetooth_value_to_buffer}, and a value that is not wanted is released. The second callback reports whether the device accepted the subscription.

## 7. Clean up

Call ${function.bluetooth_le_disconnect} for each connection you are finished with. ${function.bluetooth_shutdown} ends every connection, scan, advertisement and server, calls each callback that is still waiting for an outcome once with ${constant.BluetoothError}.NotInitialized, and drops every event callback without calling it:

```gml
if (connection != 0)
{
    bluetooth_le_disconnect(connection);
    connection = 0;
}
bluetooth_shutdown();
```

The above code closes the connection made earlier and shuts the extension down. A disconnect that you ask for takes effect at once, and the connection's `on_disconnected` is not called for it.
