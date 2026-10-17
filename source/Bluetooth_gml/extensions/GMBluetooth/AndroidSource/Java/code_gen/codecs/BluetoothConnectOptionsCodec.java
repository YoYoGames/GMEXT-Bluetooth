// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.codecs;

import java.nio.ByteBuffer;

import ${YYAndroidPackageName}.GMExtWire;
import ${YYAndroidPackageName}.GMExtWire.GMValue;
import java.util.Optional;
import ${YYAndroidPackageName}.records.*;

public final class BluetoothConnectOptionsCodec {
    private BluetoothConnectOptionsCodec()
    {
    }
    public static BluetoothConnectOptions read(ByteBuffer b)
    {
        java.util.Optional<Integer> timeout_ms = java.util.Optional.empty();
        if (GMExtWire.readBool(b))
        {
            int __opt_timeout_ms = GMExtWire.readI32(b);
            timeout_ms = java.util.Optional.of(__opt_timeout_ms);
        }

        return new BluetoothConnectOptions(timeout_ms);
    }

    public static void write(GMExtWire.IByteWriter b, BluetoothConnectOptions obj)
    {
        GMExtWire.writeBool(b, obj.timeout_ms() != null && obj.timeout_ms().isPresent());
        if (obj.timeout_ms() != null && obj.timeout_ms().isPresent())
        {
            GMExtWire.writeI32(b, obj.timeout_ms().get());
        }

    }
}