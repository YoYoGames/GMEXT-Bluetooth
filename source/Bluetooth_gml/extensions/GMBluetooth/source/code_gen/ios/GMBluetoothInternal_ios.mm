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
- (double)__EXT_NATIVE__bluetooth_update
{
    return __EXT_NATIVE__bluetooth_update();
}
- (double)__EXT_NATIVE__bluetooth_is_initialized
{
    return __EXT_NATIVE__bluetooth_is_initialized();
}
- (double)__EXT_NATIVE__bluetooth_last_error_code
{
    return __EXT_NATIVE__bluetooth_last_error_code();
}
- (char*)__EXT_NATIVE__bluetooth_last_error_message
{
    return __EXT_NATIVE__bluetooth_last_error_message();
}
- (double)__EXT_NATIVE__bluetooth_le_is_supported
{
    return __EXT_NATIVE__bluetooth_le_is_supported();
}
- (double)__EXT_NATIVE__bluetooth_classic_is_supported
{
    return __EXT_NATIVE__bluetooth_classic_is_supported();
}
- (double)__EXT_NATIVE__bluetooth_le_scan_start:(double)active
{
    return __EXT_NATIVE__bluetooth_le_scan_start(active);
}
- (double)__EXT_NATIVE__bluetooth_le_scan_stop
{
    return __EXT_NATIVE__bluetooth_le_scan_stop();
}
- (double)__EXT_NATIVE__bluetooth_le_scan_is_running
{
    return __EXT_NATIVE__bluetooth_le_scan_is_running();
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
- (double)__EXT_NATIVE__bluetooth_device_get_transport:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    return __EXT_NATIVE__bluetooth_device_get_transport(__arg_buffer, __arg_buffer_length);
}
- (char*)__EXT_NATIVE__bluetooth_device_get_id:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    return __EXT_NATIVE__bluetooth_device_get_id(__arg_buffer, __arg_buffer_length);
}
- (char*)__EXT_NATIVE__bluetooth_device_get_name:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    return __EXT_NATIVE__bluetooth_device_get_name(__arg_buffer, __arg_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_device_has_address:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    return __EXT_NATIVE__bluetooth_device_has_address(__arg_buffer, __arg_buffer_length);
}
- (char*)__EXT_NATIVE__bluetooth_device_get_address:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    return __EXT_NATIVE__bluetooth_device_get_address(__arg_buffer, __arg_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_device_has_rssi:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    return __EXT_NATIVE__bluetooth_device_has_rssi(__arg_buffer, __arg_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_device_get_rssi:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    return __EXT_NATIVE__bluetooth_device_get_rssi(__arg_buffer, __arg_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_device_is_connectable:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    return __EXT_NATIVE__bluetooth_device_is_connectable(__arg_buffer, __arg_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_set_callback_device_found:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    return __EXT_NATIVE__bluetooth_set_callback_device_found(__arg_buffer, __arg_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_remove_callback_device_found
{
    return __EXT_NATIVE__bluetooth_remove_callback_device_found();
}
- (double)__EXT_NATIVE__bluetooth_set_callback_scan_stopped:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    return __EXT_NATIVE__bluetooth_set_callback_scan_stopped(__arg_buffer, __arg_buffer_length);
}
- (double)__EXT_NATIVE__bluetooth_remove_callback_scan_stopped
{
    return __EXT_NATIVE__bluetooth_remove_callback_scan_stopped();
}
- (double)__EXT_NATIVE__GMBluetooth_invocation_handler:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    return __EXT_NATIVE__GMBluetooth_invocation_handler(__arg_buffer, __arg_buffer_length);
}
@end

