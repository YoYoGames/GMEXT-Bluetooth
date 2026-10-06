// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.codecs;

import java.nio.ByteBuffer;

import ${YYAndroidPackageName}.GMExtWire;
import ${YYAndroidPackageName}.GMExtWire.GMValue;
import java.util.List;
import ${YYAndroidPackageName}.records.*;

public final class BluetoothLeAdvertiseServiceDataCodec {
    private BluetoothLeAdvertiseServiceDataCodec()
    {
    }
    public static BluetoothLeAdvertiseServiceData read(ByteBuffer b)
    {
        String uuid = GMExtWire.readString(b);

        java.util.List<Byte> data = GMExtWire.readList(b, bb -> GMExtWire.readI8(bb));

        return new BluetoothLeAdvertiseServiceData(uuid, data);
    }

    public static void write(GMExtWire.IByteWriter b, BluetoothLeAdvertiseServiceData obj)
    {
        GMExtWire.writeString(b, obj.uuid());

        GMExtWire.writeList(b, obj.data(), (bb, x) -> GMExtWire.writeI8(bb, x));

    }
}