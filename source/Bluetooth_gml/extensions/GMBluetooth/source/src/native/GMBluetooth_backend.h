#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
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
        // An ATT refusal: read/write not permitted, request not supported.
        NotPermitted      = 14,
        // An ATT security refusal: authentication, authorization, encryption
        // or key size.
        InsufficientSecurity = 15,
    };

    // The BluetoothError an ATT error code reports to GML. Android's GATT
    // statuses share the ATT values below 0x80.
    inline Error map_att_error(int att)
    {
        switch (att)
        {
        case 0x00: return Error::Ok;
        case 0x02: case 0x03: case 0x06: return Error::NotPermitted;
        case 0x05: case 0x08: case 0x0C: case 0x0F: return Error::InsufficientSecurity;
        default: return Error::OperationFailed;
        }
    }

    // "ATT error 0x05: insufficient authentication", the message that goes
    // with map_att_error.
    inline std::string att_error_message(int att)
    {
        const char* name = "unknown error";
        switch (att)
        {
        case 0x01: name = "invalid handle"; break;
        case 0x02: name = "read not permitted"; break;
        case 0x03: name = "write not permitted"; break;
        case 0x04: name = "invalid PDU"; break;
        case 0x05: name = "insufficient authentication"; break;
        case 0x06: name = "request not supported"; break;
        case 0x07: name = "invalid offset"; break;
        case 0x08: name = "insufficient authorization"; break;
        case 0x09: name = "prepare queue full"; break;
        case 0x0A: name = "attribute not found"; break;
        case 0x0B: name = "attribute not long"; break;
        case 0x0C: name = "insufficient encryption key size"; break;
        case 0x0D: name = "invalid attribute value length"; break;
        case 0x0E: name = "unlikely error"; break;
        case 0x0F: name = "insufficient encryption"; break;
        case 0x10: name = "unsupported group type"; break;
        case 0x11: name = "insufficient resources"; break;
        default:
            if (att >= 0x80 && att <= 0x9F)
                name = "application error";
            break;
        }

        static const char hex[] = "0123456789ABCDEF";
        std::string code = "0x";
        code += hex[(att >> 4) & 0xF];
        code += hex[att & 0xF];
        return "ATT error " + code + ": " + name;
    }

    // BluetoothLeAttributePermission: Android's PERMISSION_* bits.
    constexpr std::int32_t kPermissionRead               = 1;
    constexpr std::int32_t kPermissionReadEncrypted      = 2;
    constexpr std::int32_t kPermissionReadEncryptedMitm  = 4;
    constexpr std::int32_t kPermissionWrite              = 16;
    constexpr std::int32_t kPermissionWriteEncrypted     = 32;
    constexpr std::int32_t kPermissionWriteEncryptedMitm = 64;
    constexpr std::int32_t kPermissionWriteSigned        = 128;
    constexpr std::int32_t kPermissionWriteSignedMitm    = 256;
    constexpr std::int32_t kPermissionAll = kPermissionRead | kPermissionReadEncrypted |
        kPermissionReadEncryptedMitm | kPermissionWrite | kPermissionWriteEncrypted |
        kPermissionWriteEncryptedMitm | kPermissionWriteSigned | kPermissionWriteSignedMitm;

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

    struct LeAdvertiseServiceData
    {
        std::string uuid;
        std::vector<std::uint8_t> data;
    };

    struct LeAdvertiseManufacturerData
    {
        std::uint16_t company_id = 0;
        std::vector<std::uint8_t> data;
    };

    // What one advertising packet (or Apple's merged dictionary) carried besides
    // the name. UUIDs may come in any form the platform spells them; the core
    // stores them canonical and merges packets into the device's record.
    struct LeAdvertisement
    {
        std::vector<std::string> service_uuids;
        std::vector<LeAdvertiseServiceData> service_data;
        std::vector<LeAdvertiseManufacturerData> manufacturer_data;
        std::optional<std::int32_t> tx_power; // advertised TX Power Level, dBm
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

        // LE scan results only; empty for anything else.
        std::optional<LeAdvertisement> advertisement;
    };

    // A BluetoothLeScanFilter as the core hands it on: checked, at least one
    // field set, service_uuid canonical. The core matches every result against
    // the filters itself; a backend uses them only to narrow its native scan.
    struct LeScanFilter
    {
        std::optional<std::string> service_uuid;
        std::optional<std::string> name;
        std::optional<std::uint16_t> company_id;
    };

    // A service, characteristic or descriptor an LE discovery found.
    // properties is the characteristic property bitmask; 0 for the others.
    struct LeAttribute
    {
        std::string uuid;
        std::int32_t properties = 0;
    };

    // BluetoothLeAdvertiseTxPower: Android's AdvertiseSettings levels.
    enum class LeAdvertiseTxPower : std::int32_t
    {
        UltraLow = 0,
        Low      = 1,
        Medium   = 2,
        High     = 3,
    };

    // The advertise structs as the core hands them on: checked, UUIDs in
    // canonical form (lowercase 128-bit), every service_data UUID also in
    // service_uuids, company ids in range. A field a backend cannot send is
    // that backend's NotSupported, returned before anything starts.
    struct LeAdvertiseSettings
    {
        bool connectable = true;
        std::optional<LeAdvertiseTxPower> tx_power; // empty: the platform default
    };

    struct LeAdvertiseData
    {
        bool include_name = false;
        bool include_tx_power = false;
        std::vector<std::string> service_uuids;
        std::vector<LeAdvertiseServiceData> service_data;
        std::vector<LeAdvertiseManufacturerData> manufacturer_data;
    };

    // What an LE call returns besides its error. Discovery fills attributes;
    // a characteristic or descriptor read fills value; an RSSI read fills
    // number with the dBm; the rest leave them empty.
    struct LeOpResult
    {
        std::vector<LeAttribute> attributes;
        std::vector<std::uint8_t> value;
        std::int32_t number = 0;
    };

    enum class BackendEventType : std::uint8_t
    {
        ScanStopped,
        ClassicConnected,
        ClassicClientConnected,
        ClassicDataAvailable,
        ClassicDisconnected,
        LeEvent,
        // The completion of an LE call that took an op id: op_id names the
        // call, error is Ok or its failure, message says why, and result holds
        // what it returned. No event_type and no json: the core knows what
        // the call was from its op id.
        LeOpCompleted,
        DevicePaired,
        // The answer to permission_request: value is the PermissionStatus.
        PermissionResult,
        // The answer to request_enable: error is Ok when the radio is on.
        EnableResult,
        // The answer to a device query: op_id is the query id the core passed,
        // devices what it found (Ok with none is an empty list).
        DevicesQueried,
    };

    struct BackendEvent
    {
        BackendEventType type = BackendEventType::ScanStopped;
        Transport transport = Transport::Unknown;

        std::uint64_t connection = 0;
        std::uint64_t device = 0;

        // LeOpCompleted only: the op id the core passed to the call, and what
        // the call returned. DevicesQueried: the query id.
        std::uint64_t op_id = 0;
        LeOpResult result;

        // DevicesQueried only.
        std::vector<DiscoveredDevice> devices;

        Error error = Error::Ok;
        std::int32_t value = 0;

        std::string message;

        // BLE event stream. event_type is the normalized public event name and
        // json preserves the old extension's proven payload schema. The GATT
        // server events (bluetooth_le_server_connection_state_changed and the
        // read and write requests) carry "central", a string naming the remote
        // central for as long as it stays connected, which the core maps to a
        // server connection handle; connection_state_changed with connected
        // false is that central's disconnect. "address" or Apple's nested
        // "device" names the device.
        std::string event_type;
        std::string json;
    };

    struct CoreHooks
    {
        std::function<std::uint64_t(const DiscoveredDevice&)> upsert_device;
        std::function<std::uint64_t(std::uint64_t)> create_classic_connection;
        std::function<std::uint64_t(std::uint64_t)> create_le_connection;
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

        virtual bool pairing_is_supported(const DiscoveredDevice& device) const
        {
            (void)device;
            return false;
        }

        // BluetoothFeature raw values from spec.gmidl. The core answers LeCentral,
        // LeAdvertise, LeServer, Classic and ClassicServer from the supports_*
        // calls above and asks this for the rest; false for anything unknown.
        virtual bool feature_supported(std::int32_t feature) const
        {
            (void)feature;
            return false;
        }

        // BluetoothState raw values from spec.gmidl. Backends that can observe
        // adapter/manager state should override this and emit
        // bluetooth_state_changed LeEvent events on subsequent transitions.
        virtual std::int32_t current_bluetooth_state() const
        {
            return 0; // BluetoothState.Unknown
        }

        virtual PermissionStatus permission_status() const
        {
            return PermissionStatus::Granted;
        }

        // Ok when the request started or the answer is already known; either
        // way the backend then pushes one PermissionResult event, value the
        // PermissionStatus, once the user has answered (at once when known).
        // Any other error is a pre-flight failure and pushes nothing.
        virtual Error permission_request(std::string& message) = 0;

        // Called only while the radio is not on. Ok when the request started;
        // the backend then pushes one EnableResult event. Any other error is a
        // pre-flight failure and pushes nothing.
        virtual Error request_enable(std::string& message)
        {
            message = "This platform cannot turn the Bluetooth radio on";
            return Error::NotSupported;
        }

        // Builds the cache entry for an id from bluetooth_device_get_id: the
        // transport, id and address, and whatever name the platform knows.
        // InvalidArgument for an id this backend does not issue.
        virtual Error device_from_id(const std::string& id, DiscoveredDevice& device, std::string& message)
        {
            (void)id;
            (void)device;
            message = "The id is not one this platform issues";
            return Error::InvalidArgument;
        }

        // Each pushes one DevicesQueried event carrying query_id unless it
        // fails synchronously. service_uuids are canonical.
        virtual Error le_connected_devices_query(
            std::uint64_t query_id,
            const std::vector<std::string>& service_uuids,
            std::string& message)
        {
            (void)query_id;
            (void)service_uuids;
            message = "Listing connected devices is not supported by this backend";
            return Error::NotSupported;
        }

        virtual Error paired_devices_query(std::uint64_t query_id, std::string& message)
        {
            (void)query_id;
            message = "Listing paired devices is not supported on this platform";
            return Error::NotSupported;
        }

        // filters is empty for an unfiltered scan. The core applies them to
        // every result; a backend may also hand them to its native scan.
        virtual Error le_scan_start(bool active, const std::vector<LeScanFilter>& filters, std::string& message) = 0;
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

        virtual Error classic_discoverable_start(std::int32_t duration_seconds, std::string& message)
        {
            (void)duration_seconds;

            message = "Bluetooth Classic discoverability is not supported by this backend";
            return Error::NotSupported;
        }

        virtual Error classic_discoverable_stop(std::string& message)
        {
            message = "Bluetooth Classic discoverability is not supported by this backend";
            return Error::NotSupported;
        }

        virtual bool classic_discoverable_is_running() const
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

        virtual Error pair(
            std::uint64_t device_handle,
            const DiscoveredDevice& device,
            std::string& message)
        {
            (void)device_handle;
            (void)device;

            message = "Bluetooth pairing is not supported by this backend";
            return Error::NotSupported;
        }

        virtual bool is_paired(const DiscoveredDevice& device) const
        {
            (void)device;
            return false;
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

        // The ATT MTU of a connected client connection, which the platform
        // negotiates itself; 23 when it cannot say.
        virtual std::int32_t le_connection_mtu(
            std::uint64_t connection) const
        {
            (void)connection;
            return 23;
        }

        // Completes with one LeOpCompleted whose result.number is the RSSI.
        virtual Error le_read_rssi(
            std::uint64_t op_id,
            std::uint64_t connection,
            std::string& message)
        {
            (void)op_id;
            (void)connection;
            message = "Reading the RSSI of a connection is not supported on this platform";
            return Error::NotSupported;
        }

        // priority: BluetoothLeConnectionPriority. Ok means the request was
        // made; no result follows.
        virtual Error le_request_connection_priority(
            std::uint64_t connection,
            std::int32_t priority,
            std::string& message)
        {
            (void)connection;
            (void)priority;
            message = "Connection priority is not supported on this platform";
            return Error::NotSupported;
        }

        // The LE operations below complete asynchronously. Each takes the op id
        // the core minted for the call and completes it with exactly one
        // LeOpCompleted event carrying that id, unless it fails synchronously.
        // Completions may arrive in any order; the core matches them by id.
        virtual Error le_services_discover(
            std::uint64_t op_id,
            std::uint64_t connection,
            std::string& message)
        {
            (void)op_id;
            (void)connection;

            message = "BLE GATT is not supported by this backend";
            return Error::NotSupported;
        }

        virtual Error le_characteristics_discover(
            std::uint64_t op_id,
            std::uint64_t connection,
            const std::string& service_uuid,
            std::string& message)
        {
            (void)op_id;
            (void)connection;
            (void)service_uuid;

            message = "BLE GATT is not supported by this backend";
            return Error::NotSupported;
        }

        virtual Error le_descriptors_discover(
            std::uint64_t op_id,
            std::uint64_t connection,
            const std::string& service_uuid,
            const std::string& characteristic_uuid,
            std::string& message)
        {
            (void)op_id;
            (void)connection;
            (void)service_uuid;
            (void)characteristic_uuid;

            message = "BLE GATT is not supported by this backend";
            return Error::NotSupported;
        }

        virtual Error le_characteristic_read(
            std::uint64_t op_id,
            std::uint64_t connection,
            const std::string& service_uuid,
            const std::string& characteristic_uuid,
            std::string& message)
        {
            (void)op_id;
            (void)connection;
            (void)service_uuid;
            (void)characteristic_uuid;

            message = "BLE GATT is not supported by this backend";
            return Error::NotSupported;
        }

        virtual Error le_characteristic_write(
            std::uint64_t op_id,
            std::uint64_t connection,
            const std::string& service_uuid,
            const std::string& characteristic_uuid,
            const std::string& value_base64,
            bool with_response,
            std::string& message)
        {
            (void)op_id;
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
            std::uint64_t op_id,
            std::uint64_t connection,
            const std::string& service_uuid,
            const std::string& characteristic_uuid,
            std::int32_t mode,
            std::string& message)
        {
            (void)op_id;
            (void)connection;
            (void)service_uuid;
            (void)characteristic_uuid;
            (void)mode;

            message = "BLE GATT is not supported by this backend";
            return Error::NotSupported;
        }

        virtual Error le_descriptor_read(
            std::uint64_t op_id,
            std::uint64_t connection,
            const std::string& service_uuid,
            const std::string& characteristic_uuid,
            const std::string& descriptor_uuid,
            std::string& message)
        {
            (void)op_id;
            (void)connection;
            (void)service_uuid;
            (void)characteristic_uuid;
            (void)descriptor_uuid;

            message = "BLE GATT is not supported by this backend";
            return Error::NotSupported;
        }

        virtual Error le_descriptor_write(
            std::uint64_t op_id,
            std::uint64_t connection,
            const std::string& service_uuid,
            const std::string& characteristic_uuid,
            const std::string& descriptor_uuid,
            const std::string& value_base64,
            std::string& message)
        {
            (void)op_id;
            (void)connection;
            (void)service_uuid;
            (void)characteristic_uuid;
            (void)descriptor_uuid;
            (void)value_base64;

            message = "BLE GATT is not supported by this backend";
            return Error::NotSupported;
        }

        virtual Error le_advertise_start(
            std::uint64_t op_id,
            const LeAdvertiseSettings& settings,
            const LeAdvertiseData& data,
            std::string& message)
        {
            (void)op_id;
            (void)settings;
            (void)data;

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
            std::uint64_t op_id,
            const std::string& service_json,
            std::string& message)
        {
            (void)op_id;
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

        // central: empty broadcasts to every subscriber; otherwise the key a
        // server event named the central by, and NotFound when that central is
        // not subscribed to the characteristic.
        virtual Error le_server_notify_value(
            const std::string& service_uuid,
            const std::string& characteristic_uuid,
            const std::string& central,
            const std::string& value_base64,
            std::string& message)
        {
            (void)service_uuid;
            (void)characteristic_uuid;
            (void)central;
            (void)value_base64;

            message = "BLE peripheral/server mode is not supported by this backend";
            return Error::NotSupported;
        }
    };

    std::unique_ptr<Backend> create_platform_backend(CoreHooks hooks);
}