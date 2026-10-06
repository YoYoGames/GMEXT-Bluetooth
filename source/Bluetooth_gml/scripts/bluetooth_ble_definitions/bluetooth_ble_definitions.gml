// Shared by both BLE Server and BLE Client screens
#macro DEMO_SERVICE_UUID   "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#macro DEMO_CHAR_RX_UUID   "6e400002-b5a3-f393-e0a9-e50e24dcca9e"  // client writes here, server receives
#macro DEMO_CHAR_TX_UUID   "6e400003-b5a3-f393-e0a9-e50e24dcca9e"  // server notifies here, client receives
#macro DEMO_CHAR_INFO_UUID "6e400004-b5a3-f393-e0a9-e50e24dcca9e"  // client reads demo/server info

// GATT permission bitmasks used by the demo service definition.
#macro GATT_PERMISSION_WRITE 0x10
#macro GATT_PERMISSION_READ  0x01

/// @func ble_bytes_to_string(buffer, size)
/// @desc Reads the first `size` bytes of a buffer as plain ASCII text.
function ble_bytes_to_string(_buf, _n)
{
    var _s = "";
    for (var i = 0; i < _n; i++)
    {
        _s += chr(buffer_peek(_buf, i, buffer_u8));
    }
    return _s;
}

/// @func ble_property_names(properties)
/// @desc Turns a BluetoothLeCharacteristicProperty bitmask into readable text.
function ble_property_names(_properties)
{
    var _s = "";

    if (_properties & BluetoothLeCharacteristicProperty.Read)
        _s += (_s == "" ? "" : " | ") + "READ";

    if (_properties & BluetoothLeCharacteristicProperty.Write)
        _s += (_s == "" ? "" : " | ") + "WRITE";

    if (_properties & BluetoothLeCharacteristicProperty.WriteWithoutResponse)
        _s += (_s == "" ? "" : " | ") + "WRITE NO RESP";

    if (_properties & BluetoothLeCharacteristicProperty.Notify)
        _s += (_s == "" ? "" : " | ") + "NOTIFY";

    if (_properties & BluetoothLeCharacteristicProperty.Indicate)
        _s += (_s == "" ? "" : " | ") + "INDICATE";

    return (_s == "") ? "NONE" : _s;
}
