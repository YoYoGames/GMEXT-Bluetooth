
switch(async_load[?"type"])
{
	case "bt_classic_socket_remotely_closed":
	case "bt_classic_socket_error":
		// This is a fallback code (in case of connection loss go to menu)
		room_goto(rm_bt_classic);
		break;
}
