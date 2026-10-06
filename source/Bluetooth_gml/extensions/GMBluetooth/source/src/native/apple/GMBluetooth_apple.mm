#include "GMBluetooth_backend.h"
#include "GMBluetooth_log.h"

#if defined(__APPLE__)

#import <Foundation/Foundation.h>
#import <CoreBluetooth/CoreBluetooth.h>
#include <TargetConditionals.h>

#if TARGET_OS_IOS || TARGET_OS_TV
#import <UIKit/UIKit.h>
#endif
#if TARGET_OS_OSX
// NSHost, used for the advertised name, is part of Foundation.
#import <IOBluetooth/IOBluetooth.h>
#endif

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>



// Queued requests. opId is the id the core passed in for the call, reported
// back on its completion; nil for a peripheral open, which has none.
@interface GMBTQueuedMutableDictionary : NSObject
@property (nonatomic, strong) NSNumber *opId;
@property (nonatomic, strong) NSMutableDictionary *dictionary;
// startAdvertising: was sent for it.
@property (nonatomic) BOOL issued;
// Advertising was stopped while CoreBluetooth was starting it: its op has
// already failed, and peripheralManagerDidStartAdvertising: only undoes the
// start and moves on to the next.
@property (nonatomic) BOOL abandoned;

- (instancetype)initWithOpId:(NSNumber *)opId dictionary:(NSMutableDictionary *)dictionary;
@end

@interface GMBTQueuedMutableService : NSObject
@property (nonatomic, strong) NSNumber *opId;
@property (nonatomic, strong) CBMutableService *service;
// addService: was sent for it.
@property (nonatomic) BOOL issued;
// The server was stopped or cleared while CoreBluetooth was adding it:
// didAddService fails its op and removes the service again.
@property (nonatomic) BOOL abandoned;

- (instancetype)initWithOpId:(NSNumber *)opId service:(CBMutableService *)service;
@end

// A notification waiting for the peripheral manager's transmit queue.
// centrals is nil for every subscriber; centralKey names the one central a
// targeted notification is for, so it can be dropped once that central
// unsubscribes.
@interface GMBTQueuedNotification : NSObject
@property (nonatomic, strong) CBMutableCharacteristic *characteristic;
@property (nonatomic, strong) NSData *value;
@property (nonatomic, strong) NSArray<CBCentral *> *centrals;
@property (nonatomic, copy) NSString *centralKey;
@end

@interface GMBTQueuedPeripheral : NSObject
@property (nonatomic, strong) NSNumber *opId;
@property (nonatomic, strong) CBPeripheral *peripheral;

- (instancetype)initWithOpId:(NSNumber *)opId peripheral:(CBPeripheral *)peripheral;
@end

@interface GMBTQueuedTimedPeripheral : GMBTQueuedPeripheral
@property (nonatomic, strong) CBCentralManager *manager;
@property (nonatomic, strong) NSTimer *timer;

- (instancetype)initWithOpId:(NSNumber *)opId peripheral:(CBPeripheral *)peripheral timer:(NSTimer *)timer;
@end

@interface GMBTQueuedService : GMBTQueuedPeripheral
@property (nonatomic, strong) CBService *service;

- (instancetype)initWithOpId:(NSNumber *)opId peripheral:(CBPeripheral *)peripheral service:(CBService *) service;
@end

@interface GMBTQueuedCharacteristic : GMBTQueuedPeripheral
@property (nonatomic, strong) CBCharacteristic *characteristic;

- (instancetype)initWithOpId:(NSNumber *)opId peripheral:(CBPeripheral *)peripheral characteristic:(CBCharacteristic *) characteristic;
@end

@interface GMBTQueuedCharacteristicWithData : GMBTQueuedCharacteristic
@property (nonatomic, strong) NSData* data;

- (instancetype)initWithOpId:(NSNumber *)opId peripheral:(CBPeripheral *)peripheral characteristic:(CBCharacteristic *) characteristic data:(NSData*) data;
@end

// mode is le_characteristic_subscribe's: 0 unsubscribes, 1 and 2 subscribe.
// CoreBluetooth picks notify or indicate from the characteristic itself.
@interface GMBTQueuedSubscription : GMBTQueuedCharacteristic
@property (nonatomic) NSInteger mode;

- (instancetype)initWithOpId:(NSNumber *)opId peripheral:(CBPeripheral *)peripheral characteristic:(CBCharacteristic *) characteristic mode:(NSInteger) mode;
@end

@interface GMBTQueuedDescriptor : GMBTQueuedPeripheral
@property (nonatomic, strong) CBDescriptor *descriptor;

- (instancetype)initWithOpId:(NSNumber *)opId peripheral:(CBPeripheral *)peripheral descriptor:(CBDescriptor *) descriptor;
@end

@interface GMBTQueuedDescriptorWithData : GMBTQueuedDescriptor
@property (nonatomic, strong) NSData* data;

- (instancetype)initWithOpId:(NSNumber *)opId peripheral:(CBPeripheral *)peripheral descriptor:(CBDescriptor *) descriptor data:(NSData*) data;
@end

// One peripheral's GATT requests, one queue per type. CoreBluetooth runs one
// request of each type per peripheral at a time and answers through delegate
// calls that name the peripheral and attribute but no request, so only the
// head of each queue is issued, and a delegate call completes the head only
// when it names the head's attribute.
@interface GMBTPeripheralQueues : NSObject
@property (nonatomic, strong) NSMutableArray<GMBTQueuedPeripheral *> *fetchServices;
@property (nonatomic, strong) NSMutableArray<GMBTQueuedService *> *fetchCharacteristics;
@property (nonatomic, strong) NSMutableArray<GMBTQueuedCharacteristic *> *fetchDescriptors;
@property (nonatomic, strong) NSMutableArray<GMBTQueuedCharacteristic *> *readCharacteristic;
@property (nonatomic, strong) NSMutableArray<GMBTQueuedCharacteristicWithData *> *writeCharacteristic;
@property (nonatomic, strong) NSMutableArray<GMBTQueuedSubscription *> *notifyCharacteristic;
@property (nonatomic, strong) NSMutableArray<GMBTQueuedDescriptor *> *readDescriptor;
@property (nonatomic, strong) NSMutableArray<GMBTQueuedDescriptorWithData *> *writeDescriptor;
// Writes without response waiting for canSendWriteWithoutResponse. They
// get no delegate call: each completes as it is handed to CoreBluetooth.
@property (nonatomic, strong) NSMutableArray<GMBTQueuedCharacteristicWithData *> *writeWithoutResponse;
// RSSI reads, answered by peripheral:didReadRSSI:error:, which names only
// the peripheral.
@property (nonatomic, strong) NSMutableArray<GMBTQueuedPeripheral *> *readRssi;

// The ids the core names this peripheral's services, characteristics and
// descriptors by (R1-127). CoreBluetooth exposes no attribute handle, so
// each object gets an id the first time a discovery reports it and keeps it
// when reported again; two attributes sharing a UUID get two ids. One
// counter serves every kind, from 1. The ids go with this object when the
// link ends.
@property (nonatomic, strong) NSMapTable<CBAttribute *, NSNumber *> *attributeIds;
@property (nonatomic, strong) NSMutableDictionary<NSNumber *, CBAttribute *> *attributesById;
@property (nonatomic) std::uint64_t nextAttributeId;

// The attribute's id, minted on its first report.
- (std::uint64_t)idForAttribute:(CBAttribute *)attribute;
// The attribute's id, 0 when no discovery reported it.
- (std::uint64_t)knownIdForAttribute:(CBAttribute *)attribute;
// The attribute an id names, nil for an id never minted.
- (CBAttribute *)attributeForId:(std::uint64_t)attributeId;
@end

// One didReceiveWriteRequests array. CoreBluetooth wants it treated as a unit
// and answered once, through its first request. Each attribute in it reaches
// GML as one write request with its fragments assembled, and the batch is
// answered when the last of those is: the first error, or success.
@interface GMBTWriteBatch : NSObject
@property (nonatomic, strong) CBATTRequest *firstRequest;
@property (nonatomic) NSUInteger remaining;
@property (nonatomic) CBATTError result;
@end

@interface GMBluetoothAppleTransport:NSObject<CBCentralManagerDelegate,CBPeripheralDelegate,CBPeripheralManagerDelegate>

@property(nonatomic, copy) void (^eventSink)(NSString *type, NSDictionary *params);

// The completion of a GATT, advertise-start or add-service call: the op id
// the core passed in, the BluetoothError and message (Ok and empty on
// success) and what the call returned. No event name: the core knows the
// call from its op id.
@property(nonatomic, copy) void (^opSink)(NSNumber *opId, gmbluetooth::Error error, std::string message, gmbluetooth::LeOpResult result);

// Called after either manager reports a state change, which is also when a
// pending authorization prompt has been answered.
@property(nonatomic, copy) void (^managerStateSink)(void);

// CLIENT

@property(nonatomic, strong) CBCentralManager *centralManager;

// The state each manager last reported, so a change can tell leaving
// PoweredOn from never having reached it.
@property(nonatomic) CBManagerState centralState;
@property(nonatomic) CBManagerState peripheralState;

// Connects in request order. Only the head is issued, once the central is
// PoweredOn; its timer bounds the attempt and names it in its userInfo.
@property(nonatomic, strong) NSMutableArray<GMBTQueuedTimedPeripheral *> *openPeripheralQueue;

// Keyed by peripheralKey:. An entry goes when its peripheral's link ends;
// the core fails the ops it held.
@property(nonatomic, strong) NSMutableDictionary<NSString *, GMBTPeripheralQueues *> *peripheralQueues;

// What scans found, kept across scans so a device found earlier can still
// be connected. discoveredOrder holds the same keys, least recently seen
// first, for the cap.
@property(nonatomic, strong) NSMutableDictionary <NSString *, CBPeripheral *> *discoveredPeripherals;
@property(nonatomic, strong) NSMutableOrderedSet<NSString *> *discoveredOrder;
@property(nonatomic, strong) NSMutableDictionary <NSString *, CBPeripheral *> *openedPeripherals;
@property(nonatomic, strong) NSMutableDictionary <NSString *, CBPeripheral *> *connectedPeripherals;

// The services the scan asked CoreBluetooth to narrow to, nil for every
// device. Kept so a scan deferred until PoweredOn starts with them.
@property(nonatomic, strong) NSArray<CBUUID *> *scanServices;

// SERVER

@property(nonatomic, strong) CBPeripheralManager *peripheralManager;

@property(nonatomic, strong) NSMutableArray <GMBTQueuedMutableService *> *addServiceQueue;
@property(nonatomic, strong) NSMutableArray <GMBTQueuedMutableDictionary *> *startAdvertisementQueue;

@property(nonatomic, strong) NSMutableDictionary <NSString *, CBMutableService *> *addedServices;

// Notifications updateValue refused, sent in order from
// peripheralManagerIsReadyToUpdateSubscribers:.
@property(nonatomic, strong) NSMutableArray<GMBTQueuedNotification *> *notifyQueue;

// Initial values CoreBluetooth cannot cache, keyed by their characteristic
// object: it caches a value only on a read-only characteristic and throws for
// any other, so these are served from didReceiveReadRequest instead.
@property(nonatomic, strong) NSMapTable <CBMutableCharacteristic *, NSData *> *initialValues;

@property(nonatomic, strong) NSMutableDictionary <NSNumber *, CBATTRequest *> *readRequestsLookup;
@property(nonatomic, strong) NSMutableDictionary <NSNumber *, GMBTWriteBatch *> *writeRequestsLookup;

// What each remote central is subscribed to, keyed by its identifier: the
// "service|characteristic" UUID strings, and the CBCentral a targeted notify
// sends to. CoreBluetooth has no peripheral-side disconnect, so a central's
// last unsubscribe is reported as its disconnect.
@property(nonatomic, strong) NSMutableDictionary <NSString *, NSMutableSet<NSString *> *> *centralSubscriptions;
@property(nonatomic, strong) NSMutableDictionary <NSString *, CBCentral *> *subscribedCentrals;

// The calls AppleBackend makes. Each returns Ok or why it failed, with
// message set; the asynchronous ones complete through opSink or eventSink.
- (void) bt_init;
- (void) bt_end;

- (gmbluetooth::Error) bt_le_scan_start:(NSArray<NSString *> *)serviceUuidStrings message:(std::string &)message;
- (gmbluetooth::Error) bt_le_scan_stop:(std::string &)message;
- (BOOL) bt_le_scan_is_active;

- (gmbluetooth::Error) bt_le_advertise_start:(BOOL)includeName serviceUUIDs:(NSArray<NSString *> *)serviceUuidStrings opId:(NSNumber *)opId message:(std::string &)message;
- (gmbluetooth::Error) bt_le_advertise_stop:(std::string &)message;
- (BOOL) bt_le_advertise_is_active;

- (gmbluetooth::Error) bt_le_server_open:(std::string &)message;
- (gmbluetooth::Error) bt_le_server_close:(std::string &)message;
- (gmbluetooth::Error) bt_le_server_add_service:(NSString *)serviceDataString opId:(NSNumber *)opId message:(std::string &)message;
- (gmbluetooth::Error) bt_le_server_clear_services:(std::string &)message;
- (gmbluetooth::Error) bt_le_server_respond_read:(double)requestId status:(double)status value:(NSString *)value message:(std::string &)message;
- (gmbluetooth::Error) bt_le_server_respond_write:(double)requestId status:(double)status message:(std::string &)message;
- (gmbluetooth::Error) bt_le_server_notify_value:(NSString *)serviceUuid characteristicUuid:(NSString *)characteristicUuid central:(NSString *)centralKey value:(NSString *)value message:(std::string &)message;

- (gmbluetooth::Error) bt_le_peripheral_open:(NSString *)peripheralUuid message:(std::string &)message;
- (gmbluetooth::Error) bt_le_peripheral_close:(NSString *)peripheralUuid message:(std::string &)message;
- (BOOL) bt_le_peripheral_is_connected:(NSString *)peripheralUuid;
- (NSInteger) bt_le_peripheral_mtu:(NSString *)peripheralUuid;
- (gmbluetooth::Error) bt_le_peripheral_read_rssi:(NSString *)peripheralUuid opId:(NSNumber *)opId message:(std::string &)message;

- (CBPeripheral *) bt_le_retrieve_peripheral:(NSUUID *)identifier error:(gmbluetooth::Error &)error message:(std::string &)message;
- (NSArray<CBPeripheral *> *) bt_le_connected_peripherals:(NSArray<NSString *> *)serviceUuidStrings error:(gmbluetooth::Error &)error message:(std::string &)message;

// The attribute calls name services, characteristics and descriptors by the
// ids their discovery reported (GMBTPeripheralQueues), never by UUID.
- (gmbluetooth::Error) bt_le_peripheral_get_services:(NSString *)peripheralUuid opId:(NSNumber *)opId message:(std::string &)message;
- (gmbluetooth::Error) bt_le_service_get_characteristics:(NSString *)peripheralUuid service:(std::uint64_t)serviceId opId:(NSNumber *)opId message:(std::string &)message;
- (gmbluetooth::Error) bt_le_characteristic_get_descriptors:(NSString *)peripheralUuid service:(std::uint64_t)serviceId characteristic:(std::uint64_t)characteristicId opId:(NSNumber *)opId message:(std::string &)message;
- (gmbluetooth::Error) bt_le_characteristic_read:(NSString *)peripheralUuid service:(std::uint64_t)serviceId characteristic:(std::uint64_t)characteristicId opId:(NSNumber *)opId message:(std::string &)message;
- (gmbluetooth::Error) bt_le_characteristic_write_request:(NSString *)peripheralUuid service:(std::uint64_t)serviceId characteristic:(std::uint64_t)characteristicId value:(NSString *)value opId:(NSNumber *)opId message:(std::string &)message;
- (gmbluetooth::Error) bt_le_characteristic_write_command:(NSString *)peripheralUuid service:(std::uint64_t)serviceId characteristic:(std::uint64_t)characteristicId value:(NSString *)value opId:(NSNumber *)opId message:(std::string &)message;
- (gmbluetooth::Error) bt_le_characteristic_subscribe:(NSString *)peripheralUuid service:(std::uint64_t)serviceId characteristic:(std::uint64_t)characteristicId mode:(NSInteger)mode opId:(NSNumber *)opId message:(std::string &)message;
- (gmbluetooth::Error) bt_le_descriptor_read:(NSString *)peripheralUuid service:(std::uint64_t)serviceId characteristic:(std::uint64_t)characteristicId descriptor:(std::uint64_t)descriptorId opId:(NSNumber *)opId message:(std::string &)message;
- (gmbluetooth::Error) bt_le_descriptor_write:(NSString *)peripheralUuid service:(std::uint64_t)serviceId characteristic:(std::uint64_t)characteristicId descriptor:(std::uint64_t)descriptorId value:(NSString *)value opId:(NSNumber *)opId message:(std::string &)message;

@end


@implementation GMBTQueuedMutableDictionary
- (instancetype)initWithOpId:(NSNumber *)opId dictionary:(NSMutableDictionary *)dictionary {
    self = [super init];
    if (self) {
        _opId = opId;
        _dictionary = dictionary;
    }
    return self;
}
@end

@implementation GMBTQueuedMutableService
- (instancetype)initWithOpId:(NSNumber *)opId service:(CBMutableService *)service {
    self = [super init];
    if (self) {
        _opId = opId;
        _service = service;
    }
    return self;
}
@end

@implementation GMBTQueuedPeripheral
- (instancetype)initWithOpId:(NSNumber *)opId peripheral:(CBPeripheral *)peripheral {
    self = [super init];
    if (self) {
        _opId = opId;
        _peripheral = peripheral;
    }
    return self;
}
@end

@implementation GMBTQueuedTimedPeripheral
- (instancetype)initWithOpId:(NSNumber *)opId peripheral:(CBPeripheral *)peripheral timer:(NSTimer *)timer {
    self = [super initWithOpId:opId peripheral:peripheral];
    if (self) {
        _timer = timer;
    }
    return self;
}
@end

@implementation GMBTQueuedService
- (instancetype)initWithOpId:(NSNumber *)opId peripheral:(CBPeripheral *)peripheral service:(CBService *) service {
    self = [super initWithOpId:opId peripheral:peripheral];
    if (self) {
        _service = service;
    }
    return self;
}
@end

@implementation GMBTQueuedCharacteristic
- (instancetype)initWithOpId:(NSNumber *)opId peripheral:(CBPeripheral *)peripheral characteristic:(CBCharacteristic *) characteristic {
    self = [super initWithOpId:opId peripheral:peripheral];
    if (self) {
        _characteristic = characteristic;
    }
    return self;
}
@end

@implementation GMBTQueuedCharacteristicWithData
- (instancetype)initWithOpId:(NSNumber *)opId peripheral:(CBPeripheral *)peripheral characteristic:(CBCharacteristic *) characteristic data:(NSData*) data {
    self = [super initWithOpId:opId peripheral:peripheral characteristic:characteristic];
    if (self) {
        _data = data;
    }
    return self;
}
@end

@implementation GMBTQueuedSubscription
- (instancetype)initWithOpId:(NSNumber *)opId peripheral:(CBPeripheral *)peripheral characteristic:(CBCharacteristic *) characteristic mode:(NSInteger) mode {
    self = [super initWithOpId:opId peripheral:peripheral characteristic:characteristic];
    if (self) {
        _mode = mode;
    }
    return self;
}
@end

@implementation GMBTQueuedNotification
@end

@implementation GMBTQueuedDescriptor
- (instancetype)initWithOpId:(NSNumber *)opId peripheral:(CBPeripheral *)peripheral descriptor:(CBDescriptor *) descriptor {
    self = [super initWithOpId:opId peripheral:peripheral];
    if (self) {
        _descriptor = descriptor;
    }
    return self;
}
@end

@implementation GMBTQueuedDescriptorWithData
- (instancetype)initWithOpId:(NSNumber *)opId peripheral:(CBPeripheral *)peripheral descriptor:(CBDescriptor *) descriptor data:(NSData*) data {
    self = [super initWithOpId:opId peripheral:peripheral descriptor:descriptor];
    if (self) {
        _data = data;
    }
    return self;
}
@end

@implementation GMBTPeripheralQueues
- (instancetype)init {
    self = [super init];
    if (self) {
        _fetchServices = [NSMutableArray new];
        _fetchCharacteristics = [NSMutableArray new];
        _fetchDescriptors = [NSMutableArray new];
        _readCharacteristic = [NSMutableArray new];
        _writeCharacteristic = [NSMutableArray new];
        _notifyCharacteristic = [NSMutableArray new];
        _readDescriptor = [NSMutableArray new];
        _writeDescriptor = [NSMutableArray new];
        _writeWithoutResponse = [NSMutableArray new];
        _readRssi = [NSMutableArray new];
        _attributeIds = [NSMapTable mapTableWithKeyOptions:NSPointerFunctionsStrongMemory | NSPointerFunctionsObjectPointerPersonality
                                              valueOptions:NSPointerFunctionsStrongMemory];
        _attributesById = [NSMutableDictionary new];
        _nextAttributeId = 1;
    }
    return self;
}

- (std::uint64_t)idForAttribute:(CBAttribute *)attribute {
    NSNumber *known = [_attributeIds objectForKey:attribute];
    if (known) return known.unsignedLongLongValue;

    const std::uint64_t attributeId = _nextAttributeId++;
    NSNumber *value = @(attributeId);
    [_attributeIds setObject:value forKey:attribute];
    _attributesById[value] = attribute;
    return attributeId;
}

- (std::uint64_t)knownIdForAttribute:(CBAttribute *)attribute {
    if (!attribute) return 0;
    NSNumber *known = [_attributeIds objectForKey:attribute];
    return known ? known.unsignedLongLongValue : 0;
}

- (CBAttribute *)attributeForId:(std::uint64_t)attributeId {
    if (attributeId == 0) return nil;
    return _attributesById[@(attributeId)];
}
@end

@implementation GMBTWriteBatch
@end

// The one pair of string conversions in this file. Both are nil-safe: text
// that is not valid UTF-8 becomes @"", and a nil string or one with no UTF-8
// form becomes "".
static NSString *gmbt_ns(const std::string &text) {
    NSString *value = [NSString stringWithUTF8String:text.c_str()];
    return value ? value : @"";
}

static std::string gmbt_string(NSString *text) {
    const char *utf8 = text ? text.UTF8String : nullptr;
    return utf8 ? std::string(utf8) : std::string();
}

// The key a peripheral is known by in every table and event, and the
// identifier in its "apple:ble:" device id: its identifier, uppercase; ""
// when it has none.
static NSString *gmbt_peripheral_key(CBPeripheral *peripheral) {
    NSString *key = peripheral.identifier.UUIDString;
    return key ? [key uppercaseString] : @"";
}

static gmbluetooth::Error gmbt_fail(std::string &message, gmbluetooth::Error error, std::string text) {
    message = std::move(text);
    return error;
}

static gmbluetooth::Error gmbt_ok(std::string &message) {
    message.clear();
    return gmbluetooth::Error::Ok;
}

// Queued notifications and writes without response wait at most this many
// deep; past it the call is Busy.
static const NSUInteger kGMBTMaxQueuedSends = 64;

// Scan results kept for bt_le_peripheral_open, least recently seen evicted
// first.
static const NSUInteger kGMBTMaxDiscoveredPeripherals = 256;

// How long a connect may take before it fails with Timeout.
static const NSTimeInterval kGMBTConnectTimeoutSeconds = 15;

static NSString *gmbt_manager_state_name(CBManagerState state) {
    switch (state) {
        case CBManagerStateUnknown: return @"Unknown";
        case CBManagerStateResetting: return @"Resetting";
        case CBManagerStateUnsupported: return @"Unsupported";
        case CBManagerStateUnauthorized: return @"Unauthorized";
        case CBManagerStatePoweredOff: return @"PoweredOff";
        case CBManagerStatePoweredOn: return @"PoweredOn";
    }
    return @"Unknown";
}

// Whether a manager in this state can take a request. Unknown and Resetting
// pass: the manager is on its way to a final state, and the request waits
// for it.
static gmbluetooth::Error gmbt_require_powered_on(CBManagerState state, std::string &message) {
    switch (state) {
        case CBManagerStatePoweredOff:
            return gmbt_fail(message, gmbluetooth::Error::BluetoothDisabled, "Bluetooth is powered off");
        case CBManagerStateUnauthorized:
            return gmbt_fail(message, gmbluetooth::Error::PermissionDenied, "Bluetooth permission was denied");
        case CBManagerStateUnsupported:
            return gmbt_fail(message, gmbluetooth::Error::NotSupported, "Bluetooth LE is not supported on this device");
        default:
            return gmbt_ok(message);
    }
}

// Why a request held for, or running on, a manager that has left PoweredOn
// failed: the mapping above, and OperationFailed while it is only resetting.
static gmbluetooth::Error gmbt_unavailable_error(CBManagerState state, std::string &message) {
    const gmbluetooth::Error error = gmbt_require_powered_on(state, message);
    if (error != gmbluetooth::Error::Ok) return error;
    return gmbt_fail(message, gmbluetooth::Error::OperationFailed,
                     "Bluetooth became unavailable (" + gmbt_string(gmbt_manager_state_name(state)) + ")");
}

static BOOL gmbt_state_is_transient(CBManagerState state) {
    return state == CBManagerStateUnknown || state == CBManagerStateResetting;
}

// The BluetoothError and message a CoreBluetooth NSError reports to GML. An
// ATT refusal goes through the shared ATT table; a CBError keeps its code in
// the message.
static gmbluetooth::Error gmbt_error_from_nserror(NSError *error, std::string &message) {
    const int code = (int)error.code;
    const std::string text = gmbt_string(error.localizedDescription);

    if ([error.domain isEqualToString:CBATTErrorDomain]) {
        message = gmbluetooth::att_error_message(code);
        if (!text.empty()) message += " (" + text + ")";
        return gmbluetooth::map_att_error(code);
    }

    if ([error.domain isEqualToString:CBErrorDomain]) {
        message = text + " (CBError " + std::to_string(code) + ")";
        switch (code) {
            case CBErrorConnectionTimeout:
                return gmbluetooth::Error::Timeout;
            case CBErrorNotConnected:
            case CBErrorPeripheralDisconnected:
                return gmbluetooth::Error::Disconnected;
            case CBErrorConnectionFailed:
            case CBErrorConnectionLimitReached:
                return gmbluetooth::Error::ConnectionFailed;
            default:
                return gmbluetooth::Error::OperationFailed;
        }
    }

    std::string domain = gmbt_string(error.domain);
    if (domain.empty()) domain = "error";
    message = text + " (" + domain + " " + std::to_string(code) + ")";
    return gmbluetooth::Error::OperationFailed;
}

