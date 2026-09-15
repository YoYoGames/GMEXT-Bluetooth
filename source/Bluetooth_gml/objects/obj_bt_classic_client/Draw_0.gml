draw_set_font(fnt_gm_20);
draw_set_halign(fa_left);
draw_set_valign(fa_top);
draw_set_color(c_white);

if (global.classic_conn != -1) {
    draw_text(ui_margin, 24, "Connected. Press SPACE to send a message.");
} else if (bluetooth_classic_scan_is_running()) {
    draw_text(ui_margin, 24, "Scanning... tap a device below to connect.");
} else {
    draw_text(ui_margin, 24, "Scan finished. Tap a device below to connect.");
}

if (global.classic_conn == -1) {
    var _list_top = 96;
    var _count = array_length(devices);
    for (var i = 0; i < _count; i++) {
        var _row_y = _list_top + i * (ui_row_h + ui_gap);
        var _entry = devices[i];
        draw_rectangle(ui_margin, _row_y, display_get_gui_width() - ui_margin, _row_y + ui_row_h, true);
        draw_text(ui_margin + 12, _row_y + ui_row_h / 2 - 10, _entry.name + "  (" + _entry.address + ")");
    }
}
