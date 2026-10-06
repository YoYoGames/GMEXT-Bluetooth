// ##### extgen :: Auto-generated file do not edit!! #####

#pragma once
#import <Foundation/Foundation.h>

#include <cstdint>
#include <string_view>
#include <vector>
#include <array>
#include <optional>
#include "core/GMExtWire.h"

namespace gm_consts
{
}


namespace gm_enums
{
    enum class BluetoothError : std::int32_t
    {
        Ok = 0,
        Unknown = 1,
        NotSupported = 2,
        NotInitialized = 3,
        BluetoothDisabled = 4,
        PermissionDenied = 5,
        InvalidArgument = 6,
        InvalidHandle = 7,
        Busy = 8,
        Timeout = 9,
        NotFound = 10,
        ConnectionFailed = 11,
        Disconnected = 12,
        OperationFailed = 13
    };

    enum class BluetoothTransport : std::int32_t
    {
        Unknown = 0,
        Classic = 1,
        LowEnergy = 2
    };

    enum class BluetoothPermissionStatus : std::int32_t
    {
        Unknown = 0,
        Granted = 1,
        Denied = 2
    };

    enum class BluetoothLeSubscribeMode : std::int32_t
    {
        Unsubscribe = 0,
        Notify = 1,
        Indicate = 2
    };

    enum class BluetoothLeCharacteristicProperty : std::int32_t
    {
        None = 0,
        Broadcast = 1,
        Read = 2,
        WriteWithoutResponse = 4,
        Write = 8,
        Notify = 16,
        Indicate = 32,
        AuthenticatedSignedWrites = 64,
        ExtendedProperties = 128
    };

    enum class BluetoothLeWriteType : std::int32_t
    {
        WithResponse = 0,
        WithoutResponse = 1
    };

    enum class BluetoothLeAdvertiseTxPower : std::int32_t
    {
        UltraLow = 0,
        Low = 1,
        Medium = 2,
        High = 3
    };

    enum class BluetoothState : std::int32_t
    {
        Unknown = 0,
        Resetting = 1,
        Unsupported = 2,
        Unauthorized = 3,
        PoweredOff = 4,
        PoweredOn = 5
    };

}


namespace gm_structs
{
    struct BluetoothLeDescriptorDefinition;
    struct BluetoothLeAdvertiseSettings;
    struct BluetoothLeAdvertiseServiceData;
    struct BluetoothLeAdvertiseManufacturerData;
    struct BluetoothLeCharacteristicDefinition;
    struct BluetoothLeAdvertiseData;
    struct BluetoothLeServiceDefinition;

    struct BluetoothLeDescriptorDefinition
    {
        std::string uuid;
    };

    struct BluetoothLeAdvertiseSettings
    {
        bool connectable;
        std::optional<gm_enums::BluetoothLeAdvertiseTxPower> tx_power;
    };

    struct BluetoothLeAdvertiseServiceData
    {
        std::string uuid;
        std::vector<std::uint8_t> data;
    };

    struct BluetoothLeAdvertiseManufacturerData
    {
        std::int32_t company_id;
        std::vector<std::uint8_t> data;
    };

    struct BluetoothLeCharacteristicDefinition
    {
        std::string uuid;
        std::int32_t properties;
        std::int32_t permissions;
        std::optional<std::string> value;
        std::vector<gm_structs::BluetoothLeDescriptorDefinition> descriptors;
    };

    struct BluetoothLeAdvertiseData
    {
        bool include_name;
        bool include_tx_power;
        std::vector<std::string> service_uuids;
        std::vector<gm_structs::BluetoothLeAdvertiseServiceData> service_data;
        std::vector<gm_structs::BluetoothLeAdvertiseManufacturerData> manufacturer_data;
    };

    struct BluetoothLeServiceDefinition
    {
        std::string uuid;
        std::vector<gm_structs::BluetoothLeCharacteristicDefinition> characteristics;
    };

}

namespace gm::wire::codec
{
    template<>
    inline void writeValue<gm_structs::BluetoothLeDescriptorDefinition>(gm::byteio::IByteWriter& _buf, const gm_structs::BluetoothLeDescriptorDefinition& obj)
    {
        gm::wire::codec::writeValue(_buf, obj.uuid);
    }

