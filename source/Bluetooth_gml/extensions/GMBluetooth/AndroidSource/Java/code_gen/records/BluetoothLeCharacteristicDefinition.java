// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.records;

import ${YYAndroidPackageName}.GMExtWire;
import ${YYAndroidPackageName}.GMExtWire.GMValue;
import ${YYAndroidPackageName}.codecs.*;

import java.nio.ByteBuffer;
import java.util.Optional;
import java.util.List;

public record BluetoothLeCharacteristicDefinition(String uuid, int properties, int permissions, java.util.Optional<String> value, java.util.List<BluetoothLeDescriptorDefinition> descriptors) implements GMExtWire.ITypedStruct
{
    public static final int CODEC_ID = 1;
    @Override
    public void encode(GMExtWire.IByteWriter b)
    {
        BluetoothLeCharacteristicDefinitionCodec.write(b, this);
    }
}
