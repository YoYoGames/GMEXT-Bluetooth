// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName};

import java.nio.ByteBuffer;
import java.util.*;
import ${YYAndroidPackageName}.GMExtWire;
import ${YYAndroidPackageName}.GMExtWire.GMFunction;
import ${YYAndroidPackageName}.GMExtWire.GMValue;
import ${YYAndroidPackageName}.enums.*;

public abstract class GMBluetoothInternal extends RunnerSocial implements GMBluetoothInterface {

    private final GMExtWire.DispatchQueue __dispatch_queue = new GMExtWire.DispatchQueue();
    public double __EXT_NATIVE__GMBluetooth_invocation_handler(ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        return __dispatch_queue.fetch(__ret_buffer);
    }

    private final Deque<ByteBuffer> __buffer_queue = new ArrayDeque<>();
    public double __EXT_NATIVE__GMBluetooth_queue_buffer(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        __buffer_queue.offer(__arg_buffer);
        return 0;
    }

    public double __EXT_NATIVE__bluetooth_initialize()
    {
        boolean __result = bluetooth_initialize();
        return __result ? 1.0 : 0.0;
    }

    public double __EXT_NATIVE__bluetooth_shutdown()
    {
        bluetooth_shutdown();
        return 0;
    }

    public double __EXT_NATIVE__bluetooth_update()
    {
        int __result = bluetooth_update();
        return (double)__result;
    }

    public double __EXT_NATIVE__bluetooth_is_initialized()
    {
        boolean __result = bluetooth_is_initialized();
        return __result ? 1.0 : 0.0;
    }

    public double __EXT_NATIVE__bluetooth_last_error_code()
    {
        int __result = bluetooth_last_error_code();
        return (double)__result;
    }

    public String __EXT_NATIVE__bluetooth_last_error_message()
    {
        String __result = bluetooth_last_error_message();
        return __result;
    }

    public double __EXT_NATIVE__bluetooth_le_is_supported()
    {
        boolean __result = bluetooth_le_is_supported();
        return __result ? 1.0 : 0.0;
    }

    public double __EXT_NATIVE__bluetooth_classic_is_supported()
    {
        boolean __result = bluetooth_classic_is_supported();
        return __result ? 1.0 : 0.0;
    }

    public double __EXT_NATIVE__bluetooth_classic_server_is_supported()
    {
        boolean __result = bluetooth_classic_server_is_supported();
        return __result ? 1.0 : 0.0;
    }

    public double __EXT_NATIVE__bluetooth_permission_get_status()
    {
        int __result = bluetooth_permission_get_status();
        return (double)__result;
    }

    public double __EXT_NATIVE__bluetooth_permission_request()
    {
        int __result = bluetooth_permission_request();
        return (double)__result;
    }

    public double __EXT_NATIVE__bluetooth_le_scan_start(double active)
    {
        int __result = bluetooth_le_scan_start(active != 0);
        return (double)__result;
    }

    public double __EXT_NATIVE__bluetooth_le_scan_stop()
    {
        int __result = bluetooth_le_scan_stop();
        return (double)__result;
    }

    public double __EXT_NATIVE__bluetooth_le_scan_is_running()
    {
        boolean __result = bluetooth_le_scan_is_running();
        return __result ? 1.0 : 0.0;
    }

    public double __EXT_NATIVE__bluetooth_classic_scan_start()
    {
        int __result = bluetooth_classic_scan_start();
        return (double)__result;
    }

    public double __EXT_NATIVE__bluetooth_classic_scan_stop()
    {
        int __result = bluetooth_classic_scan_stop();
        return (double)__result;
    }

    public double __EXT_NATIVE__bluetooth_classic_scan_is_running()
    {
        boolean __result = bluetooth_classic_scan_is_running();
        return __result ? 1.0 : 0.0;
    }

    public double __EXT_NATIVE__bluetooth_device_clear()
    {
        bluetooth_device_clear();
        return 0;
    }

    public double __EXT_NATIVE__bluetooth_device_get_count()
    {
        int __result = bluetooth_device_get_count();
        return (double)__result;
    }

