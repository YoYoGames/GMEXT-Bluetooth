@title Getting Started

# Getting Started

This guide takes you through a first Bluetooth Low Energy session: initialising the extension, asking for permission, scanning for a device, connecting to it and reading a characteristic. The same pattern of a call, a returned error code and one callback applies to the rest of the API.

Before you start, read ${page.platform_setup} for what your target platform needs from the project, and set the ${page.extension_options} if you are building for iOS.

## 1. Initialise the extension

You must call ${function.bluetooth_initialize} once, before any other Bluetooth function. It returns `true` once the extension is running, even on a device with no Bluetooth hardware, which then reports ${constant.BluetoothState}.Unsupported. Register the callbacks that report events, such as a change of the radio's state, straight after it:

```gml
bluetooth_initialize();

bluetooth_set_callback_state_changed(function(_state)
{
    show_debug_message($"Bluetooth state: {_state}");
});
```

The above code initialises the extension and registers a callback that is called with a ${constant.BluetoothState} whenever the local adapter changes state. It is also called once, straight away, with the current state.

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

Devices found by a scan go into the extension's device cache, and ${function.bluetooth_set_callback_device_found} tells you about each new one. A device is identified by a handle, which you pass to the `bluetooth_device_*` functions in ${module.devices}:

```gml
start_scan = function()
{
    bluetooth_set_callback_device_found(function(_device)
    {
        if (bluetooth_device_get_transport(_device) != BluetoothTransport.LowEnergy) exit;

        var _name = bluetooth_device_get_name(_device);
        show_debug_message($"Found {_name}");

        if (_name == "My Sensor")
        {
            bluetooth_le_scan_stop();
            connect_to(_device);
        }
    });

    var _error = bluetooth_le_scan_start(true, []);
    if (_error != BluetoothError.Ok)
    {
        show_debug_message($"Scan did not start: {bluetooth_last_error_message()}");
    }
};
```

The above code registers a callback for new devices and starts an active LE scan with no filters, which reports every device in range. When a device with the expected name is found, the scan is stopped and a connection is made. Pass an array of ${struct.BluetoothLeScanFilter} structs instead of an empty array to report only the devices you are interested in.

## 4. Connect and discover

${function.bluetooth_le_connect} returns a connection handle straight away and calls its callback once the device has answered. A connection's services, and each service's characteristics, are discovered in turn, and are then walked with the getter functions in ${module.le_client}:

```gml
connect_to = function(_device)
{
    connection = bluetooth_le_connect(_device, function(_error_code, _message, _connection, _device)
    {
        if (_error_code != BluetoothError.Ok)
        {
            show_debug_message($"Connect failed: {_message}");
            exit;
        }

        bluetooth_le_services_discover(_connection, function(_error_code, _message, _connection)
        {
            if (_error_code != BluetoothError.Ok) exit;

            var _count = bluetooth_le_service_get_count(_connection);
            for (var _i = 0; _i < _count; _i++)
            {
                var _service = bluetooth_le_service_get_at(_connection, _i);
                show_debug_message($"Service {bluetooth_le_service_get_uuid(_service)}");
            }
        });
    });

    if (connection == 0)
    {
        show_debug_message($"Connect did not start: {bluetooth_last_error_message()}");
    }
};
```

The above code connects to the given device and, once it is connected, discovers its services and prints the UUID of each one. Characteristics are discovered the same way, per service, with ${function.bluetooth_le_characteristics_discover}.

## 5. Read a value

Byte data that arrives from a device, whether from a read, a notification or a Classic connection, is held by the extension and handed to your callback as a value handle and a size. You copy it into a buffer of your own with ${function.bluetooth_value_fetch}, which also frees it:

```gml
bluetooth_le_characteristic_read(characteristic, function(_error_code, _message, _characteristic, _value, _size)
{
    if (_error_code != BluetoothError.Ok)
    {
        show_debug_message($"Read failed: {_message}");
        exit;
    }

    var _buffer = buffer_create(max(_size, 1), buffer_fixed, 1);
    var _copied = bluetooth_value_fetch(_value, _buffer, 0);
    if (_copied > 0)
    {
        show_debug_message($"Read {_copied} bytes, first byte {buffer_peek(_buffer, 0, buffer_u8)}");
    }
    buffer_delete(_buffer);
});
```

The above code reads a characteristic, copies the bytes into a new buffer sized from the callback's `_size` argument, and prints the first byte. A value you do not want must be freed with ${function.bluetooth_value_release}; one that is never fetched or released is freed by the extension after 30 seconds.

## 6. Clean up

Call ${function.bluetooth_le_disconnect} for each connection you are finished with. ${function.bluetooth_shutdown} ends every connection, scan, advertisement and server, and calls each callback that is still waiting once with ${constant.BluetoothError}.NotInitialized:

```gml
if (connection != 0)
{
    bluetooth_le_disconnect(connection);
    connection = 0;
}
bluetooth_shutdown();
```

The above code closes the connection made earlier and shuts the extension down. A disconnect that you ask for takes effect at once, and no disconnected callback is called for it.
