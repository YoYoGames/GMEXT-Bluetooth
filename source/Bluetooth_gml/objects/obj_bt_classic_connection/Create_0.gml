// connection is injected by whichever caller spawned this instance
// (obj_bt_classic_server's client_connected callback, or
// obj_bt_classic_device's connect-success callback), via
// instance_create_depth(..., {connection: _connection}).
remote_x = -1;
remote_y = -1;
has_remote = false;
send_log_counter = 0;

// RFCOMM is a raw byte stream, not message-framed: a single 8-byte (x,y) send
// can arrive split across multiple classic_data events, or coalesced with
// others into one. recv_buffer/recv_buffer_size accumulate bytes across calls
// so records are only parsed once a full 8 bytes are actually available.
recv_buffer = buffer_create(64, buffer_grow, 1);
recv_buffer_size = 0;
recv_log_counter = 0;

on_receive = function(_buf, _n) {
    if (_n <= 0) return;

    // TEMP DIAGNOSTIC: inspect the raw, untouched bytes exactly as handed to
    // us by the classic_data callback, before any of our own buffering runs.
    if (_n >= 8) {
        show_debug_message("[GML] raw _buf peek x=" + string(buffer_peek(_buf, 0, buffer_s32)) + " y=" + string(buffer_peek(_buf, 4, buffer_s32)) + " (n=" + string(_n) + ")");
    }

    // Grow the destination explicitly before writing past its current allocation.
    var _needed = recv_buffer_size + _n;
    if (_needed > buffer_get_size(recv_buffer)) {
        buffer_resize(recv_buffer, _needed);
    }

    // _buf is populated by the native bluetooth_classic_receive call, which writes
    // directly into the buffer's memory rather than through GML's own buffer_write/
    // buffer_poke API. That leaves the buffer's internal "written length" bookkeeping
    // at 0, and buffer_copy uses that bookkeeping (not the allocated capacity) to
    // decide how many source bytes are actually valid to copy - so buffer_copy(_buf,
    // ...) silently copies zero bytes even though buffer_peek(_buf, ...) can read the
    // real data fine. buffer_peek/buffer_poke are raw-position based and unaffected
    // by that bookkeeping, so copy byte-by-byte through them instead.
    for (var _i = 0; _i < _n; _i++) {
        buffer_poke(recv_buffer, recv_buffer_size + _i, buffer_u8, buffer_peek(_buf, _i, buffer_u8));
    }
    recv_buffer_size += _n;

    var _record_count = recv_buffer_size div 8;

    recv_log_counter++;
    if (recv_log_counter mod 10 == 0) {
        show_debug_message("[GML] on_receive n=" + string(_n) + " buffered=" + string(recv_buffer_size) + " records=" + string(_record_count));
    }

    if (_record_count > 0) {
        // Only the newest position matters, so jump straight to the last full record.
        var _last_offset = (_record_count - 1) * 8;
        remote_x = buffer_peek(recv_buffer, _last_offset, buffer_s32);
        remote_y = buffer_peek(recv_buffer, _last_offset + 4, buffer_s32);
        has_remote = true;

        // TEMP DIAGNOSTIC: unconditional (matches the raw _buf peek above) so the
        // two can be compared for the exact same on_receive call, not just every 10th.
        show_debug_message("[GML] parsed remote_x=" + string(remote_x) + " remote_y=" + string(remote_y) + " _last_offset=" + string(_last_offset) + " buffer_size=" + string(buffer_get_size(recv_buffer)));

        var _consumed = _record_count * 8;
        var _leftover = recv_buffer_size - _consumed;
        if (_leftover > 0) {
            // Shift the leftover bytes down to offset 0 for the next call. Since
            // recv_buffer is itself populated via buffer_poke above (not
            // buffer_write), its "written length" bookkeeping is unreliable too -
            // use the same peek/poke byte loop rather than buffer_copy, which would
            // be vulnerable to the same silent-zero-copy issue described above.
            for (var _i = 0; _i < _leftover; _i++) {
                buffer_poke(recv_buffer, _i, buffer_u8, buffer_peek(recv_buffer, _consumed + _i, buffer_u8));
            }
        }
        recv_buffer_size = _leftover;
    }
};
