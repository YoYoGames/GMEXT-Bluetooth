draw_set_halign(fa_left);
draw_set_valign(fa_top);
draw_set_color(c_white);

draw_text(16, 16, "BLE Client - Connected");
draw_text(16, 36, "Discovering GATT services and characteristics automatically.");
draw_text(16, 56, "Use READ on the INFO characteristic to fetch text from the BLE server.");
draw_text(16, 76, "Buttons are created only for operations supported by each characteristic.");

var _y = 104;
for (var i = 0; i < array_length(log_lines); i++)
{
    draw_text(16, _y, log_lines[i]);
    _y += 14;
}
