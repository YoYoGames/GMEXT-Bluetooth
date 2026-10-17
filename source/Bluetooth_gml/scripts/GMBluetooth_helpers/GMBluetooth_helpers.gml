/// @function bluetooth_value_to_buffer(_value, _size)
/// @desc Fetches a value a callback delivered into a new buffer whose used size is the value's size, so buffer_copy, buffer_save and the base64 functions see every byte. The buffer is yours to delete.
/// @param {Real} _value The value handle the callback delivered.
/// @param {Real} _size The size the callback reported with it.
/// @returns {Id.Buffer} The new buffer, or -1 when the fetch failed, the reason in bluetooth_last_error_code().
function bluetooth_value_to_buffer(_value, _size) {
	var _buffer = buffer_create(max(_size, 1), buffer_grow, 1);
	if (bluetooth_value_fetch(_value, _buffer, 0) != BluetoothError.Ok) {
		buffer_delete(_buffer);
		return -1;
	}
	buffer_set_used_size(_buffer, _size);
	return _buffer;
}

/// @function bluetooth_value_append(_value, _size, _buffer)
/// @desc Fetches a value a callback delivered onto the end of a buffer, at its used size, growing the buffer when it is too small and moving its used size past the new bytes. Use it to gather a stream, such as a Classic connection's chunks.
/// @param {Real} _value The value handle the callback delivered.
/// @param {Real} _size The size the callback reported with it.
/// @param {Id.Buffer} _buffer The buffer to append to.
/// @returns {Enum.BluetoothError} The fetch's result; the used size moves only when it is Ok.
function bluetooth_value_append(_value, _size, _buffer) {
	var _used = buffer_get_used_size(_buffer);
	var _needed = _used + _size;
	if (buffer_get_size(_buffer) < _needed) {
		buffer_resize(_buffer, max(_needed, buffer_get_size(_buffer) * 2));
	}
	var _error = bluetooth_value_fetch(_value, _buffer, _used);
	if (_error == BluetoothError.Ok) {
		buffer_set_used_size(_buffer, _needed);
	}
	return _error;
}
