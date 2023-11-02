

//function hexChar(value)
//{
//	switch(value)
//	{
//		case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9:
//			return string(value)
//		case 10: return "A"
//		case 11: return "B"
//		case 12: return "C"
//		case 13: return "D"
//		case 14: return "E"
//		case 15: return "F"
//	}
//}

//function u8ToHexChar(value)
//{
//	show_debug_message(hexChar(value mod 16))
//	value = value div 16
//	show_debug_message(hexChar(value mod 16))
//	value = value div 16
//	show_debug_message(hexChar(value mod 16))
//	value = value div 16
//	show_debug_message(hexChar(value mod 16))
//	value = value div 16
//}

//show_debug_message("here")
//u8ToHexChar(4104)

#macro Action_pinMode 0
#macro Action_digitalWrite 1
#macro Action_digitalRead 2
#macro Action_analogWrite 3 
#macro Action_analogRead 4
#macro Action_custom 5

function arduino_pin_mode(pin,value,_buff = undefined)
{
	var buff
	if(is_undefined(_buff))
		buff = buffer_create(3,buffer_grow,1)
	else
		buff = _buff

	buffer_write(buff,buffer_u8,Action_pinMode)
	buffer_write(buff,buffer_u8,pin)
	buffer_write(buff,buffer_u8,value)
	
	return buff
}

function arduino_digital_write(pin,value,_buff = undefined)
{
	var buff;
	if(is_undefined(_buff))
		buff = buffer_create(3,buffer_grow,1)
	else
		buff = _buff

	buffer_write(buff,buffer_u8,Action_digitalWrite)
	buffer_write(buff,buffer_u8,pin)
	buffer_write(buff,buffer_u8,value)
	
	return buff
}

function arduino_digital_read(pin,_buff = undefined)
{
	var buff
	if(is_undefined(_buff))
		buff = buffer_create(2,buffer_grow,1)
	else
		buff = _buff
	
	buffer_write(buff,buffer_u8,Action_digitalRead)
	buffer_write(buff,buffer_u8,pin)
	
	return buff
}

function arduino_analog_write(pin,value,_buff = undefined)
{
	var buff
	if(is_undefined(_buff))
		buff = buffer_create(3,buffer_grow,1)
	else
		buff = _buff
	
	buffer_write(buff,buffer_u8,Action_analogWrite)
	buffer_write(buff,buffer_u8,pin)
	buffer_write(buff,buffer_u8,value)
	
	return buff
}

function arduino_analog_read(pin,_buff = undefined)
{
	var buff
	if(is_undefined(_buff))
		buff = buffer_create(2,buffer_grow,1)
	else
		buff = _buff
		
	buffer_write(buff,buffer_u8,Action_analogRead)
	buffer_write(buff,buffer_u8,pin)
	
	return buff
}

function arduino_custom_write(source,offset,size,_buff = undefined)
{
	var buff
	if(is_undefined(_buff))
		buff = buffer_create(size+3,buffer_grow,1)
	else
		buff = _buff
	
	buffer_write(buff,buffer_u8,Action_custom)//0
	buffer_write(buff,buffer_u8,size mod 16)//1
	buffer_write(buff,buffer_u8,size div 16)//2
	buffer_copy(source,offset,size,buff,3)
	
	buffer_seek(buff,buffer_seek_relative,size)
	
	return buff
}

//function arduino_send_buffer(buff,offset,size)
//{
//	show_debug_message([global.socket,buff,offset,size])
//	bluetooth_send(global.socket,buffer_get_address(buff),offset,size)
//}
