// ##### extgen :: Auto-generated file do not edit!! #####

package ${YYAndroidPackageName};
import ${YYAndroidPackageName}.GMExtWire.GMFunction;
import ${YYAndroidPackageName}.GMExtWire.GMValue;
import ${YYAndroidPackageName}.enums.*;

public interface GMBluetoothInterface {
    public boolean bluetooth_initialize();
    public void bluetooth_shutdown();
    public int bluetooth_update();
    public boolean bluetooth_is_initialized();
    public int bluetooth_last_error_code();
    public String bluetooth_last_error_message();
    public boolean bluetooth_le_is_supported();
    public boolean bluetooth_classic_is_supported();
    public boolean bluetooth_classic_server_is_supported();
    public int bluetooth_permission_get_status();
    public int bluetooth_permission_request();
    public int bluetooth_le_scan_start(boolean active);
    public int bluetooth_le_scan_stop();
    public boolean bluetooth_le_scan_is_running();
    public int bluetooth_classic_scan_start();
    public int bluetooth_classic_scan_stop();
    public boolean bluetooth_classic_scan_is_running();
    public void bluetooth_device_clear();
    public int bluetooth_device_get_count();
    public long bluetooth_device_get_at(int index);
    public boolean bluetooth_device_is_valid(long device);
    public int bluetooth_device_get_transport(long device);
    public String bluetooth_device_get_id(long device);
    public String bluetooth_device_get_name(long device);
    public boolean bluetooth_device_has_address(long device);
    public String bluetooth_device_get_address(long device);
    public boolean bluetooth_device_has_rssi(long device);
    public int bluetooth_device_get_rssi(long device);
    public boolean bluetooth_device_is_connectable(long device);
    public long bluetooth_classic_connect(long device, String service_uuid, GMFunction callback);
    public int bluetooth_classic_disconnect(long connection);
    public boolean bluetooth_classic_connection_is_valid(long connection);
    public boolean bluetooth_classic_connection_is_connected(long connection);
    public long bluetooth_classic_connection_get_device(long connection);
    public int bluetooth_classic_receive_available(long connection);
    public int bluetooth_classic_send(long connection, java.nio.ByteBuffer data, int offset, int size);
    public int bluetooth_classic_receive(long connection, java.nio.ByteBuffer out_data, int offset, int max_size);
    public int bluetooth_classic_server_start(String name, String service_uuid);
    public int bluetooth_classic_server_stop();
    public boolean bluetooth_classic_server_is_running();
    public boolean bluetooth_set_callback_device_found(GMFunction callback);
    public boolean bluetooth_remove_callback_device_found();
    public boolean bluetooth_set_callback_scan_stopped(GMFunction callback);
    public boolean bluetooth_remove_callback_scan_stopped();
    public boolean bluetooth_set_callback_classic_client_connected(GMFunction callback);
    public boolean bluetooth_remove_callback_classic_client_connected();
    public boolean bluetooth_set_callback_classic_data(GMFunction callback);
    public boolean bluetooth_remove_callback_classic_data();
    public boolean bluetooth_set_callback_classic_disconnected(GMFunction callback);
    public boolean bluetooth_remove_callback_classic_disconnected();
}