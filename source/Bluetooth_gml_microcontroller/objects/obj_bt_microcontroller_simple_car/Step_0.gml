
var leftOn = false
var rightOn = false

for(var a = 0 ; a < 4 ; a++)
{
	if(device_mouse_check_button(a,mb_left))
	if(device_mouse_x(a) < room_width/2)
		leftOn = true
	else
		rightOn = true
}

var byte = 0

if(leftOn)
	byte += 1
	
if(rightOn)
	byte += 2

var buff = buffer_create(1,buffer_grow,1)

buffer_write(buff,buffer_s8,byte)

bt_classic_socket_send(global.socket,buff,0,buffer_get_size(buff))

buffer_delete(buff)
