#include "TargetConditionals.h"
#import "Bluetooth.h"

#if TARGET_OS_IOS
#import <UIKit/UIKit.h>
#endif

#if TARGET_OS_OSX
#import "Extension_Interface.h"
#include "YYRValue.h"
#include <sstream>
#endif

const int EVENT_OTHER_SOCIAL = 70;

#if TARGET_OS_OSX
YYRunnerInterface gs_runnerInterface;
YYRunnerInterface* g_pYYRunnerInterface;

extern "C" void PreGraphicsInitialisation(char* arg1) { }

YYEXPORT void YYExtensionInitialise(const struct YYRunnerInterface* _pFunctions, size_t _functions_size) {
    //copy out all the functions
    memcpy(&gs_runnerInterface, _pFunctions, sizeof(YYRunnerInterface));
    g_pYYRunnerInterface = &gs_runnerInterface;

    if (_functions_size < sizeof(YYRunnerInterface)) {
        DebugConsoleOutput("ERROR : runner interface mismatch in extension DLL\n ");
    } // end if

    DebugConsoleOutput("YYExtensionInitialise CONFIGURED \n ");
}

#else
#define YYEXPORT /**/
extern "C" void dsMapClear(int _dsMap );
extern "C" int dsMapCreate();
extern "C" void dsMapAddInt(int _dsMap, char* _key, int _value);

extern "C" int dsListCreate();
extern "C" void dsListAddInt(int _dsList, int _value);
extern "C" void dsListAddString(int _dsList, char* _value);
extern "C" const char* dsListGetValueString(int _dsList, int _listIdx);
extern "C" double dsListGetValueDouble(int _dsList, int _listIdx);
extern "C" int dsListGetSize(int _dsList);

extern int CreateDsMap( int _num, ... );
extern void CreateAsynEventWithDSMap(int dsmapindex, int event_index);
extern "C" void dsMapAddDouble(int _dsMap, const char* _key, double _value);
extern "C" void dsMapAddString(int _dsMap, const char* _key, const char* _value);
#endif

@implementation QueuedMutableDictionary
- (instancetype)initWithAsyncId:(NSNumber *)asyncId dictionary:(NSMutableDictionary *)dictionary {
    self = [super init];
    if (self) {
        _asyncId = asyncId;
        _dictionary = dictionary;
    }
    return self;
}
@end

@implementation QueuedMutableService
- (instancetype)initWithAsyncId:(NSNumber *)asyncId service:(CBMutableService *)service {
    self = [super init];
    if (self) {
        _asyncId = asyncId;
        _service = service;
    }
    return self;
}
@end

@implementation QueuedPeripheral
- (instancetype)initWithAsyncId:(NSNumber *)asyncId peripheral:(CBPeripheral *)peripheral {
    self = [super init];
    if (self) {
        _asyncId = asyncId;
        _peripheral = peripheral;
    }
    return self;
}
@end

@implementation QueuedTimedPeripheral
- (instancetype)initWithAsyncId:(NSNumber *)asyncId peripheral:(CBPeripheral *)peripheral timer:(NSTimer *)timer {
    self = [super initWithAsyncId:asyncId peripheral:peripheral];
    if (self) {
        _timer = timer;
    }
    return self;
}
@end

@implementation QueuedService
- (instancetype)initWithAsyncId:(NSNumber *)asyncId peripheral:(CBPeripheral *)peripheral service:(CBService *) service {
    self = [super initWithAsyncId:asyncId peripheral:peripheral];
    if (self) {
        _service = service;
    }
    return self;
}
@end

@implementation QueuedCharacteristic
- (instancetype)initWithAsyncId:(NSNumber *)asyncId peripheral:(CBPeripheral *)peripheral characteristic:(CBCharacteristic *) characteristic {
    self = [super initWithAsyncId:asyncId peripheral:peripheral];
    if (self) {
        _characteristic = characteristic;
    }
    return self;
}
@end

@implementation QueuedCharacteristicWithData
- (instancetype)initWithAsyncId:(NSNumber *)asyncId peripheral:(CBPeripheral *)peripheral characteristic:(CBCharacteristic *) characteristic data:(NSData*) data {
    self = [super initWithAsyncId:asyncId peripheral:peripheral characteristic:characteristic];
    if (self) {
        _data = data;
    }
    return self;
}
@end

@implementation QueuedDescriptor
- (instancetype)initWithAsyncId:(NSNumber *)asyncId peripheral:(CBPeripheral *)peripheral descriptor:(CBDescriptor *) descriptor {
    self = [super initWithAsyncId:asyncId peripheral:peripheral];
    if (self) {
        _descriptor = descriptor;
    }
    return self;
}
@end

