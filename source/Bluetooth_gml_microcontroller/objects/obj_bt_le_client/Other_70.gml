
switch(async_load[?"type"]) 
{
	case "bt_le_scan_result":
		
		var _name = async_load[?"name"];
		if (_name == "") break;
		if (_name == "Unknown") break;
		
		var _count = 0, _exists = false;
		var _address = async_load[? "address"];
		var _raw_signal = async_load[? "raw_signal"];
		with (obj_bt_le_device) {
			if (address == _address)
			{
				raw_signal = _raw_signal
				_exists = true;
				break;
			}
			_count++;
		}
		
		if (!_exists)
		{
			with(instance_create_depth(x_offset, y_offset + (100 * _count), 0, obj_bt_le_device))
			{
				image_xscale = 7;
				raw_signal = _raw_signal
				name = _name;
				address = _address;
				text =  _name;
			}
		}
		break;
	
	case "bt_le_peripheral_open":
		if (async_load[?"success"] == false) break;
		
		global.address = async_load[?"address"];
		room_goto(rm_bt_le_services);
		break;
		
		
	case "bt_le_peripheral_connection_state_changed":
		break;

	case "bt_le_characteristic_read":
		break;
		
	case "bt_le_characteristic_write_request":
		break;
		
	case "bt_le_characteristic_notify":
		break;
		
	case "bt_le_characteristic_indicate":
		break;
		
	case "bt_le_characteristic_unsubscribe":
		break;
		
	case "bt_le_descriptor_read":
		break;
		
	case "bt_le_descriptor_write":
		break;
}
