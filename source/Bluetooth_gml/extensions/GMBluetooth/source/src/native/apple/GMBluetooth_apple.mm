#include "GMBluetooth_backend.h"
#include "GMBluetooth_log.h"

#if defined(__APPLE__)

#import <Foundation/Foundation.h>
#import <CoreBluetooth/CoreBluetooth.h>
#import <CoreLocation/CoreLocation.h>
#include <TargetConditionals.h>

#if TARGET_OS_IOS
#import <UIKit/UIKit.h>
#else
// macOS only - NSHost is part of Foundation (already imported above)
#endif

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>



@interface GMBTQueuedMutableDictionary : NSObject
@property (nonatomic, strong) NSNumber *asyncId;
@property (nonatomic, strong) NSMutableDictionary *dictionary;

- (instancetype)initWithAsyncId:(NSNumber *)asyncId dictionary:(NSMutableDictionary *)dictionary;
@end

@interface GMBTQueuedMutableService : NSObject
@property (nonatomic, strong) NSNumber *asyncId;
@property (nonatomic, strong) CBMutableService *service;

- (instancetype)initWithAsyncId:(NSNumber *)asyncId service:(CBMutableService *)service;
@end

@interface GMBTQueuedPeripheral : NSObject
@property (nonatomic, strong) NSNumber *asyncId;
@property (nonatomic, strong) CBPeripheral *peripheral;

- (instancetype)initWithAsyncId:(NSNumber *)asyncId peripheral:(CBPeripheral *)peripheral;
@end

@interface GMBTQueuedTimedPeripheral : GMBTQueuedPeripheral
@property (nonatomic, strong) CBCentralManager *manager;
@property (nonatomic, strong) NSTimer *timer;

- (instancetype)initWithAsyncId:(NSNumber *)asyncId peripheral:(CBPeripheral *)peripheral timer:(NSTimer *)timer;
@end

@interface GMBTQueuedService : GMBTQueuedPeripheral
@property (nonatomic, strong) CBService *service;

- (instancetype)initWithAsyncId:(NSNumber *)asyncId peripheral:(CBPeripheral *)peripheral service:(CBService *) service;
@end

@interface GMBTQueuedCharacteristic : GMBTQueuedPeripheral
@property (nonatomic, strong) CBCharacteristic *characteristic;

- (instancetype)initWithAsyncId:(NSNumber *)asyncId peripheral:(CBPeripheral *)peripheral characteristic:(CBCharacteristic *) characteristic;
@end

@interface GMBTQueuedCharacteristicWithData : GMBTQueuedCharacteristic
@property (nonatomic, strong) NSData* data;

- (instancetype)initWithAsyncId:(NSNumber *)asyncId peripheral:(CBPeripheral *)peripheral characteristic:(CBCharacteristic *) characteristic data:(NSData*) data;
@end

@interface GMBTQueuedDescriptor : GMBTQueuedPeripheral
@property (nonatomic, strong) CBDescriptor *descriptor;

- (instancetype)initWithAsyncId:(NSNumber *)asyncId peripheral:(CBPeripheral *)peripheral descriptor:(CBDescriptor *) descriptor;
@end

@interface GMBTQueuedDescriptorWithData : GMBTQueuedDescriptor
@property (nonatomic, strong) NSData* data;

- (instancetype)initWithAsyncId:(NSNumber *)asyncId peripheral:(CBPeripheral *)peripheral descriptor:(CBDescriptor *) descriptor data:(NSData*) data;
@end

@interface GMBluetoothAppleTransport:NSObject<CBCentralManagerDelegate,CBPeripheralDelegate,CBPeripheralManagerDelegate,CLLocationManagerDelegate>

@property(nonatomic, copy) void (^eventSink)(NSString *type, NSDictionary *params);

// CLIENT

@property(nonatomic, strong) CBCentralManager *centralManager;
@property(nonatomic, strong) CLLocationManager *locationManager;

@property(nonatomic, strong) NSMutableArray<GMBTQueuedPeripheral *> *openPeripheralQueue;
@property(nonatomic, strong) NSMutableArray<GMBTQueuedPeripheral *> *closePeripheralQueue;

@property(nonatomic, strong) NSMutableArray<GMBTQueuedPeripheral *> *fetchServicesQueue;
@property(nonatomic, strong) NSMutableArray<GMBTQueuedService *> *fetchCharacteristicsQueue;
@property(nonatomic, strong) NSMutableArray<GMBTQueuedCharacteristic *> *fetchDescriptorsQueue;

@property(nonatomic, strong) NSMutableArray<GMBTQueuedCharacteristic *> *readCharacteristicQueue;
@property(nonatomic, strong) NSMutableArray<GMBTQueuedCharacteristicWithData *> *writeCharacteristicQueue;
@property(nonatomic, strong) NSMutableArray<GMBTQueuedCharacteristicWithData *> *notifyCharacteristicQueue;
@property(nonatomic, strong) NSMutableArray<GMBTQueuedDescriptor *> *readDescriptorQueue;
@property(nonatomic, strong) NSMutableArray<GMBTQueuedDescriptorWithData *> *writeDescriptorQueue;

@property(nonatomic, strong) NSMutableDictionary <NSString *, CBPeripheral *> *discoveredPeripherals;
@property(nonatomic, strong) NSMutableDictionary <NSString *, CBPeripheral *> *openedPeripherals;
@property(nonatomic, strong) NSMutableDictionary <NSString *, CBPeripheral *> *connectedPeripherals;

// SERVER

@property(nonatomic, strong) CBPeripheralManager *peripheralManager;

@property(nonatomic, strong) NSMutableArray <GMBTQueuedMutableService *> *addServiceQueue;
@property(nonatomic, strong) NSMutableArray <GMBTQueuedMutableDictionary *> *startAdvertisementQueue;

@property(nonatomic, strong) NSMutableDictionary <NSString *, CBMutableService *> *addedServices;

@property(nonatomic, strong) NSMutableDictionary <NSNumber *, CBATTRequest *> *readRequestsLookup;
@property(nonatomic, strong) NSMutableDictionary <NSNumber *, CBATTRequest *> *writeRequestsLookup;

@end


@implementation GMBTQueuedMutableDictionary
- (instancetype)initWithAsyncId:(NSNumber *)asyncId dictionary:(NSMutableDictionary *)dictionary {
    self = [super init];
    if (self) {
        _asyncId = asyncId;
        _dictionary = dictionary;
    }
    return self;
}
@end

@implementation GMBTQueuedMutableService
- (instancetype)initWithAsyncId:(NSNumber *)asyncId service:(CBMutableService *)service {
    self = [super init];
    if (self) {
        _asyncId = asyncId;
        _service = service;
    }
    return self;
}
@end

@implementation GMBTQueuedPeripheral
- (instancetype)initWithAsyncId:(NSNumber *)asyncId peripheral:(CBPeripheral *)peripheral {
    self = [super init];
    if (self) {
        _asyncId = asyncId;
        _peripheral = peripheral;
    }
    return self;
}
@end

@implementation GMBTQueuedTimedPeripheral
- (instancetype)initWithAsyncId:(NSNumber *)asyncId peripheral:(CBPeripheral *)peripheral timer:(NSTimer *)timer {
    self = [super initWithAsyncId:asyncId peripheral:peripheral];
    if (self) {
        _timer = timer;
    }
    return self;
}
@end

@implementation GMBTQueuedService
- (instancetype)initWithAsyncId:(NSNumber *)asyncId peripheral:(CBPeripheral *)peripheral service:(CBService *) service {
    self = [super initWithAsyncId:asyncId peripheral:peripheral];
    if (self) {
        _service = service;
    }
    return self;
}
@end

@implementation GMBTQueuedCharacteristic
- (instancetype)initWithAsyncId:(NSNumber *)asyncId peripheral:(CBPeripheral *)peripheral characteristic:(CBCharacteristic *) characteristic {
    self = [super initWithAsyncId:asyncId peripheral:peripheral];
    if (self) {
        _characteristic = characteristic;
    }
    return self;
}
@end

@implementation GMBTQueuedCharacteristicWithData
- (instancetype)initWithAsyncId:(NSNumber *)asyncId peripheral:(CBPeripheral *)peripheral characteristic:(CBCharacteristic *) characteristic data:(NSData*) data {
    self = [super initWithAsyncId:asyncId peripheral:peripheral characteristic:characteristic];
    if (self) {
        _data = data;
    }
    return self;
}
@end

