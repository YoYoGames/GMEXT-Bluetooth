
alarm[0] = room_speed/4


var K = 1
var value =  point_distance(x,y,xstart,ystart)/K

var buff = buffer_create(3,buffer_grow,1)

if(y < ystart)
{
	arduino_digital_write(pins_left[0],1,buff)
	arduino_digital_write(pins_left[1],0,buff)
	arduino_digital_write(pins_left[2],1,buff)
	arduino_digital_write(pins_left[3],0,buff)

	arduino_digital_write(pins_right[0],1,buff)
	arduino_digital_write(pins_right[1],0,buff)
	arduino_digital_write(pins_right[2],1,buff)
	arduino_digital_write(pins_right[3],0,buff)
}
else
{
	arduino_digital_write(pins_left[0],0,buff)
	arduino_digital_write(pins_left[1],1,buff)
	arduino_digital_write(pins_left[2],0,buff)
	arduino_digital_write(pins_left[3],1,buff)

	arduino_digital_write(pins_right[0],0,buff)
	arduino_digital_write(pins_right[1],1,buff)
	arduino_digital_write(pins_right[2],0,buff)
	arduino_digital_write(pins_right[3],1,buff)
}

var _value = abs(value)
if(_value > 255)
	_value = 255

var ang
if(y < ystart)
	ang = point_direction((xstart-x)/3,(ystart-y)/1,0,0)
else
	ang = point_direction((x-xstart)/3,(ystart-y)/1,0,0)

arduino_analog_write(pins_right_pwd[0],abs(lengthdir_y(_value,ang+135)),buff)
arduino_analog_write(pins_right_pwd[1],abs(lengthdir_y(_value,ang+135)),buff)
arduino_analog_write(pins_left_pwd[0],abs(lengthdir_y(_value,ang+45)),buff)
arduino_analog_write(pins_left_pwd[1],abs(lengthdir_y(_value,ang+45)),buff)

bluetooth_win_send(global.win_address,buffer_get_address(buff),buffer_tell(buff))

buffer_delete(buff)
