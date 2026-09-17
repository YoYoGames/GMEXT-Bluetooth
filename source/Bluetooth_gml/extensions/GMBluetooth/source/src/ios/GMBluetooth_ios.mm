#import "GMBluetooth_ios.h"
#include "../native/GMBluetooth_native_declarations.h"

@implementation GMBluetooth

- (bool)bluetooth_initialize { return ::bluetooth_initialize(); }
- (void)bluetooth_shutdown { ::bluetooth_shutdown(); }
- (bool)bluetooth_is_initialized { return ::bluetooth_is_initialized(); }

- (std::int32_t)bluetooth_last_error_code { return ::bluetooth_last_error_code(); }
- (std::string)bluetooth_last_error_message { return ::bluetooth_last_error_message(); }

- (bool)bluetooth_le_is_supported { return ::bluetooth_le_is_supported(); }
- (bool)bluetooth_le_advertise_is_supported { return ::bluetooth_le_advertise_is_supported(); }
- (bool)bluetooth_le_server_is_supported { return ::bluetooth_le_server_is_supported(); }
- (bool)bluetooth_classic_is_supported { return ::bluetooth_classic_is_supported(); }
- (bool)bluetooth_classic_server_is_supported { return ::bluetooth_classic_server_is_supported(); }
- (std::int32_t)bluetooth_permission_get_status { return ::bluetooth_permission_get_status(); }
- (std::int32_t)bluetooth_permission_request { return ::bluetooth_permission_request(); }

- (std::int32_t)bluetooth_le_scan_start:(bool)active { return ::bluetooth_le_scan_start(active); }
- (std::int32_t)bluetooth_le_scan_stop { return ::bluetooth_le_scan_stop(); }
- (bool)bluetooth_le_scan_is_running { return ::bluetooth_le_scan_is_running(); }

- (std::int32_t)bluetooth_classic_scan_start { return ::bluetooth_classic_scan_start(); }
- (std::int32_t)bluetooth_classic_scan_stop { return ::bluetooth_classic_scan_stop(); }
- (bool)bluetooth_classic_scan_is_running { return ::bluetooth_classic_scan_is_running(); }

- (void)bluetooth_device_clear { ::bluetooth_device_clear(); }
- (std::int32_t)bluetooth_device_get_count { return ::bluetooth_device_get_count(); }
- (std::uint64_t)bluetooth_device_get_at:(std::int32_t)index { return ::bluetooth_device_get_at(index); }
- (bool)bluetooth_device_is_valid:(std::uint64_t)device { return ::bluetooth_device_is_valid(device); }
- (std::int32_t)bluetooth_device_get_transport:(std::uint64_t)device { return ::bluetooth_device_get_transport(device); }
- (std::string)bluetooth_device_get_id:(std::uint64_t)device { return ::bluetooth_device_get_id(device); }
- (std::string)bluetooth_device_get_name:(std::uint64_t)device { return ::bluetooth_device_get_name(device); }
- (bool)bluetooth_device_has_address:(std::uint64_t)device { return ::bluetooth_device_has_address(device); }
- (std::string)bluetooth_device_get_address:(std::uint64_t)device { return ::bluetooth_device_get_address(device); }
- (bool)bluetooth_device_has_rssi:(std::uint64_t)device { return ::bluetooth_device_has_rssi(device); }
- (std::int32_t)bluetooth_device_get_rssi:(std::uint64_t)device { return ::bluetooth_device_get_rssi(device); }
- (bool)bluetooth_device_is_connectable:(std::uint64_t)device { return ::bluetooth_device_is_connectable(device); }