@implementation GMBluetoothAppleTransport

// The id a server-side ATT request is answered by (bt_le_server_respond_*).
- (int)generateRequestId {
    static int currentID = 0;
    return currentID++;
}

// EVENTS UTILITIES

- (void) notifyOperation:(NSString *)functionName extraParams:(NSDictionary *)extraParams {
    if (!self.eventSink) return;
    NSDictionary *params = extraParams ? extraParams : @{};
    self.eventSink(functionName, params);
}

// The end of a scan the game started: Ok when it asked for the stop,
// otherwise why the central could not keep it running.
- (void) notifyScanStopped:(gmbluetooth::Error)error message:(const std::string &)message {
    [self notifyOperation:@"bt_le_scan_stopped"
              extraParams:@{ @"error": @((int)error), @"message": gmbt_ns(message) }];
}

// The completion of bt_le_peripheral_open: success, or a failure with its
// BluetoothError. errorCode is what the core reads a failure from (133 is a
// timeout when no error is mapped).
- (void) notifyOpen:(CBPeripheral *)peripheral errorCode:(NSNumber *)errorCode error:(gmbluetooth::Error)error message:(const std::string &)message {
    NSMutableDictionary *params = [NSMutableDictionary dictionary];
    NSString *name = peripheral.name;
    params[@"name"] = name ? name : @"";
    params[@"address"] = [self peripheralKey:peripheral];
    params[@"success"] = @(errorCode == nil);
    if (errorCode) {
        params[@"error_code"] = errorCode;
        params[@"error"] = @((int)error);
        params[@"message"] = gmbt_ns(message);
    }
    [self notifyOperation:@"bt_le_peripheral_open" extraParams:params];
}

// The key a peripheral is known by in every table here, and the identifier
// in its "apple:ble:" device id (gmbt_peripheral_key, the one derivation).
- (NSString *) peripheralKey:(CBPeripheral *)peripheral {
    return gmbt_peripheral_key(peripheral);
}

// Completes the call the core registered as opId; Ok with an empty message
// is success. The other calls need no id: a connect is matched by its
// peripheral, and the rest have no callback.
- (void) completeOp:(NSNumber *)opId failure:(gmbluetooth::Error)failure message:(std::string)message result:(gmbluetooth::LeOpResult)result {
    if (self.opSink) self.opSink(opId, failure, std::move(message), std::move(result));
}

- (void) completeOp:(NSNumber *)opId error:(NSError *)error result:(gmbluetooth::LeOpResult)result {
    if (!error) {
        [self completeOp:opId failure:gmbluetooth::Error::Ok message:std::string() result:std::move(result)];
        return;
    }
    std::string message;
    const gmbluetooth::Error failure = gmbt_error_from_nserror(error, message);
    [self completeOp:opId failure:failure message:std::move(message) result:std::move(result)];
}

- (void) failOp:(NSNumber *)opId error:(gmbluetooth::Error)error message:(const std::string &)message {
    [self completeOp:opId failure:error message:message result:gmbluetooth::LeOpResult{}];
}

- (void) completeOp:(NSNumber *)opId error:(NSError *)error {
    [self completeOp:opId error:error result:gmbluetooth::LeOpResult{}];
}

// What a discovery returns: each attribute's UUID, the id the peripheral's
// registry names it by (minted here on its first report) and a
// characteristic's properties.
- (gmbluetooth::LeOpResult) resultFromServices:(NSArray<CBService *> *)services queues:(GMBTPeripheralQueues *)queues {
    gmbluetooth::LeOpResult result;
    for (CBService *service in services) {
        NSString *uuid = [[self convertTo128BitUUID: service.UUID.UUIDString] uppercaseString];
        result.attributes.push_back(gmbluetooth::LeAttribute{ gmbt_string(uuid), [queues idForAttribute:service], 0 });
    }
    return result;
}

- (gmbluetooth::LeOpResult) resultFromCharacteristics:(NSArray<CBCharacteristic *> *)characteristics queues:(GMBTPeripheralQueues *)queues {
    gmbluetooth::LeOpResult result;
    for (CBCharacteristic *characteristic in characteristics) {
        NSString *uuid = [characteristic.UUID.UUIDString uppercaseString];
        result.attributes.push_back(gmbluetooth::LeAttribute{
            gmbt_string(uuid), [queues idForAttribute:characteristic], static_cast<std::int32_t>(characteristic.properties) });
    }
    return result;
}

- (gmbluetooth::LeOpResult) resultFromDescriptors:(NSArray<CBDescriptor *> *)descriptors queues:(GMBTPeripheralQueues *)queues {
    gmbluetooth::LeOpResult result;
    for (CBDescriptor *descriptor in descriptors) {
        NSString *uuid = [[self convertTo128BitUUID: descriptor.UUID.UUIDString] uppercaseString];
        result.attributes.push_back(gmbluetooth::LeAttribute{ gmbt_string(uuid), [queues idForAttribute:descriptor], 0 });
    }
    return result;
}

- (gmbluetooth::LeOpResult) resultFromValue:(NSData *)value {
    gmbluetooth::LeOpResult result;
    const auto* bytes = static_cast<const std::uint8_t*>(value.bytes);
    if (bytes) result.value.assign(bytes, bytes + value.length);
    return result;
}

// TASK SYSTEM

- (void) queueEnqueue:(NSMutableArray *)queue value:(id) value withHandler:(void (^)())handler {
    [queue addObject: value];
    if (handler && queue.count == 1) handler();
}

- (id) queueDequeue:(NSMutableArray *)queue {
    if (queue.count == 0) return nil;
    id firstObject = [queue firstObject];
    [queue removeObjectAtIndex:0];
    return firstObject;
}

- (id) queuePeek:(NSMutableArray *)queue {
    if (queue.count == 0) return nil;
    return [queue firstObject];
}

- (void) handleQueue:(NSMutableArray *)queue withBlock:(void (^)(id firstObject))block {
    if (queue.count > 0) {
        block(queue.firstObject);
    }
}

- (GMBTPeripheralQueues *) queuesForPeripheral:(CBPeripheral *)peripheral create:(BOOL)create {
    NSString *key = [self peripheralKey:peripheral];
    if (key.length == 0) return nil;
    GMBTPeripheralQueues *queues = _peripheralQueues[key];
    if (!queues && create) {
        queues = [GMBTPeripheralQueues new];
        _peripheralQueues[key] = queues;
    }
    return queues;
}

// Dequeues and returns the head only if it is the request this delegate call
// answers. Anything else is not ours - a request already dropped with its
// peripheral, say - and is logged and left alone.
- (id) takeHeadOf:(NSMutableArray *)queue answering:(NSString *)callbackName matching:(BOOL (^)(id head))matches {
    id head = [self queuePeek:queue];
    if (!head || !matches(head)) {
        NSLog(@"[GMBluetooth] %@ matches no queued request - ignored", callbackName);
        return nil;
    }
    [queue removeObjectAtIndex:0];
    return head;
}

// Drops the peripheral's queued GATT requests without reporting them: the
// core fails their ops when it learns the link ended. Its attribute ids go
// with them.
- (void) dropQueuesForPeripheral:(CBPeripheral *)peripheral {
    NSString *key = [self peripheralKey:peripheral];
    if (key.length > 0) [_peripheralQueues removeObjectForKey:key];
}

// Fails, in queue order, every request on a service the peripheral just
// invalidated, then issues the new head if the head changed.
- (void) failQueue:(NSMutableArray *)queue
           service:(CBService *(^)(id entry))serviceOf
       invalidated:(NSArray<CBService *> *)invalidated
           reissue:(void (^)(void))reissue {
    if (queue.count == 0) return;

    id oldHead = queue.firstObject;
    NSMutableArray *failed = [NSMutableArray array];
    for (id entry in queue) {
        CBService *service = serviceOf(entry);
        if (!service || [invalidated indexOfObjectIdenticalTo:service] != NSNotFound)
            [failed addObject:entry];
    }
    if (failed.count == 0) return;

    [queue removeObjectsInArray:failed];
    for (GMBTQueuedPeripheral *entry in failed) {
        [self completeOp:entry.opId
                 failure:gmbluetooth::map_att_error(CBATTErrorInvalidHandle)
                 message:gmbluetooth::att_error_message(CBATTErrorInvalidHandle) + " (the peripheral removed the service)"
                  result:gmbluetooth::LeOpResult{}];
    }

    if (queue.count > 0 && queue.firstObject != oldHead) reissue();
}

// BLUETOOTH IMPLEMENTATION

- (id) init {
    self = [super init];
    if (self) {

        _discoveredPeripherals = [NSMutableDictionary new];
        _discoveredOrder = [NSMutableOrderedSet new];
        _openedPeripherals = [NSMutableDictionary new];
        _connectedPeripherals = [NSMutableDictionary new];

        _addedServices = [NSMutableDictionary new];
        _notifyQueue = [NSMutableArray new];
        _initialValues = [NSMapTable mapTableWithKeyOptions:NSPointerFunctionsStrongMemory | NSPointerFunctionsObjectPointerPersonality
                                               valueOptions:NSPointerFunctionsStrongMemory];

        _startAdvertisementQueue = [NSMutableArray new];

        _addServiceQueue = [NSMutableArray new];
        _readRequestsLookup = [NSMutableDictionary new];
        _writeRequestsLookup = [NSMutableDictionary new];
        _centralSubscriptions = [NSMutableDictionary new];
        _subscribedCentrals = [NSMutableDictionary new];

        _openPeripheralQueue = [NSMutableArray new];

        _peripheralQueues = [NSMutableDictionary new];

        _centralState = CBManagerStateUnknown;
        _peripheralState = CBManagerStateUnknown;
    }
    return self;
}

// Scan state. Declared here rather than in the SCANNER section below because
// bt_end has to reset them.
static bool _isScanning = false;
static bool _isAdvertising = false;
static bool _isServerOpen = false;

// A scan asked for before the central reached PoweredOn. CoreBluetooth reaches
// that state asynchronously, several runloop turns after bt_init, and discards
// any scan requested in the meantime - so the request is held here and replayed
// from centralManagerDidUpdateState: instead of being silently lost.
static bool _scanPendingPowerOn = false;

- (void) bt_init {
    _centralManager = [[CBCentralManager alloc] initWithDelegate:self queue:nil options:nil];
    _peripheralManager = [[CBPeripheralManager alloc] initWithDelegate:self queue:nil options:nil];
}

- (void) bt_end {
    // These outlive the managers (file-scope), so a shutdown mid-scan would
    // otherwise leave the next bt_init believing a scan is already under way.
    _isScanning = false;
    _scanPendingPowerOn = false;
    _scanServices = nil;
    _isAdvertising = false;
    _isServerOpen = false;

    // A connect timer retains this object and its entry until it fires.
    for (GMBTQueuedTimedPeripheral *queued in _openPeripheralQueue)
        [queued.timer invalidate];
    [_openPeripheralQueue removeAllObjects];

    // The core fails the ops these held once the backend is gone.
    [_peripheralQueues removeAllObjects];
    [_startAdvertisementQueue removeAllObjects];
    [_addServiceQueue removeAllObjects];
    [_addedServices removeAllObjects];
    [_notifyQueue removeAllObjects];
    [_initialValues removeAllObjects];
    [_readRequestsLookup removeAllObjects];
    [_writeRequestsLookup removeAllObjects];
    [_centralSubscriptions removeAllObjects];
    [_subscribedCentrals removeAllObjects];
    [_openedPeripherals removeAllObjects];
    [_connectedPeripherals removeAllObjects];
    [_discoveredPeripherals removeAllObjects];
    [_discoveredOrder removeAllObjects];

    _centralManager = nil;
    _peripheralManager = nil;
    _centralState = CBManagerStateUnknown;
    _peripheralState = CBManagerStateUnknown;
}


// ####################################################################################
// # SCANNER
// ####################################################################################

// Ok while a scan is already running or waiting for PoweredOn: there is one
// scan, and asking for it again changes nothing, its services included.
// serviceUuidStrings is nil for every device; the core validated them, so
// UUIDWithString cannot throw.
- (gmbluetooth::Error) bt_le_scan_start:(NSArray<NSString *> *)serviceUuidStrings message:(std::string &)message {
    const gmbluetooth::Error error = gmbt_require_powered_on(_centralManager.state, message);
    if (error != gmbluetooth::Error::Ok) return error;

    if (_isScanning || _scanPendingPowerOn) return gmbt_ok(message);

    NSMutableArray<CBUUID *> *services = nil;
    if (serviceUuidStrings.count > 0) {
        services = [NSMutableArray arrayWithCapacity:serviceUuidStrings.count];
        for (NSString *uuidString in serviceUuidStrings) {
            CBUUID *uuid = [CBUUID UUIDWithString:uuidString];
            if (![services containsObject:uuid]) [services addObject:uuid];
        }
    }
    _scanServices = services;

    if (_centralManager.state != CBManagerStatePoweredOn) {
        _scanPendingPowerOn = true;
        NSLog(@"[GMBluetooth] bt_le_scan_start: central is not PoweredOn yet - scan DEFERRED, "
              @"it will start automatically from centralManagerDidUpdateState:");
        return gmbt_ok(message);
    }

    [self beginScan];
    return gmbt_ok(message);
}

// Issues the actual CoreBluetooth scan. Only ever called with the central
// already PoweredOn, so _isScanning tracks a scan that really started.
// Narrowed to _scanServices when every filter names a service, which is
// what lets iOS keep scanning in the background; the core still matches
// each result against the filters.
- (void) beginScan {

    _isScanning = true;

    [_centralManager scanForPeripheralsWithServices:_scanServices
                                             options:@{ CBCentralManagerScanOptionAllowDuplicatesKey: @YES }];

    NSLog(@"[GMBluetooth] beginScan: CBCentralManager.isScanning=%d services=%lu",
          (int)[_centralManager isScanning], (unsigned long)_scanServices.count);
}

// A deferred scan counts as running: the game was told it started.
- (BOOL) bt_le_scan_is_active {
    return _isScanning || _scanPendingPowerOn;
}

// Ok with no event when nothing is running; otherwise the scan ends here and
// scan_stopped reports it.
- (gmbluetooth::Error) bt_le_scan_stop:(std::string &)message {

    if (!_isScanning && !_scanPendingPowerOn) return gmbt_ok(message);

    // Cancels a deferred request too, so stopping before the central powers on
    // does not leave a scan queued to fire later.
    const bool wasScanning = _isScanning;
    _scanPendingPowerOn = false;
    _isScanning = false;
    _scanServices = nil;

    if (wasScanning) [_centralManager stopScan];

    [self notifyScanStopped:gmbluetooth::Error::Ok message:std::string()];
    return gmbt_ok(message);
}

// True while the peripheral is open or waiting in the connect queue.
- (BOOL) peripheralIsInUse:(NSString *)key {
    if (_openedPeripherals[key]) return YES;
    for (GMBTQueuedTimedPeripheral *queued in _openPeripheralQueue) {
        if ([[self peripheralKey:queued.peripheral] isEqualToString:key]) return YES;
    }
    return NO;
}

// Keeps a peripheral bt_le_peripheral_open can find, most recently seen last.
// Past the cap the least recently seen goes, unless it is open or queued.
- (void) rememberPeripheral:(CBPeripheral *)peripheral {
    NSString *key = [self peripheralKey:peripheral];
    if (key.length == 0) return;

    _discoveredPeripherals[key] = peripheral;
    [_discoveredOrder removeObject:key];
    [_discoveredOrder addObject:key];

    NSUInteger index = 0;
    while (_discoveredOrder.count > kGMBTMaxDiscoveredPeripherals && index < _discoveredOrder.count) {
        NSString *oldest = _discoveredOrder[index];
        if ([self peripheralIsInUse:oldest]) {
            ++index;
            continue;
        }
        [_discoveredOrder removeObjectAtIndex:index];
        [_discoveredPeripherals removeObjectForKey:oldest];
    }
}

- (void) forgetDiscoveredPeripherals {
    [_discoveredPeripherals removeAllObjects];
    [_discoveredOrder removeAllObjects];
}

- (void) centralManager:(CBCentralManager *)central didDiscoverPeripheral:(CBPeripheral *)peripheral advertisementData:(NSDictionary<NSString *,id> *)advertisementData RSSI:(NSNumber *)RSSI {

    [self rememberPeripheral:peripheral];

    // 1. Name: the advertised one first, since peripheral.name is a cached
    // GAP name that may be stale or missing.
    NSString *name = advertisementData[CBAdvertisementDataLocalNameKey];
    if (![name isKindOfClass:[NSString class]] || name.length == 0) name = peripheral.name;
    if (!name) name = @"";

    // 2. Address (UUID in this case)
    NSString *uuidString = [self peripheralKey:peripheral];

    // 3. Is_connectable
    NSNumber *isConnectable = advertisementData[CBAdvertisementDataIsConnectable];
    BOOL connectable = [isConnectable boolValue];

    NSMutableDictionary *params = [NSMutableDictionary dictionary];
    params[@"name"] = name;
    params[@"address"] = uuidString;
    params[@"is_connectable"] = @(connectable);

    // 4. Signal strength. CoreBluetooth reports 127 when it has none.
    if (RSSI && RSSI.integerValue != 127) params[@"raw_signal"] = RSSI;

    // 5. The rest of the advertisement, as the bus carries it: UUID strings
    // and base64 bytes. CoreBluetooth hands over the advertisement and the
    // scan response merged; a key of the wrong type is skipped.
    NSMutableArray<NSString *> *serviceUuids = [NSMutableArray array];
    for (NSString *key in @[ CBAdvertisementDataServiceUUIDsKey, CBAdvertisementDataOverflowServiceUUIDsKey ]) {
        id list = advertisementData[key];
        if (![list isKindOfClass:[NSArray class]]) continue;
        for (id uuid in (NSArray *)list) {
            if ([uuid isKindOfClass:[CBUUID class]]) [serviceUuids addObject:((CBUUID *)uuid).UUIDString];
        }
    }
    if (serviceUuids.count > 0) params[@"service_uuids"] = serviceUuids;

    id serviceData = advertisementData[CBAdvertisementDataServiceDataKey];
    if ([serviceData isKindOfClass:[NSDictionary class]]) {
        NSMutableArray<NSDictionary *> *entries = [NSMutableArray array];
        for (id uuid in (NSDictionary *)serviceData) {
            id data = ((NSDictionary *)serviceData)[uuid];
            if (![uuid isKindOfClass:[CBUUID class]] || ![data isKindOfClass:[NSData class]]) continue;
            [entries addObject:@{ @"uuid": ((CBUUID *)uuid).UUIDString,
                                  @"data": [(NSData *)data base64EncodedStringWithOptions:0] }];
        }
        if (entries.count > 0) params[@"service_data"] = entries;
    }

    // The first two bytes are the company id, little-endian; anything
    // shorter names no company and is dropped.
    id manufacturerData = advertisementData[CBAdvertisementDataManufacturerDataKey];
    if ([manufacturerData isKindOfClass:[NSData class]] && ((NSData *)manufacturerData).length >= 2) {
        NSData *bytes = (NSData *)manufacturerData;
        const auto *raw = static_cast<const std::uint8_t *>(bytes.bytes);
        const int companyId = raw[0] | (raw[1] << 8);
        NSData *rest = [bytes subdataWithRange:NSMakeRange(2, bytes.length - 2)];
        params[@"manufacturer_data"] = @[ @{ @"company_id": @(companyId),
                                             @"data": [rest base64EncodedStringWithOptions:0] } ];
    }

    id txPower = advertisementData[CBAdvertisementDataTxPowerLevelKey];
    if ([txPower isKindOfClass:[NSNumber class]]) params[@"tx_power"] = txPower;

    [self notifyOperation:@"bt_le_scan_result" extraParams:params];
}

// ####################################################################################
// # ADVERTISER
// ####################################################################################

- (void) handleStartAdvertisementQueue {
    if (!_peripheralManager || _peripheralManager.state != CBManagerStatePoweredOn)
        return;

    GMBTQueuedMutableDictionary *queued = [self queuePeek:_startAdvertisementQueue];
    if (!queued || queued.issued) return;
    queued.issued = YES;
    [_peripheralManager startAdvertising:queued.dictionary];
}

// Fails every start still waiting for peripheralManagerDidStartAdvertising:.
// With keepIssuedHead (a stop or a server close, the manager still on) a
// start CoreBluetooth is working on stays at the head, abandoned, so its late
// answer cannot complete a newer start; otherwise (the manager left PoweredOn)
// no answer will come and every entry goes. An entry already abandoned was
// failed then and is not failed again.
- (void) failAdvertiseStarts:(gmbluetooth::Error)error message:(const std::string &)message keepIssuedHead:(BOOL)keepIssuedHead {
    GMBTQueuedMutableDictionary *head = [self queuePeek:_startAdvertisementQueue];
    const BOOL keepHead = keepIssuedHead && head.issued;

    NSMutableArray<GMBTQueuedMutableDictionary *> *failed = [NSMutableArray array];
    for (GMBTQueuedMutableDictionary *queued in _startAdvertisementQueue) {
        if (!queued.abandoned) [failed addObject:queued];
    }
    [_startAdvertisementQueue removeAllObjects];
    if (keepHead) {
        head.abandoned = YES;
        [_startAdvertisementQueue addObject:head];
    }

    for (GMBTQueuedMutableDictionary *queued in failed)
        [self failOp:queued.opId error:error message:message];
}

// CBPeripheralManager.startAdvertising takes only LocalName and ServiceUUIDs;
// AppleBackend::le_advertise_start refuses every other field before this.
// The UUIDs were validated by the core, so UUIDWithString cannot throw.
- (gmbluetooth::Error) bt_le_advertise_start:(BOOL)includeName serviceUUIDs:(NSArray<NSString *> *)serviceUuidStrings opId:(NSNumber *)opId message:(std::string &)message {
    const gmbluetooth::Error error = gmbt_require_powered_on(_peripheralManager.state, message);
    if (error != gmbluetooth::Error::Ok) return error;

    if ([self bt_le_advertise_is_active])
        return gmbt_fail(message, gmbluetooth::Error::Busy, "Bluetooth LE advertising is already running; stop it first");

    NSMutableDictionary *advertisementData = [NSMutableDictionary dictionary];

    if (includeName) {
#if TARGET_OS_OSX
        NSString *deviceName = [[NSHost currentHost] localizedName];
#elif TARGET_OS_IOS || TARGET_OS_TV
        NSString *deviceName = [[UIDevice currentDevice] name];
#else
        NSString *deviceName = nil;
#endif
        if (deviceName.length > 0)
            advertisementData[CBAdvertisementDataLocalNameKey] = deviceName;
    }

    NSMutableArray<CBUUID *> *serviceUUIDs = [NSMutableArray array];
    for (NSString *uuidString in serviceUuidStrings) {
        if (uuidString.length == 0) continue;
        [serviceUUIDs addObject:[CBUUID UUIDWithString:uuidString]];
    }
    if (serviceUUIDs.count > 0)
        advertisementData[CBAdvertisementDataServiceUUIDsKey] = serviceUUIDs;

    GMBTQueuedMutableDictionary *queued = [[GMBTQueuedMutableDictionary alloc] initWithOpId:opId
                                                                                    dictionary:advertisementData];
    [_startAdvertisementQueue addObject:queued];
    [self handleStartAdvertisementQueue];
    return gmbt_ok(message);
}

// Ok with nothing to do when idle. A start still waiting for its answer
// fails, since the game asked for advertising to end before it began.
- (gmbluetooth::Error) bt_le_advertise_stop:(std::string &)message {
    if (![self bt_le_advertise_is_active] && !_peripheralManager.isAdvertising) return gmbt_ok(message);

    [self failAdvertiseStarts:gmbluetooth::Error::OperationFailed message:"Advertising stopped before it started" keepIssuedHead:YES];
    // A start CoreBluetooth is still working on is left to finish: its answer
    // is what unblocks the queue, and the abandoned branch of
    // peripheralManagerDidStartAdvertising: stops it then. Stopping here could
    // cancel the start without an answer and leave the queue blocked for good.
    GMBTQueuedMutableDictionary *head = [self queuePeek:_startAdvertisementQueue];
    const BOOL startInFlight = head != nil && head.abandoned && head.issued;
    if (!startInFlight && _peripheralManager.state == CBManagerStatePoweredOn) [_peripheralManager stopAdvertising];
    _isAdvertising = false;
    return gmbt_ok(message);
}

// Running from the call that starts it, so a second start is Busy while the
// first is still waiting for CoreBluetooth. An abandoned start does not
// count: a new start queues behind it and is issued once it is answered.
- (BOOL) bt_le_advertise_is_active {
    if (_isAdvertising) return YES;
    for (GMBTQueuedMutableDictionary *queued in _startAdvertisementQueue) {
        if (!queued.abandoned) return YES;
    }
    return NO;
}

- (void) peripheralManagerDidStartAdvertising:(CBPeripheralManager *)peripheral error:(NSError *)error {
    GMBTQueuedMutableDictionary *queuedAdvertisementData = [self queuePeek:_startAdvertisementQueue];
    if (!queuedAdvertisementData || !queuedAdvertisementData.issued) return;
    [self queueDequeue:_startAdvertisementQueue];

    // Stopped while starting: its op already failed, so a start that took is
    // undone and the next one goes.
    if (queuedAdvertisementData.abandoned) {
        if (!error && peripheral.state == CBManagerStatePoweredOn) [peripheral stopAdvertising];
        _isAdvertising = false;
        [self handleStartAdvertisementQueue];
        return;
    }

    _isAdvertising = (error == nil && peripheral.isAdvertising);
    [self completeOp:queuedAdvertisementData.opId error:error];

    [self handleStartAdvertisementQueue];
}

