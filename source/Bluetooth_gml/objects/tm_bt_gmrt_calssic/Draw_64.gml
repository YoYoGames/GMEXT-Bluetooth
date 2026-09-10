var _w = display_get_gui_width();
var _h = display_get_gui_height();
var _bw = _w / 2 - 10;
var _bh = 80;

draw_set_alpha(1);
draw_set_font(Font_YoYo_15);

// Boxes with gray color
draw_set_color(c_gray);
draw_rectangle(5, 5, 5 + _bw, 5 + _bh, false);
draw_rectangle(_w/2 + 5, 5, _w - 5, 5 + _bh, false);
draw_rectangle(5, _h - _bh - 5, 5 + _bw, _h - 5, false);
draw_rectangle(_w/2 + 5, _h - _bh - 5, _w - 5, _h - 5, false);

// Text with black color
draw_set_color(c_black);
draw_text(15, 25, "REQUEST\nPERMISSION");
draw_text(_w/2 + 15, 25, "START SCAN");
draw_text(15, _h - _bh + 15, "STOP SCAN");
draw_text(_w/2 + 15, _h - _bh + 15, "CLEAR DEVICES");

// Status text
draw_set_color(c_white);
draw_text(10, 110, $"Status: {bt_ready ? "Connected" : "Not Ready"}");
draw_text(10, 130, $"Permission: {bluetooth_permission_to_string(last_permission_status)}");
draw_text(10, 150, $"Scanning: {bluetooth_classic_scan_is_running()}");
draw_text(10, 170, $"Devices: {bluetooth_device_get_count()}");

//if(bluetooth_classic_scan_is_running())
//	show_message_async("bluetooth_classic_scan_is_running")
	
