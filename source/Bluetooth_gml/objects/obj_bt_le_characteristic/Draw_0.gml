// x is set by obj_bt_le_client_connection to the right of this row's buttons.
draw_set_font(fnt_gm_15);
draw_set_halign(fa_left);
draw_set_valign(fa_top);
draw_set_color(c_white);

var _line = 17;

draw_text(x, y, name + ": " + uuid);
draw_text(x, y + _line, "Properties: " + properties_text + " (" + string(properties) + ")");
draw_text(x, y + _line * 2, "Status: " + status_text);

if (value_text != "")
{
    draw_text(x, y + _line * 3, "Value: " + value_text);
}
