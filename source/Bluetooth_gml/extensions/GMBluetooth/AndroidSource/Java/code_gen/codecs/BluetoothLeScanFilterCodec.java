// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName}.codecs;

import java.nio.ByteBuffer;

import ${YYAndroidPackageName}.GMExtWire;
import ${YYAndroidPackageName}.GMExtWire.GMValue;
import java.util.Optional;
import java.util.List;
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

        java.util.Optional<String> name_prefix = java.util.Optional.empty();
        if (GMExtWire.readBool(b))
        {
            String __opt_name_prefix = GMExtWire.readString(b);
            name_prefix = java.util.Optional.of(__opt_name_prefix);
        }

        java.util.Optional<Integer> company_id = java.util.Optional.empty();
        if (GMExtWire.readBool(b))
        {
            int __opt_company_id = GMExtWire.readI32(b);
            company_id = java.util.Optional.of(__opt_company_id);
        }

        java.util.Optional<java.util.List<Byte>> manufacturer_data = java.util.Optional.empty();
        if (GMExtWire.readBool(b))
        {
            java.util.List<Byte> __opt_manufacturer_data = GMExtWire.readList(b, bb -> GMExtWire.readI8(bb));
            manufacturer_data = java.util.Optional.of(__opt_manufacturer_data);
        }

        java.util.Optional<java.util.List<Byte>> manufacturer_data_mask = java.util.Optional.empty();
        if (GMExtWire.readBool(b))
        {
            java.util.List<Byte> __opt_manufacturer_data_mask = GMExtWire.readList(b, bb -> GMExtWire.readI8(bb));
            manufacturer_data_mask = java.util.Optional.of(__opt_manufacturer_data_mask);
        }

        java.util.Optional<String> service_data_uuid = java.util.Optional.empty();
        if (GMExtWire.readBool(b))
        {
            String __opt_service_data_uuid = GMExtWire.readString(b);
            service_data_uuid = java.util.Optional.of(__opt_service_data_uuid);
        }

        java.util.Optional<java.util.List<Byte>> service_data = java.util.Optional.empty();
        if (GMExtWire.readBool(b))
        {
            java.util.List<Byte> __opt_service_data = GMExtWire.readList(b, bb -> GMExtWire.readI8(bb));
            service_data = java.util.Optional.of(__opt_service_data);
        }

        java.util.Optional<java.util.List<Byte>> service_data_mask = java.util.Optional.empty();
        if (GMExtWire.readBool(b))
        {
            java.util.List<Byte> __opt_service_data_mask = GMExtWire.readList(b, bb -> GMExtWire.readI8(bb));
            service_data_mask = java.util.Optional.of(__opt_service_data_mask);
        }

        return new BluetoothLeScanFilter(service_uuid, name, name_prefix, company_id, manufacturer_data, manufacturer_data_mask, service_data_uuid, service_data, service_data_mask);
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

        GMExtWire.writeBool(b, obj.name_prefix() != null && obj.name_prefix().isPresent());
        if (obj.name_prefix() != null && obj.name_prefix().isPresent())
        {
            GMExtWire.writeString(b, obj.name_prefix().get());
        }

        GMExtWire.writeBool(b, obj.company_id() != null && obj.company_id().isPresent());
        if (obj.company_id() != null && obj.company_id().isPresent())
        {
            GMExtWire.writeI32(b, obj.company_id().get());
        }

        GMExtWire.writeBool(b, obj.manufacturer_data() != null && obj.manufacturer_data().isPresent());
        if (obj.manufacturer_data() != null && obj.manufacturer_data().isPresent())
        {
            GMExtWire.writeList(b, obj.manufacturer_data().get(), (bb, x) -> GMExtWire.writeI8(bb, x));
        }

        GMExtWire.writeBool(b, obj.manufacturer_data_mask() != null && obj.manufacturer_data_mask().isPresent());
        if (obj.manufacturer_data_mask() != null && obj.manufacturer_data_mask().isPresent())
        {
            GMExtWire.writeList(b, obj.manufacturer_data_mask().get(), (bb, x) -> GMExtWire.writeI8(bb, x));
        }

        GMExtWire.writeBool(b, obj.service_data_uuid() != null && obj.service_data_uuid().isPresent());
        if (obj.service_data_uuid() != null && obj.service_data_uuid().isPresent())
        {
            GMExtWire.writeString(b, obj.service_data_uuid().get());
        }

        GMExtWire.writeBool(b, obj.service_data() != null && obj.service_data().isPresent());
        if (obj.service_data() != null && obj.service_data().isPresent())
        {
            GMExtWire.writeList(b, obj.service_data().get(), (bb, x) -> GMExtWire.writeI8(bb, x));
        }

        GMExtWire.writeBool(b, obj.service_data_mask() != null && obj.service_data_mask().isPresent());
        if (obj.service_data_mask() != null && obj.service_data_mask().isPresent())
        {
            GMExtWire.writeList(b, obj.service_data_mask().get(), (bb, x) -> GMExtWire.writeI8(bb, x));
        }

    }
}