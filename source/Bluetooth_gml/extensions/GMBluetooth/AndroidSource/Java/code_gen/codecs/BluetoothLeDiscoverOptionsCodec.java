// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.codecs;

import java.nio.ByteBuffer;

import ${YYAndroidPackageName}.GMExtWire;
import ${YYAndroidPackageName}.GMExtWire.GMValue;
import java.util.Optional;
import java.util.List;
import ${YYAndroidPackageName}.records.*;

public final class BluetoothLeDiscoverOptionsCodec {
    private BluetoothLeDiscoverOptionsCodec()
    {
    }
    public static BluetoothLeDiscoverOptions read(ByteBuffer b)
    {
        java.util.Optional<java.util.List<String>> service_uuids = java.util.Optional.empty();
        if (GMExtWire.readBool(b))
        {
            java.util.List<String> __opt_service_uuids = GMExtWire.readList(b, bb -> GMExtWire.readString(bb));
            service_uuids = java.util.Optional.of(__opt_service_uuids);
        }

        java.util.Optional<Boolean> characteristics = java.util.Optional.empty();
        if (GMExtWire.readBool(b))
        {
            boolean __opt_characteristics = GMExtWire.readBool(b);
            characteristics = java.util.Optional.of(__opt_characteristics);
        }

        java.util.Optional<Boolean> descriptors = java.util.Optional.empty();
        if (GMExtWire.readBool(b))
        {
            boolean __opt_descriptors = GMExtWire.readBool(b);
            descriptors = java.util.Optional.of(__opt_descriptors);
        }

        return new BluetoothLeDiscoverOptions(service_uuids, characteristics, descriptors);
    }

    public static void write(GMExtWire.IByteWriter b, BluetoothLeDiscoverOptions obj)
    {
        GMExtWire.writeBool(b, obj.service_uuids() != null && obj.service_uuids().isPresent());
        if (obj.service_uuids() != null && obj.service_uuids().isPresent())
        {
            GMExtWire.writeList(b, obj.service_uuids().get(), (bb, x) -> GMExtWire.writeString(bb, x));
        }

        GMExtWire.writeBool(b, obj.characteristics() != null && obj.characteristics().isPresent());
        if (obj.characteristics() != null && obj.characteristics().isPresent())
        {
            GMExtWire.writeBool(b, obj.characteristics().get());
        }

        GMExtWire.writeBool(b, obj.descriptors() != null && obj.descriptors().isPresent());
        if (obj.descriptors() != null && obj.descriptors().isPresent())
        {
            GMExtWire.writeBool(b, obj.descriptors().get());
        }

    }
}