@implementation GMBTQueuedDescriptor
- (instancetype)initWithAsyncId:(NSNumber *)asyncId peripheral:(CBPeripheral *)peripheral descriptor:(CBDescriptor *) descriptor {
    self = [super initWithAsyncId:asyncId peripheral:peripheral];
    if (self) {
        _descriptor = descriptor;
    }
    return self;
}
@end

@implementation GMBTQueuedDescriptorWithData
- (instancetype)initWithAsyncId:(NSNumber *)asyncId peripheral:(CBPeripheral *)peripheral descriptor:(CBDescriptor *) descriptor data:(NSData*) data {
    self = [super initWithAsyncId:asyncId peripheral:peripheral descriptor:descriptor];
    if (self) {
        _data = data;
    }
    return self;
}
@end

@implementation GMBluetoothAppleTransport

// ASYNC ID GENERATOR

- (int)generateAsyncId {
    static int currentID = 0;
    return currentID++;
}

- (int)generateRequestId {
    static int currentID = 0;
    return currentID++;
}

// ASYNC EVENTS UTILITIES

- (void) notifyOperation:(NSString *)functionName extraParams:(NSDictionary *)extraParams {
    if (!self.eventSink) return;
    NSDictionary *params = extraParams ? extraParams : @{};
    self.eventSink(functionName, params);
}

- (void) notifyAsyncOperation:(NSString *)functionName asyncId:(int)asyncId extraParams:(NSDictionary *)extraParams {
    NSMutableDictionary *params = [NSMutableDictionary dictionary];
    params[@"async_id"] = @(asyncId);
    
    if (extraParams) {
        [params addEntriesFromDictionary:extraParams];
    }
    
    [self notifyOperation:functionName extraParams:params];
}

- (void) notifyAsyncOperationError:(NSString *)functionName asyncId:(int)asyncId errorCode:(int)errorCode extraParams:(NSDictionary *)extraParams {
    NSMutableDictionary *params = [NSMutableDictionary dictionary];
    params[@"success"] = @NO; // Assuming an error indicates non-success
    params[@"error_code"] = @(errorCode);
    
    if (extraParams) {
        [params addEntriesFromDictionary:extraParams];
    }
    
    [self notifyAsyncOperation:functionName asyncId:asyncId extraParams:params];
}

- (void) notifyAsyncOperationSuccess:(NSString *)functionName asyncId:(int)asyncId extraParams:(NSDictionary *)extraParams {
    NSMutableDictionary *params = [NSMutableDictionary dictionary];
    params[@"success"] = @YES;
    
    if (extraParams) {
        [params addEntriesFromDictionary:extraParams];
    }
    
    [self notifyAsyncOperation:functionName asyncId:asyncId extraParams:params];
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

// BLUETOOTH IMPLEMENTATION

- (id) init {
    self = [super init];
    if (self) {
        
        _discoveredPeripherals = [NSMutableDictionary new];
        _openedPeripherals = [NSMutableDictionary new];
        _connectedPeripherals = [NSMutableDictionary new];
        
        _addedServices = [NSMutableDictionary new];
        
        _startAdvertisementQueue = [NSMutableArray new];
        
        _addServiceQueue = [NSMutableArray new];
        _readRequestsLookup = [NSMutableDictionary new];
        _writeRequestsLookup = [NSMutableDictionary new];
        
        _openPeripheralQueue = [NSMutableArray new];
        _closePeripheralQueue = [NSMutableArray new];
        
        _fetchServicesQueue = [NSMutableArray new];
        _fetchCharacteristicsQueue = [NSMutableArray new];
        _fetchDescriptorsQueue = [NSMutableArray new];
        
        _readCharacteristicQueue = [NSMutableArray new];
        _writeCharacteristicQueue = [NSMutableArray new];
        _notifyCharacteristicQueue = [NSMutableArray new];
        _readDescriptorQueue = [NSMutableArray new];
        _writeDescriptorQueue = [NSMutableArray new];
    }
    return self;
}

// Scan state. Declared here rather than in the SCANNER section below because
// bt_end has to reset them.
static bool _isScanning = false;

// A scan asked for before the central reached PoweredOn. CoreBluetooth reaches
// that state asynchronously, several runloop turns after bt_init, and discards
// any scan requested in the meantime - so the request is held here and replayed
// from centralManagerDidUpdateState: instead of being silently lost.
static bool _scanPendingPowerOn = false;
static int _pendingScanAsyncId = 0;

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

    // iOS 12+ requires CLLocationManager for BLE scanning
    #if TARGET_OS_IOS
    _locationManager = [[CLLocationManager alloc] init];
    _locationManager.delegate = self;

    // Request location permission (required for BLE scanning on iOS 12+)
    if ([_locationManager respondsToSelector:@selector(requestWhenInUseAuthorization)]) {
        [_locationManager requestWhenInUseAuthorization];
    }
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

    _centralManager = nil;
    _peripheralManager = nil;
    _locationManager = nil;
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

    int asyncId = [self generateAsyncId];

    int authorization = -1;
    if (@available(iOS 13.0, macOS 10.15, *)) {
        authorization = (int)[CBManager authorization];
    }
    NSLog(@"[GMBluetooth] bt_le_scan_start: asyncId=%d CBCentralManager.state=%d (5=PoweredOn) authorization=%d",
          asyncId, (int)_centralManager.state, authorization);

    if (_centralManager.state != CBManagerStatePoweredOn) {
        _scanPendingPowerOn = true;
        _pendingScanAsyncId = asyncId;
        NSLog(@"[GMBluetooth] bt_le_scan_start: central is not PoweredOn yet - scan DEFERRED, "
              @"it will start automatically from centralManagerDidUpdateState:");
        return asyncId;
    }

    [self beginScanWithAsyncId:asyncId];
    return asyncId;
}

