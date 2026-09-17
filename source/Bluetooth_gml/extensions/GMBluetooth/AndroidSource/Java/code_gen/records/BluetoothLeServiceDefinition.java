// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.records;

import ${YYAndroidPackageName}.GMExtWire;
import ${YYAndroidPackageName}.GMExtWire.GMValue;
import ${YYAndroidPackageName}.codecs.*;

import java.nio.ByteBuffer;
import java.util.List;

public record BluetoothLeServiceDefinition(String uuid, java.util.List<BluetoothLeCharacteristicDefinition> characteristics) implements GMExtWire.ITypedStruct
{
    public static final int CODEC_ID = 2;
    @Override
    public void encode(GMExtWire.IByteWriter b)
    {
        BluetoothLeServiceDefinitionCodec.write(b, this);
    }
}
