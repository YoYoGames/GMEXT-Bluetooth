// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.records;

import ${YYAndroidPackageName}.GMExtWire;
import ${YYAndroidPackageName}.GMExtWire.GMValue;
import ${YYAndroidPackageName}.codecs.*;

import java.nio.ByteBuffer;
import java.util.Optional;
import java.util.List;

public record BluetoothLeDescriptorDefinition(String uuid, java.util.Optional<java.util.List<Byte>> value) implements GMExtWire.ITypedStruct
{
    public static final int CODEC_ID = 1;
    @Override
    public void encode(GMExtWire.IByteWriter b)
    {
        BluetoothLeDescriptorDefinitionCodec.write(b, this);
    }
}
