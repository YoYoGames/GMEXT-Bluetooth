// connection and owner arrive via the creation struct, from whichever
// controller spawned this instance (obj_bt_classic_server's on_connected, or
// obj_bt_classic_client's connect callback).
remote_x = -1;
remote_y = -1;
has_remote = false;
send_log_counter = 0;
send_fail_counter = 0;

// RFCOMM is a raw byte stream, not message-framed: one send can arrive split
// across several on_data calls, or coalesced with others into one. So every
// record carries its own frame, [u8 type][u16 length][payload], and
// recv_buffer gathers bytes up to its used size until a whole record is
// there. The record types are in bluetooth_classic_definitions.
recv_buffer = buffer_create(64, buffer_grow, 1);
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
			show_debug_message($"[GML] classic send failed: {bluetooth_last_error_message()}");
		}
		send_fail_counter++;
	}

	send_log_counter++;
	if (send_log_counter mod 30 == 0) {
		show_debug_message($"[GML] classic send x={mouse_x} y={mouse_y} result={_result}");
	}
};

send_text = function(_text) {
	var _size = string_byte_length(_text);
	var _payload = buffer_create(max(_size, 1), buffer_fixed, 1);
	buffer_write(_payload, buffer_text, _text);
	var _result = send_record(DEMO_CLASSIC_RECORD_TEXT, _payload, _size);
	buffer_delete(_payload);

	if (_result == BluetoothError.Ok) {
		show_debug_message($"[GML] classic TX text: {_text}");
	} else {
		show_debug_message($"[GML] classic text send failed: {bluetooth_last_error_message()}");
	}
};

// Forwarded from the controller's on_data for this connection's chunks.
on_receive_value = function(_value, _size) {
	var _error = bluetooth_value_append(_value, _size, recv_buffer);

	if (_error != BluetoothError.Ok) {
		show_debug_message($"[GML] classic chunk could not be fetched: {_error} {bluetooth_last_error_message()}");
		return;
	}

	var _buffered = buffer_get_used_size(recv_buffer);

	// Parse every whole record; a partial one waits for the next chunk.
	var _offset = 0;
	var _records = 0;
	while (_buffered - _offset >= DEMO_CLASSIC_RECORD_HEADER) {
		var _type = buffer_peek(recv_buffer, _offset, buffer_u8);
		var _length = buffer_peek(recv_buffer, _offset + 1, buffer_u16);
		var _payload = _offset + DEMO_CLASSIC_RECORD_HEADER;
		if (_buffered - _payload < _length) break;

		if (_type == DEMO_CLASSIC_RECORD_POSITION && _length == 8) {
			// Only the newest position matters; a later record overwrites it.
			remote_x = buffer_peek(recv_buffer, _payload, buffer_s32);
			remote_y = buffer_peek(recv_buffer, _payload + 4, buffer_s32);
			has_remote = true;
		} else if (_type == DEMO_CLASSIC_RECORD_TEXT) {
			// Copied out with a terminator, so buffer_string decodes the UTF-8.
			var _text_buf = buffer_create(_length + 1, buffer_fixed, 1);
			if (_length > 0) {
				buffer_copy(recv_buffer, _payload, _length, _text_buf, 0);
			}
			buffer_poke(_text_buf, _length, buffer_u8, 0);
			show_debug_message($"[GML] classic RX text: {buffer_peek(_text_buf, 0, buffer_string)}");
			buffer_delete(_text_buf);
		}
		// Any other type is skipped by its length.

		_offset = _payload + _length;
		_records++;
	}

	recv_log_counter++;
	if (recv_log_counter mod 10 == 0) {
		show_debug_message($"[GML] on_receive n={_size} buffered={_buffered} records={_records}");
	}

	if (_offset == 0) return;

	// buffer_copy cannot copy a buffer onto itself and a used size only
	// grows, so the leftover bytes move to a new buffer instead.
	var _leftover = _buffered - _offset;
	var _rest = buffer_create(max(_leftover, 64), buffer_grow, 1);
	if (_leftover > 0) {
		buffer_copy(recv_buffer, _offset, _leftover, _rest, 0);
	}
	buffer_delete(recv_buffer);
	recv_buffer = _rest;
};
