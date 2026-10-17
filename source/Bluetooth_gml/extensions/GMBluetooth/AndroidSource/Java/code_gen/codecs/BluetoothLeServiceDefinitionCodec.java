// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.codecs;

import java.nio.ByteBuffer;

import ${YYAndroidPackageName}.GMExtWire;
import ${YYAndroidPackageName}.GMExtWire.GMValue;
import java.util.Optional;
import java.util.List;
import ${YYAndroidPackageName}.records.*;

public final class BluetoothLeServiceDefinitionCodec {
    private BluetoothLeServiceDefinitionCodec()
    {
    }
    public static BluetoothLeServiceDefinition read(ByteBuffer b)
    {
        String uuid = GMExtWire.readString(b);

        java.util.Optional<java.util.List<BluetoothLeCharacteristicDefinition>> characteristics = java.util.Optional.empty();
        if (GMExtWire.readBool(b))
        {
            java.util.List<BluetoothLeCharacteristicDefinition> __opt_characteristics = GMExtWire.readList(b, bb -> BluetoothLeCharacteristicDefinitionCodec.read(bb));
            characteristics = java.util.Optional.of(__opt_characteristics);
        }

        return new BluetoothLeServiceDefinition(uuid, characteristics);
    }

    public static void write(GMExtWire.IByteWriter b, BluetoothLeServiceDefinition obj)
    {
        GMExtWire.writeString(b, obj.uuid());

        GMExtWire.writeBool(b, obj.characteristics() != null && obj.characteristics().isPresent());
        if (obj.characteristics() != null && obj.characteristics().isPresent())
        {
            GMExtWire.writeList(b, obj.characteristics().get(), (bb, x) -> BluetoothLeCharacteristicDefinitionCodec.write(bb, x));
        }

    }
}