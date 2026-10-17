// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.codecs;

import java.nio.ByteBuffer;

import ${YYAndroidPackageName}.GMExtWire;
import ${YYAndroidPackageName}.GMExtWire.GMValue;
import java.util.Optional;
import java.util.List;
import ${YYAndroidPackageName}.records.*;

public final class BluetoothLeCharacteristicDefinitionCodec {
    private BluetoothLeCharacteristicDefinitionCodec()
    {
    }
    public static BluetoothLeCharacteristicDefinition read(ByteBuffer b)
    {
        String uuid = GMExtWire.readString(b);

        int properties = GMExtWire.readI32(b);

        int permissions = GMExtWire.readI32(b);

        java.util.Optional<java.util.List<Byte>> value = java.util.Optional.empty();
        if (GMExtWire.readBool(b))
        {
            java.util.List<Byte> __opt_value = GMExtWire.readList(b, bb -> GMExtWire.readI8(bb));
            value = java.util.Optional.of(__opt_value);
        }

        java.util.Optional<java.util.List<BluetoothLeDescriptorDefinition>> descriptors = java.util.Optional.empty();
        if (GMExtWire.readBool(b))
        {
            java.util.List<BluetoothLeDescriptorDefinition> __opt_descriptors = GMExtWire.readList(b, bb -> BluetoothLeDescriptorDefinitionCodec.read(bb));
            descriptors = java.util.Optional.of(__opt_descriptors);
        }

        return new BluetoothLeCharacteristicDefinition(uuid, properties, permissions, value, descriptors);
    }

    public static void write(GMExtWire.IByteWriter b, BluetoothLeCharacteristicDefinition obj)
    {
        GMExtWire.writeString(b, obj.uuid());

        GMExtWire.writeI32(b, obj.properties());

        GMExtWire.writeI32(b, obj.permissions());

        GMExtWire.writeBool(b, obj.value() != null && obj.value().isPresent());
        if (obj.value() != null && obj.value().isPresent())
        {
            GMExtWire.writeList(b, obj.value().get(), (bb, x) -> GMExtWire.writeI8(bb, x));
        }

        GMExtWire.writeBool(b, obj.descriptors() != null && obj.descriptors().isPresent());
        if (obj.descriptors() != null && obj.descriptors().isPresent())
        {
            GMExtWire.writeList(b, obj.descriptors().get(), (bb, x) -> BluetoothLeDescriptorDefinitionCodec.write(bb, x));
        }

    }
}