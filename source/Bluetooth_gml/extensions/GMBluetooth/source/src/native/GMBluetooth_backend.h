#pragma once

#include <chrono>
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
    // field set, UUIDs canonical, each mask as long as its data. The core
    // matches every result against the filters itself; a backend uses what it
    // can of them only to narrow its native scan.
    struct LeScanFilter
    {
        std::optional<std::string> service_uuid;
        std::optional<std::string> name;
        std::optional<std::string> name_prefix;
        std::optional<std::uint16_t> company_id;
        std::optional<std::vector<std::uint8_t>> manufacturer_data;
        std::optional<std::vector<std::uint8_t>> manufacturer_data_mask;
        std::optional<std::string> service_data_uuid;
        std::optional<std::vector<std::uint8_t>> service_data;
        std::optional<std::vector<std::uint8_t>> service_data_mask;
    };

    // BluetoothLeScanMode: Android's ScanSettings modes.
    enum class LeScanMode : std::int32_t
    {
        LowPower   = 0,
        Balanced   = 1,
        LowLatency = 2,
    };

    // A BluetoothLeScanSettings with its defaults filled in. A backend that
    // cannot do passive scans or scan modes ignores those fields.
    struct LeScanSettings
    {
        bool active = true;
        LeScanMode mode = LeScanMode::LowLatency;
    };

    // The window an le_connect or classic_connect has from the call to an open
    // link, a pairing prompt included; a connect still not open fails Timeout.
    constexpr auto k_default_connect_timeout = std::chrono::milliseconds(15000);

    // A service, characteristic or descriptor an LE discovery found.
    // instance is the backend's own name for it: non-zero, and unique among
    // the attributes of its kind on one LE connection, so two attributes that
    // share a UUID stay apart (R1-127). properties is the characteristic
    // property bitmask; 0 for the others.
    struct LeAttribute
    {
        std::string uuid;
        std::uint64_t instance = 0;
        std::int32_t properties = 0;
    };

    // The attribute an LE call works on, by the instances its discovery
    // reported: a characteristic call fills service and characteristic, a
    // descriptor call all three, a characteristic discovery only service.
    struct LeAttributeRef
    {
        std::uint64_t service = 0;
        std::uint64_t characteristic = 0;
        std::uint64_t descriptor = 0;
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

    // A GATT server service as the core hands it to le_server_add_service.
    // Each attribute carries the id the core minted for it, which the backend
    // keys the attribute by and reports on its requests; uuid is as GML gave
    // it, already checked. A 0x2902 descriptor is never listed: the platform
    // owns the CCCD. A descriptor with a value serves it itself, at most 512
    // bytes, and its reads never reach the core; a User Description (0x2901)
    // value is UTF-8 text.
    struct LeServerDescriptor
    {
        std::uint64_t id = 0;
        std::string uuid;
        std::optional<std::vector<std::uint8_t>> value;
    };

    struct LeServerCharacteristic
    {
        std::uint64_t id = 0;
        std::string uuid;
        std::int32_t properties = 0;
        std::int32_t permissions = 0;
        std::vector<std::uint8_t> value; // empty: no initial value
        std::vector<LeServerDescriptor> descriptors;
    };

    struct LeServerService
    {
        std::uint64_t id = 0;
        std::string uuid;
        std::vector<LeServerCharacteristic> characteristics;
    };

    // The remote central a GATT server event comes from. key names it for as
    // long as it stays connected, and the core maps it to a server connection
    // handle; device_id is the id a scan result for the same peer has, and
    // address is empty where the platform gives none.
    struct LeServerCentral
    {
        std::string key;
        std::string device_id;
        std::string address;
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
        // A scan of transport ended without le_scan_stop or classic_scan_stop:
        // error and message say why, Ok for an inquiry that ran its course. A
        // stop the core asked for is never reported.
        ScanStopped,
        ClassicConnected,
        ClassicClientConnected,
        ClassicDataAvailable,
        ClassicDisconnected,
        // A receive loop paused at k_classic_receive_budget, sent every
        // k_classic_receive_paused_expiry_check while it stays paused, so the
        // core expires values even when GML never fetches; connection names it.
        ClassicReceivePaused,
        // The radio's state changed: value is the BluetoothState.
        StateChanged,
        // The end of an le_connect: error is Ok when the link is open,
        // otherwise why it never came up, with message.
        LeConnectResult,
        // A link the game did not close went away: error and message say why.
        LeDisconnected,
        // A notification or indication on connection: attribute names the
        // characteristic by its service and characteristic instances, bytes
        // holds the value.
        LeValueChanged,
        // A GATT server central arrived (connected) or left; central names it,
        // and a departure may carry error and message.
        LeServerCentral,
        // Advertising that started Ok ended without le_advertise_stop: error
        // (never Ok) and message say why.
        LeAdvertiseStopped,
        // Discoverability that started Ok ended without
        // classic_discoverable_stop: Ok when its time ran out or the system
        // withdrew it, with message.
        ClassicDiscoverableStopped,
        // A read or write request on a local attribute from central:
        // attribute.characteristic is the characteristic's id and
        // attribute.descriptor the descriptor's, 0 for a characteristic
        // request; request_id, offset, and for a write bytes and
        // response_needed.
        LeServerReadRequest,
        LeServerWriteRequest,
        // The platform dropped every service the server had (Apple, on a
        // power-off). The core ends the server as the radio-off rule does.
        LeServerServicesReset,
        // The completion of an LE call that took an op id: op_id names the
        // call, error is Ok or its failure, message says why, and result holds
        // what it returned. The core knows what the call was from its op id.
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

        // ClassicDataAvailable: one chunk received on connection, copied off
        // the receive buffer before the push; the backend counts it as
        // outstanding until classic_receive_consumed gives it back.
        // LeValueChanged and LeServerWriteRequest: the value.
        std::vector<std::uint8_t> bytes;

        Error error = Error::Ok;
        std::int32_t value = 0;

        std::string message;

        // LeValueChanged and the server requests.
        LeAttributeRef attribute;

        // LeServerCentral and the server requests.
        LeServerCentral central;
        bool connected = false;

        // The server requests.
        std::int32_t request_id = 0;
        std::uint32_t offset = 0;
        bool response_needed = true;
    };

    // upsert_device is for what a scan saw: the running scan of the device's
    // transport may report it. add_device caches a device silently: an
    // inbound link's peer, a name looked up outside a scan.
    struct CoreHooks
    {
        std::function<std::uint64_t(const DiscoveredDevice&)> upsert_device;
        std::function<std::uint64_t(const DiscoveredDevice&)> add_device;
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
        // adapter/manager state should override this and push a StateChanged
        // event on each later transition.
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
        virtual Error le_scan_start(const LeScanSettings& settings, const std::vector<LeScanFilter>& filters, std::string& message) = 0;
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

        // service_uuid is canonical. timeout is the whole window to an open
        // link (k_default_connect_timeout unless GML asked otherwise).
        virtual Error classic_connect(
            std::uint64_t connection,
            const DiscoveredDevice& device,
            const std::string& service_uuid,
            std::chrono::milliseconds timeout,
            std::string& message)
        {
            (void)connection;
            (void)device;
            (void)service_uuid;
            (void)timeout;

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

        // Received bytes reach the core as ClassicDataAvailable chunks. The
        // backend counts the bytes it pushed for a connection as outstanding:
        // while they exceed k_classic_receive_budget it stops reading (or, where
        // a channel cannot pause, ends the connection with Disconnected "receive
        // buffer full"). The core gives bytes back here as GML fetches or
        // releases their values, or they expire; any thread, no core lock held.
        static constexpr std::size_t k_classic_receive_budget = 1024 * 1024;

        // Values expire only when one is stored, fetched or released, so a
        // backend that pauses reading pushes ClassicReceivePaused this often
        // while paused: an unfetched chunk then still ends its connection.
        static constexpr auto k_classic_receive_paused_expiry_check = std::chrono::seconds(5);

        virtual void classic_receive_consumed(
            std::uint64_t connection,
            std::size_t bytes)
        {
            (void)connection;
            (void)bytes;
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

        // timeout as for classic_connect.
        virtual Error le_connect(
            std::uint64_t connection,
            const DiscoveredDevice& device,
            std::chrono::milliseconds timeout,
            std::string& message)
        {
            (void)connection;
            (void)device;
            (void)timeout;

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
            const LeAttributeRef& service,
            std::string& message)
        {
            (void)op_id;
            (void)connection;
            (void)service;

            message = "BLE GATT is not supported by this backend";
            return Error::NotSupported;
        }

        virtual Error le_descriptors_discover(
            std::uint64_t op_id,
            std::uint64_t connection,
            const LeAttributeRef& characteristic,
            std::string& message)
        {
            (void)op_id;
            (void)connection;
            (void)characteristic;

            message = "BLE GATT is not supported by this backend";
            return Error::NotSupported;
        }

        virtual Error le_characteristic_read(
            std::uint64_t op_id,
            std::uint64_t connection,
            const LeAttributeRef& characteristic,
            std::string& message)
        {
            (void)op_id;
            (void)connection;
            (void)characteristic;

            message = "BLE GATT is not supported by this backend";
            return Error::NotSupported;
        }

        virtual Error le_characteristic_write(
            std::uint64_t op_id,
            std::uint64_t connection,
            const LeAttributeRef& characteristic,
            const std::vector<std::uint8_t>& value,
            bool with_response,
            std::string& message)
        {
            (void)op_id;
            (void)connection;
            (void)characteristic;
            (void)value;
            (void)with_response;

            message = "BLE GATT is not supported by this backend";
            return Error::NotSupported;
        }

        // mode: 0 unsubscribes, 1 notify, 2 indicate (BluetoothLeSubscribeMode).
        virtual Error le_characteristic_subscribe(
            std::uint64_t op_id,
            std::uint64_t connection,
            const LeAttributeRef& characteristic,
            std::int32_t mode,
            std::string& message)
        {
            (void)op_id;
            (void)connection;
            (void)characteristic;
            (void)mode;

            message = "BLE GATT is not supported by this backend";
            return Error::NotSupported;
        }

        virtual Error le_descriptor_read(
            std::uint64_t op_id,
            std::uint64_t connection,
            const LeAttributeRef& descriptor,
            std::string& message)
        {
            (void)op_id;
            (void)connection;
            (void)descriptor;

            message = "BLE GATT is not supported by this backend";
            return Error::NotSupported;
        }

        virtual Error le_descriptor_write(
            std::uint64_t op_id,
            std::uint64_t connection,
            const LeAttributeRef& descriptor,
            const std::vector<std::uint8_t>& value,
            std::string& message)
        {
            (void)op_id;
            (void)connection;
            (void)descriptor;
            (void)value;

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
            const LeServerService& service,
            std::string& message)
        {
            (void)op_id;
            (void)service;

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
            const std::vector<std::uint8_t>& value,
            std::string& message)
        {
            (void)request_id;
            (void)status;
            (void)value;

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

        // The ATT MTU of the central a server event named by key; 23 until a
        // larger one is negotiated, or when the platform cannot say.
        virtual std::int32_t le_server_connection_mtu(const std::string& central) const
        {
            (void)central;
            return 23;
        }

        // central: empty broadcasts to every subscriber; otherwise the key a
        // server event named the central by, and NotFound when that central is
        // not subscribed to the characteristic.
        // characteristic_id: the id the core gave the characteristic in its
        // LeServerService; NotFound when no live characteristic has it.
        virtual Error le_server_notify_value(
            std::uint64_t characteristic_id,
            const std::string& central,
            const std::vector<std::uint8_t>& value,
            std::string& message)
        {
            (void)characteristic_id;
            (void)central;
            (void)value;

            message = "BLE peripheral/server mode is not supported by this backend";
            return Error::NotSupported;
        }
    };

    std::unique_ptr<Backend> create_platform_backend(CoreHooks hooks);
}