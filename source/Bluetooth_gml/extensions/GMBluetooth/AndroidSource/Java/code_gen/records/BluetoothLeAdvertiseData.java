// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.records;

import ${YYAndroidPackageName}.GMExtWire;
import ${YYAndroidPackageName}.GMExtWire.GMValue;
import ${YYAndroidPackageName}.codecs.*;

import java.nio.ByteBuffer;
import java.util.Optional;
import java.util.List;

public record BluetoothLeAdvertiseData(java.util.Optional<Boolean> include_name, java.util.Optional<Boolean> include_tx_power, java.util.Optional<java.util.List<String>> service_uuids, java.util.Optional<java.util.List<BluetoothLeAdvertiseServiceData>> service_data, java.util.Optional<java.util.List<BluetoothLeAdvertiseManufacturerData>> manufacturer_data) implements GMExtWire.ITypedStruct
{
    public static final int CODEC_ID = 9;
    @Override
    public void encode(GMExtWire.IByteWriter b)
    {
        BluetoothLeAdvertiseDataCodec.write(b, this);
    }
}
