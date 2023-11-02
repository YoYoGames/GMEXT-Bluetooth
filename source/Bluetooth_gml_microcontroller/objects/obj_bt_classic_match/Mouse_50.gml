/// @description Send data

instance_create_depth(mouse_x, mouse_y, 0, obj_bt_classic_match_cursor);
	
buffer_write(buffer_out, buffer_f32, mouse_x);
buffer_write(buffer_out, buffer_f32, mouse_y);

bt_classic_socket_send(global.socket, buffer_out, 0, buffer_out_size);