    public double __EXT_NATIVE__bluetooth_device_get_at(double index, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        long __result = bluetooth_device_get_at((int)index);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: UInt64
        GMExtWire.writeI64(__ret_buffer_writer, __result);

        return (double)__result;
    }

    public double __EXT_NATIVE__bluetooth_device_is_valid(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: device, type: UInt64
        long device = GMExtWire.readI64(__arg_buffer);

        boolean __result = bluetooth_device_is_valid(device);
        return __result ? 1.0 : 0.0;
    }

    public double __EXT_NATIVE__bluetooth_device_get_transport(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: device, type: UInt64
        long device = GMExtWire.readI64(__arg_buffer);

        int __result = bluetooth_device_get_transport(device);
        return (double)__result;
    }

    public String __EXT_NATIVE__bluetooth_device_get_id(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: device, type: UInt64
        long device = GMExtWire.readI64(__arg_buffer);

        String __result = bluetooth_device_get_id(device);
        return __result;
    }

    public String __EXT_NATIVE__bluetooth_device_get_name(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: device, type: UInt64
        long device = GMExtWire.readI64(__arg_buffer);

        String __result = bluetooth_device_get_name(device);
        return __result;
    }

    public double __EXT_NATIVE__bluetooth_device_has_address(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: device, type: UInt64
        long device = GMExtWire.readI64(__arg_buffer);

        boolean __result = bluetooth_device_has_address(device);
        return __result ? 1.0 : 0.0;
    }

    public String __EXT_NATIVE__bluetooth_device_get_address(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: device, type: UInt64
        long device = GMExtWire.readI64(__arg_buffer);

        String __result = bluetooth_device_get_address(device);
        return __result;
    }

    public double __EXT_NATIVE__bluetooth_device_has_rssi(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: device, type: UInt64
        long device = GMExtWire.readI64(__arg_buffer);

        boolean __result = bluetooth_device_has_rssi(device);
        return __result ? 1.0 : 0.0;
    }

    public double __EXT_NATIVE__bluetooth_device_get_rssi(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: device, type: UInt64
        long device = GMExtWire.readI64(__arg_buffer);

        int __result = bluetooth_device_get_rssi(device);
        return (double)__result;
    }

    public double __EXT_NATIVE__bluetooth_device_is_connectable(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: device, type: UInt64
        long device = GMExtWire.readI64(__arg_buffer);

        boolean __result = bluetooth_device_is_connectable(device);
        return __result ? 1.0 : 0.0;
    }

    public double __EXT_NATIVE__bluetooth_classic_connect(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: device, type: UInt64
        long device = GMExtWire.readI64(__arg_buffer);

        // field: service_uuid, type: String
        String service_uuid = GMExtWire.readString(__arg_buffer);

        // field: callback, type: Function
        GMFunction callback = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);

        long __result = bluetooth_classic_connect(device, service_uuid, callback);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: UInt64
        GMExtWire.writeI64(__ret_buffer_writer, __result);

