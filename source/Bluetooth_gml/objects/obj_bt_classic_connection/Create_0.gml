// connection is injected by whichever caller spawned this instance
// (obj_bt_classic_server's client_connected callback, or
// obj_bt_classic_device's connect-success callback), via
// instance_create_depth(..., {connection: _connection}).
remote_x = -1;
remote_y = -1;
has_remote = false;

// RFCOMM is a raw byte stream, not message-framed: a single 8-byte (x,y) send
// can arrive split across multiple classic_data events, or coalesced with
// others into one. recv_buffer/recv_buffer_size accumulate bytes across calls
// so records are only parsed once a full 8 bytes are actually available.
recv_buffer = buffer_create(64, buffer_grow, 1);
recv_buffer_size = 0;

on_receive = function(_buf, _n) {
    if (_n <= 0) return;

    buffer_seek(recv_buffer, buffer_seek_start, recv_buffer_size);
    buffer_copy(_buf, 0, _n, recv_buffer, recv_buffer_size);
    recv_buffer_size += _n;

    var _record_count = recv_buffer_size div 8;
    if (_record_count > 0) {
        // Only the newest position matters, so jump straight to the last full record.
        var _last_offset = (_record_count - 1) * 8;
        remote_x = buffer_peek(recv_buffer, _last_offset, buffer_s32);
        remote_y = buffer_peek(recv_buffer, _last_offset + 4, buffer_s32);
        has_remote = true;

        var _consumed = _record_count * 8;
        var _leftover = recv_buffer_size - _consumed;
        if (_leftover > 0) buffer_copy(recv_buffer, _consumed, _leftover, recv_buffer, 0);
        recv_buffer_size = _leftover;
    }
};
