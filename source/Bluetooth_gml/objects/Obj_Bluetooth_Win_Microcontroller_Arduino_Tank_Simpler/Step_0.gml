
var _mouse_x = xstart
var _mouse_y = ystart

for(var a = 0 ; a < 4 ; a ++)
if(device_mouse_check_button(a,mb_left))
{
	_mouse_x = device_mouse_x(a)
	_mouse_y = device_mouse_y(a)
	break
}

x = _mouse_x
y = _mouse_y
