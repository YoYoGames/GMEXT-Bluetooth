#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace gmbluetooth
{
    enum class Error : std::int32_t
    {
        Ok                = 0,
        Unknown           = 1,
        NotSupported      = 2,
        NotInitialized    = 3,
        BluetoothDisabled = 4,
        PermissionDenied  = 5,
        InvalidArgument   = 6,
        InvalidHandle     = 7,
        Busy              = 8,
        Timeout           = 9,
        NotFound          = 10,
        ConnectionFailed  = 11,
        Disconnected      = 12,
        OperationFailed   = 13,
    };

    enum class Transport : std::int32_t
    {
        Unknown   = 0,
        Classic   = 1,
        LowEnergy = 2,
    };

    enum class PermissionStatus : std::int32_t
    {
        Unknown = 0,
        Granted = 1,
        Denied  = 2,
    };

    struct DiscoveredDevice
    {
        Transport transport = Transport::Unknown;

        std::string id;
        std::string name;
        std::string address;

        bool address_available = false;
        bool connectable = false;

        std::int32_t rssi = 0;
        bool rssi_available = false;
    };

    enum class BackendEventType : std::uint8_t
    {
        ScanStopped,
        ClassicConnected,
        ClassicClientConnected,
        ClassicDataAvailable,
        ClassicDisconnected,
        LeEvent,
    };

    struct BackendEvent
    {
        BackendEventType type = BackendEventType::ScanStopped;
        Transport transport = Transport::Unknown;

        std::uint64_t connection = 0;
        std::uint64_t device = 0;

        Error error = Error::Ok;
        std::int32_t value = 0;

        std::string message;

        // BLE event stream. event_type is the normalized public event name and
        // json preserves the old extension's proven payload schema.
        std::string event_type;
        std::string json;
    };

    struct CoreHooks
    {
        std::function<std::uint64_t(const DiscoveredDevice&)> upsert_device;
        std::function<std::uint64_t(std::uint64_t)> create_classic_connection;
        std::function<void(BackendEvent)> push_event;
    };

    class Backend
    {
    public:
        virtual ~Backend() = default;

        virtual Error initialize(std::string& message) = 0;
        virtual void shutdown() = 0;

        virtual bool supports_ble() const = 0;
        virtual bool supports_le_advertise() const { return false; }
        virtual bool supports_le_server() const { return false; }

        virtual bool supports_classic() const = 0;
        virtual bool supports_classic_server() const { return false; }

        virtual PermissionStatus permission_status() const
        {
            return PermissionStatus::Granted;
        }

        virtual Error permission_request(std::string& message)
        {
            message.clear();
            return Error::Ok;
        }

        virtual Error le_scan_start(bool active, std::string& message) = 0;
        virtual Error le_scan_stop(std::string& message) = 0;
        virtual bool le_scan_is_running() const = 0;

        virtual Error classic_scan_start(std::string& message)
        {
            message = "Bluetooth Classic discovery is not supported by this backend";
            return Error::NotSupported;
        }

        virtual Error classic_scan_stop(std::string& message)
        {
            message.clear();
            return Error::Ok;
        }

        virtual bool classic_scan_is_running() const
        {
            return false;
        }

        virtual Error classic_connect(
            std::uint64_t connection,
            const DiscoveredDevice& device,
            const std::string& service_uuid,
            std::string& message)
        {
            (void)connection;
            (void)device;
            (void)service_uuid;

            message = "Bluetooth Classic is not supported by this backend";
            return Error::NotSupported;
        }

        virtual Error classic_disconnect(
            std::uint64_t connection,
            std::string& message)
        {
            (void)connection;

            message = "Bluetooth Classic is not supported by this backend";
            return Error::NotSupported;
        }

        virtual bool classic_connection_is_connected(
            std::uint64_t connection) const
        {
            (void)connection;
            return false;
        }

        virtual std::int32_t classic_receive_available(
            std::uint64_t connection) const
        {
            (void)connection;
            return 0;
        }

        virtual Error classic_send_bytes(
            std::uint64_t connection,
            const std::uint8_t* data,
            std::size_t size,
            std::string& message)
        {
            (void)connection;
            (void)data;
            (void)size;

            message = "Bluetooth Classic is not supported by this backend";
            return Error::NotSupported;
        }

        virtual std::size_t classic_receive_bytes(
            std::uint64_t connection,
            std::uint8_t* out,
            std::size_t max_size)
        {
            (void)connection;
            (void)out;
            (void)max_size;

            return 0;
        }

        virtual Error classic_server_start(
            const std::string& name,
            const std::string& service_uuid,
            std::string& message)
        {
            (void)name;
            (void)service_uuid;

            message = "Bluetooth Classic server is not supported by this backend";
            return Error::NotSupported;
        }

        virtual Error classic_server_stop(std::string& message)
        {
            message.clear();
            return Error::Ok;
        }

        virtual bool classic_server_is_running() const
        {
            return false;
        }

        virtual Error le_connect(
            std::uint64_t connection,
            const DiscoveredDevice& device,
            std::string& message)
        {
            (void)connection;
            (void)device;

            message = "BLE GATT connections are not supported by this backend";
            return Error::NotSupported;
        }

        virtual Error le_disconnect(
            std::uint64_t connection,
            std::string& message)
        {
            (void)connection;

            message = "BLE GATT connections are not supported by this backend";
            return Error::NotSupported;
        }

        virtual bool le_connection_is_connected(
            std::uint64_t connection) const
        {
            (void)connection;
            return false;
        }

        virtual Error le_services_discover(
            std::uint64_t connection,
            std::string& message)
        {
            (void)connection;

            message = "BLE GATT is not supported by this backend";
            return Error::NotSupported;
        }

        virtual Error le_characteristics_discover(
            std::uint64_t connection,
            const std::string& service_uuid,
            std::string& message)
        {
            (void)connection;
            (void)service_uuid;

            message = "BLE GATT is not supported by this backend";
            return Error::NotSupported;
        }

        virtual Error le_descriptors_discover(
            std::uint64_t connection,
            const std::string& service_uuid,
            const std::string& characteristic_uuid,
            std::string& message)
        {
            (void)connection;
            (void)service_uuid;
            (void)characteristic_uuid;

            message = "BLE GATT is not supported by this backend";
            return Error::NotSupported;
        }

        virtual Error le_characteristic_read(
            std::uint64_t connection,
            const std::string& service_uuid,
            const std::string& characteristic_uuid,
            std::string& message)
        {
            (void)connection;
            (void)service_uuid;
            (void)characteristic_uuid;

            message = "BLE GATT is not supported by this backend";
            return Error::NotSupported;
        }

        virtual Error le_characteristic_write(
            std::uint64_t connection,
            const std::string& service_uuid,
            const std::string& characteristic_uuid,
            const std::string& value_base64,
            bool with_response,
            std::string& message)
        {
            (void)connection;
            (void)service_uuid;
            (void)characteristic_uuid;
            (void)value_base64;
            (void)with_response;

            message = "BLE GATT is not supported by this backend";
            return Error::NotSupported;
        }

        // mode:
        // 0 = unsubscribe
        // 1 = notify
        // 2 = indicate
        virtual Error le_characteristic_subscribe(
            std::uint64_t connection,
            const std::string& service_uuid,
            const std::string& characteristic_uuid,
            std::int32_t mode,
            std::string& message)
        {
            (void)connection;
            (void)service_uuid;
            (void)characteristic_uuid;
            (void)mode;

            message = "BLE GATT is not supported by this backend";
            return Error::NotSupported;
        }

        virtual Error le_descriptor_read(
            std::uint64_t connection,
            const std::string& service_uuid,
            const std::string& characteristic_uuid,
            const std::string& descriptor_uuid,
            std::string& message)
        {
            (void)connection;
            (void)service_uuid;
            (void)characteristic_uuid;
            (void)descriptor_uuid;

            message = "BLE GATT is not supported by this backend";
            return Error::NotSupported;
        }

        virtual Error le_descriptor_write(
            std::uint64_t connection,
            const std::string& service_uuid,
            const std::string& characteristic_uuid,
            const std::string& descriptor_uuid,
            const std::string& value_base64,
            std::string& message)
        {
            (void)connection;
            (void)service_uuid;
            (void)characteristic_uuid;
            (void)descriptor_uuid;
            (void)value_base64;

            message = "BLE GATT is not supported by this backend";
            return Error::NotSupported;
        }

        virtual Error le_advertise_start(
            const std::string& settings_json,
            const std::string& data_json,
            std::string& message)
        {
            (void)settings_json;
            (void)data_json;

            message = "BLE advertising is not supported by this backend";
            return Error::NotSupported;
        }

        virtual Error le_advertise_stop(std::string& message)
        {
            message.clear();
            return Error::Ok;
        }

        virtual bool le_advertise_is_running() const
        {
            return false;
        }

        virtual Error le_server_start(std::string& message)
        {
            message = "BLE peripheral/server mode is not supported by this backend";
            return Error::NotSupported;
        }

        virtual Error le_server_stop(std::string& message)
        {
            message.clear();
            return Error::Ok;
        }

        virtual bool le_server_is_running() const
        {
            return false;
        }

        virtual Error le_server_add_service(
            const std::string& service_json,
            std::string& message)
        {
            (void)service_json;

            message = "BLE peripheral/server mode is not supported by this backend";
            return Error::NotSupported;
        }

        virtual Error le_server_clear_services(std::string& message)
        {
            message = "BLE peripheral/server mode is not supported by this backend";
            return Error::NotSupported;
        }

        virtual Error le_server_respond_read(
            std::int32_t request_id,
            std::int32_t status,
            const std::string& value_base64,
            std::string& message)
        {
            (void)request_id;
            (void)status;
            (void)value_base64;

            message = "BLE peripheral/server mode is not supported by this backend";
            return Error::NotSupported;
        }

        virtual Error le_server_respond_write(
            std::int32_t request_id,
            std::int32_t status,
            std::string& message)
        {
            (void)request_id;
            (void)status;

            message = "BLE peripheral/server mode is not supported by this backend";
            return Error::NotSupported;
        }

        virtual Error le_server_notify_value(
            const std::string& service_uuid,
            const std::string& characteristic_uuid,
            const std::string& value_base64,
            std::string& message)
        {
            (void)service_uuid;
            (void)characteristic_uuid;
            (void)value_base64;

            message = "BLE peripheral/server mode is not supported by this backend";
            return Error::NotSupported;
        }
    };

    std::unique_ptr<Backend> create_platform_backend(CoreHooks hooks);
}