- (std::uint64_t)bluetooth_classic_connect:(std::uint64_t)device
                              service_uuid:(std::string_view)service_uuid
                                  callback:(gm::wire::GMFunction)callback
{ return ::bluetooth_classic_connect(device, service_uuid, callback); }
- (std::int32_t)bluetooth_classic_disconnect:(std::uint64_t)connection { return ::bluetooth_classic_disconnect(connection); }
- (bool)bluetooth_classic_connection_is_valid:(std::uint64_t)connection { return ::bluetooth_classic_connection_is_valid(connection); }
- (bool)bluetooth_classic_connection_is_connected:(std::uint64_t)connection { return ::bluetooth_classic_connection_is_connected(connection); }
- (std::uint64_t)bluetooth_classic_connection_get_device:(std::uint64_t)connection { return ::bluetooth_classic_connection_get_device(connection); }
- (std::int32_t)bluetooth_classic_receive_available:(std::uint64_t)connection { return ::bluetooth_classic_receive_available(connection); }
- (std::int32_t)bluetooth_classic_send:(std::uint64_t)connection data:(gm::wire::GMBuffer)data offset:(std::uint32_t)offset size:(std::uint32_t)size { return ::bluetooth_classic_send(connection, data, offset, size); }
- (std::int32_t)bluetooth_classic_receive:(std::uint64_t)connection out_data:(gm::wire::GMBuffer)out_data offset:(std::uint32_t)offset max_size:(std::uint32_t)max_size { return ::bluetooth_classic_receive(connection, out_data, offset, max_size); }
- (std::int32_t)bluetooth_classic_server_start:(std::string_view)name service_uuid:(std::string_view)service_uuid { return ::bluetooth_classic_server_start(name, service_uuid); }
- (std::int32_t)bluetooth_classic_server_stop { return ::bluetooth_classic_server_stop(); }
- (bool)bluetooth_classic_server_is_running { return ::bluetooth_classic_server_is_running(); }
- (std::int32_t)bluetooth_classic_discoverable_start:(std::int32_t)duration_seconds { return ::bluetooth_classic_discoverable_start(duration_seconds); }
- (std::int32_t)bluetooth_classic_discoverable_stop { return ::bluetooth_classic_discoverable_stop(); }
- (bool)bluetooth_classic_discoverable_is_running { return ::bluetooth_classic_discoverable_is_running(); }

- (bool)bluetooth_pairing_is_supported:(std::uint64_t)device { return ::bluetooth_pairing_is_supported(device); }
- (std::int32_t)bluetooth_pair:(std::uint64_t)device callback:(gm::wire::GMFunction)callback { return ::bluetooth_pair(device, callback); }
- (bool)bluetooth_device_is_paired:(std::uint64_t)device { return ::bluetooth_device_is_paired(device); }

- (std::uint64_t)bluetooth_le_connect:(std::uint64_t)device callback:(gm::wire::GMFunction)callback { return ::bluetooth_le_connect(device, callback); }
- (std::int32_t)bluetooth_le_disconnect:(std::uint64_t)connection { return ::bluetooth_le_disconnect(connection); }
- (bool)bluetooth_le_connection_is_valid:(std::uint64_t)connection { return ::bluetooth_le_connection_is_valid(connection); }
- (bool)bluetooth_le_connection_is_connected:(std::uint64_t)connection { return ::bluetooth_le_connection_is_connected(connection); }
- (std::uint64_t)bluetooth_le_connection_get_device:(std::uint64_t)connection { return ::bluetooth_le_connection_get_device(connection); }

- (std::int32_t)bluetooth_le_services_discover:(std::uint64_t)connection callback:(gm::wire::GMFunction)callback { return ::bluetooth_le_services_discover(connection, callback); }
- (std::int32_t)bluetooth_le_service_get_count:(std::uint64_t)connection { return ::bluetooth_le_service_get_count(connection); }
- (std::uint64_t)bluetooth_le_service_get_at:(std::uint64_t)connection index:(std::int32_t)index { return ::bluetooth_le_service_get_at(connection, index); }
- (std::string)bluetooth_le_service_get_uuid:(std::uint64_t)service { return ::bluetooth_le_service_get_uuid(service); }
- (std::int32_t)bluetooth_le_characteristics_discover:(std::uint64_t)service callback:(gm::wire::GMFunction)callback { return ::bluetooth_le_characteristics_discover(service, callback); }
- (std::int32_t)bluetooth_le_characteristic_get_count:(std::uint64_t)service { return ::bluetooth_le_characteristic_get_count(service); }
- (std::uint64_t)bluetooth_le_characteristic_get_at:(std::uint64_t)service index:(std::int32_t)index { return ::bluetooth_le_characteristic_get_at(service, index); }
- (std::string)bluetooth_le_characteristic_get_uuid:(std::uint64_t)characteristic { return ::bluetooth_le_characteristic_get_uuid(characteristic); }
- (std::int32_t)bluetooth_le_characteristic_get_properties:(std::uint64_t)characteristic { return ::bluetooth_le_characteristic_get_properties(characteristic); }
- (std::int32_t)bluetooth_le_descriptors_discover:(std::uint64_t)characteristic callback:(gm::wire::GMFunction)callback { return ::bluetooth_le_descriptors_discover(characteristic, callback); }
- (std::int32_t)bluetooth_le_descriptor_get_count:(std::uint64_t)characteristic { return ::bluetooth_le_descriptor_get_count(characteristic); }
- (std::uint64_t)bluetooth_le_descriptor_get_at:(std::uint64_t)characteristic index:(std::int32_t)index { return ::bluetooth_le_descriptor_get_at(characteristic, index); }
- (std::string)bluetooth_le_descriptor_get_uuid:(std::uint64_t)descriptor { return ::bluetooth_le_descriptor_get_uuid(descriptor); }

