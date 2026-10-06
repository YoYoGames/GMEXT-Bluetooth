#include "GMBluetooth_backend.h"
#include "GMBluetooth_log.h"

#if defined(__APPLE__)

#import <Foundation/Foundation.h>
#import <CoreBluetooth/CoreBluetooth.h>
#include <TargetConditionals.h>

#if TARGET_OS_IOS
#import <UIKit/UIKit.h>
#else
// macOS only - NSHost is part of Foundation (already imported above)
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

- (instancetype)initWithOpId:(NSNumber *)opId dictionary:(NSMutableDictionary *)dictionary;
@end

@interface GMBTQueuedMutableService : NSObject
@property (nonatomic, strong) NSNumber *opId;
@property (nonatomic, strong) CBMutableService *service;

- (instancetype)initWithOpId:(NSNumber *)opId service:(CBMutableService *)service;
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
@property (nonatomic, strong) NSMutableArray<GMBTQueuedCharacteristicWithData *> *notifyCharacteristic;
@property (nonatomic, strong) NSMutableArray<GMBTQueuedDescriptor *> *readDescriptor;
@property (nonatomic, strong) NSMutableArray<GMBTQueuedDescriptorWithData *> *writeDescriptor;
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
// the core passed in, the platform error code (nil on success) and what the
// call returned. No event name: the core knows the call from its op id.
@property(nonatomic, copy) void (^opSink)(NSNumber *opId, NSNumber *errorCode, gmbluetooth::LeOpResult result);

// CLIENT

@property(nonatomic, strong) CBCentralManager *centralManager;

@property(nonatomic, strong) NSMutableArray<GMBTQueuedPeripheral *> *openPeripheralQueue;
@property(nonatomic, strong) NSMutableArray<GMBTQueuedPeripheral *> *closePeripheralQueue;

// Keyed by the peripheral's identifier. An entry goes when its peripheral's
// link ends; the core fails the ops it held.
@property(nonatomic, strong) NSMutableDictionary<NSString *, GMBTPeripheralQueues *> *peripheralQueues;

@property(nonatomic, strong) NSMutableDictionary <NSString *, CBPeripheral *> *discoveredPeripherals;
@property(nonatomic, strong) NSMutableDictionary <NSString *, CBPeripheral *> *openedPeripherals;
@property(nonatomic, strong) NSMutableDictionary <NSString *, CBPeripheral *> *connectedPeripherals;

// SERVER

@property(nonatomic, strong) CBPeripheralManager *peripheralManager;

@property(nonatomic, strong) NSMutableArray <GMBTQueuedMutableService *> *addServiceQueue;
@property(nonatomic, strong) NSMutableArray <GMBTQueuedMutableDictionary *> *startAdvertisementQueue;

@property(nonatomic, strong) NSMutableDictionary <NSString *, CBMutableService *> *addedServices;

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
    }
    return self;
}
@end

@implementation GMBTWriteBatch
@end

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

// The outcome of a call: "success", plus "error_code" when errorCode is set.
- (void) notifyResult:(NSString *)functionName errorCode:(NSNumber *)errorCode extraParams:(NSDictionary *)extraParams {
    NSMutableDictionary *params = [NSMutableDictionary dictionary];
    params[@"success"] = @(errorCode == nil);
    if (errorCode) params[@"error_code"] = errorCode;

    if (extraParams) {
        [params addEntriesFromDictionary:extraParams];
    }

    [self notifyOperation:functionName extraParams:params];
}

// Completes the call the core registered as opId; errorCode nil means
// success. The other calls need no id: a connect is matched by its
// peripheral, and the rest have no callback.
- (void) completeOp:(NSNumber *)opId errorCode:(NSNumber *)errorCode result:(gmbluetooth::LeOpResult)result {
    if (self.opSink) self.opSink(opId, errorCode, std::move(result));
}

- (void) completeOp:(NSNumber *)opId error:(NSError *)error result:(gmbluetooth::LeOpResult)result {
    [self completeOp:opId errorCode:(error ? @((int)error.code) : nil) result:std::move(result)];
}

- (void) completeOp:(NSNumber *)opId error:(NSError *)error {
    [self completeOp:opId error:error result:gmbluetooth::LeOpResult{}];
}

// What a discovery returns: each attribute's UUID, as the cache stores it,
// and a characteristic's properties.
- (gmbluetooth::LeOpResult) resultFromServices:(NSArray<CBService *> *)services {
    gmbluetooth::LeOpResult result;
    for (CBService *service in services) {
        NSString *uuid = [[self convertTo128BitUUID: service.UUID.UUIDString] uppercaseString];
        result.attributes.push_back(gmbluetooth::LeAttribute{ std::string(uuid.UTF8String ? uuid.UTF8String : ""), 0 });
    }
    return result;
}

- (gmbluetooth::LeOpResult) resultFromCharacteristics:(NSArray<CBCharacteristic *> *)characteristics {
    gmbluetooth::LeOpResult result;
    for (CBCharacteristic *characteristic in characteristics) {
        NSString *uuid = [characteristic.UUID.UUIDString uppercaseString];
        result.attributes.push_back(gmbluetooth::LeAttribute{
            std::string(uuid.UTF8String ? uuid.UTF8String : ""), static_cast<std::int32_t>(characteristic.properties) });
    }
    return result;
}