// ####################################################################################
// # SERVER
// ####################################################################################

- (void) handleAddServiceQueue {
    if (!_peripheralManager || _peripheralManager.state != CBManagerStatePoweredOn)
        return;

    GMBTQueuedMutableService *queued = [self queuePeek:_addServiceQueue];
    if (!queued || queued.issued) return;
    queued.issued = YES;
    [_peripheralManager addService:queued.service];
}

// Forgets every service, value, queued notification and subscription the
// server held. Queued adds fail with error; an add CoreBluetooth is working
// on fails too when failInFlight (no answer will come), and is otherwise left
// for didAddService to fail and undo.
- (void) resetServerState:(gmbluetooth::Error)error message:(const std::string &)message failInFlight:(BOOL)failInFlight {
    GMBTQueuedMutableService *head = [self queuePeek:_addServiceQueue];
    const BOOL keepHead = head && head.issued && !failInFlight;
    if (keepHead) {
        head.abandoned = YES;
        [_addServiceQueue removeObjectAtIndex:0];
    }
    while (_addServiceQueue.count > 0) {
        GMBTQueuedMutableService *queued = [self queueDequeue:_addServiceQueue];
        [self failOp:queued.opId error:error message:message];
    }
    if (keepHead) [_addServiceQueue addObject:head];

    if (_peripheralManager.state == CBManagerStatePoweredOn) [_peripheralManager removeAllServices];
    [_addedServices removeAllObjects];
    [_initialValues removeAllObjects];
    [_notifyQueue removeAllObjects];
    // The core answered the requests these held before asking.
    [_readRequestsLookup removeAllObjects];
    [_writeRequestsLookup removeAllObjects];
    // The characteristics they named are gone; no unsubscribe will come.
    [_centralSubscriptions removeAllObjects];
    [_subscribedCentrals removeAllObjects];
}

// Ok when already open: there is one server.
- (gmbluetooth::Error) bt_le_server_open:(std::string &)message {
    const gmbluetooth::Error error = gmbt_require_powered_on(_peripheralManager.state, message);
    if (error != gmbluetooth::Error::Ok) return error;

    _isServerOpen = true;
    return gmbt_ok(message);
}

- (gmbluetooth::Error) bt_le_server_add_service:(NSString*) serviceDataString opId:(NSNumber *)opId message:(std::string &)message {
    const gmbluetooth::Error stateError = gmbt_require_powered_on(_peripheralManager.state, message);
    if (stateError != gmbluetooth::Error::Ok) return stateError;
    if (!_isServerOpen) return gmbt_fail(message, gmbluetooth::Error::OperationFailed, "The GATT server is not running");

    NSData *data = [serviceDataString dataUsingEncoding:NSUTF8StringEncoding];
    NSError *error = nil;
    id parsed = data ? [NSJSONSerialization JSONObjectWithData:data options:0 error:&error] : nil;
    if (error || ![parsed isKindOfClass:[NSDictionary class]]) {
        NSLog(@"[GMBluetooth] invalid GATT service definition: %@", error);
        return gmbt_fail(message, gmbluetooth::Error::InvalidArgument, "Invalid GATT service definition");
    }

    NSDictionary *serviceData = (NSDictionary *)parsed;
    NSString *serviceUuidString = serviceData[@"uuid"];
    if (![serviceUuidString isKindOfClass:[NSString class]] || serviceUuidString.length == 0)
        return gmbt_fail(message, gmbluetooth::Error::InvalidArgument, "The GATT service has no UUID");

    CBUUID *serviceUUID = [CBUUID UUIDWithString:serviceUuidString];
    CBMutableService *service = [[CBMutableService alloc] initWithType:serviceUUID primary:YES];

    CBUUID *cccdUUID = [CBUUID UUIDWithString:CBUUIDClientCharacteristicConfigurationString];
    CBUUID *userDescriptionUUID = [CBUUID UUIDWithString:CBUUIDCharacteristicUserDescriptionString];

    NSMutableArray<CBMutableCharacteristic *> *characteristicsArray = [NSMutableArray array];
    // Moved into _initialValues only once the whole definition parsed, so a
    // refused definition leaves nothing behind.
    NSMapTable<CBMutableCharacteristic *, NSData *> *pendingInitialValues =
        [NSMapTable mapTableWithKeyOptions:NSPointerFunctionsStrongMemory | NSPointerFunctionsObjectPointerPersonality
                              valueOptions:NSPointerFunctionsStrongMemory];
    id characteristicsValue = serviceData[@"characteristics"];
    if ([characteristicsValue isKindOfClass:[NSArray class]]) {
        for (id charValue in (NSArray *)characteristicsValue) {
            if (![charValue isKindOfClass:[NSDictionary class]]) continue;
            NSDictionary *charDict = (NSDictionary *)charValue;

            NSString *charUuidString = charDict[@"uuid"];
            if (![charUuidString isKindOfClass:[NSString class]] || charUuidString.length == 0)
                continue;

            CBUUID *charUUID = [CBUUID UUIDWithString:charUuidString];
            NSUInteger rawProperties = [charDict[@"properties"] unsignedIntegerValue];

            // The public bit values intentionally match CoreBluetooth for the common
            // characteristic properties. Apple forbids Broadcast and ExtendedProperties
            // when constructing a local CBMutableCharacteristic, so mask those two bits.
            rawProperties &= ~(static_cast<NSUInteger>(CBCharacteristicPropertyBroadcast) |
                               static_cast<NSUInteger>(CBCharacteristicPropertyExtendedProperties));
            CBCharacteristicProperties charProperties = (CBCharacteristicProperties)rawProperties;

            // permissions is BluetoothLeAttributePermission, Android's PERMISSION_*
            // bits. CoreBluetooth has no signed-write permission, so asking for
            // one fails the call instead of adding a weaker characteristic.
            const NSUInteger permissions = [charDict[@"permissions"] unsignedIntegerValue];
            if (permissions & (gmbluetooth::kPermissionWriteSigned | gmbluetooth::kPermissionWriteSignedMitm)) {
                NSLog(@"[GMBluetooth] characteristic %@ asks for a signed-write permission, which Apple does not have", charUuidString);
                return gmbt_fail(message, gmbluetooth::Error::NotSupported, "Apple has no signed-write permission");
            }
            CBAttributePermissions charPermissions = 0;
            if (permissions & gmbluetooth::kPermissionRead)
                charPermissions |= CBAttributePermissionsReadable;
            if (permissions & (gmbluetooth::kPermissionReadEncrypted | gmbluetooth::kPermissionReadEncryptedMitm))
                charPermissions |= CBAttributePermissionsReadEncryptionRequired;
            if (permissions & gmbluetooth::kPermissionWrite)
                charPermissions |= CBAttributePermissionsWriteable;
            if (permissions & (gmbluetooth::kPermissionWriteEncrypted | gmbluetooth::kPermissionWriteEncryptedMitm))
                charPermissions |= CBAttributePermissionsWriteEncryptionRequired;

            NSData *initialValue = nil;
            id initialValueField = charDict[@"value"];
            if ([initialValueField isKindOfClass:[NSString class]] && [(NSString *)initialValueField length] > 0) {
                initialValue = [[NSData alloc] initWithBase64EncodedString:(NSString *)initialValueField options:0];
                if (!initialValue) {
                    NSLog(@"[GMBluetooth] characteristic %@ has invalid base64 initial value", charUuidString);
                    return gmbt_fail(message, gmbluetooth::Error::InvalidArgument, "A characteristic's initial value is not valid base64");
                }
            }

            // CoreBluetooth caches a value only on a read-only characteristic
            // and throws for any other; those get it from _initialValues.
            const BOOL readOnly = charProperties == CBCharacteristicPropertyRead &&
                (charPermissions & (CBAttributePermissionsWriteable | CBAttributePermissionsWriteEncryptionRequired)) == 0;

            CBMutableCharacteristic *characteristic =
                [[CBMutableCharacteristic alloc] initWithType:charUUID
                                                   properties:charProperties
                                                        value:(readOnly ? initialValue : nil)
                                                  permissions:charPermissions];
            if (initialValue && !readOnly)
                [pendingInitialValues setObject:initialValue forKey:characteristic];

            NSMutableArray<CBMutableDescriptor *> *descriptorsArray = [NSMutableArray array];
            id descriptorsValue = charDict[@"descriptors"];
            if ([descriptorsValue isKindOfClass:[NSArray class]]) {
                for (id descValue in (NSArray *)descriptorsValue) {
                    if (![descValue isKindOfClass:[NSDictionary class]]) continue;
                    NSString *descUuidString = ((NSDictionary *)descValue)[@"uuid"];
                    if (![descUuidString isKindOfClass:[NSString class]] || descUuidString.length == 0)
                        continue;

                    CBUUID *descUUID = [CBUUID UUIDWithString:descUuidString];
                    // CoreBluetooth owns the CCCD for Notify/Indicate characteristics.
                    if ([descUUID isEqual:cccdUUID])
                        continue;

                    // CBMutableDescriptor takes only the User Description, whose
                    // value must be a string, and the Presentation Format, whose
                    // value must be its 7 bytes; the definition carries no value,
                    // and anything else throws. Refused before anything is added.
                    if (![descUUID isEqual:userDescriptionUUID]) {
                        return gmbt_fail(message, gmbluetooth::Error::NotSupported,
                                         "Apple can host only the Characteristic User Description descriptor (0x2901), not " +
                                         gmbt_string(descUuidString));
                    }

                    CBMutableDescriptor *descriptor = [[CBMutableDescriptor alloc] initWithType:descUUID value:@""];
                    [descriptorsArray addObject:descriptor];
                }
            }

            if (descriptorsArray.count > 0)
                characteristic.descriptors = descriptorsArray;
            [characteristicsArray addObject:characteristic];
        }
    }

    service.characteristics = characteristicsArray;
    for (CBMutableCharacteristic *characteristic in pendingInitialValues)
        [_initialValues setObject:[pendingInitialValues objectForKey:characteristic] forKey:characteristic];

    GMBTQueuedMutableService *queueService = [[GMBTQueuedMutableService alloc] initWithOpId:opId service:service];
    [_addServiceQueue addObject:queueService];
    [self handleAddServiceQueue];
    return gmbt_ok(message);
}

// Ok when there is nothing to clear.
- (gmbluetooth::Error) bt_le_server_clear_services:(std::string &)message {
    if (!_isServerOpen) return gmbt_ok(message);

    [self resetServerState:gmbluetooth::Error::OperationFailed
                   message:"The GATT services were cleared before this one was added"
              failInFlight:NO];
    return gmbt_ok(message);
}

// Ok when already closed. Advertising stops with the server.
- (gmbluetooth::Error) bt_le_server_close:(std::string &)message {
    if (!_isServerOpen) return gmbt_ok(message);

    _isServerOpen = false;
    // The core answered the requests these held before asking, and retires
    // every central it knows once the stop succeeds.
    [self resetServerState:gmbluetooth::Error::OperationFailed
                   message:"The GATT server stopped before the service was added"
              failInFlight:NO];

    [self failAdvertiseStarts:gmbluetooth::Error::OperationFailed message:"Advertising stopped before it started" keepIssuedHead:YES];
    // As in bt_le_advertise_stop: a start still in flight is stopped by its
    // own answer, never cancelled here.
    GMBTQueuedMutableDictionary *advertiseHead = [self queuePeek:_startAdvertisementQueue];
    const BOOL startInFlight = advertiseHead != nil && advertiseHead.abandoned && advertiseHead.issued;
    if (!startInFlight && [_peripheralManager isAdvertising]) {
        [_peripheralManager stopAdvertising];
    }
    _isAdvertising = false;

    return gmbt_ok(message);
}

- (gmbluetooth::Error) bt_le_server_respond_read:(double) requestId status:(double) status value:(NSString*) value message:(std::string &)message {
    NSNumber *requestKey = [NSNumber numberWithDouble:requestId];
    CBATTRequest *request = _readRequestsLookup[requestKey];
    if (!request) return gmbt_fail(message, gmbluetooth::Error::OperationFailed, "The read request is no longer pending");

    // status is an ATT error code the core validated. A refusal carries no
    // value, so the (empty) value is only decoded for a success.
    const CBATTError result = (CBATTError)status;
    if (result == CBATTErrorSuccess) {
        NSData *dataValue = [[NSData alloc] initWithBase64EncodedString:(value ? value : @"") options:0];
        if (!dataValue) return gmbt_fail(message, gmbluetooth::Error::InvalidArgument, "The read response is not valid base64");
        request.value = dataValue;
    }

    [_peripheralManager respondToRequest:request withResult:result];
    [_readRequestsLookup removeObjectForKey:requestKey];
    return gmbt_ok(message);
}

- (gmbluetooth::Error) bt_le_server_respond_write:(double) requestId status:(double) status message:(std::string &)message {
    NSNumber *requestKey = [NSNumber numberWithDouble:requestId];

    GMBTWriteBatch *batch = _writeRequestsLookup[requestKey];
    if (!batch) return gmbt_fail(message, gmbluetooth::Error::OperationFailed, "The write request is no longer pending");
    [_writeRequestsLookup removeObjectForKey:requestKey];

    // The batch is answered once, after its last part, with the first error.
    if ((CBATTError)status != CBATTErrorSuccess && batch.result == CBATTErrorSuccess) {
        batch.result = (CBATTError)status;
    }
    if (batch.remaining > 0) {
        batch.remaining--;
    }
    if (batch.remaining == 0) {
        [_peripheralManager respondToRequest:batch.firstRequest withResult:batch.result];
    }

    return gmbt_ok(message);
}

- (BOOL) central:(NSString *)centralKey isSubscribedTo:(CBCharacteristic *)characteristic {
    return _subscribedCentrals[centralKey] != nil &&
           [_centralSubscriptions[centralKey] containsObject:[self subscriptionKey:characteristic]];
}

// Sends queued notifications in order until CoreBluetooth's transmit queue is
// full again; peripheralManagerIsReadyToUpdateSubscribers: resumes. One for a
// central that has since unsubscribed is dropped.
- (void) sendQueuedNotifications {
    while (_notifyQueue.count > 0) {
        GMBTQueuedNotification *queued = _notifyQueue.firstObject;
        if (queued.centralKey.length > 0 && ![self central:queued.centralKey isSubscribedTo:queued.characteristic]) {
            [_notifyQueue removeObjectAtIndex:0];
            continue;
        }
        if (![_peripheralManager updateValue:queued.value forCharacteristic:queued.characteristic onSubscribedCentrals:queued.centrals])
            return;
        [_notifyQueue removeObjectAtIndex:0];
    }
}

// central: empty broadcasts to every subscriber; otherwise the key a server
// event named the central by, NotFound when that central is not subscribed
// to the characteristic. A value CoreBluetooth cannot take now waits, in
// order, behind any already waiting.
- (gmbluetooth::Error) bt_le_server_notify_value:(NSString*) serviceUuid characteristicUuid:(NSString*) characteristicUuid central:(NSString*) centralKey value:(NSString*) value message:(std::string &)message {
    const gmbluetooth::Error stateError = gmbt_require_powered_on(_peripheralManager.state, message);
    if (stateError != gmbluetooth::Error::Ok) return stateError;
    if (!_isServerOpen) return gmbt_fail(message, gmbluetooth::Error::OperationFailed, "The GATT server is not running");

    NSData *dataValue = [[NSData alloc] initWithBase64EncodedString:(value ? value : @"") options:0];
    if (!dataValue) return gmbt_fail(message, gmbluetooth::Error::InvalidArgument, "The value is not valid base64");

    NSString *canonicalServiceUuid = [CBUUID UUIDWithString:serviceUuid].UUIDString;
    NSString *canonicalCharacteristicUuid = [CBUUID UUIDWithString:characteristicUuid].UUIDString;

    CBMutableService *service = _addedServices[canonicalServiceUuid];
    if (!service) return gmbt_fail(message, gmbluetooth::Error::NotFound, "Service " + gmbt_string(serviceUuid) + " not found");

    CBMutableCharacteristic *characteristic = nil;
    for (CBMutableCharacteristic *charac in service.characteristics) {
        if ([charac.UUID.UUIDString isEqualToString:canonicalCharacteristicUuid]) {
            characteristic = charac;
            break;
        }
    }
    if (!characteristic)
        return gmbt_fail(message, gmbluetooth::Error::NotFound, "Characteristic " + gmbt_string(characteristicUuid) + " not found");

    NSArray<CBCentral *> *centrals = nil;
    if (centralKey.length > 0) {
        if (![self central:centralKey isSubscribedTo:characteristic])
            return gmbt_fail(message, gmbluetooth::Error::NotFound, "That central is not subscribed to the characteristic");
        centrals = @[ _subscribedCentrals[centralKey] ];
    }

    if (_notifyQueue.count >= kGMBTMaxQueuedSends)
        return gmbt_fail(message, gmbluetooth::Error::Busy, "Too many GATT notifications are waiting to be sent");

    // Nothing waiting: sent now if CoreBluetooth takes it.
    if (_notifyQueue.count == 0 &&
        [_peripheralManager updateValue:dataValue forCharacteristic:characteristic onSubscribedCentrals:centrals])
        return gmbt_ok(message);

    GMBTQueuedNotification *queued = [GMBTQueuedNotification new];
    queued.characteristic = characteristic;
    queued.value = dataValue;
    queued.centrals = centrals;
    queued.centralKey = centrals ? centralKey : nil;
    [_notifyQueue addObject:queued];
    return gmbt_ok(message);
}

- (void) peripheralManager:(CBPeripheralManager *)peripheral didAddService:(CBService *)service error:(NSError *)error {
    GMBTQueuedMutableService *queuedService = [self queuePeek:_addServiceQueue];
    if (!queuedService || !queuedService.issued) return;
    [self queueDequeue:_addServiceQueue];

    // The server was stopped or cleared meanwhile: the service goes again.
    if (queuedService.abandoned) {
        if (!error) [peripheral removeService:queuedService.service];
        for (CBMutableCharacteristic *characteristic in queuedService.service.characteristics)
            [_initialValues removeObjectForKey:characteristic];
        [self failOp:queuedService.opId error:gmbluetooth::Error::OperationFailed
             message:"The GATT server was stopped or cleared before the service was added"];
        [self handleAddServiceQueue];
        return;
    }

    if (!error) _addedServices[[service.UUID UUIDString]] = queuedService.service;
    else {
        for (CBMutableCharacteristic *characteristic in queuedService.service.characteristics)
            [_initialValues removeObjectForKey:characteristic];
    }
    [self completeOp:queuedService.opId error:error];

    [self handleAddServiceQueue];
}

- (void) peripheralManagerIsReadyToUpdateSubscribers:(CBPeripheralManager *)peripheral {
    [self sendQueuedNotifications];
}

- (void) peripheralManagerDidUpdateState:(CBPeripheralManager *)peripheral {
    [self applyPeripheralManagerState:peripheral];
    if (self.managerStateSink) self.managerStateSink();
}

- (void) applyPeripheralManagerState:(CBPeripheralManager *)peripheral {
    const CBManagerState previous = _peripheralState;
    _peripheralState = peripheral.state;

    if (peripheral.state == CBManagerStatePoweredOn) {
        [self handleAddServiceQueue];
        [self handleStartAdvertisementQueue];
        return;
    }

    // Not up yet: what was asked for waits for PoweredOn or a final state.
    if (gmbt_state_is_transient(peripheral.state) && previous != CBManagerStatePoweredOn)
        return;

    std::string message;
    const gmbluetooth::Error error = gmbt_unavailable_error(peripheral.state, message);

    _isAdvertising = false;
    [self failAdvertiseStarts:error message:message keepIssuedHead:NO];

    // The radio is gone and every central with it; no unsubscribe follows.
    NSArray<CBCentral *> *centrals = _subscribedCentrals.allValues;
    [_centralSubscriptions removeAllObjects];
    [_subscribedCentrals removeAllObjects];
    for (CBCentral *central in centrals)
        [self notifyServerCentral:central connected:NO];

    // CoreBluetooth dropped the services too; the server stays open for the
    // game to add them again.
    [self resetServerState:error message:message failInFlight:YES];
    [self notifyOperation:@"bt_le_server_services_reset" extraParams:@{}];
}

- (NSString *)convertTo128BitUUID:(NSString *)shortUUID {
    if (shortUUID.length == 4) {  // 16-bit UUID
        NSString *baseUUID = @"00000000-0000-1000-8000-00805F9B34FB";
        NSRange range = NSMakeRange(4, 4);
        NSString *fullUUID = [baseUUID stringByReplacingCharactersInRange:range withString:shortUUID];
        return fullUUID;
    }
    return shortUUID;  // Already a 128-bit UUID or unrecognized format.
}

// CBDescriptor.value is not always NSData: Extended Properties, CCCD and
// Server Configuration come back as NSNumber, User Description as NSString.
- (NSData *)dataFromDescriptorValue:(id)value {
    if ([value isKindOfClass:[NSData class]]) {
        return value;
    }
    if ([value isKindOfClass:[NSNumber class]]) {
        // These descriptors are 16-bit little-endian on the air.
        uint16_t raw = [(NSNumber *)value unsignedShortValue];
        uint8_t bytes[2] = { (uint8_t)(raw & 0xFF), (uint8_t)(raw >> 8) };
        return [NSData dataWithBytes:bytes length:sizeof(bytes)];
    }
    if ([value isKindOfClass:[NSString class]]) {
        NSData *utf8 = [(NSString *)value dataUsingEncoding:NSUTF8StringEncoding];
        return utf8 != nil ? utf8 : [NSData data];
    }
    return [NSData data];
}

// The device a server event's central is. CoreBluetooth gives a central no
// address, only an identifier, so it is named by its device id alone.
- (NSString *) createJSONFromCentral:(CBCentral *)central {
    NSDictionary *centralDictionary = @{
        @"device_id" : [@"apple:ble:" stringByAppendingString:[self centralKey:central]]
    };
    
    NSError *error = nil;
    NSData *jsonData = [NSJSONSerialization dataWithJSONObject:centralDictionary options:0 error:&error];
    
    if (error) {
        NSLog(@"Error serializing JSON: %@", error.localizedDescription);
        return nil;
    }
    
    return [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];
}

// The key a server event names its central by, for the core's server
// connection handles.
- (NSString *) centralKey:(CBCentral *)central {
    NSString *key = central.identifier.UUIDString;
    return key != nil ? key : @"";
}

- (NSString *) subscriptionKey:(CBCharacteristic *)characteristic {
    return [NSString stringWithFormat:@"%@|%@", characteristic.service.UUID.UUIDString, characteristic.UUID.UUIDString];
}

- (void) notifyServerCentral:(CBCentral *)central connected:(BOOL)connected {
    NSMutableDictionary *params = [NSMutableDictionary dictionary];
    params[@"success"] = @(true);
    params[@"connected"] = @(connected);
    params[@"central"] = [self centralKey:central];
    params[@"device"] = [self createJSONFromCentral: central];
    [self notifyOperation:@"bt_le_server_connection_state_changed" extraParams: params];
}

// The core reports a central connected on the first event naming it, so a
// repeated connected:true is harmless.
- (void) peripheralManager:(CBPeripheralManager *)peripheral central:(CBCentral *)central didSubscribeToCharacteristic:(CBCharacteristic *)characteristic {
    NSString *key = [self centralKey:central];
    NSMutableSet<NSString *> *subscriptions = _centralSubscriptions[key];
    if (!subscriptions) {
        subscriptions = [NSMutableSet set];
        _centralSubscriptions[key] = subscriptions;
    }
    [subscriptions addObject:[self subscriptionKey:characteristic]];
    _subscribedCentrals[key] = central;

    [self notifyServerCentral:central connected:YES];
}

// A central's last unsubscribe is the only sign CoreBluetooth gives of it
// leaving: it unsubscribes from everything when its link drops.
- (void) peripheralManager:(CBPeripheralManager *)peripheral central:(CBCentral *)central didUnsubscribeFromCharacteristic:(CBCharacteristic *)characteristic {
    NSString *key = [self centralKey:central];
    NSMutableSet<NSString *> *subscriptions = _centralSubscriptions[key];
    if (!subscriptions) return;

    [subscriptions removeObject:[self subscriptionKey:characteristic]];
    if (subscriptions.count > 0) return;

    [_centralSubscriptions removeObjectForKey:key];
    [_subscribedCentrals removeObjectForKey:key];
    [self notifyServerCentral:central connected:NO];
}

- (void) peripheralManager:(CBPeripheralManager *)peripheral didReceiveReadRequest:(CBATTRequest *)request {
    // An initial value CoreBluetooth could not cache is served here, the way
    // Windows serves StaticValue, and GML never sees the request.
    NSData *initialValue = [_initialValues objectForKey:(CBMutableCharacteristic *)request.characteristic];
    if (initialValue) {
        if (request.offset > initialValue.length) {
            [peripheral respondToRequest:request withResult:CBATTErrorInvalidOffset];
            return;
        }
        request.value = [initialValue subdataWithRange:NSMakeRange(request.offset, initialValue.length - request.offset)];
        [peripheral respondToRequest:request withResult:CBATTErrorSuccess];
        return;
    }

    NSMutableDictionary *params = [NSMutableDictionary dictionary];

    int requestId = [self generateRequestId];
    
    // Storing the CBATTRequest object with the requestId for future use
    _readRequestsLookup[@(requestId)] = request;
    
    // Creating params dictionary
    params[@"request_id"] = @(requestId);
    params[@"service_uuid"] = request.characteristic.service.UUID.UUIDString;
    params[@"characteristic_uuid"] = request.characteristic.UUID.UUIDString;
    // A Read Blob continues a long value; GML answers from this offset on.
    params[@"offset"] = @(request.offset);
    params[@"central"] = [self centralKey:request.central];
    params[@"device"] = [self createJSONFromCentral:request.central];

    // CoreBluetooth never hands the app a descriptor read; only
    // characteristic reads reach here.
    [self notifyOperation:@"bt_le_server_characteristic_read_request" extraParams:params];
}

