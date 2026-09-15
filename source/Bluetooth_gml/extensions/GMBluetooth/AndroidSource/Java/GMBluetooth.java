package ${YYAndroidPackageName};

import ${YYAndroidPackageName}.GMExtWire.GMFunction;

import android.Manifest;
import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import android.bluetooth.BluetoothGattServer;
import android.bluetooth.BluetoothGattServerCallback;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.BluetoothServerSocket;
import android.bluetooth.BluetoothSocket;
import android.bluetooth.le.AdvertiseCallback;
import android.bluetooth.le.AdvertiseData;
import android.bluetooth.le.AdvertiseSettings;
import android.bluetooth.le.BluetoothLeAdvertiser;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanResult;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.ParcelUuid;
import android.util.Base64;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.IdentityHashMap;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicLong;


/**
 * Android implementation of GMBluetooth.
 *
 * This follows the normal Extension Generator Android pattern used by official
 * extensions such as GMAdMob:
 *
 *     GMBluetooth extends GMBluetoothInternal
 *
 * GMBluetoothInternal exposes the generated __EXT_NATIVE__ entry points and
 * forwards them to the methods implemented in this class.
 *
 * Android Bluetooth itself is provided by android.bluetooth.*.
 */
public class GMBluetooth extends GMBluetoothInternal
{
    // =========================================================================
    // Public constants -- must match spec.gmidl
    // =========================================================================

    private static final int OK                 = 0;
    private static final int UNKNOWN            = 1;
    private static final int NOT_SUPPORTED      = 2;
    private static final int NOT_INITIALIZED    = 3;
    private static final int BLUETOOTH_DISABLED = 4;
    private static final int PERMISSION_DENIED  = 5;
    private static final int INVALID_ARGUMENT   = 6;
    private static final int INVALID_HANDLE     = 7;
    private static final int BUSY               = 8;
    private static final int TIMEOUT            = 9;
    private static final int NOT_FOUND          = 10;
    private static final int CONNECTION_FAILED  = 11;
    private static final int DISCONNECTED       = 12;
    private static final int OPERATION_FAILED   = 13;

    private static final int TRANSPORT_UNKNOWN = 0;
    private static final int TRANSPORT_CLASSIC = 1;
    private static final int TRANSPORT_LE      = 2;

    private static final int PERMISSION_UNKNOWN = 0;
    private static final int PERMISSION_GRANTED = 1;
    private static final int PERMISSION_DENIED_STATUS = 2;

    // Keep handles inside 48 bits so callback handles are exactly representable
    // when delivered to GML as doubles.
    private static final long HANDLE_MAGIC = 0x42L;
    private static final long HANDLE_TYPE_DEVICE = 0x01L;
    private static final long HANDLE_TYPE_CLASSIC_CONNECTION = 0x02L;
    private static final long HANDLE_TYPE_LE_CONNECTION = 0x03L;
    private static final long HANDLE_TYPE_LE_SERVICE = 0x04L;
    private static final long HANDLE_TYPE_LE_CHARACTERISTIC = 0x05L;
    private static final long HANDLE_TYPE_LE_DESCRIPTOR = 0x06L;

    private static final int REQUEST_CODE_BLUETOOTH = 0xB710;

    // BluetoothLeSubscribeMode (spec.gmidl) - kept as raw ints since this file
    // has no dependency on the generated enums/ package.
    private static final int SUBSCRIBE_MODE_UNSUBSCRIBE = 0;
    private static final int SUBSCRIBE_MODE_NOTIFY = 1;
    private static final int SUBSCRIBE_MODE_INDICATE = 2;

    private static final UUID CCCD_UUID =
        UUID.fromString("00002902-0000-1000-8000-00805f9b34fb");


    // =========================================================================
    // Android Bluetooth state
    // =========================================================================

    private volatile BluetoothAdapter adapter = null;
    private volatile BluetoothLeScanner leScanner = null;

    private final AtomicBoolean leScanning = new AtomicBoolean(false);
    private final AtomicBoolean classicScanning = new AtomicBoolean(false);
    private final AtomicBoolean serverRunning = new AtomicBoolean(false);

    private volatile BluetoothServerSocket serverSocket = null;
    private volatile boolean receiverRegistered = false;

    private volatile boolean initialized = false;

    // Incrementing this invalidates callbacks from worker threads belonging to
    // an older initialize/shutdown session.
    private final AtomicLong generation = new AtomicLong(1);


    // =========================================================================
    // Error state
    // =========================================================================

    private volatile int lastErrorCode = OK;
    private volatile String lastErrorMessage = "";


    // =========================================================================
    // Device handles
    // =========================================================================

    private static final class DeviceEntry
    {
        long handle;
        int transport;
        String id = "";
        String name = "";
        String address = "";
        boolean addressAvailable = false;
        int rssi = 0;
        boolean rssiAvailable = false;
        boolean connectable = false;
        BluetoothDevice androidDevice = null;
    }

    private final Object deviceLock = new Object();
    private final HashMap<String, Long> deviceById = new HashMap<>();
    private final LinkedHashMap<Long, DeviceEntry> devices = new LinkedHashMap<>();
    private final ArrayList<Long> deviceOrder = new ArrayList<>();
    private long nextDeviceId = 1;


    // =========================================================================
    // Classic connection handles / receive queues
    // =========================================================================

    private static final class ConnectionEntry
    {
        long handle;
        long device;
        volatile BluetoothSocket socket = null;
        volatile boolean connected = false;
        volatile boolean manualClosing = false;

        final Object receiveLock = new Object();
        final ArrayDeque<byte[]> receiveChunks = new ArrayDeque<>();
        int receiveAvailable = 0;
    }

    private final Object connectionLock = new Object();
    private final HashMap<Long, ConnectionEntry> connections = new HashMap<>();
    private long nextConnectionId = 1;


    // =========================================================================
    // BLE GATT client/server handles
    // =========================================================================

    // A single outstanding GATT operation (discover/read/write) at a time per
    // BluetoothGatt is an Android platform constraint, not a project
    // convention - operations are queued per-connection and drained serially
    // from BluetoothGattCallback.
    private static final class LePendingOp
    {
        static final int KIND_DISCOVER_SERVICES = 1;
        static final int KIND_READ_CHARACTERISTIC = 2;
        static final int KIND_WRITE_CHARACTERISTIC = 3;
        static final int KIND_READ_DESCRIPTOR = 4;
        static final int KIND_WRITE_DESCRIPTOR = 5;
        static final int KIND_SUBSCRIBE = 6;

        int kind;
        GMFunction callback;
        long targetHandle;
        int subscribeMode;
    }

    private static final class LeConnectionEntry
    {
        long handle;
        long device;
        boolean serverRole;

        volatile BluetoothGatt gatt = null;
        volatile BluetoothDevice remoteDevice = null;
        volatile boolean connected = false;
        volatile boolean manualClosing = false;
        volatile GMFunction connectCallback = null;

        final Object opLock = new Object();
        final ArrayDeque<Runnable> opQueue = new ArrayDeque<>();
        boolean opRunning = false;
        volatile LePendingOp currentOp = null;

        final Object serviceListLock = new Object();
        final ArrayList<Long> serviceHandles = new ArrayList<>();
    }

    private static final class LeServiceEntry
    {
        long handle;
        long connection;
        BluetoothGattService gattService;
        String uuid = "";
        boolean characteristicsDiscovered = false;

        final ArrayList<Long> characteristicHandles = new ArrayList<>();
    }

    private static final class LeCharacteristicEntry
    {
        long handle;
        long service;
        long connection;
        BluetoothGattCharacteristic gattCharacteristic;
        String uuid = "";
        boolean descriptorsDiscovered = false;
        volatile int subscribeMode = SUBSCRIBE_MODE_UNSUBSCRIBE;

        final ArrayList<Long> descriptorHandles = new ArrayList<>();
    }

    private static final class LeDescriptorEntry
    {
        long handle;
        long characteristic;
        BluetoothGattDescriptor gattDescriptor;
        String uuid = "";
    }

    private static final class LeServerRequestEntry
    {
        int requestId;
        long connection;
        BluetoothDevice device;
        String serviceUuid = "";
        String characteristicUuid = "";
        String descriptorUuid = "";
        boolean isWrite;
        boolean responseNeeded;
        byte[] writeValue = new byte[0];
    }

    private final Object leConnectionLock = new Object();
    private final HashMap<Long, LeConnectionEntry> leConnections = new HashMap<>();
    private final HashMap<Long, Long> leServerConnectionByDevice = new HashMap<>();
    private long nextLeConnectionId = 1;

    private final Object leEntityLock = new Object();
    private final HashMap<Long, LeServiceEntry> leServices = new HashMap<>();
    private final HashMap<Long, LeCharacteristicEntry> leCharacteristics = new HashMap<>();
    private final HashMap<Long, LeDescriptorEntry> leDescriptors = new HashMap<>();
    private final IdentityHashMap<BluetoothGattService, Long> leServiceHandleByObject = new IdentityHashMap<>();
    private final IdentityHashMap<BluetoothGattCharacteristic, Long> leCharacteristicHandleByObject = new IdentityHashMap<>();
    private final IdentityHashMap<BluetoothGattDescriptor, Long> leDescriptorHandleByObject = new IdentityHashMap<>();
    private long nextLeServiceId = 1;
    private long nextLeCharacteristicId = 1;
    private long nextLeDescriptorId = 1;

    private volatile BluetoothLeAdvertiser leAdvertiser = null;
    private volatile AdvertiseCallback leAdvertiseCallback = null;
    private final AtomicBoolean leAdvertising = new AtomicBoolean(false);
    private volatile GMFunction leAdvertiseStartCallback = null;

    private volatile BluetoothGattServer gattServer = null;
    private final AtomicBoolean leServerRunning = new AtomicBoolean(false);
    private final Object leServerAddServiceLock = new Object();
    private final HashMap<String, GMFunction> leServerAddServiceCallbacks = new HashMap<>();

    private final Object leServerRequestLock = new Object();
    private final HashMap<Integer, LeServerRequestEntry> leServerRequests = new HashMap<>();

    // Android does not track who subscribed to notify/indicate for a locally
    // hosted characteristic - this is populated from CCCD descriptor writes
    // we intercept and auto-acknowledge ourselves. Keyed by
    // "<serviceUuid>|<characteristicUuid>".
    private final Object leServerSubscriberLock = new Object();
    private final HashMap<String, HashSet<Long>> leServerSubscribers = new HashMap<>();


    // =========================================================================
    // GML callbacks
    // =========================================================================

    private volatile GMFunction callbackDeviceFound = null;
    private volatile GMFunction callbackScanStopped = null;
    private volatile GMFunction callbackClassicClientConnected = null;
    private volatile GMFunction callbackClassicData = null;
    private volatile GMFunction callbackClassicDisconnected = null;
    private volatile GMFunction callbackLeDisconnected = null;
    private volatile GMFunction callbackLeCharacteristicValueChanged = null;
    private volatile GMFunction callbackLeServerConnectionStateChanged = null;
    private volatile GMFunction callbackLeServerReadRequest = null;
    private volatile GMFunction callbackLeServerWriteRequest = null;


    // =========================================================================
    // Construction
    // =========================================================================

    public GMBluetooth()
    {
    }


    // =========================================================================
    // Helpers
    // =========================================================================

    private static Activity activity()
    {
        return RunnerActivity.CurrentActivity;
    }


    private static Context context()
    {
        Activity current = activity();
        return current != null ? current.getApplicationContext() : null;
    }


    private void setLastError(int code, String message)
    {
        lastErrorCode = code;
        lastErrorMessage = message != null ? message : "";
    }


    private int result(int code, String message)
    {
        setLastError(code, message);
        return code;
    }


    private static String throwableMessage(Throwable throwable)
    {
        if (throwable == null)
            return "";

        String message = throwable.getMessage();
        return message != null ? message : throwable.toString();
    }


    private long makeHandle(long type, long id)
    {
        return (HANDLE_MAGIC << 40) |
               (type << 32) |
               (id & 0xFFFFFFFFL);
    }


    private boolean isHandleType(long handle, long type)
    {
        return ((handle >> 40) & 0xFFL) == HANDLE_MAGIC &&
               ((handle >> 32) & 0xFFL) == type;
    }


    private DeviceEntry copyDevice(long handle)
    {
        if (!isHandleType(handle, HANDLE_TYPE_DEVICE))
            return null;

        synchronized (deviceLock)
        {
            DeviceEntry source = devices.get(handle);
            if (source == null)
                return null;

            DeviceEntry copy = new DeviceEntry();
            copy.handle = source.handle;
            copy.transport = source.transport;
            copy.id = source.id;
            copy.name = source.name;
            copy.address = source.address;
            copy.addressAvailable = source.addressAvailable;
            copy.rssi = source.rssi;
            copy.rssiAvailable = source.rssiAvailable;
            copy.connectable = source.connectable;
            copy.androidDevice = source.androidDevice;
            return copy;
        }
    }


    private ConnectionEntry getConnection(long handle)
    {
        if (!isHandleType(handle, HANDLE_TYPE_CLASSIC_CONNECTION))
            return null;

        synchronized (connectionLock)
        {
            return connections.get(handle);
        }
    }


    private long createConnection(long device)
    {
        synchronized (connectionLock)
        {
            long handle = makeHandle(
                HANDLE_TYPE_CLASSIC_CONNECTION,
                nextConnectionId++);

            ConnectionEntry entry = new ConnectionEntry();
            entry.handle = handle;
            entry.device = device;
            connections.put(handle, entry);
            return handle;
        }
    }


    private void eraseConnection(long handle)
    {
        synchronized (connectionLock)
        {
            connections.remove(handle);
        }
    }


    private long upsertDevice(
        int transport,
        String id,
        String name,
        String address,
        int rssi,
        boolean hasRssi,
        boolean connectable,
        BluetoothDevice androidDevice)
    {
        String safeId = id != null ? id : "";
        String safeName = name != null ? name : "";
        String safeAddress = address != null ? address : "";

        boolean created = false;
        long handle;

        synchronized (deviceLock)
        {
            Long existing = deviceById.get(safeId);
            DeviceEntry entry;

            if (existing == null)
            {
                handle = makeHandle(HANDLE_TYPE_DEVICE, nextDeviceId++);

                entry = new DeviceEntry();
                entry.handle = handle;
                entry.id = safeId;

                devices.put(handle, entry);
                deviceById.put(safeId, handle);
                deviceOrder.add(handle);
                created = true;
            }
            else
            {
                handle = existing;
                entry = devices.get(handle);
                if (entry == null)
                    return 0;
            }

            entry.transport = transport;

            if (!safeName.isEmpty())
                entry.name = safeName;

            if (!safeAddress.isEmpty())
            {
                entry.address = safeAddress;
                entry.addressAvailable = true;
            }

            entry.rssi = rssi;
            entry.rssiAvailable = hasRssi;
            entry.connectable = connectable;

            if (androidDevice != null)
                entry.androidDevice = androidDevice;
        }

        if (created)
        {
            invoke(callbackDeviceFound, (double) handle);
        }

        return handle;
    }


