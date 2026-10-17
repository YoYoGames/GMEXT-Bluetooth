// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.codecs;

import java.nio.ByteBuffer;

import ${YYAndroidPackageName}.GMExtWire;
import ${YYAndroidPackageName}.GMExtWire.GMValue;
import java.util.Optional;
import java.util.List;
import ${YYAndroidPackageName}.records.*;

public final class BluetoothLeDescriptorDefinitionCodec {
    private BluetoothLeDescriptorDefinitionCodec()
    {
    }
    public static BluetoothLeDescriptorDefinition read(ByteBuffer b)
    {
        String uuid = GMExtWire.readString(b);

        java.util.Optional<java.util.List<Byte>> value = java.util.Optional.empty();
        if (GMExtWire.readBool(b))
        {
            java.util.List<Byte> __opt_value = GMExtWire.readList(b, bb -> GMExtWire.readI8(bb));
            value = java.util.Optional.of(__opt_value);
        }

        return new BluetoothLeDescriptorDefinition(uuid, value);
    }

    public static void write(GMExtWire.IByteWriter b, BluetoothLeDescriptorDefinition obj)
    {
        GMExtWire.writeString(b, obj.uuid());

        GMExtWire.writeBool(b, obj.value() != null && obj.value().isPresent());
        if (obj.value() != null && obj.value().isPresent())
        {
            GMExtWire.writeList(b, obj.value().get(), (bb, x) -> GMExtWire.writeI8(bb, x));
        }

    }
}