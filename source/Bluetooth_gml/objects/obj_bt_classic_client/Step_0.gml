if (!bt_ready) exit;

var _permission = bluetooth_permission_get_status();
if (auto_scan_after_permission && _permission == BluetoothPermissionStatus.Granted) {
    auto_scan_after_permission = false;
    bluetooth_start_classic_scan();
}
last_permission_status = _permission;

// Space = send (only meaningful once connected; dialog result handled in Other_63)
if (keyboard_check_pressed(vk_space) && global.classic_conn != -1) {
    send_dialog_id = get_string_async("Message to send", "");
}

//// Click-to-connect device list
//if (device_mouse_check_button_pressed(0, mb_left) && global.classic_conn == -1) {
//    var _mx = device_mouse_x_to_gui(0);
//    var _my = device_mouse_y_to_gui(0);
//    var _list_top = 96;
//    var _count = array_length(devices);
//    for (var i = 0; i < _count; i++) {
//        var _row_y = _list_top + i * (ui_row_h + ui_gap);
//        if (point_in_rectangle(_mx, _my, ui_margin, _row_y, display_get_gui_width() - ui_margin, _row_y + ui_row_h)) {
//            var _entry = devices[i];
//            show_debug_message($"[GML] connecting to {_entry.name} ({_entry.address})");
//            bluetooth_classic_scan_stop();
//            bluetooth_classic_connect(_entry.device, DEMO_CLASSIC_SERVICE_UUID,
//                function(_error_code, _message, _connection, _device) {
//                    show_debug_message("[GML] classic connect " + string(_error_code) + " " + _message);
//                    if (_error_code == BluetoothError.Ok) global.classic_conn = _connection;
//                });
//            break;
//        }
//    }
//}
