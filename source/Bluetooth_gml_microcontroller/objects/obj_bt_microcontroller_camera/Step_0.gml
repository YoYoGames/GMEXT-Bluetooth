
event_inherited()

var buff = buffer_create(1024,buffer_grow,1)

var length = bt_classic_socket_receive(global.socket,buff,0)

show_debug_message("length: " + string(length))

if(length)
{
	if(reading_camera)
	{
		var prev = 0
		for(var a = 0 ; a < length ; a ++)
		{
			var value = buffer_read(buff,buffer_u8)
			buffer_write(mbuff,buffer_u8,value)
			if(value == 0xD9 and prev == 0xFF or length == 1)
			{
				reading_camera = false
				func_next()
			}
			prev = value
		}
		exit
	}
	//show_debug_message("RECV: " +string(length))
	
	switch(buffer_read(buff,buffer_u8))
	{
		case Action_digitalWrite:
		break
		
		case Action_digitalRead:
		break
		
		case Action_analogWrite:
		break
		
		case Action_analogRead:
		break
		
		case Action_custom:
		
			var byte1 = buffer_read(buff,buffer_u8)
			var byte0 = buffer_read(buff,buffer_u8)
			var size = byte1*16+byte0
			var size = byte1*16+byte0
			var value = buffer_read(buff,buffer_u8)
			if(value == 0xFF)//our key for start camera
				reading_camera = true
			
		break
	}
}

buffer_delete(buff)

