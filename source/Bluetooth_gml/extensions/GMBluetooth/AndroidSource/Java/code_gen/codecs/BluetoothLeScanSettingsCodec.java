// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.codecs;

import java.nio.ByteBuffer;

import ${YYAndroidPackageName}.GMExtWire;
import ${YYAndroidPackageName}.GMExtWire.GMValue;
import java.util.Optional;
import ${YYAndroidPackageName}.enums.*;
import ${YYAndroidPackageName}.records.*;

public final class BluetoothLeScanSettingsCodec {
    private BluetoothLeScanSettingsCodec()
    {
    }
    public static BluetoothLeScanSettings read(ByteBuffer b)
    {
        java.util.Optional<Boolean> active = java.util.Optional.empty();
        if (GMExtWire.readBool(b))
        {
            boolean __opt_active = GMExtWire.readBool(b);
            active = java.util.Optional.of(__opt_active);
        }

        java.util.Optional<BluetoothLeScanMode> mode = java.util.Optional.empty();
        if (GMExtWire.readBool(b))
        {
            BluetoothLeScanMode __opt_mode = BluetoothLeScanMode.from(GMExtWire.readI32(b));
            mode = java.util.Optional.ofNullable(__opt_mode);
        }

        java.util.Optional<Integer> update_interval_ms = java.util.Optional.empty();
        if (GMExtWire.readBool(b))
        {
            int __opt_update_interval_ms = GMExtWire.readI32(b);
            update_interval_ms = java.util.Optional.of(__opt_update_interval_ms);
        }

        return new BluetoothLeScanSettings(active, mode, update_interval_ms);
    }

    public static void write(GMExtWire.IByteWriter b, BluetoothLeScanSettings obj)
    {
        GMExtWire.writeBool(b, obj.active() != null && obj.active().isPresent());
        if (obj.active() != null && obj.active().isPresent())
        {
            GMExtWire.writeBool(b, obj.active().get());
        }

        GMExtWire.writeBool(b, obj.mode() != null && obj.mode().isPresent());
        if (obj.mode() != null && obj.mode().isPresent())
        {
            GMExtWire.writeI32(b, obj.mode().get().value());
        }

        GMExtWire.writeBool(b, obj.update_interval_ms() != null && obj.update_interval_ms().isPresent());
        if (obj.update_interval_ms() != null && obj.update_interval_ms().isPresent())
        {
            GMExtWire.writeI32(b, obj.update_interval_ms().get());
        }

    }
}