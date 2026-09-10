// ##### extgen :: Auto-generated file do not edit!! #####

#import <objc/runtime.h>
#import "core/GMExtUtils.h"
#import "GMBluetoothInternal_ios.h"


extern "C" const char* extOptGetString(char* _ext, char* _opt);

// Adapter: matches const signature expected by the C++ API
static const char* ExtOptGetString(const char* ext, const char* opt)
{
    return extOptGetString(const_cast<char*>(ext), const_cast<char*>(opt));
}

extern "C" const char* extGetVersion(char* _ext);

// Adapter: matches const signature expected by the C++ API
static const char* ExtGetVersion(const char* ext)
{
    return extGetVersion(const_cast<char*>(ext));
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

@interface GMBluetoothInternal ()
{
    gm::runtime::DispatchQueue __dispatch_queue;
    std::queue<gm::wire::GMBuffer> __buffer_queue;
    id<GMBluetoothInterface> __impl;
}@end


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
    ri.ExtGetVersion = &ExtGetVersion;
    GMExtensionInitialise(&ri, sizeof(ri));
}

- (instancetype)init
{
    self = [super init];
    if (self)
    {
        __impl = (id<GMBluetoothInterface>)self;
    }
    return self;
}
- (double)__EXT_NATIVE__bluetooth_initialize
{
    bool __result = [__impl bluetooth_initialize];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_shutdown
{
    [__impl bluetooth_shutdown];

    return 0;
}

- (double)__EXT_NATIVE__bluetooth_update
{
    std::int32_t __result = [__impl bluetooth_update];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_is_initialized
{
    bool __result = [__impl bluetooth_is_initialized];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_last_error_code
{
    std::int32_t __result = [__impl bluetooth_last_error_code];

    return static_cast<double>(__result);
}

- (char*)__EXT_NATIVE__bluetooth_last_error_message
{
    static std::string __result;
    __result = [__impl bluetooth_last_error_message];

    return (char*)__result.c_str();
}

- (double)__EXT_NATIVE__bluetooth_le_is_supported
{
    bool __result = [__impl bluetooth_le_is_supported];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_classic_is_supported
{
    bool __result = [__impl bluetooth_classic_is_supported];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_classic_server_is_supported
{
    bool __result = [__impl bluetooth_classic_server_is_supported];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_permission_get_status
{
    std::int32_t __result = [__impl bluetooth_permission_get_status];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_permission_request
{
    std::int32_t __result = [__impl bluetooth_permission_request];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_le_scan_start:(double)active
{
    std::int32_t __result = [__impl bluetooth_le_scan_start:active];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_le_scan_stop
{
    std::int32_t __result = [__impl bluetooth_le_scan_stop];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_le_scan_is_running
{
    bool __result = [__impl bluetooth_le_scan_is_running];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_classic_scan_start
{
    std::int32_t __result = [__impl bluetooth_classic_scan_start];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_classic_scan_stop
{
    std::int32_t __result = [__impl bluetooth_classic_scan_stop];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_classic_scan_is_running
{
    bool __result = [__impl bluetooth_classic_scan_is_running];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_device_clear
{
    [__impl bluetooth_device_clear];

    return 0;
}

- (double)__EXT_NATIVE__bluetooth_device_get_count
{
    std::int32_t __result = [__impl bluetooth_device_get_count];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_device_get_at:(double)index arg1:(char*)__ret_buffer arg2:(double)__ret_buffer_length
{
    std::uint64_t __result = [__impl bluetooth_device_get_at:index];

    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: UInt64
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

- (double)__EXT_NATIVE__bluetooth_device_is_valid:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: device, type: UInt64
    std::uint64_t device = gm::wire::codec::readValue<std::uint64_t>(__br);

    bool __result = [__impl bluetooth_device_is_valid:device];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_device_get_transport:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: device, type: UInt64
    std::uint64_t device = gm::wire::codec::readValue<std::uint64_t>(__br);

    std::int32_t __result = [__impl bluetooth_device_get_transport:device];

    return static_cast<double>(__result);
}

- (char*)__EXT_NATIVE__bluetooth_device_get_id:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: device, type: UInt64
    std::uint64_t device = gm::wire::codec::readValue<std::uint64_t>(__br);

    static std::string __result;
    __result = [__impl bluetooth_device_get_id:device];

    return (char*)__result.c_str();
}

- (char*)__EXT_NATIVE__bluetooth_device_get_name:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: device, type: UInt64
    std::uint64_t device = gm::wire::codec::readValue<std::uint64_t>(__br);

    static std::string __result;
    __result = [__impl bluetooth_device_get_name:device];

    return (char*)__result.c_str();
}

- (double)__EXT_NATIVE__bluetooth_device_has_address:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: device, type: UInt64
    std::uint64_t device = gm::wire::codec::readValue<std::uint64_t>(__br);

    bool __result = [__impl bluetooth_device_has_address:device];

    return static_cast<double>(__result);
}

- (char*)__EXT_NATIVE__bluetooth_device_get_address:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: device, type: UInt64
    std::uint64_t device = gm::wire::codec::readValue<std::uint64_t>(__br);

    static std::string __result;
    __result = [__impl bluetooth_device_get_address:device];

    return (char*)__result.c_str();
}

- (double)__EXT_NATIVE__bluetooth_device_has_rssi:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: device, type: UInt64
    std::uint64_t device = gm::wire::codec::readValue<std::uint64_t>(__br);

    bool __result = [__impl bluetooth_device_has_rssi:device];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_device_get_rssi:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: device, type: UInt64
    std::uint64_t device = gm::wire::codec::readValue<std::uint64_t>(__br);

    std::int32_t __result = [__impl bluetooth_device_get_rssi:device];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_device_is_connectable:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: device, type: UInt64
    std::uint64_t device = gm::wire::codec::readValue<std::uint64_t>(__br);

    bool __result = [__impl bluetooth_device_is_connectable:device];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_classic_connect:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: device, type: UInt64
    std::uint64_t device = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: service_uuid, type: String
    std::string_view service_uuid = gm::wire::codec::readValue<std::string_view>(__br);

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    std::uint64_t __result = [__impl bluetooth_classic_connect:device service_uuid:service_uuid callback:callback];

    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: UInt64
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

- (double)__EXT_NATIVE__bluetooth_classic_disconnect:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: connection, type: UInt64
    std::uint64_t connection = gm::wire::codec::readValue<std::uint64_t>(__br);

    std::int32_t __result = [__impl bluetooth_classic_disconnect:connection];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_classic_connection_is_valid:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: connection, type: UInt64
    std::uint64_t connection = gm::wire::codec::readValue<std::uint64_t>(__br);

    bool __result = [__impl bluetooth_classic_connection_is_valid:connection];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_classic_connection_is_connected:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: connection, type: UInt64
    std::uint64_t connection = gm::wire::codec::readValue<std::uint64_t>(__br);

    bool __result = [__impl bluetooth_classic_connection_is_connected:connection];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_classic_connection_get_device:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: connection, type: UInt64
    std::uint64_t connection = gm::wire::codec::readValue<std::uint64_t>(__br);

    std::uint64_t __result = [__impl bluetooth_classic_connection_get_device:connection];

    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: UInt64
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

- (double)__EXT_NATIVE__bluetooth_classic_receive_available:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: connection, type: UInt64
    std::uint64_t connection = gm::wire::codec::readValue<std::uint64_t>(__br);

    std::int32_t __result = [__impl bluetooth_classic_receive_available:connection];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_classic_send:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: connection, type: UInt64
    std::uint64_t connection = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: data, type: Buffer
    gm::wire::GMBuffer data = __buffer_queue.front();
    __buffer_queue.pop();

    // field: offset, type: UInt32
    std::uint32_t offset = gm::wire::codec::readValue<std::uint32_t>(__br);

    // field: size, type: UInt32
    std::uint32_t size = gm::wire::codec::readValue<std::uint32_t>(__br);

    std::int32_t __result = [__impl bluetooth_classic_send:connection data:data offset:offset size:size];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_classic_receive:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: connection, type: UInt64
    std::uint64_t connection = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: out_data, type: Buffer
    gm::wire::GMBuffer out_data = __buffer_queue.front();
    __buffer_queue.pop();

    // field: offset, type: UInt32
    std::uint32_t offset = gm::wire::codec::readValue<std::uint32_t>(__br);

    // field: max_size, type: UInt32
    std::uint32_t max_size = gm::wire::codec::readValue<std::uint32_t>(__br);

    std::int32_t __result = [__impl bluetooth_classic_receive:connection out_data:out_data offset:offset max_size:max_size];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_classic_server_start:(char*)name arg1:(char*)service_uuid
{
    std::int32_t __result = [__impl bluetooth_classic_server_start:name service_uuid:service_uuid];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_classic_server_stop
{
    std::int32_t __result = [__impl bluetooth_classic_server_stop];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_classic_server_is_running
{
    bool __result = [__impl bluetooth_classic_server_is_running];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_set_callback_device_found:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    bool __result = [__impl bluetooth_set_callback_device_found:callback];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_remove_callback_device_found
{
    bool __result = [__impl bluetooth_remove_callback_device_found];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_set_callback_scan_stopped:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    bool __result = [__impl bluetooth_set_callback_scan_stopped:callback];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_remove_callback_scan_stopped
{
    bool __result = [__impl bluetooth_remove_callback_scan_stopped];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_set_callback_classic_client_connected:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    bool __result = [__impl bluetooth_set_callback_classic_client_connected:callback];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_remove_callback_classic_client_connected
{
    bool __result = [__impl bluetooth_remove_callback_classic_client_connected];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_set_callback_classic_data:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    bool __result = [__impl bluetooth_set_callback_classic_data:callback];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_remove_callback_classic_data
{
    bool __result = [__impl bluetooth_remove_callback_classic_data];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_set_callback_classic_disconnected:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    bool __result = [__impl bluetooth_set_callback_classic_disconnected:callback];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_remove_callback_classic_disconnected
{
    bool __result = [__impl bluetooth_remove_callback_classic_disconnected];

    return static_cast<double>(__result);
}

// Internal function used for fetching dispatched function calls to GML
- (double)__EXT_NATIVE__GMBluetooth_invocation_handler:(char*)__ret_buffer arg1:(double)__ret_buffer_length
{
    gm::byteio::BufferWriter __bw{ __ret_buffer, static_cast<size_t>(__ret_buffer_length) };
    return __dispatch_queue.fetch(__bw);
}

// Internal function used for queueing buffers to native code
- (double)__EXT_NATIVE__GMBluetooth_queue_buffer:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::wire::GMBuffer __buff{ __arg_buffer, static_cast<uint64_t>(__arg_buffer_length) };
    __buffer_queue.push(__buff);
    return 1.0;
}

@end

