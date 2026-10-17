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
        java.util.Optional<Boolean> connectable = java.util.Optional.empty();
        if (GMExtWire.readBool(b))
        {
            boolean __opt_connectable = GMExtWire.readBool(b);
            connectable = java.util.Optional.of(__opt_connectable);
        }

        java.util.Optional<BluetoothLeAdvertiseTxPower> tx_power = java.util.Optional.empty();
        if (GMExtWire.readBool(b))
        {
            BluetoothLeAdvertiseTxPower __opt_tx_power = BluetoothLeAdvertiseTxPower.from(GMExtWire.readI32(b));
            tx_power = java.util.Optional.ofNullable(__opt_tx_power);
        }

        return new BluetoothLeAdvertiseSettings(connectable, tx_power);
    }

    public static void write(GMExtWire.IByteWriter b, BluetoothLeAdvertiseSettings obj)
    {
        GMExtWire.writeBool(b, obj.connectable() != null && obj.connectable().isPresent());
        if (obj.connectable() != null && obj.connectable().isPresent())
        {
            GMExtWire.writeBool(b, obj.connectable().get());
        }

        GMExtWire.writeBool(b, obj.tx_power() != null && obj.tx_power().isPresent());
        if (obj.tx_power() != null && obj.tx_power().isPresent())
        {
            GMExtWire.writeI32(b, obj.tx_power().get().value());
        }

    }
}