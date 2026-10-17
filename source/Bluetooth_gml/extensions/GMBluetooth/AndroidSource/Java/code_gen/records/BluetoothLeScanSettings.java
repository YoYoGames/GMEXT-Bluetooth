// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.records;

import ${YYAndroidPackageName}.GMExtWire;
import ${YYAndroidPackageName}.GMExtWire.GMValue;
import ${YYAndroidPackageName}.codecs.*;
import ${YYAndroidPackageName}.enums.*;

import java.nio.ByteBuffer;
import java.util.Optional;

public record BluetoothLeScanSettings(java.util.Optional<Boolean> active, java.util.Optional<BluetoothLeScanMode> mode, java.util.Optional<Integer> update_interval_ms) implements GMExtWire.ITypedStruct
{
    public static final int CODEC_ID = 6;
    @Override
    public void encode(GMExtWire.IByteWriter b)
    {
        BluetoothLeScanSettingsCodec.write(b, this);
    }
}
