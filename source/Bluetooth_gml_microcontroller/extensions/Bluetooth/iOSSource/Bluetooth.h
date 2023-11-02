
#import <CoreBluetooth/CoreBluetooth.h>

@interface QueuedMutableDictionary : NSObject
@property (nonatomic, strong) NSNumber *asyncId;
@property (nonatomic, strong) NSMutableDictionary *dictionary;

- (instancetype)initWithAsyncId:(NSNumber *)asyncId dictionary:(NSMutableDictionary *)dictionary;
@end

@interface QueuedMutableService : NSObject
@property (nonatomic, strong) NSNumber *asyncId;
@property (nonatomic, strong) CBMutableService *service;

- (instancetype)initWithAsyncId:(NSNumber *)asyncId service:(CBMutableService *)service;
@end

@interface QueuedPeripheral : NSObject
@property (nonatomic, strong) NSNumber *asyncId;
@property (nonatomic, strong) CBPeripheral *peripheral;

- (instancetype)initWithAsyncId:(NSNumber *)asyncId peripheral:(CBPeripheral *)peripheral;
@end

@interface QueuedTimedPeripheral : QueuedPeripheral
@property (nonatomic, strong) CBCentralManager *manager;
@property (nonatomic, strong) NSTimer *timer;

- (instancetype)initWithAsyncId:(NSNumber *)asyncId peripheral:(CBPeripheral *)peripheral timer:(NSTimer *)timer;
@end

@interface QueuedService : QueuedPeripheral
@property (nonatomic, strong) CBService *service;

- (instancetype)initWithAsyncId:(NSNumber *)asyncId peripheral:(CBPeripheral *)peripheral service:(CBService *) service;
@end

@interface QueuedCharacteristic : QueuedPeripheral
@property (nonatomic, strong) CBCharacteristic *characteristic;

- (instancetype)initWithAsyncId:(NSNumber *)asyncId peripheral:(CBPeripheral *)peripheral characteristic:(CBCharacteristic *) characteristic;
@end

@interface QueuedCharacteristicWithData : QueuedCharacteristic
@property (nonatomic, strong) NSData* data;

- (instancetype)initWithAsyncId:(NSNumber *)asyncId peripheral:(CBPeripheral *)peripheral characteristic:(CBCharacteristic *) characteristic data:(NSData*) data;
@end

@interface QueuedDescriptor : QueuedPeripheral
@property (nonatomic, strong) CBDescriptor *descriptor;

- (instancetype)initWithAsyncId:(NSNumber *)asyncId peripheral:(CBPeripheral *)peripheral descriptor:(CBDescriptor *) descriptor;
@end

@interface QueuedDescriptorWithData : QueuedDescriptor
@property (nonatomic, strong) NSData* data;

- (instancetype)initWithAsyncId:(NSNumber *)asyncId peripheral:(CBPeripheral *)peripheral descriptor:(CBDescriptor *) descriptor data:(NSData*) data;
@end

@interface Bluetooth:NSObject<CBCentralManagerDelegate,CBPeripheralDelegate,CBPeripheralManagerDelegate>

// CLIENT

@property(nonatomic, strong) CBCentralManager *centralManager;

@property(nonatomic, strong) NSMutableArray<QueuedPeripheral *> *openPeripheralQueue;
@property(nonatomic, strong) NSMutableArray<QueuedPeripheral *> *closePeripheralQueue;

@property(nonatomic, strong) NSMutableArray<QueuedPeripheral *> *fetchServicesQueue;
@property(nonatomic, strong) NSMutableArray<QueuedService *> *fetchCharacteristicsQueue;
@property(nonatomic, strong) NSMutableArray<QueuedCharacteristic *> *fetchDescriptorsQueue;

@property(nonatomic, strong) NSMutableArray<QueuedCharacteristic *> *readCharacteristicQueue;
@property(nonatomic, strong) NSMutableArray<QueuedCharacteristicWithData *> *writeCharacteristicQueue;
@property(nonatomic, strong) NSMutableArray<QueuedCharacteristicWithData *> *notifyCharacteristicQueue;
@property(nonatomic, strong) NSMutableArray<QueuedDescriptor *> *readDescriptorQueue;
@property(nonatomic, strong) NSMutableArray<QueuedDescriptorWithData *> *writeDescriptorQueue;

@property(nonatomic, strong) NSMutableDictionary <NSString *, CBPeripheral *> *discoveredPeripherals;
@property(nonatomic, strong) NSMutableDictionary <NSString *, CBPeripheral *> *openedPeripherals;
@property(nonatomic, strong) NSMutableDictionary <NSString *, CBPeripheral *> *connectedPeripherals;

// SERVER

@property(nonatomic, strong) CBPeripheralManager *peripheralManager;

@property(nonatomic, strong) NSMutableArray <QueuedMutableService *> *addServiceQueue;
@property(nonatomic, strong) NSMutableArray <QueuedMutableDictionary *> *startAdvertisementQueue;

@property(nonatomic, strong) NSMutableDictionary <NSString *, CBMutableService *> *addedServices;

@property(nonatomic, strong) NSMutableDictionary <NSNumber *, CBATTRequest *> *readRequestsLookup;
@property(nonatomic, strong) NSMutableDictionary <NSNumber *, CBATTRequest *> *writeRequestsLookup;

@end

