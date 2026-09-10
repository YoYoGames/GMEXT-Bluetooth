#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Werror=redeclared-class-typedef"

#import "GMBluetooth_ios.h"
#include "../native/GMBluetooth_native.h"

#pragma clang diagnostic pop

@implementation GMBluetooth

- (bool)bluetooth_initialize { return ::bluetooth_initialize(); }
- (void)bluetooth_shutdown { ::bluetooth_shutdown(); }
- (std::int32_t)bluetooth_update { return ::bluetooth_update(); }
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
{
    return ::bluetooth_classic_connect(device, service_uuid, callback);
}

- (std::int32_t)bluetooth_classic_disconnect:(std::uint64_t)connection { return ::bluetooth_classic_disconnect(connection); }
- (bool)bluetooth_classic_connection_is_valid:(std::uint64_t)connection { return ::bluetooth_classic_connection_is_valid(connection); }
- (bool)bluetooth_classic_connection_is_connected:(std::uint64_t)connection { return ::bluetooth_classic_connection_is_connected(connection); }
- (std::uint64_t)bluetooth_classic_connection_get_device:(std::uint64_t)connection { return ::bluetooth_classic_connection_get_device(connection); }
- (std::int32_t)bluetooth_classic_receive_available:(std::uint64_t)connection { return ::bluetooth_classic_receive_available(connection); }

- (std::int32_t)bluetooth_classic_send:(std::uint64_t)connection
                                  data:(gm::wire::GMBuffer)data
                                offset:(std::uint32_t)offset
                                  size:(std::uint32_t)size
{
    // TODO is intentionally kept in the common native implementation. Do not
    // infer GameMaker buffer-copy semantics here.
    return ::bluetooth_classic_send(connection, data, offset, size);
}

- (std::int32_t)bluetooth_classic_receive:(std::uint64_t)connection
                                 out_data:(gm::wire::GMBuffer)out_data
                                   offset:(std::uint32_t)offset
                                 max_size:(std::uint32_t)max_size
{
    // TODO is intentionally kept in the common native implementation. Do not
    // infer GameMaker buffer-copy semantics here.
    return ::bluetooth_classic_receive(connection, out_data, offset, max_size);
}

- (std::int32_t)bluetooth_classic_server_start:(std::string_view)name
                                  service_uuid:(std::string_view)service_uuid
{
    return ::bluetooth_classic_server_start(name, service_uuid);
}
- (std::int32_t)bluetooth_classic_server_stop { return ::bluetooth_classic_server_stop(); }
- (bool)bluetooth_classic_server_is_running { return ::bluetooth_classic_server_is_running(); }

- (std::uint64_t)bluetooth_le_connect:(std::uint64_t)device { return ::bluetooth_le_connect(device); }
- (std::int32_t)bluetooth_le_disconnect:(std::uint64_t)connection { return ::bluetooth_le_disconnect(connection); }
- (bool)bluetooth_le_connection_is_valid:(std::uint64_t)connection { return ::bluetooth_le_connection_is_valid(connection); }
- (bool)bluetooth_le_connection_is_connected:(std::uint64_t)connection { return ::bluetooth_le_connection_is_connected(connection); }
- (std::uint64_t)bluetooth_le_connection_get_device:(std::uint64_t)connection { return ::bluetooth_le_connection_get_device(connection); }

