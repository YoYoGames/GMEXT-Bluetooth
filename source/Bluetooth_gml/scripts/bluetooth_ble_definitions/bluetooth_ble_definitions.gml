// Shared by both BLE Server and BLE Client screens
#macro DEMO_SERVICE_UUID   "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#macro DEMO_CHAR_RX_UUID   "6e400002-b5a3-f393-e0a9-e50e24dcca9e"  // client writes here, server receives
#macro DEMO_CHAR_TX_UUID   "6e400003-b5a3-f393-e0a9-e50e24dcca9e"  // server notifies here, client receives

// Android BluetoothGattCharacteristic bitmasks (no GML enum exists for these —
// spec.gmidl deliberately kept le_server_add_service as a single JSON string,
// see "API Design Decision" above, so the demo defines its own constants)
#macro GATT_PROPERTY_WRITE            0x08
#macro GATT_PROPERTY_WRITE_NO_RESPONSE 0x04
#macro GATT_PROPERTY_NOTIFY           0x10
#macro GATT_PERMISSION_WRITE          0x10
#macro GATT_PERMISSION_READ           0x01