- (gmbluetooth::LeOpResult) resultFromDescriptors:(NSArray<CBDescriptor *> *)descriptors {
    gmbluetooth::LeOpResult result;
    for (CBDescriptor *descriptor in descriptors) {
        NSString *uuid = [[self convertTo128BitUUID: descriptor.UUID.UUIDString] uppercaseString];
        result.attributes.push_back(gmbluetooth::LeAttribute{ std::string(uuid.UTF8String ? uuid.UTF8String : ""), 0 });
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
    NSString *key = peripheral.identifier.UUIDString;
    if (!key) return nil;
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
// core fails their ops when it learns the link ended.
- (void) dropQueuesForPeripheral:(CBPeripheral *)peripheral {
    NSString *key = peripheral.identifier.UUIDString;
    if (key) [_peripheralQueues removeObjectForKey:key];
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
        [self completeOp:entry.opId errorCode:@((int)CBATTErrorInvalidHandle) result:gmbluetooth::LeOpResult{}];
    }

    if (queue.count > 0 && queue.firstObject != oldHead) reissue();
}

// BLUETOOTH IMPLEMENTATION

- (id) init {
    self = [super init];
    if (self) {
        
        _discoveredPeripherals = [NSMutableDictionary new];
        _openedPeripherals = [NSMutableDictionary new];
        _connectedPeripherals = [NSMutableDictionary new];
        
        _addedServices = [NSMutableDictionary new];
        _initialValues = [NSMapTable mapTableWithKeyOptions:NSPointerFunctionsStrongMemory | NSPointerFunctionsObjectPointerPersonality
                                               valueOptions:NSPointerFunctionsStrongMemory];

        _startAdvertisementQueue = [NSMutableArray new];
        
        _addServiceQueue = [NSMutableArray new];
        _readRequestsLookup = [NSMutableDictionary new];
        _writeRequestsLookup = [NSMutableDictionary new];
        _centralSubscriptions = [NSMutableDictionary new];
        _subscribedCentrals = [NSMutableDictionary new];

        _openPeripheralQueue = [NSMutableArray new];
        _closePeripheralQueue = [NSMutableArray new];

        _peripheralQueues = [NSMutableDictionary new];
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
    // This class implements centralManager:willRestoreState: and
    // peripheralManager:willRestoreState:. CoreBluetooth logs
    // "API MISUSE: ... has no restore identifier but the delegate implements
    // the ...:willRestoreState: method" unless a restore identifier is supplied
    // alongside them, so pass one. State restoration itself additionally
    // requires the bluetooth-central / bluetooth-peripheral UIBackgroundModes;
    // without those the identifier is simply inert rather than harmful.
    //
    // The restore-identifier options are iOS/tvOS only - macOS has no
    // CoreBluetooth state restoration and does not declare these constants.
    #if TARGET_OS_IOS || TARGET_OS_TV
    _centralManager = [[CBCentralManager alloc] initWithDelegate:self queue:nil options:@{
        CBCentralManagerOptionRestoreIdentifierKey: @"GMBluetoothCentralManager"
    }];
    _peripheralManager = [[CBPeripheralManager alloc] initWithDelegate:self queue:nil options:@{
        CBPeripheralManagerOptionRestoreIdentifierKey: @"GMBluetoothPeripheralManager"
    }];
    #else
    _centralManager = [[CBCentralManager alloc] initWithDelegate:self queue:nil options:nil];
    _peripheralManager = [[CBPeripheralManager alloc] initWithDelegate:self queue:nil options:nil];
    #endif

    // registerForConnectionEventsWithOptions: is deliberately NOT called here.
    // The central is still in the Unknown state at this point, and CoreBluetooth
    // answers with "API MISUSE: ... can only accept this command while in the
    // powered on state" and ignores it. centralManagerDidUpdateState: issues it
    // once the central actually reaches PoweredOn.
}

// Commands CoreBluetooth only accepts once the central is powered on.
- (void) applyPoweredOnCentralOptions {
    #if TARGET_OS_OSX
    // Not available on macOS.
    #else
    if (@available(iOS 13.0, *)) {
        NSDictionary *options = @{
            CBConnectPeripheralOptionNotifyOnConnectionKey: @YES,
            CBConnectPeripheralOptionNotifyOnDisconnectionKey: @YES
        };

        [self.centralManager registerForConnectionEventsWithOptions: options];
    }
    #endif
}

- (void) bt_end {
    // These outlive the managers (file-scope), so a shutdown mid-scan would
    // otherwise leave the next bt_init believing a scan is already under way.
    _isScanning = false;
    _scanPendingPowerOn = false;
    _isAdvertising = false;
    _isServerOpen = false;

    // The core fails the ops these held once the backend is gone.
    [_peripheralQueues removeAllObjects];
    [_initialValues removeAllObjects];
    [_readRequestsLookup removeAllObjects];
    [_writeRequestsLookup removeAllObjects];
    [_centralSubscriptions removeAllObjects];
    [_subscribedCentrals removeAllObjects];

    _centralManager = nil;
    _peripheralManager = nil;
}


// ####################################################################################
// # CORE
// ####################################################################################

- (double) bt_is_enabled {
    return _centralManager && _centralManager.state == CBManagerStatePoweredOn;
}

- (double) bt_request_enable {
    return 1.0;
}

- (NSString*) bt_get_name {
    return @"";
}

- (NSString*) bt_get_address {
    return @"";
}

- (NSString*) bt_get_paired_devices {
    return @"[]";
}

// ####################################################################################
// # BASE
// ####################################################################################

- (double) bt_le_is_supported {
    return 1.0;
}

// ####################################################################################
// # SCANNER
// ####################################################################################

- (double) bt_le_scan_start {

    if (_isScanning || _scanPendingPowerOn) {
        NSLog(@"[GMBluetooth] bt_le_scan_start REJECTED: a scan is already %@ "
              @"(CBCentralManager.isScanning=%d, state=%d)",
              _scanPendingPowerOn ? @"pending power-on" : @"running",
              (int)[_centralManager isScanning], (int)_centralManager.state);
        return -1;
    }

    // Clear discovered peripherals mutable array
    [self.discoveredPeripherals removeAllObjects];

    int authorization = -1;
    if (@available(iOS 13.0, macOS 10.15, *)) {
        authorization = (int)[CBManager authorization];
    }
    NSLog(@"[GMBluetooth] bt_le_scan_start: CBCentralManager.state=%d (5=PoweredOn) authorization=%d",
          (int)_centralManager.state, authorization);

    if (_centralManager.state != CBManagerStatePoweredOn) {
        _scanPendingPowerOn = true;
        NSLog(@"[GMBluetooth] bt_le_scan_start: central is not PoweredOn yet - scan DEFERRED, "
              @"it will start automatically from centralManagerDidUpdateState:");
        return 0;
    }

    [self beginScan];
    return 0;
}

// Issues the actual CoreBluetooth scan. Only ever called with the central
// already PoweredOn, so _isScanning tracks a scan that really started.
- (void) beginScan {

    _isScanning = true;

    [_centralManager scanForPeripheralsWithServices:nil
                                             options:@{ CBCentralManagerScanOptionAllowDuplicatesKey: @YES }];

    NSLog(@"[GMBluetooth] beginScan: CBCentralManager.isScanning=%d",
          (int)[_centralManager isScanning]);

    [self notifyResult:@"bt_le_scan_start" errorCode:nil extraParams:nil];
}

- (double) bt_le_scan_is_active {
    return [self.centralManager isScanning] ? 1.0 : 0.0;
}

- (double) bt_le_scan_stop {

    if (!_isScanning && !_scanPendingPowerOn) {
        NSLog(@"[GMBluetooth] bt_le_scan_stop REJECTED: no scan running or pending");
        return -1;
    }

    // Cancels a deferred request too, so stopping before the central powers on
    // does not leave a scan queued to fire later.
    _scanPendingPowerOn = false;
    _isScanning = false;

    [_centralManager stopScan];

    [self notifyResult:@"bt_le_scan_stop" errorCode:nil extraParams:nil];

    return 0;
}

- (void) centralManager:(CBCentralManager *)central didDiscoverPeripheral:(CBPeripheral *)peripheral advertisementData:(NSDictionary<NSString *,id> *)advertisementData RSSI:(NSNumber *)RSSI {
    
    // Add discovered peripheral to the mutable array
    [_discoveredPeripherals setObject:peripheral forKey:[[peripheral.identifier UUIDString] uppercaseString]];

    // 1. Name
    NSString *name = peripheral.name;
    if (name == nil) {
        name = @"Unknown";
    }

    // 2. Address (UUID in this case)
    NSString *uuidString = peripheral.identifier.UUIDString;

    // 3. Signal Strength
    NSInteger rssiValue = RSSI.integerValue;

    // 4. Is_connectable
    NSNumber *isConnectable = advertisementData[CBAdvertisementDataIsConnectable];
    BOOL connectable = [isConnectable boolValue];

    NSDictionary *params = @{
                             @"name": name,
                             @"address": uuidString,
                             @"raw_signal": @(rssiValue),
                             @"is_connectable": @(connectable)
                            };
    
    [self notifyOperation:@"bt_le_scan_result" extraParams:params];
}

// ####################################################################################
// # ADVERTISER
// ####################################################################################

- (void) handleStartAdvertisementQueue {
    if (!_peripheralManager || _peripheralManager.state != CBManagerStatePoweredOn)
        return;

    [self handleQueue:_startAdvertisementQueue withBlock:^(GMBTQueuedMutableDictionary *queuedMutableDictionary) {
        [self->_peripheralManager startAdvertising:queuedMutableDictionary.dictionary];
    }];
}

// CBPeripheralManager.startAdvertising takes only LocalName and ServiceUUIDs;
// AppleBackend::le_advertise_start refuses every other field before this.
// The UUIDs were validated by the core, so UUIDWithString cannot throw.
- (double) bt_le_advertise_start:(BOOL)includeName serviceUUIDs:(NSArray<NSString *> *)serviceUuidStrings opId:(NSNumber *)opId {
    if (_isAdvertising || (_peripheralManager && _peripheralManager.isAdvertising)) return -1;

    NSMutableDictionary *advertisementData = [NSMutableDictionary dictionary];

    if (includeName) {
#if TARGET_OS_IOS
        NSString *deviceName = [[UIDevice currentDevice] name];
#else
        NSString *deviceName = [[NSHost currentHost] localizedName];
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
    [self queueEnqueue:_startAdvertisementQueue value:queued withHandler:^(){ [self handleStartAdvertisementQueue]; }];
    return 0;
}

- (double) bt_le_advertise_stop {
    if (!_isAdvertising && !(_peripheralManager && _peripheralManager.isAdvertising)) return -1;

    [_peripheralManager stopAdvertising];
    _isAdvertising = false;

    [self notifyResult:@"bt_le_advertise_stop" errorCode:nil extraParams:nil];
    return 0;
}

- (double) bt_le_advertise_is_active {
    return _peripheralManager.isAdvertising ? 1.0 : 0.0;
}

- (void) peripheralManagerDidStartAdvertising:(CBPeripheralManager *)peripheral error:(NSError *)error {
    GMBTQueuedMutableDictionary *queuedAdvertisementData = [self queueDequeue:_startAdvertisementQueue];
    if (!queuedAdvertisementData) return;

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

    [self handleQueue:_addServiceQueue withBlock:^(GMBTQueuedMutableService *queuedMutableService) {
        [self->_peripheralManager addService:queuedMutableService.service];
    }];
}

- (double) bt_le_server_open {
    
    if (_isServerOpen) return -1;
    
    _isServerOpen = true;
    
    [self notifyResult:@"bt_le_server_open" errorCode:nil extraParams:nil];

    return 0;
}

- (double) bt_le_server_add_service:(NSString*) serviceDataString opId:(NSNumber *)opId {
    if (!_isServerOpen) return -1;

    NSData *data = [serviceDataString dataUsingEncoding:NSUTF8StringEncoding];
    NSError *error = nil;
    id parsed = [NSJSONSerialization JSONObjectWithData:data options:0 error:&error];
    if (error || ![parsed isKindOfClass:[NSDictionary class]]) {
        NSLog(@"[GMBluetooth] invalid GATT service definition: %@", error);
        return -1;
    }

    NSDictionary *serviceData = (NSDictionary *)parsed;
    NSString *serviceUuidString = serviceData[@"uuid"];
    if (![serviceUuidString isKindOfClass:[NSString class]] || serviceUuidString.length == 0)
        return -1;

    CBUUID *serviceUUID = [CBUUID UUIDWithString:serviceUuidString];
    CBMutableService *service = [[CBMutableService alloc] initWithType:serviceUUID primary:YES];

    NSMutableArray<CBMutableCharacteristic *> *characteristicsArray = [NSMutableArray array];
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

            // permissions is still the legacy cross-platform/Android-style bitmask:
            // 1=read, 2/4=read encrypted, 16=write, 32/64=write encrypted.
            const NSUInteger permissions = [charDict[@"permissions"] unsignedIntegerValue];
            CBAttributePermissions charPermissions = 0;
            if (permissions & 1)       charPermissions |= CBAttributePermissionsReadable;
            if (permissions & (2|4))   charPermissions |= CBAttributePermissionsReadEncryptionRequired;
            if (permissions & 16)      charPermissions |= CBAttributePermissionsWriteable;
            if (permissions & (32|64)) charPermissions |= CBAttributePermissionsWriteEncryptionRequired;

            NSData *initialValue = nil;
            id initialValueField = charDict[@"value"];
            if ([initialValueField isKindOfClass:[NSString class]] && [(NSString *)initialValueField length] > 0) {
                initialValue = [[NSData alloc] initWithBase64EncodedString:(NSString *)initialValueField options:0];
                if (!initialValue) {
                    NSLog(@"[GMBluetooth] characteristic %@ has invalid base64 initial value", charUuidString);
                    return -1;
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
                [_initialValues setObject:initialValue forKey:characteristic];

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
                    if ([descUUID isEqual:[CBUUID UUIDWithString:CBUUIDClientCharacteristicConfigurationString]])
                        continue;

                    CBMutableDescriptor *descriptor = [[CBMutableDescriptor alloc] initWithType:descUUID value:[NSData data]];
                    [descriptorsArray addObject:descriptor];
                }
            }

            if (descriptorsArray.count > 0)
                characteristic.descriptors = descriptorsArray;
            [characteristicsArray addObject:characteristic];
        }
    }

    service.characteristics = characteristicsArray;

    GMBTQueuedMutableService *queueService = [[GMBTQueuedMutableService alloc] initWithOpId:opId service:service];
    [self queueEnqueue:_addServiceQueue value:queueService withHandler:^(){ [self handleAddServiceQueue]; }];
    return 0;
}

- (double) bt_le_server_clear_services {
    
    if (!_isServerOpen) return -1;

    [_peripheralManager removeAllServices];
    [_initialValues removeAllObjects];
    // The core answered the requests these held before asking.
    [_readRequestsLookup removeAllObjects];
    [_writeRequestsLookup removeAllObjects];
    // The characteristics they named are gone; no unsubscribe will come.
    [_centralSubscriptions removeAllObjects];
    [_subscribedCentrals removeAllObjects];

    [self notifyResult:@"bt_le_server_clear_services" errorCode:nil extraParams:nil];

    return 0;
}

- (double) bt_le_server_close {
    if (!_isServerOpen) return -1;
    
    _isServerOpen = false;
    [_peripheralManager removeAllServices];
    [_initialValues removeAllObjects];
    // The core answered the requests these held before asking, and retires
    // every central it knows once the stop succeeds.
    [_readRequestsLookup removeAllObjects];
    [_writeRequestsLookup removeAllObjects];
    [_centralSubscriptions removeAllObjects];
    [_subscribedCentrals removeAllObjects];

    if ([_peripheralManager isAdvertising]) {
        [_peripheralManager stopAdvertising];
    }
    
    [self notifyResult:@"bt_le_server_close" errorCode:nil extraParams:nil];

    return 0;
}

- (double) bt_le_server_respond_read:(double) requestId status:(double) status value:(NSString*) value {
    // Convert the requestId to NSNumber for dictionary lookup
    NSNumber *requestKey = [NSNumber numberWithDouble:requestId];
    
    CBATTRequest *request = nil;
    
    // Retrieve the corresponding CBATTRequest from _readRequests dictionary
    request = _readRequestsLookup[requestKey];
    
    if (!request) {
        NSLog(@"Request with ID %f not found", requestId);
        return 0;
    }
        
    // Decode the base64 value
    NSData *dataValue = [[NSData alloc] initWithBase64EncodedString:value options:0];
    if (!dataValue) {
        NSLog(@"Failed to decode base64 value");
        return -1;
    }
    
    // Set the value in the request object
    request.value = dataValue;
    
    // Respond to the read request
    [_peripheralManager respondToRequest:request withResult:(CBATTError)status];
    
    // Remove the request from the _readRequests dictionary
    [_readRequestsLookup removeObjectForKey:requestKey];
    
    return 1; // Indicate success
}

- (double) bt_le_server_respond_write:(double) requestId status:(double) status {
    // Convert the requestId to NSNumber for dictionary lookup
    NSNumber *requestKey = [NSNumber numberWithDouble:requestId];

    GMBTWriteBatch *batch = _writeRequestsLookup[requestKey];
    if (!batch) {
        NSLog(@"Write request with ID %f not found", requestId);
        return 0;
    }
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

    return 1; // Indicate success
}

// central: empty broadcasts to every subscriber; otherwise the key a server
// event named the central by. Returns 0, -1 on failure, or -2 when that
// central is not subscribed to the characteristic.
- (double) bt_le_server_notify_value:(NSString*) serviceUuid characteristicUuid:(NSString*) characteristicUuid central:(NSString*) centralKey value:(NSString*) value {

    // Decode the base64 value
    NSData *dataValue = [[NSData alloc] initWithBase64EncodedString:value options:0];
    if (!dataValue) {
        NSLog(@"Failed to decode base64 value");
        return -1;
    }

    NSString *canonicalServiceUuid = [CBUUID UUIDWithString:serviceUuid].UUIDString;
    NSString *canonicalCharacteristicUuid = [CBUUID UUIDWithString:characteristicUuid].UUIDString;

    CBMutableService *service = _addedServices[canonicalServiceUuid];
    if (!service) {
        NSLog(@"Service not found");
        return -1;
    }
    
    // Find the target characteristic
    CBMutableCharacteristic *characteristic = nil;
    for (CBMutableCharacteristic *charac in service.characteristics) {
        if ([charac.UUID.UUIDString isEqualToString:canonicalCharacteristicUuid]) {
            characteristic = charac;
            break;
        }
    }

    if (!characteristic) {
        NSLog(@"Characteristic not found");
        return -1;
    }

    NSArray<CBCentral *> *centrals = nil;
    if (centralKey.length > 0) {
        CBCentral *central = _subscribedCentrals[centralKey];
        if (!central || ![_centralSubscriptions[centralKey] containsObject:[self subscriptionKey:characteristic]])
            return -2;
        centrals = @[central];
    }

    // Notify the subscribed centrals, or the one named
    BOOL success = [_peripheralManager updateValue:dataValue forCharacteristic:characteristic onSubscribedCentrals:centrals];
    if (!success) {
        NSLog(@"Failed to notify subscribed centrals");
        return -1;
    }
    
    return 0;
}

- (void) peripheralManager:(CBPeripheralManager *)peripheral didAddService:(CBService *)service error:(NSError *)error {
    GMBTQueuedMutableService *queuedService = [self queueDequeue:_addServiceQueue];
    if (!queuedService) return;
    
    if (!error) _addedServices[[service.UUID UUIDString]] = queuedService.service;
    else {
        for (CBMutableCharacteristic *characteristic in queuedService.service.characteristics)
            [_initialValues removeObjectForKey:characteristic];
    }
    [self completeOp:queuedService.opId error:error];
    
    [self handleAddServiceQueue];
}

- (void) peripheralManagerIsReadyToUpdateSubscribers:(CBPeripheralManager *)peripheral {
    [self notifyResult:@"bt_le_server_notify_value" errorCode:@(-1) extraParams:nil];
}

- (void) peripheralManagerDidUpdateState:(CBPeripheralManager *)peripheral {
    [self notifyOperation:@"bt_le_peripheral_manager_update_state"
              extraParams:@{ @"success": @((int)peripheral.state) }];

    if (peripheral.state == CBManagerStatePoweredOn) {
        [self handleAddServiceQueue];
        [self handleStartAdvertisementQueue];
        return;
    }

    if (peripheral.state == CBManagerStateUnknown || peripheral.state == CBManagerStateResetting)
        return;

    _isAdvertising = false;

    // The radio is gone and every central with it; no unsubscribe follows.
    NSArray<CBCentral *> *centrals = _subscribedCentrals.allValues;
    [_centralSubscriptions removeAllObjects];
    [_subscribedCentrals removeAllObjects];
    for (CBCentral *central in centrals)
        [self notifyServerCentral:central connected:NO];

    while (_startAdvertisementQueue.count > 0) {
        GMBTQueuedMutableDictionary *queued = [self queueDequeue:_startAdvertisementQueue];
        [self completeOp:queued.opId errorCode:@((int)peripheral.state) result:gmbluetooth::LeOpResult{}];
    }
    while (_addServiceQueue.count > 0) {
        GMBTQueuedMutableService *queued = [self queueDequeue:_addServiceQueue];
        [self completeOp:queued.opId errorCode:@((int)peripheral.state) result:gmbluetooth::LeOpResult{}];
    }
}

- (void) peripheralManager:(CBPeripheralManager *)peripheral willRestoreState:(NSDictionary<NSString *,id> *)dict {
    
    // Restore services
    NSArray *services = dict[CBPeripheralManagerRestoredStateServicesKey];
    if (services) {
        for (CBMutableService *service in services) {
            _addedServices[[service.UUID UUIDString]] = service;
        }
    }

    // Restore advertisement data and re-start advertising if app was advertising at the time it was terminated
    NSDictionary *advertisingData = dict[CBPeripheralManagerRestoredStateAdvertisementDataKey];
    if (advertisingData) {
        [_peripheralManager startAdvertising:advertisingData];
    }
    
    [self notifyOperation:@"bt_le_server_restore_state" extraParams:nil];
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

- (NSString *) createJSONFromCentral:(CBCentral *)central {
    NSDictionary *centralDictionary = @{
        @"address" : [self convertTo128BitUUID: central.identifier.UUIDString]
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
    BOOL isDescriptorRequest = NO;
    
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

    // Loop through the characteristic's descriptors to check UUIDs
    for (CBDescriptor *descriptor in request.characteristic.descriptors) {
        // Assuming you store descriptor's UUID when it's read
        if ([descriptor.UUID isEqual:request.characteristic.UUID]) {
            params[@"descriptor_uuid"] = descriptor.UUID.UUIDString;
            isDescriptorRequest = YES;
            break;
        }
    }
    
    NSString* eventType = @"bt_le_server_characteristic_read_request";
    if (isDescriptorRequest) {
        eventType = @"bt_le_server_descriptor_read_request";
    }
    
    [self notifyOperation:eventType extraParams:params];
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

    // Each group's fragments laid out by offset, from its lowest one.
    NSMutableArray<NSData *> *values = [NSMutableArray array];
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

- (double) bt_le_ios_state {
    return _centralManager.state;
}

- (double) bt_le_ios_authorization {
    if (@available(iOS 13.0, *)) {
        return _centralManager.authorization;
    } else {
        return -4;
    }
}

static NSData *KCharacteristicUnsubscribe = [NSData dataWithBytes:(int[]){1} length:sizeof(int)];
static NSData *KCharacteristicNotify = [NSData dataWithBytes:(int[]){2} length:sizeof(int)];
static NSData *KCharacteristicIndicate = [NSData dataWithBytes:(int[]){3} length:sizeof(int)];

- (void) handleOpenPeripheralQueue {
    [self handleQueue:_openPeripheralQueue withBlock:^(GMBTQueuedTimedPeripheral *queuedTimedPeripheral) {
        [self->_centralManager connectPeripheral:queuedTimedPeripheral.peripheral options:nil];
        queuedTimedPeripheral.timer = [NSTimer scheduledTimerWithTimeInterval:10 target:self selector:@selector(connectionDidTimeout) userInfo:nil repeats:NO];
        
    }];
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
    [self handleQueue:queue withBlock:^(GMBTQueuedCharacteristicWithData *queuedCharacteristicData) {
        BOOL enable = [queuedCharacteristicData.data isEqualToData: KCharacteristicUnsubscribe] ? false : true;
        [queuedCharacteristicData.peripheral setNotifyValue:enable forCharacteristic:queuedCharacteristicData.characteristic];
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


- (CBPeripheral *) peripheralForUuid:(NSString *)peripheralUuid {
    
    CBPeripheral *peripheral = [_openedPeripherals objectForKey:peripheralUuid];
    if (peripheral == nil) {
        NSLog(@"Peripheral not found");
    }
    
    return peripheral;
}

- (NSData *) dataFromBase64:(NSString *)value {
    NSData *data = [[NSData alloc] initWithBase64EncodedString:value options:0];
    if (data == nil) {
        NSLog(@"Invalid base64 encoded value");
    }
    return data;
}

- (double) bt_le_peripheral_open:(NSString*) peripheralUuid {
    CBPeripheral *peripheral = [_discoveredPeripherals objectForKey:peripheralUuid];
    if (!peripheral) return -1;
        
    // No op id: the core matches the open by the peripheral's connection.
    GMBTQueuedTimedPeripheral *queuePeripheral = [[GMBTQueuedTimedPeripheral alloc] initWithOpId:nil peripheral:peripheral];

    [self queueEnqueue:_openPeripheralQueue value:queuePeripheral withHandler:^{ [self handleOpenPeripheralQueue]; }];

    return 0;
}

- (double) bt_le_peripheral_is_open:(NSString*) peripheralUuid {
    return [_openedPeripherals objectForKey: peripheralUuid] == nil ? 0.0 : 1.0;
}

- (double) bt_le_peripheral_is_connected:(NSString*) peripheralUuid {
    return [_connectedPeripherals objectForKey: peripheralUuid] == nil ? 0.0 : 1.0;
}

- (double) bt_le_peripheral_is_paired:(NSString*) peripheralUuid {
    NSLog(@"%s :: method not available on iOS", "bt_le_peripheral_is_paired");
    return 0.0;
}

- (double) bt_le_peripheral_close:(NSString*) peripheralUuid {
    
    CBPeripheral *peripheral = [_openedPeripherals objectForKey:peripheralUuid];
    if (peripheral == nil) {
        NSLog(@"Peripheral not found");
        return 0.0;
    }
        
    [_openedPeripherals removeObjectForKey:peripheralUuid];
    [_connectedPeripherals removeObjectForKey:peripheralUuid];
    [self dropQueuesForPeripheral:peripheral];

    [_centralManager cancelPeripheralConnection:peripheral];

    return 1.0;
}

- (double) bt_le_peripheral_close_all {

    for (NSString *key in _openedPeripherals) {
        CBPeripheral *peripheral = _openedPeripherals[key];
        [_centralManager cancelPeripheralConnection:peripheral];
    }

    [_openedPeripherals removeAllObjects];
    [_connectedPeripherals removeAllObjects];
    [_peripheralQueues removeAllObjects];

    return 1.0;
}

- (double) bt_le_peripheral_get_services:(NSString*) peripheralUuid opId:(NSNumber *)opId {
    CBPeripheral *peripheral = [self peripheralForUuid:peripheralUuid];
    if (!peripheral) return -1;

    GMBTQueuedPeripheral *queuePeripheral = [[GMBTQueuedPeripheral alloc] initWithOpId:opId peripheral:peripheral];

    NSMutableArray *queue = [self queuesForPeripheral:peripheral create:YES].fetchServices;
    [self queueEnqueue:queue value:queuePeripheral withHandler:^{ [self handleFetchServicesQueue:queue]; }];

    return 0;
}

- (CBService *) findServiceInPeripheral:(CBPeripheral *)peripheral withUUID:(NSString *)serviceUuid {
    
    // Convert the service UUID string to a CBUUID
    CBUUID *targetUuid = [CBUUID UUIDWithString:serviceUuid];
    for (CBService *service in peripheral.services) {
        if ([service.UUID isEqual:targetUuid]) {
            return service;
        }
    }
    
    NSLog(@"Service not found");
    
    return nil;
}

- (double) bt_le_service_get_characteristics:(NSString*) peripheralUuid service:(NSString*) serviceUuid opId:(NSNumber *)opId {
    CBPeripheral *peripheral = [self peripheralForUuid:peripheralUuid];
    if (!peripheral) return -1;

    // Get service with matching UUID
    CBService *service = [self findServiceInPeripheral:peripheral withUUID:serviceUuid];
    if (!service) return -1;

    GMBTQueuedService *queuedService = [[GMBTQueuedService alloc] initWithOpId:opId peripheral:peripheral service:service];

    NSMutableArray *queue = [self queuesForPeripheral:peripheral create:YES].fetchCharacteristics;
    [self queueEnqueue:queue value:queuedService withHandler:^{ [self handleFetchCharacteristicsQueue:queue]; }];

    return 0;
}

- (CBCharacteristic *) findCharacteristicInService:(CBService *)service withUUID:(NSString *)characteristicUuid {
    
    // Convert the characteristic UUID string to a CBUUID
    CBUUID *targetUuid = [CBUUID UUIDWithString:characteristicUuid];
    
    for (CBCharacteristic *characteristic in service.characteristics) {
        if ([characteristic.UUID isEqual:targetUuid]) {
            return characteristic;
        }
    }
    
    NSLog(@"Characteristic not found");
    
    return nil;
}

- (double) bt_le_characteristic_get_descriptors:(NSString*) peripheralUuid service:(NSString*) serviceUuid characteristic:(NSString*) characteristicUuid opId:(NSNumber *)opId {
    
    CBPeripheral *peripheral = [self peripheralForUuid:peripheralUuid];
    if (!peripheral) return -1;
    
    // Get service with matching UUID
    CBService *service = [self findServiceInPeripheral:peripheral withUUID:serviceUuid];
    if (!service) return -1;
    

    // Get service with matching UUID
    CBCharacteristic *characteristic = [self findCharacteristicInService:service withUUID:characteristicUuid];
    if (!characteristic) return -1;
    
    
    GMBTQueuedCharacteristic *queuedCharacteristic = [[GMBTQueuedCharacteristic alloc] initWithOpId:opId peripheral:peripheral characteristic:characteristic];

    NSMutableArray *queue = [self queuesForPeripheral:peripheral create:YES].fetchDescriptors;
    [self queueEnqueue:queue value:queuedCharacteristic withHandler:^{ [self handleFetchDescriptorsQueue:queue]; }];

    return 0;
}

- (double) bt_le_characteristic_read:(NSString*) peripheralUuid service:(NSString*) serviceUuid characteristic:(NSString*) characteristicUuid opId:(NSNumber *)opId {
    
    CBPeripheral *peripheral = [self peripheralForUuid:peripheralUuid];
    if (!peripheral) return -1;
    
    // Get service with matching UUID
    CBService *service = [self findServiceInPeripheral:peripheral withUUID:serviceUuid];
    if (!service) return -1;

    // Get service with matching UUID
    CBCharacteristic *characteristic = [self findCharacteristicInService:service withUUID:characteristicUuid];
    if (!characteristic) return -1;
    
    GMBTQueuedCharacteristic *queuedCharacteristic = [[GMBTQueuedCharacteristic alloc] initWithOpId:opId peripheral:peripheral characteristic:characteristic];

    NSMutableArray *queue = [self queuesForPeripheral:peripheral create:YES].readCharacteristic;
    [self queueEnqueue:queue value:queuedCharacteristic withHandler:^{ [self handleReadCharacteristicQueue:queue]; }];

    return 0;
}

- (double) bt_le_characteristic_write_request:(NSString*) peripheralUuid service:(NSString*) serviceUuid characteristic:(NSString*) characteristicUuid value:(NSString*) value opId:(NSNumber *)opId {
    
    CBPeripheral *peripheral = [self peripheralForUuid:peripheralUuid];
    if (!peripheral) return -1;
    
    CBService *service = [self findServiceInPeripheral:peripheral withUUID:serviceUuid];
    if (!service) return -1;

    CBCharacteristic *characteristic = [self findCharacteristicInService:service withUUID:characteristicUuid];
    if (!characteristic) return -1;
    
    NSData *data = [self dataFromBase64:value];
    if (!data) return -1;
    
    GMBTQueuedCharacteristicWithData *queuedCharacteristicData = [[GMBTQueuedCharacteristicWithData alloc] initWithOpId:opId peripheral:peripheral characteristic:characteristic data:data];

    NSMutableArray *queue = [self queuesForPeripheral:peripheral create:YES].writeCharacteristic;
    [self queueEnqueue:queue value:queuedCharacteristicData withHandler:^{ [self handleWriteCharacteristicQueue:queue]; }];

    return 0;
}

- (double) bt_le_characteristic_write_command:(NSString*) peripheralUuid service:(NSString*) serviceUuid characteristic:(NSString*) characteristicUuid value:(NSString*) value opId:(NSNumber *)opId {
    
    CBPeripheral *peripheral = [self peripheralForUuid:peripheralUuid];
    if (!peripheral) return -1;
    
    CBService *service = [self findServiceInPeripheral:peripheral withUUID:serviceUuid];
    if (!service) return -1;

    CBCharacteristic *characteristic = [self findCharacteristicInService:service withUUID:characteristicUuid];
    if (!characteristic) return -1;
    
    NSData *data = [self dataFromBase64:value];
    if (!data) return -1;
    
    [peripheral writeValue:data forCharacteristic:characteristic type:CBCharacteristicWriteWithoutResponse];

    // CoreBluetooth sends no delegate call for a write without response, so
    // it completes here, under its own op id, before this call returns.
    [self completeOp:opId error:nil];

    return 0;
}

- (double) manageCharacteristicSubscriptionForPeripheral:(NSString*) peripheralUuid service:(NSString*) serviceUuid characteristicUuid:(NSString*) characteristicUuid type:(NSData*)type opId:(NSNumber *)opId {
    CBPeripheral *peripheral = [self peripheralForUuid:peripheralUuid];
    if (!peripheral) return -1;
    
    CBService *service = [self findServiceInPeripheral:peripheral withUUID:serviceUuid];
    if (!service) return -1;
    
    CBCharacteristic *characteristic = [self findCharacteristicInService:service withUUID:characteristicUuid];
    if (!characteristic) return -1;
    
    GMBTQueuedCharacteristicWithData *queuedCharacteristicData = [[GMBTQueuedCharacteristicWithData alloc] initWithOpId:opId peripheral:peripheral characteristic:characteristic data:type];

    NSMutableArray *queue = [self queuesForPeripheral:peripheral create:YES].notifyCharacteristic;
    [self queueEnqueue:queue value:queuedCharacteristicData withHandler:^{ [self handleNotifyCharacteristicQueue:queue]; }];

    return 0;
}

- (double) bt_le_characteristic_notify:(NSString*) peripheralUuid service:(NSString*) serviceUuid characteristic:(NSString*) characteristicUuid opId:(NSNumber *)opId {

    return [self manageCharacteristicSubscriptionForPeripheral:peripheralUuid service:serviceUuid characteristicUuid:characteristicUuid type: KCharacteristicNotify opId:opId];
}

- (double) bt_le_characteristic_indicate:(NSString*) peripheralUuid service:(NSString*) serviceUuid characteristic:(NSString*) characteristicUuid opId:(NSNumber *)opId {

    return [self manageCharacteristicSubscriptionForPeripheral:peripheralUuid service:serviceUuid characteristicUuid:characteristicUuid type: KCharacteristicIndicate opId:opId];
}

- (double) bt_le_characteristic_unsubscribe:(NSString*) peripheralUuid service:(NSString*) serviceUuid characteristic:(NSString*) characteristicUuid opId:(NSNumber *)opId {

    return [self manageCharacteristicSubscriptionForPeripheral:peripheralUuid service:serviceUuid characteristicUuid:characteristicUuid type:KCharacteristicUnsubscribe opId:opId];
}

- (CBDescriptor *) findDescriptorInCharacteristic:(CBCharacteristic *)characteristic withUUID:(NSString *)descriptorUuid {
    
    // Convert the descriptor UUID string to a CBUUID
    CBUUID *targetUuid = [CBUUID UUIDWithString:descriptorUuid];
    
    for (CBDescriptor *descriptor in characteristic.descriptors) {
        if ([descriptor.UUID isEqual:targetUuid]) {
            return descriptor;
        }
    }
    
    NSLog(@"Descriptor not found");
    
    return nil;
}

- (double) bt_le_descriptor_read:(NSString*) peripheralUuid service:(NSString*) serviceUuid characteristic:(NSString*) characteristicUuid descriptor:(NSString*) descriptorUuid opId:(NSNumber *)opId {
    
    CBPeripheral *peripheral = [self peripheralForUuid:peripheralUuid];
    if (!peripheral) return -1;
    
    // Get service with matching UUID
    CBService *service = [self findServiceInPeripheral:peripheral withUUID:serviceUuid];
    if (!service) return -1;

    // Get characteristic with matching UUID
    CBCharacteristic *characteristic = [self findCharacteristicInService:service withUUID:characteristicUuid];
    if (!characteristic) return -1;
    
    // Get descriptor with matching UUID
    CBDescriptor *descriptor = [self findDescriptorInCharacteristic:characteristic withUUID:descriptorUuid];
    if (!descriptor) return -1;
    
    GMBTQueuedDescriptor *queuedDescriptor = [[GMBTQueuedDescriptor alloc] initWithOpId:opId peripheral:peripheral descriptor:descriptor];

    NSMutableArray *queue = [self queuesForPeripheral:peripheral create:YES].readDescriptor;
    [self queueEnqueue:queue value:queuedDescriptor withHandler:^{ [self handleReadDescriptorQueue:queue]; }];

    return 0;
}

- (double) bt_le_descriptor_write:(NSString*) peripheralUuid service:(NSString*) serviceUuid characteristic:(NSString*) characteristicUuid descriptor:(NSString*) descriptorUuid value:(NSString*) value opId:(NSNumber *)opId {
    
    CBPeripheral *peripheral = [self peripheralForUuid:peripheralUuid];
    if (!peripheral) return -1;
    
    // Get service with matching UUID
    CBService *service = [self findServiceInPeripheral:peripheral withUUID:serviceUuid];
    if (!service) return -1;

    // Get characteristic with matching UUID
    CBCharacteristic *characteristic = [self findCharacteristicInService:service withUUID:characteristicUuid];
    if (!characteristic) return -1;
    
    // Get descriptor with matching UUID
    CBDescriptor *descriptor = [self findDescriptorInCharacteristic:characteristic withUUID:descriptorUuid];
    if (!descriptor) return -1;
    
    NSData *data = [self dataFromBase64:value];
    if (!data) return -1;
    
    GMBTQueuedDescriptorWithData *queuedDescriptorWithData = [[GMBTQueuedDescriptorWithData alloc] initWithOpId:opId peripheral:peripheral descriptor:descriptor data:data];

    NSMutableArray *queue = [self queuesForPeripheral:peripheral create:YES].writeDescriptor;
    [self queueEnqueue:queue value:queuedDescriptorWithData withHandler:^{ [self handleWriteDescriptorQueue:queue]; }];

    return 0;
}

- (void) centralManager:(CBCentralManager *)central didConnectPeripheral:(CBPeripheral *)peripheral {
	
    GMBTQueuedTimedPeripheral *queuedPeripheral = [self queueDequeue:_openPeripheralQueue];
    
    [queuedPeripheral.timer invalidate];
    
    peripheral.delegate = self;
	
    [_openedPeripherals setObject:peripheral forKey:[peripheral.identifier UUIDString]];
    [_connectedPeripherals setObject:peripheral forKey:[peripheral.identifier UUIDString]];
    
    NSMutableDictionary* params = [[NSMutableDictionary alloc] init];
    params[@"name"] = peripheral.name ?: @"";
    params[@"address"] = peripheral.identifier.UUIDString;

    [self notifyResult:@"bt_le_peripheral_open" errorCode:nil extraParams:params];
    [self handleOpenPeripheralQueue];
}

- (void) centralManager:(CBCentralManager *)central didFailToConnectPeripheral:(CBPeripheral *)peripheral error:(NSError *)error {
    
    GMBTQueuedTimedPeripheral *queuedPeripheral = [self queueDequeue:_openPeripheralQueue];
    
    [queuedPeripheral.timer invalidate];
    
    NSMutableDictionary* params = [[NSMutableDictionary alloc] init];
    params[@"name"] = peripheral.name ?: @"";
    params[@"address"] = peripheral.identifier.UUIDString;

    [self notifyResult:@"bt_le_peripheral_open" errorCode:@((int)error.code) extraParams:params];
    [self handleOpenPeripheralQueue];
}

- (void) connectionDidTimeout {
    GMBTQueuedPeripheral *queuedPeripheral = [self queueDequeue:_openPeripheralQueue];
    
    [_centralManager cancelPeripheralConnection: queuedPeripheral.peripheral];
    
    NSMutableDictionary* params = [[NSMutableDictionary alloc] init];
    params[@"name"] = queuedPeripheral.peripheral.name ?: @"";
    params[@"address"] = queuedPeripheral.peripheral.identifier.UUIDString;

    [self notifyResult:@"bt_le_peripheral_open" errorCode:@133 extraParams:params];
    [self handleOpenPeripheralQueue];
}

- (void) centralManager:(CBCentralManager *)central didDisconnectPeripheral:(CBPeripheral *)peripheral error:(NSError *)error {
    NSString *key = peripheral.identifier.UUIDString;
    [self dropQueuesForPeripheral:peripheral];

    // Not open any more: the game closed it (bt_le_peripheral_close), the
    // central reported it when it left PoweredOn, or it never finished
    // opening. Each of those has already been reported.
    if (!key || !_openedPeripherals[key]) return;

    [_openedPeripherals removeObjectForKey:key];
    [_connectedPeripherals removeObjectForKey:key];

    // The address lets on_event name the connection, so the core can fail the
    // ops that were queued on it.
    NSMutableDictionary* params = [[NSMutableDictionary alloc] init];
    params[@"address"] = key;
    params[@"error_code"] = @(error ? (int)error.code : 0);
    [self notifyOperation:@"bt_le_peripheral_disconnect" extraParams:params];
}

#if TARGET_OS_IOS

- (void) centralManager:(CBCentralManager *)central connectionEventDidOccur:(CBConnectionEvent)event forPeripheral:(CBPeripheral *)peripheral {
    
    NSMutableDictionary* params = [[NSMutableDictionary alloc] init];
    params[@"name"] = peripheral.name ?: @"";
    params[@"address"] = peripheral.identifier.UUIDString;
    
    // Peripheral connected
    if (event == CBConnectionEventPeerConnected) {
        peripheral.delegate = self;
        [_connectedPeripherals setObject:peripheral forKey:peripheral.identifier.UUIDString];
        
        params[@"is_connected"] = @(true);
    }
    // Peripheral disconneted
    else if (event == CBConnectionEventPeerDisconnected) {
        peripheral.delegate = nil;
        [_connectedPeripherals removeObjectForKey:peripheral.identifier.UUIDString];

        params[@"is_connected"] = @(false);
    }
    
    [self notifyOperation:@"bt_le_peripheral_connection_state_changed" extraParams:params];
}

#endif

- (void) peripheral:(CBPeripheral *)peripheral didDiscoverServices:(NSError *)error {
	
    NSMutableArray *queue = [self queuesForPeripheral:peripheral create:NO].fetchServices;
    GMBTQueuedPeripheral *queuedPeripheral = [self takeHeadOf:queue answering:@"didDiscoverServices" matching:^BOOL(id head) {
        return ((GMBTQueuedPeripheral *)head).peripheral == peripheral;
    }];
    if (!queuedPeripheral) return;

    if (error) [self completeOp:queuedPeripheral.opId error:error];
    else [self completeOp:queuedPeripheral.opId error:nil result:[self resultFromServices:peripheral.services]];
    [self handleFetchServicesQueue:queue];
}
 
- (void) peripheral:(CBPeripheral *)peripheral didDiscoverIncludedServicesForService:(CBService *)service error:(NSError *)error {
    // This won't be handled
}

- (void) peripheral:(CBPeripheral *)peripheral didDiscoverCharacteristicsForService:(CBService *)service error:(NSError *)error {
    
    NSMutableArray *queue = [self queuesForPeripheral:peripheral create:NO].fetchCharacteristics;
    GMBTQueuedService *queuedService = [self takeHeadOf:queue answering:@"didDiscoverCharacteristicsForService" matching:^BOOL(id head) {
        return ((GMBTQueuedService *)head).service == service;
    }];
    if (!queuedService) return;

    if (error) [self completeOp:queuedService.opId error:error];
    else [self completeOp:queuedService.opId error:nil result:[self resultFromCharacteristics:service.characteristics]];

    // Handle next task in queue if there is one
    [self handleFetchCharacteristicsQueue:queue];
}
 
- (void) peripheral:(CBPeripheral *)peripheral didDiscoverDescriptorsForCharacteristic:(CBCharacteristic *)characteristic error:(NSError *)error {
	
    NSMutableArray *queue = [self queuesForPeripheral:peripheral create:NO].fetchDescriptors;
    GMBTQueuedCharacteristic *queuedCharacteristic = [self takeHeadOf:queue answering:@"didDiscoverDescriptorsForCharacteristic" matching:^BOOL(id head) {
        return ((GMBTQueuedCharacteristic *)head).characteristic == characteristic;
    }];
    if (!queuedCharacteristic) return;

    if (error) [self completeOp:queuedCharacteristic.opId error:error];
    else [self completeOp:queuedCharacteristic.opId error:nil result:[self resultFromDescriptors:characteristic.descriptors]];
    [self handleFetchDescriptorsQueue:queue];
}

- (void) peripheral:(CBPeripheral *)peripheral didUpdateNotificationStateForCharacteristic:(CBCharacteristic *)characteristic error:(NSError *)error {
    
    NSMutableArray *queue = [self queuesForPeripheral:peripheral create:NO].notifyCharacteristic;
    GMBTQueuedCharacteristicWithData *queuedCharacteristicWithData = [self takeHeadOf:queue answering:@"didUpdateNotificationStateForCharacteristic" matching:^BOOL(id head) {
        return ((GMBTQueuedCharacteristicWithData *)head).characteristic == characteristic;
    }];
    if (!queuedCharacteristicWithData) return;

    [self completeOp:queuedCharacteristicWithData.opId error:error];

    [self handleNotifyCharacteristicQueue:queue];
}

- (void) peripheral:(CBPeripheral *)peripheral didUpdateValueForCharacteristic:(CBCharacteristic *)characteristic error:(NSError *)error {
        
    // Convert the value to a base64 string
    NSString *valueString = [characteristic.value base64EncodedStringWithOptions:0];
    
    NSMutableArray *queue = [self queuesForPeripheral:peripheral create:NO].readCharacteristic;
    GMBTQueuedCharacteristic *queuedCharacteristic = [self queuePeek:queue];

    if (queuedCharacteristic.characteristic != characteristic) {
        queuedCharacteristic = nil;
    }
    else [self queueDequeue:queue];

    // There was not queued read request that matches the characteristic so it's a notification
    if (queuedCharacteristic == nil) {
        NSMutableDictionary *params = [[NSMutableDictionary alloc] init];
        params[@"characteristic_uuid"] = [characteristic.UUID.UUIDString uppercaseString];
        params[@"service_uuid"] = [characteristic.service.UUID.UUIDString uppercaseString];
        params[@"address"] = [peripheral.identifier.UUIDString uppercaseString];
        params[@"value"] = valueString;
        
        return [self notifyOperation:@"bt_le_characteristic_value_changed" extraParams:params];
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
    NSString *stateString = @"Unknown";
    switch (central.state) {
        case CBManagerStateUnknown:
            stateString = @"Unknown";
            break;
        case CBManagerStateResetting:
            stateString = @"Resetting";
            break;
        case CBManagerStateUnsupported:
            stateString = @"Unsupported";
            break;
        case CBManagerStateUnauthorized:
            stateString = @"Unauthorized";
            break;
        case CBManagerStatePoweredOff:
            stateString = @"PoweredOff";
            break;
        case CBManagerStatePoweredOn:
            stateString = @"PoweredOn";
            break;
    }
    NSLog(@"[GMBluetooth] CBCentralManager state changed: %@ (%d)", stateString, (int)central.state);

    if (central.state == CBManagerStatePoweredOn) {
        // Deferred out of bt_init: CoreBluetooth rejects this before PoweredOn.
        [self applyPoweredOnCentralOptions];

        if (_scanPendingPowerOn) {
            _scanPendingPowerOn = false;
            NSLog(@"[GMBluetooth] central reached PoweredOn - starting the scan deferred at request time");
            [self beginScan];
        }
    }
    else if (_scanPendingPowerOn && central.state != CBManagerStateUnknown &&
             central.state != CBManagerStateResetting) {
        // Unsupported / Unauthorized / PoweredOff are terminal for this request -
        // holding the deferral would stall the caller indefinitely.
        _scanPendingPowerOn = false;
        NSLog(@"[GMBluetooth] central reached %@ - the deferred scan cannot start and has been dropped",
              stateString);
        [self notifyResult:@"bt_le_scan_start"
                 errorCode:@((int)central.state)
               extraParams:@{ @"state": stateString }];
    }

    if (central.state != CBManagerStatePoweredOn && _openedPeripherals.count > 0) {
        // Every link is gone, and CoreBluetooth does not promise a
        // didDisconnectPeripheral for each. Report each open peripheral once
        // here; the later didDisconnectPeripheral, if any, finds it closed.
        NSArray<CBPeripheral *> *lost = [_openedPeripherals allValues];
        [_openedPeripherals removeAllObjects];
        [_connectedPeripherals removeAllObjects];
        [_peripheralQueues removeAllObjects];
        for (CBPeripheral *peripheral in lost) {
            [self notifyOperation:@"bt_le_peripheral_disconnect"
                      extraParams:@{ @"address": peripheral.identifier.UUIDString ? peripheral.identifier.UUIDString : @"",
                                     @"error_code": @((int)central.state) }];
        }
    }

    [self notifyOperation:@"bt_state_changed"
              extraParams:@{ @"state": @((int)central.state), @"state_name": stateString }];

    // Keep the legacy transport event for compatibility with existing diagnostics.
    [self notifyOperation:@"bt_le_state_update"
              extraParams:@{ @"success": @((int)central.state), @"state": stateString }];
}

- (void) centralManager:(CBCentralManager *)central willRestoreState:(NSDictionary<NSString *,id> *)dict {
    NSArray *peripherals = dict[CBCentralManagerRestoredStatePeripheralsKey];
    if (peripherals) {
        for (CBPeripheral *peripheral in peripherals) {
            _openedPeripherals[[peripheral.identifier UUIDString]] = peripheral;
        }
    }
    
    [self notifyOperation:@"bt_le_peripheral_restore_state" extraParams:nil];
}

- (void) peripheral:(CBPeripheral *)peripheral didReadRSSI:(NSNumber *)RSSI error:(NSError *)error {
	// We don't handle this
}

- (void) peripheralDidUpdateName:(CBPeripheral *)peripheral {
	// We don't handle this
}

- (void) peripheral:(CBPeripheral *)peripheral didModifyServices:(NSArray<CBService *> *)invalidatedServices {
    // The link stays up, so no disconnect will fail these: requests on an
    // invalidated service fail here, each with its own op id. Service
    // discovery is per peripheral and stays queued.
    GMBTPeripheralQueues *queues = [self queuesForPeripheral:peripheral create:NO];
    if (queues) {
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

	peripheral.delegate = self;
    [_openedPeripherals setObject:peripheral forKey:peripheral.identifier.UUIDString];
    [self notifyOperation:@"bt_le_peripheral_service_change" extraParams:@{ @"name": (peripheral.name ?: @""), @"address": peripheral.identifier.UUIDString }];
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
@end

@interface GMBTClassicInquiryDelegate : NSObject <IOBluetoothDeviceInquiryDelegate>
@property (nonatomic, copy) void (^onDeviceFound)(IOBluetoothDevice *device);
@property (nonatomic, copy) void (^onComplete)(IOReturn error, BOOL aborted);
@end

@implementation GMBTClassicInquiryDelegate
- (void)deviceInquiryDeviceFound:(IOBluetoothDeviceInquiry *)sender device:(IOBluetoothDevice *)device {
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
    static std::string to_string(NSString* value)
    {
        return value ? std::string([value UTF8String]) : std::string{};
    }

    static NSString* to_ns(const std::string& value)
    {
        return [NSString stringWithUTF8String:value.c_str()];
    }

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
            for (id item in (NSArray*)value) [out addObject:json_safe_value(item) ?: [NSNull null]];
            return out;
        }
        if ([value isKindOfClass:[NSDictionary class]])
        {
            NSMutableDictionary* out = [NSMutableDictionary dictionary];
            for (id key in (NSDictionary*)value)
                out[[key description]] = json_safe_value(((NSDictionary*)value)[key]) ?: [NSNull null];
            return out;
        }
        return [value description];
    }

    static std::string json_string(NSDictionary* dictionary)
    {
        NSDictionary* safe = (NSDictionary*)json_safe_value(dictionary ?: @{});
        NSError* error = nil;
        NSData* data = [NSJSONSerialization dataWithJSONObject:safe options:0 error:&error];
        if (!data || error) return "{}";
        NSString* text = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
        return to_string(text);
    }

    static std::string normalized_event_type(NSString* oldType)
    {
        std::string type = to_string(oldType);
        if (type.rfind("bt_", 0) == 0)
            return "bluetooth_" + type.substr(3);
        return type;
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
            transport_.opSink = ^(NSNumber* op_id, NSNumber* error_code, LeOpResult result) {
                self->on_op_completed(op_id, error_code, std::move(result));
            };
            [transport_ bt_init];
            GMBT_LOG("Apple transport created and bt_init sent. CoreBluetooth powers on asynchronously - "
                     "watch for 'CBCentralManager state changed: PoweredOn' before expecting a scan to work.");
            message.clear();
            return Error::Ok;
        }

        void shutdown() override
        {
#if TARGET_OS_OSX
            classic_shutdown();
#endif
            if (!transport_) return;
            transport_.eventSink = nil;
            transport_.opSink = nil;
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

        std::int32_t current_bluetooth_state() const override
        {
            if (!transport_ || !transport_.centralManager)
                return 0; // BluetoothState.Unknown
            return static_cast<std::int32_t>(transport_.centralManager.state);
        }

        PermissionStatus permission_status() const override
        {
#if (TARGET_OS_IOS || TARGET_OS_OSX)
            if (@available(iOS 13.0, macOS 10.15, *))
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

        Error permission_request(std::string& message) override
        {
            // CoreBluetooth owns the Apple system permission prompt. Creating the
            // managers in initialize() is the supported trigger; there is no
            // Android-style explicit requestPermissions call.
            message.clear();
            return Error::Ok;
        }

        Error le_scan_start(bool, std::string& message) override
        {
            const double r = [transport_ bt_le_scan_start];
            if (r < 0) { message = "BLE scan could not start"; return Error::OperationFailed; }
            message.clear(); return Error::Ok;
        }
        Error le_scan_stop(std::string& message) override
        {
            const double r = [transport_ bt_le_scan_stop];
            if (r < 0) { message = "BLE scan could not stop"; return Error::OperationFailed; }
            BackendEvent ev; ev.type=BackendEventType::ScanStopped; ev.transport=Transport::LowEnergy; hooks_.push_event(std::move(ev));
            message.clear(); return Error::Ok;
        }
        bool le_scan_is_running() const override { return transport_ && [transport_ bt_le_scan_is_active] > 0.5; }

        Error le_connect(std::uint64_t connection, const DiscoveredDevice& device, std::string& message) override
        {
            const std::string identifier = identifier_from_device(device);
            if (identifier.empty()) { message="BLE device identifier is unavailable"; return Error::InvalidArgument; }
            {
                std::scoped_lock lock(mutex_);
                le_connection_to_id_[connection]=identifier;
                le_id_to_connection_[identifier]=connection;
            }
            const double r=[transport_ bt_le_peripheral_open:to_ns(identifier)];
            if(r<0){ remove_connection(connection); message="BLE connection could not start"; return Error::ConnectionFailed; }
            message.clear(); return Error::Ok;
        }
        Error le_disconnect(std::uint64_t connection,std::string& message) override
        {
            const std::string id=id_for_connection(connection); if(id.empty())return invalid_connection(message);
            const double r=[transport_ bt_le_peripheral_close:to_ns(id)];
            if(r<0){message="BLE disconnect failed";return Error::OperationFailed;}
            remove_connection(connection); message.clear(); return Error::Ok;
        }
        bool le_connection_is_connected(std::uint64_t connection) const override
        {
            const std::string id=id_for_connection(connection); return !id.empty() && [transport_ bt_le_peripheral_is_connected:to_ns(id)]>0.5;
        }
        // The GATT calls, advertise start and add_service hand the core's op
        // id to the transport, which reports it on the completion.
        Error le_services_discover(std::uint64_t op,std::uint64_t c,std::string& m) override { auto id=id_for_connection(c); if(id.empty())return invalid_connection(m); return async_result([transport_ bt_le_peripheral_get_services:to_ns(id) opId:@(op)],"Service discovery could not start",m); }
        Error le_characteristics_discover(std::uint64_t op,std::uint64_t c,const std::string&s,std::string&m) override { auto id=id_for_connection(c); if(id.empty())return invalid_connection(m); return async_result([transport_ bt_le_service_get_characteristics:to_ns(id) service:to_ns(s) opId:@(op)],"Characteristic discovery could not start",m); }
        Error le_descriptors_discover(std::uint64_t op,std::uint64_t c,const std::string&s,const std::string&ch,std::string&m) override { auto id=id_for_connection(c); if(id.empty())return invalid_connection(m); return async_result([transport_ bt_le_characteristic_get_descriptors:to_ns(id) service:to_ns(s) characteristic:to_ns(ch) opId:@(op)],"Descriptor discovery could not start",m); }
        Error le_characteristic_read(std::uint64_t op,std::uint64_t c,const std::string&s,const std::string&ch,std::string&m) override { auto id=id_for_connection(c); if(id.empty())return invalid_connection(m); return async_result([transport_ bt_le_characteristic_read:to_ns(id) service:to_ns(s) characteristic:to_ns(ch) opId:@(op)],"Characteristic read could not start",m); }
        Error le_characteristic_write(std::uint64_t op,std::uint64_t c,const std::string&s,const std::string&ch,const std::string&v,bool with_response,std::string&m) override
        { auto id=id_for_connection(c); if(id.empty())return invalid_connection(m); double r=with_response?[transport_ bt_le_characteristic_write_request:to_ns(id) service:to_ns(s) characteristic:to_ns(ch) value:to_ns(v) opId:@(op)]:[transport_ bt_le_characteristic_write_command:to_ns(id) service:to_ns(s) characteristic:to_ns(ch) value:to_ns(v) opId:@(op)]; return async_result(r,"Characteristic write could not start",m); }
        Error le_characteristic_subscribe(std::uint64_t op,std::uint64_t c,const std::string&s,const std::string&ch,std::int32_t mode,std::string&m) override
        { auto id=id_for_connection(c); if(id.empty())return invalid_connection(m); double r=mode==0?[transport_ bt_le_characteristic_unsubscribe:to_ns(id) service:to_ns(s) characteristic:to_ns(ch) opId:@(op)]:mode==2?[transport_ bt_le_characteristic_indicate:to_ns(id) service:to_ns(s) characteristic:to_ns(ch) opId:@(op)]:[transport_ bt_le_characteristic_notify:to_ns(id) service:to_ns(s) characteristic:to_ns(ch) opId:@(op)]; return async_result(r,"Characteristic subscription could not start",m); }
        Error le_descriptor_read(std::uint64_t op,std::uint64_t c,const std::string&s,const std::string&ch,const std::string&d,std::string&m) override { auto id=id_for_connection(c); if(id.empty())return invalid_connection(m); return async_result([transport_ bt_le_descriptor_read:to_ns(id) service:to_ns(s) characteristic:to_ns(ch) descriptor:to_ns(d) opId:@(op)],"Descriptor read could not start",m); }
        Error le_descriptor_write(std::uint64_t op,std::uint64_t c,const std::string&s,const std::string&ch,const std::string&d,const std::string&v,std::string&m) override { auto id=id_for_connection(c); if(id.empty())return invalid_connection(m); return async_result([transport_ bt_le_descriptor_write:to_ns(id) service:to_ns(s) characteristic:to_ns(ch) descriptor:to_ns(d) value:to_ns(v) opId:@(op)],"Descriptor write could not start",m); }

        // CoreBluetooth advertises the local name and service UUIDs only, and
        // always connectable: anything else asked for is NotSupported here,
        // before anything starts (R1-36).
        Error le_advertise_start(std::uint64_t op,const LeAdvertiseSettings& settings,const LeAdvertiseData& data,std::string& m) override
        {
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
            for (const auto& uuid : data.service_uuids) [uuids addObject:to_ns(uuid)];
            return async_result([transport_ bt_le_advertise_start:data.include_name serviceUUIDs:uuids opId:@(op)],"BLE advertising could not start",m);
        }
        Error le_advertise_stop(std::string&m) override { return async_result([transport_ bt_le_advertise_stop],"BLE advertising could not stop",m); }
        bool le_advertise_is_running() const override { return transport_ && [transport_ bt_le_advertise_is_active] > 0.5; }
        Error le_server_start(std::string&m) override { return async_result([transport_ bt_le_server_open],"GATT server could not start",m); }
        Error le_server_stop(std::string&m) override { return async_result([transport_ bt_le_server_close],"GATT server could not stop",m); }
        bool le_server_is_running() const override { return _isServerOpen; }
        Error le_server_add_service(std::uint64_t op,const std::string&j,std::string&m) override { return async_result([transport_ bt_le_server_add_service:to_ns(j) opId:@(op)],"GATT service could not be added",m); }
        Error le_server_clear_services(std::string&m) override { return async_result([transport_ bt_le_server_clear_services],"GATT services could not be cleared",m); }
        Error le_server_respond_read(std::int32_t r,std::int32_t st,const std::string&v,std::string&m) override { double x=[transport_ bt_le_server_respond_read:r status:st value:to_ns(v)]; if(x>0){m.clear();return Error::Ok;}m="GATT read response failed";return Error::OperationFailed; }
        Error le_server_respond_write(std::int32_t r,std::int32_t st,std::string&m) override { double x=[transport_ bt_le_server_respond_write:r status:st]; if(x>0){m.clear();return Error::Ok;}m="GATT write response failed";return Error::OperationFailed; }
        Error le_server_notify_value(const std::string&s,const std::string&ch,const std::string&central,const std::string&v,std::string&m) override
        {
            const double r=[transport_ bt_le_server_notify_value:to_ns(s) characteristicUuid:to_ns(ch) central:to_ns(central) value:to_ns(v)];
            if(r==-2){m="That central is not subscribed to the characteristic";return Error::NotFound;}
            return async_result(r,"GATT notification could not start",m);
        }

#if TARGET_OS_OSX

        Error classic_scan_start(std::string& message) override
        {
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
                to_string([[IOBluetoothHostController defaultController] addressAsString]).c_str());

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
            if (classic_find_rfcomm_channel(btDevice, service_uuid, false, cachedChannel))
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
            bool connecting = false;
            {
                std::scoped_lock lock(classic_mutex_);
                auto it = classic_connections_.find(connection);
                if (it == classic_connections_.end()) return invalid_connection(message);
                state = it->second;

                // A connect still in SDP or opening is cancelled: its record
                // goes now, so a late SDP or open completion finds nothing, and
                // the core fires the connect callback once as cancelled.
                connecting = !state->connected;
                if (connecting)
                {
                    classic_connections_.erase(it);
                    auto pending = classic_pending_sdp_.find(connection);
                    if (pending != classic_pending_sdp_.end())
                    {
                        sdp = pending->second;
                        classic_pending_sdp_.erase(pending);
                    }
                }
            }

            if (sdp)
            {
                sdp.onComplete = nil;
                gmbt_park(sdp);
            }
            if (connecting)
                classic_detach_channel(state->channel, state->delegate);
            else
                [state->channel closeChannel];
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

            const BluetoothRFCOMMMTU mtu = [state->channel getMTU];
            std::size_t offset = 0;
            while (offset < size)
            {
                const std::size_t chunk = std::min<std::size_t>(mtu > 0 ? mtu : size, size - offset);
                const IOReturn status = [state->channel writeSync:(void*)(data + offset) length:(UInt16)chunk];
                if (status != kIOReturnSuccess) { message = "Classic write failed"; return Error::OperationFailed; }
                offset += chunk;
            }
            message.clear();
            return Error::Ok;
        }

        std::size_t classic_receive_bytes(std::uint64_t connection, std::uint8_t* out, std::size_t max_size) override
        {
            auto state = find_classic_connection(connection);
            if (!state) return 0;
            std::scoped_lock lock(state->receive_mutex);
            const std::size_t n = std::min(max_size, state->received.size());
            std::copy(state->received.begin(), state->received.begin() + n, out);
            state->received.erase(state->received.begin(), state->received.begin() + n);
            return n;
        }

        Error classic_server_start(const std::string& name, const std::string& service_uuid, std::string& message) override
        {
            if (classic_server_running_) { message.clear(); return Error::Ok; }

            IOBluetoothSDPUUID* uuid = classic_uuid_from_string(service_uuid);
            if (!uuid) { message = "Invalid service UUID"; return Error::InvalidArgument; }

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
                @"0100 - ServiceName" : to_ns(name),
            };

            IOBluetoothSDPServiceRecord* record = [IOBluetoothSDPServiceRecord publishedServiceRecordWithDictionary:serviceDict];
            if (!record) { message = "Classic service record could not be published"; return Error::OperationFailed; }

            BluetoothRFCOMMChannelID channelID = 0;
            const IOReturn channelStatus = [record getRFCOMMChannelID:&channelID];
            BluetoothSDPServiceRecordHandle recordHandle = 0;
            [record getServiceRecordHandle:&recordHandle];
            GMBT_LOG("Classic server: record published, handle=0x%08x RFCOMM channel=%d IOReturn=0x%08x uuid=%s local_address=%s",
                (unsigned)recordHandle, (int)channelID, channelStatus, service_uuid.c_str(),
                to_string([[IOBluetoothHostController defaultController] addressAsString]).c_str());
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

            IOBluetoothDevice* btDevice = const_cast<AppleBackend*>(this)->classic_device_for_address(device.address);
            return btDevice != nil && [btDevice isPaired];
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

        static Error async_result(double value,const char* failure,std::string&message)
        { if(value<0){message=failure;return Error::OperationFailed;}message.clear();return Error::Ok; }
        static Error invalid_connection(std::string&message){message="Invalid connection handle";return Error::InvalidHandle;}

        std::string id_for_connection(std::uint64_t c) const
        { std::scoped_lock lock(mutex_); auto it=le_connection_to_id_.find(c); return it==le_connection_to_id_.end()?std::string{}:it->second; }
        void remove_connection(std::uint64_t c)
        { std::scoped_lock lock(mutex_); auto it=le_connection_to_id_.find(c); if(it!=le_connection_to_id_.end()){le_id_to_connection_.erase(it->second);le_connection_to_id_.erase(it);} }
        std::uint64_t connection_for_id(const std::string&id) const
        { std::scoped_lock lock(mutex_); auto it=le_id_to_connection_.find(id); return it==le_id_to_connection_.end()?0:it->second; }

        static std::string identifier_from_device(const DiscoveredDevice& d)
        {
            constexpr const char* prefix="apple:ble:";
            if(d.id.rfind(prefix,0)==0) return d.id.substr(std::char_traits<char>::length(prefix));
            if(d.address_available&&!d.address.empty()) return d.address;
            return {};
        }

#if TARGET_OS_OSX

        struct ClassicConnectionState
        {
            std::uint64_t connection = 0;
            IOBluetoothRFCOMMChannel* channel = nil;
            GMBTClassicChannelDelegate* delegate = nil;
            std::mutex receive_mutex;
            std::deque<std::uint8_t> received;
            std::atomic_bool connected{false};
        };

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
            std::string s = to_string(addressString);
            for (auto& c : s) c = (c == '-') ? ':' : static_cast<char>(::toupper(static_cast<unsigned char>(c)));
            return s;
        }

        // CBUUID already parses both 16-bit short-form ("1101") and full
        // 128-bit service UUID strings; reuse it to build the IOBluetoothSDPUUID
        // IOBluetooth itself expects rather than duplicating that parsing here.
        static IOBluetoothSDPUUID* classic_uuid_from_string(const std::string& text)
        {
            if (text.empty()) return nil;
            CBUUID* cb = [CBUUID UUIDWithString:to_ns(text)];
            if (!cb || !cb.data) return nil;
            return [IOBluetoothSDPUUID uuidWithBytes:cb.data.bytes length:(int)cb.data.length];
        }

        void handle_classic_device_found(IOBluetoothDevice* device)
        {
            if (!device) return;
            const std::string address = classic_format_address([device addressString]);
            {
                std::scoped_lock lock(classic_mutex_);
                classic_devices_by_address_[address] = device;
            }
            DiscoveredDevice d;
            d.transport = Transport::Classic;
            d.id = "apple:classic:" + address;
            d.name = to_string([device name]);
            d.address = address;
            d.address_available = true;
            d.connectable = true;
            hooks_.upsert_device(d);
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

        // Expires with the backend, so delayed blocks can tell it is gone.
        std::shared_ptr<int> lifetime_ = std::make_shared<int>(0);

        // Finds the RFCOMM channel for service_uuid in the device's current SDP
        // records. With allow_any_rfcomm, falls back to the first record that
        // has an RFCOMM channel when the UUID is not listed.
        static bool classic_find_rfcomm_channel(IOBluetoothDevice* device, const std::string& service_uuid,
                                                bool allow_any_rfcomm, BluetoothRFCOMMChannelID& channelID)
        {
            IOBluetoothSDPUUID* uuid = classic_uuid_from_string(service_uuid);
            if (uuid)
            {
                IOBluetoothSDPServiceRecord* record = [device getServiceRecordForUUID:uuid];
                if (record && [record getRFCOMMChannelID:&channelID] == kIOReturnSuccess) return true;
            }
            if (!allow_any_rfcomm && uuid) return false;
            for (IOBluetoothSDPServiceRecord* record in [device services])
            {
                if ([record getRFCOMMChannelID:&channelID] == kIOReturnSuccess) return true;
            }
            return false;
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
            if (!classic_find_rfcomm_channel(device, service_uuid, true, channelID))
            {
                complete_classic_connect(connection, Error::NotFound, "No RFCOMM service was found on the device");
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

        void complete_classic_connect(std::uint64_t connection, Error error, const char* message)
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
            ev.message = message ? message : "";
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
            }
            if (channel)
            {
                [channel setDelegate:nil];
                [channel closeChannel];
            }
            gmbt_release_after_callback(channel);
            gmbt_release_after_callback(delegate);
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

        void handle_classic_channel_data(std::uint64_t connection, NSData* data)
        {
            auto state = find_classic_connection(connection);
            if (!state) return;
            std::int32_t available;
            {
                std::scoped_lock lock(state->receive_mutex);
                const std::uint8_t* bytes = static_cast<const std::uint8_t*>(data.bytes);
                state->received.insert(state->received.end(), bytes, bytes + data.length);
                available = static_cast<std::int32_t>(state->received.size());
            }
            BackendEvent ev;
            ev.type = BackendEventType::ClassicDataAvailable;
            ev.transport = Transport::Classic;
            ev.connection = connection;
            ev.value = available;
            hooks_.push_event(std::move(ev));
        }

        void handle_classic_channel_closed(std::uint64_t connection)
        {
            bool wasConnected = false;
            {
                std::scoped_lock lock(classic_mutex_);
                auto it = classic_connections_.find(connection);
                if (it == classic_connections_.end()) return;
                wasConnected = it->second->connected;
                gmbt_release_after_callback(it->second->channel);
                gmbt_release_after_callback(it->second->delegate);
                classic_connections_.erase(it);
            }
            // A channel that closes before it ever finished opening is reported
            // through the ClassicConnected completion path instead, not here.
            if (!wasConnected) return;
            BackendEvent ev;
            ev.type = BackendEventType::ClassicDisconnected;
            ev.transport = Transport::Classic;
            ev.connection = connection;
            ev.error = Error::Disconnected;
            ev.message = "RFCOMM channel closed";
            hooks_.push_event(std::move(ev));
        }

        void handle_classic_server_channel_opened(IOBluetoothRFCOMMChannel* channel)
        {
            IOBluetoothDevice* device = [channel getDevice];
            const std::string address = classic_format_address([device addressString]);
            GMBT_LOG("Classic server: incoming RFCOMM channel %d from %s (%s)",
                (int)[channel getChannelID], address.c_str(), to_string([device name]).c_str());
            DiscoveredDevice d;
            d.transport = Transport::Classic;
            d.id = "apple:classic:" + address;
            d.name = to_string([device name]);
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

        void on_op_completed(NSNumber* op_id, NSNumber* error_code, LeOpResult result)
        {
            BackendEvent ev;
            ev.type = BackendEventType::LeOpCompleted;
            ev.transport = Transport::LowEnergy;
            ev.op_id = op_id ? op_id.unsignedLongLongValue : 0;
            if (error_code)
            {
                ev.error = Error::OperationFailed;
                ev.value = error_code.intValue;
            }
            ev.result = std::move(result);
            hooks_.push_event(std::move(ev));
        }

        void on_event(NSString* type, NSDictionary* params)
        {
            if (!type)
            {
                GMBT_LOG("transport raised an event with no type - ignored");
                return;
            }
            GMBT_LOG("transport event '%s' -> normalized '%s'",
                to_string(type).c_str(), normalized_event_type(type).c_str());

            // bt_le_scan_start can fail asynchronously: the transport defers the
            // request while CBCentralManager's state is still Unknown/Resetting,
            // then reports failure later from centralManagerDidUpdateState: if the
            // central lands on a terminal non-PoweredOn state. That failure has no
            // caller waiting on the original synchronous return value anymore, and
            // BackendEventType::LeEvent has no GML callback wired up in the core
            // - so without this, the scan silently never starts and no GML callback
            // ever fires. Re-signal it on the ScanStopped channel instead, which is
            // already wired to bluetooth_set_callback_scan_stopped for both scan types.
            if ([type isEqualToString:@"bt_le_scan_start"] &&
                params[@"success"] && ![params[@"success"] boolValue])
            {
                BackendEvent ev;
                ev.type = BackendEventType::ScanStopped;
                ev.transport = Transport::LowEnergy;
                switch (static_cast<CBManagerState>([params[@"error_code"] intValue]))
                {
                    case CBManagerStateUnsupported:
                        ev.error = Error::NotSupported;
                        ev.message = "Bluetooth LE is not supported on this device";
                        break;
                    case CBManagerStateUnauthorized:
                        ev.error = Error::PermissionDenied;
                        ev.message = "Bluetooth permission was denied";
                        break;
                    case CBManagerStatePoweredOff:
                        ev.error = Error::BluetoothDisabled;
                        ev.message = "Bluetooth is powered off";
                        break;
                    default:
                        ev.error = Error::OperationFailed;
                        ev.message = "BLE scan could not start";
                        break;
                }
                hooks_.push_event(std::move(ev));
                return;
            }

            NSString* address = [params[@"address"] isKindOfClass:[NSString class]] ? params[@"address"] : nil;
            if ([type isEqualToString:@"bt_le_scan_result"] && address)
            {
                DiscoveredDevice d;
                const std::string id=to_string(address);
                d.transport=Transport::LowEnergy;
                d.id="apple:ble:"+id;
                d.name=to_string(params[@"name"]);
                // Apple exposes a stable CoreBluetooth identifier, not a public MAC.
                d.address_available=false;
                d.rssi=[params[@"raw_signal"] intValue];
                d.rssi_available=params[@"raw_signal"]!=nil;
                d.connectable=params[@"is_connectable"]?[params[@"is_connectable"] boolValue]:true;
                hooks_.upsert_device(d);
            }

            NSMutableDictionary* decorated=[NSMutableDictionary dictionaryWithDictionary:params?:@{}];
            if(address)
            {
                const std::uint64_t c=connection_for_id(to_string(address));
                if(c!=0) decorated[@"connection"]=@(c);
            }

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