- (void) peripheralManager:(CBPeripheralManager *)peripheral didReceiveWriteRequests:(NSArray<CBATTRequest *> *)requests {
    if (requests.count == 0) return;

    // Group by characteristic in arrival order: a prepared (long) write comes
    // as several requests on one characteristic, each at its own offset.
    // CoreBluetooth hands the app characteristic writes only; descriptor
    // writes never reach it.
    NSMutableArray<CBCharacteristic *> *order = [NSMutableArray array];
    NSMapTable<CBCharacteristic *, NSMutableArray<CBATTRequest *> *> *groups =
        [NSMapTable mapTableWithKeyOptions:(NSPointerFunctionsStrongMemory | NSPointerFunctionsObjectPointerPersonality)
                              valueOptions:NSPointerFunctionsStrongMemory];
    for (CBATTRequest *request in requests) {
        NSMutableArray<CBATTRequest *> *group = [groups objectForKey:request.characteristic];
        if (!group) {
            group = [NSMutableArray array];
            [groups setObject:group forKey:request.characteristic];
            [order addObject:request.characteristic];
        }
        [group addObject:request];
    }

    // Each group's fragments laid out by offset, from its lowest one, which
    // GML gets as the write's offset.
    NSMutableArray<NSData *> *values = [NSMutableArray array];
    NSMutableArray<NSNumber *> *offsets = [NSMutableArray array];
    for (CBCharacteristic *characteristic in order) {
        NSArray<CBATTRequest *> *group = [groups objectForKey:characteristic];
        NSUInteger start = NSUIntegerMax;
        NSUInteger end = 0;
        for (CBATTRequest *request in group) {
            start = MIN(start, request.offset);
            end = MAX(end, request.offset + request.value.length);
        }
        // 512 bytes is the longest attribute value ATT allows.
        if (end - start > 512) {
            [peripheral respondToRequest:requests.firstObject withResult:CBATTErrorInvalidAttributeValueLength];
            return;
        }
        NSMutableData *value = [NSMutableData dataWithLength:end - start];
        for (CBATTRequest *request in group) {
            if (request.value.length > 0) {
                [value replaceBytesInRange:NSMakeRange(request.offset - start, request.value.length) withBytes:request.value.bytes];
            }
        }
        [values addObject:value];
        [offsets addObject:@(start)];
    }

    GMBTWriteBatch *batch = [GMBTWriteBatch new];
    batch.firstRequest = requests.firstObject;
    batch.remaining = order.count;
    batch.result = CBATTErrorSuccess;

    for (NSUInteger i = 0; i < order.count; ++i) {
        CBCharacteristic *characteristic = order[i];
        int requestId = [self generateRequestId];
        _writeRequestsLookup[@(requestId)] = batch;

        NSMutableDictionary *params = [NSMutableDictionary dictionary];
        params[@"request_id"] = @(requestId);
        params[@"service_uuid"] = characteristic.service.UUID.UUIDString;
        params[@"characteristic_uuid"] = characteristic.UUID.UUIDString;
        params[@"value"] = [values[i] base64EncodedStringWithOptions:0];
        params[@"offset"] = offsets[i];
        // CBATTRequest does not say whether the central asked for a response,
        // so every write is reported as needing one.
        params[@"response_needed"] = @YES;
        CBCentral *central = [groups objectForKey:characteristic].firstObject.central;
        params[@"central"] = [self centralKey:central];
        params[@"device"] = [self createJSONFromCentral:central];

        [self notifyOperation:@"bt_le_server_characteristic_write_request" extraParams:params];
    }
}

- (void) peripheralManager:(CBPeripheralManager *)peripheral didPublishL2CAPChannel:(CBL2CAPPSM)PSM error:(NSError *)error {
    // We won't handle this
}

- (void) peripheralManager:(CBPeripheralManager *)peripheral didUnpublishL2CAPChannel:(CBL2CAPPSM)PSM error:(NSError *)error {
    // We won't handle this
}

- (void) peripheralManager:(CBPeripheralManager *)peripheral didOpenL2CAPChannel:(CBL2CAPChannel *)channel error:(NSError *)error API_AVAILABLE(ios(11.0)){
    // We won't handle this
}

// ####################################################################################
// # CLIENT
// ####################################################################################

static gmbluetooth::Error gmbt_not_open(std::string &message) {
    return gmbt_fail(message, gmbluetooth::Error::OperationFailed, "The LE connection is not open");
}

// The attribute is named by the id its discovery reported; CoreBluetooth
// has no handle to show instead.
static gmbluetooth::Error gmbt_not_found(const char *what, std::uint64_t attributeId, std::string &message) {
    return gmbt_fail(message, gmbluetooth::Error::NotFound,
                     std::string(what) + " not found (instance " + std::to_string(attributeId) + ")");
}

// Whether array holds this very object. nil-safe: a nil array would
// otherwise answer 0, a valid index.
static BOOL gmbt_contains_identical(NSArray *array, id object) {
    return array != nil && object != nil && [array indexOfObjectIdenticalTo:object] != NSNotFound;
}

static gmbluetooth::Error gmbt_bad_base64(std::string &message) {
    return gmbt_fail(message, gmbluetooth::Error::InvalidArgument, "The value is not valid base64");
}

// Issues the head of the connect queue once the central can take it. A head
// with a timer was already issued.
- (void) handleOpenPeripheralQueue {
    if (!_centralManager || _centralManager.state != CBManagerStatePoweredOn) return;

    GMBTQueuedTimedPeripheral *queued = [self queuePeek:_openPeripheralQueue];
    if (!queued || queued.timer) return;

    [_centralManager connectPeripheral:queued.peripheral options:nil];
    queued.timer = [NSTimer scheduledTimerWithTimeInterval:kGMBTConnectTimeoutSeconds
                                                    target:self
                                                  selector:@selector(connectionDidTimeout:)
                                                  userInfo:queued
                                                   repeats:NO];
}

- (GMBTQueuedTimedPeripheral *) queuedOpenFor:(CBPeripheral *)peripheral {
    for (GMBTQueuedTimedPeripheral *queued in _openPeripheralQueue) {
        if (queued.peripheral == peripheral) return queued;
    }
    return nil;
}

// Takes a connect out of the queue and stops its timer; cancel also stops
// the attempt CoreBluetooth is making for it. The caller issues the next.
- (void) removeQueuedOpen:(GMBTQueuedTimedPeripheral *)queued cancel:(BOOL)cancel {
    const BOOL issued = queued.timer != nil;
    [queued.timer invalidate];
    [_openPeripheralQueue removeObjectIdenticalTo:queued];
    if (cancel && issued && _centralManager.state == CBManagerStatePoweredOn)
        [_centralManager cancelPeripheralConnection:queued.peripheral];
}

// Fails every connect still waiting, for a central that left PoweredOn.
- (void) failQueuedOpens:(gmbluetooth::Error)error message:(const std::string &)message {
    NSArray<GMBTQueuedTimedPeripheral *> *queued = [_openPeripheralQueue copy];
    [_openPeripheralQueue removeAllObjects];
    for (GMBTQueuedTimedPeripheral *entry in queued) {
        [entry.timer invalidate];
        [self notifyOpen:entry.peripheral errorCode:@((int)error) error:error message:message];
    }
}

- (void) handleFetchServicesQueue:(NSMutableArray *)queue {
    [self handleQueue:queue withBlock:^(GMBTQueuedPeripheral *queuedPeripheral) {
        [queuedPeripheral.peripheral discoverServices: nil];
    }];
}

- (void) handleFetchCharacteristicsQueue:(NSMutableArray *)queue {
    [self handleQueue:queue withBlock:^(GMBTQueuedService *queueService) {
        [queueService.peripheral discoverCharacteristics:nil forService:queueService.service];
    }];
}

- (void) handleFetchDescriptorsQueue:(NSMutableArray *)queue {
    [self handleQueue:queue withBlock:^(GMBTQueuedCharacteristic *queuedCharacteristic) {
        [queuedCharacteristic.peripheral discoverDescriptorsForCharacteristic: queuedCharacteristic.characteristic];
    }];
}

- (void) handleReadCharacteristicQueue:(NSMutableArray *)queue {
    [self handleQueue:queue withBlock:^(GMBTQueuedCharacteristic *queuedCharacteristic) {
        [queuedCharacteristic.peripheral readValueForCharacteristic: queuedCharacteristic.characteristic];
    }];
}

- (void) handleWriteCharacteristicQueue:(NSMutableArray *)queue {
    [self handleQueue:queue withBlock:^(GMBTQueuedCharacteristicWithData *queuedCharacteristicData) {
        [queuedCharacteristicData.peripheral writeValue:queuedCharacteristicData.data forCharacteristic:queuedCharacteristicData.characteristic type: CBCharacteristicWriteWithResponse];
    }];
}

- (void) handleNotifyCharacteristicQueue:(NSMutableArray *)queue {
    [self handleQueue:queue withBlock:^(GMBTQueuedSubscription *queuedSubscription) {
        [queuedSubscription.peripheral setNotifyValue:(queuedSubscription.mode != 0) forCharacteristic:queuedSubscription.characteristic];
    }];
}

- (void) handleReadDescriptorQueue:(NSMutableArray *)queue {
    [self handleQueue:queue withBlock:^(GMBTQueuedDescriptor *queuedDescriptor) {
        [queuedDescriptor.peripheral readValueForDescriptor: queuedDescriptor.descriptor];
    }];
}

- (void) handleWriteDescriptorQueue:(NSMutableArray *)queue {
    [self handleQueue:queue withBlock:^(GMBTQueuedDescriptorWithData *queuedDescriptorWithData) {
        [queuedDescriptorWithData.peripheral writeValue:queuedDescriptorWithData.data forDescriptor:queuedDescriptorWithData.descriptor];
    }];
}

- (void) handleReadRssiQueue:(NSMutableArray *)queue {
    [self handleQueue:queue withBlock:^(GMBTQueuedPeripheral *queuedPeripheral) {
        [queuedPeripheral.peripheral readRSSI];
    }];
}

// Hands queued writes without response to CoreBluetooth while it can take
// them, completing each as it goes; peripheralIsReadyToSendWriteWithoutResponse:
// resumes. Before iOS 11 / macOS 10.13 there is no flow control to wait for.
- (void) sendWritesWithoutResponse:(CBPeripheral *)peripheral {
    NSMutableArray *queue = [self queuesForPeripheral:peripheral create:NO].writeWithoutResponse;
    while (queue.count > 0) {
        if (@available(iOS 11.0, macOS 10.13, *)) {
            if (!peripheral.canSendWriteWithoutResponse) return;
        }
        GMBTQueuedCharacteristicWithData *queued = [self queueDequeue:queue];
        [peripheral writeValue:queued.data forCharacteristic:queued.characteristic type:CBCharacteristicWriteWithoutResponse];
        [self completeOp:queued.opId error:nil];
    }
}

- (CBPeripheral *) peripheralForUuid:(NSString *)peripheralUuid {
    return [_openedPeripherals objectForKey:[peripheralUuid uppercaseString]];
}

- (NSData *) dataFromBase64:(NSString *)value {
    return [[NSData alloc] initWithBase64EncodedString:(value ? value : @"") options:0];
}

// Busy while the peripheral is open or connecting. A peripheral no scan has
// reported since the central last powered on may still be known to
// CoreBluetooth by its identifier; NotFound when it is not.
- (gmbluetooth::Error) bt_le_peripheral_open:(NSString*) peripheralUuid message:(std::string &)message {
    const gmbluetooth::Error stateError = gmbt_require_powered_on(_centralManager.state, message);
    if (stateError != gmbluetooth::Error::Ok) return stateError;

    NSString *key = [peripheralUuid uppercaseString];
    if ([self peripheralIsInUse:key])
        return gmbt_fail(message, gmbluetooth::Error::Busy, "The device already has a connection");

    CBPeripheral *peripheral = _discoveredPeripherals[key];
    if (!peripheral && _centralManager.state == CBManagerStatePoweredOn) {
        NSUUID *identifier = [[NSUUID alloc] initWithUUIDString:peripheralUuid];
        if (identifier) {
            peripheral = [_centralManager retrievePeripheralsWithIdentifiers:@[ identifier ]].firstObject;
            if (peripheral) [self rememberPeripheral:peripheral];
        }
    }
    if (!peripheral)
        return gmbt_fail(message, gmbluetooth::Error::NotFound, "The device is not known to CoreBluetooth; scan for it again");

    // No op id: the core matches the open by the peripheral's connection.
    GMBTQueuedTimedPeripheral *queuePeripheral = [[GMBTQueuedTimedPeripheral alloc] initWithOpId:nil peripheral:peripheral];
    [_openPeripheralQueue addObject:queuePeripheral];
    [self handleOpenPeripheralQueue];

    return gmbt_ok(message);
}

- (BOOL) bt_le_peripheral_is_connected:(NSString*) peripheralUuid {
    return [_connectedPeripherals objectForKey:[peripheralUuid uppercaseString]] != nil;
}

// The ATT MTU CoreBluetooth negotiated: the longest write without response
// plus the 3-byte ATT header. 23, the minimum, when the link is not up.
- (NSInteger) bt_le_peripheral_mtu:(NSString *)peripheralUuid {
    CBPeripheral *peripheral = [_connectedPeripherals objectForKey:[peripheralUuid uppercaseString]];
    if (!peripheral || peripheral.state != CBPeripheralStateConnected) return 23;
    if (@available(iOS 9.0, macOS 10.12, *)) {
        return (NSInteger)[peripheral maximumWriteValueLengthForType:CBCharacteristicWriteWithoutResponse] + 3;
    }
    return 23;
}

- (gmbluetooth::Error) bt_le_peripheral_read_rssi:(NSString *)peripheralUuid opId:(NSNumber *)opId message:(std::string &)message {
    CBPeripheral *peripheral = [self peripheralForUuid:peripheralUuid];
    if (!peripheral) return gmbt_not_open(message);

    GMBTQueuedPeripheral *queuedPeripheral = [[GMBTQueuedPeripheral alloc] initWithOpId:opId peripheral:peripheral];

    NSMutableArray *queue = [self queuesForPeripheral:peripheral create:YES].readRssi;
    [self queueEnqueue:queue value:queuedPeripheral withHandler:^{ [self handleReadRssiQueue:queue]; }];

    return gmbt_ok(message);
}

// The device queries ask the central for what it already knows, so it has
// to be on: no request waits for it here.
- (gmbluetooth::Error) requireCentralPoweredOn:(std::string &)message {
    const gmbluetooth::Error error = gmbt_require_powered_on(_centralManager.state, message);
    if (error != gmbluetooth::Error::Ok) return error;
    if (_centralManager.state != CBManagerStatePoweredOn)
        return gmbt_fail(message, gmbluetooth::Error::BluetoothDisabled, "Bluetooth is not powered on yet");
    return gmbt_ok(message);
}

// A peripheral CoreBluetooth knows by its identifier, kept for
// bt_le_peripheral_open; nil with error set when it knows none.
- (CBPeripheral *) bt_le_retrieve_peripheral:(NSUUID *)identifier error:(gmbluetooth::Error &)error message:(std::string &)message {
    error = [self requireCentralPoweredOn:message];
    if (error != gmbluetooth::Error::Ok) return nil;

    CBPeripheral *peripheral = [_centralManager retrievePeripheralsWithIdentifiers:@[ identifier ]].firstObject;
    if (!peripheral) {
        error = gmbt_fail(message, gmbluetooth::Error::NotFound, "The system does not know this peripheral");
        return nil;
    }
    [self rememberPeripheral:peripheral];
    error = gmbt_ok(message);
    return peripheral;
}

// The peripherals connected to the system (by any app) that host one of the
// services, each kept for bt_le_peripheral_open. The core validated the
// UUIDs, so UUIDWithString cannot throw.
- (NSArray<CBPeripheral *> *) bt_le_connected_peripherals:(NSArray<NSString *> *)serviceUuidStrings error:(gmbluetooth::Error &)error message:(std::string &)message {
    error = [self requireCentralPoweredOn:message];
    if (error != gmbluetooth::Error::Ok) return nil;

    NSMutableArray<CBUUID *> *services = [NSMutableArray arrayWithCapacity:serviceUuidStrings.count];
    for (NSString *uuidString in serviceUuidStrings) [services addObject:[CBUUID UUIDWithString:uuidString]];

    NSArray<CBPeripheral *> *peripherals = [_centralManager retrieveConnectedPeripheralsWithServices:services];
    for (CBPeripheral *peripheral in peripherals) [self rememberPeripheral:peripheral];
    error = gmbt_ok(message);
    return peripherals ? peripherals : @[];
}

// Synchronous and silent: the core retires the connection itself. A
// peripheral still connecting leaves the queue, and the next connect goes.
- (gmbluetooth::Error) bt_le_peripheral_close:(NSString*) peripheralUuid message:(std::string &)message {
    NSString *key = [peripheralUuid uppercaseString];
    CBPeripheral *peripheral = [_openedPeripherals objectForKey:key];
    if (peripheral) {
        [_openedPeripherals removeObjectForKey:key];
        [_connectedPeripherals removeObjectForKey:key];
        [self dropQueuesForPeripheral:peripheral];
        [_centralManager cancelPeripheralConnection:peripheral];
        return gmbt_ok(message);
    }

    GMBTQueuedTimedPeripheral *queued = nil;
    for (GMBTQueuedTimedPeripheral *entry in _openPeripheralQueue) {
        if ([[self peripheralKey:entry.peripheral] isEqualToString:key]) {
            queued = entry;
            break;
        }
    }
    if (queued) {
        [self removeQueuedOpen:queued cancel:YES];
        [self handleOpenPeripheralQueue];
    }
    return gmbt_ok(message);
}

- (gmbluetooth::Error) bt_le_peripheral_get_services:(NSString*) peripheralUuid opId:(NSNumber *)opId message:(std::string &)message {
    CBPeripheral *peripheral = [self peripheralForUuid:peripheralUuid];
    if (!peripheral) return gmbt_not_open(message);

    GMBTQueuedPeripheral *queuePeripheral = [[GMBTQueuedPeripheral alloc] initWithOpId:opId peripheral:peripheral];

    NSMutableArray *queue = [self queuesForPeripheral:peripheral create:YES].fetchServices;
    [self queueEnqueue:queue value:queuePeripheral withHandler:^{ [self handleFetchServicesQueue:queue]; }];

    return gmbt_ok(message);
}

// The attribute an id names on the peripheral when it is of class kind; nil
// for an id its discovery never reported, or one of another kind.
- (id) attributeOf:(CBPeripheral *)peripheral withId:(std::uint64_t)attributeId kind:(Class)kind {
    CBAttribute *attribute = [[self queuesForPeripheral:peripheral create:NO] attributeForId:attributeId];
    return [attribute isKindOfClass:kind] ? attribute : nil;
}

// The lookups by id answer only while the object is still under its parent:
// one the peripheral has since dropped or replaced (didModifyServices) is
// not found, as an unknown UUID was.
- (CBService *) findServiceInPeripheral:(CBPeripheral *)peripheral withId:(std::uint64_t)serviceId {
    CBService *service = [self attributeOf:peripheral withId:serviceId kind:[CBService class]];
    return gmbt_contains_identical(peripheral.services, service) ? service : nil;
}

- (gmbluetooth::Error) bt_le_service_get_characteristics:(NSString*) peripheralUuid service:(std::uint64_t) serviceId opId:(NSNumber *)opId message:(std::string &)message {
    CBPeripheral *peripheral = [self peripheralForUuid:peripheralUuid];
    if (!peripheral) return gmbt_not_open(message);

    CBService *service = [self findServiceInPeripheral:peripheral withId:serviceId];
    if (!service) return gmbt_not_found("Service", serviceId, message);

    GMBTQueuedService *queuedService = [[GMBTQueuedService alloc] initWithOpId:opId peripheral:peripheral service:service];

    NSMutableArray *queue = [self queuesForPeripheral:peripheral create:YES].fetchCharacteristics;
    [self queueEnqueue:queue value:queuedService withHandler:^{ [self handleFetchCharacteristicsQueue:queue]; }];

    return gmbt_ok(message);
}

- (CBCharacteristic *) findCharacteristicInService:(CBService *)service ofPeripheral:(CBPeripheral *)peripheral withId:(std::uint64_t)characteristicId {
    CBCharacteristic *characteristic = [self attributeOf:peripheral withId:characteristicId kind:[CBCharacteristic class]];
    return gmbt_contains_identical(service.characteristics, characteristic) ? characteristic : nil;
}

// The characteristic a GATT call names on an open peripheral, or nil with
// the call's error and message set.
- (CBCharacteristic *) characteristicFor:(NSString *)peripheralUuid service:(std::uint64_t)serviceId characteristic:(std::uint64_t)characteristicId error:(gmbluetooth::Error &)error message:(std::string &)message {
    CBPeripheral *peripheral = [self peripheralForUuid:peripheralUuid];
    if (!peripheral) {
        error = gmbt_not_open(message);
        return nil;
    }
    CBService *service = [self findServiceInPeripheral:peripheral withId:serviceId];
    if (!service) {
        error = gmbt_not_found("Service", serviceId, message);
        return nil;
    }
    CBCharacteristic *characteristic = [self findCharacteristicInService:service ofPeripheral:peripheral withId:characteristicId];
    if (!characteristic) {
        error = gmbt_not_found("Characteristic", characteristicId, message);
        return nil;
    }
    error = gmbt_ok(message);
    return characteristic;
}

- (gmbluetooth::Error) bt_le_characteristic_get_descriptors:(NSString*) peripheralUuid service:(std::uint64_t) serviceId characteristic:(std::uint64_t) characteristicId opId:(NSNumber *)opId message:(std::string &)message {
    gmbluetooth::Error error = gmbluetooth::Error::Ok;
    CBCharacteristic *characteristic = [self characteristicFor:peripheralUuid service:serviceId characteristic:characteristicId error:error message:message];
    if (!characteristic) return error;
    CBPeripheral *peripheral = characteristic.service.peripheral;

    GMBTQueuedCharacteristic *queuedCharacteristic = [[GMBTQueuedCharacteristic alloc] initWithOpId:opId peripheral:peripheral characteristic:characteristic];

    NSMutableArray *queue = [self queuesForPeripheral:peripheral create:YES].fetchDescriptors;
    [self queueEnqueue:queue value:queuedCharacteristic withHandler:^{ [self handleFetchDescriptorsQueue:queue]; }];

    return gmbt_ok(message);
}

- (gmbluetooth::Error) bt_le_characteristic_read:(NSString*) peripheralUuid service:(std::uint64_t) serviceId characteristic:(std::uint64_t) characteristicId opId:(NSNumber *)opId message:(std::string &)message {
    gmbluetooth::Error error = gmbluetooth::Error::Ok;
    CBCharacteristic *characteristic = [self characteristicFor:peripheralUuid service:serviceId characteristic:characteristicId error:error message:message];
    if (!characteristic) return error;
    CBPeripheral *peripheral = characteristic.service.peripheral;

    GMBTQueuedCharacteristic *queuedCharacteristic = [[GMBTQueuedCharacteristic alloc] initWithOpId:opId peripheral:peripheral characteristic:characteristic];

    NSMutableArray *queue = [self queuesForPeripheral:peripheral create:YES].readCharacteristic;
    [self queueEnqueue:queue value:queuedCharacteristic withHandler:^{ [self handleReadCharacteristicQueue:queue]; }];

    return gmbt_ok(message);
}

- (gmbluetooth::Error) bt_le_characteristic_write_request:(NSString*) peripheralUuid service:(std::uint64_t) serviceId characteristic:(std::uint64_t) characteristicId value:(NSString*) value opId:(NSNumber *)opId message:(std::string &)message {
    gmbluetooth::Error error = gmbluetooth::Error::Ok;
    CBCharacteristic *characteristic = [self characteristicFor:peripheralUuid service:serviceId characteristic:characteristicId error:error message:message];
    if (!characteristic) return error;
    CBPeripheral *peripheral = characteristic.service.peripheral;

    NSData *data = [self dataFromBase64:value];
    if (!data) return gmbt_bad_base64(message);

    GMBTQueuedCharacteristicWithData *queuedCharacteristicData = [[GMBTQueuedCharacteristicWithData alloc] initWithOpId:opId peripheral:peripheral characteristic:characteristic data:data];

    NSMutableArray *queue = [self queuesForPeripheral:peripheral create:YES].writeCharacteristic;
    [self queueEnqueue:queue value:queuedCharacteristicData withHandler:^{ [self handleWriteCharacteristicQueue:queue]; }];

    return gmbt_ok(message);
}

// CoreBluetooth sends no delegate call for a write without response, so each
// completes when it is handed over: now, or from the queue once the link
// can take it.
- (gmbluetooth::Error) bt_le_characteristic_write_command:(NSString*) peripheralUuid service:(std::uint64_t) serviceId characteristic:(std::uint64_t) characteristicId value:(NSString*) value opId:(NSNumber *)opId message:(std::string &)message {
    gmbluetooth::Error error = gmbluetooth::Error::Ok;
    CBCharacteristic *characteristic = [self characteristicFor:peripheralUuid service:serviceId characteristic:characteristicId error:error message:message];
    if (!characteristic) return error;
    CBPeripheral *peripheral = characteristic.service.peripheral;

    if (!(characteristic.properties & CBCharacteristicPropertyWriteWithoutResponse))
        return gmbt_fail(message, gmbluetooth::Error::NotSupported, "The characteristic does not take writes without response");

    NSData *data = [self dataFromBase64:value];
    if (!data) return gmbt_bad_base64(message);

    if (@available(iOS 9.0, macOS 10.12, *)) {
        const NSUInteger maximum = [peripheral maximumWriteValueLengthForType:CBCharacteristicWriteWithoutResponse];
        if (data.length > maximum) {
            return gmbt_fail(message, gmbluetooth::Error::InvalidArgument,
                             "A write without response takes at most " + std::to_string((unsigned long)maximum) + " bytes on this link");
        }
    }

    NSMutableArray *queue = [self queuesForPeripheral:peripheral create:YES].writeWithoutResponse;
    if (queue.count >= kGMBTMaxQueuedSends)
        return gmbt_fail(message, gmbluetooth::Error::Busy, "Too many writes without response are waiting to be sent");

    GMBTQueuedCharacteristicWithData *queued = [[GMBTQueuedCharacteristicWithData alloc] initWithOpId:opId peripheral:peripheral characteristic:characteristic data:data];
    [queue addObject:queued];
    [self sendWritesWithoutResponse:peripheral];

    return gmbt_ok(message);
}