    template<>
    inline gm_structs::BluetoothLeDescriptorDefinition readValue<gm_structs::BluetoothLeDescriptorDefinition>(gm::byteio::BufferReader& _buf)
    {
        gm_structs::BluetoothLeDescriptorDefinition obj;
        obj.uuid = gm::wire::codec::readValue<std::string>(_buf);
        return obj;
    }

    template<>
    inline void writeValue<gm_structs::BluetoothLeAdvertiseSettings>(gm::byteio::IByteWriter& _buf, const gm_structs::BluetoothLeAdvertiseSettings& obj)
    {
        gm::wire::codec::writeValue(_buf, obj.connectable);
        gm::wire::codec::writeValue(_buf, obj.tx_power);
    }

    template<>
    inline gm_structs::BluetoothLeAdvertiseSettings readValue<gm_structs::BluetoothLeAdvertiseSettings>(gm::byteio::BufferReader& _buf)
    {
        gm_structs::BluetoothLeAdvertiseSettings obj;
        obj.connectable = gm::wire::codec::readValue<bool>(_buf);
        obj.tx_power = gm::wire::codec::readOptional<gm_enums::BluetoothLeAdvertiseTxPower>(_buf);
        return obj;
    }

    template<>
    inline void writeValue<gm_structs::BluetoothLeAdvertiseServiceData>(gm::byteio::IByteWriter& _buf, const gm_structs::BluetoothLeAdvertiseServiceData& obj)
    {
        gm::wire::codec::writeValue(_buf, obj.uuid);
        gm::wire::codec::writeValue(_buf, obj.data);
    }

    template<>
    inline gm_structs::BluetoothLeAdvertiseServiceData readValue<gm_structs::BluetoothLeAdvertiseServiceData>(gm::byteio::BufferReader& _buf)
    {
        gm_structs::BluetoothLeAdvertiseServiceData obj;
        obj.uuid = gm::wire::codec::readValue<std::string>(_buf);
        obj.data = gm::wire::codec::readVector<std::uint8_t>(_buf);
        return obj;
    }

    template<>
    inline void writeValue<gm_structs::BluetoothLeAdvertiseManufacturerData>(gm::byteio::IByteWriter& _buf, const gm_structs::BluetoothLeAdvertiseManufacturerData& obj)
    {
        gm::wire::codec::writeValue(_buf, obj.company_id);
        gm::wire::codec::writeValue(_buf, obj.data);
    }

    template<>
    inline gm_structs::BluetoothLeAdvertiseManufacturerData readValue<gm_structs::BluetoothLeAdvertiseManufacturerData>(gm::byteio::BufferReader& _buf)
    {
        gm_structs::BluetoothLeAdvertiseManufacturerData obj;
        obj.company_id = gm::wire::codec::readValue<std::int32_t>(_buf);
        obj.data = gm::wire::codec::readVector<std::uint8_t>(_buf);
        return obj;
    }

    template<>
    inline void writeValue<gm_structs::BluetoothLeCharacteristicDefinition>(gm::byteio::IByteWriter& _buf, const gm_structs::BluetoothLeCharacteristicDefinition& obj)
    {
        gm::wire::codec::writeValue(_buf, obj.uuid);
        gm::wire::codec::writeValue(_buf, obj.properties);
        gm::wire::codec::writeValue(_buf, obj.permissions);
        gm::wire::codec::writeValue(_buf, obj.value);
        gm::wire::codec::writeValue(_buf, obj.descriptors);
    }

    template<>
    inline gm_structs::BluetoothLeCharacteristicDefinition readValue<gm_structs::BluetoothLeCharacteristicDefinition>(gm::byteio::BufferReader& _buf)
    {
        gm_structs::BluetoothLeCharacteristicDefinition obj;
        obj.uuid = gm::wire::codec::readValue<std::string>(_buf);
        obj.properties = gm::wire::codec::readValue<std::int32_t>(_buf);
        obj.permissions = gm::wire::codec::readValue<std::int32_t>(_buf);
        obj.value = gm::wire::codec::readOptional<std::string>(_buf);
        obj.descriptors = gm::wire::codec::readVector<gm_structs::BluetoothLeDescriptorDefinition>(_buf);
        return obj;
    }

