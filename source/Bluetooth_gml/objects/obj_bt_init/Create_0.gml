show_debug_message("========== BLUETOOTH MANAGER ==========");

// Persistent owner of the extension lifetime.
// Demo rooms must not call bluetooth_initialize()/bluetooth_shutdown().
if (instance_number(obj_bt_init) > 1)
{
    instance_destroy();
    exit;
}

function bt_state_name(_state)
{
    switch (_state)
    {
        case BluetoothState.Unknown:      return "Unknown";
        case BluetoothState.Resetting:    return "Resetting";
        case BluetoothState.Unsupported:  return "Unsupported";
        case BluetoothState.Unauthorized: return "Unauthorized";
        case BluetoothState.PoweredOff:   return "PoweredOff";
        case BluetoothState.PoweredOn:    return "PoweredOn";
    }

    return "Unknown(" + string(_state) + ")";
}

function bt_permission_name(_status)
{
    switch (_status)
    {
        case BluetoothPermissionStatus.Unknown: return "Unknown";
        case BluetoothPermissionStatus.Granted: return "Granted";
        case BluetoothPermissionStatus.Denied:  return "Denied";
    }

    return "Unknown(" + string(_status) + ")";
}

function bt_yes_no(_value)
{
    return _value ? "YES" : "NO";
}

bt_ready = bluetooth_initialize();

global.bt_state = BluetoothState.Unknown;
global.bt_permission_status = BluetoothPermissionStatus.Unknown;
global.bt_capabilities = {
    le: false,
    le_advertise: false,
    le_server: false,
    classic: false,
    classic_server: false
};

if (!bt_ready)
{
    show_debug_message(
        "[GML] bluetooth_initialize failed: "
        + string(bluetooth_last_error_code())
        + " "
        + bluetooth_last_error_message()
    );

    room_goto(rm_bt_menu);
    exit;
}

// Cache the capability matrix once so demo rooms can also use it for UI.
global.bt_capabilities.le = bluetooth_le_is_supported();
global.bt_capabilities.le_advertise = bluetooth_le_advertise_is_supported();
global.bt_capabilities.le_server = bluetooth_le_server_is_supported();
global.bt_capabilities.classic = bluetooth_classic_is_supported();
global.bt_capabilities.classic_server = bluetooth_classic_server_is_supported();

global.bt_permission_status = bluetooth_permission_get_status();

show_debug_message("----------------------------------------");
show_debug_message("[GML] Initialized: " + bt_yes_no(bluetooth_is_initialized()));
show_debug_message("[GML] Permission status: " + bt_permission_name(global.bt_permission_status));
show_debug_message("[GML] BLE supported: " + bt_yes_no(global.bt_capabilities.le));
show_debug_message("[GML] BLE advertising supported: " + bt_yes_no(global.bt_capabilities.le_advertise));
show_debug_message("[GML] BLE GATT server supported: " + bt_yes_no(global.bt_capabilities.le_server));
show_debug_message("[GML] Classic supported: " + bt_yes_no(global.bt_capabilities.classic));
show_debug_message("[GML] Classic server supported: " + bt_yes_no(global.bt_capabilities.classic_server));
show_debug_message("[GML] Explicit pairing support is device-specific.");
show_debug_message("[GML] After discovery use bluetooth_pairing_is_supported(device).");
show_debug_message("----------------------------------------");

bluetooth_set_callback_state_changed(function(_state)
{
    global.bt_state = _state;

    show_debug_message(
        "[GML] Bluetooth state changed: "
        + bt_state_name(_state)
        + " ("
        + string(_state)
        + ")"
    );
});

if (global.bt_permission_status != BluetoothPermissionStatus.Granted)
{
    show_debug_message("[GML] Requesting Bluetooth permission...");
    bluetooth_permission_request();
}

room_goto(rm_bt_menu);
