package ${YYAndroidPackageName};
import static com.gamemaker.ExtensionCore.ExtBridge.GMBluetoothBridge.*;
import java.lang.String;
import java.nio.ByteBuffer;

public class GMBluetoothInternal extends RunnerSocial {
    public double __EXT_NATIVE__GMBluetooth_invocation_handler(ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        return __EXT_JNI__GMBluetooth_invocation_handler(__ret_buffer, __ret_buffer_length);
    }
    public double __EXT_NATIVE__bluetooth_initialize()
    {
        return __EXT_JNI__bluetooth_initialize();
    }
    public double __EXT_NATIVE__bluetooth_shutdown()
    {
        return __EXT_JNI__bluetooth_shutdown();
    }
    public double __EXT_NATIVE__bluetooth_update()
    {
        return __EXT_JNI__bluetooth_update();
    }
    public double __EXT_NATIVE__bluetooth_is_initialized()
    {
        return __EXT_JNI__bluetooth_is_initialized();
    }
    public double __EXT_NATIVE__bluetooth_last_error_code()
    {
        return __EXT_JNI__bluetooth_last_error_code();
    }
    public String __EXT_NATIVE__bluetooth_last_error_message()
    {
        return __EXT_JNI__bluetooth_last_error_message();
    }
    public double __EXT_NATIVE__bluetooth_le_is_supported()
    {
        return __EXT_JNI__bluetooth_le_is_supported();
    }
    public double __EXT_NATIVE__bluetooth_classic_is_supported()
    {
        return __EXT_JNI__bluetooth_classic_is_supported();
    }
    public double __EXT_NATIVE__bluetooth_le_scan_start(double active)
    {
        return __EXT_JNI__bluetooth_le_scan_start(active);
    }
    public double __EXT_NATIVE__bluetooth_le_scan_stop()
    {
        return __EXT_JNI__bluetooth_le_scan_stop();
    }
    public double __EXT_NATIVE__bluetooth_le_scan_is_running()
    {
        return __EXT_JNI__bluetooth_le_scan_is_running();
    }
    public double __EXT_NATIVE__bluetooth_device_clear()
    {
        return __EXT_JNI__bluetooth_device_clear();
    }
    public double __EXT_NATIVE__bluetooth_device_get_count()
    {
        return __EXT_JNI__bluetooth_device_get_count();
    }
    public double __EXT_NATIVE__bluetooth_device_get_at(double index, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        return __EXT_JNI__bluetooth_device_get_at(index, __ret_buffer, __ret_buffer_length);
    }
    public double __EXT_NATIVE__bluetooth_device_is_valid(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        return __EXT_JNI__bluetooth_device_is_valid(__arg_buffer, __arg_buffer_length);
    }
    public double __EXT_NATIVE__bluetooth_device_get_transport(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        return __EXT_JNI__bluetooth_device_get_transport(__arg_buffer, __arg_buffer_length);
    }
    public String __EXT_NATIVE__bluetooth_device_get_id(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        return __EXT_JNI__bluetooth_device_get_id(__arg_buffer, __arg_buffer_length);
    }
    public String __EXT_NATIVE__bluetooth_device_get_name(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        return __EXT_JNI__bluetooth_device_get_name(__arg_buffer, __arg_buffer_length);
    }
    public double __EXT_NATIVE__bluetooth_device_has_address(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        return __EXT_JNI__bluetooth_device_has_address(__arg_buffer, __arg_buffer_length);
    }
    public String __EXT_NATIVE__bluetooth_device_get_address(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        return __EXT_JNI__bluetooth_device_get_address(__arg_buffer, __arg_buffer_length);
    }
    public double __EXT_NATIVE__bluetooth_device_has_rssi(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        return __EXT_JNI__bluetooth_device_has_rssi(__arg_buffer, __arg_buffer_length);
    }
    public double __EXT_NATIVE__bluetooth_device_get_rssi(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        return __EXT_JNI__bluetooth_device_get_rssi(__arg_buffer, __arg_buffer_length);
    }
    public double __EXT_NATIVE__bluetooth_device_is_connectable(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        return __EXT_JNI__bluetooth_device_is_connectable(__arg_buffer, __arg_buffer_length);
    }
    public double __EXT_NATIVE__bluetooth_set_callback_device_found(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        return __EXT_JNI__bluetooth_set_callback_device_found(__arg_buffer, __arg_buffer_length);
    }
    public double __EXT_NATIVE__bluetooth_remove_callback_device_found()
    {
        return __EXT_JNI__bluetooth_remove_callback_device_found();
    }
    public double __EXT_NATIVE__bluetooth_set_callback_scan_stopped(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        return __EXT_JNI__bluetooth_set_callback_scan_stopped(__arg_buffer, __arg_buffer_length);
    }
    public double __EXT_NATIVE__bluetooth_remove_callback_scan_stopped()
    {
        return __EXT_JNI__bluetooth_remove_callback_scan_stopped();
    }
}