#pragma once

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

    struct DiscoveredDevice
    {
        std::string id;
        std::string name;
        std::string address;

        bool address_available = false;
        bool connectable = false;

        std::int32_t rssi = 0;
        bool rssi_available = false;
    };

    using DeviceDiscoveredSink = std::function<void(const DiscoveredDevice&)>;
    using ScanStoppedSink = std::function<void(Error, const std::string&)>;

    class Backend
    {
    public:
        virtual ~Backend() = default;

        virtual Error initialize(std::string& message) = 0;
        virtual void shutdown() = 0;

        virtual bool supports_ble() const = 0;
        virtual bool supports_classic() const = 0;

        virtual Error le_scan_start(bool active, std::string& message) = 0;
        virtual Error le_scan_stop(std::string& message) = 0;
        virtual bool le_scan_is_running() const = 0;
    };

    std::unique_ptr<Backend> create_platform_backend(
        DeviceDiscoveredSink device_sink,
        ScanStoppedSink scan_stopped_sink);
}
