
show_debug_message("Pairing")

bluetooth_pair(device,
    function(_error_code, _message, _device) {
        show_debug_message("[GML] pair " + string(_error_code) + " " + _message);
        if (!instance_exists(id)) return;
        paired = bluetooth_device_is_paired(_device);
        update_text();
    });