- (std::int32_t)bluetooth_le_characteristic_read:(std::uint64_t)characteristic callback:(gm::wire::GMFunction)callback { return ::bluetooth_le_characteristic_read(characteristic, callback); }
- (std::int32_t)bluetooth_le_characteristic_get_value:(std::uint64_t)characteristic out_data:(gm::wire::GMBuffer)out_data offset:(std::uint32_t)offset max_size:(std::uint32_t)max_size { return ::bluetooth_le_characteristic_get_value(characteristic, out_data, offset, max_size); }
- (std::int32_t)bluetooth_le_characteristic_write:(std::uint64_t)characteristic data:(gm::wire::GMBuffer)data offset:(std::uint32_t)offset size:(std::uint32_t)size write_type:(std::int32_t)write_type callback:(gm::wire::GMFunction)callback { return ::bluetooth_le_characteristic_write(characteristic, data, offset, size, write_type, callback); }
- (std::int32_t)bluetooth_le_characteristic_subscribe:(std::uint64_t)characteristic mode:(std::int32_t)mode callback:(gm::wire::GMFunction)callback { return ::bluetooth_le_characteristic_subscribe(characteristic, mode, callback); }
- (std::int32_t)bluetooth_le_descriptor_read:(std::uint64_t)descriptor callback:(gm::wire::GMFunction)callback { return ::bluetooth_le_descriptor_read(descriptor, callback); }
- (std::int32_t)bluetooth_le_descriptor_get_value:(std::uint64_t)descriptor out_data:(gm::wire::GMBuffer)out_data offset:(std::uint32_t)offset max_size:(std::uint32_t)max_size { return ::bluetooth_le_descriptor_get_value(descriptor, out_data, offset, max_size); }
- (std::int32_t)bluetooth_le_descriptor_write:(std::uint64_t)descriptor data:(gm::wire::GMBuffer)data offset:(std::uint32_t)offset size:(std::uint32_t)size callback:(gm::wire::GMFunction)callback { return ::bluetooth_le_descriptor_write(descriptor, data, offset, size, callback); }

- (std::int32_t)bluetooth_le_advertise_start:(std::string_view)settings_json data_json:(std::string_view)data_json callback:(gm::wire::GMFunction)callback { return ::bluetooth_le_advertise_start(settings_json, data_json, callback); }
- (std::int32_t)bluetooth_le_advertise_stop { return ::bluetooth_le_advertise_stop(); }
- (bool)bluetooth_le_advertise_is_running { return ::bluetooth_le_advertise_is_running(); }

