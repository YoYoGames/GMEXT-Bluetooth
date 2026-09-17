// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.codecs;

import java.nio.ByteBuffer;

import ${YYAndroidPackageName}.GMExtWire;
import ${YYAndroidPackageName}.GMExtWire.GMValue;
import ${YYAndroidPackageName}.records.*;

public final class BluetoothLeDescriptorDefinitionCodec {
    private BluetoothLeDescriptorDefinitionCodec()
    {
    }
    public static BluetoothLeDescriptorDefinition read(ByteBuffer b)
    {
        String uuid = GMExtWire.readString(b);

        return new BluetoothLeDescriptorDefinition(uuid);
    }

    public static void write(GMExtWire.IByteWriter b, BluetoothLeDescriptorDefinition obj)
    {
        GMExtWire.writeString(b, obj.uuid());

    }
}