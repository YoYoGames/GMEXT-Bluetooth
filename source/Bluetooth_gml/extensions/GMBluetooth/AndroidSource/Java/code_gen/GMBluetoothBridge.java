package com.gamemaker.ExtensionCore.ExtBridge;
import java.lang.String;
import java.nio.ByteBuffer;
import ${YYAndroidPackageName}.GMExtUtils;

public final class GMBluetoothBridge {
    static {
        // this is the extension lib name
        System.loadLibrary("GMBluetooth");
        nativeRegister();
    }
    // this registers the native functions on the C++ layer
    private static native void nativeRegister();

    public static String __EXT_JAVA__GetExtensionOption(String extName, String optName)
    {
        return GMExtUtils.GetExtensionOption(extName, optName);
    }

    public static native double __EXT_JNI__GMBluetooth_invocation_handler(ByteBuffer __ret_buffer, double __ret_buffer_length);
    public static native double __EXT_JNI__bluetooth_initialize();
    public static native double __EXT_JNI__bluetooth_shutdown();
    public static native double __EXT_JNI__bluetooth_update();
    public static native double __EXT_JNI__bluetooth_is_initialized();
    public static native double __EXT_JNI__bluetooth_last_error_code();
    public static native String __EXT_JNI__bluetooth_last_error_message();
    public static native double __EXT_JNI__bluetooth_le_is_supported();
    public static native double __EXT_JNI__bluetooth_classic_is_supported();
    public static native double __EXT_JNI__bluetooth_le_scan_start(double active);
    public static native double __EXT_JNI__bluetooth_le_scan_stop();
    public static native double __EXT_JNI__bluetooth_le_scan_is_running();
    public static native double __EXT_JNI__bluetooth_device_clear();
    public static native double __EXT_JNI__bluetooth_device_get_count();
    public static native double __EXT_JNI__bluetooth_device_get_at(double index, ByteBuffer __ret_buffer, double __ret_buffer_length);
    public static native double __EXT_JNI__bluetooth_device_is_valid(ByteBuffer __arg_buffer, double __arg_buffer_length);
    public static native double __EXT_JNI__bluetooth_device_get_transport(ByteBuffer __arg_buffer, double __arg_buffer_length);
    public static native String __EXT_JNI__bluetooth_device_get_id(ByteBuffer __arg_buffer, double __arg_buffer_length);
    public static native String __EXT_JNI__bluetooth_device_get_name(ByteBuffer __arg_buffer, double __arg_buffer_length);
    public static native double __EXT_JNI__bluetooth_device_has_address(ByteBuffer __arg_buffer, double __arg_buffer_length);
    public static native String __EXT_JNI__bluetooth_device_get_address(ByteBuffer __arg_buffer, double __arg_buffer_length);
    public static native double __EXT_JNI__bluetooth_device_has_rssi(ByteBuffer __arg_buffer, double __arg_buffer_length);
    public static native double __EXT_JNI__bluetooth_device_get_rssi(ByteBuffer __arg_buffer, double __arg_buffer_length);
    public static native double __EXT_JNI__bluetooth_device_is_connectable(ByteBuffer __arg_buffer, double __arg_buffer_length);
    public static native double __EXT_JNI__bluetooth_set_callback_device_found(ByteBuffer __arg_buffer, double __arg_buffer_length);
    public static native double __EXT_JNI__bluetooth_remove_callback_device_found();
    public static native double __EXT_JNI__bluetooth_set_callback_scan_stopped(ByteBuffer __arg_buffer, double __arg_buffer_length);
    public static native double __EXT_JNI__bluetooth_remove_callback_scan_stopped();
}