        return (double)__result;
    }

    public double __EXT_NATIVE__bluetooth_classic_disconnect(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: connection, type: UInt64
        long connection = GMExtWire.readI64(__arg_buffer);

        int __result = bluetooth_classic_disconnect(connection);
        return (double)__result;
    }

    public double __EXT_NATIVE__bluetooth_classic_connection_is_valid(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: connection, type: UInt64
        long connection = GMExtWire.readI64(__arg_buffer);

        boolean __result = bluetooth_classic_connection_is_valid(connection);
        return __result ? 1.0 : 0.0;
    }

    public double __EXT_NATIVE__bluetooth_classic_connection_is_connected(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: connection, type: UInt64
        long connection = GMExtWire.readI64(__arg_buffer);

        boolean __result = bluetooth_classic_connection_is_connected(connection);
        return __result ? 1.0 : 0.0;
    }

    public double __EXT_NATIVE__bluetooth_classic_connection_get_device(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: connection, type: UInt64
        long connection = GMExtWire.readI64(__arg_buffer);

        long __result = bluetooth_classic_connection_get_device(connection);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: UInt64
        GMExtWire.writeI64(__ret_buffer_writer, __result);

        return (double)__result;
    }

    public double __EXT_NATIVE__bluetooth_classic_receive_available(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: connection, type: UInt64
        long connection = GMExtWire.readI64(__arg_buffer);

        int __result = bluetooth_classic_receive_available(connection);
        return (double)__result;
    }

    public double __EXT_NATIVE__bluetooth_classic_send(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: connection, type: UInt64
        long connection = GMExtWire.readI64(__arg_buffer);

        // field: data, type: Buffer
        java.nio.ByteBuffer data = __buffer_queue.poll();

        // field: offset, type: UInt32
        int offset = GMExtWire.readI32(__arg_buffer);

        // field: size, type: UInt32
        int size = GMExtWire.readI32(__arg_buffer);

        int __result = bluetooth_classic_send(connection, data, offset, size);
        return (double)__result;
    }

    public double __EXT_NATIVE__bluetooth_classic_receive(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: connection, type: UInt64
        long connection = GMExtWire.readI64(__arg_buffer);

        // field: out_data, type: Buffer
        java.nio.ByteBuffer out_data = __buffer_queue.poll();

        // field: offset, type: UInt32
        int offset = GMExtWire.readI32(__arg_buffer);

        // field: max_size, type: UInt32
        int max_size = GMExtWire.readI32(__arg_buffer);

        int __result = bluetooth_classic_receive(connection, out_data, offset, max_size);
        return (double)__result;
    }

    public double __EXT_NATIVE__bluetooth_classic_server_start(String name, String service_uuid)
    {
        int __result = bluetooth_classic_server_start(name, service_uuid);
        return (double)__result;
    }

    public double __EXT_NATIVE__bluetooth_classic_server_stop()
    {
        int __result = bluetooth_classic_server_stop();
        return (double)__result;
    }

    public double __EXT_NATIVE__bluetooth_classic_server_is_running()
    {
        boolean __result = bluetooth_classic_server_is_running();
        return __result ? 1.0 : 0.0;
    }

    public double __EXT_NATIVE__bluetooth_set_callback_device_found(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: callback, type: Function
        GMFunction callback = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);

        boolean __result = bluetooth_set_callback_device_found(callback);
        return __result ? 1.0 : 0.0;
    }

    public double __EXT_NATIVE__bluetooth_remove_callback_device_found()
    {
        boolean __result = bluetooth_remove_callback_device_found();
        return __result ? 1.0 : 0.0;
    }

    public double __EXT_NATIVE__bluetooth_set_callback_scan_stopped(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: callback, type: Function
        GMFunction callback = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);

        boolean __result = bluetooth_set_callback_scan_stopped(callback);
        return __result ? 1.0 : 0.0;
    }

    public double __EXT_NATIVE__bluetooth_remove_callback_scan_stopped()
    {
        boolean __result = bluetooth_remove_callback_scan_stopped();
        return __result ? 1.0 : 0.0;
    }

    public double __EXT_NATIVE__bluetooth_set_callback_classic_client_connected(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: callback, type: Function
        GMFunction callback = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);

        boolean __result = bluetooth_set_callback_classic_client_connected(callback);
        return __result ? 1.0 : 0.0;
    }

    public double __EXT_NATIVE__bluetooth_remove_callback_classic_client_connected()
    {
        boolean __result = bluetooth_remove_callback_classic_client_connected();
        return __result ? 1.0 : 0.0;
    }

    public double __EXT_NATIVE__bluetooth_set_callback_classic_data(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: callback, type: Function
        GMFunction callback = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);

        boolean __result = bluetooth_set_callback_classic_data(callback);
        return __result ? 1.0 : 0.0;
    }

    public double __EXT_NATIVE__bluetooth_remove_callback_classic_data()
    {
        boolean __result = bluetooth_remove_callback_classic_data();
        return __result ? 1.0 : 0.0;
    }

    public double __EXT_NATIVE__bluetooth_set_callback_classic_disconnected(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: callback, type: Function
        GMFunction callback = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);

        boolean __result = bluetooth_set_callback_classic_disconnected(callback);
        return __result ? 1.0 : 0.0;
    }

    public double __EXT_NATIVE__bluetooth_remove_callback_classic_disconnected()
    {
        boolean __result = bluetooth_remove_callback_classic_disconnected();
        return __result ? 1.0 : 0.0;
    }

}