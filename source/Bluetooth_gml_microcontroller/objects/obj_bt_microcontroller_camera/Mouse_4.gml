
var buff = buffer_create(1,buffer_fixed,1)
buffer_write(buff,buffer_u8,0x10)
var buff_send = arduino_custom_write(buff,0,1)
buffer_delete(buff)

bt_classic_socket_send(global.socket,buff_send,0,buffer_tell(buff_send))

buffer_delete(buff_send)
