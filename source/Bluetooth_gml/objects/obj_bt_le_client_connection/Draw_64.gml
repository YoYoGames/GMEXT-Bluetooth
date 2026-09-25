draw_set_halign(fa_left);
draw_set_valign(fa_top);
draw_set_color(c_white);

draw_text(16, 16, "BLE Client - Connected");
draw_text(16, 36, "Press DISCOVER GATT to find the demo service.");
draw_text(16, 56, "INFO = READ, RX = WRITE, TX = SUBSCRIBE / NOTIFY.");

var _y = 84;
for (var i = 0; i < array_length(log_lines); i++)
{
    draw_text(16, _y, log_lines[i]);
    _y += 14;
}
