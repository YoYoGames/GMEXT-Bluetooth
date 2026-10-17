// ##### extgen :: Auto-generated file do not edit!! #####

#import "GMBluetoothInternal_ios.h"
#import "native/GMBluetoothInternal_exports.h"
#import <objc/runtime.h>


extern "C" const char* extOptGetString(char* _ext, char* _opt);

// Adapter: matches const signature expected by the C++ API
static const char* ExtOptGetString(const char* ext, const char* opt)
{
    return extOptGetString(const_cast<char*>(ext), const_cast<char*>(opt));
}

static BOOL GMIsSubclassOf(Class cls, Class base)
{
    for (Class c = cls; c != Nil; c = class_getSuperclass(c)) {
        if (c == base) return YES;
    }
    return NO;
}

static void GMInjectSelectorsIntoSubclass(Class subclass, Class base)
{
    // Build set of methods already defined on subclass
    unsigned subCount = 0;
    Method *subList = class_copyMethodList(subclass, &subCount);

    CFMutableSetRef owned = CFSetCreateMutable(kCFAllocatorDefault, 0, NULL);
    for (unsigned i = 0; i < subCount; ++i) {
        CFSetAddValue(owned, method_getName(subList[i]));
    }

    // Walk base class methods
    unsigned baseCount = 0;
    Method *baseList = class_copyMethodList(base, &baseCount);

    for (unsigned i = 0; i < baseCount; ++i) {
        SEL sel = method_getName(baseList[i]);
        const char *name = sel_getName(sel);

        // Only inject extension selectors (methods prefixed with __EXT_NATIVE__)
        if (!name || strncmp(name, "__EXT_NATIVE__", 13) != 0) continue;

        // Add only if subclass doesn't already have it
        if (!CFSetContainsValue(owned, sel)) {
            IMP imp = method_getImplementation(baseList[i]);
            const char *types = method_getTypeEncoding(baseList[i]);
            if (class_addMethod(subclass, sel, imp, types)) {
                CFSetAddValue(owned, sel);
            }
        }
    }

    if (subList) free(subList);
    if (baseList) free(baseList);
    if (owned) CFRelease(owned);
}

@implementation GMBluetoothInternal

+ (void)load
{
    // Find all loaded classes
    int num = objc_getClassList(NULL, 0);
    if (num <= 0) return;

    Class *classes = (Class *)malloc(sizeof(Class) * (unsigned)num);
    num = objc_getClassList(classes, num);

    Class base = [GMBluetoothInternal class];

    for (int i = 0; i < num; ++i) {
        Class cls = classes[i];
        if (cls == base) continue;

        // We only care about direct or indirect subclasses
        if (GMIsSubclassOf(cls, base)) {
            GMInjectSelectorsIntoSubclass(cls, base);
        }
    }

    free(classes);

    gm::details::GMRTRunnerInterface ri{};
    ri.ExtOptGetString = &ExtOptGetString;
    GMExtensionInitialise(&ri, sizeof(ri));
}

