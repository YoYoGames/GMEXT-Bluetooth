

// feather ignore GM1063
// feather ignore GM2017
// feather ignore GM2044

switch(async_load[?"type"]) 
{	
	case "bt_le_server_add_service":
	
		var _settings = (platform_mobile() || isMacOs) ? json_stringify(advertise_settings) : advertise_settings;
		var _data = (platform_mobile() || isMacOs) ? json_stringify(advertise_data) : advertise_data;
	
		bt_le_advertise_start(_settings, _data);
		break;
	
	case "bt_le_server_characteristic_read_request":
		var _request_id = async_load[? "request_id"];
		var _data = base64_encode("Hello!");
		
		show_debug_message("Sent data: {0}", _data);
		
		bt_le_server_respond_read(_request_id, BT_STATUS_SUCCESS, _data);
		break;
		
	case "bt_le_server_characteristic_write_request":
		var _request_id = async_load[? "request_id"];
		var _data = async_load[? "value"]; // This is a base64 encoded string
		
		show_debug_message("Received data: {0}", _data);
		
		bt_le_server_respond_write(_request_id, BT_STATUS_SUCCESS);
		break;
	
	case "bt_le_server_descriptor_read_request":
		var _request_id = async_load[? "request_id"];
		var _data = base64_encode("World!");
		
		show_debug_message("Sent data: {0}", _data);
		
		bt_le_server_respond_read(_request_id, BT_STATUS_SUCCESS, _data);
		break;
		
	case "bt_le_server_descriptor_write_request":
		var _request_id = async_load[? "request_id"];
		var _data = async_load[? "value"]; // This is a base64 encoded string
		
		show_debug_message("Received data: {0}", _data);
		
		bt_le_server_respond_write(_request_id, BT_STATUS_SUCCESS);
		break;
}
