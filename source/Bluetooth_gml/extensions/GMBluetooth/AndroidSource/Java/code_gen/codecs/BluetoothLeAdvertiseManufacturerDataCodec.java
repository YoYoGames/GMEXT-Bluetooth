// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.codecs;

import java.nio.ByteBuffer;

import ${YYAndroidPackageName}.GMExtWire;
import ${YYAndroidPackageName}.GMExtWire.GMValue;
import java.util.List;
import ${YYAndroidPackageName}.records.*;

public final class BluetoothLeAdvertiseManufacturerDataCodec {
    private BluetoothLeAdvertiseManufacturerDataCodec()
    {
    }
    public static BluetoothLeAdvertiseManufacturerData read(ByteBuffer b)
    {
        int company_id = GMExtWire.readI32(b);

        java.util.List<Byte> data = GMExtWire.readList(b, bb -> GMExtWire.readI8(bb));

        return new BluetoothLeAdvertiseManufacturerData(company_id, data);
    }

    public static void write(GMExtWire.IByteWriter b, BluetoothLeAdvertiseManufacturerData obj)
    {
        GMExtWire.writeI32(b, obj.company_id());

        GMExtWire.writeList(b, obj.data(), (bb, x) -> GMExtWire.writeI8(bb, x));

    }
}