// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.codecs;

import java.nio.ByteBuffer;

import ${YYAndroidPackageName}.GMExtWire;
import ${YYAndroidPackageName}.GMExtWire.GMValue;
import java.util.Optional;
import ${YYAndroidPackageName}.records.*;

public final class BluetoothLeScanFilterCodec {
    private BluetoothLeScanFilterCodec()
    {
    }
    public static BluetoothLeScanFilter read(ByteBuffer b)
    {
        java.util.Optional<String> service_uuid = java.util.Optional.empty();
        if (GMExtWire.readBool(b))
        {
            String __opt_service_uuid = GMExtWire.readString(b);
            service_uuid = java.util.Optional.of(__opt_service_uuid);
        }

        java.util.Optional<String> name = java.util.Optional.empty();
        if (GMExtWire.readBool(b))
        {
            String __opt_name = GMExtWire.readString(b);
            name = java.util.Optional.of(__opt_name);
        }

        java.util.Optional<Integer> company_id = java.util.Optional.empty();
        if (GMExtWire.readBool(b))
        {
            int __opt_company_id = GMExtWire.readI32(b);
            company_id = java.util.Optional.of(__opt_company_id);
        }

        return new BluetoothLeScanFilter(service_uuid, name, company_id);
    }

    public static void write(GMExtWire.IByteWriter b, BluetoothLeScanFilter obj)
    {
        GMExtWire.writeBool(b, obj.service_uuid() != null && obj.service_uuid().isPresent());
        if (obj.service_uuid() != null && obj.service_uuid().isPresent())
        {
            GMExtWire.writeString(b, obj.service_uuid().get());
        }

        GMExtWire.writeBool(b, obj.name() != null && obj.name().isPresent());
        if (obj.name() != null && obj.name().isPresent())
        {
            GMExtWire.writeString(b, obj.name().get());
        }

        GMExtWire.writeBool(b, obj.company_id() != null && obj.company_id().isPresent());
        if (obj.company_id() != null && obj.company_id().isPresent())
        {
            GMExtWire.writeI32(b, obj.company_id().get());
        }

    }
}