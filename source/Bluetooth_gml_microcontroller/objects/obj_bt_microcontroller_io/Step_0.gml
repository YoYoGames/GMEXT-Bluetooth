
event_inherited()

var buff_temp = buffer_create(1024,buffer_grow,1)
var length = bt_classic_socket_receive(global.socket,buff_temp,0)

if(!length)
	exit

buffer_seek(buff,buffer_seek_start,content)

content += length
var _length = length
var tell = buffer_tell(buff)

while(length)
{
	buffer_write(buff,buffer_u8,buffer_read(buff_temp,buffer_u8))
	length--
}

buffer_seek(buff,buffer_seek_start,tell)

buffer_delete(buff_temp)

buffer_seek(buff,buffer_seek_relative,-_length)

while(buffer_tell(buff) <= content-3)
{	
	switch(buffer_read(buff,buffer_u8))
	{
		case Action_digitalWrite:
		break
		
		case Action_digitalRead:
			event_user(0)
		break
		
		case Action_analogWrite:
		break
		
		case Action_analogRead:
			event_user(1)
		break
		
		case Action_custom:
			event_user(2)
		break
	}
}

if(content != 0)
if(content == buffer_tell(buff))
{
	buffer_seek(buff,buffer_seek_start,0)
	content = 0
}
