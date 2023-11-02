
switch(async_load[?"type"])
{
	case "bt_classic_scan_result":
		
		if (!async_load[? "success"]) break;

		var _name = async_load[?"name"];
		if (_name == "") break;
		if (_name == "Unknown") break;

		var _count = 0, _exists = false;
		var _address = async_load[? "address"];
		with (obj_bt_classic_device) {
			if (address == _address)
			{
				_exists = true;
				break;
			}
			_count++;
		}

		if (!_exists) {
			var _authenticated = async_load[? "authenticated"];
			var _connected = async_load[? "connected"]
			with (instance_create_depth(x_offset, y_offset + (100 * _count), 0, obj_bt_classic_device)) {	
				name = _name;
				text = _name;
				address = _address;
				connected = _connected;
				authenticated = _authenticated;
				image_xscale = 8;
			}
		}
		break;
	
	case "bt_classic_socket_open":
	
		if (!async_load[? "success"]) break;
	
		global.socket = async_load[? "socket_id"];
		
		//TODO: Francisco
		//room_goto(rm_bt_classic_match);
		room_goto(rm_bt_microcontroller_menu);
		
		break;
}

