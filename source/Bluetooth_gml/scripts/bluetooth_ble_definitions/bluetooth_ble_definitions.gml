// Shared by both BLE Server and BLE Client screens
#macro DEMO_SERVICE_UUID   "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#macro DEMO_CHAR_RX_UUID   "6e400002-b5a3-f393-e0a9-e50e24dcca9e"  // client writes here, server receives
#macro DEMO_CHAR_TX_UUID   "6e400003-b5a3-f393-e0a9-e50e24dcca9e"  // server notifies here, client receives
#macro DEMO_CHAR_INFO_UUID "6e400004-b5a3-f393-e0a9-e50e24dcca9e"  // client reads demo/server info

// GATT permission bitmasks used by the demo service definition.
#macro GATT_PROPERTY_READ             0x02
#macro GATT_PROPERTY_WRITE            0x08
#macro GATT_PROPERTY_WRITE_NO_RESPONSE 0x04
#macro GATT_PROPERTY_NOTIFY           0x10
#macro GATT_PROPERTY_INDICATE         0x20
#macro GATT_PERMISSION_WRITE          0x10
#macro GATT_PERMISSION_READ           0x01

