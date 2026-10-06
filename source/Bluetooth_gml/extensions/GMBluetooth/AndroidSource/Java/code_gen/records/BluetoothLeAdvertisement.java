// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.records;

import ${YYAndroidPackageName}.GMExtWire;
import ${YYAndroidPackageName}.GMExtWire.GMValue;
import ${YYAndroidPackageName}.codecs.*;

import java.nio.ByteBuffer;
import java.util.Optional;
import java.util.List;

public record BluetoothLeAdvertisement(java.util.List<String> service_uuids, java.util.List<BluetoothLeAdvertiseServiceData> service_data, java.util.List<BluetoothLeAdvertiseManufacturerData> manufacturer_data, java.util.Optional<Integer> tx_power) implements GMExtWire.ITypedStruct
{
    public static final int CODEC_ID = 7;
    @Override
    public void encode(GMExtWire.IByteWriter b)
    {
        BluetoothLeAdvertisementCodec.write(b, this);
    }
}
