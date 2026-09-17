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

        java.util.Optional<String> value = java.util.Optional.empty();
        if (GMExtWire.readBool(b))
        {
            String __opt_value = GMExtWire.readString(b);
            value = java.util.Optional.of(__opt_value);
        }

        java.util.List<BluetoothLeDescriptorDefinition> descriptors = GMExtWire.readList(b, bb -> BluetoothLeDescriptorDefinitionCodec.read(bb));

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
            GMExtWire.writeString(b, obj.value().get());
        }

        GMExtWire.writeList(b, obj.descriptors(), (bb, x) -> BluetoothLeDescriptorDefinitionCodec.write(bb, x));

    }
}