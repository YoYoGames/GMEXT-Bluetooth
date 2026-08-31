#include "../GMBluetooth_backend.h"

#if defined(_WIN32)

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <atomic>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

#include <winrt/base.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Foundation.h>


namespace gmbluetooth
{
    namespace WDB = winrt::Windows::Devices::Bluetooth;
    namespace WDBA = winrt::Windows::Devices::Bluetooth::Advertisement;

    namespace
    {
        std::string format_bluetooth_address(std::uint64_t address)
        {
            std::ostringstream out;
            out << std::uppercase << std::hex << std::setfill('0');

            for (int byte = 5; byte >= 0; --byte)
            {
                if (byte != 5)
                    out << ':';

                out << std::setw(2)
                    << ((address >> (byte * 8)) & 0xFFULL);
            }

            return out.str();
        }

        bool advertisement_is_connectable(WDBA::BluetoothLEAdvertisementType type)
        {
            return type == WDBA::BluetoothLEAdvertisementType::ConnectableUndirected ||
                   type == WDBA::BluetoothLEAdvertisementType::ConnectableDirected;
        }

        Error map_bluetooth_error(WDB::BluetoothError error)
        {
            switch (error)
            {
                case WDB::BluetoothError::Success:
                    return Error::Ok;

                case WDB::BluetoothError::RadioNotAvailable:
                case WDB::BluetoothError::DisabledByUser:
                    return Error::BluetoothDisabled;

                case WDB::BluetoothError::ResourceInUse:
                    return Error::Busy;

                case WDB::BluetoothError::DeviceNotConnected:
                    return Error::Disconnected;

                case WDB::BluetoothError::DisabledByPolicy:
                case WDB::BluetoothError::ConsentRequired:
                    return Error::PermissionDenied;

                case WDB::BluetoothError::NotSupported:
                case WDB::BluetoothError::TransportNotSupported:
                    return Error::NotSupported;

                default:
                    return Error::OperationFailed;
            }
        }

        std::string bluetooth_error_message(WDB::BluetoothError error)
        {
            switch (error)
            {
                case WDB::BluetoothError::Success:
                    return {};
                case WDB::BluetoothError::RadioNotAvailable:
                    return "Bluetooth radio is not available";
                case WDB::BluetoothError::ResourceInUse:
                    return "Bluetooth resource is in use";
                case WDB::BluetoothError::DeviceNotConnected:
                    return "Bluetooth device is not connected";
                case WDB::BluetoothError::DisabledByPolicy:
                    return "Bluetooth is disabled by policy";
                case WDB::BluetoothError::NotSupported:
                    return "Bluetooth operation is not supported";
                case WDB::BluetoothError::DisabledByUser:
                    return "Bluetooth is disabled by the user";
                case WDB::BluetoothError::ConsentRequired:
                    return "Bluetooth operation requires consent";
                case WDB::BluetoothError::TransportNotSupported:
                    return "Bluetooth transport is not supported";
                default:
                    return "Windows Bluetooth operation failed";
            }
        }
    }

    class WindowsBackend final : public Backend
    {
    public:
        WindowsBackend(
            DeviceDiscoveredSink device_sink,
            ScanStoppedSink scan_stopped_sink)
            : device_sink_(std::move(device_sink)),
              scan_stopped_sink_(std::move(scan_stopped_sink))
        {
        }

        Error initialize(std::string& message) override
        {
            if (initialized_)
                return Error::Ok;

            try
            {
                winrt::init_apartment(winrt::apartment_type::multi_threaded);
                owns_apartment_ = true;
            }
            catch (const winrt::hresult_error& error)
            {
                // GameMaker or another extension may already have initialized
                // this thread using a different COM apartment model. WinRT's
                // Bluetooth watcher is agile, so an already-initialized thread
                // is acceptable; only fail for a genuine initialization error.
                if (error.code() != winrt::hresult{RPC_E_CHANGED_MODE})
                {
                    message = winrt::to_string(error.message());
                    return Error::OperationFailed;
                }
            }

            initialized_ = true;
            message.clear();
            return Error::Ok;
        }

        void shutdown() override
        {
            std::string ignored;
            le_scan_stop(ignored);
            release_watcher_noexcept();

            initialized_ = false;

            if (owns_apartment_)
            {
                winrt::uninit_apartment();
                owns_apartment_ = false;
            }
        }

        bool supports_ble() const override
        {
            return true;
        }

        bool supports_classic() const override
        {
            // Windows supports Classic, but this extension reports only features
            // whose backend has actually been implemented. RFCOMM comes next.
            return false;
        }