- (std::int32_t)bluetooth_le_server_start { return ::bluetooth_le_server_start(); }
- (std::int32_t)bluetooth_le_server_stop { return ::bluetooth_le_server_stop(); }
- (bool)bluetooth_le_server_is_running { return ::bluetooth_le_server_is_running(); }
- (std::int32_t)bluetooth_le_server_add_service:(const gm_structs::BluetoothLeServiceDefinition&)service callback:(gm::wire::GMFunction)callback { return ::bluetooth_le_server_add_service(service, callback); }
- (std::int32_t)bluetooth_le_server_clear_services { return ::bluetooth_le_server_clear_services(); }
- (std::int32_t)bluetooth_le_server_respond_read:(std::int32_t)request_id error_code:(std::int32_t)error_code data:(gm::wire::GMBuffer)data offset:(std::uint32_t)offset size:(std::uint32_t)size { return ::bluetooth_le_server_respond_read(request_id, error_code, data, offset, size); }
- (std::int32_t)bluetooth_le_server_respond_write:(std::int32_t)request_id error_code:(std::int32_t)error_code { return ::bluetooth_le_server_respond_write(request_id, error_code); }
- (std::int32_t)bluetooth_le_server_write_request_get_value:(std::int32_t)request_id out_data:(gm::wire::GMBuffer)out_data offset:(std::uint32_t)offset max_size:(std::uint32_t)max_size { return ::bluetooth_le_server_write_request_get_value(request_id, out_data, offset, max_size); }
- (std::int32_t)bluetooth_le_server_notify_value:(std::string_view)service_uuid characteristic_uuid:(std::string_view)characteristic_uuid connection:(std::uint64_t)connection data:(gm::wire::GMBuffer)data offset:(std::uint32_t)offset size:(std::uint32_t)size { return ::bluetooth_le_server_notify_value(service_uuid, characteristic_uuid, connection, data, offset, size); }

- (bool)bluetooth_set_callback_state_changed:(gm::wire::GMFunction)callback { return ::bluetooth_set_callback_state_changed(callback); }
- (bool)bluetooth_remove_callback_state_changed { return ::bluetooth_remove_callback_state_changed(); }
- (bool)bluetooth_set_callback_device_found:(gm::wire::GMFunction)callback { return ::bluetooth_set_callback_device_found(callback); }
- (bool)bluetooth_remove_callback_device_found { return ::bluetooth_remove_callback_device_found(); }
- (bool)bluetooth_set_callback_scan_stopped:(gm::wire::GMFunction)callback { return ::bluetooth_set_callback_scan_stopped(callback); }
- (bool)bluetooth_remove_callback_scan_stopped { return ::bluetooth_remove_callback_scan_stopped(); }
- (bool)bluetooth_set_callback_classic_client_connected:(gm::wire::GMFunction)callback { return ::bluetooth_set_callback_classic_client_connected(callback); }
- (bool)bluetooth_remove_callback_classic_client_connected { return ::bluetooth_remove_callback_classic_client_connected(); }
- (bool)bluetooth_set_callback_classic_data:(gm::wire::GMFunction)callback { return ::bluetooth_set_callback_classic_data(callback); }
- (bool)bluetooth_remove_callback_classic_data { return ::bluetooth_remove_callback_classic_data(); }
- (bool)bluetooth_set_callback_classic_disconnected:(gm::wire::GMFunction)callback { return ::bluetooth_set_callback_classic_disconnected(callback); }
- (bool)bluetooth_remove_callback_classic_disconnected { return ::bluetooth_remove_callback_classic_disconnected(); }
- (bool)bluetooth_set_callback_le_disconnected:(gm::wire::GMFunction)callback { return ::bluetooth_set_callback_le_disconnected(callback); }
- (bool)bluetooth_remove_callback_le_disconnected { return ::bluetooth_remove_callback_le_disconnected(); }
- (bool)bluetooth_set_callback_le_characteristic_value_changed:(gm::wire::GMFunction)callback { return ::bluetooth_set_callback_le_characteristic_value_changed(callback); }
- (bool)bluetooth_remove_callback_le_characteristic_value_changed { return ::bluetooth_remove_callback_le_characteristic_value_changed(); }
- (bool)bluetooth_set_callback_le_server_connection_state_changed:(gm::wire::GMFunction)callback { return ::bluetooth_set_callback_le_server_connection_state_changed(callback); }
- (bool)bluetooth_remove_callback_le_server_connection_state_changed { return ::bluetooth_remove_callback_le_server_connection_state_changed(); }
- (bool)bluetooth_set_callback_le_server_read_request:(gm::wire::GMFunction)callback { return ::bluetooth_set_callback_le_server_read_request(callback); }
- (bool)bluetooth_remove_callback_le_server_read_request { return ::bluetooth_remove_callback_le_server_read_request(); }
- (bool)bluetooth_set_callback_le_server_write_request:(gm::wire::GMFunction)callback { return ::bluetooth_set_callback_le_server_write_request(callback); }
- (bool)bluetooth_remove_callback_le_server_write_request { return ::bluetooth_remove_callback_le_server_write_request(); }

@end