- (std::int32_t)bluetooth_le_services_discover:(std::uint64_t)connection { return ::bluetooth_le_services_discover(connection); }
- (std::int32_t)bluetooth_le_characteristics_discover:(std::uint64_t)connection
                                         service_uuid:(std::string_view)service_uuid
{
    return ::bluetooth_le_characteristics_discover(connection, service_uuid);
}
- (std::int32_t)bluetooth_le_descriptors_discover:(std::uint64_t)connection
                                     service_uuid:(std::string_view)service_uuid
                              characteristic_uuid:(std::string_view)characteristic_uuid
{
    return ::bluetooth_le_descriptors_discover(connection, service_uuid, characteristic_uuid);
}
- (std::int32_t)bluetooth_le_characteristic_read:(std::uint64_t)connection
                                    service_uuid:(std::string_view)service_uuid
                             characteristic_uuid:(std::string_view)characteristic_uuid
{
    return ::bluetooth_le_characteristic_read(connection, service_uuid, characteristic_uuid);
}
- (std::int32_t)bluetooth_le_characteristic_write_request:(std::uint64_t)connection
                                             service_uuid:(std::string_view)service_uuid
                                      characteristic_uuid:(std::string_view)characteristic_uuid
                                             value_base64:(std::string_view)value_base64
{
    return ::bluetooth_le_characteristic_write_request(connection, service_uuid, characteristic_uuid, value_base64);
}
- (std::int32_t)bluetooth_le_characteristic_write_command:(std::uint64_t)connection
                                             service_uuid:(std::string_view)service_uuid
                                      characteristic_uuid:(std::string_view)characteristic_uuid
                                             value_base64:(std::string_view)value_base64
{
    return ::bluetooth_le_characteristic_write_command(connection, service_uuid, characteristic_uuid, value_base64);
}
- (std::int32_t)bluetooth_le_characteristic_notify:(std::uint64_t)connection
                                      service_uuid:(std::string_view)service_uuid
                               characteristic_uuid:(std::string_view)characteristic_uuid
{
    return ::bluetooth_le_characteristic_notify(connection, service_uuid, characteristic_uuid);
}
- (std::int32_t)bluetooth_le_characteristic_indicate:(std::uint64_t)connection
                                        service_uuid:(std::string_view)service_uuid
                                 characteristic_uuid:(std::string_view)characteristic_uuid
{
    return ::bluetooth_le_characteristic_indicate(connection, service_uuid, characteristic_uuid);
}
- (std::int32_t)bluetooth_le_characteristic_unsubscribe:(std::uint64_t)connection
                                           service_uuid:(std::string_view)service_uuid
                                    characteristic_uuid:(std::string_view)characteristic_uuid
{
    return ::bluetooth_le_characteristic_unsubscribe(connection, service_uuid, characteristic_uuid);
}
- (std::int32_t)bluetooth_le_descriptor_read:(std::uint64_t)connection
                                service_uuid:(std::string_view)service_uuid
                         characteristic_uuid:(std::string_view)characteristic_uuid
                             descriptor_uuid:(std::string_view)descriptor_uuid
{
    return ::bluetooth_le_descriptor_read(connection, service_uuid, characteristic_uuid, descriptor_uuid);
}
- (std::int32_t)bluetooth_le_descriptor_write:(std::uint64_t)connection
                                 service_uuid:(std::string_view)service_uuid
                          characteristic_uuid:(std::string_view)characteristic_uuid
                              descriptor_uuid:(std::string_view)descriptor_uuid
                                 value_base64:(std::string_view)value_base64
{
    return ::bluetooth_le_descriptor_write(connection, service_uuid, characteristic_uuid, descriptor_uuid, value_base64);
}

- (std::int32_t)bluetooth_le_advertise_start:(std::string_view)settings_json
                                    data_json:(std::string_view)data_json
{
    return ::bluetooth_le_advertise_start(settings_json, data_json);
}
- (std::int32_t)bluetooth_le_advertise_stop { return ::bluetooth_le_advertise_stop(); }
- (bool)bluetooth_le_advertise_is_running { return ::bluetooth_le_advertise_is_running(); }

- (std::int32_t)bluetooth_le_server_start { return ::bluetooth_le_server_start(); }
- (std::int32_t)bluetooth_le_server_stop { return ::bluetooth_le_server_stop(); }
- (bool)bluetooth_le_server_is_running { return ::bluetooth_le_server_is_running(); }
- (std::int32_t)bluetooth_le_server_add_service:(std::string_view)service_json { return ::bluetooth_le_server_add_service(service_json); }
- (std::int32_t)bluetooth_le_server_clear_services { return ::bluetooth_le_server_clear_services(); }
- (std::int32_t)bluetooth_le_server_respond_read:(std::int32_t)request_id
                                          status:(std::int32_t)status
                                    value_base64:(std::string_view)value_base64
{
    return ::bluetooth_le_server_respond_read(request_id, status, value_base64);
}
- (std::int32_t)bluetooth_le_server_respond_write:(std::int32_t)request_id
                                           status:(std::int32_t)status
{
    return ::bluetooth_le_server_respond_write(request_id, status);
}
- (std::int32_t)bluetooth_le_server_notify_value:(std::string_view)service_uuid
                             characteristic_uuid:(std::string_view)characteristic_uuid
                                    value_base64:(std::string_view)value_base64
{
    return ::bluetooth_le_server_notify_value(service_uuid, characteristic_uuid, value_base64);
}

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
- (bool)bluetooth_set_callback_le_event:(gm::wire::GMFunction)callback { return ::bluetooth_set_callback_le_event(callback); }
- (bool)bluetooth_remove_callback_le_event { return ::bluetooth_remove_callback_le_event(); }

@end
