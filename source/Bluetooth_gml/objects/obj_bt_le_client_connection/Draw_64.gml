draw_text(16, 16, "Connected - browsing characteristics");

var _y = 32;
for (var i = 0; i < array_length(log_lines); i++) {
    draw_text(16, _y, log_lines[i]);
    _y += 16;
}
