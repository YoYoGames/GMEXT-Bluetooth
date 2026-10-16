
#macro DEMO_CLASSIC_SERVICE_UUID "6e400010-b5a3-f393-e0a9-e50e24dcca9e"
#macro DEMO_CLASSIC_SERVICE_NAME "GMBluetoothDemo"

// The demo's records on the RFCOMM stream: [u8 type][u16 length][payload],
// little-endian as GML buffers are (see obj_bt_classic_connection).
#macro DEMO_CLASSIC_RECORD_HEADER 3
#macro DEMO_CLASSIC_RECORD_POSITION 1 // payload: x, y as two s32
#macro DEMO_CLASSIC_RECORD_TEXT 2 // payload: UTF-8 text, no terminator

