/// @description Insert description here
// You can write your code in this editor

switch(async_load[?"type"]) 
{
	case "bt_le_peripheral_get_services":
	
		if (async_load[? "success"] == false) break;
		
		var _services = async_load[? "services"];
		// On mobile we return a JSON formatted array instead
		if (is_string(_services)) _services = json_parse(_services);
		
		var _count = 0;
		for (var _i = 0; _i < array_length(_services); _i++) {
			var _service = _services[_i];
			var _uuid = _service.uuid;
			
			// This will filter BLE internal services (this is for demo purposes only)
			if (string_starts_with(_uuid, "0000") && string_ends_with(_uuid, "00805F9B34FB")) continue;
			
			with(instance_create_depth(x_offset, y_offset + (100 * _count), 0, obj_bt_le_service))
			{
				image_xscale = 11;
				uuid = _uuid;
				text =  _uuid;
			}
			_count++;
		}
		break;
		
	case "bt_le_peripheral_connection_state_changed":
		break;

	case "bt_le_characteristic_value_changed":
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





