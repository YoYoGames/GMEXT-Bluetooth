
alarm[0] = room_speed/4


pins_right = [14,15,16,17]
pins_right_pwd = [5,3]

pins_left = [8,7,2,4]
pins_left_pwd = [9,6]

var buff = arduino_pin_mode(pins_left[0],1)
arduino_pin_mode(pins_left[1],1,buff)
arduino_pin_mode(pins_left[2],1,buff)
arduino_pin_mode(pins_left[3],1,buff)

arduino_pin_mode(pins_right[0],1,buff)
arduino_pin_mode(pins_right[1],1,buff)
arduino_pin_mode(pins_right[2],1,buff)
arduino_pin_mode(pins_right[3],1,buff)

bt_classic_socket_send(global.socket,buff,0,buffer_tell(buff))

buffer_delete(buff)
