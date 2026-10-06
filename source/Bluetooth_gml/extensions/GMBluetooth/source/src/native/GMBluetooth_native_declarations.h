#ifndef GMBLUETOOTH_NATIVE_DECLARATIONS_H
#define GMBLUETOOTH_NATIVE_DECLARATIONS_H

#include <cstdint>
#include <string>
#include <string_view>
#include "core/GMExtWire.h"

// The core's exports as code_gen/native/GMBluetoothInternal_native.h declares
// them, for the iOS glue. That header cannot be included beside
// code_gen/ios/GMBluetoothInternal_ios.h, which defines the same gm_enums and
// gm_structs, so the types are declared opaquely here and keep in step with
// the regen by hand.
namespace gm_enums
{
    enum class BluetoothError : std::int32_t;
    enum class BluetoothTransport : std::int32_t;
    enum class BluetoothPermissionStatus : std::int32_t;
    enum class BluetoothLeSubscribeMode : std::int32_t;
    enum class BluetoothLeWriteType : std::int32_t;
    enum class BluetoothAttError : std::int32_t;
}

namespace gm_structs
{
    struct BluetoothLeServiceDefinition;
    struct BluetoothLeAdvertiseSettings;
    struct BluetoothLeAdvertiseData;
}

bool bluetooth_initialize();
void bluetooth_shutdown();
bool bluetooth_is_initialized();
gm_enums::BluetoothError bluetooth_last_error_code();
std::string bluetooth_last_error_message();
bool bluetooth_le_is_supported();
bool bluetooth_le_advertise_is_supported();
bool bluetooth_le_server_is_supported();
bool bluetooth_classic_is_supported();
bool bluetooth_classic_server_is_supported();
gm_enums::BluetoothPermissionStatus bluetooth_permission_get_status();
gm_enums::BluetoothError bluetooth_permission_request(const gm::wire::GMFunction& callback);
gm_enums::BluetoothError bluetooth_le_scan_start(bool active);
gm_enums::BluetoothError bluetooth_le_scan_stop();
bool bluetooth_le_scan_is_running();
gm_enums::BluetoothError bluetooth_classic_scan_start();
gm_enums::BluetoothError bluetooth_classic_scan_stop();
bool bluetooth_classic_scan_is_running();
void bluetooth_device_clear();
std::int32_t bluetooth_device_get_count();
std::uint64_t bluetooth_device_get_at(std::int32_t index);
bool bluetooth_device_is_valid(std::uint64_t device);
gm_enums::BluetoothTransport bluetooth_device_get_transport(std::uint64_t device);
std::string bluetooth_device_get_id(std::uint64_t device);
std::string bluetooth_device_get_name(std::uint64_t device);
bool bluetooth_device_has_address(std::uint64_t device);
std::string bluetooth_device_get_address(std::uint64_t device);
bool bluetooth_device_has_rssi(std::uint64_t device);
std::int32_t bluetooth_device_get_rssi(std::uint64_t device);
bool bluetooth_device_is_connectable(std::uint64_t device);
std::uint64_t bluetooth_classic_connect(std::uint64_t device, std::string_view service_uuid, const gm::wire::GMFunction& callback);
gm_enums::BluetoothError bluetooth_classic_disconnect(std::uint64_t connection);
bool bluetooth_classic_connection_is_valid(std::uint64_t connection);
bool bluetooth_classic_connection_is_connected(std::uint64_t connection);
std::uint64_t bluetooth_classic_connection_get_device(std::uint64_t connection);
std::int32_t bluetooth_classic_receive_available(std::uint64_t connection);
gm_enums::BluetoothError bluetooth_classic_send(std::uint64_t connection, gm::wire::GMBuffer data, std::uint32_t offset, std::uint32_t size);
std::int32_t bluetooth_classic_receive(std::uint64_t connection, gm::wire::GMBuffer out_data, std::uint32_t offset, std::uint32_t max_size);
gm_enums::BluetoothError bluetooth_classic_server_start(std::string_view name, std::string_view service_uuid);
gm_enums::BluetoothError bluetooth_classic_server_stop();
bool bluetooth_classic_server_is_running();
gm_enums::BluetoothError bluetooth_classic_discoverable_start(std::int32_t duration_seconds);
gm_enums::BluetoothError bluetooth_classic_discoverable_stop();
bool bluetooth_classic_discoverable_is_running();
bool bluetooth_pairing_is_supported(std::uint64_t device);
gm_enums::BluetoothError bluetooth_pair(std::uint64_t device, const gm::wire::GMFunction& callback);
bool bluetooth_device_is_paired(std::uint64_t device);
std::uint64_t bluetooth_le_connect(std::uint64_t device, const gm::wire::GMFunction& callback);
gm_enums::BluetoothError bluetooth_le_disconnect(std::uint64_t connection);
bool bluetooth_le_connection_is_valid(std::uint64_t connection);
bool bluetooth_le_connection_is_connected(std::uint64_t connection);
std::uint64_t bluetooth_le_connection_get_device(std::uint64_t connection);
gm_enums::BluetoothError bluetooth_le_services_discover(std::uint64_t connection, const gm::wire::GMFunction& callback);
std::int32_t bluetooth_le_service_get_count(std::uint64_t connection);
std::uint64_t bluetooth_le_service_get_at(std::uint64_t connection, std::int32_t index);
std::string bluetooth_le_service_get_uuid(std::uint64_t service);
gm_enums::BluetoothError bluetooth_le_characteristics_discover(std::uint64_t service, const gm::wire::GMFunction& callback);
std::int32_t bluetooth_le_characteristic_get_count(std::uint64_t service);
std::uint64_t bluetooth_le_characteristic_get_at(std::uint64_t service, std::int32_t index);
std::string bluetooth_le_characteristic_get_uuid(std::uint64_t characteristic);
std::int32_t bluetooth_le_characteristic_get_properties(std::uint64_t characteristic);
gm_enums::BluetoothError bluetooth_le_descriptors_discover(std::uint64_t characteristic, const gm::wire::GMFunction& callback);
std::int32_t bluetooth_le_descriptor_get_count(std::uint64_t characteristic);
std::uint64_t bluetooth_le_descriptor_get_at(std::uint64_t characteristic, std::int32_t index);
std::string bluetooth_le_descriptor_get_uuid(std::uint64_t descriptor);
gm_enums::BluetoothError bluetooth_le_characteristic_read(std::uint64_t characteristic, const gm::wire::GMFunction& callback);
gm_enums::BluetoothError bluetooth_le_characteristic_write(std::uint64_t characteristic, gm::wire::GMBuffer data, std::uint32_t offset, std::uint32_t size, gm_enums::BluetoothLeWriteType write_type, const gm::wire::GMFunction& callback);
gm_enums::BluetoothError bluetooth_le_characteristic_subscribe(std::uint64_t characteristic, gm_enums::BluetoothLeSubscribeMode mode, const gm::wire::GMFunction& callback);
gm_enums::BluetoothError bluetooth_le_descriptor_read(std::uint64_t descriptor, const gm::wire::GMFunction& callback);
gm_enums::BluetoothError bluetooth_le_descriptor_write(std::uint64_t descriptor, gm::wire::GMBuffer data, std::uint32_t offset, std::uint32_t size, const gm::wire::GMFunction& callback);
gm_enums::BluetoothError bluetooth_le_value_copy(std::uint64_t value, gm::wire::GMBuffer out_data, std::uint32_t offset);
gm_enums::BluetoothError bluetooth_le_value_release(std::uint64_t value);
gm_enums::BluetoothError bluetooth_le_advertise_start(const gm_structs::BluetoothLeAdvertiseSettings& settings, const gm_structs::BluetoothLeAdvertiseData& data, const gm::wire::GMFunction& callback);
gm_enums::BluetoothError bluetooth_le_advertise_stop();
bool bluetooth_le_advertise_is_running();
gm_enums::BluetoothError bluetooth_le_server_start();
gm_enums::BluetoothError bluetooth_le_server_stop();
bool bluetooth_le_server_is_running();
gm_enums::BluetoothError bluetooth_le_server_add_service(const gm_structs::BluetoothLeServiceDefinition& service, const gm::wire::GMFunction& callback);
gm_enums::BluetoothError bluetooth_le_server_clear_services();
gm_enums::BluetoothError bluetooth_le_server_respond_read(std::int32_t request_id, gm_enums::BluetoothAttError error_code, gm::wire::GMBuffer data, std::uint32_t offset, std::uint32_t size);
gm_enums::BluetoothError bluetooth_le_server_respond_write(std::int32_t request_id, gm_enums::BluetoothAttError error_code);
std::int32_t bluetooth_le_server_write_request_get_value(std::int32_t request_id, gm::wire::GMBuffer out_data, std::uint32_t offset, std::uint32_t max_size);
gm_enums::BluetoothError bluetooth_le_server_notify_value(std::string_view service_uuid, std::string_view characteristic_uuid, std::uint64_t connection, gm::wire::GMBuffer data, std::uint32_t offset, std::uint32_t size);
bool bluetooth_set_callback_state_changed(const gm::wire::GMFunction& callback);
bool bluetooth_remove_callback_state_changed();
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

#endif
