// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.codecs;

import java.nio.ByteBuffer;

import ${YYAndroidPackageName}.GMExtWire;
import ${YYAndroidPackageName}.GMExtWire.GMValue;
import java.util.Optional;
import java.util.List;
import ${YYAndroidPackageName}.records.*;

public final class BluetoothLeAdvertisementCodec {
    private BluetoothLeAdvertisementCodec()
    {
    }
    public static BluetoothLeAdvertisement read(ByteBuffer b)
    {
        java.util.List<String> service_uuids = GMExtWire.readList(b, bb -> GMExtWire.readString(bb));

        java.util.List<BluetoothLeAdvertiseServiceData> service_data = GMExtWire.readList(b, bb -> BluetoothLeAdvertiseServiceDataCodec.read(bb));

        java.util.List<BluetoothLeAdvertiseManufacturerData> manufacturer_data = GMExtWire.readList(b, bb -> BluetoothLeAdvertiseManufacturerDataCodec.read(bb));

        java.util.Optional<Integer> tx_power = java.util.Optional.empty();
        if (GMExtWire.readBool(b))
        {
            int __opt_tx_power = GMExtWire.readI32(b);
            tx_power = java.util.Optional.of(__opt_tx_power);
        }

        return new BluetoothLeAdvertisement(service_uuids, service_data, manufacturer_data, tx_power);
    }

    public static void write(GMExtWire.IByteWriter b, BluetoothLeAdvertisement obj)
    {
        GMExtWire.writeList(b, obj.service_uuids(), (bb, x) -> GMExtWire.writeString(bb, x));

        GMExtWire.writeList(b, obj.service_data(), (bb, x) -> BluetoothLeAdvertiseServiceDataCodec.write(bb, x));

        GMExtWire.writeList(b, obj.manufacturer_data(), (bb, x) -> BluetoothLeAdvertiseManufacturerDataCodec.write(bb, x));

        GMExtWire.writeBool(b, obj.tx_power() != null && obj.tx_power().isPresent());
        if (obj.tx_power() != null && obj.tx_power().isPresent())
        {
            GMExtWire.writeI32(b, obj.tx_power().get());
        }

    }
}