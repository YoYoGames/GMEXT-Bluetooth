
alarm[0] = room_speed/4

var K = 1

var buff = buffer_create(3,buffer_grow,1)

var wise = left.y <= left.ystart
arduino_digital_write(pins_left[0],wise,buff)
arduino_digital_write(pins_left[1],!wise,buff)
arduino_digital_write(pins_left[2],wise,buff)
arduino_digital_write(pins_left[3],!wise,buff)

var value = point_distance(x,y,xstart,ystart)/K
var value = abs(point_distance(left.x,left.y,left.xstart,left.ystart)/K)
if(value > 255)
	value = 255
	
arduino_analog_write(pins_left_pwd[0],value,buff)
arduino_analog_write(pins_left_pwd[1],value,buff)

var wise = right.y <= right.ystart
arduino_digital_write(pins_right[0],wise,buff)
arduino_digital_write(pins_right[1],!wise,buff)
arduino_digital_write(pins_right[2],wise,buff)
arduino_digital_write(pins_right[3],!wise,buff)

var value = abs(point_distance(right.x,right.y,right.xstart,right.ystart)/K)
if(value > 255)
	value = 255

arduino_analog_write(pins_right_pwd[0],value,buff)
arduino_analog_write(pins_right_pwd[1],value,buff)

bt_classic_socket_send(global.socket,buff,0,buffer_tell(buff))

buffer_delete(buff)