// Issues the actual CoreBluetooth scan. Only ever called with the central
// already PoweredOn, so _isScanning tracks a scan that really started.
- (void) beginScanWithAsyncId:(int)asyncId {

    _isScanning = true;

    [_centralManager scanForPeripheralsWithServices:nil options:nil];

    NSLog(@"[GMBluetooth] beginScan: asyncId=%d CBCentralManager.isScanning=%d",
          asyncId, (int)[_centralManager isScanning]);

    [self notifyAsyncOperationSuccess:@"bt_le_scan_start" asyncId:asyncId extraParams:nil];
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
    int asyncId = [self generateAsyncId];
    
    [_centralManager stopScan];
    
    [self notifyAsyncOperationSuccess:@"bt_le_scan_stop" asyncId:asyncId extraParams:nil];
    
    return asyncId;
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

static bool _isAdvertising = false;

- (void) handleStartAdvertisementQueue {
    [self handleQueue:_startAdvertisementQueue withBlock:^(GMBTQueuedMutableDictionary *queuedMutableDictionary) {
        [self->_peripheralManager startAdvertising:queuedMutableDictionary.dictionary];
    }];
}

- (double) bt_le_advertise_start:(NSString*)settings data:(NSString*)data {
    
    // Check if is already advertising
    if (_isAdvertising) return -1;
    
    // Parse the JSON strings
    NSError *error = nil;
    NSDictionary *settingsDict = [NSJSONSerialization JSONObjectWithData:[settings dataUsingEncoding:NSUTF8StringEncoding] options:0 error:&error];
    NSDictionary *dataDict = [NSJSONSerialization JSONObjectWithData:[data dataUsingEncoding:NSUTF8StringEncoding] options:0 error:&error];

    // Handle JSON parsing error
    if (error) {
        NSLog(@"JSON Parsing Error: %@", error.localizedDescription);
        return -1;
    }
    
    // Process the settings data
    
    // The property 'connectable' is set to on if services array is not empty
    // BOOL connectable = [settingsDict[@"connectable"] boolValue];
    // Currently, CBPeripheralManager doesn't provide a direct way to set discoverability or timeout, so they might be ignored or handled differently.
    // NSInteger timeout = [settingsDict[@"timeout"] integerValue];
    NSInteger txPowerLevel = [settingsDict[@"txPowerLevel"] integerValue];
    
    // Process the data to be advertised
    NSMutableDictionary* advertisementData = [NSMutableDictionary dictionary];

    // Use preprocessor directive (compatible with iOS|macOS)
    #if TARGET_OS_IOS
    NSString *deviceName = [[UIDevice currentDevice] name];
    #elif TARGET_OS_MAC
    NSString *deviceName = [[NSHost currentHost] localizedName];
    #endif

    if (![dataDict[@"includeName"] boolValue]) {
        advertisementData[CBAdvertisementDataLocalNameKey] = deviceName;
    }
    if ([dataDict[@"includePowerLevel"] boolValue]) {
        advertisementData[CBAdvertisementDataTxPowerLevelKey] = @(txPowerLevel);
    }
    
    // Convert Base64 encoded service data to NSData and add to the dictionary
    NSMutableArray *serviceUUIDs = [NSMutableArray array];
    for (NSDictionary *service in dataDict[@"services"]) {
        CBUUID *serviceUUID = [CBUUID UUIDWithString:service[@"uuid"]];
        [serviceUUIDs addObject:serviceUUID];
        
        NSData *serviceData = [[NSData alloc] initWithBase64EncodedString:service[@"data"] options:0];
        advertisementData[CBAdvertisementDataServiceDataKey] = @{serviceUUID: serviceData};
    }
    advertisementData[CBAdvertisementDataServiceUUIDsKey] = serviceUUIDs;

    // Add manufacturer data if it exists
    NSDictionary *manufacturer = dataDict[@"manufacturer"];
    if (manufacturer) {
        int manufacturerId = [manufacturer[@"id"] intValue];
        NSData *manufacturerData = [[NSData alloc] initWithBase64EncodedString:manufacturer[@"data"] options:0];
        advertisementData[CBAdvertisementDataManufacturerDataKey] = [NSData dataWithBytes:&manufacturerId length:sizeof(manufacturerId)];
        [advertisementData[CBAdvertisementDataManufacturerDataKey] appendData:manufacturerData];
    }
    
    // Generate new asyncId for this task
    int asyncId = [self generateAsyncId];
    
    GMBTQueuedMutableDictionary* queuedMutableDictionary = [[GMBTQueuedMutableDictionary alloc] initWithAsyncId:@(asyncId) dictionary:advertisementData];
    [self queueEnqueue:_startAdvertisementQueue value:queuedMutableDictionary withHandler:^(){ [self handleStartAdvertisementQueue]; }];
    
    // Return asyncId
    return asyncId;
}

- (double) bt_le_advertise_stop {
    
    if (!_isAdvertising) return -1;
    
    [_peripheralManager stopAdvertising];
    
    int asyncId = [self generateAsyncId];
    [self notifyAsyncOperationSuccess:@"bt_le_advertise_stop" asyncId:asyncId extraParams:nil];
    
    return asyncId;
}

- (double) bt_le_advertise_is_active {
    return _peripheralManager.isAdvertising ? 1.0 : 0.0;
}

- (void) peripheralManagerDidStartAdvertising:(CBPeripheralManager *)peripheral error:(NSError *)error {
    
    GMBTQueuedMutableDictionary* queuedAdvertisementData = [self queueDequeue:_startAdvertisementQueue];
    
    int asyncId = [queuedAdvertisementData.asyncId intValue];
    NSString* functionName = @"bt_le_advertise_start";
    
    if (error) [self notifyAsyncOperationError:functionName asyncId:asyncId errorCode:(int)error.code extraParams:nil];
    else [self notifyAsyncOperationSuccess:functionName asyncId:asyncId extraParams:nil];
    
    [self handleStartAdvertisementQueue];
}

// ####################################################################################
// # SERVER
// ####################################################################################

static bool _isServerOpen = false;

- (void) handleAddServiceQueue {
    [self handleQueue:_addServiceQueue withBlock:^(GMBTQueuedMutableService *queuedMutableService) {
        [self->_peripheralManager addService:queuedMutableService.service];
    }];
}

- (double) bt_le_server_open {
    
    if (_isServerOpen) return -1;
    
    _isServerOpen = true;
    
    int asyncId = [self generateAsyncId];
    [self notifyAsyncOperationSuccess:@"bt_le_server_open" asyncId:asyncId extraParams:nil];
    
    return asyncId;
}

- (double) bt_le_server_add_service:(NSString*) serviceDataString {
    
    if (!_isServerOpen) return -1;
    
    NSData *data = [serviceDataString dataUsingEncoding:NSUTF8StringEncoding];
    NSError *error = nil;
    NSDictionary *serviceData = [NSJSONSerialization JSONObjectWithData:data options:0 error:&error];
    
    if (error) {
        // Handle error, perhaps with a callback or NSLog
        NSLog(@"JSON Parsing Error: %@", error.localizedDescription);
        return -1;
    }
    
    // Create the service
    CBUUID *serviceUUID = [CBUUID UUIDWithString:serviceData[@"uuid"]];
    CBMutableService *service = [[CBMutableService alloc] initWithType:serviceUUID primary:YES];
    
    NSData *emptyValue = [NSData data];
    
    // Extract characteristics
    NSMutableArray *characteristicsArray = [NSMutableArray new];
    for (NSDictionary *charDict in serviceData[@"characteristics"]) {
        CBUUID *charUUID = [CBUUID UUIDWithString:charDict[@"uuid"]];
        CBCharacteristicProperties charProperties = [charDict[@"properties"] unsignedIntValue];
        CBAttributePermissions permissions = [charDict[@"permissions"] unsignedIntValue];
        
        // Map permissions from Android to Apple
        CBAttributePermissions charPermissions = 1 | 2;
        if (permissions & 1) charPermissions |= 1;
        if (permissions & (2 | 4)) charPermissions |= 4;
        if (permissions & 16) charPermissions |= 2;
        if (permissions & (32 | 64)) charPermissions |= 8;
                
        CBMutableCharacteristic *characteristic = [[CBMutableCharacteristic alloc] initWithType:charUUID properties:charProperties value:nil permissions:charPermissions];
        
        // Extract descriptors for each characteristic
        NSMutableArray *descriptorsArray = [NSMutableArray new];
        for (NSDictionary *descDict in charDict[@"descriptors"]) {
            CBUUID *descUUID = [CBUUID UUIDWithString:descDict[@"uuid"]];
            // CBAttributePermissions descPermissions = [descDict[@"permissions"] unsignedIntValue];
            
            CBMutableDescriptor *descriptor = [[CBMutableDescriptor alloc] initWithType:descUUID value:emptyValue];
            [descriptorsArray addObject:descriptor];
        }
        
        NSString *description = charDict[@"description"];
        if (description) {
            CBUUID *uuid = [CBUUID UUIDWithString:CBUUIDCharacteristicUserDescriptionString];
            [descriptorsArray addObject:[[CBMutableDescriptor alloc] initWithType:uuid value:description]];
        }
        
        characteristic.descriptors = descriptorsArray;
        [characteristicsArray addObject:characteristic];
    }
    service.characteristics = characteristicsArray;
    
    int asyncId = [self generateAsyncId];
    GMBTQueuedMutableService *queueService = [[GMBTQueuedMutableService alloc] initWithAsyncId:@(asyncId) service:service];
    
    [self queueEnqueue:_addServiceQueue value:queueService withHandler:^(){ [self handleAddServiceQueue]; }];
        
    return asyncId;
}

- (double) bt_le_server_clear_services {
    
    if (!_isServerOpen) return -1;
    
    [_peripheralManager removeAllServices];
    
    int asyncId = [self generateAsyncId];
    [self notifyAsyncOperationSuccess:@"bt_le_server_clear_services" asyncId:asyncId extraParams:nil];
    
    return asyncId;
}

- (double) bt_le_server_close {
    if (!_isServerOpen) return -1;
    
    _isServerOpen = false;
    [_peripheralManager removeAllServices];
    
    if ([_peripheralManager isAdvertising]) {
        [_peripheralManager stopAdvertising];
    }
    
    int asyncId = [self generateAsyncId];
    [self notifyAsyncOperationSuccess:@"bt_le_server_close" asyncId:asyncId extraParams:nil];
        
    return asyncId;
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
    
    CBATTRequest *request = nil;

    // Retrieve the corresponding CBATTRequest from _writeRequests dictionary
    request = _writeRequestsLookup[requestKey];
    
    if (!request) {
        NSLog(@"Write request with ID %f not found", requestId);
        return 0;
    }
    
    // Respond to the write request
    [_peripheralManager respondToRequest:request withResult:(CBATTError)status];
    
    // Remove the request from the _writeRequests dictionary
    [_writeRequestsLookup removeObjectForKey:requestKey];
    
    return 1; // Indicate success
}

- (double) bt_le_server_notify_value:(NSString*) serviceUuid characteristicUuid:(NSString*) characteristicUuid value:(NSString*) value {

    // Decode the base64 value
    NSData *dataValue = [[NSData alloc] initWithBase64EncodedString:value options:0];
    if (!dataValue) {
        NSLog(@"Failed to decode base64 value");
        return -1;
    }

    // Retrieve the service from the addedServices dictionary (thread safe)
    CBMutableService *service = _addedServices[serviceUuid];
    if (!service) {
        NSLog(@"Service not found");
        return -1;
    }
    
    // Find the target characteristic
    CBMutableCharacteristic *characteristic = nil;
    for (CBMutableCharacteristic *charac in service.characteristics) {
        if ([charac.UUID.UUIDString isEqualToString:characteristicUuid]) {
            characteristic = charac;
            break;
        }
    }

    if (!characteristic) {
        NSLog(@"Characteristic not found");
        return -1;
    }

    // Notify the subscribed centrals
    BOOL success = [_peripheralManager updateValue:dataValue forCharacteristic:characteristic onSubscribedCentrals:nil];
    if (!success) {
        NSLog(@"Failed to notify subscribed centrals");
        return -1;
    }
    
    return 0;
}

- (void) peripheralManager:(CBPeripheralManager *)peripheral didAddService:(CBService *)service error:(NSError *)error {
    GMBTQueuedMutableService *queuedService = _addServiceQueue.firstObject;
    [_addServiceQueue removeObjectAtIndex:0];
    
    NSString *functionName = @"bt_le_server_add_service";
    int asyncId = [queuedService.asyncId intValue];
    
    NSMutableDictionary *params = [NSMutableDictionary dictionary];
    params[@"service"] = [queuedService.service.UUID UUIDString];
    
    if (error) [self notifyAsyncOperationError:functionName asyncId:asyncId errorCode:(int)error.code extraParams:params];
    else {
        _addedServices[[service.UUID UUIDString]] = queuedService.service;
        [self notifyAsyncOperationSuccess:functionName asyncId:asyncId extraParams:params];
    }
    
    [self handleAddServiceQueue];
}

- (void) peripheralManagerIsReadyToUpdateSubscribers:(CBPeripheralManager *)peripheral {
    [self notifyAsyncOperationError: @"bt_le_server_notify_value" asyncId:0 errorCode:-1 extraParams:nil];
}

- (void) peripheralManagerDidUpdateState:(CBPeripheralManager *)peripheral {
    [self notifyOperation:@"bt_le_peripheral_manager_update_state"
              extraParams:@{ @"success": @((int)peripheral.state) }];
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

- (void) peripheralManager:(CBPeripheralManager *)peripheral central:(CBCentral *)central didSubscribeToCharacteristic:(CBCharacteristic *)characteristic {
    
    NSMutableDictionary *params = [NSMutableDictionary dictionary];
    params[@"success"] = @(true);
    params[@"connected"] = @(true);
    params[@"device"] = [self createJSONFromCentral: central];
    [self notifyOperation:@"bt_le_server_connection_state_changed" extraParams: params];
}

- (void) peripheralManager:(CBPeripheralManager *)peripheral central:(CBCentral *)central didUnsubscribeFromCharacteristic:(CBCharacteristic *)characteristic {
    
    NSMutableDictionary *params = [NSMutableDictionary dictionary];
    params[@"success"] = @(true);
    params[@"connected"] = @(false);
    params[@"device"] = [self createJSONFromCentral: central];
    [self notifyOperation:@"bt_le_server_connection_state_changed" extraParams: params];
}

- (void) peripheralManager:(CBPeripheralManager *)peripheral didReceiveReadRequest:(CBATTRequest *)request {
    NSMutableDictionary *params = [NSMutableDictionary dictionary];
    BOOL isDescriptorRequest = NO;
    
    int requestId = [self generateRequestId];
    
    // Storing the CBATTRequest object with the requestId for future use
    _readRequestsLookup[@(requestId)] = request;
    
    // Creating params dictionary
    params[@"request_id"] = @(requestId);
    params[@"service_uuid"] = request.characteristic.service.UUID.UUIDString;
    params[@"characteristic_uuid"] = request.characteristic.UUID.UUIDString;

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
    for (CBATTRequest *request in requests) {
        NSMutableDictionary *params = [NSMutableDictionary dictionary];
        BOOL isDescriptorRequest = NO;

        int requestId = [self generateRequestId];

        // Storing the CBATTRequest object with the requestId for future use
        _writeRequestsLookup[@(requestId)] = request;

        // Creating params dictionary
        params[@"request_id"] = @(requestId);
        params[@"service_uuid"] = request.characteristic.service.UUID.UUIDString;
        params[@"characteristic_uuid"] = request.characteristic.UUID.UUIDString;

        if (request.value) {
            NSString *base64Value = [request.value base64EncodedStringWithOptions:0];
            params[@"value"] = base64Value;
        }

        // Loop through the characteristic's descriptors to check UUIDs
        for (CBDescriptor *descriptor in request.characteristic.descriptors) {
            // Assuming you store descriptor's UUID when it's read
            if ([descriptor.UUID isEqual:request.characteristic.UUID]) {
                params[@"descriptor_uuid"] = descriptor.UUID.UUIDString;
                isDescriptorRequest = YES;
                break;
            }
        }

        NSString* eventType = @"bt_le_server_characteristic_write_request";
        if (isDescriptorRequest) {
            eventType = @"bt_le_server_descriptor_write_request";
        }
        
        [self notifyOperation:eventType extraParams:params];
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

- (void) handleFetchServicesQueue {
    [self handleQueue:_fetchServicesQueue withBlock:^(GMBTQueuedPeripheral *queuedPeripheral) {
        [queuedPeripheral.peripheral discoverServices: nil];
    }];
}

- (void) handleFetchCharacteristicsQueue {
    [self handleQueue:_fetchCharacteristicsQueue withBlock:^(GMBTQueuedService *queueService) {
        [queueService.peripheral discoverCharacteristics:nil forService:queueService.service];
    }];
}

- (void) handleFetchDescriptorsQueue {
    [self handleQueue:_fetchDescriptorsQueue withBlock:^(GMBTQueuedCharacteristic *queuedCharacteristic) {
        [queuedCharacteristic.peripheral discoverDescriptorsForCharacteristic: queuedCharacteristic.characteristic];
    }];
}

- (void) handleReadCharacteristicQueue {
    [self handleQueue:_readCharacteristicQueue withBlock:^(GMBTQueuedCharacteristic *queuedCharacteristic) {
        [queuedCharacteristic.peripheral readValueForCharacteristic: queuedCharacteristic.characteristic];
    }];
}

- (void) handleWriteCharacteristicQueue {
    [self handleQueue:_writeCharacteristicQueue withBlock:^(GMBTQueuedCharacteristicWithData *queuedCharacteristicData) {
        [queuedCharacteristicData.peripheral writeValue:queuedCharacteristicData.data forCharacteristic:queuedCharacteristicData.characteristic type: CBCharacteristicWriteWithResponse];
    }];
}

- (void) handleNotifyCharacteristicQueue {
    [self handleQueue:_notifyCharacteristicQueue withBlock:^(GMBTQueuedCharacteristicWithData *queuedCharacteristicData) {
        BOOL enable = [queuedCharacteristicData.data isEqualToData: KCharacteristicUnsubscribe] ? false : true;
        [queuedCharacteristicData.peripheral setNotifyValue:enable forCharacteristic:queuedCharacteristicData.characteristic];
    }];
}

- (void) handleReadDescriptorQueue {
    [self handleQueue:_readCharacteristicQueue withBlock:^(GMBTQueuedDescriptor *queuedDescriptor) {
        [queuedDescriptor.peripheral readValueForDescriptor: queuedDescriptor.descriptor];
    }];
}

- (void) handleWriteDescriptorQueue {
    [self handleQueue:_writeCharacteristicQueue withBlock:^(GMBTQueuedDescriptorWithData *queuedDescriptorWithData) {
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
        
    int asyncId = [self generateAsyncId];
    GMBTQueuedTimedPeripheral *queuePeripheral = [[GMBTQueuedTimedPeripheral alloc] initWithAsyncId:@(asyncId) peripheral:peripheral];
    
    [self queueEnqueue:_openPeripheralQueue value:queuePeripheral withHandler:^{ [self handleOpenPeripheralQueue]; }];
    
    return asyncId;
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
    
    return 1.0;
}

- (double) bt_le_peripheral_get_services:(NSString*) peripheralUuid {
    CBPeripheral *peripheral = [self peripheralForUuid:peripheralUuid];
    if (!peripheral) return -1;
    
    int asyncId = [self generateAsyncId];
    GMBTQueuedPeripheral *queuePeripheral = [[GMBTQueuedPeripheral alloc] initWithAsyncId:@(asyncId) peripheral:peripheral];

    [self queueEnqueue:_fetchServicesQueue value:queuePeripheral withHandler:^{ [self handleFetchServicesQueue]; }];
    
    return asyncId;
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

- (double) bt_le_service_get_characteristics:(NSString*) peripheralUuid service:(NSString*) serviceUuid {
    CBPeripheral *peripheral = [self peripheralForUuid:peripheralUuid];
    if (!peripheral) return -1;
    
    // Get service with matching UUID
    CBService *service = [self findServiceInPeripheral:peripheral withUUID:serviceUuid];
    if (!service) return -1;

    int asyncId = [self generateAsyncId];
    GMBTQueuedService *queuedService = [[GMBTQueuedService alloc] initWithAsyncId:@(asyncId) peripheral:peripheral service:service];

    [self queueEnqueue:_fetchCharacteristicsQueue value:queuedService withHandler:^{ [self handleFetchCharacteristicsQueue]; }];
    
    return asyncId;
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

- (double) bt_le_characteristic_get_descriptors:(NSString*) peripheralUuid service:(NSString*) serviceUuid characteristic:(NSString*) characteristicUuid {
    
    CBPeripheral *peripheral = [self peripheralForUuid:peripheralUuid];
    if (!peripheral) return -1;
    
    // Get service with matching UUID
    CBService *service = [self findServiceInPeripheral:peripheral withUUID:serviceUuid];
    if (!service) return -1;
    

    // Get service with matching UUID
    CBCharacteristic *characteristic = [self findCharacteristicInService:service withUUID:characteristicUuid];
    if (!characteristic) return -1;
    
    
    int asyncId = [self generateAsyncId];
    GMBTQueuedCharacteristic *queuedCharacteristic = [[GMBTQueuedCharacteristic alloc] initWithAsyncId:@(asyncId) peripheral:peripheral characteristic:characteristic];

    [self queueEnqueue:_fetchDescriptorsQueue value:queuedCharacteristic withHandler:^{ [self handleFetchDescriptorsQueue]; }];
    
    return asyncId;
}

- (double) bt_le_characteristic_read:(NSString*) peripheralUuid service:(NSString*) serviceUuid characteristic:(NSString*) characteristicUuid {
    
    CBPeripheral *peripheral = [self peripheralForUuid:peripheralUuid];
    if (!peripheral) return -1;
    
    // Get service with matching UUID
    CBService *service = [self findServiceInPeripheral:peripheral withUUID:serviceUuid];
    if (!service) return -1;

    // Get service with matching UUID
    CBCharacteristic *characteristic = [self findCharacteristicInService:service withUUID:characteristicUuid];
    if (!characteristic) return -1;
    
    int asyncId = [self generateAsyncId];
    GMBTQueuedCharacteristic *queuedCharacteristic = [[GMBTQueuedCharacteristic alloc] initWithAsyncId:@(asyncId) peripheral:peripheral characteristic:characteristic];

    [self queueEnqueue:_readCharacteristicQueue value:queuedCharacteristic withHandler:^{ [self handleReadCharacteristicQueue]; }];
    
    return asyncId;
}

- (double) bt_le_characteristic_write_request:(NSString*) peripheralUuid service:(NSString*) serviceUuid characteristic:(NSString*) characteristicUuid value:(NSString*) value {
    
    CBPeripheral *peripheral = [self peripheralForUuid:peripheralUuid];
    if (!peripheral) return -1;
    
    CBService *service = [self findServiceInPeripheral:peripheral withUUID:serviceUuid];
    if (!service) return -1;

    CBCharacteristic *characteristic = [self findCharacteristicInService:service withUUID:characteristicUuid];
    if (!characteristic) return -1;
    
    NSData *data = [self dataFromBase64:value];
    if (!data) return -1;
    
    int asyncId = [self generateAsyncId];
    GMBTQueuedCharacteristicWithData *queuedCharacteristicData = [[GMBTQueuedCharacteristicWithData alloc] initWithAsyncId:@(asyncId) peripheral:peripheral characteristic:characteristic data:data];

    [self queueEnqueue:_writeCharacteristicQueue value:queuedCharacteristicData withHandler:^{ [self handleWriteCharacteristicQueue]; }];
    
    return asyncId;
}

- (double) bt_le_characteristic_write_command:(NSString*) peripheralUuid service:(NSString*) serviceUuid characteristic:(NSString*) characteristicUuid value:(NSString*) value {
    
    CBPeripheral *peripheral = [self peripheralForUuid:peripheralUuid];
    if (!peripheral) return -1;
    
    CBService *service = [self findServiceInPeripheral:peripheral withUUID:serviceUuid];
    if (!service) return -1;

    CBCharacteristic *characteristic = [self findCharacteristicInService:service withUUID:characteristicUuid];
    if (!characteristic) return -1;
    
    NSData *data = [self dataFromBase64:value];
    if (!data) return -1;
    
    [peripheral writeValue:data forCharacteristic:characteristic type:CBCharacteristicWriteWithoutResponse];
    
    int asyncId = [self generateAsyncId];
    [self notifyAsyncOperationSuccess:@"bt_le_characteristic_write_command" asyncId:asyncId extraParams:nil];

    return asyncId;
}

- (double) manageCharacteristicSubscriptionForPeripheral:(NSString*) peripheralUuid service:(NSString*) serviceUuid characteristicUuid:(NSString*) characteristicUuid type:(NSData*)type function:(const char *)functionName {
    CBPeripheral *peripheral = [self peripheralForUuid:peripheralUuid];
    if (!peripheral) return -1;
    
    CBService *service = [self findServiceInPeripheral:peripheral withUUID:serviceUuid];
    if (!service) return -1;
    
    CBCharacteristic *characteristic = [self findCharacteristicInService:service withUUID:characteristicUuid];
    if (!characteristic) return -1;
    
    int asyncId = [self generateAsyncId];
    GMBTQueuedCharacteristicWithData *queuedCharacteristicData = [[GMBTQueuedCharacteristicWithData alloc] initWithAsyncId:@(asyncId) peripheral:peripheral characteristic:characteristic data:type];
    
    [self queueEnqueue:_notifyCharacteristicQueue value:queuedCharacteristicData withHandler:^{ [self handleNotifyCharacteristicQueue]; }];
        
    return asyncId;
}

- (double) bt_le_characteristic_notify:(NSString*) peripheralUuid service:(NSString*) serviceUuid characteristic:(NSString*) characteristicUuid {
    
    return [self manageCharacteristicSubscriptionForPeripheral:peripheralUuid service:serviceUuid characteristicUuid:characteristicUuid type: KCharacteristicNotify function:"bt_le_characteristic_notify"];
}

- (double) bt_le_characteristic_indicate:(NSString*) peripheralUuid service:(NSString*) serviceUuid characteristic:(NSString*) characteristicUuid {
    
    return [self manageCharacteristicSubscriptionForPeripheral:peripheralUuid service:serviceUuid characteristicUuid:characteristicUuid type: KCharacteristicIndicate function:"bt_le_characteristic_indicate"];
}

- (double) bt_le_characteristic_unsubscribe:(NSString*) peripheralUuid service:(NSString*) serviceUuid characteristic:(NSString*) characteristicUuid {
    
    return [self manageCharacteristicSubscriptionForPeripheral:peripheralUuid service:serviceUuid characteristicUuid:characteristicUuid type:KCharacteristicUnsubscribe function:"bt_le_characteristic_unsubscribe"];
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

- (double) bt_le_descriptor_read:(NSString*) peripheralUuid service:(NSString*) serviceUuid characteristic:(NSString*) characteristicUuid descriptor:(NSString*) descriptorUuid {
    
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
    
    int asyncId = [self generateAsyncId];
    GMBTQueuedDescriptor *queuedDescriptor = [[GMBTQueuedDescriptor alloc] initWithAsyncId:@(asyncId) peripheral:peripheral descriptor:descriptor];

    [self queueEnqueue:_readDescriptorQueue value:queuedDescriptor withHandler:^{ [self handleReadDescriptorQueue]; }];
    
    return asyncId;
}

- (double) bt_le_descriptor_write:(NSString*) peripheralUuid service:(NSString*) serviceUuid characteristic:(NSString*) characteristicUuid descriptor:(NSString*) descriptorUuid value:(NSString*) value {
    
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
    
    int asyncId = [self generateAsyncId];
    GMBTQueuedDescriptorWithData *queuedDescriptorWithData = [[GMBTQueuedDescriptorWithData alloc] initWithAsyncId:@(asyncId) peripheral:peripheral descriptor:descriptor data:data];

    [self queueEnqueue:_writeDescriptorQueue value:queuedDescriptorWithData withHandler:^{ [self handleWriteDescriptorQueue]; }];
    
    return asyncId;
}

- (void) centralManager:(CBCentralManager *)central didConnectPeripheral:(CBPeripheral *)peripheral {
	
    GMBTQueuedTimedPeripheral *queuedPeripheral = [self queueDequeue:_openPeripheralQueue];
    
    [queuedPeripheral.timer invalidate];
    
    peripheral.delegate = self;
	
    [_openedPeripherals setObject:peripheral forKey:[peripheral.identifier UUIDString]];
    [_connectedPeripherals setObject:peripheral forKey:[peripheral.identifier UUIDString]];
    
    int asyncId = [queuedPeripheral.asyncId intValue];
    
    NSMutableDictionary* params = [[NSMutableDictionary alloc] init];
    params[@"name"] = peripheral.name ?: @"";
    params[@"address"] = peripheral.identifier.UUIDString;
    
    [self notifyAsyncOperationSuccess:@"bt_le_peripheral_open" asyncId:asyncId extraParams:params];
    [self handleOpenPeripheralQueue];
}

- (void) centralManager:(CBCentralManager *)central didFailToConnectPeripheral:(CBPeripheral *)peripheral error:(NSError *)error {
    
    GMBTQueuedTimedPeripheral *queuedPeripheral = [self queueDequeue:_openPeripheralQueue];
    
    [queuedPeripheral.timer invalidate];
    
    int asyncId = [queuedPeripheral.asyncId intValue];
    
    NSMutableDictionary* params = [[NSMutableDictionary alloc] init];
    params[@"name"] = peripheral.name ?: @"";
    params[@"address"] = peripheral.identifier.UUIDString;
    
    [self notifyAsyncOperationError:@"bt_le_peripheral_open" asyncId:asyncId errorCode:(int)error.code extraParams:params];
    [self handleOpenPeripheralQueue];
}

- (void) connectionDidTimeout {
    GMBTQueuedPeripheral *queuedPeripheral = [self queueDequeue:_openPeripheralQueue];
    
    [_centralManager cancelPeripheralConnection: queuedPeripheral.peripheral];
    
    int asyncId = [queuedPeripheral.asyncId intValue];
    
    NSMutableDictionary* params = [[NSMutableDictionary alloc] init];
    params[@"name"] = queuedPeripheral.peripheral.name ?: @"";
    params[@"address"] = queuedPeripheral.peripheral.identifier.UUIDString;
    
    [self notifyAsyncOperationError:@"bt_le_peripheral_open" asyncId:asyncId errorCode:(int)133 extraParams:params];
    [self handleOpenPeripheralQueue];
}

- (void) centralManager:(CBCentralManager *)central didDisconnectPeripheral:(CBPeripheral *)peripheral error:(NSError *)error {
    if (error) {
        NSMutableDictionary* params = [[NSMutableDictionary alloc] init];
        params[@"error_code"] = @((int)error.code);
        [self notifyOperation:@"bt_le_peripheral_disconnect" extraParams:params];
        return;
    }
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

- (NSMutableDictionary *) dictionaryFromService:(CBService *)service {
    NSMutableDictionary *serviceDictionary = [NSMutableDictionary new];
        
    serviceDictionary[@"uuid"] = [[self convertTo128BitUUID: service.UUID.UUIDString] uppercaseString];
    
    return serviceDictionary;
}

- (NSString *) createJSONFromServiceArray:(NSArray<CBService *> *)services {
    NSMutableArray *serviceArray = [NSMutableArray new];
    
    for (CBService *service in services) {
        [serviceArray addObject:[self dictionaryFromService: service]];
    }
    
    NSError *error = nil;
    NSData *jsonData = [NSJSONSerialization dataWithJSONObject:serviceArray options:0 error:&error];
    
    if (error) {
        NSLog(@"Error creating JSON: %@", error.localizedDescription);
        return nil;
    }
    
    return [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];
}

- (void) peripheral:(CBPeripheral *)peripheral didDiscoverServices:(NSError *)error {
	
    GMBTQueuedPeripheral *queuedPeripheral = [self queueDequeue:_fetchServicesQueue];
    	
    NSString* type = @"bt_le_peripheral_get_services";
    int asyncId = [queuedPeripheral.asyncId intValue];
    
    if (error) {
        [self notifyAsyncOperationError:type asyncId:asyncId errorCode:(int)error.code extraParams:nil];
    }
    else {
        NSMutableDictionary *params = [[NSMutableDictionary alloc] init];
        params[@"services"] = [self createJSONFromServiceArray:peripheral.services];

        [self notifyAsyncOperationSuccess:type asyncId:asyncId extraParams:params];
    }
    [self handleFetchServicesQueue];
}
 
- (void) peripheral:(CBPeripheral *)peripheral didDiscoverIncludedServicesForService:(CBService *)service error:(NSError *)error {
    // This won't be handled
}

- (NSMutableDictionary *) dictionaryFromCharacteristic:(CBCharacteristic *)characteristic {
    NSMutableDictionary *charDict = [NSMutableDictionary new];
    [charDict setObject:[characteristic.UUID.UUIDString uppercaseString] forKey:@"uuid"];
    [charDict setObject:@(characteristic.properties) forKey:@"properties"];
    return charDict;
}

- (NSString *) createJSONFromCharacteristicsArray:(NSArray<CBCharacteristic *> *)characteristics {
    NSMutableArray *characteristicObjects = [NSMutableArray new];
    
    for (CBCharacteristic *characteristic in characteristics) {
        [characteristicObjects addObject:[self dictionaryFromCharacteristic:characteristic]];
    }
    
    NSError *error = nil;
    NSData *jsonData = [NSJSONSerialization dataWithJSONObject:characteristicObjects options:0 error:&error];
    
    if (error) {
        NSLog(@"Error creating JSON: %@", error.localizedDescription);
        return nil;
    }
    
    return [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];
}

- (void) peripheral:(CBPeripheral *)peripheral didDiscoverCharacteristicsForService:(CBService *)service error:(NSError *)error {
    
    GMBTQueuedService *queuedService = [self queueDequeue: _fetchCharacteristicsQueue];
    
    NSString* type = @"bt_le_service_get_characteristics";
    int asyncId = [queuedService.asyncId intValue];
    
    if (error) {
        [self notifyAsyncOperationError:type asyncId:asyncId errorCode:(int)error.code extraParams:nil];
    }
    else {
        NSMutableDictionary *params = [[NSMutableDictionary alloc] init];
        params[@"characteristics"] = [self createJSONFromCharacteristicsArray:service.characteristics];
        
        [self notifyAsyncOperationSuccess:type asyncId:asyncId extraParams:params];
    }
    
    // Handle next task in queue if there is one
    [self handleFetchCharacteristicsQueue];
}
 
- (NSMutableDictionary *) dictionaryFromDescriptor:(CBDescriptor *)descriptor {
    NSMutableDictionary *descriptorDictionary = [NSMutableDictionary new];
        
    descriptorDictionary[@"uuid"] = [[self convertTo128BitUUID: descriptor.UUID.UUIDString] uppercaseString];
    
    return descriptorDictionary;
}

- (NSString *) createJSONFromDescriptorsArray:(NSArray<CBDescriptor *> *)descriptors {
    NSMutableArray *descriptorArray = [NSMutableArray new];

    for (CBDescriptor *descriptor in descriptors) {
        [descriptorArray addObject: [self dictionaryFromDescriptor:descriptor]];
    }

    NSError *error = nil;
    NSData *jsonData = [NSJSONSerialization dataWithJSONObject:descriptorArray options:0 error:&error];

    if (error) {
        NSLog(@"Failed to create JSON: %@", error.localizedDescription);
        return nil;
    }

    return [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];
}

- (void) peripheral:(CBPeripheral *)peripheral didDiscoverDescriptorsForCharacteristic:(CBCharacteristic *)characteristic error:(NSError *)error {
	
    GMBTQueuedCharacteristic *queuedCharacteristic = [self queueDequeue:_fetchDescriptorsQueue];
    
    NSString* type = @"bt_le_characteristic_get_descriptors";
    int asyncId = [queuedCharacteristic.asyncId intValue];
    
    if (error) {
        [self notifyAsyncOperationError:type asyncId:asyncId errorCode:(int)error.code extraParams:nil];
    }
    else {
        NSMutableDictionary *params = [[NSMutableDictionary alloc] init];
        params[@"descriptors"] = [self createJSONFromDescriptorsArray:characteristic.descriptors];
        
        [self notifyAsyncOperationSuccess:type asyncId:asyncId extraParams:params];
    }
    [self handleFetchDescriptorsQueue];
}

- (void) peripheral:(CBPeripheral *)peripheral didUpdateNotificationStateForCharacteristic:(CBCharacteristic *)characteristic error:(NSError *)error {
    
    GMBTQueuedCharacteristicWithData *queuedCharacteristicWithData = [self queueDequeue:_notifyCharacteristicQueue];
    
    int asyncId = [queuedCharacteristicWithData.asyncId intValue];
    NSData* data = queuedCharacteristicWithData.data;
    
    if ([data isEqualToData: KCharacteristicUnsubscribe]) {
        NSString* type = @"bt_le_characteristic_unsubscribe";
        
        if (error) [self notifyAsyncOperationError:type asyncId:asyncId errorCode:(int)error.code extraParams:nil];
        else [self notifyAsyncOperationSuccess:type asyncId:asyncId extraParams:nil];
    }
    else if ([data isEqualToData: KCharacteristicNotify]) {
        NSString* type = @"bt_le_characteristic_notify";
        
        if (error) [self notifyAsyncOperationError:type asyncId:asyncId errorCode:(int)error.code extraParams:nil];
        else [self notifyAsyncOperationSuccess:type asyncId:asyncId extraParams:nil];
        
    }
    else if ([data isEqualToData: KCharacteristicIndicate]) {
        NSString* type = @"bt_le_characteristic_indicate";
        
        if (error) [self notifyAsyncOperationError:type asyncId:asyncId errorCode:(int)error.code extraParams:nil];
        else [self notifyAsyncOperationSuccess:type asyncId:asyncId extraParams:nil];
    }
    
    [self handleNotifyCharacteristicQueue];
}

- (void) peripheral:(CBPeripheral *)peripheral didUpdateValueForCharacteristic:(CBCharacteristic *)characteristic error:(NSError *)error {
        
    // Convert the value to a base64 string
    NSString *valueString = [characteristic.value base64EncodedStringWithOptions:0];
    
    GMBTQueuedCharacteristic *queuedCharacteristic = [self queuePeek:_readCharacteristicQueue];
    
    if (queuedCharacteristic.characteristic != characteristic) {
        queuedCharacteristic = nil;
    }
    else [self queueDequeue:_readCharacteristicQueue];
    
    // There was not queued read request that matches the characteristic so it's a notification
    if (queuedCharacteristic == nil) {
        NSMutableDictionary *params = [[NSMutableDictionary alloc] init];
        params[@"characteristic_uuid"] = [characteristic.UUID.UUIDString uppercaseString];
        params[@"service_uuid"] = [characteristic.service.UUID.UUIDString uppercaseString];
        params[@"address"] = [peripheral.identifier.UUIDString uppercaseString];
        params[@"value"] = valueString;
        
        return [self notifyOperation:@"bt_le_characteristic_value_changed" extraParams:params];
    }

    NSString *type = @"bt_le_characteristic_read";
    int asyncId = [queuedCharacteristic.asyncId intValue];
    
    // Check if there was an error
    if (error) [self notifyAsyncOperationError:type asyncId:asyncId errorCode:(int)error.code extraParams:nil];
    else [self notifyAsyncOperationSuccess:type asyncId:asyncId extraParams:@{@"value": valueString}];
    
    [self handleReadCharacteristicQueue];
}

- (void) peripheral:(CBPeripheral *)peripheral didWriteValueForCharacteristic:(CBCharacteristic *)characteristic error:(NSError *)error {
    
    GMBTQueuedCharacteristicWithData *queuedCharacteristicWithData = [self queueDequeue:_writeCharacteristicQueue];
        
    NSString* type = @"bt_le_characteristic_write_request";
    int asyncId = [queuedCharacteristicWithData.asyncId intValue];
    
    if (error) [self notifyAsyncOperationError:type asyncId:asyncId errorCode:(int)error.code extraParams:nil];
    else [self notifyAsyncOperationSuccess:type asyncId:asyncId extraParams:nil];
    
    [self handleWriteCharacteristicQueue];
}

- (void) peripheral:(CBPeripheral *)peripheral didUpdateValueForDescriptor:(CBDescriptor *)descriptor error:(nullable NSError *)error {
    
    GMBTQueuedDescriptor *queuedDescriptor = [self queueDequeue:_readDescriptorQueue];
    
    NSString *type = @"bt_le_descriptor_read";
    int asyncId = [queuedDescriptor.asyncId intValue];
    
    // Check if there was an error
    if (error) [self notifyAsyncOperationError:type asyncId:asyncId errorCode:(int)error.code extraParams:nil];
    else [self notifyAsyncOperationSuccess:type asyncId:asyncId extraParams:@{@"value": [descriptor.value base64EncodedStringWithOptions:0]}];
    
    [self handleReadDescriptorQueue];
}

- (void) peripheral:(CBPeripheral *)peripheral didWriteValueForDescriptor:(CBDescriptor *)descriptor error:(nullable NSError *)error {
    
    GMBTQueuedDescriptorWithData *queuedDescriptorWithData = [self queueDequeue:_writeDescriptorQueue];
    
    NSString *type = @"bt_le_descriptor_write";
    int asyncId = [queuedDescriptorWithData.asyncId intValue];
    
    // Check if there was an error
    if (error) [self notifyAsyncOperationError:type asyncId:asyncId errorCode:(int)error.code extraParams:nil];
    else [self notifyAsyncOperationSuccess:type asyncId:asyncId extraParams:nil];
    
    [self handleWriteDescriptorQueue];
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
            [self beginScanWithAsyncId:_pendingScanAsyncId];
        }
    }
    else if (_scanPendingPowerOn && central.state != CBManagerStateUnknown &&
             central.state != CBManagerStateResetting) {
        // Unsupported / Unauthorized / PoweredOff are terminal for this request -
        // holding the deferral would stall the caller indefinitely.
        _scanPendingPowerOn = false;
        NSLog(@"[GMBluetooth] central reached %@ - the deferred scan cannot start and has been dropped",
              stateString);
        [self notifyAsyncOperationError:@"bt_le_scan_start"
                                asyncId:_pendingScanAsyncId
                              errorCode:(int)central.state
                            extraParams:@{ @"state": stateString }];
    }

    [self notifyOperation:@"bt_le_state_update"
              extraParams:@{ @"success": @((int)central.state), @"state": stateString }];
}

#if TARGET_OS_IOS
- (void) locationManagerDidChangeAuthorization:(CLLocationManager *)manager {
    CLAuthorizationStatus status = [CLLocationManager authorizationStatus];
    NSString *statusString = @"Unknown";
    switch (status) {
        case kCLAuthorizationStatusNotDetermined:
            statusString = @"NotDetermined";
            break;
        case kCLAuthorizationStatusRestricted:
            statusString = @"Restricted";
            break;
        case kCLAuthorizationStatusDenied:
            statusString = @"Denied";
            break;
        case kCLAuthorizationStatusAuthorizedAlways:
            statusString = @"AuthorizedAlways";
            break;
        case kCLAuthorizationStatusAuthorizedWhenInUse:
            statusString = @"AuthorizedWhenInUse";
            break;
    }
    NSLog(@"[GMBluetooth] Location authorization changed: %@ (%d)", statusString, (int)status);
    [self notifyOperation:@"bt_location_auth_changed"
              extraParams:@{ @"status": statusString }];
}
#endif

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
	peripheral.delegate = self;
    [_openedPeripherals setObject:peripheral forKey:peripheral.identifier.UUIDString];
    [self notifyOperation:@"bt_le_peripheral_service_change" extraParams:@{ @"name": (peripheral.name ?: @""), @"address": peripheral.identifier.UUIDString }];
}


@end


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
            [transport_ bt_init];
            GMBT_LOG("Apple transport created and bt_init sent. CoreBluetooth powers on asynchronously - "
                     "watch for 'CBCentralManager state changed: PoweredOn' before expecting a scan to work.");
            message.clear();
            return Error::Ok;
        }

        void shutdown() override
        {
            if (!transport_) return;
            transport_.eventSink = nil;
            [transport_ bt_end];
            transport_ = nil;
            std::scoped_lock lock(mutex_);
            le_connection_to_id_.clear();
            le_id_to_connection_.clear();
        }

        bool supports_ble() const override { return true; }
        bool supports_le_advertise() const override { return true; }
        bool supports_le_server() const override { return true; }
        bool supports_classic() const override { return false; }
        bool supports_classic_server() const override { return false; }

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
        Error le_services_discover(std::uint64_t c,std::string& m) override { auto id=id_for_connection(c); if(id.empty())return invalid_connection(m); return async_result([transport_ bt_le_peripheral_get_services:to_ns(id)],"Service discovery could not start",m); }
        Error le_characteristics_discover(std::uint64_t c,const std::string&s,std::string&m) override { auto id=id_for_connection(c); if(id.empty())return invalid_connection(m); return async_result([transport_ bt_le_service_get_characteristics:to_ns(id) service:to_ns(s)],"Characteristic discovery could not start",m); }
        Error le_descriptors_discover(std::uint64_t c,const std::string&s,const std::string&ch,std::string&m) override { auto id=id_for_connection(c); if(id.empty())return invalid_connection(m); return async_result([transport_ bt_le_characteristic_get_descriptors:to_ns(id) service:to_ns(s) characteristic:to_ns(ch)],"Descriptor discovery could not start",m); }
        Error le_characteristic_read(std::uint64_t c,const std::string&s,const std::string&ch,std::string&m) override { auto id=id_for_connection(c); if(id.empty())return invalid_connection(m); return async_result([transport_ bt_le_characteristic_read:to_ns(id) service:to_ns(s) characteristic:to_ns(ch)],"Characteristic read could not start",m); }
        Error le_characteristic_write(std::uint64_t c,const std::string&s,const std::string&ch,const std::string&v,bool with_response,std::string&m) override
        { auto id=id_for_connection(c); if(id.empty())return invalid_connection(m); double r=with_response?[transport_ bt_le_characteristic_write_request:to_ns(id) service:to_ns(s) characteristic:to_ns(ch) value:to_ns(v)]:[transport_ bt_le_characteristic_write_command:to_ns(id) service:to_ns(s) characteristic:to_ns(ch) value:to_ns(v)]; return async_result(r,"Characteristic write could not start",m); }
        Error le_characteristic_subscribe(std::uint64_t c,const std::string&s,const std::string&ch,std::int32_t mode,std::string&m) override
        { auto id=id_for_connection(c); if(id.empty())return invalid_connection(m); double r=mode==0?[transport_ bt_le_characteristic_unsubscribe:to_ns(id) service:to_ns(s) characteristic:to_ns(ch)]:mode==2?[transport_ bt_le_characteristic_indicate:to_ns(id) service:to_ns(s) characteristic:to_ns(ch)]:[transport_ bt_le_characteristic_notify:to_ns(id) service:to_ns(s) characteristic:to_ns(ch)]; return async_result(r,"Characteristic subscription could not start",m); }
        Error le_descriptor_read(std::uint64_t c,const std::string&s,const std::string&ch,const std::string&d,std::string&m) override { auto id=id_for_connection(c); if(id.empty())return invalid_connection(m); return async_result([transport_ bt_le_descriptor_read:to_ns(id) service:to_ns(s) characteristic:to_ns(ch) descriptor:to_ns(d)],"Descriptor read could not start",m); }
        Error le_descriptor_write(std::uint64_t c,const std::string&s,const std::string&ch,const std::string&d,const std::string&v,std::string&m) override { auto id=id_for_connection(c); if(id.empty())return invalid_connection(m); return async_result([transport_ bt_le_descriptor_write:to_ns(id) service:to_ns(s) characteristic:to_ns(ch) descriptor:to_ns(d) value:to_ns(v)],"Descriptor write could not start",m); }

        Error le_advertise_start(const std::string&s,const std::string&d,std::string&m) override { return async_result([transport_ bt_le_advertise_start:to_ns(s) data:to_ns(d)],"BLE advertising could not start",m); }
        Error le_advertise_stop(std::string&m) override { return async_result([transport_ bt_le_advertise_stop],"BLE advertising could not stop",m); }
        bool le_advertise_is_running() const override { return _isAdvertising; }
        Error le_server_start(std::string&m) override { return async_result([transport_ bt_le_server_open],"GATT server could not start",m); }
        Error le_server_stop(std::string&m) override { return async_result([transport_ bt_le_server_close],"GATT server could not stop",m); }
        bool le_server_is_running() const override { return _isServerOpen; }
        Error le_server_add_service(const std::string&j,std::string&m) override { return async_result([transport_ bt_le_server_add_service:to_ns(j)],"GATT service could not be added",m); }
        Error le_server_clear_services(std::string&m) override { return async_result([transport_ bt_le_server_clear_services],"GATT services could not be cleared",m); }
        Error le_server_respond_read(std::int32_t r,std::int32_t st,const std::string&v,std::string&m) override { double x=[transport_ bt_le_server_respond_read:r status:st value:to_ns(v)]; if(x>0){m.clear();return Error::Ok;}m="GATT read response failed";return Error::OperationFailed; }
        Error le_server_respond_write(std::int32_t r,std::int32_t st,std::string&m) override { double x=[transport_ bt_le_server_respond_write:r status:st]; if(x>0){m.clear();return Error::Ok;}m="GATT write response failed";return Error::OperationFailed; }
        Error le_server_notify_value(const std::string&s,const std::string&ch,const std::string&v,std::string&m) override { return async_result([transport_ bt_le_server_notify_value:to_ns(s) characteristicUuid:to_ns(ch) value:to_ns(v)],"GATT notification could not start",m); }

        Error classic_scan_start(std::string&m) override {m="Bluetooth Classic is not implemented by the old Apple transport";return Error::NotSupported;}
        Error classic_scan_stop(std::string&m) override {m.clear();return Error::Ok;}
        bool classic_scan_is_running() const override {return false;}
        Error classic_connect(std::uint64_t,const DiscoveredDevice&,const std::string&,std::string&m) override {m="Bluetooth Classic is not implemented by the old Apple transport";return Error::NotSupported;}
        Error classic_disconnect(std::uint64_t,std::string&m) override {m="Bluetooth Classic is not implemented by the old Apple transport";return Error::NotSupported;}
        bool classic_connection_is_connected(std::uint64_t) const override {return false;}
        std::int32_t classic_receive_available(std::uint64_t) const override {return 0;}
        Error classic_send_bytes(std::uint64_t,const std::uint8_t*,std::size_t,std::string&m) override {m="Bluetooth Classic is not implemented by the old Apple transport";return Error::NotSupported;}
        std::size_t classic_receive_bytes(std::uint64_t,std::uint8_t*,std::size_t) override {return 0;}
        Error classic_server_start(const std::string&,const std::string&,std::string&m) override {m="Bluetooth Classic server is not implemented by the old Apple transport";return Error::NotSupported;}
        Error classic_server_stop(std::string&m) override {m.clear();return Error::Ok;}
        bool classic_server_is_running() const override {return false;}

    private:
        CoreHooks hooks_;
        GMBluetoothAppleTransport* transport_ = nil;
        mutable std::mutex mutex_;
        std::unordered_map<std::uint64_t,std::string> le_connection_to_id_;
        std::unordered_map<std::string,std::uint64_t> le_id_to_connection_;

        static Error async_result(double value,const char* failure,std::string&message)
        { if(value<0){message=failure;return Error::OperationFailed;}message.clear();return Error::Ok; }
        static Error invalid_connection(std::string&message){message="Invalid BLE connection handle";return Error::InvalidHandle;}

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

        void on_event(NSString* type, NSDictionary* params)
        {
            if (!type)
            {
                GMBT_LOG("transport raised an event with no type - ignored");
                return;
            }
            GMBT_LOG("transport event '%s' -> normalized '%s'",
                to_string(type).c_str(), normalized_event_type(type).c_str());

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
