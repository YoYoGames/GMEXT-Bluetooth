/// @description Receive data

var _bytes = bt_classic_socket_receive(global.socket, buffer_in, 0);

if (_bytes > 0)
{
	buffer_seek(buffer_in, buffer_seek_start, 0);
	while (_bytes >= buffer_chunck_size)
	{
		var _x = buffer_read(buffer_in, buffer_f32);
		var _y = buffer_read(buffer_in, buffer_f32);
		instance_create_depth(_x, _y, 0, obj_bt_classic_match_cursor);
		
		_bytes -= buffer_chunck_size;
	}
}
