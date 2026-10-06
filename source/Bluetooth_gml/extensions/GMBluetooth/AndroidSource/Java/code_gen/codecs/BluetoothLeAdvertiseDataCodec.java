// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.codecs;

import java.nio.ByteBuffer;

import ${YYAndroidPackageName}.GMExtWire;
import ${YYAndroidPackageName}.GMExtWire.GMValue;
import java.util.List;
import ${YYAndroidPackageName}.records.*;

public final class BluetoothLeAdvertiseDataCodec {
    private BluetoothLeAdvertiseDataCodec()
    {
    }
    public static BluetoothLeAdvertiseData read(ByteBuffer b)
    {
        boolean include_name = GMExtWire.readBool(b);

        boolean include_tx_power = GMExtWire.readBool(b);

        java.util.List<String> service_uuids = GMExtWire.readList(b, bb -> GMExtWire.readString(bb));

        java.util.List<BluetoothLeAdvertiseServiceData> service_data = GMExtWire.readList(b, bb -> BluetoothLeAdvertiseServiceDataCodec.read(bb));

        java.util.List<BluetoothLeAdvertiseManufacturerData> manufacturer_data = GMExtWire.readList(b, bb -> BluetoothLeAdvertiseManufacturerDataCodec.read(bb));

        return new BluetoothLeAdvertiseData(include_name, include_tx_power, service_uuids, service_data, manufacturer_data);
    }

    public static void write(GMExtWire.IByteWriter b, BluetoothLeAdvertiseData obj)
    {
        GMExtWire.writeBool(b, obj.include_name());

        GMExtWire.writeBool(b, obj.include_tx_power());

        GMExtWire.writeList(b, obj.service_uuids(), (bb, x) -> GMExtWire.writeString(bb, x));

        GMExtWire.writeList(b, obj.service_data(), (bb, x) -> BluetoothLeAdvertiseServiceDataCodec.write(bb, x));

        GMExtWire.writeList(b, obj.manufacturer_data(), (bb, x) -> BluetoothLeAdvertiseManufacturerDataCodec.write(bb, x));

    }
}