- (void) peripheralIsReadyToSendWriteWithoutResponse:(CBPeripheral *)peripheral {
    [self sendWritesWithoutResponse:peripheral];
}

- (gmbluetooth::Error) bt_le_characteristic_subscribe:(NSString*) peripheralUuid service:(std::uint64_t) serviceId characteristic:(std::uint64_t) characteristicId mode:(NSInteger)mode opId:(NSNumber *)opId message:(std::string &)message {
    gmbluetooth::Error error = gmbluetooth::Error::Ok;
    CBCharacteristic *characteristic = [self characteristicFor:peripheralUuid service:serviceId characteristic:characteristicId error:error message:message];
    if (!characteristic) return error;
    CBPeripheral *peripheral = characteristic.service.peripheral;

    GMBTQueuedSubscription *queuedSubscription = [[GMBTQueuedSubscription alloc] initWithOpId:opId peripheral:peripheral characteristic:characteristic mode:mode];

    NSMutableArray *queue = [self queuesForPeripheral:peripheral create:YES].notifyCharacteristic;
    [self queueEnqueue:queue value:queuedSubscription withHandler:^{ [self handleNotifyCharacteristicQueue:queue]; }];

    return gmbt_ok(message);
}

- (CBDescriptor *) findDescriptorInCharacteristic:(CBCharacteristic *)characteristic ofPeripheral:(CBPeripheral *)peripheral withId:(std::uint64_t)descriptorId {
    CBDescriptor *descriptor = [self attributeOf:peripheral withId:descriptorId kind:[CBDescriptor class]];
    return gmbt_contains_identical(characteristic.descriptors, descriptor) ? descriptor : nil;
}

- (gmbluetooth::Error) bt_le_descriptor_read:(NSString*) peripheralUuid service:(std::uint64_t) serviceId characteristic:(std::uint64_t) characteristicId descriptor:(std::uint64_t) descriptorId opId:(NSNumber *)opId message:(std::string &)message {
    gmbluetooth::Error error = gmbluetooth::Error::Ok;
    CBCharacteristic *characteristic = [self characteristicFor:peripheralUuid service:serviceId characteristic:characteristicId error:error message:message];
    if (!characteristic) return error;
    CBPeripheral *peripheral = characteristic.service.peripheral;

    CBDescriptor *descriptor = [self findDescriptorInCharacteristic:characteristic ofPeripheral:peripheral withId:descriptorId];
    if (!descriptor) return gmbt_not_found("Descriptor", descriptorId, message);

    GMBTQueuedDescriptor *queuedDescriptor = [[GMBTQueuedDescriptor alloc] initWithOpId:opId peripheral:peripheral descriptor:descriptor];

    NSMutableArray *queue = [self queuesForPeripheral:peripheral create:YES].readDescriptor;
    [self queueEnqueue:queue value:queuedDescriptor withHandler:^{ [self handleReadDescriptorQueue:queue]; }];

    return gmbt_ok(message);
}

- (gmbluetooth::Error) bt_le_descriptor_write:(NSString*) peripheralUuid service:(std::uint64_t) serviceId characteristic:(std::uint64_t) characteristicId descriptor:(std::uint64_t) descriptorId value:(NSString*) value opId:(NSNumber *)opId message:(std::string &)message {
    gmbluetooth::Error error = gmbluetooth::Error::Ok;
    CBCharacteristic *characteristic = [self characteristicFor:peripheralUuid service:serviceId characteristic:characteristicId error:error message:message];
    if (!characteristic) return error;
    CBPeripheral *peripheral = characteristic.service.peripheral;

    CBDescriptor *descriptor = [self findDescriptorInCharacteristic:characteristic ofPeripheral:peripheral withId:descriptorId];
    if (!descriptor) return gmbt_not_found("Descriptor", descriptorId, message);

    NSData *data = [self dataFromBase64:value];
    if (!data) return gmbt_bad_base64(message);

    GMBTQueuedDescriptorWithData *queuedDescriptorWithData = [[GMBTQueuedDescriptorWithData alloc] initWithOpId:opId peripheral:peripheral descriptor:descriptor data:data];

    NSMutableArray *queue = [self queuesForPeripheral:peripheral create:YES].writeDescriptor;
    [self queueEnqueue:queue value:queuedDescriptorWithData withHandler:^{ [self handleWriteDescriptorQueue:queue]; }];

    return gmbt_ok(message);
}

// Only a connect still queued is reported. Any other connect - one the game
// closed while it was connecting, or one that timed out just before this -
// is cancelled again, unless the peripheral is open.
- (void) centralManager:(CBCentralManager *)central didConnectPeripheral:(CBPeripheral *)peripheral {
    NSString *key = [self peripheralKey:peripheral];
    GMBTQueuedTimedPeripheral *queuedPeripheral = [self queuedOpenFor:peripheral];
    if (!queuedPeripheral) {
        if (!_openedPeripherals[key]) [central cancelPeripheralConnection:peripheral];
        return;
    }
    [self removeQueuedOpen:queuedPeripheral cancel:NO];

    peripheral.delegate = self;

    [_openedPeripherals setObject:peripheral forKey:key];
    [_connectedPeripherals setObject:peripheral forKey:key];

    [self notifyOpen:peripheral errorCode:nil error:gmbluetooth::Error::Ok message:std::string()];
    [self handleOpenPeripheralQueue];
}

- (void) centralManager:(CBCentralManager *)central didFailToConnectPeripheral:(CBPeripheral *)peripheral error:(NSError *)error {
    GMBTQueuedTimedPeripheral *queuedPeripheral = [self queuedOpenFor:peripheral];
    if (!queuedPeripheral) return;
    [self removeQueuedOpen:queuedPeripheral cancel:NO];

    // A connect that failed for no more specific reason is ConnectionFailed.
    std::string message = "LE connect failed";
    gmbluetooth::Error failure = gmbluetooth::Error::ConnectionFailed;
    if (error) {
        failure = gmbt_error_from_nserror(error, message);
        if (failure == gmbluetooth::Error::OperationFailed) failure = gmbluetooth::Error::ConnectionFailed;
    }

    [self notifyOpen:peripheral errorCode:@(error ? (int)error.code : 0) error:failure message:message];
    [self handleOpenPeripheralQueue];
}

// timer.userInfo is the connect it bounds; one that already left the queue
// has had its answer.
- (void) connectionDidTimeout:(NSTimer *)timer {
    GMBTQueuedTimedPeripheral *queuedPeripheral = timer.userInfo;
    if (![queuedPeripheral isKindOfClass:[GMBTQueuedTimedPeripheral class]] ||
        [_openPeripheralQueue indexOfObjectIdenticalTo:queuedPeripheral] == NSNotFound)
        return;

    [self removeQueuedOpen:queuedPeripheral cancel:YES];
    [self notifyOpen:queuedPeripheral.peripheral errorCode:@133 error:gmbluetooth::Error::Timeout message:"LE connect timed out"];
    [self handleOpenPeripheralQueue];
}

- (void) centralManager:(CBCentralManager *)central didDisconnectPeripheral:(CBPeripheral *)peripheral error:(NSError *)error {
    NSString *key = [self peripheralKey:peripheral];
    [self dropQueuesForPeripheral:peripheral];

    // Not open any more: the game closed it (bt_le_peripheral_close), the
    // central reported it when it left PoweredOn, or it never finished
    // opening. Each of those has already been reported.
    if (key.length == 0 || !_openedPeripherals[key]) return;

    [_openedPeripherals removeObjectForKey:key];
    [_connectedPeripherals removeObjectForKey:key];

    // The address lets on_event name the connection, so the core can fail the
    // ops that were queued on it.
    NSMutableDictionary* params = [[NSMutableDictionary alloc] init];
    params[@"address"] = key;
    params[@"error_code"] = @(error ? (int)error.code : 0);
    // A disconnect the game asked for (bt_le_peripheral_close) never gets
    // here, so this is always the link going away.
    std::string message = "The peripheral disconnected";
    if (error) (void)gmbt_error_from_nserror(error, message);
    params[@"error"] = @((int)gmbluetooth::Error::Disconnected);
    params[@"message"] = gmbt_ns(message);
    [self notifyOperation:@"bt_le_peripheral_disconnect" extraParams:params];
}

- (void) peripheral:(CBPeripheral *)peripheral didDiscoverServices:(NSError *)error {
	
    GMBTPeripheralQueues *queues = [self queuesForPeripheral:peripheral create:NO];
    NSMutableArray *queue = queues.fetchServices;
    GMBTQueuedPeripheral *queuedPeripheral = [self takeHeadOf:queue answering:@"didDiscoverServices" matching:^BOOL(id head) {
        return ((GMBTQueuedPeripheral *)head).peripheral == peripheral;
    }];
    if (!queuedPeripheral) return;

    if (error) [self completeOp:queuedPeripheral.opId error:error];
    else [self completeOp:queuedPeripheral.opId error:nil result:[self resultFromServices:peripheral.services queues:queues]];
    [self handleFetchServicesQueue:queue];
}
 
- (void) peripheral:(CBPeripheral *)peripheral didDiscoverIncludedServicesForService:(CBService *)service error:(NSError *)error {
    // This won't be handled
}

- (void) peripheral:(CBPeripheral *)peripheral didDiscoverCharacteristicsForService:(CBService *)service error:(NSError *)error {
    
    GMBTPeripheralQueues *queues = [self queuesForPeripheral:peripheral create:NO];
    NSMutableArray *queue = queues.fetchCharacteristics;
    GMBTQueuedService *queuedService = [self takeHeadOf:queue answering:@"didDiscoverCharacteristicsForService" matching:^BOOL(id head) {
        return ((GMBTQueuedService *)head).service == service;
    }];
    if (!queuedService) return;

    if (error) [self completeOp:queuedService.opId error:error];
    else [self completeOp:queuedService.opId error:nil result:[self resultFromCharacteristics:service.characteristics queues:queues]];

    // Handle next task in queue if there is one
    [self handleFetchCharacteristicsQueue:queue];
}
 
- (void) peripheral:(CBPeripheral *)peripheral didDiscoverDescriptorsForCharacteristic:(CBCharacteristic *)characteristic error:(NSError *)error {
	
    GMBTPeripheralQueues *queues = [self queuesForPeripheral:peripheral create:NO];
    NSMutableArray *queue = queues.fetchDescriptors;
    GMBTQueuedCharacteristic *queuedCharacteristic = [self takeHeadOf:queue answering:@"didDiscoverDescriptorsForCharacteristic" matching:^BOOL(id head) {
        return ((GMBTQueuedCharacteristic *)head).characteristic == characteristic;
    }];
    if (!queuedCharacteristic) return;

    if (error) [self completeOp:queuedCharacteristic.opId error:error];
    else [self completeOp:queuedCharacteristic.opId error:nil result:[self resultFromDescriptors:characteristic.descriptors queues:queues]];
    [self handleFetchDescriptorsQueue:queue];
}

- (void) peripheral:(CBPeripheral *)peripheral didUpdateNotificationStateForCharacteristic:(CBCharacteristic *)characteristic error:(NSError *)error {
    
    NSMutableArray *queue = [self queuesForPeripheral:peripheral create:NO].notifyCharacteristic;
    GMBTQueuedSubscription *queuedSubscription = [self takeHeadOf:queue answering:@"didUpdateNotificationStateForCharacteristic" matching:^BOOL(id head) {
        return ((GMBTQueuedSubscription *)head).characteristic == characteristic;
    }];
    if (!queuedSubscription) return;

    [self completeOp:queuedSubscription.opId error:error];

    [self handleNotifyCharacteristicQueue:queue];
}

- (void) peripheral:(CBPeripheral *)peripheral didUpdateValueForCharacteristic:(CBCharacteristic *)characteristic error:(NSError *)error {
        
    // Convert the value to a base64 string
    NSString *valueString = [characteristic.value base64EncodedStringWithOptions:0];
    
    GMBTPeripheralQueues *queues = [self queuesForPeripheral:peripheral create:NO];
    NSMutableArray *queue = queues.readCharacteristic;
    GMBTQueuedCharacteristic *queuedCharacteristic = [self queuePeek:queue];

    if (queuedCharacteristic.characteristic != characteristic) {
        queuedCharacteristic = nil;
    }
    else [self queueDequeue:queue];

    // There was not queued read request that matches the characteristic so it's a notification
    if (queuedCharacteristic == nil) {
        // Named by the ids its discovery reported. A characteristic no
        // discovery on this link reported has no handle in the core and is
        // dropped, as an unknown one always was.
        const std::uint64_t serviceId = [queues knownIdForAttribute:characteristic.service];
        const std::uint64_t characteristicId = [queues knownIdForAttribute:characteristic];
        if (serviceId == 0 || characteristicId == 0) {
            GMBT_TRACE("value change on a characteristic no discovery reported - dropped");
            return;
        }

        NSMutableDictionary *params = [[NSMutableDictionary alloc] init];
        params[@"service_instance"] = @(serviceId);
        params[@"characteristic_instance"] = @(characteristicId);
        params[@"address"] = [self peripheralKey:peripheral];
        params[@"value"] = valueString;

        [self notifyOperation:@"bt_le_characteristic_value_changed" extraParams:params];
        return;
    }

    if (error) [self completeOp:queuedCharacteristic.opId error:error];
    else [self completeOp:queuedCharacteristic.opId error:nil result:[self resultFromValue:characteristic.value]];

    [self handleReadCharacteristicQueue:queue];
}

- (void) peripheral:(CBPeripheral *)peripheral didWriteValueForCharacteristic:(CBCharacteristic *)characteristic error:(NSError *)error {

    NSMutableArray *queue = [self queuesForPeripheral:peripheral create:NO].writeCharacteristic;
    GMBTQueuedCharacteristicWithData *queuedCharacteristicWithData = [self takeHeadOf:queue answering:@"didWriteValueForCharacteristic" matching:^BOOL(id head) {
        return ((GMBTQueuedCharacteristicWithData *)head).characteristic == characteristic;
    }];
    if (!queuedCharacteristicWithData) return;

    [self completeOp:queuedCharacteristicWithData.opId error:error];

    [self handleWriteCharacteristicQueue:queue];
}

- (void) peripheral:(CBPeripheral *)peripheral didUpdateValueForDescriptor:(CBDescriptor *)descriptor error:(nullable NSError *)error {

    NSMutableArray *queue = [self queuesForPeripheral:peripheral create:NO].readDescriptor;
    GMBTQueuedDescriptor *queuedDescriptor = [self takeHeadOf:queue answering:@"didUpdateValueForDescriptor" matching:^BOOL(id head) {
        return ((GMBTQueuedDescriptor *)head).descriptor == descriptor;
    }];
    if (!queuedDescriptor) return;

    if (error) [self completeOp:queuedDescriptor.opId error:error];
    else [self completeOp:queuedDescriptor.opId error:nil result:[self resultFromValue:[self dataFromDescriptorValue:descriptor.value]]];

    [self handleReadDescriptorQueue:queue];
}

- (void) peripheral:(CBPeripheral *)peripheral didWriteValueForDescriptor:(CBDescriptor *)descriptor error:(nullable NSError *)error {

    NSMutableArray *queue = [self queuesForPeripheral:peripheral create:NO].writeDescriptor;
    GMBTQueuedDescriptorWithData *queuedDescriptorWithData = [self takeHeadOf:queue answering:@"didWriteValueForDescriptor" matching:^BOOL(id head) {
        return ((GMBTQueuedDescriptorWithData *)head).descriptor == descriptor;
    }];
    if (!queuedDescriptorWithData) return;

    [self completeOp:queuedDescriptorWithData.opId error:error];

    [self handleWriteDescriptorQueue:queue];
}

- (void) centralManagerDidUpdateState:(nonnull CBCentralManager *)central {
    NSString *stateString = gmbt_manager_state_name(central.state);
    NSLog(@"[GMBluetooth] CBCentralManager state changed: %@ (%d)", stateString, (int)central.state);

    const CBManagerState previous = _centralState;
    _centralState = central.state;

    if (central.state == CBManagerStatePoweredOn) {
        if (_scanPendingPowerOn) {
            _scanPendingPowerOn = false;
            NSLog(@"[GMBluetooth] central reached PoweredOn - starting the scan deferred at request time");
            [self beginScan];
        }
        [self handleOpenPeripheralQueue];
    }
    // Unknown or Resetting before the central was ever PoweredOn: a deferred
    // scan or connect keeps waiting. Otherwise the central has left PoweredOn
    // or reached a final state, and nothing asked of it can go on.
    else if (!gmbt_state_is_transient(central.state) || previous == CBManagerStatePoweredOn) {
        std::string message;
        const gmbluetooth::Error error = gmbt_unavailable_error(central.state, message);

        if (_isScanning || _scanPendingPowerOn) {
            _isScanning = false;
            _scanPendingPowerOn = false;
            _scanServices = nil;
            NSLog(@"[GMBluetooth] central reached %@ - the scan has ended", stateString);
            [self notifyScanStopped:error message:message];
        }

        [self failQueuedOpens:error message:message];

        // The peripherals a scan found are not valid across a power cycle.
        [self forgetDiscoveredPeripherals];

        if (_openedPeripherals.count > 0) {
            // Every link is gone, and CoreBluetooth does not promise a
            // didDisconnectPeripheral for each. Report each open peripheral once
            // here; the later didDisconnectPeripheral, if any, finds it closed.
            NSArray<CBPeripheral *> *lost = [_openedPeripherals allValues];
            [_openedPeripherals removeAllObjects];
            [_connectedPeripherals removeAllObjects];
            [_peripheralQueues removeAllObjects];
            NSString *lostMessage = [NSString stringWithFormat:@"Bluetooth became unavailable (%@)", stateString];
            for (CBPeripheral *peripheral in lost) {
                [self notifyOperation:@"bt_le_peripheral_disconnect"
                          extraParams:@{ @"address": [self peripheralKey:peripheral],
                                         @"error_code": @((int)central.state),
                                         @"error": @((int)gmbluetooth::Error::Disconnected),
                                         @"message": lostMessage }];
            }
        }
    }

    [self notifyOperation:@"bt_state_changed"
              extraParams:@{ @"state": @((int)central.state), @"state_name": stateString }];

    if (self.managerStateSink) self.managerStateSink();
}

// Completes the head of the peripheral's RSSI reads with the dBm value.
- (void) peripheral:(CBPeripheral *)peripheral didReadRSSI:(NSNumber *)RSSI error:(NSError *)error {

    NSMutableArray *queue = [self queuesForPeripheral:peripheral create:NO].readRssi;
    GMBTQueuedPeripheral *queuedPeripheral = [self takeHeadOf:queue answering:@"didReadRSSI" matching:^BOOL(id head) {
        return ((GMBTQueuedPeripheral *)head).peripheral == peripheral;
    }];
    if (!queuedPeripheral) return;

    gmbluetooth::LeOpResult result;
    if (!error) result.number = RSSI.intValue;
    [self completeOp:queuedPeripheral.opId error:error result:std::move(result)];

    [self handleReadRssiQueue:queue];
}

- (void) peripheralDidUpdateName:(CBPeripheral *)peripheral {
	// We don't handle this
}

// Only an open peripheral's requests are failed here; nothing is reported to
// the game and the peripheral's open state is left as it is.
- (void) peripheral:(CBPeripheral *)peripheral didModifyServices:(NSArray<CBService *> *)invalidatedServices {
    if (!_openedPeripherals[[self peripheralKey:peripheral]]) return;

    // The link stays up, so no disconnect will fail these: requests on an
    // invalidated service fail here, each with its own op id. Service
    // discovery is per peripheral and stays queued.
    GMBTPeripheralQueues *queues = [self queuesForPeripheral:peripheral create:NO];
    if (!queues) return;

    CBService *(^characteristicService)(id) = ^CBService *(id entry) {
        return ((GMBTQueuedCharacteristic *)entry).characteristic.service;
    };
    CBService *(^descriptorService)(id) = ^CBService *(id entry) {
        return ((GMBTQueuedDescriptor *)entry).descriptor.characteristic.service;
    };

    NSMutableArray *q = queues.fetchCharacteristics;
    [self failQueue:q
            service:^CBService *(id entry) { return ((GMBTQueuedService *)entry).service; }
        invalidated:invalidatedServices
            reissue:^{ [self handleFetchCharacteristicsQueue:q]; }];

    NSMutableArray *d = queues.fetchDescriptors;
    [self failQueue:d
            service:characteristicService
        invalidated:invalidatedServices
            reissue:^{ [self handleFetchDescriptorsQueue:d]; }];

    NSMutableArray *r = queues.readCharacteristic;
    [self failQueue:r
            service:characteristicService
        invalidated:invalidatedServices
            reissue:^{ [self handleReadCharacteristicQueue:r]; }];

    NSMutableArray *w = queues.writeCharacteristic;
    [self failQueue:w
            service:characteristicService
        invalidated:invalidatedServices
            reissue:^{ [self handleWriteCharacteristicQueue:w]; }];

    NSMutableArray *wn = queues.writeWithoutResponse;
    [self failQueue:wn
            service:characteristicService
        invalidated:invalidatedServices
            reissue:^{ [self sendWritesWithoutResponse:peripheral]; }];

    NSMutableArray *n = queues.notifyCharacteristic;
    [self failQueue:n
            service:characteristicService
        invalidated:invalidatedServices
            reissue:^{ [self handleNotifyCharacteristicQueue:n]; }];

    NSMutableArray *rd = queues.readDescriptor;
    [self failQueue:rd
            service:descriptorService
        invalidated:invalidatedServices
            reissue:^{ [self handleReadDescriptorQueue:rd]; }];

    NSMutableArray *wd = queues.writeDescriptor;
    [self failQueue:wd
            service:descriptorService
        invalidated:invalidatedServices
            reissue:^{ [self handleWriteDescriptorQueue:wd]; }];
}


@end


#if !__has_feature(objc_arc)
#error "GMBluetooth_apple.mm must be compiled with -fobjc-arc: the C++ backend stores Objective-C objects in plain members and relies on ARC to retain them"
#endif

#if TARGET_OS_OSX

// IOBluetooth invokes delegate methods on objects the backend owns, and the
// completion handlers below drop the backend's reference to that same object.
// Releasing it there frees the delegate - and the block it is still executing -
// mid-call (EXC_BAD_ACCESS). Keep it alive until the next main-queue turn.
static void gmbt_release_after_callback(id object)
{
    if (!object) return;
    dispatch_async(dispatch_get_main_queue(), ^{ (void)object; });
}

// An SDP query handler the backend let go of while IOBluetooth may still call
// it. Its block is nilled first, so a late call does nothing; it is kept here
// until that call, so IOBluetooth never calls a freed object.
static NSMutableSet* gmbt_parked_objects()
{
    static NSMutableSet* parked = [NSMutableSet new];
    return parked;
}

static void gmbt_park(id object)
{
    if (object) [gmbt_parked_objects() addObject:object];
}

static void gmbt_unpark(id object)
{
    if (!object) return;
    dispatch_async(dispatch_get_main_queue(), ^{ [gmbt_parked_objects() removeObject:object]; });
}

// Bluetooth Classic (RFCOMM) support is macOS-only: IOBluetooth is not
// available on iOS. These small delegate/notification shims translate
// IOBluetooth's target+selector and delegate-protocol callbacks into blocks
// so the C++ AppleBackend below can stay in one place.

@interface GMBTClassicChannelDelegate : NSObject <IOBluetoothRFCOMMChannelDelegate>
@property (nonatomic, copy) void (^onOpenComplete)(IOReturn status);
@property (nonatomic, copy) void (^onData)(NSData *data);
@property (nonatomic, copy) void (^onClose)(void);
// The writeAsync: in flight finished; the next chunk goes from here.
@property (nonatomic, copy) void (^onWriteComplete)(IOReturn status);
@end

@implementation GMBTClassicChannelDelegate
- (void)rfcommChannelOpenComplete:(IOBluetoothRFCOMMChannel *)rfcommChannel status:(IOReturn)error {
    if (self.onOpenComplete) self.onOpenComplete(error);
}
- (void)rfcommChannelData:(IOBluetoothRFCOMMChannel *)rfcommChannel data:(void *)dataPointer length:(size_t)dataLength {
    if (self.onData) self.onData([NSData dataWithBytes:dataPointer length:dataLength]);
}
- (void)rfcommChannelClosed:(IOBluetoothRFCOMMChannel *)rfcommChannel {
    if (self.onClose) self.onClose();
}
- (void)rfcommChannelWriteComplete:(IOBluetoothRFCOMMChannel *)rfcommChannel refcon:(void *)refcon status:(IOReturn)error {
    if (self.onWriteComplete) self.onWriteComplete(error);
}
@end

@interface GMBTClassicInquiryDelegate : NSObject <IOBluetoothDeviceInquiryDelegate>
@property (nonatomic, copy) void (^onDeviceFound)(IOBluetoothDevice *device);
@property (nonatomic, copy) void (^onComplete)(IOReturn error, BOOL aborted);
@end

@implementation GMBTClassicInquiryDelegate
- (void)deviceInquiryDeviceFound:(IOBluetoothDeviceInquiry *)sender device:(IOBluetoothDevice *)device {
    if (self.onDeviceFound) self.onDeviceFound(device);
}
// setUpdateNewDeviceNames: resolves names after the device was found; the
// device is reported again so the core's entry gets the name.
- (void)deviceInquiryDeviceNameUpdated:(IOBluetoothDeviceInquiry *)sender device:(IOBluetoothDevice *)device devicesRemaining:(uint32_t)devicesRemaining {
    if (self.onDeviceFound) self.onDeviceFound(device);
}
- (void)deviceInquiryComplete:(IOBluetoothDeviceInquiry *)sender error:(IOReturn)error aborted:(BOOL)aborted {
    if (self.onComplete) self.onComplete(error, aborted);
}
@end

