// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.codecs;

import java.nio.ByteBuffer;

import ${YYAndroidPackageName}.GMExtWire;
import ${YYAndroidPackageName}.GMExtWire.GMValue;
import java.util.List;
import ${YYAndroidPackageName}.records.*;

public final class BluetoothLeServiceDefinitionCodec {
    private BluetoothLeServiceDefinitionCodec()
    {
    }
    public static BluetoothLeServiceDefinition read(ByteBuffer b)
    {
        String uuid = GMExtWire.readString(b);

        java.util.List<BluetoothLeCharacteristicDefinition> characteristics = GMExtWire.readList(b, bb -> BluetoothLeCharacteristicDefinitionCodec.read(bb));

        return new BluetoothLeServiceDefinition(uuid, characteristics);
    }

    public static void write(GMExtWire.IByteWriter b, BluetoothLeServiceDefinition obj)
    {
        GMExtWire.writeString(b, obj.uuid());

        GMExtWire.writeList(b, obj.characteristics(), (bb, x) -> BluetoothLeCharacteristicDefinitionCodec.write(bb, x));

    }
}