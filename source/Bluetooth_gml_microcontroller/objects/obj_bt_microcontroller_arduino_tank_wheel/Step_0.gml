
y = ystart

for(var a = 0 ; a < 4 ; a++)
if(device_mouse_check_button(a,mb_left))
	if(abs(x - device_mouse_x(a)) < 200)
		y = device_mouse_y(a)
