/// @description Digital Read

var pin = buffer_read(buff,buffer_u8)
var value = buffer_read(buff,buffer_u8)
with(obj_bt_microcontroller_arduino_digital_read)
	text = "Digital Read: " + string(value)
				