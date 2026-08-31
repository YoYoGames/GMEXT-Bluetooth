#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

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

    struct ConnectionInfo
    {
        std::uint64_t handle = 0;
        std::uint64_t device = 0;
        Transport transport = Transport::Unknown;
    };

    enum class BackendEventType : std::uint8_t
    {
        ScanStopped,
        ClassicConnected,
        ClassicClientConnected,
        ClassicDataAvailable,
        ClassicDisconnected,
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
    };

    struct CoreHooks
    {
        // Thread-safe. Inserts/updates a discovered device and returns the common
        // GameMaker-facing device handle.
        std::function<std::uint64_t(const DiscoveredDevice&)> upsert_device;

        // Thread-safe. Allocates a common Classic connection handle associated
        // with a device. Used by server-side accepted connections.
        std::function<std::uint64_t(std::uint64_t device)> create_classic_connection;

        // Thread-safe. Platform backends push normalized events here. GML is
        // never called directly from platform worker/callback threads.
        std::function<void(BackendEvent)> push_event;
    };

    class Backend
    {
    public:
        virtual ~Backend() = default;

        virtual Error initialize(std::string& message) = 0;
        virtual void shutdown() = 0;

        virtual bool supports_ble() const = 0;
        virtual bool supports_classic() const = 0;
        virtual bool supports_classic_server() const = 0;

        virtual PermissionStatus permission_status() const = 0;
        virtual Error permission_request(std::string& message) = 0;

        virtual Error le_scan_start(bool active, std::string& message) = 0;
        virtual Error le_scan_stop(std::string& message) = 0;
        virtual bool le_scan_is_running() const = 0;

        virtual Error classic_scan_start(std::string& message) = 0;
        virtual Error classic_scan_stop(std::string& message) = 0;
        virtual bool classic_scan_is_running() const = 0;

        virtual Error classic_connect(
            std::uint64_t connection,
            const DiscoveredDevice& device,
            const std::string& service_uuid,
            std::string& message) = 0;

        virtual Error classic_disconnect(
            std::uint64_t connection,
            std::string& message) = 0;

        virtual bool classic_connection_is_connected(
            std::uint64_t connection) const = 0;

        virtual std::int32_t classic_receive_available(
            std::uint64_t connection) const = 0;

        // Native byte helpers are intentionally available behind the common
        // interface even though the current public GMBuffer wrappers remain
        // TODO. This lets platform transport/queues be complete now.
        virtual Error classic_send_bytes(
            std::uint64_t connection,
            const std::uint8_t* data,
            std::size_t size,
            std::string& message) = 0;

        virtual std::size_t classic_receive_bytes(
            std::uint64_t connection,
            std::uint8_t* out,
            std::size_t max_size) = 0;

        virtual Error classic_server_start(
            const std::string& name,
            const std::string& service_uuid,
            std::string& message) = 0;

        virtual Error classic_server_stop(std::string& message) = 0;
        virtual bool classic_server_is_running() const = 0;
    };

    std::unique_ptr<Backend> create_platform_backend(CoreHooks hooks);
}