        Error le_scan_start(bool active, std::string& message) override
        {
            if (!initialized_)
            {
                message = "Bluetooth backend is not initialized";
                return Error::NotInitialized;
            }

            if (scanning_.load())
            {
                message.clear();
                return Error::Ok;
            }

            try
            {
                // A previous stopped watcher may still be retained so its
                // Stopped event could be delivered. Before starting a new scan,
                // detach that old source and create a fresh watcher.
                release_watcher_noexcept();
                watcher_ = WDBA::BluetoothLEAdvertisementWatcher{};
                watcher_.ScanningMode(
                    active
                        ? WDBA::BluetoothLEScanningMode::Active
                        : WDBA::BluetoothLEScanningMode::Passive);

                received_token_ = watcher_.Received(
                    [this](
                        const WDBA::BluetoothLEAdvertisementWatcher&,
                        const WDBA::BluetoothLEAdvertisementReceivedEventArgs& args)
                    {
                        const std::uint64_t raw_address = args.BluetoothAddress();
                        const std::string address = format_bluetooth_address(raw_address);

                        DiscoveredDevice device;
                        device.id = "win:ble:" + address;
                        device.address = address;
                        device.address_available = true;
                        device.rssi = static_cast<std::int32_t>(args.RawSignalStrengthInDBm());
                        device.rssi_available = device.rssi != -127;
                        device.connectable = advertisement_is_connectable(args.AdvertisementType());

                        const auto local_name = args.Advertisement().LocalName();
                        if (!local_name.empty())
                            device.name = winrt::to_string(local_name);

                        if (device_sink_)
                            device_sink_(device);
                    });

                stopped_token_ = watcher_.Stopped(
                    [this](
                        const WDBA::BluetoothLEAdvertisementWatcher&,
                        const WDBA::BluetoothLEAdvertisementWatcherStoppedEventArgs& args)
                    {
                        scanning_.store(false);

                        const WDB::BluetoothError native_error = args.Error();
                        const Error error = map_bluetooth_error(native_error);
                        const std::string message = bluetooth_error_message(native_error);

                        if (scan_stopped_sink_)
                            scan_stopped_sink_(error, message);
                    });

                scanning_.store(true);
                watcher_.Start();
                message.clear();
                return Error::Ok;
            }
            catch (const winrt::hresult_error& error)
            {
                scanning_.store(false);
                release_watcher_noexcept();
                message = winrt::to_string(error.message());
                return Error::OperationFailed;
            }
            catch (...)
            {
                scanning_.store(false);
                release_watcher_noexcept();
                message = "Failed to start Windows BLE scan";
                return Error::OperationFailed;
            }
        }

        Error le_scan_stop(std::string& message) override
        {
            if (!initialized_)
            {
                message = "Bluetooth backend is not initialized";
                return Error::NotInitialized;
            }

            if (!watcher_)
            {
                scanning_.store(false);
                message.clear();
                return Error::Ok;
            }

            try
            {
                // Keep handlers registered through Stop(), so the Stopped event
                // is normalized into our queued scan-stopped callback.
                watcher_.Stop();
                scanning_.store(false);
                message.clear();
                return Error::Ok;
            }
            catch (const winrt::hresult_error& error)
            {
                scanning_.store(false);
                message = winrt::to_string(error.message());
                return Error::OperationFailed;
            }
        }

        bool le_scan_is_running() const override
        {
            return scanning_.load();
        }

        ~WindowsBackend() override
        {
            release_watcher_noexcept();
        }

    private:
        void release_watcher_noexcept() noexcept
        {
            try
            {
                if (watcher_)
                {
                    if (received_token_.value != 0)
                    {
                        watcher_.Received(received_token_);
                        received_token_ = {};
                    }

                    if (stopped_token_.value != 0)
                    {
                        watcher_.Stopped(stopped_token_);
                        stopped_token_ = {};
                    }

                    watcher_ = nullptr;
                }
            }
            catch (...)
            {
            }
        }

        DeviceDiscoveredSink device_sink_;
        ScanStoppedSink scan_stopped_sink_;

        bool initialized_ = false;
        bool owns_apartment_ = false;
        std::atomic_bool scanning_{false};

        WDBA::BluetoothLEAdvertisementWatcher watcher_{nullptr};
        winrt::event_token received_token_{};
        winrt::event_token stopped_token_{};
    };

    std::unique_ptr<Backend> create_platform_backend(
        DeviceDiscoveredSink device_sink,
        ScanStoppedSink scan_stopped_sink)
    {
        return std::make_unique<WindowsBackend>(
            std::move(device_sink),
            std::move(scan_stopped_sink));
    }
}

#else

namespace gmbluetooth
{
    std::unique_ptr<Backend> create_platform_backend(
        DeviceDiscoveredSink,
        ScanStoppedSink)
    {
        return nullptr;
    }
}

#endif
