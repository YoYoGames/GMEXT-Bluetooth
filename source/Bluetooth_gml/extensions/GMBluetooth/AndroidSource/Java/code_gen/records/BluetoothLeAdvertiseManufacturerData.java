// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.records;

import ${YYAndroidPackageName}.GMExtWire;
import ${YYAndroidPackageName}.GMExtWire.GMValue;
import ${YYAndroidPackageName}.codecs.*;

import java.nio.ByteBuffer;
import java.util.List;

public record BluetoothLeAdvertiseManufacturerData(int company_id, java.util.List<Byte> data) implements GMExtWire.ITypedStruct
{
    public static final int CODEC_ID = 3;
    @Override
    public void encode(GMExtWire.IByteWriter b)
    {
        BluetoothLeAdvertiseManufacturerDataCodec.write(b, this);
    }
}
