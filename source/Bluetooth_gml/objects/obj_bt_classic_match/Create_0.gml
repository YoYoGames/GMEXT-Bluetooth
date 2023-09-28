
buffer_element_size = buffer_sizeof(buffer_f32);
buffer_chunck_size = 2 * buffer_element_size;

// We are always writing the same max amount of data per-frame.
// The buffer can be of type 'buffer_wrap' to avoid seek and delete
buffer_out_size = buffer_chunck_size;
buffer_out = buffer_create(buffer_chunck_size, buffer_wrap, buffer_element_size);

// We may lag behind the other device. We need to have a good amount of space
// on the receiving buffer for those cases.
buffer_in_size = 1024;
buffer_in = buffer_create(buffer_in_size, buffer_fixed, buffer_element_size);