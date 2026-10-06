// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.records;

import ${YYAndroidPackageName}.GMExtWire;
import ${YYAndroidPackageName}.GMExtWire.GMValue;
import ${YYAndroidPackageName}.codecs.*;

import java.nio.ByteBuffer;
import java.util.Optional;

public record BluetoothLeScanFilter(java.util.Optional<String> service_uuid, java.util.Optional<String> name, java.util.Optional<Integer> company_id) implements GMExtWire.ITypedStruct
{
    public static final int CODEC_ID = 4;
    @Override
    public void encode(GMExtWire.IByteWriter b)
    {
        BluetoothLeScanFilterCodec.write(b, this);
    }
}
