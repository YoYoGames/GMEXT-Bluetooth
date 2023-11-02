/// @description Custom Read

var byte1 = buffer_read(buff,buffer_u8)
var byte0 = buffer_read(buff,buffer_u8)
var size = byte1*16+byte0
show_debug_message(size)
//ECHO: is incluided as demostration from Arduino
show_debug_message(buffer_read(buff,buffer_string))
			