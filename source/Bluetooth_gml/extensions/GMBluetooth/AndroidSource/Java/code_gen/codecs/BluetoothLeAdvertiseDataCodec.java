// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.codecs;

import java.nio.ByteBuffer;

import ${YYAndroidPackageName}.GMExtWire;
import ${YYAndroidPackageName}.GMExtWire.GMValue;
import java.util.Optional;
import java.util.List;
import ${YYAndroidPackageName}.records.*;

public final class BluetoothLeAdvertiseDataCodec {
    private BluetoothLeAdvertiseDataCodec()
    {
    }
    public static BluetoothLeAdvertiseData read(ByteBuffer b)
    {
        java.util.Optional<Boolean> include_name = java.util.Optional.empty();
        if (GMExtWire.readBool(b))
        {
            boolean __opt_include_name = GMExtWire.readBool(b);
            include_name = java.util.Optional.of(__opt_include_name);
        }

        java.util.Optional<Boolean> include_tx_power = java.util.Optional.empty();
        if (GMExtWire.readBool(b))
        {
            boolean __opt_include_tx_power = GMExtWire.readBool(b);
            include_tx_power = java.util.Optional.of(__opt_include_tx_power);
        }

        java.util.Optional<java.util.List<String>> service_uuids = java.util.Optional.empty();
        if (GMExtWire.readBool(b))
        {
            java.util.List<String> __opt_service_uuids = GMExtWire.readList(b, bb -> GMExtWire.readString(bb));
            service_uuids = java.util.Optional.of(__opt_service_uuids);
        }

        java.util.Optional<java.util.List<BluetoothLeAdvertiseServiceData>> service_data = java.util.Optional.empty();
        if (GMExtWire.readBool(b))
        {
            java.util.List<BluetoothLeAdvertiseServiceData> __opt_service_data = GMExtWire.readList(b, bb -> BluetoothLeAdvertiseServiceDataCodec.read(bb));
            service_data = java.util.Optional.of(__opt_service_data);
        }

        java.util.Optional<java.util.List<BluetoothLeAdvertiseManufacturerData>> manufacturer_data = java.util.Optional.empty();
        if (GMExtWire.readBool(b))
        {
            java.util.List<BluetoothLeAdvertiseManufacturerData> __opt_manufacturer_data = GMExtWire.readList(b, bb -> BluetoothLeAdvertiseManufacturerDataCodec.read(bb));
            manufacturer_data = java.util.Optional.of(__opt_manufacturer_data);
        }

        return new BluetoothLeAdvertiseData(include_name, include_tx_power, service_uuids, service_data, manufacturer_data);
    }

    public static void write(GMExtWire.IByteWriter b, BluetoothLeAdvertiseData obj)
    {
        GMExtWire.writeBool(b, obj.include_name() != null && obj.include_name().isPresent());
        if (obj.include_name() != null && obj.include_name().isPresent())
        {
            GMExtWire.writeBool(b, obj.include_name().get());
        }

        GMExtWire.writeBool(b, obj.include_tx_power() != null && obj.include_tx_power().isPresent());
        if (obj.include_tx_power() != null && obj.include_tx_power().isPresent())
        {
            GMExtWire.writeBool(b, obj.include_tx_power().get());
        }

        GMExtWire.writeBool(b, obj.service_uuids() != null && obj.service_uuids().isPresent());
        if (obj.service_uuids() != null && obj.service_uuids().isPresent())
        {
            GMExtWire.writeList(b, obj.service_uuids().get(), (bb, x) -> GMExtWire.writeString(bb, x));
        }

        GMExtWire.writeBool(b, obj.service_data() != null && obj.service_data().isPresent());
        if (obj.service_data() != null && obj.service_data().isPresent())
        {
            GMExtWire.writeList(b, obj.service_data().get(), (bb, x) -> BluetoothLeAdvertiseServiceDataCodec.write(bb, x));
        }

        GMExtWire.writeBool(b, obj.manufacturer_data() != null && obj.manufacturer_data().isPresent());
        if (obj.manufacturer_data() != null && obj.manufacturer_data().isPresent())
        {
            GMExtWire.writeList(b, obj.manufacturer_data().get(), (bb, x) -> BluetoothLeAdvertiseManufacturerDataCodec.write(bb, x));
        }

    }
}