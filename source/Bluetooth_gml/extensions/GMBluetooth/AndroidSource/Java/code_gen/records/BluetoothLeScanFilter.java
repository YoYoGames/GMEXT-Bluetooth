// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.records;

import ${YYAndroidPackageName}.GMExtWire;
import ${YYAndroidPackageName}.GMExtWire.GMValue;
import ${YYAndroidPackageName}.codecs.*;

import java.nio.ByteBuffer;
import java.util.Optional;
import java.util.List;

public record BluetoothLeScanFilter(java.util.Optional<String> service_uuid, java.util.Optional<String> name, java.util.Optional<String> name_prefix, java.util.Optional<Integer> company_id, java.util.Optional<java.util.List<Byte>> manufacturer_data, java.util.Optional<java.util.List<Byte>> manufacturer_data_mask, java.util.Optional<String> service_data_uuid, java.util.Optional<java.util.List<Byte>> service_data, java.util.Optional<java.util.List<Byte>> service_data_mask) implements GMExtWire.ITypedStruct
{
    public static final int CODEC_ID = 5;
    @Override
    public void encode(GMExtWire.IByteWriter b)
    {
        BluetoothLeScanFilterCodec.write(b, this);
    }
}
