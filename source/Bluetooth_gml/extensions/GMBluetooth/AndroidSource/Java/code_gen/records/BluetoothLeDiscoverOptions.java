// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.records;

import ${YYAndroidPackageName}.GMExtWire;
import ${YYAndroidPackageName}.GMExtWire.GMValue;
import ${YYAndroidPackageName}.codecs.*;

import java.nio.ByteBuffer;
import java.util.Optional;
import java.util.List;

public record BluetoothLeDiscoverOptions(java.util.Optional<java.util.List<String>> service_uuids, java.util.Optional<Boolean> characteristics, java.util.Optional<Boolean> descriptors) implements GMExtWire.ITypedStruct
{
    public static final int CODEC_ID = 7;
    @Override
    public void encode(GMExtWire.IByteWriter b)
    {
        BluetoothLeDiscoverOptionsCodec.write(b, this);
    }
}
