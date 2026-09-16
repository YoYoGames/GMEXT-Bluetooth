draw_set_color(c_white);
draw_text(16, 16, "Client connected");

var _n = array_length(log_lines);
for (var i = 0; i < _n; i++) {
    draw_text(16, 40 + i * 20, log_lines[i]);
}
