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
std::int32_t bluetooth_update();
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
