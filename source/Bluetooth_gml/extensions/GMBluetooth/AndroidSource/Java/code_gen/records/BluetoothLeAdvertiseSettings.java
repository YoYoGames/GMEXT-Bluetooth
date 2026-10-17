// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.records;

import ${YYAndroidPackageName}.GMExtWire;
import ${YYAndroidPackageName}.GMExtWire.GMValue;
import ${YYAndroidPackageName}.codecs.*;
import ${YYAndroidPackageName}.enums.*;

import java.nio.ByteBuffer;
import java.util.Optional;

public record BluetoothLeAdvertiseSettings(java.util.Optional<Boolean> connectable, java.util.Optional<BluetoothLeAdvertiseTxPower> tx_power) implements GMExtWire.ITypedStruct
{
    public static final int CODEC_ID = 2;
    @Override
    public void encode(GMExtWire.IByteWriter b)
    {
        BluetoothLeAdvertiseSettingsCodec.write(b, this);
    }
}