    template<>
    inline void writeValue<gm_structs::BluetoothLeAdvertiseData>(gm::byteio::IByteWriter& _buf, const gm_structs::BluetoothLeAdvertiseData& obj)
    {
        gm::wire::codec::writeValue(_buf, obj.include_name);
        gm::wire::codec::writeValue(_buf, obj.include_tx_power);
        gm::wire::codec::writeValue(_buf, obj.service_uuids);
        gm::wire::codec::writeValue(_buf, obj.service_data);
        gm::wire::codec::writeValue(_buf, obj.manufacturer_data);
    }

    template<>
    inline gm_structs::BluetoothLeAdvertiseData readValue<gm_structs::BluetoothLeAdvertiseData>(gm::byteio::BufferReader& _buf)
    {
        gm_structs::BluetoothLeAdvertiseData obj;
        obj.include_name = gm::wire::codec::readValue<bool>(_buf);
        obj.include_tx_power = gm::wire::codec::readValue<bool>(_buf);
        obj.service_uuids = gm::wire::codec::readVector<std::string>(_buf);
        obj.service_data = gm::wire::codec::readVector<gm_structs::BluetoothLeAdvertiseServiceData>(_buf);
        obj.manufacturer_data = gm::wire::codec::readVector<gm_structs::BluetoothLeAdvertiseManufacturerData>(_buf);
        return obj;
    }

    template<>
    inline void writeValue<gm_structs::BluetoothLeServiceDefinition>(gm::byteio::IByteWriter& _buf, const gm_structs::BluetoothLeServiceDefinition& obj)
    {
        gm::wire::codec::writeValue(_buf, obj.uuid);
        gm::wire::codec::writeValue(_buf, obj.characteristics);
    }

    template<>
    inline gm_structs::BluetoothLeServiceDefinition readValue<gm_structs::BluetoothLeServiceDefinition>(gm::byteio::BufferReader& _buf)
    {
        gm_structs::BluetoothLeServiceDefinition obj;
        obj.uuid = gm::wire::codec::readValue<std::string>(_buf);
        obj.characteristics = gm::wire::codec::readVector<gm_structs::BluetoothLeCharacteristicDefinition>(_buf);
        return obj;
    }

}

namespace gm::wire::details
{
    template<>
    struct gm_struct_traits<gm_structs::BluetoothLeDescriptorDefinition>
    {
        static constexpr bool is_gm_struct = true;
        static constexpr std::uint32_t codec_id = 0;
    };

    template<>
    struct gm_struct_traits<gm_structs::BluetoothLeAdvertiseSettings>
    {
        static constexpr bool is_gm_struct = true;
        static constexpr std::uint32_t codec_id = 1;
    };

    template<>
    struct gm_struct_traits<gm_structs::BluetoothLeAdvertiseServiceData>
    {
        static constexpr bool is_gm_struct = true;
        static constexpr std::uint32_t codec_id = 2;
    };

    template<>
    struct gm_struct_traits<gm_structs::BluetoothLeAdvertiseManufacturerData>
    {
        static constexpr bool is_gm_struct = true;
        static constexpr std::uint32_t codec_id = 3;
    };

    template<>
    struct gm_struct_traits<gm_structs::BluetoothLeCharacteristicDefinition>
    {
        static constexpr bool is_gm_struct = true;
        static constexpr std::uint32_t codec_id = 4;
    };

    template<>
    struct gm_struct_traits<gm_structs::BluetoothLeAdvertiseData>
    {
        static constexpr bool is_gm_struct = true;
        static constexpr std::uint32_t codec_id = 5;
    };

    template<>
    struct gm_struct_traits<gm_structs::BluetoothLeServiceDefinition>
    {
        static constexpr bool is_gm_struct = true;
        static constexpr std::uint32_t codec_id = 6;
    };

}

