draw_set_halign(fa_left);
draw_set_valign(fa_top);
draw_set_color(c_white);

draw_text(x, y, "Characteristic: " + uuid);
draw_text(x, y + 20, "Properties: " + properties_text + " (" + string(properties) + ")");
draw_text(x, y + 40, "Status: " + status_text);

if (value_text != "")
{
    draw_text(x, y + 60, "Value: " + value_text);
}
