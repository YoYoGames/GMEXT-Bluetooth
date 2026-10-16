// connection is injected by whichever caller spawned this instance
// (obj_bt_classic_server's client_connected callback, or
// obj_bt_classic_client's connect callback), via
// instance_create_depth(..., {connection: _connection}).
remote_x = -1;
remote_y = -1;
has_remote = false;
send_log_counter = 0;
send_fail_counter = 0;

// RFCOMM is a raw byte stream, not message-framed: one send can arrive split
// across several classic_data events, or coalesced with others into one. So
// every record carries its own frame, [u8 type][u16 length][payload], and
// recv_buffer/recv_buffer_size accumulate bytes until a whole record is there.
// The record types are in bluetooth_classic_definitions.

recv_buffer = buffer_create(64, buffer_grow, 1);
recv_buffer_size = 0;
recv_log_counter = 0;

// Sends one framed record; the payload is _size bytes of _payload from 0.
// Returns the BluetoothError of the send.
send_record = function(_type, _payload, _size) {
    var _buf = buffer_create(DEMO_CLASSIC_RECORD_HEADER + _size, buffer_fixed, 1);
    buffer_write(_buf, buffer_u8, _type);
    buffer_write(_buf, buffer_u16, _size);
    if (_size > 0) {
        buffer_copy(_payload, 0, _size, _buf, DEMO_CLASSIC_RECORD_HEADER);
    }
    var _result = bluetooth_classic_send(connection, _buf, 0, buffer_get_size(_buf));
    buffer_delete(_buf);
    return _result;
};

// Called every Step: a failure is logged the first time, then every 30th.
send_position = function() {
    var _payload = buffer_create(8, buffer_fixed, 1);
    buffer_write(_payload, buffer_s32, mouse_x);
    buffer_write(_payload, buffer_s32, mouse_y);
    var _result = send_record(DEMO_CLASSIC_RECORD_POSITION, _payload, 8);
    buffer_delete(_payload);

    if (_result != BluetoothError.Ok) {
        if (send_fail_counter mod 30 == 0) {
            show_debug_message("[GML] classic send failed: " + bluetooth_last_error_message());
        }
        send_fail_counter++;
    }

    send_log_counter++;
    if (send_log_counter mod 30 == 0) {
        show_debug_message("[GML] classic send x=" + string(mouse_x) + " y=" + string(mouse_y) + " result=" + string(_result));
    }
};

send_text = function(_text) {
    var _size = string_byte_length(_text);
    var _payload = buffer_create(max(_size, 1), buffer_fixed, 1);
    buffer_write(_payload, buffer_text, _text);
    var _result = send_record(DEMO_CLASSIC_RECORD_TEXT, _payload, _size);
    buffer_delete(_payload);

    if (_result == BluetoothError.Ok) {
        show_debug_message("[GML] classic TX text: " + _text);
    } else {
        show_debug_message("[GML] classic text send failed: " + bluetooth_last_error_message());
    }
};

on_receive = function(_buf, _n) {
    if (_n <= 0) return;

    // Grow the destination explicitly before writing past its current allocation.
    var _needed = recv_buffer_size + _n;
    if (_needed > buffer_get_size(recv_buffer)) {
        buffer_resize(recv_buffer, _needed);
    }

    // _buf is populated by the native bluetooth_value_fetch call, which writes
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

    // Parse every whole record; a partial one waits for the next call.
    var _offset = 0;
    var _records = 0;
    while (recv_buffer_size - _offset >= DEMO_CLASSIC_RECORD_HEADER) {
        var _type = buffer_peek(recv_buffer, _offset, buffer_u8);
        var _size = buffer_peek(recv_buffer, _offset + 1, buffer_u16);
        var _payload = _offset + DEMO_CLASSIC_RECORD_HEADER;
        if (recv_buffer_size - _payload < _size) break;

        if (_type == DEMO_CLASSIC_RECORD_POSITION && _size == 8) {
            // Only the newest position matters; a later record overwrites it.
            remote_x = buffer_peek(recv_buffer, _payload, buffer_s32);
            remote_y = buffer_peek(recv_buffer, _payload + 4, buffer_s32);
            has_remote = true;
        } else if (_type == DEMO_CLASSIC_RECORD_TEXT) {
            // Copied out with a terminator, so buffer_string decodes the UTF-8.
            var _text_buf = buffer_create(_size + 1, buffer_fixed, 1);
            for (var _i = 0; _i < _size; _i++) {
                buffer_poke(_text_buf, _i, buffer_u8, buffer_peek(recv_buffer, _payload + _i, buffer_u8));
            }
            buffer_poke(_text_buf, _size, buffer_u8, 0);
            show_debug_message("[GML] classic RX text: " + buffer_peek(_text_buf, 0, buffer_string));
            buffer_delete(_text_buf);
        }
        // Any other type is skipped by its length.

        _offset = _payload + _size;
        _records++;
    }

    recv_log_counter++;
    if (recv_log_counter mod 10 == 0) {
        show_debug_message("[GML] on_receive n=" + string(_n) + " buffered=" + string(recv_buffer_size) + " records=" + string(_records));
    }

    if (_offset > 0) {
        // Shift the leftover bytes down to offset 0 for the next call. Since
        // recv_buffer is itself populated via buffer_poke above (not
        // buffer_write), its "written length" bookkeeping is unreliable too -
        // use the same peek/poke byte loop rather than buffer_copy, which would
        // be vulnerable to the same silent-zero-copy issue described above.
        var _leftover = recv_buffer_size - _offset;
        for (var _i = 0; _i < _leftover; _i++) {
            buffer_poke(recv_buffer, _i, buffer_u8, buffer_peek(recv_buffer, _offset + _i, buffer_u8));
        }
        recv_buffer_size = _leftover;
    }
};
