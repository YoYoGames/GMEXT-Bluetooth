/// @description Analog Read

var pin = buffer_read(buff,buffer_u8)
var value = buffer_read(buff,buffer_u8)
with(obj_bt_microcontroller_arduino_analog_read)
	text = "Analog input: " + string(value)