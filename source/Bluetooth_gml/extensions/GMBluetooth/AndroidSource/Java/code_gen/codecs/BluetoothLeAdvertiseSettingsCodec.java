// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.codecs;

import java.nio.ByteBuffer;

import ${YYAndroidPackageName}.GMExtWire;
import ${YYAndroidPackageName}.GMExtWire.GMValue;
import java.util.Optional;
import ${YYAndroidPackageName}.enums.*;
import ${YYAndroidPackageName}.records.*;

public final class BluetoothLeAdvertiseSettingsCodec {
    private BluetoothLeAdvertiseSettingsCodec()
    {
    }
    public static BluetoothLeAdvertiseSettings read(ByteBuffer b)
    {
        boolean connectable = GMExtWire.readBool(b);

        java.util.Optional<BluetoothLeAdvertiseTxPower> tx_power = java.util.Optional.empty();
        if (GMExtWire.readBool(b))
        {
            BluetoothLeAdvertiseTxPower __opt_tx_power = BluetoothLeAdvertiseTxPower.from(GMExtWire.readI32(b));
            tx_power = java.util.Optional.of(__opt_tx_power);
        }

        return new BluetoothLeAdvertiseSettings(connectable, tx_power);
    }

    public static void write(GMExtWire.IByteWriter b, BluetoothLeAdvertiseSettings obj)
    {
        GMExtWire.writeBool(b, obj.connectable());

        GMExtWire.writeBool(b, obj.tx_power() != null && obj.tx_power().isPresent());
        if (obj.tx_power() != null && obj.tx_power().isPresent())
        {
            GMExtWire.writeI32(b, obj.tx_power().get().value());
        }

    }
}