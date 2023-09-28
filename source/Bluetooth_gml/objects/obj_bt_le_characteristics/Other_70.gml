/// @description Insert description here
// You can write your code in this editor

switch(async_load[?"type"]) 
{
	case "bt_le_service_get_characteristics":
	
		if (async_load[? "success"] == false) break;
		
		var _characteristics = async_load[? "characteristics"];
		// On mobile we return a JSON formatted array instead
		if (is_string(_characteristics)) _characteristics = json_parse(_characteristics);
		
		for (var _i = 0; _i < array_length(_characteristics); _i++)
		{
			var _characteristic = _characteristics[_i];
			with(instance_create_depth(x_offset, y_offset + (100 * _i), 0, obj_bt_le_characteristic, _characteristic))
			{
				image_xscale = 11;
				text = uuid;
			}
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

