
var buff = buffer_create(1,buffer_fixed,1)
buffer_write(buff,buffer_u8,0x10)
var buff_send = arduino_custom_write(buff,0,1)
buffer_delete(buff)

//bluetooth_send(global.socket,buffer_get_address(buff_send),0,buffer_tell(buff_send))
bluetooth_win_send(global.win_address,buffer_get_address(buff_send),buffer_tell(buff_send))

buffer_delete(buff_send)