// performSDPQuery: does not use a formal delegate protocol - it calls back
// on whatever object it is given via -sdpQueryComplete:status:.
@interface GMBTSDPQueryHandler : NSObject
@property (nonatomic, copy) void (^onComplete)(IOBluetoothDevice *device, IOReturn status);
@end

@implementation GMBTSDPQueryHandler
- (void)sdpQueryComplete:(IOBluetoothDevice *)device status:(IOReturn)status {
    if (self.onComplete) self.onComplete(device, status);
    gmbt_unpark(self);
}
@end

// registerForChannelOpenNotifications:selector:withChannelID:direction: is
// likewise a plain target+selector callback, not a delegate protocol.
@interface GMBTClassicServerHub : NSObject
@property (nonatomic, copy) void (^onChannelOpened)(IOBluetoothRFCOMMChannel *channel);
@end

@implementation GMBTClassicServerHub
- (void)rfcommChannelOpened:(IOBluetoothUserNotification *)notification channel:(IOBluetoothRFCOMMChannel *)channel {
    if (self.onChannelOpened) self.onChannelOpened(channel);
}
@end

// IOBluetoothDevicePair's PIN/passkey/numeric-confirmation delegate methods are
// all optional; leaving them unimplemented makes it fall back to the system's
// own pairing UI for those steps, so only the terminal callback is needed here.
@interface GMBTDevicePairDelegate : NSObject <IOBluetoothDevicePairDelegate>
@property (nonatomic, copy) void (^onFinished)(IOReturn error);
@end

@implementation GMBTDevicePairDelegate
- (void)devicePairingFinished:(id)sender error:(IOReturn)error {
    if (self.onFinished) self.onFinished(error);
}
@end

#endif // TARGET_OS_OSX


namespace gmbluetooth
{
namespace
{
    // Never nil, so the result can always go into a collection.
    static id json_safe_value(id value)
    {
        if (!value || value == [NSNull null]) return [NSNull null];
        if ([value isKindOfClass:[NSData class]])
            return [(NSData*)value base64EncodedStringWithOptions:0];
        if ([value isKindOfClass:[NSUUID class]])
            return [(NSUUID*)value UUIDString];
        if ([value isKindOfClass:[CBUUID class]])
            return [(CBUUID*)value UUIDString];
        if ([value isKindOfClass:[NSString class]] || [value isKindOfClass:[NSNumber class]])
            return value;
        if ([value isKindOfClass:[NSArray class]])
        {
            NSMutableArray* out = [NSMutableArray array];
            for (id item in (NSArray*)value) [out addObject:json_safe_value(item)];
            return out;
        }
        if ([value isKindOfClass:[NSDictionary class]])
        {
            NSMutableDictionary* out = [NSMutableDictionary dictionary];
            for (id key in (NSDictionary*)value)
            {
                NSString* name = [key description];
                if (name) out[name] = json_safe_value(((NSDictionary*)value)[key]);
            }
            return out;
        }
        NSString* text = [value description];
        if (!text) return [NSNull null];
        return text;
    }

    static std::string json_string(NSDictionary* dictionary)
    {
        NSDictionary* safe = (NSDictionary*)json_safe_value(dictionary ? dictionary : @{});
        NSError* error = nil;
        NSData* data = [NSJSONSerialization dataWithJSONObject:safe options:0 error:&error];
        if (!data || error) return "{}";
        NSString* text = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
        return gmbt_string(text);
    }

    static std::string normalized_event_type(NSString* oldType)
    {
        std::string type = gmbt_string(oldType);
        if (type.rfind("bt_", 0) == 0)
            return "bluetooth_" + type.substr(3);
        return type;
    }

    // True while the user has not answered the Bluetooth prompt; creating a
    // CoreBluetooth manager now would show it. [CBManager authorization] is
    // iOS 13.1 and macOS 10.15; before them there is no prompt to wait for.
    static bool authorization_not_determined()
    {
        if (@available(iOS 13.1, macOS 10.15, *))
            return [CBManager authorization] == CBManagerAuthorizationNotDetermined;
        return false;
    }

    class AppleBackend final : public Backend
    {
    public:
        explicit AppleBackend(CoreHooks hooks) : hooks_(std::move(hooks)) {}

        ~AppleBackend() override { shutdown(); }

        Error initialize(std::string& message) override
        {
            if (transport_)
            {
                GMBT_LOG("Apple transport already created");
                return Error::Ok;
            }
            transport_ = [GMBluetoothAppleTransport new];
            AppleBackend* self = this;
            transport_.eventSink = ^(NSString* type, NSDictionary* params) {
                self->on_event(type, params);
            };
            transport_.opSink = ^(NSNumber* op_id, Error error, std::string text, LeOpResult result) {
                self->on_op_completed(op_id, error, std::move(text), std::move(result));
            };
            transport_.managerStateSink = ^{
                self->on_manager_state();
            };

            // Creating a manager is what shows the system prompt, so while the
            // user has not answered it the managers wait for permission_request.
            if (authorization_not_determined())
            {
                GMBT_LOG("Apple transport created; Bluetooth authorization is not determined, so the "
                         "CoreBluetooth managers wait for bluetooth_permission_request.");
            }
            else
            {
                [transport_ bt_init];
                GMBT_LOG("Apple transport created and bt_init sent. CoreBluetooth powers on asynchronously - "
                         "watch for 'CBCentralManager state changed: PoweredOn' before expecting a scan to work.");
            }
            message.clear();
            return Error::Ok;
        }

        void shutdown() override
        {
#if TARGET_OS_OSX
            classic_shutdown();
#endif
            // The core fails the requests still waiting for an answer; a late
            // state change or poll finds nothing pending.
            permission_pending_ = false;
            ++permission_generation_;
            if (!transport_) return;
            transport_.eventSink = nil;
            transport_.opSink = nil;
            transport_.managerStateSink = nil;
            [transport_ bt_end];
            transport_ = nil;
            std::scoped_lock lock(mutex_);
            le_connection_to_id_.clear();
            le_id_to_connection_.clear();
        }

        bool supports_ble() const override
        {
            return !transport_ || !transport_.centralManager || transport_.centralManager.state != CBManagerStateUnsupported;
        }

        bool supports_le_advertise() const override
        {
            return !transport_ || !transport_.peripheralManager || transport_.peripheralManager.state != CBManagerStateUnsupported;
        }

        bool supports_le_server() const override { return supports_le_advertise(); }

#if TARGET_OS_OSX
        bool supports_classic() const override { return true; }
        bool supports_classic_server() const override { return true; }
        bool pairing_is_supported(const DiscoveredDevice& device) const override
        {
            return device.transport == Transport::Classic && device.address_available && !device.address.empty();
        }
#else
        bool supports_classic() const override { return false; }
        bool supports_classic_server() const override { return false; }
        bool pairing_is_supported(const DiscoveredDevice&) const override { return false; }
#endif

        // The core answers LeCentral, LeAdvertise, LeServer, Classic and
        // ClassicServer from the supports_* calls. Of the rest, CoreBluetooth
        // advertises only the name and service UUIDs (always connectable, no
        // TX power), never scans passively, hands the app no descriptor
        // requests, signed writes or connection events, and pairs on its own
        // when an attribute asks for it; macOS pairs Classic devices through
        // IOBluetooth and cannot be made discoverable. CoreBluetooth
        // negotiates the MTU itself, takes no connection priority and cannot
        // turn the radio on; only macOS lists paired (Classic) devices.
        bool feature_supported(std::int32_t feature) const override
        {
            switch (feature)
            {
                case kFeatureLeAdvertiseName:
                case kFeatureLeAdvertiseServiceUuids:
                    return supports_le_advertise();
                case kFeatureClassicPairing:
                case kFeaturePairedDevicesQuery:
                    return supports_classic();
                case kFeaturePermissionRequest:
                    return true;
                case kFeatureLeReadRssi:
                    return supports_ble();
                case kFeatureLeMtuRequest:
                case kFeatureLeConnectionPriority:
                case kFeatureRequestEnable:
                    return false;
                default:
                    return false;
            }
        }

        std::int32_t current_bluetooth_state() const override
        {
            if (!transport_)
                return 0; // BluetoothState.Unknown
            if (!transport_.centralManager)
                return 3; // BluetoothState.Unauthorized: waiting for permission_request
            return static_cast<std::int32_t>(transport_.centralManager.state);
        }

        PermissionStatus permission_status() const override
        {
#if (TARGET_OS_IOS || TARGET_OS_OSX)
            if (@available(iOS 13.1, macOS 10.15, *))
            {
                switch ([CBManager authorization])
                {
                    case CBManagerAuthorizationAllowedAlways: return PermissionStatus::Granted;
                    case CBManagerAuthorizationDenied:
                    case CBManagerAuthorizationRestricted: return PermissionStatus::Denied;
                    case CBManagerAuthorizationNotDetermined: return PermissionStatus::Unknown;
                }
            }
#endif
            return PermissionStatus::Granted;
        }

        // CoreBluetooth has no explicit request call: creating a manager while
        // authorization is NotDetermined shows the prompt, and the managers
        // report a state change once the user has answered.
        Error permission_request(std::string& message) override
        {
            if (!transport_)
            {
                message = "Bluetooth backend is not initialized";
                return Error::NotInitialized;
            }
            message.clear();

            // A prompt is already up: its answer is pushed once, and the core
            // hands it to every request waiting.
            if (permission_pending_) return Error::Ok;

            if (transport_.centralManager || !authorization_not_determined())
            {
                push_permission_result();
                return Error::Ok;
            }

            // Pending before the managers exist, so their first state change
            // finds it.
            permission_pending_ = true;
            const std::uint64_t generation = ++permission_generation_;
            [transport_ bt_init];
            GMBT_LOG("Bluetooth permission requested: CoreBluetooth managers created, the system prompt follows.");
            schedule_permission_poll(generation);
            return Error::Ok;
        }

        // Ids this backend issues: "apple:ble:" + the peripheral's identifier,
        // and on macOS "apple:classic:" + the "XX:XX:XX:XX:XX:XX" address.
        Error device_from_id(const std::string& id, DiscoveredDevice& device, std::string& message) override
        {
            constexpr const char* ble_prefix = "apple:ble:";
            constexpr const char* classic_prefix = "apple:classic:";

            if (id.rfind(ble_prefix, 0) == 0)
            {
                NSUUID* identifier = [[NSUUID alloc] initWithUUIDString:gmbt_ns(id.substr(std::char_traits<char>::length(ble_prefix)))];
                if (!identifier) { message = "The id does not hold a valid CoreBluetooth identifier"; return Error::InvalidArgument; }
                if (const Error e = require_managers(message); e != Error::Ok) return e;

                Error error = Error::Ok;
                CBPeripheral* peripheral = [transport_ bt_le_retrieve_peripheral:identifier error:error message:message];
                if (!peripheral) return error;
                device = le_device_from_peripheral(peripheral);
                message.clear();
                return Error::Ok;
            }

            if (id.rfind(classic_prefix, 0) == 0)
            {
#if TARGET_OS_OSX
                std::string address = id.substr(std::char_traits<char>::length(classic_prefix));
                if (!classic_address_is_well_formed(address)) { message = "The id does not hold a valid Bluetooth address"; return Error::InvalidArgument; }
                if (const Error e = require_managers(message); e != Error::Ok) return e;

                // IOBluetooth spells addresses with dashes.
                for (auto& c : address) if (c == ':') c = '-';
                IOBluetoothDevice* btDevice = [IOBluetoothDevice deviceWithAddressString:gmbt_ns(address)];
                if (!btDevice) { message = "The id does not hold a valid Bluetooth address"; return Error::InvalidArgument; }
                device = classic_device_entry(btDevice);
                message.clear();
                return Error::Ok;
#else
                message = "Bluetooth Classic ids are not issued on this platform";
                return Error::InvalidArgument;
#endif
            }

            message = "The id is not one this platform issues";
            return Error::InvalidArgument;
        }

        // CoreBluetooth lists connected peripherals only by the services they
        // host, so an empty list cannot mean every device here.
        Error le_connected_devices_query(std::uint64_t query_id, const std::vector<std::string>& service_uuids, std::string& message) override
        {
            if (service_uuids.empty())
            {
                message = "Apple lists connected devices by service: pass at least one service UUID";
                return Error::InvalidArgument;
            }
            if (const Error e = require_managers(message); e != Error::Ok) return e;

            NSMutableArray<NSString*>* uuids = [NSMutableArray arrayWithCapacity:service_uuids.size()];
            for (const auto& uuid : service_uuids) [uuids addObject:gmbt_ns(uuid)];

            Error error = Error::Ok;
            NSArray<CBPeripheral*>* peripherals = [transport_ bt_le_connected_peripherals:uuids error:error message:message];
            if (error != Error::Ok) return error;

            // The answer is known now; the core registered the callback
            // before asking, so it can go out at once.
            BackendEvent ev;
            ev.type = BackendEventType::DevicesQueried;
            ev.transport = Transport::LowEnergy;
            ev.op_id = query_id;
            for (CBPeripheral* peripheral in peripherals) ev.devices.push_back(le_device_from_peripheral(peripheral));
            hooks_.push_event(std::move(ev));
            message.clear();
            return Error::Ok;
        }

        // active is a hint CoreBluetooth has no switch for: every scan is
        // active (LePassiveScan). When every filter names a service, the
        // scan is narrowed to those services natively too.
        Error le_scan_start(bool, const std::vector<LeScanFilter>& filters, std::string& message) override
        {
            if (const Error e = require_managers(message); e != Error::Ok) return e;
            NSMutableArray<NSString*>* services = nil;
            const bool all_name_a_service = std::all_of(filters.begin(), filters.end(),
                [](const LeScanFilter& filter) { return filter.service_uuid.has_value(); });
            if (!filters.empty() && all_name_a_service)
            {
                services = [NSMutableArray arrayWithCapacity:filters.size()];
                for (const auto& filter : filters) [services addObject:gmbt_ns(*filter.service_uuid)];
            }
            return [transport_ bt_le_scan_start:services message:message];
        }
        Error le_scan_stop(std::string& message) override
        {
            if (const Error e = require_managers(message); e != Error::Ok) return e;
            return [transport_ bt_le_scan_stop:message];
        }
        bool le_scan_is_running() const override { return transport_ && [transport_ bt_le_scan_is_active]; }

        Error le_connect(std::uint64_t connection, const DiscoveredDevice& device, std::string& message) override
        {
            if (const Error e = require_managers(message); e != Error::Ok) return e;
            const std::string identifier = identifier_from_device(device);
            if (identifier.empty()) { message="BLE device identifier is unavailable"; return Error::InvalidArgument; }
            {
                std::scoped_lock lock(mutex_);
                // One link per peripheral: an entry is removed when its link
                // ends or its connect fails.
                if (le_id_to_connection_.count(identifier) != 0) { message="The device already has a connection"; return Error::Busy; }
                le_connection_to_id_[connection]=identifier;
                le_id_to_connection_[identifier]=connection;
            }
            const Error e=[transport_ bt_le_peripheral_open:gmbt_ns(identifier) message:message];
            if (e != Error::Ok) remove_connection(connection);
            return e;
        }
        Error le_disconnect(std::uint64_t connection,std::string& message) override
        {
            const std::string id=id_for_connection(connection); if(id.empty())return invalid_connection(message);
            const Error e=[transport_ bt_le_peripheral_close:gmbt_ns(id) message:message];
            remove_connection(connection);
            return e;
        }
        bool le_connection_is_connected(std::uint64_t connection) const override
        {
            const std::string id=id_for_connection(connection); return !id.empty() && [transport_ bt_le_peripheral_is_connected:gmbt_ns(id)];
        }
        std::int32_t le_connection_mtu(std::uint64_t connection) const override
        {
            const std::string id=id_for_connection(connection);
            if (id.empty() || !transport_) return 23;
            return static_cast<std::int32_t>([transport_ bt_le_peripheral_mtu:gmbt_ns(id)]);
        }
        Error le_read_rssi(std::uint64_t op, std::uint64_t c, std::string& m) override
        {
            auto id=id_for_connection(c); if(id.empty())return invalid_connection(m);
            return [transport_ bt_le_peripheral_read_rssi:gmbt_ns(id) opId:@(op) message:m];
        }
        // The GATT calls, advertise start and add_service hand the core's op
        // id to the transport, which reports it on the completion.
        Error le_services_discover(std::uint64_t op,std::uint64_t c,std::string& m) override { auto id=id_for_connection(c); if(id.empty())return invalid_connection(m); return [transport_ bt_le_peripheral_get_services:gmbt_ns(id) opId:@(op) message:m]; }
        // Attributes go by the instances the transport's discoveries reported.
        Error le_characteristics_discover(std::uint64_t op,std::uint64_t c,const LeAttributeRef&a,std::string&m) override { auto id=id_for_connection(c); if(id.empty())return invalid_connection(m); return [transport_ bt_le_service_get_characteristics:gmbt_ns(id) service:a.service opId:@(op) message:m]; }
        Error le_descriptors_discover(std::uint64_t op,std::uint64_t c,const LeAttributeRef&a,std::string&m) override { auto id=id_for_connection(c); if(id.empty())return invalid_connection(m); return [transport_ bt_le_characteristic_get_descriptors:gmbt_ns(id) service:a.service characteristic:a.characteristic opId:@(op) message:m]; }
        Error le_characteristic_read(std::uint64_t op,std::uint64_t c,const LeAttributeRef&a,std::string&m) override { auto id=id_for_connection(c); if(id.empty())return invalid_connection(m); return [transport_ bt_le_characteristic_read:gmbt_ns(id) service:a.service characteristic:a.characteristic opId:@(op) message:m]; }
        Error le_characteristic_write(std::uint64_t op,std::uint64_t c,const LeAttributeRef&a,const std::string&v,bool with_response,std::string&m) override
        {
            auto id=id_for_connection(c); if(id.empty())return invalid_connection(m);
            if (with_response)
                return [transport_ bt_le_characteristic_write_request:gmbt_ns(id) service:a.service characteristic:a.characteristic value:gmbt_ns(v) opId:@(op) message:m];
            return [transport_ bt_le_characteristic_write_command:gmbt_ns(id) service:a.service characteristic:a.characteristic value:gmbt_ns(v) opId:@(op) message:m];
        }
        Error le_characteristic_subscribe(std::uint64_t op,std::uint64_t c,const LeAttributeRef&a,std::int32_t mode,std::string&m) override
        { auto id=id_for_connection(c); if(id.empty())return invalid_connection(m); return [transport_ bt_le_characteristic_subscribe:gmbt_ns(id) service:a.service characteristic:a.characteristic mode:mode opId:@(op) message:m]; }
        Error le_descriptor_read(std::uint64_t op,std::uint64_t c,const LeAttributeRef&a,std::string&m) override { auto id=id_for_connection(c); if(id.empty())return invalid_connection(m); return [transport_ bt_le_descriptor_read:gmbt_ns(id) service:a.service characteristic:a.characteristic descriptor:a.descriptor opId:@(op) message:m]; }
        Error le_descriptor_write(std::uint64_t op,std::uint64_t c,const LeAttributeRef&a,const std::string&v,std::string&m) override { auto id=id_for_connection(c); if(id.empty())return invalid_connection(m); return [transport_ bt_le_descriptor_write:gmbt_ns(id) service:a.service characteristic:a.characteristic descriptor:a.descriptor value:gmbt_ns(v) opId:@(op) message:m]; }

        // CoreBluetooth advertises the local name and service UUIDs only, and
        // always connectable: anything else asked for is NotSupported here,
        // before anything starts (R1-36).
        Error le_advertise_start(std::uint64_t op,const LeAdvertiseSettings& settings,const LeAdvertiseData& data,std::string& m) override
        {
            if (const Error e = require_managers(m); e != Error::Ok) return e;
            const char* field = nullptr;
            if (!settings.connectable) field = "connectable = false";
            else if (settings.tx_power) field = "tx_power";
            else if (data.include_tx_power) field = "include_tx_power";
            else if (!data.service_data.empty()) field = "service_data";
            else if (!data.manufacturer_data.empty()) field = "manufacturer_data";
            if (field)
            {
                m = std::string("Apple cannot advertise ") + field + ": CoreBluetooth sends only the local name and service UUIDs";
                return Error::NotSupported;
            }

            NSMutableArray<NSString*>* uuids = [NSMutableArray arrayWithCapacity:data.service_uuids.size()];
            for (const auto& uuid : data.service_uuids) [uuids addObject:gmbt_ns(uuid)];
            return [transport_ bt_le_advertise_start:data.include_name serviceUUIDs:uuids opId:@(op) message:m];
        }
        // Stopping, clearing and closing with no managers have nothing to undo.
        Error le_advertise_stop(std::string&m) override
        {
            if (!managers_created()) { m.clear(); return Error::Ok; }
            return [transport_ bt_le_advertise_stop:m];
        }
        bool le_advertise_is_running() const override { return transport_ && [transport_ bt_le_advertise_is_active]; }
        Error le_server_start(std::string&m) override
        {
            if (const Error e = require_managers(m); e != Error::Ok) return e;
            return [transport_ bt_le_server_open:m];
        }
        Error le_server_stop(std::string&m) override
        {
            if (!managers_created()) { m.clear(); return Error::Ok; }
            return [transport_ bt_le_server_close:m];
        }
        bool le_server_is_running() const override { return _isServerOpen; }
        Error le_server_add_service(std::uint64_t op,const std::string&j,std::string&m) override
        {
            if (const Error e = require_managers(m); e != Error::Ok) return e;
            return [transport_ bt_le_server_add_service:gmbt_ns(j) opId:@(op) message:m];
        }
        Error le_server_clear_services(std::string&m) override
        {
            if (!managers_created()) { m.clear(); return Error::Ok; }
            return [transport_ bt_le_server_clear_services:m];
        }
        Error le_server_respond_read(std::int32_t r,std::int32_t st,const std::string&v,std::string&m) override
        {
            if (const Error e = require_managers(m); e != Error::Ok) return e;
            return [transport_ bt_le_server_respond_read:r status:st value:gmbt_ns(v) message:m];
        }
        Error le_server_respond_write(std::int32_t r,std::int32_t st,std::string&m) override
        {
            if (const Error e = require_managers(m); e != Error::Ok) return e;
            return [transport_ bt_le_server_respond_write:r status:st message:m];
        }
        Error le_server_notify_value(const std::string&s,const std::string&ch,const std::string&central,const std::string&v,std::string&m) override
        {
            if (const Error e = require_managers(m); e != Error::Ok) return e;
            return [transport_ bt_le_server_notify_value:gmbt_ns(s) characteristicUuid:gmbt_ns(ch) central:gmbt_ns(central) value:gmbt_ns(v) message:m];
        }

#if TARGET_OS_OSX

        Error classic_scan_start(std::string& message) override
        {
            if (const Error e = require_managers(message); e != Error::Ok) return e;
            if (classic_inquiry_) { GMBT_LOG("Classic inquiry already running"); message.clear(); return Error::Ok; }

            // IOBluetooth delivers every callback on the run loop of the thread
            // that started the operation. CoreBluetooth (queue:nil) always uses
            // the main queue instead, so BLE can work while Classic stays silent
            // if this is not the main thread.
            GMBT_LOG("Classic inquiry start: main_thread=%d current_runloop_is_main=%d",
                [NSThread isMainThread] ? 1 : 0,
                CFRunLoopGetCurrent() == CFRunLoopGetMain() ? 1 : 0);
            GMBT_LOG("Classic inquiry start: controller powered=%d address=%s",
                [[IOBluetoothHostController defaultController] powerState] == kBluetoothHCIPowerStateON ? 1 : 0,
                gmbt_string([[IOBluetoothHostController defaultController] addressAsString]).c_str());

            GMBTClassicInquiryDelegate* delegate = [GMBTClassicInquiryDelegate new];
            AppleBackend* self = this;
            std::weak_ptr<int> alive = lifetime_;
            delegate.onDeviceFound = ^(IOBluetoothDevice* device) {
                if (alive.expired()) return;
                self->handle_classic_device_found(device);
            };
            delegate.onComplete = ^(IOReturn error, BOOL aborted) {
                if (alive.expired()) return;
                GMBT_LOG("Classic inquiry complete: IOReturn=0x%08x aborted=%d", error, aborted ? 1 : 0);
                self->handle_classic_inquiry_complete(error);
            };

            IOBluetoothDeviceInquiry* inquiry = [IOBluetoothDeviceInquiry inquiryWithDelegate:delegate];
            [inquiry setInquiryLength:10];
            [inquiry setUpdateNewDeviceNames:YES];
            const IOReturn status = [inquiry start];
            GMBT_LOG("Classic inquiry start -> IOReturn=0x%08x", status);
            if (status != kIOReturnSuccess)
            {
                message = "Bluetooth Classic scan could not start";
                return Error::OperationFailed;
            }
            classic_inquiry_delegate_ = delegate;
            classic_inquiry_ = inquiry;
            message.clear();
            return Error::Ok;
        }

        Error classic_scan_stop(std::string& message) override
        {
            if (classic_inquiry_) [classic_inquiry_ stop];
            message.clear();
            return Error::Ok;
        }

        bool classic_scan_is_running() const override { return classic_inquiry_ != nil; }

