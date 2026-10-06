// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.records;

import ${YYAndroidPackageName}.GMExtWire;
import ${YYAndroidPackageName}.GMExtWire.GMValue;
import ${YYAndroidPackageName}.codecs.*;

import java.nio.ByteBuffer;
import java.util.List;

public record BluetoothLeAdvertiseData(boolean include_name, boolean include_tx_power, java.util.List<String> service_uuids, java.util.List<BluetoothLeAdvertiseServiceData> service_data, java.util.List<BluetoothLeAdvertiseManufacturerData> manufacturer_data) implements GMExtWire.ITypedStruct
{
    public static final int CODEC_ID = 5;
    @Override
    public void encode(GMExtWire.IByteWriter b)
    {
        BluetoothLeAdvertiseDataCodec.write(b, this);
    }
}