    private static String safeName(BluetoothDevice device)
    {
        if (device == null)
            return "";

        try
        {
            String value = device.getName();
            return value != null ? value : "";
        }
        catch (SecurityException ignored)
        {
            return "";
        }
    }


    private static String safeAddress(BluetoothDevice device)
    {
        if (device == null)
            return "";

        try
        {
            String value = device.getAddress();
            return value != null ? value : "";
        }
        catch (SecurityException ignored)
        {
            return "";
        }
    }


    private String deviceId(int transport, BluetoothDevice device)
    {
        String address = safeAddress(device);

        if (!address.isEmpty())
        {
            return transport == TRANSPORT_LE
                ? "android:ble:" + address
                : "android:classic:" + address;
        }

        return (transport == TRANSPORT_LE
            ? "android:ble:object:"
            : "android:classic:object:") +
            System.identityHashCode(device);
    }


    private boolean hasPermission(String permission)
    {
        Context current = context();

        if (current == null)
            return false;

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M)
            return true;

        return current.checkSelfPermission(permission) ==
            PackageManager.PERMISSION_GRANTED;
    }


    private boolean hasScanPermission()
    {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S)
            return hasPermission(Manifest.permission.BLUETOOTH_SCAN);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M)
            return hasPermission(Manifest.permission.ACCESS_FINE_LOCATION);

        return true;
    }


    private boolean hasConnectPermission()
    {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S)
            return hasPermission(Manifest.permission.BLUETOOTH_CONNECT);

        return true;
    }


    private boolean hasAdvertisePermission()
    {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S)
            return hasPermission(Manifest.permission.BLUETOOTH_ADVERTISE);

        return true;
    }


    private boolean adapterEnabled()
    {
        BluetoothAdapter current = adapter;

        if (current == null)
            return false;

        try
        {
            return current.isEnabled();
        }
        catch (SecurityException ignored)
        {
            return false;
        }
    }


    private void dispatchScanStopped(int transport, int error, String message)
    {
        String safeMessage = message != null ? message : "";

        if (error != OK)
            setLastError(error, safeMessage);

        invoke(callbackScanStopped, error, safeMessage);
    }


    private void dispatchConnectResult(
        long connection,
        long device,
        int error,
        String message,
        GMFunction callback)
    {
        String safeMessage = message != null ? message : "";
        ConnectionEntry entry = getConnection(connection);

        if (error == OK)
        {
            if (entry != null)
                entry.connected = true;
        }
        else
        {
            eraseConnection(connection);
            setLastError(error, safeMessage);
        }

        invoke(
            callback,
            error,
            safeMessage,
            (double) connection,
            (double) device);
    }


    private void dispatchDisconnected(
        long connection,
        int error,
        String message)
    {
        String safeMessage = message != null ? message : "";

        invoke(
            callbackClassicDisconnected,
            (double) connection,
            error,
            safeMessage);

        eraseConnection(connection);

        if (error != OK)
            setLastError(error, safeMessage);
    }


    private void appendReceived(long connection, byte[] data)
    {
        if (data == null || data.length == 0)
            return;

        ConnectionEntry entry = getConnection(connection);
        if (entry == null)
            return;

        int available;

        synchronized (entry.receiveLock)
        {
            entry.receiveChunks.addLast(data);
            entry.receiveAvailable += data.length;
            available = entry.receiveAvailable;
        }

        invoke(
            callbackClassicData,
            (double) connection,
            available);
    }


    private void invoke(GMFunction callback, Object... arguments)
    {
        if (callback == null)
            return;

        try
        {
            callback.call(arguments);
        }
        catch (Throwable ignored)
        {
        }
    }


    // =========================================================================
    // BLE GATT handle helpers
    // =========================================================================

    private LeConnectionEntry getLeConnection(long handle)
    {
        if (!isHandleType(handle, HANDLE_TYPE_LE_CONNECTION))
            return null;

        synchronized (leConnectionLock)
        {
            return leConnections.get(handle);
        }
    }


    private long createLeConnection(long device, boolean serverRole)
    {
        synchronized (leConnectionLock)
        {
            long handle = makeHandle(HANDLE_TYPE_LE_CONNECTION, nextLeConnectionId++);

            LeConnectionEntry entry = new LeConnectionEntry();
            entry.handle = handle;
            entry.device = device;
            entry.serverRole = serverRole;
            leConnections.put(handle, entry);
            return handle;
        }
    }


    private void eraseLeConnection(long handle)
    {
        LeConnectionEntry entry;

        synchronized (leConnectionLock)
        {
            entry = leConnections.remove(handle);

            Iterator<Map.Entry<Long, Long>> iterator =
                leServerConnectionByDevice.entrySet().iterator();

            while (iterator.hasNext())
            {
                if (iterator.next().getValue() == handle)
                    iterator.remove();
            }
        }

        if (entry == null)
            return;

        ArrayList<Long> services;

        synchronized (entry.serviceListLock)
        {
            services = new ArrayList<>(entry.serviceHandles);
        }

        synchronized (leEntityLock)
        {
            for (long serviceHandle : services)
            {
                LeServiceEntry service = leServices.remove(serviceHandle);
                if (service == null)
                    continue;

                if (service.gattService != null)
                    leServiceHandleByObject.remove(service.gattService);

                for (long characteristicHandle : service.characteristicHandles)
                {
                    LeCharacteristicEntry characteristic =
                        leCharacteristics.remove(characteristicHandle);

                    if (characteristic == null)
                        continue;

                    if (characteristic.gattCharacteristic != null)
                        leCharacteristicHandleByObject.remove(
                            characteristic.gattCharacteristic);

                    for (long descriptorHandle : characteristic.descriptorHandles)
                    {
                        LeDescriptorEntry descriptor =
                            leDescriptors.remove(descriptorHandle);

                        if (descriptor != null && descriptor.gattDescriptor != null)
                            leDescriptorHandleByObject.remove(
                                descriptor.gattDescriptor);
                    }
                }
            }
        }
    }


    private long findOrCreateServiceHandle(
        long connectionHandle,
        BluetoothGattService gattService)
    {
        synchronized (leEntityLock)
        {
            Long existing = leServiceHandleByObject.get(gattService);
            if (existing != null)
                return existing;

            long handle = makeHandle(HANDLE_TYPE_LE_SERVICE, nextLeServiceId++);

            LeServiceEntry entry = new LeServiceEntry();
            entry.handle = handle;
            entry.connection = connectionHandle;
            entry.gattService = gattService;
            entry.uuid = gattService.getUuid() != null
                ? gattService.getUuid().toString()
                : "";

            leServices.put(handle, entry);
            leServiceHandleByObject.put(gattService, handle);
            return handle;
        }
    }


    private long findOrCreateCharacteristicHandle(
        long serviceHandle,
        long connectionHandle,
        BluetoothGattCharacteristic gattCharacteristic)
    {
        synchronized (leEntityLock)
        {
            Long existing = leCharacteristicHandleByObject.get(gattCharacteristic);
            if (existing != null)
                return existing;

            long handle = makeHandle(
                HANDLE_TYPE_LE_CHARACTERISTIC,
                nextLeCharacteristicId++);

            LeCharacteristicEntry entry = new LeCharacteristicEntry();
            entry.handle = handle;
            entry.service = serviceHandle;
            entry.connection = connectionHandle;
            entry.gattCharacteristic = gattCharacteristic;
            entry.uuid = gattCharacteristic.getUuid() != null
                ? gattCharacteristic.getUuid().toString()
                : "";

            leCharacteristics.put(handle, entry);
            leCharacteristicHandleByObject.put(gattCharacteristic, handle);
            return handle;
        }
    }


    private long findOrCreateDescriptorHandle(
        long characteristicHandle,
        BluetoothGattDescriptor gattDescriptor)
    {
        synchronized (leEntityLock)
        {
            Long existing = leDescriptorHandleByObject.get(gattDescriptor);
            if (existing != null)
                return existing;

            long handle = makeHandle(HANDLE_TYPE_LE_DESCRIPTOR, nextLeDescriptorId++);

            LeDescriptorEntry entry = new LeDescriptorEntry();
            entry.handle = handle;
            entry.characteristic = characteristicHandle;
            entry.gattDescriptor = gattDescriptor;
            entry.uuid = gattDescriptor.getUuid() != null
                ? gattDescriptor.getUuid().toString()
                : "";

            leDescriptors.put(handle, entry);
            leDescriptorHandleByObject.put(gattDescriptor, handle);
            return handle;
        }
    }


    private LeServiceEntry getLeService(long handle)
    {
        if (!isHandleType(handle, HANDLE_TYPE_LE_SERVICE))
            return null;

        synchronized (leEntityLock)
        {
            return leServices.get(handle);
        }
    }


    private LeCharacteristicEntry getLeCharacteristic(long handle)
    {
        if (!isHandleType(handle, HANDLE_TYPE_LE_CHARACTERISTIC))
            return null;

        synchronized (leEntityLock)
        {
            return leCharacteristics.get(handle);
        }
    }


    private LeDescriptorEntry getLeDescriptor(long handle)
    {
        if (!isHandleType(handle, HANDLE_TYPE_LE_DESCRIPTOR))
            return null;

        synchronized (leEntityLock)
        {
            return leDescriptors.get(handle);
        }
    }


    // =========================================================================
    // BLE GATT client operation queue
    //
    // Android's BluetoothGatt allows only one outstanding read/write/discover
    // operation at a time - a second call issued while one is in flight
    // silently fails. Every client-role operation on a connection is funneled
    // through this queue and drained one at a time from BluetoothGattCallback.
    // =========================================================================

    private void enqueueGattOp(LeConnectionEntry connection, Runnable op)
    {
        boolean startNow;

        synchronized (connection.opLock)
        {
            connection.opQueue.addLast(op);
            startNow = !connection.opRunning;
            if (startNow)
                connection.opRunning = true;
        }

        if (startNow)
            runNextGattOp(connection);
    }


    private void runNextGattOp(LeConnectionEntry connection)
    {
        Runnable op;

        synchronized (connection.opLock)
        {
            op = connection.opQueue.pollFirst();

            if (op == null)
            {
                connection.opRunning = false;
                return;
            }
        }

        op.run();
    }


    private void completeGattOp(LeConnectionEntry connection)
    {
        connection.currentOp = null;
        runNextGattOp(connection);
    }


    // =========================================================================
    // BLE GATT dispatch helpers
    // =========================================================================

    private void dispatchLeDisconnected(long connection, int error, String message)
    {
        String safeMessage = message != null ? message : "";

        invoke(callbackLeDisconnected, (double) connection, error, safeMessage);

        if (error != OK)
            setLastError(error, safeMessage);
    }


    private void dispatchLeCharacteristicValueChanged(
        long characteristic,
        long connection)
    {
        invoke(
            callbackLeCharacteristicValueChanged,
            (double) characteristic,
            (double) connection);
    }


    private void dispatchLeServerConnectionStateChanged(
        long connection,
        boolean connected,
        long device)
    {
        invoke(
            callbackLeServerConnectionStateChanged,
            (double) connection,
            connected,
            (double) device);
    }


    private static String subscriberKey(String serviceUuid, String characteristicUuid)
    {
        return serviceUuid + "|" + characteristicUuid;
    }


    // =========================================================================
    // Lifecycle
    // =========================================================================

    @Override
    public boolean bluetooth_initialize()
    {
        if (initialized)
        {
            setLastError(OK, "");
            return true;
        }

        Context current = context();

        if (current == null)
        {
            setLastError(
                OPERATION_FAILED,
                "Android application context is unavailable");
            return false;
        }

        try
        {
            BluetoothManager manager =
                (BluetoothManager) current.getSystemService(
                    Context.BLUETOOTH_SERVICE);

            adapter = manager != null
                ? manager.getAdapter()
                : BluetoothAdapter.getDefaultAdapter();

            if (adapter == null)
            {
                setLastError(
                    NOT_SUPPORTED,
                    "Android BluetoothAdapter is unavailable");
                return false;
            }

            generation.incrementAndGet();
            initialized = true;
            setLastError(OK, "");
            return true;
        }
        catch (Throwable throwable)
        {
            adapter = null;
            initialized = false;
            setLastError(OPERATION_FAILED, throwableMessage(throwable));
            return false;
        }
    }


    @Override
    public void bluetooth_shutdown()
    {
        if (!initialized)
            return;

        initialized = false;
        generation.incrementAndGet();

        // Stop scanner without emitting callbacks during shutdown.
        try
        {
            BluetoothLeScanner scanner = leScanner;
            if (scanner != null && leScanning.get() && hasScanPermission())
                scanner.stopScan(leScanCallback);
        }
        catch (Throwable ignored)
        {
        }

        leScanning.set(false);
        leScanner = null;

        try
        {
            BluetoothAdapter current = adapter;
            if (current != null && current.isDiscovering())
                current.cancelDiscovery();
        }
        catch (Throwable ignored)
        {
        }

        classicScanning.set(false);
        unregisterClassicReceiver();

        stopServerInternal();
        stopLeAdvertiseInternal();
        stopLeServerInternal();

        ArrayList<LeConnectionEntry> openLeConnections = new ArrayList<>();

        synchronized (leConnectionLock)
        {
            openLeConnections.addAll(leConnections.values());
            leConnections.clear();
            leServerConnectionByDevice.clear();
        }

        for (LeConnectionEntry entry : openLeConnections)
        {
            entry.manualClosing = true;

            BluetoothGatt gatt = entry.gatt;
            if (gatt != null)
            {
                try
                {
                    gatt.close();
                }
                catch (Throwable ignored)
                {
                }
            }
        }

        synchronized (leEntityLock)
        {
            leServices.clear();
            leCharacteristics.clear();
            leDescriptors.clear();
            leServiceHandleByObject.clear();
            leCharacteristicHandleByObject.clear();
            leDescriptorHandleByObject.clear();
        }

        synchronized (leServerRequestLock)
        {
            leServerRequests.clear();
        }

        synchronized (leServerSubscriberLock)
        {
            leServerSubscribers.clear();
        }

        ArrayList<ConnectionEntry> openConnections = new ArrayList<>();

        synchronized (connectionLock)
        {
            openConnections.addAll(connections.values());
            connections.clear();
        }

        for (ConnectionEntry entry : openConnections)
        {
            entry.manualClosing = true;

            BluetoothSocket socket = entry.socket;
            if (socket != null)
            {
                try
                {
                    socket.close();
                }
                catch (Throwable ignored)
                {
                }
            }
        }

        synchronized (deviceLock)
        {
            deviceById.clear();
            devices.clear();
            deviceOrder.clear();
        }

        callbackDeviceFound = null;
        callbackScanStopped = null;
        callbackClassicClientConnected = null;
        callbackClassicData = null;
        callbackClassicDisconnected = null;
        callbackLeDisconnected = null;
        callbackLeCharacteristicValueChanged = null;
        callbackLeServerConnectionStateChanged = null;
        callbackLeServerReadRequest = null;
        callbackLeServerWriteRequest = null;

        adapter = null;
        setLastError(OK, "");
    }


    @Override
    public boolean bluetooth_is_initialized()
    {
        return initialized;
    }


    // =========================================================================
    // Error state
    // =========================================================================

    @Override
    public int bluetooth_last_error_code()
    {
        return lastErrorCode;
    }


    @Override
    public String bluetooth_last_error_message()
    {
        return lastErrorMessage;
    }


    // =========================================================================
    // Capabilities / permissions
    // =========================================================================

    @Override
    public boolean bluetooth_le_is_supported()
    {
        if (!initialized || adapter == null)
            return false;

        Context current = context();

        return current != null &&
            current.getPackageManager().hasSystemFeature(
                PackageManager.FEATURE_BLUETOOTH_LE);
    }


    @Override
    public boolean bluetooth_classic_is_supported()
    {
        return initialized && adapter != null;
    }


    @Override
    public boolean bluetooth_classic_server_is_supported()
    {
        return initialized && adapter != null;
    }


    @Override
    public int bluetooth_permission_get_status()
    {
        if (!initialized)
            return PERMISSION_UNKNOWN;

        return hasScanPermission() && hasConnectPermission()
            ? PERMISSION_GRANTED
            : PERMISSION_DENIED_STATUS;
    }


    @Override
    public int bluetooth_permission_request()
    {
        if (!initialized)
            return result(
                NOT_INITIALIZED,
                "Bluetooth is not initialized");

        Activity current = activity();

        if (current == null)
            return result(
                NOT_INITIALIZED,
                "Current Android Activity is unavailable");

        if (bluetooth_permission_get_status() == PERMISSION_GRANTED)
            return result(OK, "");

        try
        {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S)
            {
                current.requestPermissions(
                    new String[] {
                        Manifest.permission.BLUETOOTH_SCAN,
                        Manifest.permission.BLUETOOTH_CONNECT
                    },
                    REQUEST_CODE_BLUETOOTH);
            }
            else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M)
            {
                current.requestPermissions(
                    new String[] {
                        Manifest.permission.ACCESS_FINE_LOCATION
                    },
                    REQUEST_CODE_BLUETOOTH);
            }

            return result(OK, "");
        }
        catch (Throwable throwable)
        {
            return result(
                OPERATION_FAILED,
                throwableMessage(throwable));
        }
    }


    // =========================================================================
    // BLE scanning
    // =========================================================================

    private final ScanCallback leScanCallback = new ScanCallback()
    {
        @Override
        public void onScanResult(int callbackType, ScanResult scanResult)
        {
            if (!initialized || scanResult == null)
                return;

            BluetoothDevice device = scanResult.getDevice();

            if (device == null)
                return;

            boolean connectable = true;

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
                connectable = scanResult.isConnectable();

            int rssi = scanResult.getRssi();

            upsertDevice(
                TRANSPORT_LE,
                deviceId(TRANSPORT_LE, device),
                safeName(device),
                safeAddress(device),
                rssi,
                rssi != 127,
                connectable,
                device);
        }


        @Override
        public void onScanFailed(int errorCode)
        {
            if (!initialized)
                return;

            leScanning.set(false);

            dispatchScanStopped(
                TRANSPORT_LE,
                OPERATION_FAILED,
                "Android BLE scan failed: " + errorCode);
        }
    };


    @Override
    public int bluetooth_le_scan_start(boolean active)
    {
        // Android's scanner does not expose a direct active/passive flag in the
        // same sense as Windows. Keep the API argument for cross-platform parity.
        if (!initialized || adapter == null)
            return result(
                NOT_INITIALIZED,
                "Bluetooth is not initialized");

        if (!hasScanPermission())
            return result(
                PERMISSION_DENIED,
                "Bluetooth scan permission is not granted");

        if (!adapterEnabled())
            return result(
                BLUETOOTH_DISABLED,
                "Bluetooth is disabled");

        if (leScanning.get())
            return result(OK, "");

        try
        {
            leScanner = adapter.getBluetoothLeScanner();

            if (leScanner == null)
                return result(
                    NOT_SUPPORTED,
                    "Bluetooth LE scanner is unavailable");

            leScanner.startScan(leScanCallback);
            leScanning.set(true);

            return result(OK, "");
        }
        catch (SecurityException exception)
        {
            return result(
                PERMISSION_DENIED,
                throwableMessage(exception));
        }
        catch (Throwable throwable)
        {
            return result(
                OPERATION_FAILED,
                throwableMessage(throwable));
        }
    }


    @Override
    public int bluetooth_le_scan_stop()
    {
        if (!initialized)
            return result(
                NOT_INITIALIZED,
                "Bluetooth is not initialized");

        if (!leScanning.getAndSet(false))
            return result(OK, "");

        try
        {
            BluetoothLeScanner scanner = leScanner;

            if (scanner != null && hasScanPermission())
                scanner.stopScan(leScanCallback);
        }
        catch (Throwable ignored)
        {
        }

        dispatchScanStopped(TRANSPORT_LE, OK, "");
        return result(OK, "");
    }


    @Override
    public boolean bluetooth_le_scan_is_running()
    {
        return initialized && leScanning.get();
    }


    // =========================================================================
    // BLE GATT client - connect / disconnect
    // =========================================================================

    private BluetoothGattCallback createGattCallback(
        final long connection,
        final long workerGeneration)
    {
        return new BluetoothGattCallback()
        {
            @Override
            public void onConnectionStateChange(
                BluetoothGatt gatt,
                int status,
                int newState)
            {
                if (generation.get() != workerGeneration)
                    return;

                LeConnectionEntry entry = getLeConnection(connection);
                if (entry == null)
                    return;

                if (newState == BluetoothProfile.STATE_CONNECTED)
                {
                    entry.connected = true;

                    GMFunction connectCallback = entry.connectCallback;
                    entry.connectCallback = null;

                    invoke(
                        connectCallback,
                        status == BluetoothGatt.GATT_SUCCESS ? OK : CONNECTION_FAILED,
                        status == BluetoothGatt.GATT_SUCCESS
                            ? ""
                            : ("GATT status " + status),
                        (double) connection,
                        (double) entry.device);

                    if (status != BluetoothGatt.GATT_SUCCESS)
                    {
                        entry.connected = false;

                        try
                        {
                            gatt.close();
                        }
                        catch (Throwable ignored)
                        {
                        }

                        eraseLeConnection(connection);
                    }
                }
                else if (newState == BluetoothProfile.STATE_DISCONNECTED)
                {
                    boolean wasConnected = entry.connected;
                    boolean manual = entry.manualClosing;
                    entry.connected = false;

                    GMFunction pendingConnectCallback = entry.connectCallback;
                    entry.connectCallback = null;

                    try
                    {
                        gatt.close();
                    }
                    catch (Throwable ignored)
                    {
                    }

                    if (pendingConnectCallback != null)
                    {
                        // Disconnected before STATE_CONNECTED ever fired -
                        // report the connect attempt itself as failed.
                        invoke(
                            pendingConnectCallback,
                            CONNECTION_FAILED,
                            "GATT status " + status,
                            (double) connection,
                            (double) entry.device);
                    }
                    else if (wasConnected)
                    {
                        dispatchLeDisconnected(
                            connection,
                            manual ? OK : DISCONNECTED,
                            manual ? "Disconnected" : ("GATT status " + status));
                    }

                    eraseLeConnection(connection);
                }
            }


            @Override
            public void onServicesDiscovered(BluetoothGatt gatt, int status)
            {
                if (generation.get() != workerGeneration)
                    return;

                LeConnectionEntry entry = getLeConnection(connection);
                if (entry == null)
                    return;

                LePendingOp op = entry.currentOp;
                GMFunction callback = op != null ? op.callback : null;

                if (status == BluetoothGatt.GATT_SUCCESS)
                {
                    synchronized (entry.serviceListLock)
                    {
                        entry.serviceHandles.clear();

                        for (BluetoothGattService service : gatt.getServices())
                        {
                            entry.serviceHandles.add(
                                findOrCreateServiceHandle(connection, service));
                        }
                    }

                    invoke(callback, OK, "", (double) connection);
                }
                else
                {
                    invoke(
                        callback,
                        OPERATION_FAILED,
                        "GATT status " + status,
                        (double) connection);
                }

                completeGattOp(entry);
            }


            @Override
            public void onCharacteristicRead(
                BluetoothGatt gatt,
                BluetoothGattCharacteristic characteristic,
                int status)
            {
                if (generation.get() != workerGeneration)
                    return;

                LeConnectionEntry entry = getLeConnection(connection);
                if (entry == null)
                    return;

                LePendingOp op = entry.currentOp;

                if (op != null)
                {
                    if (status == BluetoothGatt.GATT_SUCCESS)
                        invoke(op.callback, OK, "", (double) op.targetHandle);
                    else
                        invoke(
                            op.callback,
                            OPERATION_FAILED,
                            "GATT status " + status,
                            (double) op.targetHandle);
                }

                completeGattOp(entry);
            }


            @Override
            public void onCharacteristicWrite(
                BluetoothGatt gatt,
                BluetoothGattCharacteristic characteristic,
                int status)
            {
                if (generation.get() != workerGeneration)
                    return;

                LeConnectionEntry entry = getLeConnection(connection);
                if (entry == null)
                    return;

                LePendingOp op = entry.currentOp;

                if (op != null)
                {
                    if (status == BluetoothGatt.GATT_SUCCESS)
                        invoke(op.callback, OK, "", (double) op.targetHandle);
                    else
                        invoke(
                            op.callback,
                            OPERATION_FAILED,
                            "GATT status " + status,
                            (double) op.targetHandle);
                }

                completeGattOp(entry);
            }


            @Override
            public void onDescriptorRead(
                BluetoothGatt gatt,
                BluetoothGattDescriptor descriptor,
                int status)
            {
                if (generation.get() != workerGeneration)
                    return;

                LeConnectionEntry entry = getLeConnection(connection);
                if (entry == null)
                    return;

                LePendingOp op = entry.currentOp;

                if (op != null)
                {
                    if (status == BluetoothGatt.GATT_SUCCESS)
                        invoke(op.callback, OK, "", (double) op.targetHandle);
                    else
                        invoke(
                            op.callback,
                            OPERATION_FAILED,
                            "GATT status " + status,
                            (double) op.targetHandle);
                }

                completeGattOp(entry);
            }


            @Override
            public void onDescriptorWrite(
                BluetoothGatt gatt,
                BluetoothGattDescriptor descriptor,
                int status)
            {
                if (generation.get() != workerGeneration)
                    return;

                LeConnectionEntry entry = getLeConnection(connection);
                if (entry == null)
                    return;

                LePendingOp op = entry.currentOp;

                if (op == null)
                {
                    completeGattOp(entry);
                    return;
                }

                if (op.kind == LePendingOp.KIND_SUBSCRIBE)
                {
                    LeCharacteristicEntry characteristicEntry =
                        getLeCharacteristic(op.targetHandle);

                    if (status == BluetoothGatt.GATT_SUCCESS)
                    {
                        if (characteristicEntry != null)
                            characteristicEntry.subscribeMode = op.subscribeMode;

                        invoke(op.callback, OK, "", (double) op.targetHandle);
                    }
                    else
                    {
                        invoke(
                            op.callback,
                            OPERATION_FAILED,
                            "GATT status " + status,
                            (double) op.targetHandle);
                    }
                }
                else if (status == BluetoothGatt.GATT_SUCCESS)
                {
                    invoke(op.callback, OK, "", (double) op.targetHandle);
                }
                else
                {
                    invoke(
                        op.callback,
                        OPERATION_FAILED,
                        "GATT status " + status,
                        (double) op.targetHandle);
                }

                completeGattOp(entry);
            }


            @Override
            public void onCharacteristicChanged(
                BluetoothGatt gatt,
                BluetoothGattCharacteristic characteristic)
            {
                if (generation.get() != workerGeneration)
                    return;

                Long characteristicHandle;

                synchronized (leEntityLock)
                {
                    characteristicHandle =
                        leCharacteristicHandleByObject.get(characteristic);
                }

                if (characteristicHandle != null)
                    dispatchLeCharacteristicValueChanged(
                        characteristicHandle,
                        connection);
            }
        };
    }


    @Override
    public long bluetooth_le_connect(long device, GMFunction callback)
    {
        if (!initialized || adapter == null)
        {
            setLastError(NOT_INITIALIZED, "Bluetooth is not initialized");
            return 0;
        }

        DeviceEntry deviceEntry = copyDevice(device);

        if (deviceEntry == null || deviceEntry.transport != TRANSPORT_LE)
        {
            setLastError(INVALID_HANDLE, "Expected a BLE device handle");
            return 0;
        }

        if (!hasConnectPermission())
        {
            setLastError(
                PERMISSION_DENIED,
                "Bluetooth connect permission is not granted");
            return 0;
        }

        if (!adapterEnabled())
        {
            setLastError(BLUETOOTH_DISABLED, "Bluetooth is disabled");
            return 0;
        }

        BluetoothDevice androidDevice = deviceEntry.androidDevice;

        if (androidDevice == null)
        {
            if (deviceEntry.address == null || deviceEntry.address.isEmpty())
            {
                setLastError(
                    INVALID_ARGUMENT,
                    "BLE device has no usable address");
                return 0;
            }

            try
            {
                androidDevice = adapter.getRemoteDevice(deviceEntry.address);
            }
            catch (Throwable throwable)
            {
                setLastError(INVALID_ARGUMENT, throwableMessage(throwable));
                return 0;
            }
        }

        Context current = context();

        if (current == null)
        {
            setLastError(
                OPERATION_FAILED,
                "Android application context is unavailable");
            return 0;
        }

        final long connection = createLeConnection(device, false);
        final long workerGeneration = generation.get();

        LeConnectionEntry entry = getLeConnection(connection);
        entry.connectCallback = callback;

        setLastError(OK, "");

        try
        {
            BluetoothGattCallback gattCallback =
                createGattCallback(connection, workerGeneration);

            BluetoothGatt gatt;

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M)
            {
                gatt = androidDevice.connectGatt(
                    current,
                    false,
                    gattCallback,
                    BluetoothDevice.TRANSPORT_LE);
            }
            else
            {
                gatt = androidDevice.connectGatt(current, false, gattCallback);
            }

            if (gatt == null)
            {
                eraseLeConnection(connection);
                setLastError(OPERATION_FAILED, "connectGatt() returned null");
                return 0;
            }

            entry.gatt = gatt;
        }
        catch (SecurityException exception)
        {
            eraseLeConnection(connection);
            setLastError(PERMISSION_DENIED, throwableMessage(exception));
            return 0;
        }
        catch (Throwable throwable)
        {
            eraseLeConnection(connection);
            setLastError(OPERATION_FAILED, throwableMessage(throwable));
            return 0;
        }

        return connection;
    }


    @Override
    public int bluetooth_le_disconnect(long connection)
    {
        LeConnectionEntry entry = getLeConnection(connection);

        if (entry == null)
            return result(INVALID_HANDLE, "Invalid BLE connection handle");

        entry.manualClosing = true;

        BluetoothGatt gatt = entry.gatt;

        if (gatt == null)
        {
            eraseLeConnection(connection);
            return result(OK, "");
        }

        try
        {
            gatt.disconnect();
            return result(OK, "");
        }
        catch (Throwable throwable)
        {
            return result(OPERATION_FAILED, throwableMessage(throwable));
        }
    }


    @Override
    public boolean bluetooth_le_connection_is_valid(long connection)
    {
        return getLeConnection(connection) != null;
    }


    @Override
    public boolean bluetooth_le_connection_is_connected(long connection)
    {
        LeConnectionEntry entry = getLeConnection(connection);
        return entry != null && entry.connected;
    }


    @Override
    public long bluetooth_le_connection_get_device(long connection)
    {
        LeConnectionEntry entry = getLeConnection(connection);
        return entry != null ? entry.device : 0;
    }


    // =========================================================================
    // BLE GATT client - discovery + enumeration
    // =========================================================================

    @Override
    public int bluetooth_le_services_discover(long connection, GMFunction callback)
    {
        if (!initialized)
            return result(NOT_INITIALIZED, "Bluetooth is not initialized");

        LeConnectionEntry connEntry = getLeConnection(connection);

        if (connEntry == null || connEntry.gatt == null)
            return result(INVALID_HANDLE, "Invalid BLE connection handle");

        if (!connEntry.connected)
            return result(DISCONNECTED, "BLE connection is not connected");

        final BluetoothGatt gatt = connEntry.gatt;

        enqueueGattOp(
            connEntry,
            () ->
            {
                LePendingOp op = new LePendingOp();
                op.kind = LePendingOp.KIND_DISCOVER_SERVICES;
                op.callback = callback;
                op.targetHandle = connection;
                connEntry.currentOp = op;

                boolean started;

                try
                {
                    started = gatt.discoverServices();
                }
                catch (Throwable throwable)
                {
                    started = false;
                }

                if (!started)
                {
                    invoke(
                        callback,
                        OPERATION_FAILED,
                        "discoverServices() failed to start",
                        (double) connection);
                    completeGattOp(connEntry);
                }
            });

        return result(OK, "");
    }


    @Override
    public int bluetooth_le_service_get_count(long connection)
    {
        LeConnectionEntry entry = getLeConnection(connection);

        if (entry == null)
            return 0;

        synchronized (entry.serviceListLock)
        {
            return entry.serviceHandles.size();
        }
    }


    @Override
    public long bluetooth_le_service_get_at(long connection, int index)
    {
        LeConnectionEntry entry = getLeConnection(connection);

        if (entry == null)
            return 0;

        synchronized (entry.serviceListLock)
        {
            if (index < 0 || index >= entry.serviceHandles.size())
                return 0;

            return entry.serviceHandles.get(index);
        }
    }


    @Override
    public String bluetooth_le_service_get_uuid(long service)
    {
        LeServiceEntry entry = getLeService(service);
        return entry != null ? entry.uuid : "";
    }


    @Override
    public int bluetooth_le_characteristics_discover(
        long service,
        GMFunction callback)
    {
        if (!initialized)
            return result(NOT_INITIALIZED, "Bluetooth is not initialized");

        LeServiceEntry entry = getLeService(service);

        if (entry == null || entry.gattService == null)
            return result(INVALID_HANDLE, "Invalid BLE service handle");

        // The complete GATT database (services, characteristics and
        // descriptors) is already downloaded by the single discoverServices()
        // round trip that already ran - there is no further remote operation
        // to perform here, so the callback fires synchronously.
        synchronized (leEntityLock)
        {
            if (!entry.characteristicsDiscovered)
            {
                entry.characteristicHandles.clear();

                for (BluetoothGattCharacteristic characteristic :
                    entry.gattService.getCharacteristics())
                {
                    entry.characteristicHandles.add(
                        findOrCreateCharacteristicHandle(
                            service,
                            entry.connection,
                            characteristic));
                }

                entry.characteristicsDiscovered = true;
            }
        }

        invoke(callback, OK, "", (double) service);
        return result(OK, "");
    }


    @Override
    public int bluetooth_le_characteristic_get_count(long service)
    {
        LeServiceEntry entry = getLeService(service);

        if (entry == null)
            return 0;

        synchronized (leEntityLock)
        {
            return entry.characteristicHandles.size();
        }
    }


    @Override
    public long bluetooth_le_characteristic_get_at(long service, int index)
    {
        LeServiceEntry entry = getLeService(service);

        if (entry == null)
            return 0;

        synchronized (leEntityLock)
        {
            if (index < 0 || index >= entry.characteristicHandles.size())
                return 0;

            return entry.characteristicHandles.get(index);
        }
    }


    @Override
    public String bluetooth_le_characteristic_get_uuid(long characteristic)
    {
        LeCharacteristicEntry entry = getLeCharacteristic(characteristic);
        return entry != null ? entry.uuid : "";
    }


    @Override
    public int bluetooth_le_characteristic_get_properties(long characteristic)
    {
        LeCharacteristicEntry entry = getLeCharacteristic(characteristic);

        if (entry == null || entry.gattCharacteristic == null)
            return 0;

        return entry.gattCharacteristic.getProperties();
    }


    @Override
    public int bluetooth_le_descriptors_discover(
        long characteristic,
        GMFunction callback)
    {
        if (!initialized)
            return result(NOT_INITIALIZED, "Bluetooth is not initialized");

        LeCharacteristicEntry entry = getLeCharacteristic(characteristic);

        if (entry == null || entry.gattCharacteristic == null)
            return result(INVALID_HANDLE, "Invalid BLE characteristic handle");

        synchronized (leEntityLock)
        {
            if (!entry.descriptorsDiscovered)
            {
                entry.descriptorHandles.clear();

                for (BluetoothGattDescriptor descriptor :
                    entry.gattCharacteristic.getDescriptors())
                {
                    entry.descriptorHandles.add(
                        findOrCreateDescriptorHandle(characteristic, descriptor));
                }

                entry.descriptorsDiscovered = true;
            }
        }

        invoke(callback, OK, "", (double) characteristic);
        return result(OK, "");
    }


    @Override
    public int bluetooth_le_descriptor_get_count(long characteristic)
    {
        LeCharacteristicEntry entry = getLeCharacteristic(characteristic);

        if (entry == null)
            return 0;

        synchronized (leEntityLock)
        {
            return entry.descriptorHandles.size();
        }
    }


    @Override
    public long bluetooth_le_descriptor_get_at(long characteristic, int index)
    {
        LeCharacteristicEntry entry = getLeCharacteristic(characteristic);

        if (entry == null)
            return 0;

        synchronized (leEntityLock)
        {
            if (index < 0 || index >= entry.descriptorHandles.size())
                return 0;

            return entry.descriptorHandles.get(index);
        }
    }


    @Override
    public String bluetooth_le_descriptor_get_uuid(long descriptor)
    {
        LeDescriptorEntry entry = getLeDescriptor(descriptor);
        return entry != null ? entry.uuid : "";
    }


    // =========================================================================
    // BLE GATT client - read / write / subscribe
    // =========================================================================

    @Override
    public int bluetooth_le_characteristic_read(
        long characteristic,
        GMFunction callback)
    {
        if (!initialized)
            return result(NOT_INITIALIZED, "Bluetooth is not initialized");

        LeCharacteristicEntry charEntry = getLeCharacteristic(characteristic);

        if (charEntry == null)
            return result(INVALID_HANDLE, "Invalid BLE characteristic handle");

        LeConnectionEntry connEntry = getLeConnection(charEntry.connection);

        if (connEntry == null || connEntry.gatt == null || !connEntry.connected)
            return result(DISCONNECTED, "BLE connection is not connected");

        final BluetoothGatt gatt = connEntry.gatt;
        final BluetoothGattCharacteristic gattCharacteristic =
            charEntry.gattCharacteristic;

        enqueueGattOp(
            connEntry,
            () ->
            {
                LePendingOp op = new LePendingOp();
                op.kind = LePendingOp.KIND_READ_CHARACTERISTIC;
                op.callback = callback;
                op.targetHandle = characteristic;
                connEntry.currentOp = op;

                boolean started;

                try
                {
                    started = gatt.readCharacteristic(gattCharacteristic);
                }
                catch (Throwable throwable)
                {
                    started = false;
                }

                if (!started)
                {
                    invoke(
                        callback,
                        OPERATION_FAILED,
                        "readCharacteristic() failed to start",
                        (double) characteristic);
                    completeGattOp(connEntry);
                }
            });

        return result(OK, "");
    }


    @Override
    public int bluetooth_le_characteristic_get_value(
        long characteristic,
        ByteBuffer out_data,
        int offset,
        int max_size)
    {
        LeCharacteristicEntry entry = getLeCharacteristic(characteristic);

        if (entry == null || entry.gattCharacteristic == null)
            return 0;

        byte[] value = entry.gattCharacteristic.getValue();

        if (value == null || value.length == 0)
            return 0;

        if (bufferRangeInvalid(out_data, offset, max_size))
        {
            setLastError(
                INVALID_ARGUMENT,
                "Invalid buffer offset/size for bluetooth_le_characteristic_get_value");
            return 0;
        }

        int copyLength = Math.min(max_size, value.length);

        if (copyLength <= 0)
            return 0;

        ByteBuffer view = out_data.duplicate();
        view.position(offset);
        view.put(value, 0, copyLength);

        return copyLength;
    }


    @Override
    public int bluetooth_le_characteristic_write(
        long characteristic,
        ByteBuffer data,
        int offset,
        int size,
        int write_type,
        GMFunction callback)
    {
        if (!initialized)
            return result(NOT_INITIALIZED, "Bluetooth is not initialized");

        LeCharacteristicEntry charEntry = getLeCharacteristic(characteristic);

        if (charEntry == null)
            return result(INVALID_HANDLE, "Invalid BLE characteristic handle");

        LeConnectionEntry connEntry = getLeConnection(charEntry.connection);

        if (connEntry == null || connEntry.gatt == null || !connEntry.connected)
            return result(DISCONNECTED, "BLE connection is not connected");

        if (bufferRangeInvalid(data, offset, size))
            return result(
                INVALID_ARGUMENT,
                "Invalid buffer offset/size for bluetooth_le_characteristic_write");

        final byte[] payload = new byte[size];

        if (size > 0)
        {
            ByteBuffer view = data.duplicate();
            view.position(offset);
            view.get(payload, 0, size);
        }

        final BluetoothGatt gatt = connEntry.gatt;
        final BluetoothGattCharacteristic gattCharacteristic =
            charEntry.gattCharacteristic;
        final int androidWriteType = write_type == 1
            ? BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
            : BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT;

        enqueueGattOp(
            connEntry,
            () ->
            {
                LePendingOp op = new LePendingOp();
                op.kind = LePendingOp.KIND_WRITE_CHARACTERISTIC;
                op.callback = callback;
                op.targetHandle = characteristic;
                connEntry.currentOp = op;

                boolean started;

                try
                {
                    gattCharacteristic.setWriteType(androidWriteType);
                    gattCharacteristic.setValue(payload);
                    started = gatt.writeCharacteristic(gattCharacteristic);
                }
                catch (Throwable throwable)
                {
                    started = false;
                }

                if (!started)
                {
                    invoke(
                        callback,
                        OPERATION_FAILED,
                        "writeCharacteristic() failed to start",
                        (double) characteristic);
                    completeGattOp(connEntry);
                }
            });

        return result(OK, "");
    }


    @Override
    public int bluetooth_le_characteristic_subscribe(
        long characteristic,
        int mode,
        GMFunction callback)
    {
        if (!initialized)
            return result(NOT_INITIALIZED, "Bluetooth is not initialized");

        LeCharacteristicEntry charEntry = getLeCharacteristic(characteristic);

        if (charEntry == null)
            return result(INVALID_HANDLE, "Invalid BLE characteristic handle");

        LeConnectionEntry connEntry = getLeConnection(charEntry.connection);

        if (connEntry == null || connEntry.gatt == null || !connEntry.connected)
            return result(DISCONNECTED, "BLE connection is not connected");

        if (mode != SUBSCRIBE_MODE_UNSUBSCRIBE &&
            mode != SUBSCRIBE_MODE_NOTIFY &&
            mode != SUBSCRIBE_MODE_INDICATE)
            return result(INVALID_ARGUMENT, "Invalid BluetoothLeSubscribeMode value");

        final BluetoothGatt gatt = connEntry.gatt;
        final BluetoothGattCharacteristic gattCharacteristic =
            charEntry.gattCharacteristic;
        final BluetoothGattDescriptor cccd =
            gattCharacteristic.getDescriptor(CCCD_UUID);

        if (cccd == null)
            return result(
                NOT_SUPPORTED,
                "Characteristic has no client characteristic configuration descriptor");

        final byte[] cccdValue;

        if (mode == SUBSCRIBE_MODE_UNSUBSCRIBE)
            cccdValue = BluetoothGattDescriptor.DISABLE_NOTIFICATION_VALUE;
        else if (mode == SUBSCRIBE_MODE_INDICATE)
            cccdValue = BluetoothGattDescriptor.ENABLE_INDICATION_VALUE;
        else
            cccdValue = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE;

        enqueueGattOp(
            connEntry,
            () ->
            {
                LePendingOp op = new LePendingOp();
                op.kind = LePendingOp.KIND_SUBSCRIBE;
                op.callback = callback;
                op.targetHandle = characteristic;
                op.subscribeMode = mode;
                connEntry.currentOp = op;

                boolean started;

                try
                {
                    // Two-step subscribe unique to Android: toggling local
                    // delivery here does not by itself tell the remote
                    // peripheral anything - only the CCCD descriptor write
                    // below does that, and this operation waits for its
                    // completion via onDescriptorWrite before resolving.
                    gatt.setCharacteristicNotification(
                        gattCharacteristic,
                        mode != SUBSCRIBE_MODE_UNSUBSCRIBE);

                    cccd.setValue(cccdValue);
                    started = gatt.writeDescriptor(cccd);
                }
                catch (Throwable throwable)
                {
                    started = false;
                }

                if (!started)
                {
                    invoke(
                        callback,
                        OPERATION_FAILED,
                        "CCCD write failed to start",
                        (double) characteristic);
                    completeGattOp(connEntry);
                }
            });

        return result(OK, "");
    }


    @Override
    public int bluetooth_le_descriptor_read(long descriptor, GMFunction callback)
    {
        if (!initialized)
            return result(NOT_INITIALIZED, "Bluetooth is not initialized");

        LeDescriptorEntry descEntry = getLeDescriptor(descriptor);

        if (descEntry == null)
            return result(INVALID_HANDLE, "Invalid BLE descriptor handle");

        LeCharacteristicEntry charEntry = getLeCharacteristic(descEntry.characteristic);

        if (charEntry == null)
            return result(INVALID_HANDLE, "Invalid BLE characteristic handle");

        LeConnectionEntry connEntry = getLeConnection(charEntry.connection);

        if (connEntry == null || connEntry.gatt == null || !connEntry.connected)
            return result(DISCONNECTED, "BLE connection is not connected");

        final BluetoothGatt gatt = connEntry.gatt;
        final BluetoothGattDescriptor gattDescriptor = descEntry.gattDescriptor;

        enqueueGattOp(
            connEntry,
            () ->
            {
                LePendingOp op = new LePendingOp();
                op.kind = LePendingOp.KIND_READ_DESCRIPTOR;
                op.callback = callback;
                op.targetHandle = descriptor;
                connEntry.currentOp = op;

                boolean started;

                try
                {
                    started = gatt.readDescriptor(gattDescriptor);
                }
                catch (Throwable throwable)
                {
                    started = false;
                }

                if (!started)
                {
                    invoke(
                        callback,
                        OPERATION_FAILED,
                        "readDescriptor() failed to start",
                        (double) descriptor);
                    completeGattOp(connEntry);
                }
            });

        return result(OK, "");
    }


    @Override
    public int bluetooth_le_descriptor_get_value(
        long descriptor,
        ByteBuffer out_data,
        int offset,
        int max_size)
    {
        LeDescriptorEntry entry = getLeDescriptor(descriptor);

        if (entry == null || entry.gattDescriptor == null)
            return 0;

        byte[] value = entry.gattDescriptor.getValue();

        if (value == null || value.length == 0)
            return 0;

        if (bufferRangeInvalid(out_data, offset, max_size))
        {
            setLastError(
                INVALID_ARGUMENT,
                "Invalid buffer offset/size for bluetooth_le_descriptor_get_value");
            return 0;
        }

        int copyLength = Math.min(max_size, value.length);

        if (copyLength <= 0)
            return 0;

        ByteBuffer view = out_data.duplicate();
        view.position(offset);
        view.put(value, 0, copyLength);

        return copyLength;
    }


    @Override
    public int bluetooth_le_descriptor_write(
        long descriptor,
        ByteBuffer data,
        int offset,
        int size,
        GMFunction callback)
    {
        if (!initialized)
            return result(NOT_INITIALIZED, "Bluetooth is not initialized");

        LeDescriptorEntry descEntry = getLeDescriptor(descriptor);

        if (descEntry == null)
            return result(INVALID_HANDLE, "Invalid BLE descriptor handle");

        LeCharacteristicEntry charEntry = getLeCharacteristic(descEntry.characteristic);

        if (charEntry == null)
            return result(INVALID_HANDLE, "Invalid BLE characteristic handle");

        LeConnectionEntry connEntry = getLeConnection(charEntry.connection);

        if (connEntry == null || connEntry.gatt == null || !connEntry.connected)
            return result(DISCONNECTED, "BLE connection is not connected");

        if (bufferRangeInvalid(data, offset, size))
            return result(
                INVALID_ARGUMENT,
                "Invalid buffer offset/size for bluetooth_le_descriptor_write");

        final byte[] payload = new byte[size];

        if (size > 0)
        {
            ByteBuffer view = data.duplicate();
            view.position(offset);
            view.get(payload, 0, size);
        }

        final BluetoothGatt gatt = connEntry.gatt;
        final BluetoothGattDescriptor gattDescriptor = descEntry.gattDescriptor;

        enqueueGattOp(
            connEntry,
            () ->
            {
                LePendingOp op = new LePendingOp();
                op.kind = LePendingOp.KIND_WRITE_DESCRIPTOR;
                op.callback = callback;
                op.targetHandle = descriptor;
                connEntry.currentOp = op;

                boolean started;

                try
                {
                    gattDescriptor.setValue(payload);
                    started = gatt.writeDescriptor(gattDescriptor);
                }
                catch (Throwable throwable)
                {
                    started = false;
                }

                if (!started)
                {
                    invoke(
                        callback,
                        OPERATION_FAILED,
                        "writeDescriptor() failed to start",
                        (double) descriptor);
                    completeGattOp(connEntry);
                }
            });

        return result(OK, "");
    }


    // =========================================================================
    // BLE advertise
    // =========================================================================

    private void stopLeAdvertiseInternal()
    {
        leAdvertising.set(false);
        leAdvertiseStartCallback = null;

        BluetoothLeAdvertiser advertiser = leAdvertiser;
        AdvertiseCallback callback = leAdvertiseCallback;
        leAdvertiseCallback = null;

        if (advertiser != null && callback != null)
        {
            try
            {
                advertiser.stopAdvertising(callback);
            }
            catch (Throwable ignored)
            {
            }
        }
    }


    private static byte[] decodeBase64(String value)
    {
        if (value == null || value.isEmpty())
            return new byte[0];

        try
        {
            return Base64.decode(value, Base64.NO_WRAP);
        }
        catch (Throwable throwable)
        {
            return new byte[0];
        }
    }


    @Override
    public int bluetooth_le_advertise_start(
        String settings_json,
        String data_json,
        GMFunction callback)
    {
        if (!initialized || adapter == null)
            return result(NOT_INITIALIZED, "Bluetooth is not initialized");

        if (!hasAdvertisePermission())
            return result(
                PERMISSION_DENIED,
                "Bluetooth advertise permission is not granted");

        if (!adapterEnabled())
            return result(BLUETOOTH_DISABLED, "Bluetooth is disabled");

        if (leAdvertising.get())
            return result(OK, "");

        BluetoothLeAdvertiser advertiser;

        try
        {
            advertiser = adapter.getBluetoothLeAdvertiser();
        }
        catch (Throwable throwable)
        {
            advertiser = null;
        }

        if (advertiser == null)
            return result(NOT_SUPPORTED, "Bluetooth LE advertising is unavailable");

        int txPowerLevel = AdvertiseSettings.ADVERTISE_TX_POWER_MEDIUM;

        try
        {
            if (settings_json != null && !settings_json.isEmpty())
            {
                JSONObject settings = new JSONObject(settings_json);

                if (settings.has("txPowerLevel"))
                    txPowerLevel = settings.getInt("txPowerLevel");
            }
        }
        catch (Throwable throwable)
        {
            return result(INVALID_ARGUMENT, "Invalid settings_json: " + throwable.getMessage());
        }

        AdvertiseData.Builder dataBuilder = new AdvertiseData.Builder();

        try
        {
            if (data_json != null && !data_json.isEmpty())
            {
                JSONObject data = new JSONObject(data_json);

                if (data.optBoolean("includeName", false))
                    dataBuilder.setIncludeDeviceName(true);

                if (data.optBoolean("includePowerLevel", false))
                    dataBuilder.setIncludeTxPowerLevel(true);

                if (data.has("services"))
                {
                    JSONArray services = data.getJSONArray("services");

                    for (int i = 0; i < services.length(); i++)
                    {
                        JSONObject service = services.getJSONObject(i);
                        ParcelUuid uuid =
                            new ParcelUuid(UUID.fromString(service.getString("uuid")));

                        if (service.has("data"))
                            dataBuilder.addServiceData(
                                uuid,
                                decodeBase64(service.getString("data")));
                        else
                            dataBuilder.addServiceUuid(uuid);
                    }
                }

                if (data.has("manufacturer"))
                {
                    JSONObject manufacturer = data.getJSONObject("manufacturer");
                    dataBuilder.addManufacturerData(
                        manufacturer.getInt("id"),
                        decodeBase64(manufacturer.optString("data", "")));
                }
            }
        }
        catch (Throwable throwable)
        {
            return result(INVALID_ARGUMENT, "Invalid data_json: " + throwable.getMessage());
        }

        AdvertiseSettings advertiseSettings = new AdvertiseSettings.Builder()
            .setAdvertiseMode(AdvertiseSettings.ADVERTISE_MODE_LOW_LATENCY)
            .setTxPowerLevel(txPowerLevel)
            .setConnectable(true)
            .build();

        leAdvertiseStartCallback = callback;

        final AdvertiseCallback advertiseCallback = new AdvertiseCallback()
        {
            @Override
            public void onStartSuccess(AdvertiseSettings settingsInEffect)
            {
                leAdvertising.set(true);

                GMFunction startCallback = leAdvertiseStartCallback;
                leAdvertiseStartCallback = null;

                invoke(startCallback, OK, "");
            }


            @Override
            public void onStartFailure(int errorCode)
            {
                leAdvertising.set(false);
                leAdvertiseCallback = null;

                GMFunction startCallback = leAdvertiseStartCallback;
                leAdvertiseStartCallback = null;

                invoke(
                    startCallback,
                    OPERATION_FAILED,
                    "Advertise start failed: " + errorCode);
            }
        };

        leAdvertiser = advertiser;
        leAdvertiseCallback = advertiseCallback;

        try
        {
            advertiser.startAdvertising(
                advertiseSettings,
                dataBuilder.build(),
                advertiseCallback);
        }
        catch (Throwable throwable)
        {
            leAdvertiseCallback = null;
            leAdvertiseStartCallback = null;
            return result(OPERATION_FAILED, throwableMessage(throwable));
        }

        return result(OK, "");
    }


    @Override
    public int bluetooth_le_advertise_stop()
    {
        if (!initialized)
            return result(NOT_INITIALIZED, "Bluetooth is not initialized");

        stopLeAdvertiseInternal();
        return result(OK, "");
    }


    @Override
    public boolean bluetooth_le_advertise_is_running()
    {
        return initialized && leAdvertising.get();
    }


    // =========================================================================
    // BLE GATT server
    // =========================================================================

    private void handleServerConnectionStateChange(
        BluetoothDevice device,
        int status,
        int newState)
    {
        long deviceHandle = upsertDevice(
            TRANSPORT_LE,
            deviceId(TRANSPORT_LE, device),
            safeName(device),
            safeAddress(device),
            0,
            false,
            true,
            device);

        if (newState == BluetoothProfile.STATE_CONNECTED)
        {
            Long existing;

            synchronized (leConnectionLock)
            {
                existing = leServerConnectionByDevice.get(deviceHandle);
            }

            long connection;

            if (existing != null)
            {
                connection = existing;
            }
            else
            {
                connection = createLeConnection(deviceHandle, true);

                LeConnectionEntry entry = getLeConnection(connection);

                if (entry != null)
                {
                    entry.remoteDevice = device;
                    entry.connected = true;
                }

                synchronized (leConnectionLock)
                {
                    leServerConnectionByDevice.put(deviceHandle, connection);
                }
            }

            dispatchLeServerConnectionStateChanged(connection, true, deviceHandle);
        }
        else if (newState == BluetoothProfile.STATE_DISCONNECTED)
        {
            Long connection;

            synchronized (leConnectionLock)
            {
                connection = leServerConnectionByDevice.remove(deviceHandle);
            }

            if (connection != null)
            {
                eraseLeConnection(connection);
                dispatchLeServerConnectionStateChanged(connection, false, deviceHandle);
            }
        }
    }


    private long resolveServerConnection(BluetoothDevice device)
    {
        long deviceHandle = upsertDevice(
            TRANSPORT_LE,
            deviceId(TRANSPORT_LE, device),
            safeName(device),
            safeAddress(device),
            0,
            false,
            true,
            device);

        synchronized (leConnectionLock)
        {
            Long connection = leServerConnectionByDevice.get(deviceHandle);
            return connection != null ? connection : 0;
        }
    }


    private static String characteristicServiceUuid(BluetoothGattCharacteristic characteristic)
    {
        return characteristic != null &&
            characteristic.getService() != null &&
            characteristic.getService().getUuid() != null
            ? characteristic.getService().getUuid().toString()
            : "";
    }


    private static String characteristicUuidOf(BluetoothGattCharacteristic characteristic)
    {
        return characteristic != null && characteristic.getUuid() != null
            ? characteristic.getUuid().toString()
            : "";
    }


    private BluetoothGattServerCallback createGattServerCallback(final long workerGeneration)
    {
        return new BluetoothGattServerCallback()
        {
            @Override
            public void onConnectionStateChange(
                BluetoothDevice device,
                int status,
                int newState)
            {
                if (generation.get() != workerGeneration)
                    return;

                handleServerConnectionStateChange(device, status, newState);
            }


            @Override
            public void onServiceAdded(int status, BluetoothGattService service)
            {
                if (generation.get() != workerGeneration)
                    return;

                String uuid = service != null && service.getUuid() != null
                    ? service.getUuid().toString()
                    : "";

                GMFunction callback;

                synchronized (leServerAddServiceLock)
                {
                    callback = leServerAddServiceCallbacks.remove(uuid);
                }

                if (status == BluetoothGatt.GATT_SUCCESS)
                    invoke(callback, OK, "");
                else
                    invoke(
                        callback,
                        OPERATION_FAILED,
                        "onServiceAdded status " + status);
            }


            @Override
            public void onCharacteristicReadRequest(
                BluetoothDevice device,
                int requestId,
                int offset,
                BluetoothGattCharacteristic characteristic)
            {
                if (generation.get() != workerGeneration)
                    return;

                long connection = resolveServerConnection(device);
                String serviceUuid = characteristicServiceUuid(characteristic);
                String characteristicUuid = characteristicUuidOf(characteristic);

                LeServerRequestEntry request = new LeServerRequestEntry();
                request.requestId = requestId;
                request.connection = connection;
                request.device = device;
                request.serviceUuid = serviceUuid;
                request.characteristicUuid = characteristicUuid;
                request.isWrite = false;
                request.responseNeeded = true;

                synchronized (leServerRequestLock)
                {
                    leServerRequests.put(requestId, request);
                }

                invoke(
                    callbackLeServerReadRequest,
                    requestId,
                    (double) connection,
                    serviceUuid,
                    characteristicUuid,
                    "",
                    offset);
            }


            @Override
            public void onCharacteristicWriteRequest(
                BluetoothDevice device,
                int requestId,
                BluetoothGattCharacteristic characteristic,
                boolean preparedWrite,
                boolean responseNeeded,
                int offset,
                byte[] value)
            {
                if (generation.get() != workerGeneration)
                    return;

                long connection = resolveServerConnection(device);
                String serviceUuid = characteristicServiceUuid(characteristic);
                String characteristicUuid = characteristicUuidOf(characteristic);

                LeServerRequestEntry request = new LeServerRequestEntry();
                request.requestId = requestId;
                request.connection = connection;
                request.device = device;
                request.serviceUuid = serviceUuid;
                request.characteristicUuid = characteristicUuid;
                request.isWrite = true;
                request.responseNeeded = responseNeeded;
                request.writeValue = value != null ? value : new byte[0];

                synchronized (leServerRequestLock)
                {
                    leServerRequests.put(requestId, request);
                }

                invoke(
                    callbackLeServerWriteRequest,
                    requestId,
                    (double) connection,
                    serviceUuid,
                    characteristicUuid,
                    "");

                if (!responseNeeded)
                {
                    synchronized (leServerRequestLock)
                    {
                        leServerRequests.remove(requestId);
                    }
                }
            }


            @Override
            public void onDescriptorReadRequest(
                BluetoothDevice device,
                int requestId,
                int offset,
                BluetoothGattDescriptor descriptor)
            {
                if (generation.get() != workerGeneration)
                    return;

                BluetoothGattCharacteristic parent =
                    descriptor != null ? descriptor.getCharacteristic() : null;

                long connection = resolveServerConnection(device);
                String serviceUuid = characteristicServiceUuid(parent);
                String characteristicUuid = characteristicUuidOf(parent);
                String descriptorUuid = descriptor != null && descriptor.getUuid() != null
                    ? descriptor.getUuid().toString()
                    : "";

                LeServerRequestEntry request = new LeServerRequestEntry();
                request.requestId = requestId;
                request.connection = connection;
                request.device = device;
                request.serviceUuid = serviceUuid;
                request.characteristicUuid = characteristicUuid;
                request.descriptorUuid = descriptorUuid;
                request.isWrite = false;
                request.responseNeeded = true;

                synchronized (leServerRequestLock)
                {
                    leServerRequests.put(requestId, request);
                }

                invoke(
                    callbackLeServerReadRequest,
                    requestId,
                    (double) connection,
                    serviceUuid,
                    characteristicUuid,
                    descriptorUuid,
                    offset);
            }


            @Override
            public void onDescriptorWriteRequest(
                BluetoothDevice device,
                int requestId,
                BluetoothGattDescriptor descriptor,
                boolean preparedWrite,
                boolean responseNeeded,
                int offset,
                byte[] value)
            {
                if (generation.get() != workerGeneration)
                    return;

                BluetoothGattCharacteristic parent =
                    descriptor != null ? descriptor.getCharacteristic() : null;

                String serviceUuid = characteristicServiceUuid(parent);
                String characteristicUuid = characteristicUuidOf(parent);

                if (descriptor != null && CCCD_UUID.equals(descriptor.getUuid()))
                {
                    // Android's own GATT server API does not track
                    // notify/indicate subscribers - intercept and
                    // auto-acknowledge the CCCD write here rather than
                    // surfacing it to GML as a normal write request.
                    long connection = resolveServerConnection(device);
                    int mode = SUBSCRIBE_MODE_UNSUBSCRIBE;

                    if (value != null &&
                        Arrays.equals(value, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE))
                        mode = SUBSCRIBE_MODE_NOTIFY;
                    else if (value != null &&
                        Arrays.equals(value, BluetoothGattDescriptor.ENABLE_INDICATION_VALUE))
                        mode = SUBSCRIBE_MODE_INDICATE;

                    String key = subscriberKey(serviceUuid, characteristicUuid);

                    synchronized (leServerSubscriberLock)
                    {
                        HashSet<Long> subscribers = leServerSubscribers.get(key);

                        if (mode == SUBSCRIBE_MODE_UNSUBSCRIBE)
                        {
                            if (subscribers != null)
                                subscribers.remove(connection);
                        }
                        else
                        {
                            if (subscribers == null)
                            {
                                subscribers = new HashSet<>();
                                leServerSubscribers.put(key, subscribers);
                            }

                            subscribers.add(connection);
                        }
                    }

                    if (responseNeeded)
                    {
                        BluetoothGattServer server = gattServer;

                        if (server != null)
                        {
                            try
                            {
                                server.sendResponse(
                                    device,
                                    requestId,
                                    BluetoothGatt.GATT_SUCCESS,
                                    offset,
                                    value);
                            }
                            catch (Throwable ignored)
                            {
                            }
                        }
                    }

                    return;
                }

                long connection = resolveServerConnection(device);
                String descriptorUuid = descriptor != null && descriptor.getUuid() != null
                    ? descriptor.getUuid().toString()
                    : "";

                LeServerRequestEntry request = new LeServerRequestEntry();
                request.requestId = requestId;
                request.connection = connection;
                request.device = device;
                request.serviceUuid = serviceUuid;
                request.characteristicUuid = characteristicUuid;
                request.descriptorUuid = descriptorUuid;
                request.isWrite = true;
                request.responseNeeded = responseNeeded;
                request.writeValue = value != null ? value : new byte[0];

                synchronized (leServerRequestLock)
                {
                    leServerRequests.put(requestId, request);
                }

                invoke(
                    callbackLeServerWriteRequest,
                    requestId,
                    (double) connection,
                    serviceUuid,
                    characteristicUuid,
                    descriptorUuid);

                if (!responseNeeded)
                {
                    synchronized (leServerRequestLock)
                    {
                        leServerRequests.remove(requestId);
                    }
                }
            }
        };
    }


    private void stopLeServerInternal()
    {
        leServerRunning.set(false);

        BluetoothGattServer server = gattServer;
        gattServer = null;

        if (server != null)
        {
            try
            {
                server.close();
            }
            catch (Throwable ignored)
            {
            }
        }

        ArrayList<Long> serverConnections = new ArrayList<>();

        synchronized (leConnectionLock)
        {
            serverConnections.addAll(leServerConnectionByDevice.values());
            leServerConnectionByDevice.clear();
        }

        for (long connection : serverConnections)
            eraseLeConnection(connection);

        synchronized (leServerRequestLock)
        {
            leServerRequests.clear();
        }

        synchronized (leServerSubscriberLock)
        {
            leServerSubscribers.clear();
        }

        synchronized (leServerAddServiceLock)
        {
            leServerAddServiceCallbacks.clear();
        }
    }


    @Override
    public int bluetooth_le_server_start()
    {
        if (!initialized || adapter == null)
            return result(NOT_INITIALIZED, "Bluetooth is not initialized");

        if (!hasConnectPermission())
            return result(
                PERMISSION_DENIED,
                "Bluetooth connect permission is not granted");

        if (!adapterEnabled())
            return result(BLUETOOTH_DISABLED, "Bluetooth is disabled");

        if (leServerRunning.get())
            return result(OK, "");

        Context current = context();

        if (current == null)
            return result(
                OPERATION_FAILED,
                "Android application context is unavailable");

        try
        {
            BluetoothManager manager = (BluetoothManager)
                current.getSystemService(Context.BLUETOOTH_SERVICE);

            if (manager == null)
                return result(NOT_SUPPORTED, "Bluetooth manager is unavailable");

            final long workerGeneration = generation.get();

            BluetoothGattServer server = manager.openGattServer(
                current,
                createGattServerCallback(workerGeneration));

            if (server == null)
                return result(
                    NOT_SUPPORTED,
                    "Bluetooth LE peripheral role is unavailable");

            gattServer = server;
            leServerRunning.set(true);

            return result(OK, "");
        }
        catch (SecurityException exception)
        {
            return result(PERMISSION_DENIED, throwableMessage(exception));
        }
        catch (Throwable throwable)
        {
            return result(OPERATION_FAILED, throwableMessage(throwable));
        }
    }


    @Override
    public int bluetooth_le_server_stop()
    {
        if (!initialized)
            return result(NOT_INITIALIZED, "Bluetooth is not initialized");

        stopLeServerInternal();
        return result(OK, "");
    }


    @Override
    public boolean bluetooth_le_server_is_running()
    {
        return initialized && leServerRunning.get();
    }


    @Override
    public int bluetooth_le_server_add_service(String service_json, GMFunction callback)
    {
        if (!initialized)
            return result(NOT_INITIALIZED, "Bluetooth is not initialized");

        if (!leServerRunning.get() || gattServer == null)
            return result(OPERATION_FAILED, "BLE server is not running");

        if (service_json == null || service_json.isEmpty())
            return result(INVALID_ARGUMENT, "service_json cannot be empty");

        final BluetoothGattService gattService;
        final String serviceUuid;

        try
        {
            JSONObject serviceObject = new JSONObject(service_json);
            serviceUuid = serviceObject.getString("uuid");

            gattService = new BluetoothGattService(
                UUID.fromString(serviceUuid),
                BluetoothGattService.SERVICE_TYPE_PRIMARY);

            JSONArray characteristics = serviceObject.optJSONArray("characteristics");

            if (characteristics != null)
            {
                for (int i = 0; i < characteristics.length(); i++)
                {
                    JSONObject characteristicObject = characteristics.getJSONObject(i);

                    BluetoothGattCharacteristic characteristic =
                        new BluetoothGattCharacteristic(
                            UUID.fromString(characteristicObject.getString("uuid")),
                            characteristicObject.getInt("properties"),
                            characteristicObject.getInt("permissions"));

                    if (characteristicObject.has("value"))
                        characteristic.setValue(
                            decodeBase64(characteristicObject.getString("value")));

                    JSONArray descriptors = characteristicObject.optJSONArray("descriptors");

                    if (descriptors != null)
                    {
                        for (int d = 0; d < descriptors.length(); d++)
                        {
                            JSONObject descriptorObject = descriptors.getJSONObject(d);

                            characteristic.addDescriptor(
                                new BluetoothGattDescriptor(
                                    UUID.fromString(descriptorObject.getString("uuid")),
                                    BluetoothGattDescriptor.PERMISSION_READ |
                                        BluetoothGattDescriptor.PERMISSION_WRITE));
                        }
                    }

                    boolean supportsNotifyOrIndicate =
                        (characteristic.getProperties() &
                            (BluetoothGattCharacteristic.PROPERTY_NOTIFY |
                                BluetoothGattCharacteristic.PROPERTY_INDICATE)) != 0;

                    if (supportsNotifyOrIndicate &&
                        characteristic.getDescriptor(CCCD_UUID) == null)
                    {
                        // Android requires an explicit CCCD for
                        // notify/indicate to function - add one
                        // automatically if the caller's JSON omitted it.
                        characteristic.addDescriptor(
                            new BluetoothGattDescriptor(
                                CCCD_UUID,
                                BluetoothGattDescriptor.PERMISSION_READ |
                                    BluetoothGattDescriptor.PERMISSION_WRITE));
                    }

                    gattService.addCharacteristic(characteristic);
                }
            }
        }
        catch (Throwable throwable)
        {
            return result(
                INVALID_ARGUMENT,
                "Invalid service_json: " + throwableMessage(throwable));
        }

        synchronized (leServerAddServiceLock)
        {
            leServerAddServiceCallbacks.put(serviceUuid, callback);
        }

        try
        {
            boolean started = gattServer.addService(gattService);

            if (!started)
            {
                synchronized (leServerAddServiceLock)
                {
                    leServerAddServiceCallbacks.remove(serviceUuid);
                }

                return result(OPERATION_FAILED, "addService() failed to start");
            }
        }
        catch (Throwable throwable)
        {
            synchronized (leServerAddServiceLock)
            {
                leServerAddServiceCallbacks.remove(serviceUuid);
            }

            return result(OPERATION_FAILED, throwableMessage(throwable));
        }

        return result(OK, "");
    }


    @Override
    public int bluetooth_le_server_clear_services()
    {
        if (!initialized)
            return result(NOT_INITIALIZED, "Bluetooth is not initialized");

        BluetoothGattServer server = gattServer;

        if (server == null)
            return result(OPERATION_FAILED, "BLE server is not running");

        try
        {
            server.clearServices();

            synchronized (leServerAddServiceLock)
            {
                leServerAddServiceCallbacks.clear();
            }

            return result(OK, "");
        }
        catch (Throwable throwable)
        {
            return result(OPERATION_FAILED, throwableMessage(throwable));
        }
    }


    @Override
    public int bluetooth_le_server_respond_read(
        int request_id,
        int error_code,
        ByteBuffer data,
        int offset,
        int size)
    {
        if (!initialized)
            return result(NOT_INITIALIZED, "Bluetooth is not initialized");

        LeServerRequestEntry request;

        synchronized (leServerRequestLock)
        {
            request = leServerRequests.remove(request_id);
        }

        if (request == null)
            return result(INVALID_ARGUMENT, "Unknown request_id");

        BluetoothGattServer server = gattServer;

        if (server == null)
            return result(OPERATION_FAILED, "BLE server is not running");

        byte[] payload = new byte[0];

        if (error_code == OK && size > 0)
        {
            if (bufferRangeInvalid(data, offset, size))
                return result(
                    INVALID_ARGUMENT,
                    "Invalid buffer offset/size for bluetooth_le_server_respond_read");

            payload = new byte[size];

            ByteBuffer view = data.duplicate();
            view.position(offset);
            view.get(payload, 0, size);
        }

        int status = error_code == OK
            ? BluetoothGatt.GATT_SUCCESS
            : BluetoothGatt.GATT_FAILURE;

        try
        {
            // The response payload above is already the exact slice the
            // caller intends to answer with, so the ATT-level offset
            // handed back to Android is always 0.
            server.sendResponse(request.device, request_id, status, 0, payload);
            return result(OK, "");
        }
        catch (Throwable throwable)
        {
            return result(OPERATION_FAILED, throwableMessage(throwable));
        }
    }


    @Override
    public int bluetooth_le_server_respond_write(int request_id, int error_code)
    {
        if (!initialized)
            return result(NOT_INITIALIZED, "Bluetooth is not initialized");

        LeServerRequestEntry request;

        synchronized (leServerRequestLock)
        {
            request = leServerRequests.remove(request_id);
        }

        if (request == null)
            return result(INVALID_ARGUMENT, "Unknown request_id");

        BluetoothGattServer server = gattServer;

        if (server == null)
            return result(OPERATION_FAILED, "BLE server is not running");

        if (!request.responseNeeded)
            return result(OK, "");

        int status = error_code == OK
            ? BluetoothGatt.GATT_SUCCESS
            : BluetoothGatt.GATT_FAILURE;

        try
        {
            server.sendResponse(request.device, request_id, status, 0, request.writeValue);
            return result(OK, "");
        }
        catch (Throwable throwable)
        {
            return result(OPERATION_FAILED, throwableMessage(throwable));
        }
    }


    @Override
    public int bluetooth_le_server_write_request_get_value(
        int request_id,
        ByteBuffer out_data,
        int offset,
        int max_size)
    {
        LeServerRequestEntry request;

        synchronized (leServerRequestLock)
        {
            request = leServerRequests.get(request_id);
        }

        if (request == null || !request.isWrite)
            return 0;

        byte[] value = request.writeValue;

        if (value == null || value.length == 0)
            return 0;

        if (bufferRangeInvalid(out_data, offset, max_size))
        {
            setLastError(
                INVALID_ARGUMENT,
                "Invalid buffer offset/size for bluetooth_le_server_write_request_get_value");
            return 0;
        }

        int copyLength = Math.min(max_size, value.length);

        if (copyLength <= 0)
            return 0;

        ByteBuffer view = out_data.duplicate();
        view.position(offset);
        view.put(value, 0, copyLength);

        return copyLength;
    }


    @Override
    public int bluetooth_le_server_notify_value(
        String service_uuid,
        String characteristic_uuid,
        long connection,
        ByteBuffer data,
        int offset,
        int size)
    {
        if (!initialized)
            return result(NOT_INITIALIZED, "Bluetooth is not initialized");

        BluetoothGattServer server = gattServer;

        if (server == null)
            return result(OPERATION_FAILED, "BLE server is not running");

        BluetoothGattService service;
        BluetoothGattCharacteristic characteristic;

        try
        {
            service = server.getService(UUID.fromString(service_uuid));
        }
        catch (Throwable throwable)
        {
            return result(INVALID_ARGUMENT, "Invalid service_uuid");
        }

        if (service == null)
            return result(INVALID_HANDLE, "Unknown local service_uuid");

        try
        {
            characteristic = service.getCharacteristic(UUID.fromString(characteristic_uuid));
        }
        catch (Throwable throwable)
        {
            return result(INVALID_ARGUMENT, "Invalid characteristic_uuid");
        }

        if (characteristic == null)
            return result(INVALID_HANDLE, "Unknown local characteristic_uuid");

        if (bufferRangeInvalid(data, offset, size))
            return result(
                INVALID_ARGUMENT,
                "Invalid buffer offset/size for bluetooth_le_server_notify_value");

        byte[] payload = new byte[size];

        if (size > 0)
        {
            ByteBuffer view = data.duplicate();
            view.position(offset);
            view.get(payload, 0, size);
        }

        characteristic.setValue(payload);

        ArrayList<Long> targets = new ArrayList<>();

        if (connection != 0)
        {
            targets.add(connection);
        }
        else
        {
            String key = subscriberKey(service_uuid, characteristic_uuid);

            synchronized (leServerSubscriberLock)
            {
                HashSet<Long> subscribers = leServerSubscribers.get(key);

                if (subscribers != null)
                    targets.addAll(subscribers);
            }
        }

        if (targets.isEmpty())
            return result(OK, "");

        boolean indicate =
            (characteristic.getProperties() & BluetoothGattCharacteristic.PROPERTY_INDICATE) != 0 &&
            (characteristic.getProperties() & BluetoothGattCharacteristic.PROPERTY_NOTIFY) == 0;

        boolean anySent = false;

        for (long targetConnection : targets)
        {
            LeConnectionEntry entry = getLeConnection(targetConnection);

            if (entry == null || entry.remoteDevice == null || !entry.connected)
                continue;

            try
            {
                if (server.notifyCharacteristicChanged(
                    entry.remoteDevice,
                    characteristic,
                    indicate))
                    anySent = true;
            }
            catch (Throwable ignored)
            {
            }
        }

        return result(OK, "");
    }


    // =========================================================================
    // Classic discovery
    // =========================================================================

    private final BroadcastReceiver classicReceiver = new BroadcastReceiver()
    {
        @Override
        public void onReceive(Context receiverContext, Intent intent)
        {
            if (!initialized || intent == null)
                return;

            String action = intent.getAction();

            if (BluetoothDevice.ACTION_FOUND.equals(action))
            {
                BluetoothDevice device;

                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU)
                {
                    device = intent.getParcelableExtra(
                        BluetoothDevice.EXTRA_DEVICE,
                        BluetoothDevice.class);
                }
                else
                {
                    //noinspection deprecation
                    device = intent.getParcelableExtra(
                        BluetoothDevice.EXTRA_DEVICE);
                }

                if (device == null)
                    return;

                short rssi = intent.getShortExtra(
                    BluetoothDevice.EXTRA_RSSI,
                    Short.MIN_VALUE);

                upsertDevice(
                    TRANSPORT_CLASSIC,
                    deviceId(TRANSPORT_CLASSIC, device),
                    safeName(device),
                    safeAddress(device),
                    rssi,
                    rssi != Short.MIN_VALUE,
                    true,
                    device);
            }
            else if (
                BluetoothAdapter.ACTION_DISCOVERY_FINISHED.equals(action))
            {
                if (classicScanning.getAndSet(false))
                    dispatchScanStopped(
                        TRANSPORT_CLASSIC,
                        OK,
                        "");
            }
        }
    };


    private void ensureClassicReceiver()
    {
        if (receiverRegistered)
            return;

        Activity current = activity();

        if (current == null)
            return;

        IntentFilter filter = new IntentFilter();
        filter.addAction(BluetoothDevice.ACTION_FOUND);
        filter.addAction(BluetoothAdapter.ACTION_DISCOVERY_FINISHED);

        //noinspection deprecation
        current.registerReceiver(classicReceiver, filter);
        receiverRegistered = true;
    }


    private void unregisterClassicReceiver()
    {
        if (!receiverRegistered)
            return;

        Activity current = activity();

        if (current != null)
        {
            try
            {
                current.unregisterReceiver(classicReceiver);
            }
            catch (Throwable ignored)
            {
            }
        }

        receiverRegistered = false;
    }


    @Override
    public int bluetooth_classic_scan_start()
    {
        if (!initialized || adapter == null)
            return result(
                NOT_INITIALIZED,
                "Bluetooth is not initialized");

        if (!hasScanPermission() || !hasConnectPermission())
            return result(
                PERMISSION_DENIED,
                "Bluetooth scan/connect permission is not granted");

        if (!adapterEnabled())
            return result(
                BLUETOOTH_DISABLED,
                "Bluetooth is disabled");

        if (classicScanning.get())
            return result(OK, "");

        try
        {
            ensureClassicReceiver();

            Set<BluetoothDevice> bonded = adapter.getBondedDevices();

            if (bonded != null)
            {
                for (BluetoothDevice device : bonded)
                {
                    upsertDevice(
                        TRANSPORT_CLASSIC,
                        deviceId(TRANSPORT_CLASSIC, device),
                        safeName(device),
                        safeAddress(device),
                        0,
                        false,
                        true,
                        device);
                }
            }

            if (adapter.isDiscovering())
                adapter.cancelDiscovery();

            if (!adapter.startDiscovery())
                return result(
                    OPERATION_FAILED,
                    "Android Bluetooth discovery could not start");

            classicScanning.set(true);
            return result(OK, "");
        }
        catch (SecurityException exception)
        {
            return result(
                PERMISSION_DENIED,
                throwableMessage(exception));
        }
        catch (Throwable throwable)
        {
            return result(
                OPERATION_FAILED,
                throwableMessage(throwable));
        }
    }


    @Override
    public int bluetooth_classic_scan_stop()
    {
        if (!initialized)
            return result(
                NOT_INITIALIZED,
                "Bluetooth is not initialized");

        if (!classicScanning.get())
            return result(OK, "");

        try
        {
            if (adapter != null && adapter.isDiscovering())
                adapter.cancelDiscovery();
        }
        catch (Throwable ignored)
        {
        }

        if (classicScanning.getAndSet(false))
            dispatchScanStopped(
                TRANSPORT_CLASSIC,
                OK,
                "");

        return result(OK, "");
    }


    @Override
    public boolean bluetooth_classic_scan_is_running()
    {
        return initialized && classicScanning.get();
    }


    // =========================================================================
    // Device cache
    // =========================================================================

    @Override
    public void bluetooth_device_clear()
    {
        synchronized (deviceLock)
        {
            deviceById.clear();
            devices.clear();
            deviceOrder.clear();
        }
    }


    @Override
    public int bluetooth_device_get_count()
    {
        synchronized (deviceLock)
        {
            return deviceOrder.size();
        }
    }


    @Override
    public long bluetooth_device_get_at(int index)
    {
        synchronized (deviceLock)
        {
            if (index < 0 || index >= deviceOrder.size())
                return 0;

            return deviceOrder.get(index);
        }
    }


    @Override
    public boolean bluetooth_device_is_valid(long device)
    {
        return copyDevice(device) != null;
    }


    @Override
    public int bluetooth_device_get_transport(long device)
    {
        DeviceEntry entry = copyDevice(device);
        return entry != null ? entry.transport : TRANSPORT_UNKNOWN;
    }


    @Override
    public String bluetooth_device_get_id(long device)
    {
        DeviceEntry entry = copyDevice(device);
        return entry != null ? entry.id : "";
    }


    @Override
    public String bluetooth_device_get_name(long device)
    {
        DeviceEntry entry = copyDevice(device);
        return entry != null ? entry.name : "";
    }


    @Override
    public boolean bluetooth_device_has_address(long device)
    {
        DeviceEntry entry = copyDevice(device);
        return entry != null && entry.addressAvailable;
    }


    @Override
    public String bluetooth_device_get_address(long device)
    {
        DeviceEntry entry = copyDevice(device);
        return entry != null && entry.addressAvailable
            ? entry.address
            : "";
    }


    @Override
    public boolean bluetooth_device_has_rssi(long device)
    {
        DeviceEntry entry = copyDevice(device);
        return entry != null && entry.rssiAvailable;
    }


    @Override
    public int bluetooth_device_get_rssi(long device)
    {
        DeviceEntry entry = copyDevice(device);
        return entry != null && entry.rssiAvailable
            ? entry.rssi
            : 0;
    }


    @Override
    public boolean bluetooth_device_is_connectable(long device)
    {
        DeviceEntry entry = copyDevice(device);
        return entry != null && entry.connectable;
    }


    // =========================================================================
    // Classic RFCOMM client
    // =========================================================================

    @Override
    public long bluetooth_classic_connect(
        long device,
        String service_uuid,
        GMFunction callback)
    {
        if (!initialized || adapter == null)
        {
            setLastError(
                NOT_INITIALIZED,
                "Bluetooth is not initialized");
            return 0;
        }

        DeviceEntry deviceEntry = copyDevice(device);

        if (
            deviceEntry == null ||
            deviceEntry.transport != TRANSPORT_CLASSIC)
        {
            setLastError(
                INVALID_HANDLE,
                "Expected a Bluetooth Classic device handle");
            return 0;
        }

        if (service_uuid == null || service_uuid.isEmpty())
        {
            setLastError(
                INVALID_ARGUMENT,
                "service_uuid cannot be empty");
            return 0;
        }

        if (!hasConnectPermission())
        {
            setLastError(
                PERMISSION_DENIED,
                "Bluetooth connect permission is not granted");
            return 0;
        }

        if (!adapterEnabled())
        {
            setLastError(
                BLUETOOTH_DISABLED,
                "Bluetooth is disabled");
            return 0;
        }

        final UUID uuid;

        try
        {
            uuid = UUID.fromString(service_uuid);
        }
        catch (Throwable throwable)
        {
            setLastError(
                INVALID_ARGUMENT,
                "service_uuid is not a valid UUID");
            return 0;
        }

        final long connection = createConnection(device);
        final long workerGeneration = generation.get();

        setLastError(OK, "");

        Thread thread = new Thread(
            () ->
            {
                BluetoothSocket socket = null;

                try
                {
                    if (
                        !initialized ||
                        generation.get() != workerGeneration)
                    {
                        return;
                    }

                    try
                    {
                        if (adapter.isDiscovering())
                            adapter.cancelDiscovery();
                    }
                    catch (Throwable ignored)
                    {
                    }

                    BluetoothDevice androidDevice =
                        deviceEntry.androidDevice;

                    if (androidDevice == null)
                    {
                        if (
                            deviceEntry.address == null ||
                            deviceEntry.address.isEmpty())
                        {
                            dispatchConnectResult(
                                connection,
                                device,
                                INVALID_ARGUMENT,
                                "Bluetooth Classic device has no usable address",
                                callback);
                            return;
                        }

                        androidDevice =
                            adapter.getRemoteDevice(
                                deviceEntry.address);
                    }

                    socket =
                        androidDevice.createRfcommSocketToServiceRecord(
                            uuid);

                    ConnectionEntry connectionEntry =
                        getConnection(connection);

                    if (connectionEntry == null)
                    {
                        try
                        {
                            socket.close();
                        }
                        catch (Throwable ignored)
                        {
                        }
                        return;
                    }

                    connectionEntry.socket = socket;
                    socket.connect();

                    if (
                        !initialized ||
                        generation.get() != workerGeneration)
                    {
                        try
                        {
                            socket.close();
                        }
                        catch (Throwable ignored)
                        {
                        }
                        return;
                    }

                    dispatchConnectResult(
                        connection,
                        device,
                        OK,
                        "",
                        callback);

                    startReadLoop(
                        connection,
                        socket,
                        workerGeneration);
                }
                catch (SecurityException exception)
                {
                    if (socket != null)
                    {
                        try
                        {
                            socket.close();
                        }
                        catch (Throwable ignored)
                        {
                        }
                    }

                    if (generation.get() == workerGeneration)
                    {
                        dispatchConnectResult(
                            connection,
                            device,
                            PERMISSION_DENIED,
                            throwableMessage(exception),
                            callback);
                    }
                }
                catch (IOException exception)
                {
                    if (socket != null)
                    {
                        try
                        {
                            socket.close();
                        }
                        catch (Throwable ignored)
                        {
                        }
                    }

                    if (generation.get() == workerGeneration)
                    {
                        dispatchConnectResult(
                            connection,
                            device,
                            CONNECTION_FAILED,
                            throwableMessage(exception),
                            callback);
                    }
                }
                catch (Throwable throwable)
                {
                    if (socket != null)
                    {
                        try
                        {
                            socket.close();
                        }
                        catch (Throwable ignored)
                        {
                        }
                    }

                    if (generation.get() == workerGeneration)
                    {
                        dispatchConnectResult(
                            connection,
                            device,
                            OPERATION_FAILED,
                            throwableMessage(throwable),
                            callback);
                    }
                }
            },
            "GMBluetooth-RFCOMM-Connect-" + connection);

        thread.setDaemon(true);
        thread.start();

        return connection;
    }


    private void startReadLoop(
        final long connection,
        final BluetoothSocket socket,
        final long workerGeneration)
    {
        Thread thread = new Thread(
            () ->
            {
                try
                {
                    InputStream input = socket.getInputStream();
                    byte[] buffer = new byte[4096];

                    while (
                        initialized &&
                        generation.get() == workerGeneration)
                    {
                        int count = input.read(buffer);

                        if (count < 0)
                            break;

                        if (count == 0)
                            continue;

                        appendReceived(
                            connection,
                            Arrays.copyOf(buffer, count));
                    }

                    ConnectionEntry entry =
                        getConnection(connection);

                    boolean manual =
                        entry != null && entry.manualClosing;

                    if (
                        initialized &&
                        generation.get() == workerGeneration)
                    {
                        dispatchDisconnected(
                            connection,
                            manual ? OK : DISCONNECTED,
                            manual
                                ? "Disconnected"
                                : "Remote device disconnected");
                    }
                }
                catch (IOException exception)
                {
                    ConnectionEntry entry =
                        getConnection(connection);

                    boolean manual =
                        entry != null && entry.manualClosing;

                    if (
                        initialized &&
                        generation.get() == workerGeneration)
                    {
                        dispatchDisconnected(
                            connection,
                            manual ? OK : DISCONNECTED,
                            manual
                                ? "Disconnected"
                                : throwableMessage(exception));
                    }
                }
                catch (Throwable throwable)
                {
                    if (
                        initialized &&
                        generation.get() == workerGeneration)
                    {
                        dispatchDisconnected(
                            connection,
                            OPERATION_FAILED,
                            throwableMessage(throwable));
                    }
                }
                finally
                {
                    try
                    {
                        socket.close();
                    }
                    catch (Throwable ignored)
                    {
                    }
                }
            },
            "GMBluetooth-RFCOMM-Read-" + connection);

        thread.setDaemon(true);
        thread.start();
    }


    @Override
    public int bluetooth_classic_disconnect(long connection)
    {
        ConnectionEntry entry = getConnection(connection);

        if (entry == null)
            return result(
                INVALID_HANDLE,
                "Invalid Bluetooth Classic connection handle");

        BluetoothSocket socket = entry.socket;

        if (socket == null)
            return result(
                INVALID_HANDLE,
                "Bluetooth Classic socket is not connected");

        entry.manualClosing = true;

        try
        {
            socket.close();
            return result(OK, "");
        }
        catch (IOException exception)
        {
            return result(
                OPERATION_FAILED,
                throwableMessage(exception));
        }
    }


    @Override
    public boolean bluetooth_classic_connection_is_valid(
        long connection)
    {
        return getConnection(connection) != null;
    }


    @Override
    public boolean bluetooth_classic_connection_is_connected(
        long connection)
    {
        ConnectionEntry entry = getConnection(connection);

        if (entry == null || !entry.connected)
            return false;

        BluetoothSocket socket = entry.socket;

        return socket != null && socket.isConnected();
    }


    @Override
    public long bluetooth_classic_connection_get_device(
        long connection)
    {
        ConnectionEntry entry = getConnection(connection);
        return entry != null ? entry.device : 0;
    }


    @Override
    public int bluetooth_classic_receive_available(
        long connection)
    {
        ConnectionEntry entry = getConnection(connection);

        if (entry == null)
            return 0;

        synchronized (entry.receiveLock)
        {
            return entry.receiveAvailable;
        }
    }


    // data/out_data span the caller's ENTIRE GameMaker buffer (mirroring the
    // native GMBuffer convention in GMBluetooth_native.cpp, where offset is
    // applied as data.data() + offset) - not a pre-sliced window. A duplicate()
    // is used so we never disturb the position/limit of the buffer the runner
    // owns.
    private static boolean bufferRangeInvalid(ByteBuffer buffer, int offset, int length)
    {
        return
            buffer == null ||
            offset < 0 ||
            length < 0 ||
            (long) offset + (long) length > buffer.capacity();
    }


    @Override
    public int bluetooth_classic_send(
        long connection,
        ByteBuffer data,
        int offset,
        int size)
    {
        if (!initialized)
            return result(NOT_INITIALIZED, "Bluetooth is not initialized");

        ConnectionEntry entry = getConnection(connection);

        if (entry == null)
            return result(
                INVALID_HANDLE,
                "Invalid Bluetooth Classic connection handle");

        BluetoothSocket socket = entry.socket;

        if (socket == null || !entry.connected)
            return result(
                DISCONNECTED,
                "Classic connection is not connected");

        if (bufferRangeInvalid(data, offset, size))
            return result(
                INVALID_ARGUMENT,
                "Invalid buffer offset/size for bluetooth_classic_send");

        byte[] chunk = new byte[size];

        if (size > 0)
        {
            ByteBuffer view = data.duplicate();
            view.position(offset);
            view.get(chunk, 0, size);
        }

        try
        {
            OutputStream output = socket.getOutputStream();
            output.write(chunk);
            return result(OK, "");
        }
        catch (IOException exception)
        {
            return result(DISCONNECTED, throwableMessage(exception));
        }
    }


    @Override
    public int bluetooth_classic_receive(
        long connection,
        ByteBuffer out_data,
        int offset,
        int max_size)
    {
        ConnectionEntry entry = getConnection(connection);

        if (entry == null)
            return 0;

        if (bufferRangeInvalid(out_data, offset, max_size))
        {
            setLastError(
                INVALID_ARGUMENT,
                "Invalid buffer offset/size for bluetooth_classic_receive");
            return 0;
        }

        if (max_size == 0)
            return 0;

        byte[] copiedBytes;

        synchronized (entry.receiveLock)
        {
            int copied = Math.min(max_size, entry.receiveAvailable);

            if (copied == 0)
                return 0;

            copiedBytes = new byte[copied];
            int filled = 0;

            while (filled < copied)
            {
                byte[] head = entry.receiveChunks.peekFirst();
                int take = Math.min(head.length, copied - filled);

                System.arraycopy(head, 0, copiedBytes, filled, take);
                entry.receiveChunks.pollFirst();

                if (take < head.length)
                {
                    // Only part of this chunk was consumed - push the
                    // remainder back as the new head of the queue.
                    entry.receiveChunks.addFirst(
                        Arrays.copyOfRange(head, take, head.length));
                }

                filled += take;
            }

            entry.receiveAvailable -= copied;
        }

        ByteBuffer view = out_data.duplicate();
        view.position(offset);
        view.put(copiedBytes);

        return copiedBytes.length;
    }


    // =========================================================================
    // Classic RFCOMM server
    // =========================================================================

    @Override
    public int bluetooth_classic_server_start(
        String name,
        String service_uuid)
    {
        if (!initialized || adapter == null)
            return result(
                NOT_INITIALIZED,
                "Bluetooth is not initialized");

        if (!hasConnectPermission())
            return result(
                PERMISSION_DENIED,
                "Bluetooth connect permission is not granted");

        if (!adapterEnabled())
            return result(
                BLUETOOTH_DISABLED,
                "Bluetooth is disabled");

        if (serverRunning.get())
            return result(OK, "");

        if (service_uuid == null || service_uuid.isEmpty())
            return result(
                INVALID_ARGUMENT,
                "service_uuid cannot be empty");

        final UUID uuid;

        try
        {
            uuid = UUID.fromString(service_uuid);
        }
        catch (Throwable throwable)
        {
            return result(
                INVALID_ARGUMENT,
                "service_uuid is not a valid UUID");
        }

        try
        {
            serverSocket =
                adapter.listenUsingRfcommWithServiceRecord(
                    name == null || name.isEmpty()
                        ? "GMBluetooth RFCOMM"
                        : name,
                    uuid);

            serverRunning.set(true);
        }
        catch (SecurityException exception)
        {
            return result(
                PERMISSION_DENIED,
                throwableMessage(exception));
        }
        catch (IOException exception)
        {
            return result(
                OPERATION_FAILED,
                throwableMessage(exception));
        }

        final long workerGeneration = generation.get();

        Thread thread = new Thread(
            () ->
            {
                while (
                    initialized &&
                    serverRunning.get() &&
                    generation.get() == workerGeneration)
                {
                    try
                    {
                        BluetoothServerSocket server =
                            serverSocket;

                        if (server == null)
                            break;

                        BluetoothSocket socket =
                            server.accept();

                        if (socket == null)
                            continue;

                        BluetoothDevice remote =
                            socket.getRemoteDevice();

                        long device = upsertDevice(
                            TRANSPORT_CLASSIC,
                            deviceId(
                                TRANSPORT_CLASSIC,
                                remote),
                            safeName(remote),
                            safeAddress(remote),
                            0,
                            false,
                            true,
                            remote);

                        long connection =
                            createConnection(device);

                        ConnectionEntry entry =
                            getConnection(connection);

                        if (entry == null)
                        {
                            try
                            {
                                socket.close();
                            }
                            catch (Throwable ignored)
                            {
                            }
                            continue;
                        }

                        entry.socket = socket;
                        entry.connected = true;

                        invoke(
                            callbackClassicClientConnected,
                            (double) connection,
                            (double) device);

                        startReadLoop(
                            connection,
                            socket,
                            workerGeneration);
                    }
                    catch (IOException exception)
                    {
                        if (
                            initialized &&
                            serverRunning.get() &&
                            generation.get() ==
                                workerGeneration)
                        {
                            setLastError(
                                OPERATION_FAILED,
                                throwableMessage(exception));
                        }

                        break;
                    }
                    catch (Throwable throwable)
                    {
                        if (
                            initialized &&
                            generation.get() ==
                                workerGeneration)
                        {
                            setLastError(
                                OPERATION_FAILED,
                                throwableMessage(throwable));
                        }

                        break;
                    }
                }

                serverRunning.set(false);
            },
            "GMBluetooth-RFCOMM-Accept");

        thread.setDaemon(true);
        thread.start();

        return result(OK, "");
    }


    private void stopServerInternal()
    {
        serverRunning.set(false);

        BluetoothServerSocket server = serverSocket;
        serverSocket = null;

        if (server != null)
        {
            try
            {
                server.close();
            }
            catch (Throwable ignored)
            {
            }
        }
    }


    @Override
    public int bluetooth_classic_server_stop()
    {
        if (!initialized)
            return result(
                NOT_INITIALIZED,
                "Bluetooth is not initialized");

        stopServerInternal();
        return result(OK, "");
    }


    @Override
    public boolean bluetooth_classic_server_is_running()
    {
        return initialized && serverRunning.get();
    }


    @Override
    public int bluetooth_classic_discoverable_start(int duration_seconds)
    {
        if (!initialized)
            return result(
                NOT_INITIALIZED,
                "Bluetooth is not initialized");

        if (!hasAdvertisePermission())
            return result(
                PERMISSION_DENIED,
                "Bluetooth advertise permission is not granted");

        if (!adapterEnabled())
            return result(
                BLUETOOTH_DISABLED,
                "Bluetooth is disabled");

        // Android has no real "indefinite" discoverable mode; fall back to its own default.
        int requestedDuration = duration_seconds > 0 ? duration_seconds : 120;

        Activity current = activity();

        if (current == null)
            return result(
                OPERATION_FAILED,
                "No foreground activity available to request discoverability");

        try
        {
            Intent discoverableIntent =
                new Intent(BluetoothAdapter.ACTION_REQUEST_DISCOVERABLE);
            discoverableIntent.putExtra(
                BluetoothAdapter.EXTRA_DISCOVERABLE_DURATION,
                requestedDuration);
            discoverableIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            current.startActivity(discoverableIntent);
        }
        catch (Throwable throwable)
        {
            return result(
                OPERATION_FAILED,
                throwableMessage(throwable));
        }

        return result(OK, "");
    }


    @Override
    public int bluetooth_classic_discoverable_stop()
    {
        return result(
            NOT_SUPPORTED,
            "Android does not provide an API to cancel discoverability early; it expires on its own");
    }


    @Override
    public boolean bluetooth_classic_discoverable_is_running()
    {
        return initialized &&
            adapter != null &&
            adapter.getScanMode() == BluetoothAdapter.SCAN_MODE_CONNECTABLE_DISCOVERABLE;
    }


    // =========================================================================
    // Callback registration
    // =========================================================================

    @Override
    public boolean bluetooth_set_callback_device_found(
        GMFunction callback)
    {
        callbackDeviceFound = callback;
        return true;
    }


    @Override
    public boolean bluetooth_remove_callback_device_found()
    {
        callbackDeviceFound = null;
        return true;
    }


    @Override
    public boolean bluetooth_set_callback_scan_stopped(
        GMFunction callback)
    {
        callbackScanStopped = callback;
        return true;
    }


    @Override
    public boolean bluetooth_remove_callback_scan_stopped()
    {
        callbackScanStopped = null;
        return true;
    }


    @Override
    public boolean bluetooth_set_callback_classic_client_connected(
        GMFunction callback)
    {
        callbackClassicClientConnected = callback;
        return true;
    }


    @Override
    public boolean bluetooth_remove_callback_classic_client_connected()
    {
        callbackClassicClientConnected = null;
        return true;
    }


    @Override
    public boolean bluetooth_set_callback_classic_data(
        GMFunction callback)
    {
        callbackClassicData = callback;
        return true;
    }


    @Override
    public boolean bluetooth_remove_callback_classic_data()
    {
        callbackClassicData = null;
        return true;
    }


    @Override
    public boolean bluetooth_set_callback_classic_disconnected(
        GMFunction callback)
    {
        callbackClassicDisconnected = callback;
        return true;
    }


    @Override
    public boolean bluetooth_remove_callback_classic_disconnected()
    {
        callbackClassicDisconnected = null;
        return true;
    }


    @Override
    public boolean bluetooth_set_callback_le_disconnected(
        GMFunction callback)
    {
        callbackLeDisconnected = callback;
        return true;
    }


    @Override
    public boolean bluetooth_remove_callback_le_disconnected()
    {
        callbackLeDisconnected = null;
        return true;
    }


    @Override
    public boolean bluetooth_set_callback_le_characteristic_value_changed(
        GMFunction callback)
    {
        callbackLeCharacteristicValueChanged = callback;
        return true;
    }


    @Override
    public boolean bluetooth_remove_callback_le_characteristic_value_changed()
    {
        callbackLeCharacteristicValueChanged = null;
        return true;
    }


    @Override
    public boolean bluetooth_set_callback_le_server_connection_state_changed(
        GMFunction callback)
    {
        callbackLeServerConnectionStateChanged = callback;
        return true;
    }


    @Override
    public boolean bluetooth_remove_callback_le_server_connection_state_changed()
    {
        callbackLeServerConnectionStateChanged = null;
        return true;
    }


    @Override
    public boolean bluetooth_set_callback_le_server_read_request(
        GMFunction callback)
    {
        callbackLeServerReadRequest = callback;
        return true;
    }


    @Override
    public boolean bluetooth_remove_callback_le_server_read_request()
    {
        callbackLeServerReadRequest = null;
        return true;
    }


    @Override
    public boolean bluetooth_set_callback_le_server_write_request(
        GMFunction callback)
    {
        callbackLeServerWriteRequest = callback;
        return true;
    }


    @Override
    public boolean bluetooth_remove_callback_le_server_write_request()
    {
        callbackLeServerWriteRequest = null;
        return true;
    }
}