@implementation QueuedDescriptorWithData
- (instancetype)initWithAsyncId:(NSNumber *)asyncId peripheral:(CBPeripheral *)peripheral descriptor:(CBDescriptor *) descriptor data:(NSData*) data {
    self = [super initWithAsyncId:asyncId peripheral:peripheral descriptor:descriptor];
    if (self) {
        _data = data;
    }
    return self;
}
@end

@implementation Bluetooth

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

int CreateDsMap_comaptibility() {
    
#if TARGET_OS_OSX
    return CreateDsMap(0,0);
#else
    return CreateDsMap(0,0);
#endif
}

void DsMapAddString_comaptibility(int dsMapIndex, const char* _key, const char* _value) {
    
#if TARGET_OS_OSX
    DsMapAddString(dsMapIndex, _key, _value);
#else
    dsMapAddString(dsMapIndex, _key, _value);
#endif
}

void DsMapAddDouble_comaptibility(int dsMapIndex, const char* _key, double _value) {
    
#if TARGET_OS_OSX
    DsMapAddDouble(dsMapIndex, _key, _value);
#else
    dsMapAddDouble(dsMapIndex, _key, _value);
#endif
}

void CreateAsyncEventWithDSMap_comaptibility(int dsMapIndex) {
    
#if TARGET_OS_OSX
    CreateAsyncEventWithDSMap(dsMapIndex,EVENT_OTHER_SOCIAL);
#else
    CreateAsynEventWithDSMap(dsMapIndex,EVENT_OTHER_SOCIAL);
#endif
}

