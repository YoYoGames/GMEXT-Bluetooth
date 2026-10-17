// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.records;

import ${YYAndroidPackageName}.GMExtWire;
import ${YYAndroidPackageName}.GMExtWire.GMValue;
import ${YYAndroidPackageName}.codecs.*;

import java.nio.ByteBuffer;
import java.util.Optional;

public record BluetoothConnectOptions(java.util.Optional<Integer> timeout_ms) implements GMExtWire.ITypedStruct
{
    public static final int CODEC_ID = 0;
    @Override
    public void encode(GMExtWire.IByteWriter b)
    {
        BluetoothConnectOptionsCodec.write(b, this);
    }
}