        Error classic_connect(std::uint64_t connection, const DiscoveredDevice& device, const std::string& service_uuid, std::string& message) override
        {
            if (const Error e = require_managers(message); e != Error::Ok) return e;
            if (!device.address_available || device.address.empty())
            {
                message = "Classic device address is unavailable";
                return Error::InvalidArgument;
            }
            IOBluetoothDevice* btDevice = classic_device_for_address(device.address);
            if (!btDevice)
            {
                GMBT_LOG("Classic connect: could not resolve address '%s'", device.address.c_str());
                message = "Classic device could not be resolved";
                return Error::NotFound;
            }
            GMBT_LOG("Classic connect: address=%s paired=%d service_uuid='%s'",
                device.address.c_str(), [btDevice isPaired] ? 1 : 0, service_uuid.c_str());

            auto state = std::make_shared<ClassicConnectionState>();
            state->connection = connection;
            {
                std::scoped_lock lock(classic_mutex_);
                classic_connections_[connection] = state;
            }

            // Paired devices keep the SDP records from pairing (or the last
            // query) cached on the IOBluetoothDevice. Use them directly when
            // they already hold the requested service: a fresh SDP query can
            // stall without ever calling back.
            BluetoothRFCOMMChannelID cachedChannel = 0;
            if (classic_find_rfcomm_channel(btDevice, service_uuid, cachedChannel))
            {
                GMBT_LOG("Classic connect: using cached SDP record, RFCOMM channel %d", (int)cachedChannel);
                classic_open_rfcomm(connection, btDevice, cachedChannel);
                message.clear();
                return Error::Ok;
            }

            GMBTSDPQueryHandler* sdpHandler = [GMBTSDPQueryHandler new];
            AppleBackend* self = this;
            std::weak_ptr<int> alive = lifetime_;
            const std::string uuidCopy = service_uuid;
            sdpHandler.onComplete = ^(IOBluetoothDevice* d, IOReturn status) {
                if (alive.expired()) return;
                self->handle_classic_sdp_complete(connection, d, status, uuidCopy);
            };
            {
                std::scoped_lock lock(classic_mutex_);
                classic_pending_sdp_[connection] = sdpHandler;
            }

            // Unfiltered: the UUID-filtered overload is less reliable against
            // some stacks, and the record is matched by UUID afterwards anyway.
            const IOReturn status = [btDevice performSDPQuery:sdpHandler];
            GMBT_LOG("Classic connect: SDP query start (cached records=%d) -> IOReturn=0x%08x",
                (int)[[btDevice services] count], status);
            if (status == kIOReturnSuccess)
            {
                // Fail the connect instead of leaving the GML callback waiting
                // forever if IOBluetooth never delivers sdpQueryComplete:.
                dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(kClassicSdpTimeoutSeconds * NSEC_PER_SEC)),
                               dispatch_get_main_queue(), ^{
                    if (alive.expired()) return;
                    self->handle_classic_sdp_timeout(connection);
                });
            }
            if (status != kIOReturnSuccess)
            {
                std::scoped_lock lock(classic_mutex_);
                classic_connections_.erase(connection);
                classic_pending_sdp_.erase(connection);
                message = "Classic service discovery could not start";
                return Error::OperationFailed;
            }
            message.clear();
            return Error::Ok;
        }

        Error classic_disconnect(std::uint64_t connection, std::string& message) override
        {
            std::shared_ptr<ClassicConnectionState> state;
            GMBTSDPQueryHandler* sdp = nil;
            {
                std::scoped_lock lock(classic_mutex_);
                auto it = classic_connections_.find(connection);
                if (it == classic_connections_.end()) return invalid_connection(message);
                state = it->second;

                // The game is done with the handle, so its record goes now and
                // the channel is detached before it closes: a late SDP, open,
                // data or close callback finds nothing and reports nothing,
                // and unread bytes go with it. A connect still in SDP or
                // opening is cancelled; the core fires its callback once.
                classic_connections_.erase(it);
                state->connected = false;
                auto pending = classic_pending_sdp_.find(connection);
                if (pending != classic_pending_sdp_.end())
                {
                    sdp = pending->second;
                    classic_pending_sdp_.erase(pending);
                }
            }

            if (sdp)
            {
                sdp.onComplete = nil;
                gmbt_park(sdp);
            }
            classic_drop_writes(*state);
            classic_detach_channel(state->channel, state->delegate);
            message.clear();
            return Error::Ok;
        }

        bool classic_connection_is_connected(std::uint64_t connection) const override
        {
            std::scoped_lock lock(classic_mutex_);
            auto it = classic_connections_.find(connection);
            return it != classic_connections_.end() && it->second->connected;
        }

        std::int32_t classic_receive_available(std::uint64_t connection) const override
        {
            auto state = find_classic_connection(connection);
            if (!state) return 0;
            std::scoped_lock lock(state->receive_mutex);
            return static_cast<std::int32_t>(state->received.size());
        }

        Error classic_send_bytes(std::uint64_t connection, const std::uint8_t* data, std::size_t size, std::string& message) override
        {
            auto state = find_classic_connection(connection);
            if (!state) return invalid_connection(message);
            if (!state->connected) { message = "Classic connection is not open"; return Error::Disconnected; }
            if (size == 0) { message.clear(); return Error::Ok; }

            // Queued here and written from the main queue, where IOBluetooth
            // calls back, one MTU-sized writeAsync at a time.
            bool start = false;
            {
                std::scoped_lock lock(state->send_mutex);
                if (state->send_pending + size > kClassicSendCap)
                {
                    message = "The connection's send queue would pass 1 MiB; send less at a time";
                    return Error::Busy;
                }
                state->outbound.insert(state->outbound.end(), data, data + size);
                state->send_pending += size;
                start = !state->writing;
                if (start) state->writing = true;
            }
            if (start)
            {
                AppleBackend* self = this;
                std::weak_ptr<int> alive = lifetime_;
                dispatch_async(dispatch_get_main_queue(), ^{
                    if (alive.expired()) return;
                    self->classic_write_next(connection);
                });
            }
            message.clear();
            return Error::Ok;
        }

        std::size_t classic_receive_bytes(std::uint64_t connection, std::uint8_t* out, std::size_t max_size) override
        {
            auto state = find_classic_connection(connection);
            if (!state) return 0;
            std::size_t n = 0;
            bool drained = false;
            {
                std::scoped_lock lock(state->receive_mutex);
                n = std::min(max_size, state->received.size());
                std::copy(state->received.begin(), state->received.begin() + n, out);
                state->received.erase(state->received.begin(), state->received.begin() + n);
                // The next data to arrive is announced again.
                state->data_pending = false;
                drained = state->finished && state->received.empty();
            }
            // The last bytes of a connection that has ended: nothing is left
            // to keep it registered for.
            if (drained)
            {
                std::scoped_lock lock(classic_mutex_);
                auto it = classic_connections_.find(connection);
                if (it != classic_connections_.end() && it->second == state) classic_connections_.erase(it);
            }
            return n;
        }

        Error classic_server_start(const std::string& name, const std::string& service_uuid, std::string& message) override
        {
            if (const Error e = require_managers(message); e != Error::Ok) return e;
            if (classic_server_running_) { message.clear(); return Error::Ok; }

            IOBluetoothSDPUUID* uuid = classic_uuid_from_string(service_uuid);
            if (!uuid) { message = "Invalid service UUID"; return Error::InvalidArgument; }

            NSString* serviceName = [NSString stringWithUTF8String:name.c_str()];
            if (!serviceName) { message = "The service name is not valid UTF-8"; return Error::InvalidArgument; }

            // Laid out like Apple's SerialPortDictionary.plist. The RFCOMM
            // channel must be an explicit 1-byte unsigned integer: a bare
            // NSNumber is encoded as a 4-byte value, which macOS clients
            // tolerate but Windows' SDP parser rejects, so the service is
            // never matched there. The channel number is only a preference;
            // publishing assigns a free one, read back below.
            NSDictionary* rfcommChannel = @{
                @"DataElementType" : @1,  // unsigned integer
                @"DataElementSize" : @1,  // 1 byte
                @"DataElementValue" : @10,
            };
            NSDictionary* serviceDict = @{
                @"0001 - ServiceClassIDList" : @[ uuid ],
                @"0004 - ProtocolDescriptorList" : @[
                    @[ [IOBluetoothSDPUUID uuid16:kBluetoothSDPUUID16L2CAP] ],
                    @[ [IOBluetoothSDPUUID uuid16:kBluetoothSDPUUID16RFCOMM], rfcommChannel ],
                ],
                // Public Browse Root, so SDP browsing clients list the service too.
                @"0005 - BrowseGroupList" : @[ [IOBluetoothSDPUUID uuid16:kBluetoothSDPUUID16ServiceClassPublicBrowseGroup] ],
                @"0100 - ServiceName" : serviceName,
            };

            IOBluetoothSDPServiceRecord* record = [IOBluetoothSDPServiceRecord publishedServiceRecordWithDictionary:serviceDict];
            if (!record) { message = "Classic service record could not be published"; return Error::OperationFailed; }

            BluetoothRFCOMMChannelID channelID = 0;
            const IOReturn channelStatus = [record getRFCOMMChannelID:&channelID];
            BluetoothSDPServiceRecordHandle recordHandle = 0;
            [record getServiceRecordHandle:&recordHandle];
            GMBT_LOG("Classic server: record published, handle=0x%08x RFCOMM channel=%d IOReturn=0x%08x uuid=%s local_address=%s",
                (unsigned)recordHandle, (int)channelID, channelStatus, service_uuid.c_str(),
                gmbt_string([[IOBluetoothHostController defaultController] addressAsString]).c_str());
            if (channelStatus != kIOReturnSuccess)
            {
                [record removeServiceRecord];
                message = "Classic service record has no RFCOMM channel";
                return Error::OperationFailed;
            }

            GMBTClassicServerHub* hub = [GMBTClassicServerHub new];
            AppleBackend* self = this;
            std::weak_ptr<int> alive = lifetime_;
            hub.onChannelOpened = ^(IOBluetoothRFCOMMChannel* channel) {
                if (alive.expired()) return;
                self->handle_classic_server_channel_opened(channel);
            };

            IOBluetoothUserNotification* notification =
                [IOBluetoothRFCOMMChannel registerForChannelOpenNotifications:hub
                                                                      selector:@selector(rfcommChannelOpened:channel:)
                                                                withChannelID:channelID
                                                                     direction:kIOBluetoothUserNotificationChannelDirectionIncoming];
            if (!notification)
            {
                [record removeServiceRecord];
                message = "Classic server could not listen for incoming connections";
                return Error::OperationFailed;
            }

            classic_server_hub_ = hub;
            classic_server_notification_ = notification;
            classic_server_record_ = record;
            classic_server_running_ = true;
            message.clear();
            return Error::Ok;
        }

        Error classic_server_stop(std::string& message) override
        {
            if (classic_server_notification_) { [classic_server_notification_ unregister]; classic_server_notification_ = nil; }
            if (classic_server_record_) { [classic_server_record_ removeServiceRecord]; classic_server_record_ = nil; }
            if (classic_server_hub_) classic_server_hub_.onChannelOpened = nil;
            classic_server_hub_ = nil;
            classic_server_running_ = false;
            message.clear();
            return Error::Ok;
        }

        bool classic_server_is_running() const override { return classic_server_running_; }

        Error pair(std::uint64_t device_handle, const DiscoveredDevice& device, std::string& message) override
        {
            if (const Error e = require_managers(message); e != Error::Ok) return e;
            if (device.transport != Transport::Classic)
            {
                message = "Bluetooth pairing is only supported for Classic devices on macOS";
                return Error::NotSupported;
            }
            if (!device.address_available || device.address.empty())
            {
                message = "Classic device address is unavailable";
                return Error::InvalidArgument;
            }

            IOBluetoothDevice* btDevice = classic_device_for_address(device.address);
            if (!btDevice)
            {
                message = "Classic device could not be resolved";
                return Error::NotFound;
            }

            IOBluetoothDevicePair* pair = [IOBluetoothDevicePair pairWithDevice:btDevice];
            if (!pair)
            {
                message = "Bluetooth pairing could not be created";
                return Error::OperationFailed;
            }

            GMBTDevicePairDelegate* delegate = [GMBTDevicePairDelegate new];
            AppleBackend* self = this;
            std::weak_ptr<int> alive = lifetime_;
            delegate.onFinished = ^(IOReturn error) {
                if (alive.expired()) return;
                self->handle_pair_finished(device_handle, error);
            };
            pair.delegate = delegate;

            std::uint64_t generation = 0;
            {
                std::scoped_lock lock(pair_mutex_);
                generation = ++pair_generation_;
                pending_pairs_[device_handle] = PendingPair{ pair, delegate, generation };
            }

            const IOReturn status = [pair start];
            if (status != kIOReturnSuccess)
            {
                std::scoped_lock lock(pair_mutex_);
                pending_pairs_.erase(device_handle);
                message = "Bluetooth pairing could not start";
                return Error::OperationFailed;
            }

            // A dismissed pairing dialog or a peer that never answers would
            // otherwise leave the pair callback waiting forever.
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(kPairTimeoutSeconds * NSEC_PER_SEC)),
                           dispatch_get_main_queue(), ^{
                if (alive.expired()) return;
                self->handle_pair_timeout(device_handle, generation);
            });

            message.clear();
            return Error::Ok;
        }

        bool is_paired(const DiscoveredDevice& device) const override
        {
            if (device.transport != Transport::Classic || !device.address_available || device.address.empty())
                return false;
            // Touching IOBluetooth would show the prompt the game has not asked for.
            if (!managers_created())
                return false;

            IOBluetoothDevice* btDevice = const_cast<AppleBackend*>(this)->classic_device_for_address(device.address);
            return btDevice != nil && [btDevice isPaired];
        }

        // The Classic devices macOS holds a bond with, in range or not. The
        // list is local, so the answer goes out at once.
        Error paired_devices_query(std::uint64_t query_id, std::string& message) override
        {
            if (const Error e = require_managers(message); e != Error::Ok) return e;

            BackendEvent ev;
            ev.type = BackendEventType::DevicesQueried;
            ev.transport = Transport::Classic;
            ev.op_id = query_id;
            for (id entry in [IOBluetoothDevice pairedDevices])
            {
                if (![entry isKindOfClass:[IOBluetoothDevice class]]) continue;
                ev.devices.push_back(classic_device_entry((IOBluetoothDevice*)entry));
            }
            hooks_.push_event(std::move(ev));
            message.clear();
            return Error::Ok;
        }

#else

        Error classic_scan_start(std::string&m) override {m="Bluetooth Classic is not supported on this platform";return Error::NotSupported;}
        Error classic_scan_stop(std::string&m) override {m.clear();return Error::Ok;}
        bool classic_scan_is_running() const override {return false;}
        Error classic_connect(std::uint64_t,const DiscoveredDevice&,const std::string&,std::string&m) override {m="Bluetooth Classic is not supported on this platform";return Error::NotSupported;}
        Error classic_disconnect(std::uint64_t,std::string&m) override {m="Bluetooth Classic is not supported on this platform";return Error::NotSupported;}
        bool classic_connection_is_connected(std::uint64_t) const override {return false;}
        std::int32_t classic_receive_available(std::uint64_t) const override {return 0;}
        Error classic_send_bytes(std::uint64_t,const std::uint8_t*,std::size_t,std::string&m) override {m="Bluetooth Classic is not supported on this platform";return Error::NotSupported;}
        std::size_t classic_receive_bytes(std::uint64_t,std::uint8_t*,std::size_t) override {return 0;}
        Error classic_server_start(const std::string&,const std::string&,std::string&m) override {m="Bluetooth Classic server is not supported on this platform";return Error::NotSupported;}
        Error classic_server_stop(std::string&m) override {m.clear();return Error::Ok;}
        bool classic_server_is_running() const override {return false;}

#endif // TARGET_OS_OSX

    private:
        CoreHooks hooks_;
        GMBluetoothAppleTransport* transport_ = nil;
        mutable std::mutex mutex_;
        std::unordered_map<std::uint64_t,std::string> le_connection_to_id_;
        std::unordered_map<std::string,std::uint64_t> le_id_to_connection_;

        // Expires with the backend, so delayed blocks can tell it is gone.
        std::shared_ptr<int> lifetime_ = std::make_shared<int>(0);

        // A permission_request is waiting for the user's answer. The
        // generation tells a poll for it from one for an earlier request.
        std::atomic_bool permission_pending_{false};
        std::atomic<std::uint64_t> permission_generation_{0};

        static constexpr double kPermissionPollSeconds = 0.5;

        // BluetoothFeature raw values from spec.gmidl that feature_supported
        // answers itself.
        static constexpr std::int32_t kFeatureLeAdvertiseName = 3;
        static constexpr std::int32_t kFeatureLeAdvertiseServiceUuids = 4;
        static constexpr std::int32_t kFeatureClassicPairing = 17;
        static constexpr std::int32_t kFeaturePermissionRequest = 20;
        static constexpr std::int32_t kFeatureLeMtuRequest = 21;
        static constexpr std::int32_t kFeatureLeReadRssi = 22;
        static constexpr std::int32_t kFeatureLeConnectionPriority = 23;
        static constexpr std::int32_t kFeatureRequestEnable = 24;
        static constexpr std::int32_t kFeaturePairedDevicesQuery = 25;

        static Error invalid_connection(std::string&message){message="Invalid connection handle";return Error::InvalidHandle;}

        // The managers exist once authorization was decided at initialize or
        // permission_request created them.
        bool managers_created() const { return transport_ && transport_.centralManager != nil; }

        // Every call that uses the radio fails until then, rather than show
        // the system prompt from a call the game did not mean as a request.
        Error require_managers(std::string& message) const
        {
            if (!transport_) { message = "Bluetooth backend is not initialized"; return Error::NotInitialized; }
            if (!managers_created()) { message = "call bluetooth_permission_request first"; return Error::PermissionDenied; }
            return Error::Ok;
        }

        void push_permission_result()
        {
            BackendEvent ev;
            ev.type = BackendEventType::PermissionResult;
            ev.value = static_cast<std::int32_t>(permission_status());
            hooks_.push_event(std::move(ev));
        }

        // Both managers report state changes, in any order and more than once;
        // the first one after the user answered pushes the result and clears
        // the pending flag, so the result goes out once.
        void on_manager_state()
        {
            if (!permission_pending_ || authorization_not_determined()) return;
            if (permission_pending_.exchange(false))
            {
                GMBT_LOG("Bluetooth permission answered: status %d", static_cast<int>(permission_status()));
                push_permission_result();
            }
        }

        // A fallback for an answer no state change reports: rechecks the
        // authorization on the main queue until the request is answered or
        // superseded.
        void schedule_permission_poll(std::uint64_t generation)
        {
            AppleBackend* self = this;
            std::weak_ptr<int> alive = lifetime_;
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(kPermissionPollSeconds * NSEC_PER_SEC)),
                           dispatch_get_main_queue(), ^{
                if (alive.expired()) return;
                if (self->permission_generation_ != generation || !self->permission_pending_) return;
                self->on_manager_state();
                if (self->permission_pending_) self->schedule_permission_poll(generation);
            });
        }

        std::string id_for_connection(std::uint64_t c) const
        { std::scoped_lock lock(mutex_); auto it=le_connection_to_id_.find(c); return it==le_connection_to_id_.end()?std::string{}:it->second; }
        // The id entry goes only while it still names c: a later connect to
        // the same peripheral may have taken it.
        void remove_connection(std::uint64_t c)
        {
            std::scoped_lock lock(mutex_);
            auto it=le_connection_to_id_.find(c);
            if(it==le_connection_to_id_.end()) return;
            auto id=le_id_to_connection_.find(it->second);
            if(id!=le_id_to_connection_.end() && id->second==c) le_id_to_connection_.erase(id);
            le_connection_to_id_.erase(it);
        }
        std::uint64_t connection_for_id(const std::string&id) const
        { std::scoped_lock lock(mutex_); auto it=le_id_to_connection_.find(id); return it==le_id_to_connection_.end()?0:it->second; }

        static std::string identifier_from_device(const DiscoveredDevice& d)
        {
            constexpr const char* prefix="apple:ble:";
            if(d.id.rfind(prefix,0)==0) return d.id.substr(std::char_traits<char>::length(prefix));
            if(d.address_available&&!d.address.empty()) return d.address;
            return {};
        }

        // A peripheral CoreBluetooth handed back by identifier or service, as
        // a scan would report it minus the advertisement and the RSSI.
        static DiscoveredDevice le_device_from_peripheral(CBPeripheral* peripheral)
        {
            DiscoveredDevice d;
            d.transport = Transport::LowEnergy;
            d.id = "apple:ble:" + gmbt_string(gmbt_peripheral_key(peripheral));
            d.name = gmbt_string(peripheral.name);
            d.address_available = false;
            d.connectable = true;
            return d;
        }