@protocol GMBluetoothInterface <NSObject>
- (bool)bluetooth_initialize;
- (void)bluetooth_shutdown;
- (bool)bluetooth_is_initialized;
- (gm_enums::BluetoothError)bluetooth_last_error_code;
- (std::string)bluetooth_last_error_message;
- (bool)bluetooth_le_is_supported;
- (bool)bluetooth_le_advertise_is_supported;
- (bool)bluetooth_le_server_is_supported;
- (bool)bluetooth_classic_is_supported;
- (bool)bluetooth_classic_server_is_supported;
- (gm_enums::BluetoothPermissionStatus)bluetooth_permission_get_status;
- (gm_enums::BluetoothError)bluetooth_permission_request;
- (gm_enums::BluetoothError)bluetooth_le_scan_start:(bool)active;
- (gm_enums::BluetoothError)bluetooth_le_scan_stop;
- (bool)bluetooth_le_scan_is_running;
- (gm_enums::BluetoothError)bluetooth_classic_scan_start;
- (gm_enums::BluetoothError)bluetooth_classic_scan_stop;
- (bool)bluetooth_classic_scan_is_running;
- (void)bluetooth_device_clear;
- (std::int32_t)bluetooth_device_get_count;
- (std::uint64_t)bluetooth_device_get_at:(std::int32_t)index;
- (bool)bluetooth_device_is_valid:(std::uint64_t)device;
- (gm_enums::BluetoothTransport)bluetooth_device_get_transport:(std::uint64_t)device;
- (std::string)bluetooth_device_get_id:(std::uint64_t)device;
- (std::string)bluetooth_device_get_name:(std::uint64_t)device;
- (bool)bluetooth_device_has_address:(std::uint64_t)device;
- (std::string)bluetooth_device_get_address:(std::uint64_t)device;
- (bool)bluetooth_device_has_rssi:(std::uint64_t)device;
- (std::int32_t)bluetooth_device_get_rssi:(std::uint64_t)device;
- (bool)bluetooth_device_is_connectable:(std::uint64_t)device;
- (std::uint64_t)bluetooth_classic_connect:(std::uint64_t)device service_uuid:(std::string_view)service_uuid callback:(gm::wire::GMFunction)callback;
- (gm_enums::BluetoothError)bluetooth_classic_disconnect:(std::uint64_t)connection;
- (bool)bluetooth_classic_connection_is_valid:(std::uint64_t)connection;
- (bool)bluetooth_classic_connection_is_connected:(std::uint64_t)connection;
- (std::uint64_t)bluetooth_classic_connection_get_device:(std::uint64_t)connection;
- (std::int32_t)bluetooth_classic_receive_available:(std::uint64_t)connection;
- (gm_enums::BluetoothError)bluetooth_classic_send:(std::uint64_t)connection data:(gm::wire::GMBuffer)data offset:(std::uint32_t)offset size:(std::uint32_t)size;
- (std::int32_t)bluetooth_classic_receive:(std::uint64_t)connection out_data:(gm::wire::GMBuffer)out_data offset:(std::uint32_t)offset max_size:(std::uint32_t)max_size;
- (gm_enums::BluetoothError)bluetooth_classic_server_start:(std::string_view)name service_uuid:(std::string_view)service_uuid;
- (gm_enums::BluetoothError)bluetooth_classic_server_stop;
- (bool)bluetooth_classic_server_is_running;
- (gm_enums::BluetoothError)bluetooth_classic_discoverable_start:(std::int32_t)duration_seconds;
- (gm_enums::BluetoothError)bluetooth_classic_discoverable_stop;
- (bool)bluetooth_classic_discoverable_is_running;
- (bool)bluetooth_pairing_is_supported:(std::uint64_t)device;
- (gm_enums::BluetoothError)bluetooth_pair:(std::uint64_t)device callback:(gm::wire::GMFunction)callback;
- (bool)bluetooth_device_is_paired:(std::uint64_t)device;
- (std::uint64_t)bluetooth_le_connect:(std::uint64_t)device callback:(gm::wire::GMFunction)callback;
- (gm_enums::BluetoothError)bluetooth_le_disconnect:(std::uint64_t)connection;
- (bool)bluetooth_le_connection_is_valid:(std::uint64_t)connection;
- (bool)bluetooth_le_connection_is_connected:(std::uint64_t)connection;
- (std::uint64_t)bluetooth_le_connection_get_device:(std::uint64_t)connection;
- (gm_enums::BluetoothError)bluetooth_le_services_discover:(std::uint64_t)connection callback:(gm::wire::GMFunction)callback;
- (std::int32_t)bluetooth_le_service_get_count:(std::uint64_t)connection;
- (std::uint64_t)bluetooth_le_service_get_at:(std::uint64_t)connection index:(std::int32_t)index;
- (std::string)bluetooth_le_service_get_uuid:(std::uint64_t)service;
- (gm_enums::BluetoothError)bluetooth_le_characteristics_discover:(std::uint64_t)service callback:(gm::wire::GMFunction)callback;
- (std::int32_t)bluetooth_le_characteristic_get_count:(std::uint64_t)service;
- (std::uint64_t)bluetooth_le_characteristic_get_at:(std::uint64_t)service index:(std::int32_t)index;
- (std::string)bluetooth_le_characteristic_get_uuid:(std::uint64_t)characteristic;
- (std::int32_t)bluetooth_le_characteristic_get_properties:(std::uint64_t)characteristic;
- (gm_enums::BluetoothError)bluetooth_le_descriptors_discover:(std::uint64_t)characteristic callback:(gm::wire::GMFunction)callback;
- (std::int32_t)bluetooth_le_descriptor_get_count:(std::uint64_t)characteristic;
- (std::uint64_t)bluetooth_le_descriptor_get_at:(std::uint64_t)characteristic index:(std::int32_t)index;
- (std::string)bluetooth_le_descriptor_get_uuid:(std::uint64_t)descriptor;
- (gm_enums::BluetoothError)bluetooth_le_characteristic_read:(std::uint64_t)characteristic callback:(gm::wire::GMFunction)callback;
- (gm_enums::BluetoothError)bluetooth_le_characteristic_write:(std::uint64_t)characteristic data:(gm::wire::GMBuffer)data offset:(std::uint32_t)offset size:(std::uint32_t)size write_type:(gm_enums::BluetoothLeWriteType)write_type callback:(gm::wire::GMFunction)callback;
- (gm_enums::BluetoothError)bluetooth_le_characteristic_subscribe:(std::uint64_t)characteristic mode:(gm_enums::BluetoothLeSubscribeMode)mode callback:(gm::wire::GMFunction)callback;
- (gm_enums::BluetoothError)bluetooth_le_descriptor_read:(std::uint64_t)descriptor callback:(gm::wire::GMFunction)callback;
- (gm_enums::BluetoothError)bluetooth_le_descriptor_write:(std::uint64_t)descriptor data:(gm::wire::GMBuffer)data offset:(std::uint32_t)offset size:(std::uint32_t)size callback:(gm::wire::GMFunction)callback;
- (gm_enums::BluetoothError)bluetooth_le_value_copy:(std::uint64_t)value out_data:(gm::wire::GMBuffer)out_data offset:(std::uint32_t)offset;
- (gm_enums::BluetoothError)bluetooth_le_value_release:(std::uint64_t)value;
- (gm_enums::BluetoothError)bluetooth_le_advertise_start:(const gm_structs::BluetoothLeAdvertiseSettings&)settings data:(const gm_structs::BluetoothLeAdvertiseData&)data callback:(gm::wire::GMFunction)callback;
- (gm_enums::BluetoothError)bluetooth_le_advertise_stop;
- (bool)bluetooth_le_advertise_is_running;
- (gm_enums::BluetoothError)bluetooth_le_server_start;
- (gm_enums::BluetoothError)bluetooth_le_server_stop;
- (bool)bluetooth_le_server_is_running;
- (gm_enums::BluetoothError)bluetooth_le_server_add_service:(const gm_structs::BluetoothLeServiceDefinition&)service callback:(gm::wire::GMFunction)callback;
- (gm_enums::BluetoothError)bluetooth_le_server_clear_services;
- (gm_enums::BluetoothError)bluetooth_le_server_respond_read:(std::int32_t)request_id error_code:(std::int32_t)error_code data:(gm::wire::GMBuffer)data offset:(std::uint32_t)offset size:(std::uint32_t)size;
- (gm_enums::BluetoothError)bluetooth_le_server_respond_write:(std::int32_t)request_id error_code:(std::int32_t)error_code;
- (std::int32_t)bluetooth_le_server_write_request_get_value:(std::int32_t)request_id out_data:(gm::wire::GMBuffer)out_data offset:(std::uint32_t)offset max_size:(std::uint32_t)max_size;
- (gm_enums::BluetoothError)bluetooth_le_server_notify_value:(std::string_view)service_uuid characteristic_uuid:(std::string_view)characteristic_uuid connection:(std::uint64_t)connection data:(gm::wire::GMBuffer)data offset:(std::uint32_t)offset size:(std::uint32_t)size;
- (bool)bluetooth_set_callback_state_changed:(gm::wire::GMFunction)callback;
- (bool)bluetooth_remove_callback_state_changed;
- (bool)bluetooth_set_callback_device_found:(gm::wire::GMFunction)callback;
- (bool)bluetooth_remove_callback_device_found;
- (bool)bluetooth_set_callback_scan_stopped:(gm::wire::GMFunction)callback;
- (bool)bluetooth_remove_callback_scan_stopped;
- (bool)bluetooth_set_callback_classic_client_connected:(gm::wire::GMFunction)callback;
- (bool)bluetooth_remove_callback_classic_client_connected;
- (bool)bluetooth_set_callback_classic_data:(gm::wire::GMFunction)callback;
- (bool)bluetooth_remove_callback_classic_data;
- (bool)bluetooth_set_callback_classic_disconnected:(gm::wire::GMFunction)callback;
- (bool)bluetooth_remove_callback_classic_disconnected;
- (bool)bluetooth_set_callback_le_disconnected:(gm::wire::GMFunction)callback;
- (bool)bluetooth_remove_callback_le_disconnected;
- (bool)bluetooth_set_callback_le_characteristic_value_changed:(gm::wire::GMFunction)callback;
- (bool)bluetooth_remove_callback_le_characteristic_value_changed;
- (bool)bluetooth_set_callback_le_server_connection_state_changed:(gm::wire::GMFunction)callback;
- (bool)bluetooth_remove_callback_le_server_connection_state_changed;
- (bool)bluetooth_set_callback_le_server_read_request:(gm::wire::GMFunction)callback;
- (bool)bluetooth_remove_callback_le_server_read_request;
- (bool)bluetooth_set_callback_le_server_write_request:(gm::wire::GMFunction)callback;
- (bool)bluetooth_remove_callback_le_server_write_request;
@end


