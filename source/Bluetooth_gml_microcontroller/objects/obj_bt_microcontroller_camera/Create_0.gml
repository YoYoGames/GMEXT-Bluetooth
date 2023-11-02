
event_inherited()

mbuff = buffer_create(1,buffer_grow,1)
pos = 0

_str = ""

function func_next()
{
	buffer_save(mbuff,"spr.jpg")
	buffer_delete(mbuff)
	mbuff = buffer_create(1,buffer_grow,1)
	
	var spr
	try
	{spr = sprite_add("spr.jpg",0,0,0,0,0)}
	catch(e)
	{spr = noone}
	
	if(sprite_exists(sprite_index))
	if(sprite_exists(spr))
	if(image_xscale > 1)//not the first (its our yoyobutton)
		sprite_delete(sprite_index)
	
	image_xscale = 3
	image_yscale = 3
	
	if(sprite_exists(spr))
		sprite_index = spr
	
	var buff = buffer_create(1,buffer_fixed,1)
	buffer_write(buff,buffer_u8,0x10)
	var buff_send = arduino_custom_write(buff,0,1)
	buffer_delete(buff)

	bt_classic_socket_send(global.socket,buff_send,0,buffer_tell(buff_send))
	
	buffer_delete(buff_send)
	
}

reading_camera = false