#if TARGET_OS_OSX

        struct ClassicConnectionState
        {
            std::uint64_t connection = 0;
            IOBluetoothRFCOMMChannel* channel = nil;
            GMBTClassicChannelDelegate* delegate = nil;
            std::mutex receive_mutex;
            std::deque<std::uint8_t> received;
            // Under receive_mutex. A ClassicDataAvailable went out and the
            // game has not read since; the next one waits for
            // classic_receive_bytes.
            bool data_pending = false;
            // Under receive_mutex. The channel is gone; the state stays
            // registered only while it holds bytes the game has not read.
            bool finished = false;
            std::atomic_bool connected{false};

            // classic_send_bytes queues here; the main queue writes it out.
            std::mutex send_mutex;
            std::deque<std::uint8_t> outbound;
            // Queued plus the chunk in flight.
            std::size_t send_pending = 0;
            // Writing has been started on the main queue and has not yet run
            // out of queued bytes.
            bool writing = false;
            // The bytes of the writeAsync in flight, kept until it completes.
            NSMutableData* in_flight = nil;
        };

        // Bytes a connection may hold waiting to be sent, and waiting to be
        // read by the game.
        static constexpr std::size_t kClassicSendCap = 1024 * 1024;
        static constexpr std::size_t kClassicReceiveCap = 1024 * 1024;

        mutable std::mutex classic_mutex_;
        std::unordered_map<std::uint64_t, std::shared_ptr<ClassicConnectionState>> classic_connections_;
        std::unordered_map<std::string, IOBluetoothDevice*> classic_devices_by_address_;
        std::unordered_map<std::uint64_t, GMBTSDPQueryHandler*> classic_pending_sdp_;

        IOBluetoothDeviceInquiry* classic_inquiry_ = nil;
        GMBTClassicInquiryDelegate* classic_inquiry_delegate_ = nil;

        IOBluetoothSDPServiceRecord* classic_server_record_ = nil;
        IOBluetoothUserNotification* classic_server_notification_ = nil;
        GMBTClassicServerHub* classic_server_hub_ = nil;
        std::atomic_bool classic_server_running_{false};

        std::shared_ptr<ClassicConnectionState> find_classic_connection(std::uint64_t connection) const
        {
            std::scoped_lock lock(classic_mutex_);
            auto it = classic_connections_.find(connection);
            return it == classic_connections_.end() ? nullptr : it->second;
        }

        // The MAC-style "XX:XX:XX:XX:XX:XX" address stored on DiscoveredDevice
        // is looked up directly as an IOBluetoothDevice - no string round-trip
        // is required, unlike Windows' raw-socket SOCKADDR_BTH path.
        IOBluetoothDevice* classic_device_for_address(const std::string& address)
        {
            {
                std::scoped_lock lock(classic_mutex_);
                auto it = classic_devices_by_address_.find(address);
                if (it != classic_devices_by_address_.end()) return it->second;
            }
            BluetoothDeviceAddress raw{};
            unsigned int bytes[6];
            std::string normalized = address;
            for (auto& c : normalized) if (c == '-') c = ':';
            if (std::sscanf(normalized.c_str(), "%02x:%02x:%02x:%02x:%02x:%02x",
                             &bytes[0], &bytes[1], &bytes[2], &bytes[3], &bytes[4], &bytes[5]) != 6)
                return nil;
            for (int i = 0; i < 6; ++i) raw.data[i] = static_cast<unsigned char>(bytes[i]);

            IOBluetoothDevice* device = [IOBluetoothDevice deviceWithAddress:&raw];
            if (device)
            {
                std::scoped_lock lock(classic_mutex_);
                classic_devices_by_address_[address] = device;
            }
            return device;
        }

        static std::string classic_format_address(NSString* addressString)
        {
            std::string s = gmbt_string(addressString);
            for (auto& c : s) c = (c == '-') ? ':' : static_cast<char>(::toupper(static_cast<unsigned char>(c)));
            return s;
        }

        // CBUUID already parses both 16-bit short-form ("1101") and full
        // 128-bit service UUID strings; reuse it to build the IOBluetoothSDPUUID
        // IOBluetooth itself expects rather than duplicating that parsing here.
        static IOBluetoothSDPUUID* classic_uuid_from_string(const std::string& text)
        {
            if (text.empty()) return nil;
            CBUUID* cb = [CBUUID UUIDWithString:gmbt_ns(text)];
            if (!cb || !cb.data) return nil;
            return [IOBluetoothSDPUUID uuidWithBytes:cb.data.bytes length:(int)cb.data.length];
        }

        // "XX:XX:XX:XX:XX:XX", as classic_format_address writes it; dashes
        // and lowercase hex are taken too.
        static bool classic_address_is_well_formed(const std::string& address)
        {
            if (address.size() != 17) return false;
            for (std::size_t i = 0; i < address.size(); ++i)
            {
                const char c = address[i];
                if (i % 3 == 2) { if (c != ':' && c != '-') return false; }
                else if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
            }
            return true;
        }

        // The cache entry for a Classic device, which is also kept for
        // classic_device_for_address. Used for inquiry results, ids and bonds.
        DiscoveredDevice classic_device_entry(IOBluetoothDevice* device)
        {
            const std::string address = classic_format_address([device addressString]);
            {
                std::scoped_lock lock(classic_mutex_);
                classic_devices_by_address_[address] = device;
            }
            DiscoveredDevice d;
            d.transport = Transport::Classic;
            d.id = "apple:classic:" + address;
            d.name = gmbt_string([device name]);
            d.address = address;
            d.address_available = true;
            d.connectable = true;
            return d;
        }

        void handle_classic_device_found(IOBluetoothDevice* device)
        {
            if (!device) return;
            hooks_.upsert_device(classic_device_entry(device));
        }

        void handle_classic_inquiry_complete(IOReturn error)
        {
            gmbt_release_after_callback(classic_inquiry_);
            gmbt_release_after_callback(classic_inquiry_delegate_);
            classic_inquiry_ = nil;
            classic_inquiry_delegate_ = nil;
            BackendEvent ev;
            ev.type = BackendEventType::ScanStopped;
            ev.transport = Transport::Classic;
            if (error != kIOReturnSuccess)
            {
                char text[96];
                std::snprintf(text, sizeof(text), "Classic device inquiry ended with an error (IOReturn 0x%08x)", error);
                ev.error = Error::OperationFailed;
                ev.message = text;
            }
            hooks_.push_event(std::move(ev));
        }

        static constexpr double kClassicSdpTimeoutSeconds = 15.0;

        // Finds the RFCOMM channel for service_uuid in the device's current SDP
        // records. Another service's channel is never used in its place.
        static bool classic_find_rfcomm_channel(IOBluetoothDevice* device, const std::string& service_uuid,
                                                BluetoothRFCOMMChannelID& channelID)
        {
            IOBluetoothSDPUUID* uuid = classic_uuid_from_string(service_uuid);
            if (!uuid) return false;
            IOBluetoothSDPServiceRecord* record = [device getServiceRecordForUUID:uuid];
            return record && [record getRFCOMMChannelID:&channelID] == kIOReturnSuccess;
        }

        void handle_classic_sdp_timeout(std::uint64_t connection)
        {
            {
                std::scoped_lock lock(classic_mutex_);
                auto sdp = classic_pending_sdp_.find(connection);
                if (sdp == classic_pending_sdp_.end()) return; // already completed
                // IOBluetooth may still deliver sdpQueryComplete: to it.
                sdp->second.onComplete = nil;
                gmbt_park(sdp->second);
                classic_pending_sdp_.erase(sdp);
            }
            GMBT_LOG("Classic SDP query timed out after %.0fs (connection %llu)",
                kClassicSdpTimeoutSeconds, static_cast<unsigned long long>(connection));
            complete_classic_connect(connection, Error::Timeout, "Classic service discovery timed out");
        }

        void handle_classic_sdp_complete(std::uint64_t connection, IOBluetoothDevice* device, IOReturn status, std::string service_uuid)
        {
            GMBT_LOG("Classic SDP complete: IOReturn=0x%08x records=%d",
                status, (int)[[device services] count]);
            {
                std::scoped_lock lock(classic_mutex_);
                auto sdp = classic_pending_sdp_.find(connection);
                if (sdp == classic_pending_sdp_.end()) return; // timed out meanwhile
                gmbt_release_after_callback(sdp->second);
                classic_pending_sdp_.erase(sdp);
                if (classic_connections_.find(connection) == classic_connections_.end()) return; // disconnected/cancelled meanwhile
            }
            if (status != kIOReturnSuccess)
            {
                complete_classic_connect(connection, Error::NotFound, "Classic service discovery failed");
                return;
            }

            BluetoothRFCOMMChannelID channelID = 0;
            if (!classic_find_rfcomm_channel(device, service_uuid, channelID))
            {
                complete_classic_connect(connection, Error::NotFound, "service " + service_uuid + " not found");
                return;
            }
            classic_open_rfcomm(connection, device, channelID);
        }

        void classic_open_rfcomm(std::uint64_t connection, IOBluetoothDevice* device, BluetoothRFCOMMChannelID channelID)
        {
            GMBTClassicChannelDelegate* delegate = [GMBTClassicChannelDelegate new];
            AppleBackend* self = this;
            std::weak_ptr<int> alive = lifetime_;
            delegate.onOpenComplete = ^(IOReturn openStatus) {
                if (alive.expired()) return;
                self->handle_classic_channel_open_complete(connection, openStatus);
            };
            delegate.onData = ^(NSData* data) {
                if (alive.expired()) return;
                self->handle_classic_channel_data(connection, data);
            };
            delegate.onClose = ^{
                if (alive.expired()) return;
                self->handle_classic_channel_closed(connection);
            };
            delegate.onWriteComplete = ^(IOReturn writeStatus) {
                if (alive.expired()) return;
                self->handle_classic_write_complete(connection, writeStatus);
            };

            IOBluetoothRFCOMMChannel* channel = nil;
            const IOReturn openStatus = [device openRFCOMMChannelAsync:&channel withChannelID:channelID delegate:delegate];
            GMBT_LOG("Classic RFCOMM open (channel %d) -> IOReturn=0x%08x", (int)channelID, openStatus);
            if (openStatus != kIOReturnSuccess || !channel)
            {
                complete_classic_connect(connection, Error::ConnectionFailed, "RFCOMM channel could not be opened");
                return;
            }

            {
                std::scoped_lock lock(classic_mutex_);
                auto it = classic_connections_.find(connection);
                if (it != classic_connections_.end())
                {
                    it->second->channel = channel;
                    it->second->delegate = delegate;
                    return;
                }
            }

            // Disconnected meanwhile. Closed with the lock released: a close
            // that calls rfcommChannelClosed: synchronously would otherwise
            // re-lock classic_mutex_ on this thread.
            classic_detach_channel(channel, delegate);
        }

        void handle_classic_channel_open_complete(std::uint64_t connection, IOReturn status)
        {
            GMBT_LOG("Classic RFCOMM open complete: IOReturn=0x%08x", status);
            if (status != kIOReturnSuccess)
            {
                complete_classic_connect(connection, Error::ConnectionFailed, "RFCOMM channel open failed");
                return;
            }
            {
                std::scoped_lock lock(classic_mutex_);
                auto it = classic_connections_.find(connection);
                if (it == classic_connections_.end()) return;
                it->second->connected = true;
            }
            complete_classic_connect(connection, Error::Ok, "");
        }

        void complete_classic_connect(std::uint64_t connection, Error error, const std::string& message)
        {
            if (error != Error::Ok)
            {
                std::scoped_lock lock(classic_mutex_);
                auto it = classic_connections_.find(connection);
                if (it != classic_connections_.end())
                {
                    gmbt_release_after_callback(it->second->channel);
                    gmbt_release_after_callback(it->second->delegate);
                    classic_connections_.erase(it);
                }
            }
            BackendEvent ev;
            ev.type = BackendEventType::ClassicConnected;
            ev.transport = Transport::Classic;
            ev.connection = connection;
            ev.error = error;
            ev.message = message;
            hooks_.push_event(std::move(ev));
        }

        static constexpr double kPairTimeoutSeconds = 60.0;

        struct PendingPair
        {
            IOBluetoothDevicePair* pair = nil;
            GMBTDevicePairDelegate* delegate = nil;
            // Tells a timeout for this pairing from one for a later pairing of
            // the same device.
            std::uint64_t generation = 0;
        };

        mutable std::mutex pair_mutex_;
        std::unordered_map<std::uint64_t, PendingPair> pending_pairs_;
        std::uint64_t pair_generation_ = 0;

        // Stops a pairing the backend gives up on, with nothing of ours left
        // for IOBluetooth to call.
        static void classic_abandon_pair(const PendingPair& pending)
        {
            if (pending.delegate) pending.delegate.onFinished = nil;
            if (pending.pair)
            {
                pending.pair.delegate = nil;
                [pending.pair stop];
            }
            gmbt_release_after_callback(pending.pair);
            gmbt_release_after_callback(pending.delegate);
        }

        // Detaches a channel from its delegate before closing and releasing
        // it, so a close or a late open completion calls nothing of ours.
        static void classic_detach_channel(IOBluetoothRFCOMMChannel* channel, GMBTClassicChannelDelegate* delegate)
        {
            if (delegate)
            {
                delegate.onOpenComplete = nil;
                delegate.onData = nil;
                delegate.onClose = nil;
                delegate.onWriteComplete = nil;
            }
            if (channel)
            {
                [channel setDelegate:nil];
                [channel closeChannel];
            }
            gmbt_release_after_callback(channel);
            gmbt_release_after_callback(delegate);
        }

        // Forgets what was queued to send. The chunk IOBluetooth may still be
        // writing is let go no sooner than its channel.
        static void classic_drop_writes(ClassicConnectionState& state)
        {
            NSMutableData* in_flight = nil;
            {
                std::scoped_lock lock(state.send_mutex);
                state.outbound.clear();
                state.send_pending = 0;
                in_flight = state.in_flight;
                state.in_flight = nil;
            }
            gmbt_release_after_callback(in_flight);
        }

        void handle_pair_timeout(std::uint64_t device_handle, std::uint64_t generation)
        {
            PendingPair pending;
            {
                std::scoped_lock lock(pair_mutex_);
                auto it = pending_pairs_.find(device_handle);
                if (it == pending_pairs_.end() || it->second.generation != generation) return; // already finished
                pending = it->second;
                pending_pairs_.erase(it);
            }
            GMBT_LOG("Bluetooth pairing timed out after %.0fs (device %llu)",
                kPairTimeoutSeconds, static_cast<unsigned long long>(device_handle));
            classic_abandon_pair(pending);

            BackendEvent ev;
            ev.type = BackendEventType::DevicePaired;
            ev.transport = Transport::Classic;
            ev.device = device_handle;
            ev.error = Error::Timeout;
            ev.message = "Bluetooth pairing timed out";
            hooks_.push_event(std::move(ev));
        }

        void handle_pair_finished(std::uint64_t device_handle, IOReturn error)
        {
            {
                std::scoped_lock lock(pair_mutex_);
                auto it = pending_pairs_.find(device_handle);
                if (it == pending_pairs_.end()) return; // timed out meanwhile
                gmbt_release_after_callback(it->second.pair);
                gmbt_release_after_callback(it->second.delegate);
                pending_pairs_.erase(it);
            }

            BackendEvent ev;
            ev.type = BackendEventType::DevicePaired;
            ev.transport = Transport::Classic;
            ev.device = device_handle;
            if (error == kIOReturnSuccess)
            {
                ev.error = Error::Ok;
            }
            else
            {
                ev.error = Error::OperationFailed;
                ev.message = "Bluetooth pairing failed";
            }
            hooks_.push_event(std::move(ev));
        }

        // Data is announced once, and again only after the game has read. A
        // channel cannot be paused, so bytes that would take the queue past
        // 1 MiB end the connection instead; what fit stays readable.
        void handle_classic_channel_data(std::uint64_t connection, NSData* data)
        {
            auto state = find_classic_connection(connection);
            if (!state || !state->connected) return;
            std::int32_t available = 0;
            bool announce = false;
            bool full = false;
            {
                std::scoped_lock lock(state->receive_mutex);
                const std::uint8_t* bytes = static_cast<const std::uint8_t*>(data.bytes);
                const std::size_t room = kClassicReceiveCap - std::min(kClassicReceiveCap, state->received.size());
                const std::size_t taken = std::min<std::size_t>(room, data.length);
                if (bytes && taken > 0) state->received.insert(state->received.end(), bytes, bytes + taken);
                full = taken < data.length;
                available = static_cast<std::int32_t>(state->received.size());
                announce = taken > 0 && !state->data_pending;
                if (announce) state->data_pending = true;
            }
            if (announce)
            {
                BackendEvent ev;
                ev.type = BackendEventType::ClassicDataAvailable;
                ev.transport = Transport::Classic;
                ev.connection = connection;
                ev.value = available;
                hooks_.push_event(std::move(ev));
            }
            if (full) classic_end_connection(connection, Error::Disconnected, "receive buffer full", true);
        }

        void handle_classic_channel_closed(std::uint64_t connection)
        {
            classic_end_connection(connection, Error::Disconnected, "RFCOMM channel closed", false);
        }

        // Ends a connection and reports it once. close_channel is for an end
        // this backend decides; a channel IOBluetooth closed itself is only
        // let go. The state stays registered while it holds bytes the game has
        // not read, and classic_receive_bytes removes it once they are.
        void classic_end_connection(std::uint64_t connection, Error error, const std::string& message, bool close_channel)
        {
            std::shared_ptr<ClassicConnectionState> state;
            IOBluetoothRFCOMMChannel* channel = nil;
            GMBTClassicChannelDelegate* delegate = nil;
            bool wasConnected = false;
            {
                std::scoped_lock lock(classic_mutex_);
                auto it = classic_connections_.find(connection);
                if (it == classic_connections_.end()) return;
                state = it->second;
                wasConnected = state->connected.exchange(false);
                channel = state->channel;
                delegate = state->delegate;
                state->channel = nil;
                state->delegate = nil;
                bool keep = false;
                if (wasConnected)
                {
                    std::scoped_lock receive_lock(state->receive_mutex);
                    state->finished = true;
                    keep = !state->received.empty();
                }
                if (!keep) classic_connections_.erase(it);
            }
            classic_drop_writes(*state);
            if (close_channel)
            {
                classic_detach_channel(channel, delegate);
            }
            else
            {
                gmbt_release_after_callback(channel);
                gmbt_release_after_callback(delegate);
            }

            // A channel that closes before it ever finished opening is reported
            // through the ClassicConnected completion path instead, not here.
            if (!wasConnected) return;
            BackendEvent ev;
            ev.type = BackendEventType::ClassicDisconnected;
            ev.transport = Transport::Classic;
            ev.connection = connection;
            ev.error = error;
            ev.message = message;
            hooks_.push_event(std::move(ev));
        }

        // Starts the next writeAsync unless one is in flight or nothing is
        // queued. Main queue only, where IOBluetooth calls back. A write takes
        // at most the channel's MTU (RFCOMM's default 127 while it reports
        // none) and a UInt16 length.
        void classic_write_next(std::uint64_t connection)
        {
            auto state = find_classic_connection(connection);
            if (!state) return;
            IOBluetoothRFCOMMChannel* channel = state->channel;
            NSMutableData* chunk = nil;
            {
                std::scoped_lock lock(state->send_mutex);
                if (state->in_flight) return;
                if (!state->connected || !channel || state->outbound.empty())
                {
                    state->writing = false;
                    return;
                }
                const BluetoothRFCOMMMTU mtu = [channel getMTU];
                const std::size_t limit = mtu == 0 ? 127 : std::min<std::size_t>(mtu, 65535);
                const std::size_t size = std::min(limit, state->outbound.size());
                chunk = [NSMutableData dataWithLength:size];
                std::copy(state->outbound.begin(), state->outbound.begin() + size, static_cast<std::uint8_t*>(chunk.mutableBytes));
                state->outbound.erase(state->outbound.begin(), state->outbound.begin() + size);
                state->in_flight = chunk;
            }
            const IOReturn status = [channel writeAsync:chunk.mutableBytes length:(UInt16)chunk.length refcon:nullptr];
            if (status != kIOReturnSuccess) classic_write_failed(connection, status);
        }

        void handle_classic_write_complete(std::uint64_t connection, IOReturn status)
        {
            auto state = find_classic_connection(connection);
            if (!state) return;
            {
                std::scoped_lock lock(state->send_mutex);
                if (state->in_flight)
                {
                    state->send_pending -= std::min<std::size_t>(state->send_pending, state->in_flight.length);
                    state->in_flight = nil;
                }
            }
            if (status != kIOReturnSuccess)
            {
                classic_write_failed(connection, status);
                return;
            }
            classic_write_next(connection);
        }

        // A send the game was told had been queued cannot go out: the
        // connection ends, and classic_disconnected says why.
        void classic_write_failed(std::uint64_t connection, IOReturn status)
        {
            char text[64];
            std::snprintf(text, sizeof(text), "Classic write failed (IOReturn 0x%08x)", status);
            GMBT_LOG("%s (connection %llu)", text, static_cast<unsigned long long>(connection));
            classic_end_connection(connection, Error::OperationFailed, text, true);
        }

        void handle_classic_server_channel_opened(IOBluetoothRFCOMMChannel* channel)
        {
            IOBluetoothDevice* device = [channel getDevice];
            const std::string address = classic_format_address([device addressString]);
            GMBT_LOG("Classic server: incoming RFCOMM channel %d from %s (%s)",
                (int)[channel getChannelID], address.c_str(), gmbt_string([device name]).c_str());
            DiscoveredDevice d;
            d.transport = Transport::Classic;
            d.id = "apple:classic:" + address;
            d.name = gmbt_string([device name]);
            d.address = address;
            d.address_available = true;
            d.connectable = true;
            const std::uint64_t device_handle = hooks_.upsert_device(d);
            const std::uint64_t connection = hooks_.create_classic_connection(device_handle);

            auto state = std::make_shared<ClassicConnectionState>();
            state->connection = connection;
            state->connected = true;

            GMBTClassicChannelDelegate* delegate = [GMBTClassicChannelDelegate new];
            AppleBackend* self = this;
            std::weak_ptr<int> alive = lifetime_;
            delegate.onData = ^(NSData* data) {
                if (alive.expired()) return;
                self->handle_classic_channel_data(connection, data);
            };
            delegate.onClose = ^{
                if (alive.expired()) return;
                self->handle_classic_channel_closed(connection);
            };
            delegate.onWriteComplete = ^(IOReturn writeStatus) {
                if (alive.expired()) return;
                self->handle_classic_write_complete(connection, writeStatus);
            };
            [channel setDelegate:delegate];
            state->channel = channel;
            state->delegate = delegate;

            {
                std::scoped_lock lock(classic_mutex_);
                classic_connections_[connection] = state;
            }

            BackendEvent ev;
            ev.type = BackendEventType::ClassicClientConnected;
            ev.transport = Transport::Classic;
            ev.connection = connection;
            ev.device = device_handle;
            hooks_.push_event(std::move(ev));
        }

        // Every IOBluetooth object still in flight is detached before it is let
        // go: its blocks are nilled, its delegate cleared, and anything that
        // may still be called is kept alive until then. The blocks also check
        // lifetime_, which expires with this backend.
        void classic_shutdown()
        {
            if (classic_inquiry_delegate_)
            {
                classic_inquiry_delegate_.onDeviceFound = nil;
                classic_inquiry_delegate_.onComplete = nil;
            }
            if (classic_inquiry_)
            {
                [classic_inquiry_ setDelegate:nil];
                [classic_inquiry_ stop];
            }
            gmbt_release_after_callback(classic_inquiry_);
            gmbt_release_after_callback(classic_inquiry_delegate_);
            classic_inquiry_ = nil;
            classic_inquiry_delegate_ = nil;

            std::unordered_map<std::uint64_t, std::shared_ptr<ClassicConnectionState>> connections;
            std::unordered_map<std::uint64_t, GMBTSDPQueryHandler*> pending_sdp;
            {
                std::scoped_lock lock(classic_mutex_);
                connections.swap(classic_connections_);
                pending_sdp.swap(classic_pending_sdp_);
                classic_devices_by_address_.clear();
            }
            for (auto& [connection, state] : connections)
            {
                (void)connection;
                classic_drop_writes(*state);
                classic_detach_channel(state->channel, state->delegate);
            }
            for (auto& [connection, handler] : pending_sdp)
            {
                (void)connection;
                handler.onComplete = nil;
                gmbt_park(handler);
            }

            std::unordered_map<std::uint64_t, PendingPair> pairs;
            {
                std::scoped_lock lock(pair_mutex_);
                pairs.swap(pending_pairs_);
            }
            for (auto& [device_handle, pending] : pairs)
            {
                (void)device_handle;
                classic_abandon_pair(pending);
            }

            if (classic_server_notification_) { [classic_server_notification_ unregister]; classic_server_notification_ = nil; }
            if (classic_server_record_) { [classic_server_record_ removeServiceRecord]; classic_server_record_ = nil; }
            if (classic_server_hub_) classic_server_hub_.onChannelOpened = nil;
            classic_server_hub_ = nil;
            classic_server_running_ = false;
        }

#endif // TARGET_OS_OSX

        void on_op_completed(NSNumber* op_id, Error error, std::string message, LeOpResult result)
        {
            BackendEvent ev;
            ev.type = BackendEventType::LeOpCompleted;
            ev.transport = Transport::LowEnergy;
            ev.op_id = op_id ? op_id.unsignedLongLongValue : 0;
            ev.error = error;
            ev.message = std::move(message);
            ev.result = std::move(result);
            hooks_.push_event(std::move(ev));
        }

        // The transport events the core routes as LE events. Anything else is
        // not forwarded.
        static bool is_routed_event(NSString* type)
        {
            static NSSet<NSString*>* routed = [NSSet setWithArray:@[
                @"bt_state_changed",
                @"bt_le_peripheral_open",
                @"bt_le_peripheral_disconnect",
                @"bt_le_characteristic_value_changed",
                @"bt_le_server_connection_state_changed",
                @"bt_le_server_characteristic_read_request",
                @"bt_le_server_characteristic_write_request",
                @"bt_le_server_services_reset",
            ]];
            return [routed containsObject:type];
        }

        void upsert_scan_result(NSDictionary* params)
        {
            NSString* address = [params[@"address"] isKindOfClass:[NSString class]] ? params[@"address"] : nil;
            if (!address) return;
            NSString* name = [params[@"name"] isKindOfClass:[NSString class]] ? params[@"name"] : nil;
            NSNumber* rssi = [params[@"raw_signal"] isKindOfClass:[NSNumber class]] ? params[@"raw_signal"] : nil;
            NSNumber* connectable = [params[@"is_connectable"] isKindOfClass:[NSNumber class]] ? params[@"is_connectable"] : nil;

            DiscoveredDevice d;
            d.transport=Transport::LowEnergy;
            d.id="apple:ble:"+gmbt_string(address);
            d.name=gmbt_string(name);
            // Apple exposes a stable CoreBluetooth identifier, not a public MAC.
            d.address_available=false;
            d.rssi_available=rssi!=nil;
            d.rssi=rssi ? rssi.intValue : 0;
            d.connectable=connectable ? connectable.boolValue : true;
            d.advertisement=advertisement_from_params(params);
            hooks_.upsert_device(d);
        }

        // Bytes the transport sent as base64; empty when they do not decode.
        static std::vector<std::uint8_t> bytes_from_base64(id value)
        {
            if (![value isKindOfClass:[NSString class]]) return {};
            NSData* data = [[NSData alloc] initWithBase64EncodedString:(NSString*)value options:0];
            const auto* bytes = static_cast<const std::uint8_t*>(data.bytes);
            if (!bytes) return {};
            return std::vector<std::uint8_t>(bytes, bytes + data.length);
        }

        // didDiscoverPeripheral's advertisement keys. The core canonicalizes
        // the UUIDs; an entry of the wrong shape is skipped.
        static LeAdvertisement advertisement_from_params(NSDictionary* params)
        {
            LeAdvertisement advertisement;

            id uuids = params[@"service_uuids"];
            if ([uuids isKindOfClass:[NSArray class]])
            {
                for (id uuid in (NSArray*)uuids)
                    if ([uuid isKindOfClass:[NSString class]] && [(NSString*)uuid length] > 0)
                        advertisement.service_uuids.push_back(gmbt_string((NSString*)uuid));
            }

            id serviceData = params[@"service_data"];
            if ([serviceData isKindOfClass:[NSArray class]])
            {
                for (id entry in (NSArray*)serviceData)
                {
                    if (![entry isKindOfClass:[NSDictionary class]]) continue;
                    id uuid = ((NSDictionary*)entry)[@"uuid"];
                    if (![uuid isKindOfClass:[NSString class]] || [(NSString*)uuid length] == 0) continue;
                    advertisement.service_data.push_back(LeAdvertiseServiceData{
                        gmbt_string((NSString*)uuid), bytes_from_base64(((NSDictionary*)entry)[@"data"]) });
                }
            }

            id manufacturerData = params[@"manufacturer_data"];
            if ([manufacturerData isKindOfClass:[NSArray class]])
            {
                for (id entry in (NSArray*)manufacturerData)
                {
                    if (![entry isKindOfClass:[NSDictionary class]]) continue;
                    id company = ((NSDictionary*)entry)[@"company_id"];
                    if (![company isKindOfClass:[NSNumber class]]) continue;
                    const int companyId = [(NSNumber*)company intValue];
                    if (companyId < 0 || companyId > 0xFFFF) continue;
                    advertisement.manufacturer_data.push_back(LeAdvertiseManufacturerData{
                        static_cast<std::uint16_t>(companyId), bytes_from_base64(((NSDictionary*)entry)[@"data"]) });
                }
            }

            id txPower = params[@"tx_power"];
            if ([txPower isKindOfClass:[NSNumber class]])
                advertisement.tx_power = [(NSNumber*)txPower intValue];

            return advertisement;
        }

        void on_event(NSString* type, NSDictionary* params)
        {
            if (!type) return;

            // Scan results only feed the device cache; device_found is the
            // core's to fire.
            if ([type isEqualToString:@"bt_le_scan_result"])
            {
                upsert_scan_result(params);
                return;
            }

            if ([type isEqualToString:@"bt_le_scan_stopped"])
            {
                BackendEvent ev;
                ev.type = BackendEventType::ScanStopped;
                ev.transport = Transport::LowEnergy;
                ev.error = static_cast<Error>([params[@"error"] intValue]);
                ev.message = gmbt_string([params[@"message"] isKindOfClass:[NSString class]] ? params[@"message"] : nil);
                hooks_.push_event(std::move(ev));
                return;
            }

            if (!is_routed_event(type)) return;

            NSString* address = [params[@"address"] isKindOfClass:[NSString class]] ? params[@"address"] : nil;
            NSMutableDictionary* decorated=[NSMutableDictionary dictionaryWithDictionary:params ? params : @{}];
            std::uint64_t c=0;
            if(address)
            {
                c=connection_for_id(gmbt_string(address));
                if(c!=0) decorated[@"connection"]=@(c);
            }

            // The link is gone or never came up, so the peripheral may be
            // connected again, from this event's own callback too.
            const bool ended = [type isEqualToString:@"bt_le_peripheral_disconnect"] ||
                ([type isEqualToString:@"bt_le_peripheral_open"] && params[@"error_code"] != nil);
            if (c != 0 && ended) remove_connection(c);

            BackendEvent ev;
            ev.type=BackendEventType::LeEvent;
            ev.transport=Transport::LowEnergy;
            ev.event_type=normalized_event_type(type);
            ev.json=json_string(decorated);
            hooks_.push_event(std::move(ev));
        }
    };
}

std::unique_ptr<Backend> create_platform_backend(CoreHooks hooks)
{
    return std::make_unique<AppleBackend>(std::move(hooks));
}
}

#endif // __APPLE__
