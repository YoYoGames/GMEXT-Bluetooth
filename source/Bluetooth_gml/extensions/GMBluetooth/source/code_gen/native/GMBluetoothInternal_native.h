// ##### extgen :: Auto-generated file do not edit!! #####

#pragma once
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

}


namespace gm_structs
{

}

namespace gm::wire::codec
{
}

namespace gm::wire::details
{
}

bool bluetooth_initialize();
void bluetooth_shutdown();
bool bluetooth_is_initialized();
std::int32_t bluetooth_last_error_code();
std::string bluetooth_last_error_message();
bool bluetooth_le_is_supported();
bool bluetooth_classic_is_supported();
bool bluetooth_classic_server_is_supported();
std::int32_t bluetooth_permission_get_status();
std::int32_t bluetooth_permission_request();
std::int32_t bluetooth_le_scan_start(bool active);
std::int32_t bluetooth_le_scan_stop();
bool bluetooth_le_scan_is_running();
std::int32_t bluetooth_classic_scan_start();
std::int32_t bluetooth_classic_scan_stop();
bool bluetooth_classic_scan_is_running();
void bluetooth_device_clear();
std::int32_t bluetooth_device_get_count();
std::uint64_t bluetooth_device_get_at(std::int32_t index);
bool bluetooth_device_is_valid(std::uint64_t device);
std::int32_t bluetooth_device_get_transport(std::uint64_t device);
std::string bluetooth_device_get_id(std::uint64_t device);
std::string bluetooth_device_get_name(std::uint64_t device);
bool bluetooth_device_has_address(std::uint64_t device);
std::string bluetooth_device_get_address(std::uint64_t device);
bool bluetooth_device_has_rssi(std::uint64_t device);
std::int32_t bluetooth_device_get_rssi(std::uint64_t device);
bool bluetooth_device_is_connectable(std::uint64_t device);
std::uint64_t bluetooth_classic_connect(std::uint64_t device, std::string_view service_uuid, const gm::wire::GMFunction& callback);
std::int32_t bluetooth_classic_disconnect(std::uint64_t connection);
bool bluetooth_classic_connection_is_valid(std::uint64_t connection);
bool bluetooth_classic_connection_is_connected(std::uint64_t connection);
std::uint64_t bluetooth_classic_connection_get_device(std::uint64_t connection);
std::int32_t bluetooth_classic_receive_available(std::uint64_t connection);
std::int32_t bluetooth_classic_send(std::uint64_t connection, gm::wire::GMBuffer data, std::uint32_t offset, std::uint32_t size);
std::int32_t bluetooth_classic_receive(std::uint64_t connection, gm::wire::GMBuffer out_data, std::uint32_t offset, std::uint32_t max_size);
std::int32_t bluetooth_classic_server_start(std::string_view name, std::string_view service_uuid);
std::int32_t bluetooth_classic_server_stop();
bool bluetooth_classic_server_is_running();
std::int32_t bluetooth_classic_discoverable_start(std::int32_t duration_seconds);
std::int32_t bluetooth_classic_discoverable_stop();
bool bluetooth_classic_discoverable_is_running();
std::int32_t bluetooth_pair(std::uint64_t device, const gm::wire::GMFunction& callback);
bool bluetooth_device_is_paired(std::uint64_t device);
std::uint64_t bluetooth_le_connect(std::uint64_t device, const gm::wire::GMFunction& callback);
std::int32_t bluetooth_le_disconnect(std::uint64_t connection);
bool bluetooth_le_connection_is_valid(std::uint64_t connection);
bool bluetooth_le_connection_is_connected(std::uint64_t connection);
std::uint64_t bluetooth_le_connection_get_device(std::uint64_t connection);
std::int32_t bluetooth_le_services_discover(std::uint64_t connection, const gm::wire::GMFunction& callback);
std::int32_t bluetooth_le_service_get_count(std::uint64_t connection);
std::uint64_t bluetooth_le_service_get_at(std::uint64_t connection, std::int32_t index);
std::string bluetooth_le_service_get_uuid(std::uint64_t service);
std::int32_t bluetooth_le_characteristics_discover(std::uint64_t service, const gm::wire::GMFunction& callback);
std::int32_t bluetooth_le_characteristic_get_count(std::uint64_t service);
std::uint64_t bluetooth_le_characteristic_get_at(std::uint64_t service, std::int32_t index);
std::string bluetooth_le_characteristic_get_uuid(std::uint64_t characteristic);
std::int32_t bluetooth_le_characteristic_get_properties(std::uint64_t characteristic);
std::int32_t bluetooth_le_descriptors_discover(std::uint64_t characteristic, const gm::wire::GMFunction& callback);
std::int32_t bluetooth_le_descriptor_get_count(std::uint64_t characteristic);
std::uint64_t bluetooth_le_descriptor_get_at(std::uint64_t characteristic, std::int32_t index);
std::string bluetooth_le_descriptor_get_uuid(std::uint64_t descriptor);
std::int32_t bluetooth_le_characteristic_read(std::uint64_t characteristic, const gm::wire::GMFunction& callback);
std::int32_t bluetooth_le_characteristic_get_value(std::uint64_t characteristic, gm::wire::GMBuffer out_data, std::uint32_t offset, std::uint32_t max_size);
std::int32_t bluetooth_le_characteristic_write(std::uint64_t characteristic, gm::wire::GMBuffer data, std::uint32_t offset, std::uint32_t size, std::int32_t write_type, const gm::wire::GMFunction& callback);
std::int32_t bluetooth_le_characteristic_subscribe(std::uint64_t characteristic, std::int32_t mode, const gm::wire::GMFunction& callback);
std::int32_t bluetooth_le_descriptor_read(std::uint64_t descriptor, const gm::wire::GMFunction& callback);
std::int32_t bluetooth_le_descriptor_get_value(std::uint64_t descriptor, gm::wire::GMBuffer out_data, std::uint32_t offset, std::uint32_t max_size);
std::int32_t bluetooth_le_descriptor_write(std::uint64_t descriptor, gm::wire::GMBuffer data, std::uint32_t offset, std::uint32_t size, const gm::wire::GMFunction& callback);
std::int32_t bluetooth_le_advertise_start(std::string_view settings_json, std::string_view data_json, const gm::wire::GMFunction& callback);
std::int32_t bluetooth_le_advertise_stop();
bool bluetooth_le_advertise_is_running();
std::int32_t bluetooth_le_server_start();
std::int32_t bluetooth_le_server_stop();
bool bluetooth_le_server_is_running();
std::int32_t bluetooth_le_server_add_service(std::string_view service_json, const gm::wire::GMFunction& callback);
std::int32_t bluetooth_le_server_clear_services();
std::int32_t bluetooth_le_server_respond_read(std::int32_t request_id, std::int32_t error_code, gm::wire::GMBuffer data, std::uint32_t offset, std::uint32_t size);
std::int32_t bluetooth_le_server_respond_write(std::int32_t request_id, std::int32_t error_code);
std::int32_t bluetooth_le_server_write_request_get_value(std::int32_t request_id, gm::wire::GMBuffer out_data, std::uint32_t offset, std::uint32_t max_size);
std::int32_t bluetooth_le_server_notify_value(std::string_view service_uuid, std::string_view characteristic_uuid, std::uint64_t connection, gm::wire::GMBuffer data, std::uint32_t offset, std::uint32_t size);
bool bluetooth_set_callback_device_found(const gm::wire::GMFunction& callback);
bool bluetooth_remove_callback_device_found();
bool bluetooth_set_callback_scan_stopped(const gm::wire::GMFunction& callback);
bool bluetooth_remove_callback_scan_stopped();
bool bluetooth_set_callback_classic_client_connected(const gm::wire::GMFunction& callback);
bool bluetooth_remove_callback_classic_client_connected();
bool bluetooth_set_callback_classic_data(const gm::wire::GMFunction& callback);
bool bluetooth_remove_callback_classic_data();
bool bluetooth_set_callback_classic_disconnected(const gm::wire::GMFunction& callback);
bool bluetooth_remove_callback_classic_disconnected();
bool bluetooth_set_callback_le_disconnected(const gm::wire::GMFunction& callback);
bool bluetooth_remove_callback_le_disconnected();
bool bluetooth_set_callback_le_characteristic_value_changed(const gm::wire::GMFunction& callback);
bool bluetooth_remove_callback_le_characteristic_value_changed();
bool bluetooth_set_callback_le_server_connection_state_changed(const gm::wire::GMFunction& callback);
bool bluetooth_remove_callback_le_server_connection_state_changed();
bool bluetooth_set_callback_le_server_read_request(const gm::wire::GMFunction& callback);
bool bluetooth_remove_callback_le_server_read_request();
bool bluetooth_set_callback_le_server_write_request(const gm::wire::GMFunction& callback);
bool bluetooth_remove_callback_le_server_write_request();
