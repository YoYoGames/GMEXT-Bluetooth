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

- (double)__EXT_NATIVE__bluetooth_le_advertise_is_supported
{
    bool __result = [__impl bluetooth_le_advertise_is_supported];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_le_server_is_supported
{
    bool __result = [__impl bluetooth_le_server_is_supported];

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

- (double)__EXT_NATIVE__bluetooth_classic_discoverable_start:(double)duration_seconds
{
    std::int32_t __result = [__impl bluetooth_classic_discoverable_start:duration_seconds];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_classic_discoverable_stop
{
    std::int32_t __result = [__impl bluetooth_classic_discoverable_stop];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_classic_discoverable_is_running
{
    bool __result = [__impl bluetooth_classic_discoverable_is_running];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_pairing_is_supported:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: device, type: UInt64
    std::uint64_t device = gm::wire::codec::readValue<std::uint64_t>(__br);

    bool __result = [__impl bluetooth_pairing_is_supported:device];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_pair:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: device, type: UInt64
    std::uint64_t device = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    std::int32_t __result = [__impl bluetooth_pair:device callback:callback];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_device_is_paired:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: device, type: UInt64
    std::uint64_t device = gm::wire::codec::readValue<std::uint64_t>(__br);

    bool __result = [__impl bluetooth_device_is_paired:device];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_le_connect:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: device, type: UInt64
    std::uint64_t device = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    std::uint64_t __result = [__impl bluetooth_le_connect:device callback:callback];

    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: UInt64
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

- (double)__EXT_NATIVE__bluetooth_le_disconnect:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: connection, type: UInt64
    std::uint64_t connection = gm::wire::codec::readValue<std::uint64_t>(__br);

    std::int32_t __result = [__impl bluetooth_le_disconnect:connection];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_le_connection_is_valid:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: connection, type: UInt64
    std::uint64_t connection = gm::wire::codec::readValue<std::uint64_t>(__br);

    bool __result = [__impl bluetooth_le_connection_is_valid:connection];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_le_connection_is_connected:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: connection, type: UInt64
    std::uint64_t connection = gm::wire::codec::readValue<std::uint64_t>(__br);

    bool __result = [__impl bluetooth_le_connection_is_connected:connection];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_le_connection_get_device:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: connection, type: UInt64
    std::uint64_t connection = gm::wire::codec::readValue<std::uint64_t>(__br);

    std::uint64_t __result = [__impl bluetooth_le_connection_get_device:connection];

    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: UInt64
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

- (double)__EXT_NATIVE__bluetooth_le_services_discover:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: connection, type: UInt64
    std::uint64_t connection = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    std::int32_t __result = [__impl bluetooth_le_services_discover:connection callback:callback];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_le_service_get_count:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: connection, type: UInt64
    std::uint64_t connection = gm::wire::codec::readValue<std::uint64_t>(__br);

    std::int32_t __result = [__impl bluetooth_le_service_get_count:connection];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_le_service_get_at:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: connection, type: UInt64
    std::uint64_t connection = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: index, type: Int32
    std::int32_t index = gm::wire::codec::readValue<std::int32_t>(__br);

    std::uint64_t __result = [__impl bluetooth_le_service_get_at:connection index:index];

    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: UInt64
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

- (char*)__EXT_NATIVE__bluetooth_le_service_get_uuid:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: service, type: UInt64
    std::uint64_t service = gm::wire::codec::readValue<std::uint64_t>(__br);

    static std::string __result;
    __result = [__impl bluetooth_le_service_get_uuid:service];

    return (char*)__result.c_str();
}

- (double)__EXT_NATIVE__bluetooth_le_characteristics_discover:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: service, type: UInt64
    std::uint64_t service = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    std::int32_t __result = [__impl bluetooth_le_characteristics_discover:service callback:callback];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_le_characteristic_get_count:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: service, type: UInt64
    std::uint64_t service = gm::wire::codec::readValue<std::uint64_t>(__br);

    std::int32_t __result = [__impl bluetooth_le_characteristic_get_count:service];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_le_characteristic_get_at:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: service, type: UInt64
    std::uint64_t service = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: index, type: Int32
    std::int32_t index = gm::wire::codec::readValue<std::int32_t>(__br);

    std::uint64_t __result = [__impl bluetooth_le_characteristic_get_at:service index:index];

    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: UInt64
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

- (char*)__EXT_NATIVE__bluetooth_le_characteristic_get_uuid:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: characteristic, type: UInt64
    std::uint64_t characteristic = gm::wire::codec::readValue<std::uint64_t>(__br);

    static std::string __result;
    __result = [__impl bluetooth_le_characteristic_get_uuid:characteristic];

    return (char*)__result.c_str();
}

- (double)__EXT_NATIVE__bluetooth_le_characteristic_get_properties:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: characteristic, type: UInt64
    std::uint64_t characteristic = gm::wire::codec::readValue<std::uint64_t>(__br);

    std::int32_t __result = [__impl bluetooth_le_characteristic_get_properties:characteristic];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_le_descriptors_discover:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: characteristic, type: UInt64
    std::uint64_t characteristic = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    std::int32_t __result = [__impl bluetooth_le_descriptors_discover:characteristic callback:callback];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_le_descriptor_get_count:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: characteristic, type: UInt64
    std::uint64_t characteristic = gm::wire::codec::readValue<std::uint64_t>(__br);

    std::int32_t __result = [__impl bluetooth_le_descriptor_get_count:characteristic];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_le_descriptor_get_at:(char*)__arg_buffer arg1:(double)__arg_buffer_length arg2:(char*)__ret_buffer arg3:(double)__ret_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: characteristic, type: UInt64
    std::uint64_t characteristic = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: index, type: Int32
    std::int32_t index = gm::wire::codec::readValue<std::int32_t>(__br);

    std::uint64_t __result = [__impl bluetooth_le_descriptor_get_at:characteristic index:index];

    gm::byteio::BufferWriter __bw{__ret_buffer, static_cast<size_t>(__ret_buffer_length)};

    // return: __result, type: UInt64
    gm::wire::codec::writeValue(__bw, __result);
    return 0;
}

- (char*)__EXT_NATIVE__bluetooth_le_descriptor_get_uuid:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: descriptor, type: UInt64
    std::uint64_t descriptor = gm::wire::codec::readValue<std::uint64_t>(__br);

    static std::string __result;
    __result = [__impl bluetooth_le_descriptor_get_uuid:descriptor];

    return (char*)__result.c_str();
}

- (double)__EXT_NATIVE__bluetooth_le_characteristic_read:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: characteristic, type: UInt64
    std::uint64_t characteristic = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    std::int32_t __result = [__impl bluetooth_le_characteristic_read:characteristic callback:callback];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_le_characteristic_get_value:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: characteristic, type: UInt64
    std::uint64_t characteristic = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: out_data, type: Buffer
    gm::wire::GMBuffer out_data = __buffer_queue.front();
    __buffer_queue.pop();

    // field: offset, type: UInt32
    std::uint32_t offset = gm::wire::codec::readValue<std::uint32_t>(__br);

    // field: max_size, type: UInt32
    std::uint32_t max_size = gm::wire::codec::readValue<std::uint32_t>(__br);

    std::int32_t __result = [__impl bluetooth_le_characteristic_get_value:characteristic out_data:out_data offset:offset max_size:max_size];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_le_characteristic_write:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: characteristic, type: UInt64
    std::uint64_t characteristic = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: data, type: Buffer
    gm::wire::GMBuffer data = __buffer_queue.front();
    __buffer_queue.pop();

    // field: offset, type: UInt32
    std::uint32_t offset = gm::wire::codec::readValue<std::uint32_t>(__br);

    // field: size, type: UInt32
    std::uint32_t size = gm::wire::codec::readValue<std::uint32_t>(__br);

    // field: write_type, type: Int32
    std::int32_t write_type = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    std::int32_t __result = [__impl bluetooth_le_characteristic_write:characteristic data:data offset:offset size:size write_type:write_type callback:callback];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_le_characteristic_subscribe:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: characteristic, type: UInt64
    std::uint64_t characteristic = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: mode, type: Int32
    std::int32_t mode = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    std::int32_t __result = [__impl bluetooth_le_characteristic_subscribe:characteristic mode:mode callback:callback];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_le_descriptor_read:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: descriptor, type: UInt64
    std::uint64_t descriptor = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    std::int32_t __result = [__impl bluetooth_le_descriptor_read:descriptor callback:callback];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_le_descriptor_get_value:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: descriptor, type: UInt64
    std::uint64_t descriptor = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: out_data, type: Buffer
    gm::wire::GMBuffer out_data = __buffer_queue.front();
    __buffer_queue.pop();

    // field: offset, type: UInt32
    std::uint32_t offset = gm::wire::codec::readValue<std::uint32_t>(__br);

    // field: max_size, type: UInt32
    std::uint32_t max_size = gm::wire::codec::readValue<std::uint32_t>(__br);

    std::int32_t __result = [__impl bluetooth_le_descriptor_get_value:descriptor out_data:out_data offset:offset max_size:max_size];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_le_descriptor_write:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: descriptor, type: UInt64
    std::uint64_t descriptor = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: data, type: Buffer
    gm::wire::GMBuffer data = __buffer_queue.front();
    __buffer_queue.pop();

    // field: offset, type: UInt32
    std::uint32_t offset = gm::wire::codec::readValue<std::uint32_t>(__br);

    // field: size, type: UInt32
    std::uint32_t size = gm::wire::codec::readValue<std::uint32_t>(__br);

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    std::int32_t __result = [__impl bluetooth_le_descriptor_write:descriptor data:data offset:offset size:size callback:callback];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_le_advertise_start:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: settings_json, type: String
    std::string_view settings_json = gm::wire::codec::readValue<std::string_view>(__br);

    // field: data_json, type: String
    std::string_view data_json = gm::wire::codec::readValue<std::string_view>(__br);

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    std::int32_t __result = [__impl bluetooth_le_advertise_start:settings_json data_json:data_json callback:callback];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_le_advertise_stop
{
    std::int32_t __result = [__impl bluetooth_le_advertise_stop];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_le_advertise_is_running
{
    bool __result = [__impl bluetooth_le_advertise_is_running];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_le_server_start
{
    std::int32_t __result = [__impl bluetooth_le_server_start];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_le_server_stop
{
    std::int32_t __result = [__impl bluetooth_le_server_stop];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_le_server_is_running
{
    bool __result = [__impl bluetooth_le_server_is_running];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_le_server_add_service:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: service, type: struct BluetoothLeServiceDefinition
    gm_structs::BluetoothLeServiceDefinition service = gm::wire::codec::readValue<gm_structs::BluetoothLeServiceDefinition>(__br);

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    std::int32_t __result = [__impl bluetooth_le_server_add_service:service callback:callback];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_le_server_clear_services
{
    std::int32_t __result = [__impl bluetooth_le_server_clear_services];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_le_server_respond_read:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: request_id, type: Int32
    std::int32_t request_id = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: error_code, type: Int32
    std::int32_t error_code = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: data, type: Buffer
    gm::wire::GMBuffer data = __buffer_queue.front();
    __buffer_queue.pop();

    // field: offset, type: UInt32
    std::uint32_t offset = gm::wire::codec::readValue<std::uint32_t>(__br);

    // field: size, type: UInt32
    std::uint32_t size = gm::wire::codec::readValue<std::uint32_t>(__br);

    std::int32_t __result = [__impl bluetooth_le_server_respond_read:request_id error_code:error_code data:data offset:offset size:size];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_le_server_respond_write:(double)request_id arg1:(double)error_code
{
    std::int32_t __result = [__impl bluetooth_le_server_respond_write:request_id error_code:static_cast<std::int32_t>(error_code)];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_le_server_write_request_get_value:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: request_id, type: Int32
    std::int32_t request_id = gm::wire::codec::readValue<std::int32_t>(__br);

    // field: out_data, type: Buffer
    gm::wire::GMBuffer out_data = __buffer_queue.front();
    __buffer_queue.pop();

    // field: offset, type: UInt32
    std::uint32_t offset = gm::wire::codec::readValue<std::uint32_t>(__br);

    // field: max_size, type: UInt32
    std::uint32_t max_size = gm::wire::codec::readValue<std::uint32_t>(__br);

    std::int32_t __result = [__impl bluetooth_le_server_write_request_get_value:request_id out_data:out_data offset:offset max_size:max_size];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_le_server_notify_value:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: service_uuid, type: String
    std::string_view service_uuid = gm::wire::codec::readValue<std::string_view>(__br);

    // field: characteristic_uuid, type: String
    std::string_view characteristic_uuid = gm::wire::codec::readValue<std::string_view>(__br);

    // field: connection, type: UInt64
    std::uint64_t connection = gm::wire::codec::readValue<std::uint64_t>(__br);

    // field: data, type: Buffer
    gm::wire::GMBuffer data = __buffer_queue.front();
    __buffer_queue.pop();

    // field: offset, type: UInt32
    std::uint32_t offset = gm::wire::codec::readValue<std::uint32_t>(__br);

    // field: size, type: UInt32
    std::uint32_t size = gm::wire::codec::readValue<std::uint32_t>(__br);

    std::int32_t __result = [__impl bluetooth_le_server_notify_value:service_uuid characteristic_uuid:characteristic_uuid connection:connection data:data offset:offset size:size];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_set_callback_state_changed:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    bool __result = [__impl bluetooth_set_callback_state_changed:callback];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_remove_callback_state_changed
{
    bool __result = [__impl bluetooth_remove_callback_state_changed];

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

- (double)__EXT_NATIVE__bluetooth_set_callback_le_disconnected:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    bool __result = [__impl bluetooth_set_callback_le_disconnected:callback];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_remove_callback_le_disconnected
{
    bool __result = [__impl bluetooth_remove_callback_le_disconnected];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_set_callback_le_characteristic_value_changed:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    bool __result = [__impl bluetooth_set_callback_le_characteristic_value_changed:callback];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_remove_callback_le_characteristic_value_changed
{
    bool __result = [__impl bluetooth_remove_callback_le_characteristic_value_changed];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_set_callback_le_server_connection_state_changed:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    bool __result = [__impl bluetooth_set_callback_le_server_connection_state_changed:callback];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_remove_callback_le_server_connection_state_changed
{
    bool __result = [__impl bluetooth_remove_callback_le_server_connection_state_changed];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_set_callback_le_server_read_request:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    bool __result = [__impl bluetooth_set_callback_le_server_read_request:callback];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_remove_callback_le_server_read_request
{
    bool __result = [__impl bluetooth_remove_callback_le_server_read_request];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_set_callback_le_server_write_request:(char*)__arg_buffer arg1:(double)__arg_buffer_length
{
    gm::byteio::BufferReader __br{__arg_buffer, static_cast<size_t>(__arg_buffer_length)};

    // field: callback, type: Function
    gm::wire::GMFunction callback = gm::wire::codec::readFunction(__br, &__dispatch_queue);

    bool __result = [__impl bluetooth_set_callback_le_server_write_request:callback];

    return static_cast<double>(__result);
}

- (double)__EXT_NATIVE__bluetooth_remove_callback_le_server_write_request
{
    bool __result = [__impl bluetooth_remove_callback_le_server_write_request];

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

