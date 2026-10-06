// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.records;

import ${YYAndroidPackageName}.GMExtWire;
import ${YYAndroidPackageName}.GMExtWire.GMValue;
import ${YYAndroidPackageName}.codecs.*;

import java.nio.ByteBuffer;
import java.util.List;

public record BluetoothLeAdvertiseServiceData(String uuid, java.util.List<Byte> data) implements GMExtWire.ITypedStruct
{
    public static final int CODEC_ID = 2;
    @Override
    public void encode(GMExtWire.IByteWriter b)
    {
        BluetoothLeAdvertiseServiceDataCodec.write(b, this);
    }
}
