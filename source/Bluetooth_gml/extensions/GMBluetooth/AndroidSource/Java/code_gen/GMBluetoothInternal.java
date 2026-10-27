// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName};

import java.nio.ByteBuffer;
import java.util.*;
import ${YYAndroidPackageName}.GMExtWire;
import ${YYAndroidPackageName}.GMExtWire.GMFunction;
import ${YYAndroidPackageName}.GMExtWire.GMValue;
import ${YYAndroidPackageName}.records.*;
import ${YYAndroidPackageName}.codecs.*;
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

    public double __EXT_NATIVE__bluetooth_is_initialized()
    {
        boolean __result = bluetooth_is_initialized();
        return __result ? 1.0 : 0.0;
    }

    public double __EXT_NATIVE__bluetooth_last_error_code(ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        BluetoothError __result = bluetooth_last_error_code();

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: enum BluetoothError
        GMExtWire.writeI32(__ret_buffer_writer, __result.value());

        return 0;
    }

    public String __EXT_NATIVE__bluetooth_last_error_message()
    {
        String __result = bluetooth_last_error_message();
        return __result;
    }

    public double __EXT_NATIVE__bluetooth_feature_is_supported(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: feature, type: enum BluetoothFeature
        BluetoothFeature feature = BluetoothFeature.from(GMExtWire.readI32(__arg_buffer));

        boolean __result = bluetooth_feature_is_supported(feature);
        return __result ? 1.0 : 0.0;
    }

    public double __EXT_NATIVE__bluetooth_permission_get_status(ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        BluetoothPermissionStatus __result = bluetooth_permission_get_status();

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: enum BluetoothPermissionStatus
        GMExtWire.writeI32(__ret_buffer_writer, __result.value());

        return 0;
    }

    public double __EXT_NATIVE__bluetooth_permission_request(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: callback, type: Function
        GMFunction callback = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);

        BluetoothError __result = bluetooth_permission_request(callback);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: enum BluetoothError
        GMExtWire.writeI32(__ret_buffer_writer, __result.value());

        return 0;
    }

    public double __EXT_NATIVE__bluetooth_request_enable(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: callback, type: Function
        GMFunction callback = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);

        BluetoothError __result = bluetooth_request_enable(callback);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: enum BluetoothError
        GMExtWire.writeI32(__ret_buffer_writer, __result.value());

        return 0;
    }

    public double __EXT_NATIVE__bluetooth_le_scan_start(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: settings, type: optional<struct BluetoothLeScanSettings>
        java.util.Optional<BluetoothLeScanSettings> settings = java.util.Optional.empty();
        if (GMExtWire.readBool(__arg_buffer))
        {
            BluetoothLeScanSettings __opt_settings = BluetoothLeScanSettingsCodec.read(__arg_buffer);
            settings = java.util.Optional.of(__opt_settings);
        }

        // field: filters, type: optional<struct BluetoothLeScanFilter[]>
        java.util.Optional<java.util.List<BluetoothLeScanFilter>> filters = java.util.Optional.empty();
        if (GMExtWire.readBool(__arg_buffer))
        {
            java.util.List<BluetoothLeScanFilter> __opt_filters = GMExtWire.readList(__arg_buffer, bb -> BluetoothLeScanFilterCodec.read(bb));
            filters = java.util.Optional.of(__opt_filters);
        }

        // field: on_found, type: Function
        GMFunction on_found = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);

        // field: on_updated, type: optional<Function>
        java.util.Optional<GMFunction> on_updated = java.util.Optional.empty();
        if (GMExtWire.readBool(__arg_buffer))
        {
            GMFunction __opt_on_updated = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);
            on_updated = java.util.Optional.of(__opt_on_updated);
        }

        // field: on_stopped, type: optional<Function>
        java.util.Optional<GMFunction> on_stopped = java.util.Optional.empty();
        if (GMExtWire.readBool(__arg_buffer))
        {
            GMFunction __opt_on_stopped = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);
            on_stopped = java.util.Optional.of(__opt_on_stopped);
        }

        BluetoothError __result = bluetooth_le_scan_start(settings, filters, on_found, on_updated, on_stopped);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: enum BluetoothError
        GMExtWire.writeI32(__ret_buffer_writer, __result.value());

        return 0;
    }

    public double __EXT_NATIVE__bluetooth_le_scan_stop(ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        BluetoothError __result = bluetooth_le_scan_stop();

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: enum BluetoothError
        GMExtWire.writeI32(__ret_buffer_writer, __result.value());

        return 0;
    }

    public double __EXT_NATIVE__bluetooth_le_scan_is_running()
    {
        boolean __result = bluetooth_le_scan_is_running();
        return __result ? 1.0 : 0.0;
    }

    public double __EXT_NATIVE__bluetooth_classic_scan_start(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: on_found, type: Function
        GMFunction on_found = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);

        // field: on_updated, type: optional<Function>
        java.util.Optional<GMFunction> on_updated = java.util.Optional.empty();
        if (GMExtWire.readBool(__arg_buffer))
        {
            GMFunction __opt_on_updated = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);
            on_updated = java.util.Optional.of(__opt_on_updated);
        }

        // field: on_stopped, type: optional<Function>
        java.util.Optional<GMFunction> on_stopped = java.util.Optional.empty();
        if (GMExtWire.readBool(__arg_buffer))
        {
            GMFunction __opt_on_stopped = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);
            on_stopped = java.util.Optional.of(__opt_on_stopped);
        }

        BluetoothError __result = bluetooth_classic_scan_start(on_found, on_updated, on_stopped);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: enum BluetoothError
        GMExtWire.writeI32(__ret_buffer_writer, __result.value());

        return 0;
    }

    public double __EXT_NATIVE__bluetooth_classic_scan_stop(ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        BluetoothError __result = bluetooth_classic_scan_stop();

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: enum BluetoothError
        GMExtWire.writeI32(__ret_buffer_writer, __result.value());

        return 0;
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

    public double __EXT_NATIVE__bluetooth_device_get_transport(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: device, type: UInt64
        long device = GMExtWire.readI64(__arg_buffer);

        BluetoothTransport __result = bluetooth_device_get_transport(device);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: enum BluetoothTransport
        GMExtWire.writeI32(__ret_buffer_writer, __result.value());

        return 0;
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

    public double __EXT_NATIVE__bluetooth_device_get_address(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: device, type: UInt64
        long device = GMExtWire.readI64(__arg_buffer);

        java.util.Optional<String> __result = bluetooth_device_get_address(device);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: optional<String>
        GMExtWire.writeBool(__ret_buffer_writer, __result != null && __result.isPresent());
        if (__result != null && __result.isPresent())
        {
            GMExtWire.writeString(__ret_buffer_writer, __result.get());
        }

        return 0;
    }

    public double __EXT_NATIVE__bluetooth_device_get_rssi(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: device, type: UInt64
        long device = GMExtWire.readI64(__arg_buffer);

        java.util.Optional<Integer> __result = bluetooth_device_get_rssi(device);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: optional<Int32>
        GMExtWire.writeBool(__ret_buffer_writer, __result != null && __result.isPresent());
        if (__result != null && __result.isPresent())
        {
            GMExtWire.writeI32(__ret_buffer_writer, __result.get());
        }

        return 0;
    }

    public double __EXT_NATIVE__bluetooth_device_get_last_seen_age(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: device, type: UInt64
        long device = GMExtWire.readI64(__arg_buffer);

        java.util.Optional<Integer> __result = bluetooth_device_get_last_seen_age(device);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: optional<Int32>
        GMExtWire.writeBool(__ret_buffer_writer, __result != null && __result.isPresent());
        if (__result != null && __result.isPresent())
        {
            GMExtWire.writeI32(__ret_buffer_writer, __result.get());
        }

        return 0;
    }

    public double __EXT_NATIVE__bluetooth_device_is_connectable(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: device, type: UInt64
        long device = GMExtWire.readI64(__arg_buffer);

        boolean __result = bluetooth_device_is_connectable(device);
        return __result ? 1.0 : 0.0;
    }

    public double __EXT_NATIVE__bluetooth_device_get_advertisement(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: device, type: UInt64
        long device = GMExtWire.readI64(__arg_buffer);

        BluetoothLeAdvertisement __result = bluetooth_device_get_advertisement(device);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: struct BluetoothLeAdvertisement
        BluetoothLeAdvertisementCodec.write(__ret_buffer_writer, __result);

        return 0;
    }

    public double __EXT_NATIVE__bluetooth_device_from_id(String id, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        long __result = bluetooth_device_from_id(id);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: UInt64
        GMExtWire.writeI64(__ret_buffer_writer, __result);

        return (double)__result;
    }

    public double __EXT_NATIVE__bluetooth_le_connected_devices_query(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: service_uuids, type: String[]
        java.util.List<String> service_uuids = GMExtWire.readList(__arg_buffer, bb -> GMExtWire.readString(bb));

        // field: callback, type: Function
        GMFunction callback = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);

        BluetoothError __result = bluetooth_le_connected_devices_query(service_uuids, callback);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: enum BluetoothError
        GMExtWire.writeI32(__ret_buffer_writer, __result.value());

        return 0;
    }

    public double __EXT_NATIVE__bluetooth_paired_devices_query(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: callback, type: Function
        GMFunction callback = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);

        BluetoothError __result = bluetooth_paired_devices_query(callback);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: enum BluetoothError
        GMExtWire.writeI32(__ret_buffer_writer, __result.value());

        return 0;
    }

    public double __EXT_NATIVE__bluetooth_classic_connect(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: device, type: UInt64
        long device = GMExtWire.readI64(__arg_buffer);

        // field: service_uuid, type: String
        String service_uuid = GMExtWire.readString(__arg_buffer);

        // field: options, type: optional<struct BluetoothConnectOptions>
        java.util.Optional<BluetoothConnectOptions> options = java.util.Optional.empty();
        if (GMExtWire.readBool(__arg_buffer))
        {
            BluetoothConnectOptions __opt_options = BluetoothConnectOptionsCodec.read(__arg_buffer);
            options = java.util.Optional.of(__opt_options);
        }

        // field: on_data, type: Function
        GMFunction on_data = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);

        // field: on_disconnected, type: Function
        GMFunction on_disconnected = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);

        // field: callback, type: Function
        GMFunction callback = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);

        long __result = bluetooth_classic_connect(device, service_uuid, options, on_data, on_disconnected, callback);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: UInt64
        GMExtWire.writeI64(__ret_buffer_writer, __result);

        return (double)__result;
    }

    public double __EXT_NATIVE__bluetooth_classic_disconnect(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: connection, type: UInt64
        long connection = GMExtWire.readI64(__arg_buffer);

        BluetoothError __result = bluetooth_classic_disconnect(connection);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: enum BluetoothError
        GMExtWire.writeI32(__ret_buffer_writer, __result.value());

        return 0;
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

    public double __EXT_NATIVE__bluetooth_classic_send(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
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

        BluetoothError __result = bluetooth_classic_send(connection, data, offset, size);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: enum BluetoothError
        GMExtWire.writeI32(__ret_buffer_writer, __result.value());

        return 0;
    }

    public double __EXT_NATIVE__bluetooth_classic_server_start(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: name, type: String
        String name = GMExtWire.readString(__arg_buffer);

        // field: service_uuid, type: String
        String service_uuid = GMExtWire.readString(__arg_buffer);

        // field: on_connected, type: Function
        GMFunction on_connected = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);

        // field: on_data, type: Function
        GMFunction on_data = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);

        // field: on_disconnected, type: Function
        GMFunction on_disconnected = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);

        // field: on_stopped, type: optional<Function>
        java.util.Optional<GMFunction> on_stopped = java.util.Optional.empty();
        if (GMExtWire.readBool(__arg_buffer))
        {
            GMFunction __opt_on_stopped = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);
            on_stopped = java.util.Optional.of(__opt_on_stopped);
        }

        BluetoothError __result = bluetooth_classic_server_start(name, service_uuid, on_connected, on_data, on_disconnected, on_stopped);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: enum BluetoothError
        GMExtWire.writeI32(__ret_buffer_writer, __result.value());

        return 0;
    }

    public double __EXT_NATIVE__bluetooth_classic_server_stop(ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        BluetoothError __result = bluetooth_classic_server_stop();

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: enum BluetoothError
        GMExtWire.writeI32(__ret_buffer_writer, __result.value());

        return 0;
    }

    public double __EXT_NATIVE__bluetooth_classic_server_is_running()
    {
        boolean __result = bluetooth_classic_server_is_running();
        return __result ? 1.0 : 0.0;
    }

    public double __EXT_NATIVE__bluetooth_classic_discoverable_start(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: duration_seconds, type: Int32
        int duration_seconds = GMExtWire.readI32(__arg_buffer);

        // field: callback, type: Function
        GMFunction callback = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);

        // field: on_stopped, type: optional<Function>
        java.util.Optional<GMFunction> on_stopped = java.util.Optional.empty();
        if (GMExtWire.readBool(__arg_buffer))
        {
            GMFunction __opt_on_stopped = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);
            on_stopped = java.util.Optional.of(__opt_on_stopped);
        }

        BluetoothError __result = bluetooth_classic_discoverable_start(duration_seconds, callback, on_stopped);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: enum BluetoothError
        GMExtWire.writeI32(__ret_buffer_writer, __result.value());

        return 0;
    }

    public double __EXT_NATIVE__bluetooth_classic_discoverable_stop(ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        BluetoothError __result = bluetooth_classic_discoverable_stop();

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: enum BluetoothError
        GMExtWire.writeI32(__ret_buffer_writer, __result.value());

        return 0;
    }

    public double __EXT_NATIVE__bluetooth_classic_discoverable_is_running()
    {
        boolean __result = bluetooth_classic_discoverable_is_running();
        return __result ? 1.0 : 0.0;
    }

    public double __EXT_NATIVE__bluetooth_pairing_is_supported(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: device, type: UInt64
        long device = GMExtWire.readI64(__arg_buffer);

        boolean __result = bluetooth_pairing_is_supported(device);
        return __result ? 1.0 : 0.0;
    }

    public double __EXT_NATIVE__bluetooth_pair(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: device, type: UInt64
        long device = GMExtWire.readI64(__arg_buffer);

        // field: callback, type: Function
        GMFunction callback = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);

        BluetoothError __result = bluetooth_pair(device, callback);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: enum BluetoothError
        GMExtWire.writeI32(__ret_buffer_writer, __result.value());

        return 0;
    }

    public double __EXT_NATIVE__bluetooth_device_is_paired(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: device, type: UInt64
        long device = GMExtWire.readI64(__arg_buffer);

        boolean __result = bluetooth_device_is_paired(device);
        return __result ? 1.0 : 0.0;
    }

    public double __EXT_NATIVE__bluetooth_le_connect(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: device, type: UInt64
        long device = GMExtWire.readI64(__arg_buffer);

        // field: options, type: optional<struct BluetoothConnectOptions>
        java.util.Optional<BluetoothConnectOptions> options = java.util.Optional.empty();
        if (GMExtWire.readBool(__arg_buffer))
        {
            BluetoothConnectOptions __opt_options = BluetoothConnectOptionsCodec.read(__arg_buffer);
            options = java.util.Optional.of(__opt_options);
        }

        // field: on_disconnected, type: Function
        GMFunction on_disconnected = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);

        // field: callback, type: Function
        GMFunction callback = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);

        long __result = bluetooth_le_connect(device, options, on_disconnected, callback);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: UInt64
        GMExtWire.writeI64(__ret_buffer_writer, __result);

        return (double)__result;
    }

    public double __EXT_NATIVE__bluetooth_le_disconnect(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: connection, type: UInt64
        long connection = GMExtWire.readI64(__arg_buffer);

        BluetoothError __result = bluetooth_le_disconnect(connection);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: enum BluetoothError
        GMExtWire.writeI32(__ret_buffer_writer, __result.value());

        return 0;
    }

    public double __EXT_NATIVE__bluetooth_le_connection_is_valid(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: connection, type: UInt64
        long connection = GMExtWire.readI64(__arg_buffer);

        boolean __result = bluetooth_le_connection_is_valid(connection);
        return __result ? 1.0 : 0.0;
    }

    public double __EXT_NATIVE__bluetooth_le_connection_is_connected(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: connection, type: UInt64
        long connection = GMExtWire.readI64(__arg_buffer);

        boolean __result = bluetooth_le_connection_is_connected(connection);
        return __result ? 1.0 : 0.0;
    }

    public double __EXT_NATIVE__bluetooth_le_connection_get_device(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: connection, type: UInt64
        long connection = GMExtWire.readI64(__arg_buffer);

        long __result = bluetooth_le_connection_get_device(connection);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: UInt64
        GMExtWire.writeI64(__ret_buffer_writer, __result);

        return (double)__result;
    }

    public double __EXT_NATIVE__bluetooth_le_connection_get_mtu(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: connection, type: UInt64
        long connection = GMExtWire.readI64(__arg_buffer);

        int __result = bluetooth_le_connection_get_mtu(connection);
        return (double)__result;
    }

    public double __EXT_NATIVE__bluetooth_le_connection_request_mtu(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: connection, type: UInt64
        long connection = GMExtWire.readI64(__arg_buffer);

        // field: mtu, type: Int32
        int mtu = GMExtWire.readI32(__arg_buffer);

        // field: callback, type: Function
        GMFunction callback = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);

        BluetoothError __result = bluetooth_le_connection_request_mtu(connection, mtu, callback);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: enum BluetoothError
        GMExtWire.writeI32(__ret_buffer_writer, __result.value());

        return 0;
    }

    public double __EXT_NATIVE__bluetooth_le_connection_read_rssi(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: connection, type: UInt64
        long connection = GMExtWire.readI64(__arg_buffer);

        // field: callback, type: Function
        GMFunction callback = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);

        BluetoothError __result = bluetooth_le_connection_read_rssi(connection, callback);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: enum BluetoothError
        GMExtWire.writeI32(__ret_buffer_writer, __result.value());

        return 0;
    }

    public double __EXT_NATIVE__bluetooth_le_connection_request_priority(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: connection, type: UInt64
        long connection = GMExtWire.readI64(__arg_buffer);

        // field: priority, type: enum BluetoothLeConnectionPriority
        BluetoothLeConnectionPriority priority = BluetoothLeConnectionPriority.from(GMExtWire.readI32(__arg_buffer));

        BluetoothError __result = bluetooth_le_connection_request_priority(connection, priority);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: enum BluetoothError
        GMExtWire.writeI32(__ret_buffer_writer, __result.value());

        return 0;
    }

    public double __EXT_NATIVE__bluetooth_le_services_discover(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: connection, type: UInt64
        long connection = GMExtWire.readI64(__arg_buffer);

        // field: options, type: optional<struct BluetoothLeDiscoverOptions>
        java.util.Optional<BluetoothLeDiscoverOptions> options = java.util.Optional.empty();
        if (GMExtWire.readBool(__arg_buffer))
        {
            BluetoothLeDiscoverOptions __opt_options = BluetoothLeDiscoverOptionsCodec.read(__arg_buffer);
            options = java.util.Optional.of(__opt_options);
        }

        // field: callback, type: Function
        GMFunction callback = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);

        BluetoothError __result = bluetooth_le_services_discover(connection, options, callback);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: enum BluetoothError
        GMExtWire.writeI32(__ret_buffer_writer, __result.value());

        return 0;
    }

    public double __EXT_NATIVE__bluetooth_le_service_get_count(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: connection, type: UInt64
        long connection = GMExtWire.readI64(__arg_buffer);

        int __result = bluetooth_le_service_get_count(connection);
        return (double)__result;
    }

    public double __EXT_NATIVE__bluetooth_le_service_get_at(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: connection, type: UInt64
        long connection = GMExtWire.readI64(__arg_buffer);

        // field: index, type: Int32
        int index = GMExtWire.readI32(__arg_buffer);

        long __result = bluetooth_le_service_get_at(connection, index);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: UInt64
        GMExtWire.writeI64(__ret_buffer_writer, __result);

        return (double)__result;
    }

    public String __EXT_NATIVE__bluetooth_le_service_get_uuid(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: service, type: UInt64
        long service = GMExtWire.readI64(__arg_buffer);

        String __result = bluetooth_le_service_get_uuid(service);
        return __result;
    }

    public double __EXT_NATIVE__bluetooth_le_service_find(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: connection, type: UInt64
        long connection = GMExtWire.readI64(__arg_buffer);

        // field: uuid, type: String
        String uuid = GMExtWire.readString(__arg_buffer);

        long __result = bluetooth_le_service_find(connection, uuid);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: UInt64
        GMExtWire.writeI64(__ret_buffer_writer, __result);

        return (double)__result;
    }

    public double __EXT_NATIVE__bluetooth_le_characteristics_discover(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: service, type: UInt64
        long service = GMExtWire.readI64(__arg_buffer);

        // field: callback, type: Function
        GMFunction callback = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);

        BluetoothError __result = bluetooth_le_characteristics_discover(service, callback);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: enum BluetoothError
        GMExtWire.writeI32(__ret_buffer_writer, __result.value());

        return 0;
    }

    public double __EXT_NATIVE__bluetooth_le_characteristic_get_count(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: service, type: UInt64
        long service = GMExtWire.readI64(__arg_buffer);

        int __result = bluetooth_le_characteristic_get_count(service);
        return (double)__result;
    }

    public double __EXT_NATIVE__bluetooth_le_characteristic_get_at(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: service, type: UInt64
        long service = GMExtWire.readI64(__arg_buffer);

        // field: index, type: Int32
        int index = GMExtWire.readI32(__arg_buffer);

        long __result = bluetooth_le_characteristic_get_at(service, index);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: UInt64
        GMExtWire.writeI64(__ret_buffer_writer, __result);

        return (double)__result;
    }

    public String __EXT_NATIVE__bluetooth_le_characteristic_get_uuid(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: characteristic, type: UInt64
        long characteristic = GMExtWire.readI64(__arg_buffer);

        String __result = bluetooth_le_characteristic_get_uuid(characteristic);
        return __result;
    }

    public double __EXT_NATIVE__bluetooth_le_characteristic_find(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: service, type: UInt64
        long service = GMExtWire.readI64(__arg_buffer);

        // field: uuid, type: String
        String uuid = GMExtWire.readString(__arg_buffer);

        long __result = bluetooth_le_characteristic_find(service, uuid);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: UInt64
        GMExtWire.writeI64(__ret_buffer_writer, __result);

        return (double)__result;
    }

    public double __EXT_NATIVE__bluetooth_le_characteristic_get_properties(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: characteristic, type: UInt64
        long characteristic = GMExtWire.readI64(__arg_buffer);

        int __result = bluetooth_le_characteristic_get_properties(characteristic);
        return (double)__result;
    }

    public double __EXT_NATIVE__bluetooth_le_descriptors_discover(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: characteristic, type: UInt64
        long characteristic = GMExtWire.readI64(__arg_buffer);

        // field: callback, type: Function
        GMFunction callback = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);

        BluetoothError __result = bluetooth_le_descriptors_discover(characteristic, callback);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: enum BluetoothError
        GMExtWire.writeI32(__ret_buffer_writer, __result.value());

        return 0;
    }

    public double __EXT_NATIVE__bluetooth_le_descriptor_get_count(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: characteristic, type: UInt64
        long characteristic = GMExtWire.readI64(__arg_buffer);

        int __result = bluetooth_le_descriptor_get_count(characteristic);
        return (double)__result;
    }

    public double __EXT_NATIVE__bluetooth_le_descriptor_get_at(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: characteristic, type: UInt64
        long characteristic = GMExtWire.readI64(__arg_buffer);

        // field: index, type: Int32
        int index = GMExtWire.readI32(__arg_buffer);

        long __result = bluetooth_le_descriptor_get_at(characteristic, index);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: UInt64
        GMExtWire.writeI64(__ret_buffer_writer, __result);

        return (double)__result;
    }

    public String __EXT_NATIVE__bluetooth_le_descriptor_get_uuid(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: descriptor, type: UInt64
        long descriptor = GMExtWire.readI64(__arg_buffer);

        String __result = bluetooth_le_descriptor_get_uuid(descriptor);
        return __result;
    }

    public double __EXT_NATIVE__bluetooth_le_descriptor_find(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: characteristic, type: UInt64
        long characteristic = GMExtWire.readI64(__arg_buffer);

        // field: uuid, type: String
        String uuid = GMExtWire.readString(__arg_buffer);

        long __result = bluetooth_le_descriptor_find(characteristic, uuid);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: UInt64
        GMExtWire.writeI64(__ret_buffer_writer, __result);

        return (double)__result;
    }

    public double __EXT_NATIVE__bluetooth_le_characteristic_read(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: characteristic, type: UInt64
        long characteristic = GMExtWire.readI64(__arg_buffer);

        // field: callback, type: Function
        GMFunction callback = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);

        BluetoothError __result = bluetooth_le_characteristic_read(characteristic, callback);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: enum BluetoothError
        GMExtWire.writeI32(__ret_buffer_writer, __result.value());

        return 0;
    }

    public double __EXT_NATIVE__bluetooth_le_characteristic_write(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: characteristic, type: UInt64
        long characteristic = GMExtWire.readI64(__arg_buffer);

        // field: data, type: Buffer
        java.nio.ByteBuffer data = __buffer_queue.poll();

        // field: offset, type: UInt32
        int offset = GMExtWire.readI32(__arg_buffer);

        // field: size, type: UInt32
        int size = GMExtWire.readI32(__arg_buffer);

        // field: write_type, type: enum BluetoothLeWriteType
        BluetoothLeWriteType write_type = BluetoothLeWriteType.from(GMExtWire.readI32(__arg_buffer));

        // field: callback, type: Function
        GMFunction callback = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);

        BluetoothError __result = bluetooth_le_characteristic_write(characteristic, data, offset, size, write_type, callback);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: enum BluetoothError
        GMExtWire.writeI32(__ret_buffer_writer, __result.value());

        return 0;
    }

    public double __EXT_NATIVE__bluetooth_le_characteristic_subscribe(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: characteristic, type: UInt64
        long characteristic = GMExtWire.readI64(__arg_buffer);

        // field: mode, type: enum BluetoothLeSubscribeMode
        BluetoothLeSubscribeMode mode = BluetoothLeSubscribeMode.from(GMExtWire.readI32(__arg_buffer));

        // field: on_value, type: Function
        GMFunction on_value = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);

        // field: callback, type: Function
        GMFunction callback = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);

        BluetoothError __result = bluetooth_le_characteristic_subscribe(characteristic, mode, on_value, callback);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: enum BluetoothError
        GMExtWire.writeI32(__ret_buffer_writer, __result.value());

        return 0;
    }

    public double __EXT_NATIVE__bluetooth_le_characteristic_unsubscribe(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: characteristic, type: UInt64
        long characteristic = GMExtWire.readI64(__arg_buffer);

        // field: callback, type: Function
        GMFunction callback = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);

        BluetoothError __result = bluetooth_le_characteristic_unsubscribe(characteristic, callback);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: enum BluetoothError
        GMExtWire.writeI32(__ret_buffer_writer, __result.value());

        return 0;
    }

    public double __EXT_NATIVE__bluetooth_le_descriptor_read(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: descriptor, type: UInt64
        long descriptor = GMExtWire.readI64(__arg_buffer);

        // field: callback, type: Function
        GMFunction callback = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);

        BluetoothError __result = bluetooth_le_descriptor_read(descriptor, callback);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: enum BluetoothError
        GMExtWire.writeI32(__ret_buffer_writer, __result.value());

        return 0;
    }

    public double __EXT_NATIVE__bluetooth_le_descriptor_write(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: descriptor, type: UInt64
        long descriptor = GMExtWire.readI64(__arg_buffer);

        // field: data, type: Buffer
        java.nio.ByteBuffer data = __buffer_queue.poll();

        // field: offset, type: UInt32
        int offset = GMExtWire.readI32(__arg_buffer);

        // field: size, type: UInt32
        int size = GMExtWire.readI32(__arg_buffer);

        // field: callback, type: Function
        GMFunction callback = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);

        BluetoothError __result = bluetooth_le_descriptor_write(descriptor, data, offset, size, callback);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: enum BluetoothError
        GMExtWire.writeI32(__ret_buffer_writer, __result.value());

        return 0;
    }

    public double __EXT_NATIVE__bluetooth_value_fetch(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: value, type: UInt64
        long value = GMExtWire.readI64(__arg_buffer);

        // field: out_data, type: Buffer
        java.nio.ByteBuffer out_data = __buffer_queue.poll();

        // field: offset, type: UInt32
        int offset = GMExtWire.readI32(__arg_buffer);

        BluetoothError __result = bluetooth_value_fetch(value, out_data, offset);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: enum BluetoothError
        GMExtWire.writeI32(__ret_buffer_writer, __result.value());

        return 0;
    }

    public double __EXT_NATIVE__bluetooth_value_release(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: value, type: UInt64
        long value = GMExtWire.readI64(__arg_buffer);

        BluetoothError __result = bluetooth_value_release(value);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: enum BluetoothError
        GMExtWire.writeI32(__ret_buffer_writer, __result.value());

        return 0;
    }

    public double __EXT_NATIVE__bluetooth_le_advertise_start(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: settings, type: optional<struct BluetoothLeAdvertiseSettings>
        java.util.Optional<BluetoothLeAdvertiseSettings> settings = java.util.Optional.empty();
        if (GMExtWire.readBool(__arg_buffer))
        {
            BluetoothLeAdvertiseSettings __opt_settings = BluetoothLeAdvertiseSettingsCodec.read(__arg_buffer);
            settings = java.util.Optional.of(__opt_settings);
        }

        // field: data, type: struct BluetoothLeAdvertiseData
        BluetoothLeAdvertiseData data = BluetoothLeAdvertiseDataCodec.read(__arg_buffer);

        // field: on_stopped, type: optional<Function>
        java.util.Optional<GMFunction> on_stopped = java.util.Optional.empty();
        if (GMExtWire.readBool(__arg_buffer))
        {
            GMFunction __opt_on_stopped = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);
            on_stopped = java.util.Optional.of(__opt_on_stopped);
        }

        // field: callback, type: Function
        GMFunction callback = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);

        BluetoothError __result = bluetooth_le_advertise_start(settings, data, on_stopped, callback);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: enum BluetoothError
        GMExtWire.writeI32(__ret_buffer_writer, __result.value());

        return 0;
    }

    public double __EXT_NATIVE__bluetooth_le_advertise_stop(ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        BluetoothError __result = bluetooth_le_advertise_stop();

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: enum BluetoothError
        GMExtWire.writeI32(__ret_buffer_writer, __result.value());

        return 0;
    }

    public double __EXT_NATIVE__bluetooth_le_advertise_is_running()
    {
        boolean __result = bluetooth_le_advertise_is_running();
        return __result ? 1.0 : 0.0;
    }

    public double __EXT_NATIVE__bluetooth_le_server_start(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: on_connected, type: Function
        GMFunction on_connected = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);

        // field: on_disconnected, type: Function
        GMFunction on_disconnected = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);

        // field: on_read, type: Function
        GMFunction on_read = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);

        // field: on_write, type: Function
        GMFunction on_write = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);

        // field: on_stopped, type: optional<Function>
        java.util.Optional<GMFunction> on_stopped = java.util.Optional.empty();
        if (GMExtWire.readBool(__arg_buffer))
        {
            GMFunction __opt_on_stopped = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);
            on_stopped = java.util.Optional.of(__opt_on_stopped);
        }

        BluetoothError __result = bluetooth_le_server_start(on_connected, on_disconnected, on_read, on_write, on_stopped);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: enum BluetoothError
        GMExtWire.writeI32(__ret_buffer_writer, __result.value());

        return 0;
    }

    public double __EXT_NATIVE__bluetooth_le_server_stop(ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        BluetoothError __result = bluetooth_le_server_stop();

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: enum BluetoothError
        GMExtWire.writeI32(__ret_buffer_writer, __result.value());

        return 0;
    }

    public double __EXT_NATIVE__bluetooth_le_server_is_running()
    {
        boolean __result = bluetooth_le_server_is_running();
        return __result ? 1.0 : 0.0;
    }

    public double __EXT_NATIVE__bluetooth_le_server_add_service(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: service, type: struct BluetoothLeServiceDefinition
        BluetoothLeServiceDefinition service = BluetoothLeServiceDefinitionCodec.read(__arg_buffer);

        // field: callback, type: Function
        GMFunction callback = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);

        BluetoothError __result = bluetooth_le_server_add_service(service, callback);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: enum BluetoothError
        GMExtWire.writeI32(__ret_buffer_writer, __result.value());

        return 0;
    }

    public double __EXT_NATIVE__bluetooth_le_server_clear_services(ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        BluetoothError __result = bluetooth_le_server_clear_services();

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: enum BluetoothError
        GMExtWire.writeI32(__ret_buffer_writer, __result.value());

        return 0;
    }

    public double __EXT_NATIVE__bluetooth_le_server_respond_read(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: request_id, type: Int32
        int request_id = GMExtWire.readI32(__arg_buffer);

        // field: data, type: Buffer
        java.nio.ByteBuffer data = __buffer_queue.poll();

        // field: offset, type: UInt32
        int offset = GMExtWire.readI32(__arg_buffer);

        // field: size, type: UInt32
        int size = GMExtWire.readI32(__arg_buffer);

        BluetoothError __result = bluetooth_le_server_respond_read(request_id, data, offset, size);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: enum BluetoothError
        GMExtWire.writeI32(__ret_buffer_writer, __result.value());

        return 0;
    }

    public double __EXT_NATIVE__bluetooth_le_server_respond_read_error(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: request_id, type: Int32
        int request_id = GMExtWire.readI32(__arg_buffer);

        // field: error_code, type: enum BluetoothAttError
        BluetoothAttError error_code = BluetoothAttError.from(GMExtWire.readI32(__arg_buffer));

        BluetoothError __result = bluetooth_le_server_respond_read_error(request_id, error_code);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: enum BluetoothError
        GMExtWire.writeI32(__ret_buffer_writer, __result.value());

        return 0;
    }

    public double __EXT_NATIVE__bluetooth_le_server_respond_write(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: request_id, type: Int32
        int request_id = GMExtWire.readI32(__arg_buffer);

        // field: error_code, type: enum BluetoothAttError
        BluetoothAttError error_code = BluetoothAttError.from(GMExtWire.readI32(__arg_buffer));

        BluetoothError __result = bluetooth_le_server_respond_write(request_id, error_code);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: enum BluetoothError
        GMExtWire.writeI32(__ret_buffer_writer, __result.value());

        return 0;
    }

    public double __EXT_NATIVE__bluetooth_le_server_notify_value(ByteBuffer __arg_buffer, double __arg_buffer_length, ByteBuffer __ret_buffer, double __ret_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: characteristic, type: UInt64
        long characteristic = GMExtWire.readI64(__arg_buffer);

        // field: connection, type: UInt64
        long connection = GMExtWire.readI64(__arg_buffer);

        // field: data, type: Buffer
        java.nio.ByteBuffer data = __buffer_queue.poll();

        // field: offset, type: UInt32
        int offset = GMExtWire.readI32(__arg_buffer);

        // field: size, type: UInt32
        int size = GMExtWire.readI32(__arg_buffer);

        BluetoothError __result = bluetooth_le_server_notify_value(characteristic, connection, data, offset, size);

        GMExtWire.order(__ret_buffer);
        GMExtWire.IByteWriter __ret_buffer_writer = new GMExtWire.GMBufferWriter(__ret_buffer);
        // return: __result, type: enum BluetoothError
        GMExtWire.writeI32(__ret_buffer_writer, __result.value());

        return 0;
    }

    public double __EXT_NATIVE__bluetooth_set_callback_state_changed(ByteBuffer __arg_buffer, double __arg_buffer_length)
    {
        GMExtWire.order(__arg_buffer);

        // field: callback, type: Function
        GMFunction callback = GMExtWire.readGMFunction(__arg_buffer, __dispatch_queue);

        boolean __result = bluetooth_set_callback_state_changed(callback);
        return __result ? 1.0 : 0.0;
    }

    public double __EXT_NATIVE__bluetooth_remove_callback_state_changed()
    {
        boolean __result = bluetooth_remove_callback_state_changed();
        return __result ? 1.0 : 0.0;
    }

}