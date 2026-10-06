// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.codecs;

import java.nio.ByteBuffer;

import ${YYAndroidPackageName}.GMExtWire;
import ${YYAndroidPackageName}.GMExtWire.GMValue;
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

        java.util.List<Byte> value = GMExtWire.readList(b, bb -> GMExtWire.readI8(bb));

        java.util.List<BluetoothLeDescriptorDefinition> descriptors = GMExtWire.readList(b, bb -> BluetoothLeDescriptorDefinitionCodec.read(bb));

        return new BluetoothLeCharacteristicDefinition(uuid, properties, permissions, value, descriptors);
    }

    public static void write(GMExtWire.IByteWriter b, BluetoothLeCharacteristicDefinition obj)
    {
        GMExtWire.writeString(b, obj.uuid());

        GMExtWire.writeI32(b, obj.properties());

        GMExtWire.writeI32(b, obj.permissions());

        GMExtWire.writeList(b, obj.value(), (bb, x) -> GMExtWire.writeI8(bb, x));

        GMExtWire.writeList(b, obj.descriptors(), (bb, x) -> BluetoothLeDescriptorDefinitionCodec.write(bb, x));

    }
}