- (void) notifyOperation:(NSString *)functionName extraParams:(NSDictionary *)extraParams {
    int dsMapIndex = CreateDsMap_comaptibility();
    DsMapAddString_comaptibility(dsMapIndex, "type", [functionName UTF8String]);
    
    for (NSString *key in [extraParams allKeys]) {
        id value = extraParams[key];
        
        if ([value isKindOfClass:[NSString class]]) {
            DsMapAddString_comaptibility(dsMapIndex, [key UTF8String], [value UTF8String]);
        } else if ([value isKindOfClass:[NSNumber class]]) {
            DsMapAddDouble_comaptibility(dsMapIndex, [key UTF8String], [value doubleValue]);
        } else if ([value isKindOfClass:[NSData class]]) {
            NSString *base64String = [value base64EncodedStringWithOptions:0];
            DsMapAddString_comaptibility(dsMapIndex, [key UTF8String], [base64String UTF8String]);
        } else if ([value isKindOfClass:[NSUUID class]]) {
            DsMapAddString_comaptibility(dsMapIndex, [key UTF8String], [[value UUIDString] UTF8String]);
        } else if ([value isKindOfClass:[NSArray class]] || [value isKindOfClass:[NSDictionary class]]) {
            NSError *error = nil;
            NSData *jsonData = [NSJSONSerialization dataWithJSONObject:value options:0 error:&error];
            if (!jsonData) {
                NSLog(@"Failed to serialize JSON: %@", error);
                continue;
            }
            NSString *jsonString = [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];
            DsMapAddString_comaptibility(dsMapIndex, [key UTF8String], [jsonString UTF8String]);
        } else {
            NSLog(@"notifyAsyncOperation :: value type not supported for key '%@', ignoring!", key);
        }
    }
    
    CreateAsyncEventWithDSMap_comaptibility(dsMapIndex);
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

- (void) bt_init {
	_centralManager = [[CBCentralManager alloc] initWithDelegate:self queue:nil options:nil];
    _peripheralManager = [[CBPeripheralManager alloc] initWithDelegate: self queue: nil options: nil];
    
    #if TARGET_OS_OSX
    // This functions doesnt work on Macos..
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

bool _isScanning = false;

- (double) bt_le_scan_start {
    
    if (_isScanning) return -1;
    
    // Clear discovered peripherals mutable array
    [self.discoveredPeripherals removeAllObjects];
    
    _isScanning = true;
    int asyncId = [self generateAsyncId];
    
    [_centralManager scanForPeripheralsWithServices:nil options:nil];
    
    [self notifyAsyncOperationSuccess:@("bt_le_scan_start") asyncId:asyncId extraParams:nil];
    
    return asyncId;
}

- (double) bt_le_scan_is_active {
    return [self.centralManager isScanning] ? 1.0 : 0.0;
}

- (double) bt_le_scan_stop {
    
    if (!_isScanning) return -1;
    
    _isScanning = false;
    int asyncId = [self generateAsyncId];
    
    [_centralManager stopScan];
    
    [self notifyAsyncOperationSuccess:@("bt_le_scan_stop") asyncId:asyncId extraParams:nil];
    
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

bool _isAdvertising = false;

- (void) handleStartAdvertisementQueue {
    [self handleQueue:_startAdvertisementQueue withBlock:^(QueuedMutableDictionary *queuedMutableDictionary) {
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
    
    QueuedMutableDictionary* queuedMutableDictionary = [[QueuedMutableDictionary alloc] initWithAsyncId:@(asyncId) dictionary:advertisementData];
    [self queueEnqueue:_startAdvertisementQueue value:queuedMutableDictionary withHandler:^(){ [self handleStartAdvertisementQueue]; }];
    
    // Return asyncId
    return asyncId;
}

- (double) bt_le_advertise_stop {
    
    if (!_isAdvertising) return -1;
    
    [_peripheralManager stopAdvertising];
    
    int asyncId = [self generateAsyncId];
    [self notifyAsyncOperationSuccess:@("bt_le_advertise_stop") asyncId:asyncId extraParams:nil];
    
    return asyncId;
}

- (double) bt_le_advertise_is_active {
    return _peripheralManager.isAdvertising ? 1.0 : 0.0;
}

- (void) peripheralManagerDidStartAdvertising:(CBPeripheralManager *)peripheral error:(NSError *)error {
    
    QueuedMutableDictionary* queuedAdvertisementData = [self queueDequeue:_startAdvertisementQueue];
    
    int asyncId = [queuedAdvertisementData.asyncId intValue];
    NSString* functionName = @"bt_le_advertise_start";
    
    if (error) [self notifyAsyncOperationError:functionName asyncId:asyncId errorCode:(int)error.code extraParams:nil];
    else [self notifyAsyncOperationSuccess:functionName asyncId:asyncId extraParams:nil];
    
    [self handleStartAdvertisementQueue];
}

// ####################################################################################
// # SERVER
// ####################################################################################

bool _isServerOpen = false;

- (void) handleAddServiceQueue {
    [self handleQueue:_addServiceQueue withBlock:^(QueuedMutableService *queuedMutableService) {
        [self->_peripheralManager addService:queuedMutableService.service];
    }];
}

- (double) bt_le_server_open {
    
    if (_isServerOpen) return -1;
    
    _isServerOpen = true;
    
    int asyncId = [self generateAsyncId];
    [self notifyAsyncOperationSuccess:@("bt_le_server_open") asyncId:asyncId extraParams:nil];
    
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
    QueuedMutableService *queueService = [[QueuedMutableService alloc] initWithAsyncId:@(asyncId) service:service];
    
    [self queueEnqueue:_addServiceQueue value:queueService withHandler:^(){ [self handleAddServiceQueue]; }];
        
    return asyncId;
}

- (double) bt_le_server_clear_services {
    
    if (!_isServerOpen) return -1;
    
    [_peripheralManager removeAllServices];
    
    int asyncId = [self generateAsyncId];
    [self notifyAsyncOperationSuccess:@("bt_le_server_clear_services") asyncId:asyncId extraParams:nil];
    
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
    [self notifyAsyncOperationSuccess:@("bt_le_server_close") asyncId:asyncId extraParams:nil];
        
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
    QueuedMutableService *queuedService = _addServiceQueue.firstObject;
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
    int dsMapIndex = CreateDsMap_comaptibility();
    DsMapAddString_comaptibility(dsMapIndex, "type","bt_le_peripheral_manager_update_state");
    CreateAsyncEventWithDSMap_comaptibility(dsMapIndex);
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

NSData *KCharacteristicUnsubscribe = [NSData dataWithBytes:(int[]){1} length:sizeof(int)];
NSData *KCharacteristicNotify = [NSData dataWithBytes:(int[]){2} length:sizeof(int)];
NSData *KCharacteristicIndicate = [NSData dataWithBytes:(int[]){3} length:sizeof(int)];

- (void) handleOpenPeripheralQueue {
    [self handleQueue:_openPeripheralQueue withBlock:^(QueuedTimedPeripheral *queuedTimedPeripheral) {
        [self->_centralManager connectPeripheral:queuedTimedPeripheral.peripheral options:nil];
        queuedTimedPeripheral.timer = [NSTimer scheduledTimerWithTimeInterval:10 target:self selector:@selector(connectionDidTimeout) userInfo:nil repeats:NO];
        
    }];
}

- (void) handleFetchServicesQueue {
    [self handleQueue:_fetchServicesQueue withBlock:^(QueuedPeripheral *queuedPeripheral) {
        [queuedPeripheral.peripheral discoverServices: nil];
    }];
}

- (void) handleFetchCharacteristicsQueue {
    [self handleQueue:_fetchCharacteristicsQueue withBlock:^(QueuedService *queueService) {
        [queueService.peripheral discoverCharacteristics:nil forService:queueService.service];
    }];
}

- (void) handleFetchDescriptorsQueue {
    [self handleQueue:_fetchDescriptorsQueue withBlock:^(QueuedCharacteristic *queuedCharacteristic) {
        [queuedCharacteristic.peripheral discoverDescriptorsForCharacteristic: queuedCharacteristic.characteristic];
    }];
}

- (void) handleReadCharacteristicQueue {
    [self handleQueue:_readCharacteristicQueue withBlock:^(QueuedCharacteristic *queuedCharacteristic) {
        [queuedCharacteristic.peripheral readValueForCharacteristic: queuedCharacteristic.characteristic];
    }];
}

- (void) handleWriteCharacteristicQueue {
    [self handleQueue:_writeCharacteristicQueue withBlock:^(QueuedCharacteristicWithData *queuedCharacteristicData) {
        [queuedCharacteristicData.peripheral writeValue:queuedCharacteristicData.data forCharacteristic:queuedCharacteristicData.characteristic type: CBCharacteristicWriteWithResponse];
    }];
}

- (void) handleNotifyCharacteristicQueue {
    [self handleQueue:_notifyCharacteristicQueue withBlock:^(QueuedCharacteristicWithData *queuedCharacteristicData) {
        BOOL enable = [queuedCharacteristicData.data isEqualToData: KCharacteristicUnsubscribe] ? false : true;
        [queuedCharacteristicData.peripheral setNotifyValue:enable forCharacteristic:queuedCharacteristicData.characteristic];
    }];
}

- (void) handleReadDescriptorQueue {
    [self handleQueue:_readCharacteristicQueue withBlock:^(QueuedDescriptor *queuedDescriptor) {
        [queuedDescriptor.peripheral readValueForDescriptor: queuedDescriptor.descriptor];
    }];
}

- (void) handleWriteDescriptorQueue {
    [self handleQueue:_writeCharacteristicQueue withBlock:^(QueuedDescriptorWithData *queuedDescriptorWithData) {
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
    QueuedTimedPeripheral *queuePeripheral = [[QueuedTimedPeripheral alloc] initWithAsyncId:@(asyncId) peripheral:peripheral];
    
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
    QueuedPeripheral *queuePeripheral = [[QueuedPeripheral alloc] initWithAsyncId:@(asyncId) peripheral:peripheral];

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
    QueuedService *queuedService = [[QueuedService alloc] initWithAsyncId:@(asyncId) peripheral:peripheral service:service];

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
    QueuedCharacteristic *queuedCharacteristic = [[QueuedCharacteristic alloc] initWithAsyncId:@(asyncId) peripheral:peripheral characteristic:characteristic];

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
    QueuedCharacteristic *queuedCharacteristic = [[QueuedCharacteristic alloc] initWithAsyncId:@(asyncId) peripheral:peripheral characteristic:characteristic];

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
    QueuedCharacteristicWithData *queuedCharacteristicData = [[QueuedCharacteristicWithData alloc] initWithAsyncId:@(asyncId) peripheral:peripheral characteristic:characteristic data:data];

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
    [self notifyAsyncOperationSuccess:@("bt_le_characteristic_write_command") asyncId:asyncId extraParams:nil];

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
    QueuedCharacteristicWithData *queuedCharacteristicData = [[QueuedCharacteristicWithData alloc] initWithAsyncId:@(asyncId) peripheral:peripheral characteristic:characteristic data:type];
    
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
    QueuedDescriptor *queuedDescriptor = [[QueuedDescriptor alloc] initWithAsyncId:@(asyncId) peripheral:peripheral descriptor:descriptor];

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
    QueuedDescriptorWithData *queuedDescriptorWithData = [[QueuedDescriptorWithData alloc] initWithAsyncId:@(asyncId) peripheral:peripheral descriptor:descriptor data:data];

    [self queueEnqueue:_writeDescriptorQueue value:queuedDescriptorWithData withHandler:^{ [self handleWriteDescriptorQueue]; }];
    
    return asyncId;
}

- (void) centralManager:(CBCentralManager *)central didConnectPeripheral:(CBPeripheral *)peripheral {
	
    QueuedTimedPeripheral *queuedPeripheral = [self queueDequeue:_openPeripheralQueue];
    
    [queuedPeripheral.timer invalidate];
    
    peripheral.delegate = self;
	
    [_openedPeripherals setObject:peripheral forKey:[peripheral.identifier UUIDString]];
    [_connectedPeripherals setObject:peripheral forKey:[peripheral.identifier UUIDString]];
    
    int asyncId = [queuedPeripheral.asyncId intValue];
    
    NSMutableDictionary* params = [[NSMutableDictionary alloc] init];
    params[@"name"] = peripheral.name;
    params[@"address"] = peripheral.identifier.UUIDString;
    
    [self notifyAsyncOperationSuccess:@"bt_le_peripheral_open" asyncId:asyncId extraParams:params];
    [self handleOpenPeripheralQueue];
}

- (void) centralManager:(CBCentralManager *)central didFailToConnectPeripheral:(CBPeripheral *)peripheral error:(NSError *)error {
    
    QueuedTimedPeripheral *queuedPeripheral = [self queueDequeue:_openPeripheralQueue];
    
    [queuedPeripheral.timer invalidate];
    
    int asyncId = [queuedPeripheral.asyncId intValue];
    
    NSMutableDictionary* params = [[NSMutableDictionary alloc] init];
    params[@"name"] = peripheral.name;
    params[@"address"] = peripheral.identifier.UUIDString;
    
    [self notifyAsyncOperationError:@"bt_le_peripheral_open" asyncId:asyncId errorCode:(int)error.code extraParams:params];
    [self handleOpenPeripheralQueue];
}

- (void) connectionDidTimeout {
    QueuedPeripheral *queuedPeripheral = [self queueDequeue:_openPeripheralQueue];
    
    [_centralManager cancelPeripheralConnection: queuedPeripheral.peripheral];
    
    int asyncId = [queuedPeripheral.asyncId intValue];
    
    NSMutableDictionary* params = [[NSMutableDictionary alloc] init];
    params[@"name"] = queuedPeripheral.peripheral.name;
    params[@"address"] = queuedPeripheral.peripheral.identifier.UUIDString;
    
    [self notifyAsyncOperationError:@"bt_le_peripheral_open" asyncId:asyncId errorCode:(int)133 extraParams:params];
    [self handleOpenPeripheralQueue];
}

- (void) centralManager:(CBCentralManager *)central didDisconnectPeripheral:(CBPeripheral *)peripheral error:(NSError *)error {
    if (error) {
        NSMutableDictionary* params = [[NSMutableDictionary alloc] init];
        params[@"error_code"] = @((int)error.code);
        return [self notifyOperation:@"bt_le_peripheral_disconnect" extraParams:params];
    }
}

#if TARGET_OS_IOS

- (void) centralManager:(CBCentralManager *)central connectionEventDidOccur:(CBConnectionEvent)event forPeripheral:(CBPeripheral *)peripheral {
    
    NSMutableDictionary* params = [[NSMutableDictionary alloc] init];
    params[@"name"] = peripheral.name;
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
	
    QueuedPeripheral *queuedPeripheral = [self queueDequeue:_fetchServicesQueue];
    	
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
    
    QueuedService *queuedService = [self queueDequeue: _fetchCharacteristicsQueue];
    
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
	
    QueuedCharacteristic *queuedCharacteristic = [self queueDequeue:_fetchDescriptorsQueue];
    
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
    
    QueuedCharacteristicWithData *queuedCharacteristicWithData = [self queueDequeue:_notifyCharacteristicQueue];
    
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
    
    QueuedCharacteristic *queuedCharacteristic = [self queuePeek:_readCharacteristicQueue];
    
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
    
    QueuedCharacteristicWithData *queuedCharacteristicWithData = [self queueDequeue:_writeCharacteristicQueue];
        
    NSString* type = @"bt_le_characteristic_write_request";
    int asyncId = [queuedCharacteristicWithData.asyncId intValue];
    
    if (error) [self notifyAsyncOperationError:type asyncId:asyncId errorCode:(int)error.code extraParams:nil];
    else [self notifyAsyncOperationSuccess:type asyncId:asyncId extraParams:nil];
    
    [self handleWriteCharacteristicQueue];
}

- (void) peripheral:(CBPeripheral *)peripheral didUpdateValueForDescriptor:(CBDescriptor *)descriptor error:(nullable NSError *)error {
    
    QueuedDescriptor *queuedDescriptor = [self queueDequeue:_readDescriptorQueue];
    
    NSString *type = @"bt_le_descriptor_read";
    int asyncId = [queuedDescriptor.asyncId intValue];
    
    // Check if there was an error
    if (error) [self notifyAsyncOperationError:type asyncId:asyncId errorCode:(int)error.code extraParams:nil];
    else [self notifyAsyncOperationSuccess:type asyncId:asyncId extraParams:@{@"value": [descriptor.value base64EncodedStringWithOptions:0]}];
    
    [self handleReadDescriptorQueue];
}

- (void) peripheral:(CBPeripheral *)peripheral didWriteValueForDescriptor:(CBDescriptor *)descriptor error:(nullable NSError *)error {
    
    QueuedDescriptorWithData *queuedDescriptorWithData = [self queueDequeue:_writeDescriptorQueue];
    
    NSString *type = @"bt_le_descriptor_write";
    int asyncId = [queuedDescriptorWithData.asyncId intValue];
    
    // Check if there was an error
    if (error) [self notifyAsyncOperationError:type asyncId:asyncId errorCode:(int)error.code extraParams:nil];
    else [self notifyAsyncOperationSuccess:type asyncId:asyncId extraParams:nil];
    
    [self handleWriteDescriptorQueue];
}

- (void) centralManagerDidUpdateState:(nonnull CBCentralManager *)central {
	int dsMapIndex = CreateDsMap_comaptibility();
	DsMapAddString_comaptibility(dsMapIndex, "type","bt_le_state_update");
	DsMapAddDouble_comaptibility(dsMapIndex, "success", central.state);
	CreateAsyncEventWithDSMap_comaptibility(dsMapIndex);
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
	peripheral.delegate = self;
    [_openedPeripherals setObject:peripheral forKey:peripheral];
    [self notifyOperation:@"bt_le_peripheral_service_change" extraParams:@{ @"name": peripheral.name, @"address": peripheral.identifier.UUIDString }];
}

#if TARGET_OS_OSX

Bluetooth *bluetooth;

YYEXPORT void bt_init(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    bluetooth = [Bluetooth new];
    [bluetooth bt_init];
}

YYEXPORT void bt_end(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    [bluetooth bt_end];
    bluetooth = nil;
}

// CORE

YYEXPORT void bt_is_enabled(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    Result.kind = VALUE_BOOL;
    Result.val = [bluetooth bt_is_enabled];
}

YYEXPORT void bt_request_enable(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    Result.kind = VALUE_BOOL;
    Result.val = [bluetooth bt_request_enable];
}

YYEXPORT void bt_get_name(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    YYCreateString(&Result, [[bluetooth bt_get_name] UTF8String]);
}

YYEXPORT void bt_get_address(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    YYCreateString(&Result, [[bluetooth bt_get_address] UTF8String]);
}

YYEXPORT void bt_get_paired_devices(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    YYCreateArray(&Result);
}

// CLASSIC

YYEXPORT void bt_classic_is_supported(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    Result.kind = VALUE_BOOL;
    Result.val = false;
}

YYEXPORT void bt_classic_scan_start(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    Result.kind = VALUE_REAL;
    Result.val = -1;
}

YYEXPORT void bt_classic_scan_is_active(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    Result.kind = VALUE_BOOL;
    Result.val = false;
}

YYEXPORT void bt_classic_scan_stop(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    Result.kind = VALUE_REAL;
    Result.val = -1;
}

YYEXPORT void bt_classic_discoverability_enable(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    Result.kind = VALUE_REAL;
    Result.val = -1;
}

YYEXPORT void bt_classic_discoverability_is_active(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    Result.kind = VALUE_BOOL;
    Result.val = false;
}

YYEXPORT void bt_classic_server_start(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    Result.kind = VALUE_REAL;
    Result.val = -1;
}

YYEXPORT void bt_classic_server_stop(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    Result.kind = VALUE_REAL;
    Result.val = -1;
}

YYEXPORT void bt_classic_socket_open(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    Result.kind = VALUE_REAL;
    Result.val = -1;
}

YYEXPORT void bt_classic_socket_send(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    Result.kind = VALUE_REAL;
    Result.val = -1;
}

YYEXPORT void bt_classic_socket_receive(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    Result.kind = VALUE_REAL;
    Result.val = -1;
}

YYEXPORT void bt_classic_socket_close(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    Result.kind = VALUE_BOOL;
    Result.val = false;
}

YYEXPORT void bt_classic_socket_close_all(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    Result.kind = VALUE_BOOL;
    Result.val = false;
}

// BASE

YYEXPORT void bt_le_is_supported(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    Result.kind = VALUE_BOOL;
    Result.val = true; // [bluetooth bt_le_is_supported];
}

// SCANNER

YYEXPORT void bt_le_scan_start(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    Result.kind = VALUE_REAL;
    Result.val = [bluetooth bt_le_scan_start];
}

YYEXPORT void bt_le_scan_is_active(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    Result.kind = VALUE_BOOL;
    Result.val = [bluetooth bt_le_scan_is_active];
}

YYEXPORT void bt_le_scan_stop(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    Result.kind = VALUE_REAL;
    Result.val = [bluetooth bt_le_scan_stop];
}

// ADVERTISER

YYEXPORT void bt_le_advertise_start(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    const char* settings = YYGetString(arg, 0);
    const char* data = YYGetString(arg, 1);
    
    Result.kind = VALUE_REAL;
    Result.val = [bluetooth bt_le_advertise_start:@(settings) data:@(data)];
}

YYEXPORT void bt_le_advertise_is_active(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    Result.kind = VALUE_BOOL;
    Result.val = [bluetooth bt_le_advertise_is_active];
}

YYEXPORT void bt_le_advertise_stop(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    Result.kind = VALUE_REAL;
    Result.val = [bluetooth bt_le_advertise_stop];
}

// SERVER

YYEXPORT void bt_le_server_open(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    Result.kind = VALUE_REAL;
    Result.val = [bluetooth bt_le_server_open];
}

YYEXPORT void bt_le_server_close(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    Result.kind = VALUE_REAL;
    Result.val = [bluetooth bt_le_server_close];
}

YYEXPORT void bt_le_server_add_service(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    const char* service = YYGetString(arg, 0);
    
    Result.kind = VALUE_REAL;
    Result.val = [bluetooth bt_le_server_add_service:@(service)];
}

YYEXPORT void bt_le_server_clear_services(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    Result.kind = VALUE_REAL;
    Result.val = [bluetooth bt_le_server_clear_services];
}

YYEXPORT void bt_le_server_respond_read(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)//:(NSString*) characteristicUUIDstring data:(char *) buff lenght:(double) lenght
{
    double requestId = YYGetReal(arg, 0);
    double status = YYGetReal(arg, 1);
    const char* value = YYGetString(arg, 2);
    
    [bluetooth bt_le_server_respond_read:requestId status:status value:@(value)];
}

YYEXPORT void bt_le_server_respond_write(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)//:(NSString*) characteristicUUIDstring data:(char *) buff lenght:(double) lenght
{
    double requestId = YYGetReal(arg, 0);
    double status = YYGetReal(arg, 1);
    
    [bluetooth bt_le_server_respond_write:requestId status:status];
}

YYEXPORT void bt_le_server_notify_value(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)//:(NSString*) characteristicUUIDstring data:(char *) buff lenght:(double) lenght
{
    const char* serviceUuid = YYGetString(arg, 0);
    const char* characteristicUuid = YYGetString(arg, 1);
    const char* value = YYGetString(arg, 2);
    
    [bluetooth bt_le_server_notify_value:@(serviceUuid) characteristicUuid:@(characteristicUuid) value:@(value)];
}

// CLIENT

YYEXPORT void bt_le_peripheral_open(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    const char* peripheralUuid = YYGetString(arg, 0);
    
    Result.kind = VALUE_REAL;
    Result.val = [bluetooth bt_le_peripheral_open:@(peripheralUuid)];
}

YYEXPORT void bt_le_peripheral_is_open(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    const char* peripheralUuid = YYGetString(arg, 0);
    
    Result.kind = VALUE_REAL;
    Result.val = [bluetooth bt_le_peripheral_is_open:@(peripheralUuid)];
}

YYEXPORT void bt_le_peripheral_is_connected(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    const char* peripheralUuid = YYGetString(arg, 0);
    
    Result.kind = VALUE_REAL;
    Result.val = [bluetooth bt_le_peripheral_is_connected:@(peripheralUuid)];
}

YYEXPORT void bt_le_peripheral_is_paired(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    const char* peripheralUuid = YYGetString(arg, 0);
    
    Result.kind = VALUE_BOOL;
    Result.val = [bluetooth bt_le_peripheral_is_paired:@(peripheralUuid)];
}

YYEXPORT void bt_le_peripheral_close(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    const char* peripheralUuid = YYGetString(arg, 0);
    
    Result.kind = VALUE_REAL;
    Result.val = [bluetooth bt_le_peripheral_close:@(peripheralUuid)];
}

YYEXPORT void bt_le_peripheral_close_all(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    Result.kind = VALUE_BOOL;
    Result.val = [bluetooth bt_le_peripheral_close_all];
}

YYEXPORT void bt_le_peripheral_get_services(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    const char* peripheralUuid = YYGetString(arg, 0);
    
    Result.kind = VALUE_REAL;
    Result.val = [bluetooth bt_le_peripheral_get_services:@(peripheralUuid)];
}

YYEXPORT void bt_le_service_get_characteristics(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    const char* peripheralUuid = YYGetString(arg, 0);
    const char* serviceUuid = YYGetString(arg, 1);
    
    Result.kind = VALUE_REAL;
    Result.val = [bluetooth bt_le_service_get_characteristics:@(peripheralUuid) service:@(serviceUuid)];
}
 
YYEXPORT void bt_le_characteristic_get_descriptors(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    const char* peripheralUuid = YYGetString(arg, 0);
    const char* serviceUuid = YYGetString(arg, 1);
    const char* characteristicUuid = YYGetString(arg, 2);
    
    Result.kind = VALUE_REAL;
    Result.val = [bluetooth bt_le_characteristic_get_descriptors:@(peripheralUuid) service:@(serviceUuid) characteristic:@(characteristicUuid)];
}

YYEXPORT void bt_le_characteristic_read(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    const char* peripheralUuid = YYGetString(arg, 0);
    const char* serviceUuid = YYGetString(arg, 1);
    const char* characteristicUuid = YYGetString(arg, 2);
    
    Result.kind = VALUE_REAL;
    Result.val = [bluetooth bt_le_characteristic_read:@(peripheralUuid) service: @(serviceUuid) characteristic: @(characteristicUuid)];
}

YYEXPORT void bt_le_characteristic_write_request(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    const char* peripheralUuid = YYGetString(arg, 0);
    const char* serviceUuid = YYGetString(arg, 1);
    const char* characteristicUuid = YYGetString(arg, 2);
    const char* value = YYGetString(arg, 3);
    
    Result.kind = VALUE_REAL;
    Result.val = [bluetooth bt_le_characteristic_write_request:@(peripheralUuid) service: @(serviceUuid) characteristic: @(characteristicUuid) value: @(value)];
}

YYEXPORT void bt_le_characteristic_write_command(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    const char* peripheralUuid = YYGetString(arg, 0);
    const char* serviceUuid = YYGetString(arg, 1);
    const char* characteristicUuid = YYGetString(arg, 2);
    const char* value = YYGetString(arg, 3);
    
    Result.kind = VALUE_REAL;
    Result.val = [bluetooth bt_le_characteristic_write_command:@(peripheralUuid) service: @(serviceUuid) characteristic: @(characteristicUuid) value: @(value)];
}

YYEXPORT void bt_le_characteristic_notify(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    const char* peripheralUuid = YYGetString(arg, 0);
    const char* serviceUuid = YYGetString(arg, 1);
    const char* characteristicUuid = YYGetString(arg, 2);
    
    Result.kind = VALUE_REAL;
    Result.val = [bluetooth bt_le_characteristic_notify:@(peripheralUuid) service: @(serviceUuid) characteristic: @(characteristicUuid)];
}

YYEXPORT void bt_le_characteristic_indicate(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    const char* peripheralUuid = YYGetString(arg, 0);
    const char* serviceUuid = YYGetString(arg, 1);
    const char* characteristicUuid = YYGetString(arg, 2);
    
    Result.kind = VALUE_REAL;
    Result.val = [bluetooth bt_le_characteristic_indicate:@(peripheralUuid) service: @(serviceUuid) characteristic: @(characteristicUuid)];
}

YYEXPORT void bt_le_characteristic_unsubscribe(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    const char* peripheralUuid = YYGetString(arg, 0);
    const char* serviceUuid = YYGetString(arg, 1);
    const char* characteristicUuid = YYGetString(arg, 2);
    
    Result.kind = VALUE_REAL;
    Result.val = [bluetooth bt_le_characteristic_unsubscribe:@(peripheralUuid) service: @(serviceUuid) characteristic: @(characteristicUuid)];
}

YYEXPORT void bt_le_descriptor_read(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    const char* peripheralUuid = YYGetString(arg, 0);
    const char* serviceUuid = YYGetString(arg, 1);
    const char* characteristicUuid = YYGetString(arg, 2);
    const char* descriptorUuid = YYGetString(arg, 3);
    
    Result.kind = VALUE_REAL;
    Result.val = [bluetooth bt_le_descriptor_read:@(peripheralUuid) service: @(serviceUuid) characteristic: @(characteristicUuid) descriptor: @(descriptorUuid)];
}

YYEXPORT void bt_le_descriptor_write(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    const char* peripheralUuid = YYGetString(arg, 0);
    const char* serviceUuid = YYGetString(arg, 1);
    const char* characteristicUuid = YYGetString(arg, 2);
    const char* descriptorUuid = YYGetString(arg, 3);
    const char* value = YYGetString(arg, 3);
    
    Result.kind = VALUE_REAL;
    Result.val = [bluetooth bt_le_descriptor_write:@(peripheralUuid) service: @(serviceUuid) characteristic: @(characteristicUuid) descriptor: @(descriptorUuid) value:@(value)];
}

#endif

@end