@interface GMBluetoothInternal : NSObject
- (double)__EXT_NATIVE__bluetooth_initialize;
- (double)__EXT_NATIVE__bluetooth_shutdown;
- (double)__EXT_NATIVE__bluetooth_is_initialized;
- (double)__EXT_NATIVE__bluetooth_last_error_code:(char*)__ret_buffer arg1:(double)__ret_buffer_length;
- (char*)__EXT_NATIVE__bluetooth_last_error_message;
- (double)__EXT_NATIVE__bluetooth_le_is_supported;
- (double)__EXT_NATIVE__bluetooth_le_advertise_is_supported;
- (double)__EXT_NATIVE__bluetooth_le_server_is_supported;
- (double)__EXT_NATIVE__bluetooth_classic_is_supported;
- (double)__EXT_NATIVE__bluetooth_classic_server_is_supported;
- (double)__EXT_NATIVE__bluetooth_permission_get_status:(char*)__ret_buffer arg1:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__bluetooth_permission_request:(char*)__ret_buffer arg1:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_scan_start:(double)active arg1:(char*)__ret_buffer arg2:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_scan_stop:(char*)__ret_buffer arg1:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_scan_is_running;
- (double)__EXT_NATIVE__bluetooth_classic_scan_start:(char*)__ret_buffer arg1:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__bluetooth_classic_scan_stop:(char*)__ret_buffer arg1:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__bluetooth_classic_scan_is_running;
- (double)__EXT_NATIVE__bluetooth_device_clear;
- (double)__EXT_NATIVE__bluetooth_device_get_count;
- (double)__EXT_NATIVE__bluetooth_device_get_at:(double)index arg1:(char*)__ret_buffer arg2:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__bluetooth_device_is_valid:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_device_get_transport:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length;
- (char*)__EXT_NATIVE__bluetooth_device_get_id:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (char*)__EXT_NATIVE__bluetooth_device_get_name:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_device_has_address:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (char*)__EXT_NATIVE__bluetooth_device_get_address:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_device_has_rssi:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_device_get_rssi:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_device_is_connectable:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_classic_connect:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__bluetooth_classic_disconnect:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__bluetooth_classic_connection_is_valid:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_classic_connection_is_connected:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_classic_connection_get_device:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__bluetooth_classic_receive_available:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_classic_send:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__bluetooth_classic_receive:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_classic_server_start:(char*)name arg1:(char*)service_uuid arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__bluetooth_classic_server_stop:(char*)__ret_buffer arg1:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__bluetooth_classic_server_is_running;
- (double)__EXT_NATIVE__bluetooth_classic_discoverable_start:(double)duration_seconds arg1:(char*)__ret_buffer arg2:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__bluetooth_classic_discoverable_stop:(char*)__ret_buffer arg1:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__bluetooth_classic_discoverable_is_running;
- (double)__EXT_NATIVE__bluetooth_pairing_is_supported:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_pair:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__bluetooth_device_is_paired:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_connect:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_disconnect:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_connection_is_valid:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_connection_is_connected:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_connection_get_device:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_services_discover:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_service_get_count:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_service_get_at:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length;
- (char*)__EXT_NATIVE__bluetooth_le_service_get_uuid:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_characteristics_discover:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_characteristic_get_count:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_characteristic_get_at:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length;
- (char*)__EXT_NATIVE__bluetooth_le_characteristic_get_uuid:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_characteristic_get_properties:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_descriptors_discover:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_descriptor_get_count:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_descriptor_get_at:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length;
- (char*)__EXT_NATIVE__bluetooth_le_descriptor_get_uuid:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_characteristic_read:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_characteristic_write:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_characteristic_subscribe:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_descriptor_read:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_descriptor_write:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_value_copy:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_value_release:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_advertise_start:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_advertise_stop:(char*)__ret_buffer arg1:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_advertise_is_running;
- (double)__EXT_NATIVE__bluetooth_le_server_start:(char*)__ret_buffer arg1:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_server_stop:(char*)__ret_buffer arg1:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_server_is_running;
- (double)__EXT_NATIVE__bluetooth_le_server_add_service:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_server_clear_services:(char*)__ret_buffer arg1:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_server_respond_read:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_server_respond_write:(double)request_id arg1:(double)error_code arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_server_write_request_get_value:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_le_server_notify_value:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__bluetooth_set_callback_state_changed:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_remove_callback_state_changed;
- (double)__EXT_NATIVE__bluetooth_set_callback_device_found:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_remove_callback_device_found;
- (double)__EXT_NATIVE__bluetooth_set_callback_scan_stopped:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_remove_callback_scan_stopped;
- (double)__EXT_NATIVE__bluetooth_set_callback_classic_client_connected:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_remove_callback_classic_client_connected;
- (double)__EXT_NATIVE__bluetooth_set_callback_classic_data:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_remove_callback_classic_data;
- (double)__EXT_NATIVE__bluetooth_set_callback_classic_disconnected:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_remove_callback_classic_disconnected;
- (double)__EXT_NATIVE__bluetooth_set_callback_le_disconnected:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_remove_callback_le_disconnected;
- (double)__EXT_NATIVE__bluetooth_set_callback_le_characteristic_value_changed:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_remove_callback_le_characteristic_value_changed;
- (double)__EXT_NATIVE__bluetooth_set_callback_le_server_connection_state_changed:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_remove_callback_le_server_connection_state_changed;
- (double)__EXT_NATIVE__bluetooth_set_callback_le_server_read_request:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_remove_callback_le_server_read_request;
- (double)__EXT_NATIVE__bluetooth_set_callback_le_server_write_request:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
- (double)__EXT_NATIVE__bluetooth_remove_callback_le_server_write_request;
- (double)__EXT_NATIVE__GMBluetooth_invocation_handler:(char*)__ret_buffer arg1:(double)__ret_buffer_length;
- (double)__EXT_NATIVE__GMBluetooth_queue_buffer:(char*)__arg_buffer arg1:(double)__arg_buffer_length;
@end


