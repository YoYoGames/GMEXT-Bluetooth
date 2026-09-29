// Drawn on the right half so it does not cover the status panels on the left.
draw_set_font(fnt_gm_15);
draw_set_halign(fa_left);
draw_set_valign(fa_top);
draw_set_color(c_white);

var _x = ui_x;
var _y = 16;
var _line = 17;

var _hint = "Press DISCOVER GATT to find the demo service.";
if (discovery_in_progress) _hint = "Discovering the demo service...";
else if (array_length(rows) > 0) _hint = "Use the buttons next to each characteristic.";

draw_text(_x, _y, "BLE CLIENT - CONNECTED");
_y += _line;
draw_text(_x, _y, _hint);
_y += _line;
draw_text(_x, _y, "INFO = READ, RX = WRITE, TX = SUBSCRIBE / NOTIFY.");
_y += _line + 8;

draw_set_color(c_ltgray);
for (var i = 0; i < array_length(log_lines); i++)
{
    draw_text(_x, _y, log_lines[i]);
    _y += _line;
}
