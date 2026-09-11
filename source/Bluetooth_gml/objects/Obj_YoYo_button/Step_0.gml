
// Button is locked should't be interactable
if (locked) {
	image_index = 2
	return;
}

// Button is being hovered
if (point_in_rectangle(mouse_x,mouse_y,bbox_left,bbox_top,bbox_right,bbox_bottom)) {
	// Set the hover image index
	image_index = 1
	// The buttom was clicked (flicker for 1 frame)
	if (mouse_check_button_pressed(mb_left)) {
		image_index = 0
	}
}
// Button is in it's normal state
else {
	image_index = 0
}

// Was scripts/platform_mobile, which went away with the legacy Bluetooth demo.
// Inlined rather than restored: this is now its only caller.
var _is_mobile = (os_type == os_android || os_type == os_ios);

if (_is_mobile && !mouse_check_button(mb_left)) image_index = 0;