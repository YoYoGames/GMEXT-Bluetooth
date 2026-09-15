draw_set_color(c_white);
draw_text(16, 16, "You: " + string(mouse_x) + ", " + string(mouse_y));

if (has_remote) {
    draw_set_color(c_red);
    draw_circle(remote_x, remote_y, 12, false);
    draw_text(remote_x + 16, remote_y - 8, "Peer");
    draw_set_color(c_white);
}
