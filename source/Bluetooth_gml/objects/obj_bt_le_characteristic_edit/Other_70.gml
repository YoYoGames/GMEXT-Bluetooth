/// @description Insert description here
// You can write your code in this editor

switch(async_load[?"type"]) 
{
	case "bt_le_characteristic_get_descriptors":
	
		if (async_load[? "success"] == false) break;
		
		var _descriptors = async_load[? "descriptors"];
		// On mobile we return a JSON formatted array instead
		if (is_string(_descriptors)) _descriptors = json_parse(_descriptors);
		
		for (var _i = array_length(_descriptors) - 1; _i >= 0; --_i) {
			var _descriptor = _descriptors[_i];
			var _uuid = _descriptor.uuid;
			instance_create_depth(x_offset, y_offset + (100 * _i), 0, obj_bt_le_descriptor, {
				image_xscale: 11,
				uuid: _uuid,
				text: _uuid
			
			});
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