- (double)__EXT_NATIVE__bluetooth_initialize
{
    return __EXT_NATIVE__bluetooth_initialize();
}
- (double)__EXT_NATIVE__bluetooth_shutdown
{
    return __EXT_NATIVE__bluetooth_shutdown();
}
- (double)__EXT_NATIVE__bluetooth_is_initialized
{
    return __EXT_NATIVE__bluetooth_is_initialized();
}
- (double)__EXT_NATIVE__bluetooth_last_error_code:(char*)__ret_buffer arg1:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_last_error_code(__ret_buffer, __ret_buffer_length);
}
- (char*)__EXT_NATIVE__bluetooth_last_error_message
{
    return __EXT_NATIVE__bluetooth_last_error_message();
}
- (double)__EXT_NATIVE__bluetooth_feature_is_supported:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    return __EXT_NATIVE__bluetooth_feature_is_supported(__arg_buffer, __arg_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_permission_get_status:(char*)__ret_buffer arg1:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_permission_get_status(__ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_permission_request:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_permission_request(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_request_enable:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_request_enable(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_le_scan_start:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_scan_start(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_le_scan_stop:(char*)__ret_buffer arg1:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_scan_stop(__ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_le_scan_is_running
{
    return __EXT_NATIVE__bluetooth_le_scan_is_running();
}
- (double)__EXT_NATIVE__bluetooth_classic_scan_start:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_classic_scan_start(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_classic_scan_stop:(char*)__ret_buffer arg1:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_classic_scan_stop(__ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_classic_scan_is_running
{
    return __EXT_NATIVE__bluetooth_classic_scan_is_running();
}
- (double)__EXT_NATIVE__bluetooth_device_clear
{
    return __EXT_NATIVE__bluetooth_device_clear();
}
- (double)__EXT_NATIVE__bluetooth_device_get_count
{
    return __EXT_NATIVE__bluetooth_device_get_count();
}
- (double)__EXT_NATIVE__bluetooth_device_get_at:(double)index arg1:(char*)__ret_buffer arg2:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_device_get_at(index, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_device_is_valid:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    return __EXT_NATIVE__bluetooth_device_is_valid(__arg_buffer, __arg_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_device_get_transport:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_device_get_transport(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (char*)__EXT_NATIVE__bluetooth_device_get_id:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    return __EXT_NATIVE__bluetooth_device_get_id(__arg_buffer, __arg_buffer_length);
}
- (char*)__EXT_NATIVE__bluetooth_device_get_name:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    return __EXT_NATIVE__bluetooth_device_get_name(__arg_buffer, __arg_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_device_get_address:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_device_get_address(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_device_get_rssi:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_device_get_rssi(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_device_get_last_seen_age:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_device_get_last_seen_age(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_device_is_connectable:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    return __EXT_NATIVE__bluetooth_device_is_connectable(__arg_buffer, __arg_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_device_get_advertisement:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_device_get_advertisement(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_device_from_id:(char*)id arg1:(char*)__ret_buffer arg2:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_device_from_id(id, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_le_connected_devices_query:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_connected_devices_query(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_paired_devices_query:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_paired_devices_query(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_classic_connect:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_classic_connect(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_classic_disconnect:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_classic_disconnect(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_classic_connection_is_valid:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    return __EXT_NATIVE__bluetooth_classic_connection_is_valid(__arg_buffer, __arg_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_classic_connection_is_connected:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    return __EXT_NATIVE__bluetooth_classic_connection_is_connected(__arg_buffer, __arg_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_classic_connection_get_device:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_classic_connection_get_device(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_classic_send:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_classic_send(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_classic_server_start:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_classic_server_start(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_classic_server_stop:(char*)__ret_buffer arg1:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_classic_server_stop(__ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_classic_server_is_running
{
    return __EXT_NATIVE__bluetooth_classic_server_is_running();
}
- (double)__EXT_NATIVE__bluetooth_classic_discoverable_start:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_classic_discoverable_start(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_classic_discoverable_stop:(char*)__ret_buffer arg1:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_classic_discoverable_stop(__ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_classic_discoverable_is_running
{
    return __EXT_NATIVE__bluetooth_classic_discoverable_is_running();
}
- (double)__EXT_NATIVE__bluetooth_pairing_is_supported:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    return __EXT_NATIVE__bluetooth_pairing_is_supported(__arg_buffer, __arg_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_pair:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_pair(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_device_is_paired:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    return __EXT_NATIVE__bluetooth_device_is_paired(__arg_buffer, __arg_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_le_connect:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_connect(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_le_disconnect:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_disconnect(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_le_connection_is_valid:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_connection_is_valid(__arg_buffer, __arg_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_le_connection_is_connected:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_connection_is_connected(__arg_buffer, __arg_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_le_connection_get_device:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_connection_get_device(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_le_connection_get_mtu:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_connection_get_mtu(__arg_buffer, __arg_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_le_connection_request_mtu:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_connection_request_mtu(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_le_connection_read_rssi:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_connection_read_rssi(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_le_connection_request_priority:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_connection_request_priority(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_le_services_discover:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_services_discover(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_le_service_get_count:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_service_get_count(__arg_buffer, __arg_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_le_service_get_at:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_service_get_at(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (char*)__EXT_NATIVE__bluetooth_le_service_get_uuid:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_service_get_uuid(__arg_buffer, __arg_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_le_service_find:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_service_find(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_le_characteristics_discover:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_characteristics_discover(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_le_characteristic_get_count:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_characteristic_get_count(__arg_buffer, __arg_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_le_characteristic_get_at:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_characteristic_get_at(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (char*)__EXT_NATIVE__bluetooth_le_characteristic_get_uuid:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_characteristic_get_uuid(__arg_buffer, __arg_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_le_characteristic_find:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_characteristic_find(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_le_characteristic_get_properties:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_characteristic_get_properties(__arg_buffer, __arg_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_le_descriptors_discover:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_descriptors_discover(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_le_descriptor_get_count:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_descriptor_get_count(__arg_buffer, __arg_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_le_descriptor_get_at:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_descriptor_get_at(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (char*)__EXT_NATIVE__bluetooth_le_descriptor_get_uuid:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_descriptor_get_uuid(__arg_buffer, __arg_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_le_descriptor_find:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_descriptor_find(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_le_characteristic_read:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_characteristic_read(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_le_characteristic_write:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_characteristic_write(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_le_characteristic_subscribe:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_characteristic_subscribe(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_le_characteristic_unsubscribe:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_characteristic_unsubscribe(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_le_descriptor_read:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_descriptor_read(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_le_descriptor_write:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_descriptor_write(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_value_fetch:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_value_fetch(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_value_release:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_value_release(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_le_advertise_start:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_advertise_start(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_le_advertise_stop:(char*)__ret_buffer arg1:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_advertise_stop(__ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_le_advertise_is_running
{
    return __EXT_NATIVE__bluetooth_le_advertise_is_running();
}
- (double)__EXT_NATIVE__bluetooth_le_server_start:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_server_start(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_le_server_stop:(char*)__ret_buffer arg1:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_server_stop(__ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_le_server_is_running
{
    return __EXT_NATIVE__bluetooth_le_server_is_running();
}
- (double)__EXT_NATIVE__bluetooth_le_server_add_service:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_server_add_service(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_le_server_clear_services:(char*)__ret_buffer arg1:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_server_clear_services(__ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_le_server_respond_read:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_server_respond_read(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_le_server_respond_read_error:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_server_respond_read_error(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_le_server_respond_write:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_server_respond_write(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_le_server_notify_value:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    return __EXT_NATIVE__bluetooth_le_server_notify_value(__arg_buffer, __arg_buffer_length, __ret_buffer, __ret_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_set_callback_state_changed:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    return __EXT_NATIVE__bluetooth_set_callback_state_changed(__arg_buffer, __arg_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_remove_callback_state_changed
{
    return __EXT_NATIVE__bluetooth_remove_callback_state_changed();
}
- (double)__EXT_NATIVE__GMBluetooth_invocation_handler:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    return __EXT_NATIVE__GMBluetooth_invocation_handler(__arg_buffer, __arg_buffer_length);
}
- (double)__EXT_NATIVE__GMBluetooth_queue_buffer:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    return __EXT_NATIVE__GMBluetooth_queue_buffer(__arg_buffer, __arg_buffer_length);
}
@end

