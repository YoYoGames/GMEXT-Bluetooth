

switch(async_load[?"type"])
{
	case "bt_classic_server_accept":
		global.socket = async_load[?"socket_id"]
		room_goto(rm_bt_classic_match);
		break;
}

