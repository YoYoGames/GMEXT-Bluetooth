package ${YYAndroidPackageName};

import ${YYAndroidPackageName}.GMExtWire.GMFunction;
import ${YYAndroidPackageName}.enums.BluetoothError;
import ${YYAndroidPackageName}.enums.BluetoothLeAdvertiseTxPower;
import ${YYAndroidPackageName}.enums.BluetoothLeSubscribeMode;
import ${YYAndroidPackageName}.enums.BluetoothLeWriteType;
import ${YYAndroidPackageName}.enums.BluetoothPermissionStatus;
import ${YYAndroidPackageName}.enums.BluetoothTransport;
import ${YYAndroidPackageName}.records.BluetoothLeAdvertiseData;
import ${YYAndroidPackageName}.records.BluetoothLeAdvertiseManufacturerData;
import ${YYAndroidPackageName}.records.BluetoothLeAdvertiseServiceData;
import ${YYAndroidPackageName}.records.BluetoothLeAdvertiseSettings;
import ${YYAndroidPackageName}.records.BluetoothLeServiceDefinition;

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
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.location.LocationManager;
import android.os.Build;
import android.os.ParcelUuid;
import android.provider.Settings;
import android.util.Base64;

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

    // BluetoothError. Exports return the enum; invoke() hands it to GML as its
    // number, since the callback wire carries no enum type.
    private static final BluetoothError OK                 = BluetoothError.Ok;
    private static final BluetoothError UNKNOWN            = BluetoothError.Unknown;
    private static final BluetoothError NOT_SUPPORTED      = BluetoothError.NotSupported;
    private static final BluetoothError NOT_INITIALIZED    = BluetoothError.NotInitialized;
    private static final BluetoothError BLUETOOTH_DISABLED = BluetoothError.BluetoothDisabled;
    private static final BluetoothError PERMISSION_DENIED  = BluetoothError.PermissionDenied;
    private static final BluetoothError INVALID_ARGUMENT   = BluetoothError.InvalidArgument;
    private static final BluetoothError INVALID_HANDLE     = BluetoothError.InvalidHandle;
    private static final BluetoothError BUSY               = BluetoothError.Busy;
    private static final BluetoothError TIMEOUT            = BluetoothError.Timeout;
    private static final BluetoothError NOT_FOUND          = BluetoothError.NotFound;
    private static final BluetoothError CONNECTION_FAILED  = BluetoothError.ConnectionFailed;
    private static final BluetoothError DISCONNECTED       = BluetoothError.Disconnected;
    private static final BluetoothError OPERATION_FAILED   = BluetoothError.OperationFailed;

    private static final int TRANSPORT_UNKNOWN = 0;
    private static final int TRANSPORT_CLASSIC = 1;
    private static final int TRANSPORT_LE      = 2;

    private static final BluetoothPermissionStatus PERMISSION_UNKNOWN = BluetoothPermissionStatus.Unknown;
    private static final BluetoothPermissionStatus PERMISSION_GRANTED = BluetoothPermissionStatus.Granted;
    private static final BluetoothPermissionStatus PERMISSION_DENIED_STATUS = BluetoothPermissionStatus.Denied;

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

    // BluetoothLeSubscribeMode (spec.gmidl), as the subscriber bookkeeping
    // stores it.
    private static final int SUBSCRIBE_MODE_UNSUBSCRIBE = 0;
    private static final int SUBSCRIBE_MODE_NOTIFY = 1;
    private static final int SUBSCRIBE_MODE_INDICATE = 2;

    // BluetoothState (spec.gmidl)
    private static final int STATE_UNKNOWN      = 0;
    private static final int STATE_RESETTING    = 1;
    private static final int STATE_UNSUPPORTED  = 2;
    private static final int STATE_UNAUTHORIZED = 3;
    private static final int STATE_POWERED_OFF  = 4;
    private static final int STATE_POWERED_ON   = 5;

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
    private volatile boolean stateReceiverRegistered = false;

    private volatile boolean initialized = false;

    // Incrementing this invalidates callbacks from worker threads belonging to
    // an older initialize/shutdown session.
    private final AtomicLong generation = new AtomicLong(1);


    // =========================================================================
    // Error state
    // =========================================================================

    private volatile BluetoothError lastErrorCode = OK;
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
    // Local Bluetooth adapter state
    // =========================================================================

    private int currentBluetoothState()
    {
        BluetoothAdapter current = adapter;
        if (current == null)
            return STATE_UNSUPPORTED;

        try
        {
            switch (current.getState())
            {
                case BluetoothAdapter.STATE_ON:
                    // On, but the app may not use it: Android 12+ grants the
                    // nearby-devices permissions at runtime, and Apple
                    // reports the same situation as Unauthorized.
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S &&
                        (!hasScanPermission() || !hasConnectPermission()))
                        return STATE_UNAUTHORIZED;
                    return STATE_POWERED_ON;
                case BluetoothAdapter.STATE_OFF:
                    return STATE_POWERED_OFF;
                case BluetoothAdapter.STATE_TURNING_ON:
                case BluetoothAdapter.STATE_TURNING_OFF:
                    return STATE_RESETTING;
                default:
                    return STATE_UNKNOWN;
            }
        }
        catch (SecurityException ignored)
        {
            return STATE_UNAUTHORIZED;
        }
        catch (Throwable ignored)
        {
            return STATE_UNKNOWN;
        }
    }

    private final BroadcastReceiver stateReceiver = new BroadcastReceiver()
    {
        @Override
        public void onReceive(Context receiverContext, Intent intent)
        {
            if (!initialized || intent == null ||
                !BluetoothAdapter.ACTION_STATE_CHANGED.equals(intent.getAction()))
                return;

            invoke(callbackStateChanged, currentBluetoothState());

            int adapterState = intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, -1);

            if (adapterState == BluetoothAdapter.STATE_TURNING_OFF ||
                adapterState == BluetoothAdapter.STATE_OFF)
                stopScansForRadioOff();
        }
    };


    // The stack drops a running scan when the radio goes off without calling
    // onScanFailed, so the flags are cleared and each scan reported here.
    // getAndSet makes each scan report once, whichever of this and
    // ACTION_DISCOVERY_FINISHED comes first.
    private void stopScansForRadioOff()
    {
        if (leScanning.getAndSet(false))
        {
            try
            {
                BluetoothLeScanner scanner = leScanner;
                if (scanner != null && hasScanPermission())
                    scanner.stopScan(leScanCallback);
            }
            catch (Throwable ignored)
            {
            }

            leScanner = null;
            dispatchScanStopped(TRANSPORT_LE, BLUETOOTH_DISABLED, "Bluetooth was turned off");
        }

        if (classicScanning.getAndSet(false))
        {
            try
            {
                BluetoothAdapter current = adapter;
                if (current != null && current.isDiscovering())
                    current.cancelDiscovery();
            }
            catch (Throwable ignored)
            {
            }

            dispatchScanStopped(TRANSPORT_CLASSIC, BLUETOOTH_DISABLED, "Bluetooth was turned off");
        }
    }

    private void ensureStateReceiver()
    {
        if (stateReceiverRegistered)
            return;

        Context current = context();
        if (current == null)
            return;

        try
        {
            IntentFilter filter = new IntentFilter(BluetoothAdapter.ACTION_STATE_CHANGED);
            current.registerReceiver(stateReceiver, filter);
            stateReceiverRegistered = true;
        }
        catch (Throwable ignored)
        {
        }
    }

    private void unregisterStateReceiver()
    {
        if (!stateReceiverRegistered)
            return;

        Context current = context();
        if (current != null)
        {
            try
            {
                current.unregisterReceiver(stateReceiver);
            }
            catch (Throwable ignored)
            {
            }
        }

        stateReceiverRegistered = false;
    }


    // =========================================================================
    // Pairing (bonding)
    // =========================================================================

    // Keyed by Android device address rather than our device handle, since the
    // ACTION_BOND_STATE_CHANGED broadcast only identifies the BluetoothDevice.
    private final Object pairLock = new Object();
    private final HashMap<String, Long> pairDeviceHandles = new HashMap<>();
    private final HashMap<String, GMFunction> pairCallbacks = new HashMap<>();
    private volatile boolean bondReceiverRegistered = false;


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

        // Set when the read loop has ended. A finished entry stays only while
        // it holds bytes the game has not read.
        volatile boolean finished = false;

        // Sends queued for the writer thread; sendQueuedBytes counts a chunk
        // until its write() has returned.
        final Object sendLock = new Object();
        final ArrayDeque<byte[]> sendQueue = new ArrayDeque<>();
        int sendQueuedBytes = 0;
        boolean closeAfterSend = false;
    }

    // Past this many queued bytes bluetooth_classic_send answers BUSY.
    private static final int MAX_QUEUED_SEND_BYTES = 1024 * 1024;

    // How long bluetooth_classic_disconnect lets queued bytes drain to a peer
    // that has stopped reading before the socket is closed anyway.
    private static final long CLOSE_AFTER_SEND_TIMEOUT_MS = 2000;

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
    private interface GattOpStart
    {
        // Issues the request; false when the stack refused it.
        boolean start();
    }

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
        GattOpStart start;
        String startFailureMessage = "";
    }

    private static final String LE_OP_DISCONNECTED_MESSAGE =
        "LE connection closed before the operation completed";
    private static final String SHUTDOWN_MESSAGE =
        "Bluetooth was shut down before the operation completed";

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

        // Guarded by opLock. Once closed, the link is gone and new ops fail at once.
        final Object opLock = new Object();
        final ArrayDeque<LePendingOp> opQueue = new ArrayDeque<>();
        boolean opRunning = false;
        boolean closed = false;
        LePendingOp currentOp = null;

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
        // The stack's id, which it allocates per link: two centrals can use
        // the same one, so GML sees the extension's own id instead.
        int stackRequestId;
        long connection;
        BluetoothDevice device;
        String serviceUuid = "";
        String characteristicUuid = "";
        String descriptorUuid = "";
        boolean isWrite;
        boolean responseNeeded;
        byte[] writeValue = new byte[0];
        long receivedAtNanos = System.nanoTime();

        // Set for one attribute of an executed prepared write; the execute is
        // answered once, through the batch.
        LeServerWriteBatch batch;
    }

    // An Execute Write: one write request per attribute the central prepared,
    // answered once when the last of them is - the first error, or success.
    private static final class LeServerWriteBatch
    {
        BluetoothDevice device;
        int stackRequestId;
        int remaining;
        int status = BluetoothGatt.GATT_SUCCESS;
    }

    // The fragments a central has prepared for one attribute, laid out at
    // their offsets, until its Execute Write.
    private static final class LeServerPreparedWrite
    {
        BluetoothGattCharacteristic characteristic;
        BluetoothGattDescriptor descriptor;
        byte[] value = new byte[0];
    }

    // A write without response needs no answer, so nothing else removes it.
    // It is kept this long for GML to read its value, then dropped.
    private static final long NO_RESPONSE_WRITE_TTL_NANOS = 5_000_000_000L;

    // The ATT transaction timeout: a request still waiting after this has
    // already cost the central its link.
    private static final long SERVER_REQUEST_TTL_NANOS = 30_000_000_000L;

    // The longest attribute value ATT allows.
    private static final int MAX_ATTRIBUTE_LENGTH = 512;

    // ATT error codes the extension answers with when GML does not.
    private static final int ATT_REQUEST_NOT_SUPPORTED = 0x06;
    private static final int ATT_INVALID_OFFSET = 0x07;
    private static final int ATT_INVALID_ATTRIBUTE_LENGTH = 0x0D;
    private static final int ATT_UNLIKELY_ERROR = 0x0E;

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
    // BluetoothGattServer takes one addService at a time, until its
    // onServiceAdded, so adds wait here in order; only the head is in flight.
    private final ArrayDeque<LeServerAddEntry> leServerAddQueue = new ArrayDeque<>();

    private static final class LeServerAddEntry
    {
        BluetoothGattService service;
        GMFunction callback;
        boolean inFlight = false;

        // Failed by clear_services while the stack still had it: its
        // onServiceAdded fires nothing and removes the service again.
        boolean cancelled = false;
    }

    private final Object leServerRequestLock = new Object();
    private final HashMap<Integer, LeServerRequestEntry> leServerRequests = new HashMap<>();
    private int nextLeServerRequestId = 1;

    // Per central (by address), the attributes it has prepared writes for, in
    // the order it first wrote them. Guarded by leServerRequestLock.
    private final HashMap<String, LinkedHashMap<Object, LeServerPreparedWrite>> leServerPreparedWrites =
        new HashMap<>();

    // Initial values from add_service, served here without reaching GML, as
    // Windows (StaticValue) and Apple (a cached value) serve theirs. Keyed by
    // the characteristic object the server hands back in its read requests.
    private final Object leServerInitialValueLock = new Object();
    private final IdentityHashMap<BluetoothGattCharacteristic, byte[]> leServerInitialValues =
        new IdentityHashMap<>();

    // Android does not track who subscribed to notify/indicate for a locally
    // hosted characteristic - this is populated from CCCD descriptor writes
    // we intercept and auto-acknowledge ourselves. Keyed by
    // "<serviceUuid>|<characteristicUuid>".
    private final Object leServerSubscriberLock = new Object();
    private final HashMap<String, HashSet<Long>> leServerSubscribers = new HashMap<>();


    // =========================================================================
    // BLE values
    // =========================================================================

    // A value read or notified, held for GML under its own id until
    // bluetooth_le_value_copy or _release frees it. Values outlive their link,
    // so one delivered just before a disconnect can still be copied; the cap
    // drops the oldest, and shutdown frees the rest.
    private static final int LE_VALUE_LIMIT = 256;

    private final Object leValueLock = new Object();
    private long nextLeValueId = 1;
    private final LinkedHashMap<Long, byte[]> leValues = new LinkedHashMap<>();


    // =========================================================================
    // GML callbacks
    // =========================================================================

    private volatile GMFunction callbackStateChanged = null;
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


    // A UUID from GML. A 4- or 8-hex-digit SIG UUID ("180D") is expanded onto
    // the Bluetooth base UUID, as CoreBluetooth and the native core read it;
    // UUID.fromString alone takes only the 36-character form.
    private static UUID parseUuid(String text)
    {
        String value = text != null ? text.trim() : "";

        if (value.length() == 4)
            value = "0000" + value + "-0000-1000-8000-00805f9b34fb";
        else if (value.length() == 8)
            value = value + "-0000-1000-8000-00805f9b34fb";

        return UUID.fromString(value);
    }


    private void setLastError(BluetoothError code, String message)
    {
        lastErrorCode = code;
        lastErrorMessage = message != null ? message : "";
    }


    // An export's result. Last-error is the detail for a failure; a success
    // leaves it alone.
    private BluetoothError result(BluetoothError code, String message)
    {
        if (code != OK)
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


    // From Android 12, BLUETOOTH_SCAN is declared with neverForLocation, so a
    // scan needs it alone; before, a scan needs FINE location.
    private boolean hasScanPermission()
    {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S)
            return hasPermission(Manifest.permission.BLUETOOTH_SCAN);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M)
            return hasPermission(Manifest.permission.ACCESS_FINE_LOCATION);

        return true;
    }


    // Android 6 to 11 deliver no scan results while system Location is off,
    // even with the permission granted. Android 12+ scans without it.
    private boolean scanNeedsLocationOn()
    {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M ||
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.S)
            return false;

        Context current = context();

        if (current == null)
            return false;

        try
        {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P)
            {
                LocationManager manager =
                    (LocationManager) current.getSystemService(Context.LOCATION_SERVICE);
                return manager != null && !manager.isLocationEnabled();
            }

            return Settings.Secure.getInt(
                current.getContentResolver(),
                Settings.Secure.LOCATION_MODE,
                Settings.Secure.LOCATION_MODE_OFF) == Settings.Secure.LOCATION_MODE_OFF;
        }
        catch (Throwable ignored)
        {
            return false;
        }
    }


    private static final String LOCATION_OFF_MESSAGE =
        "System Location is off; Android 11 and below need it on to scan";


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


    private void dispatchScanStopped(int transport, BluetoothError error, String message)
    {
        String safeMessage = message != null ? message : "";

        if (error != OK)
            setLastError(error, safeMessage);

        invoke(callbackScanStopped, error, safeMessage);
    }


    private void dispatchConnectResult(
        long connection,
        long device,
        BluetoothError error,
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
        BluetoothError error,
        String message)
    {
        String safeMessage = message != null ? message : "";
        ConnectionEntry entry = getConnection(connection);

        invoke(
            callbackClassicDisconnected,
            (double) connection,
            error,
            safeMessage);

        // Bytes the game has not read yet outlive a remote hang-up: the entry
        // stays until bluetooth_classic_receive drains it,
        // bluetooth_classic_disconnect drops it or shutdown clears it.
        boolean keep = false;

        if (entry != null)
        {
            synchronized (entry.receiveLock)
            {
                entry.finished = true;
                entry.connected = false;
                keep = !entry.manualClosing && entry.receiveAvailable > 0;
            }

            wakeWriter(entry);
        }

        if (!keep)
            eraseConnection(connection);

        if (error != OK)
            setLastError(error, safeMessage);
    }


    private static void wakeWriter(ConnectionEntry entry)
    {
        synchronized (entry.sendLock)
        {
            entry.sendLock.notifyAll();
        }
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

        for (int i = 0; i < arguments.length; i++)
        {
            if (arguments[i] instanceof BluetoothError)
                arguments[i] = ((BluetoothError) arguments[i]).value();
        }

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

        failGattOps(entry, DISCONNECTED, LE_OP_DISCONNECTED_MESSAGE);

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


    // The characteristic's handle, null when the object is not one this
    // extension knows.
    private Long characteristicHandleOf(BluetoothGattCharacteristic characteristic)
    {
        synchronized (leEntityLock)
        {
            Long handle = leCharacteristicHandleByObject.get(characteristic);
            return handle != null && leCharacteristics.containsKey(handle) ? handle : null;
        }
    }


    // Copies a value in the GATT callback - the stack and the write path both
    // reuse the shared characteristic object - and holds it for GML. Returns
    // its id.
    private long storeLeValue(byte[] value)
    {
        byte[] copy = value != null ? value.clone() : new byte[0];

        synchronized (leValueLock)
        {
            long id = nextLeValueId++;
            leValues.put(id, copy);

            if (leValues.size() > LE_VALUE_LIMIT)
            {
                Iterator<Long> oldest = leValues.keySet().iterator();
                oldest.next();
                oldest.remove();
            }

            return id;
        }
    }


    // A read op's callback carries (value, size) after its target, 0 and 0
    // when it failed.
    private static boolean isReadOp(LePendingOp op)
    {
        return op.kind == LePendingOp.KIND_READ_CHARACTERISTIC ||
            op.kind == LePendingOp.KIND_READ_DESCRIPTOR;
    }


    private void failOp(LePendingOp op, BluetoothError error, String message)
    {
        if (isReadOp(op))
            invoke(op.callback, error, message, (double) op.targetHandle, 0.0, 0);
        else
            invoke(op.callback, error, message, (double) op.targetHandle);
    }


    // =========================================================================
    // BLE GATT client operation queue
    //
    // Android's BluetoothGatt allows only one outstanding read/write/discover
    // operation at a time - a second call issued while one is in flight
    // silently fails. Every client-role operation on a connection is funneled
    // through this queue and drained one at a time from BluetoothGattCallback.
    // =========================================================================

    private void enqueueGattOp(LeConnectionEntry connection, LePendingOp op)
    {
        boolean closed;
        boolean startNow = false;

        synchronized (connection.opLock)
        {
            closed = connection.closed;

            if (!closed)
            {
                connection.opQueue.addLast(op);
                startNow = !connection.opRunning;
                if (startNow)
                    connection.opRunning = true;
            }
        }

        if (closed)
        {
            failOp(op, DISCONNECTED, LE_OP_DISCONNECTED_MESSAGE);
            return;
        }

        if (startNow)
            runNextGattOp(connection);
    }


    // Starts queued ops until the stack accepts one or the queue is empty. An
    // op the stack refuses is failed here and the next one tried.
    private void runNextGattOp(LeConnectionEntry connection)
    {
        while (true)
        {
            LePendingOp op;

            synchronized (connection.opLock)
            {
                op = connection.opQueue.pollFirst();

                if (op == null)
                {
                    connection.opRunning = false;
                    return;
                }

                connection.currentOp = op;
            }

            boolean started;

            try
            {
                started = op.start.start();
            }
            catch (Throwable throwable)
            {
                started = false;
            }

            if (started)
                return;

            // A purge may have failed it already.
            if (takeCurrentOp(connection) == op)
                failOp(op, OPERATION_FAILED, op.startFailureMessage);
        }
    }


    // Completions and failGattOps both take the op here, so each fires once.
    private LePendingOp takeCurrentOp(LeConnectionEntry connection)
    {
        synchronized (connection.opLock)
        {
            LePendingOp op = connection.currentOp;
            connection.currentOp = null;
            return op;
        }
    }


    private void completeGattOp(LeConnectionEntry connection)
    {
        runNextGattOp(connection);
    }


    // Fails the current op and every queued one, once each, and refuses any
    // op queued afterwards: the link is closed, so no completion will come.
    private void failGattOps(LeConnectionEntry connection, BluetoothError error, String message)
    {
        ArrayList<LePendingOp> failed = new ArrayList<>();

        synchronized (connection.opLock)
        {
            connection.closed = true;

            if (connection.currentOp != null)
                failed.add(connection.currentOp);

            connection.currentOp = null;
            failed.addAll(connection.opQueue);
            connection.opQueue.clear();
            connection.opRunning = false;
        }

        for (LePendingOp op : failed)
            failOp(op, error, message);
    }


    // =========================================================================
    // BLE GATT dispatch helpers
    // =========================================================================

    private void dispatchLeDisconnected(long connection, BluetoothError error, String message)
    {
        String safeMessage = message != null ? message : "";

        invoke(callbackLeDisconnected, (double) connection, error, safeMessage);

        if (error != OK)
            setLastError(error, safeMessage);
    }


    private void dispatchLeCharacteristicValueChanged(
        long characteristic,
        long connection,
        byte[] value)
    {
        // No handler, no one to free the value.
        if (callbackLeCharacteristicValueChanged == null)
            return;

        int size = value != null ? value.length : 0;
        long id = storeLeValue(value);

        invoke(
            callbackLeCharacteristicValueChanged,
            (double) characteristic,
            (double) connection,
            (double) id,
            size);
    }


    private void dispatchReadValue(LePendingOp op, byte[] value)
    {
        int size = value != null ? value.length : 0;
        long id = storeLeValue(value);

        invoke(op.callback, OK, "", (double) op.targetHandle, (double) id, size);
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


    private static Object objectField(Object object, String name) throws Exception
    {
        try
        {
            return object.getClass().getField(name).get(object);
        }
        catch (NoSuchFieldException ignored)
        {
            String suffix = Character.toUpperCase(name.charAt(0)) + name.substring(1);
            try
            {
                return object.getClass().getMethod("get" + suffix).invoke(object);
            }
            catch (NoSuchMethodException ignoredGetter)
            {
                return object.getClass().getMethod(name).invoke(object);
            }
        }
    }

    private static String objectString(Object object, String name) throws Exception
    {
        Object value = objectField(object, name);
        return value != null ? String.valueOf(value) : "";
    }

    // The generated records carry an optional field as java.util.Optional;
    // String.valueOf would turn it into the text "Optional[...]".
    private static String objectNullableString(Object object, String name) throws Exception
    {
        Object value = objectField(object, name);
        if (value instanceof java.util.Optional)
            value = ((java.util.Optional<?>) value).orElse(null);
        return value != null ? String.valueOf(value) : null;
    }

    private static int objectInt(Object object, String name, int fallback) throws Exception
    {
        Object value = objectField(object, name);
        return value instanceof Number ? ((Number) value).intValue() : fallback;
    }

    private static Object[] objectArray(Object object, String name) throws Exception
    {
        Object value = objectField(object, name);
        if (value == null)
            return new Object[0];
        if (value instanceof Object[])
            return (Object[]) value;
        if (value instanceof java.util.List)
            return ((java.util.List<?>) value).toArray();
        return new Object[0];
    }


    // =========================================================================
    // Lifecycle
    // =========================================================================

    @Override
    public boolean bluetooth_initialize()
    {
        if (initialized)
            return true;

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
            ensureStateReceiver();
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

        synchronized (leValueLock)
        {
            leValues.clear();
        }

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
        unregisterBondReceiver();
        unregisterStateReceiver();

        synchronized (pairLock)
        {
            pairDeviceHandles.clear();
            pairCallbacks.clear();
        }

        stopServerInternal();
        stopLeAdvertiseInternal();
        stopLeServerInternal(NOT_INITIALIZED, SHUTDOWN_MESSAGE);

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

            // The generation bump above silences the GATT callbacks, so every
            // callback still waiting is failed here, once.
            GMFunction connectCallback = entry.connectCallback;
            entry.connectCallback = null;

            invoke(
                connectCallback,
                NOT_INITIALIZED,
                SHUTDOWN_MESSAGE,
                (double) entry.handle,
                (double) entry.device);

            failGattOps(entry, NOT_INITIALIZED, SHUTDOWN_MESSAGE);

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
            leServerPreparedWrites.clear();
        }

        synchronized (leServerInitialValueLock)
        {
            leServerInitialValues.clear();
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

        callbackStateChanged = null;
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
    public BluetoothError bluetooth_last_error_code()
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
    public boolean bluetooth_le_advertise_is_supported()
    {
        if (!bluetooth_le_is_supported() || Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP)
            return false;

        try
        {
            return adapter.isMultipleAdvertisementSupported() &&
                adapter.getBluetoothLeAdvertiser() != null;
        }
        catch (Throwable ignored)
        {
            return false;
        }
    }


    @Override
    public boolean bluetooth_le_server_is_supported()
    {
        if (!bluetooth_le_is_supported() || Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP)
            return false;

        Context current = context();
        if (current == null)
            return false;

        try
        {
            BluetoothManager manager = (BluetoothManager)
                current.getSystemService(Context.BLUETOOTH_SERVICE);
            return manager != null;
        }
        catch (Throwable ignored)
        {
            return false;
        }
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
    public BluetoothPermissionStatus bluetooth_permission_get_status()
    {
        if (!initialized)
            return PERMISSION_UNKNOWN;

        if (hasScanPermission() && hasConnectPermission() && hasAdvertisePermission())
            return PERMISSION_GRANTED;

        // Android cannot tell "never asked" from "denied", so the extension
        // remembers asking; Unknown until then, as Apple's notDetermined.
        return permissionRequested() ? PERMISSION_DENIED_STATUS : PERMISSION_UNKNOWN;
    }


    private static final String PREFERENCES_NAME = "GMBluetooth";
    private static final String PREFERENCE_PERMISSION_REQUESTED = "permission_requested";

    // Kept in the app's own preferences, so it survives restarts and is
    // cleared with the app's data, as the grants are.
    private boolean permissionRequested()
    {
        Context current = context();

        if (current == null)
            return false;

        try
        {
            return current
                .getSharedPreferences(PREFERENCES_NAME, Context.MODE_PRIVATE)
                .getBoolean(PREFERENCE_PERMISSION_REQUESTED, false);
        }
        catch (Throwable ignored)
        {
            return false;
        }
    }


    private void markPermissionRequested()
    {
        Context current = context();

        if (current == null)
            return;

        try
        {
            SharedPreferences.Editor editor = current
                .getSharedPreferences(PREFERENCES_NAME, Context.MODE_PRIVATE)
                .edit();
            editor.putBoolean(PREFERENCE_PERMISSION_REQUESTED, true);
            editor.apply();
        }
        catch (Throwable ignored)
        {
        }
    }


    // The runner forwards the result of every permission request; the state
    // callback hears whether Bluetooth is now usable (R1-63).
    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults)
    {
        if (requestCode != REQUEST_CODE_BLUETOOTH || !initialized)
            return;

        invoke(callbackStateChanged, currentBluetoothState());
    }


    @Override
    public BluetoothError bluetooth_permission_request()
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
            return OK;

        try
        {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S)
            {
                markPermissionRequested();
                current.requestPermissions(
                    new String[] {
                        Manifest.permission.BLUETOOTH_SCAN,
                        Manifest.permission.BLUETOOTH_CONNECT,
                        Manifest.permission.BLUETOOTH_ADVERTISE
                    },
                    REQUEST_CODE_BLUETOOTH);
            }
            else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M)
            {
                markPermissionRequested();
                current.requestPermissions(
                    new String[] {
                        Manifest.permission.ACCESS_FINE_LOCATION,
                        Manifest.permission.ACCESS_COARSE_LOCATION
                    },
                    REQUEST_CODE_BLUETOOTH);
            }

            return OK;
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
    public BluetoothError bluetooth_le_scan_start(boolean active)
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

        if (scanNeedsLocationOn())
            return result(PERMISSION_DENIED, LOCATION_OFF_MESSAGE);

        if (!adapterEnabled())
            return result(
                BLUETOOTH_DISABLED,
                "Bluetooth is disabled");

        if (leScanning.get())
            return OK;

        try
        {
            leScanner = adapter.getBluetoothLeScanner();

            if (leScanner == null)
                return result(
                    NOT_SUPPORTED,
                    "Bluetooth LE scanner is unavailable");

            leScanner.startScan(leScanCallback);
            leScanning.set(true);

            return OK;
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
    public BluetoothError bluetooth_le_scan_stop()
    {
        if (!initialized)
            return result(
                NOT_INITIALIZED,
                "Bluetooth is not initialized");

        if (!leScanning.getAndSet(false))
            return OK;

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
        return OK;
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

                    // Before le_disconnected, as the native core orders it.
                    failGattOps(entry, DISCONNECTED, LE_OP_DISCONNECTED_MESSAGE);

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

                LePendingOp op = takeCurrentOp(entry);
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


            // Before API 33 the value exists only on the shared object, so it
            // is copied here, before another read or notify replaces it.
            @Override
            public void onCharacteristicRead(
                BluetoothGatt gatt,
                BluetoothGattCharacteristic characteristic,
                int status)
            {
                characteristicRead(characteristic, characteristic.getValue(), status);
            }


            // API 33+: the stack hands the value over and no longer calls the
            // overload above.
            @Override
            public void onCharacteristicRead(
                BluetoothGatt gatt,
                BluetoothGattCharacteristic characteristic,
                byte[] value,
                int status)
            {
                characteristicRead(characteristic, value, status);
            }


            private void characteristicRead(
                BluetoothGattCharacteristic characteristic,
                byte[] value,
                int status)
            {
                if (generation.get() != workerGeneration)
                    return;

                LeConnectionEntry entry = getLeConnection(connection);
                if (entry == null)
                    return;

                LePendingOp op = takeCurrentOp(entry);

                if (op != null)
                {
                    if (status == BluetoothGatt.GATT_SUCCESS)
                        dispatchReadValue(op, value);
                    else
                        failOp(op, OPERATION_FAILED, "GATT status " + status);
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

                LePendingOp op = takeCurrentOp(entry);

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
                descriptorRead(descriptor, descriptor.getValue(), status);
            }


            @Override
            public void onDescriptorRead(
                BluetoothGatt gatt,
                BluetoothGattDescriptor descriptor,
                int status,
                byte[] value)
            {
                descriptorRead(descriptor, value, status);
            }


            private void descriptorRead(
                BluetoothGattDescriptor descriptor,
                byte[] value,
                int status)
            {
                if (generation.get() != workerGeneration)
                    return;

                LeConnectionEntry entry = getLeConnection(connection);
                if (entry == null)
                    return;

                LePendingOp op = takeCurrentOp(entry);

                if (op != null)
                {
                    if (status == BluetoothGatt.GATT_SUCCESS)
                        dispatchReadValue(op, value);
                    else
                        failOp(op, OPERATION_FAILED, "GATT status " + status);
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

                LePendingOp op = takeCurrentOp(entry);

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
                characteristicChanged(characteristic, characteristic.getValue());
            }


            @Override
            public void onCharacteristicChanged(
                BluetoothGatt gatt,
                BluetoothGattCharacteristic characteristic,
                byte[] value)
            {
                characteristicChanged(characteristic, value);
            }


            private void characteristicChanged(
                BluetoothGattCharacteristic characteristic,
                byte[] value)
            {
                if (generation.get() != workerGeneration)
                    return;

                Long characteristicHandle = characteristicHandleOf(characteristic);

                if (characteristicHandle != null)
                    dispatchLeCharacteristicValueChanged(
                        characteristicHandle,
                        connection,
                        value);
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
    public BluetoothError bluetooth_le_disconnect(long connection)
    {
        LeConnectionEntry entry = getLeConnection(connection);

        if (entry == null)
            return result(INVALID_HANDLE, "Invalid BLE connection handle");

        entry.manualClosing = true;

        // The game is done with this link: no op waiting on it outlives the call.
        failGattOps(entry, DISCONNECTED, LE_OP_DISCONNECTED_MESSAGE);

        BluetoothGatt gatt = entry.gatt;

        if (gatt == null)
        {
            eraseLeConnection(connection);
            return OK;
        }

        try
        {
            gatt.disconnect();
            return OK;
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
    public BluetoothError bluetooth_le_services_discover(long connection, GMFunction callback)
    {
        if (!initialized)
            return result(NOT_INITIALIZED, "Bluetooth is not initialized");

        LeConnectionEntry connEntry = getLeConnection(connection);

        if (connEntry == null || connEntry.gatt == null)
            return result(INVALID_HANDLE, "Invalid BLE connection handle");

        if (!connEntry.connected)
            return result(DISCONNECTED, "BLE connection is not connected");

        final BluetoothGatt gatt = connEntry.gatt;

        LePendingOp op = new LePendingOp();
        op.kind = LePendingOp.KIND_DISCOVER_SERVICES;
        op.callback = callback;
        op.targetHandle = connection;
        op.startFailureMessage = "discoverServices() failed to start";
        op.start = () -> gatt.discoverServices();

        enqueueGattOp(connEntry, op);

        return OK;
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
    public BluetoothError bluetooth_le_characteristics_discover(
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
        return OK;
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
    public BluetoothError bluetooth_le_descriptors_discover(
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
        return OK;
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
    public BluetoothError bluetooth_le_characteristic_read(
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

        LePendingOp op = new LePendingOp();
        op.kind = LePendingOp.KIND_READ_CHARACTERISTIC;
        op.callback = callback;
        op.targetHandle = characteristic;
        op.startFailureMessage = "readCharacteristic() failed to start";
        op.start = () -> gatt.readCharacteristic(gattCharacteristic);

        enqueueGattOp(connEntry, op);

        return OK;
    }


    @Override
    public BluetoothError bluetooth_le_characteristic_write(
        long characteristic,
        ByteBuffer data,
        int offset,
        int size,
        BluetoothLeWriteType write_type,
        GMFunction callback)
    {
        if (!initialized)
            return result(NOT_INITIALIZED, "Bluetooth is not initialized");

        if (write_type != BluetoothLeWriteType.WithResponse &&
            write_type != BluetoothLeWriteType.WithoutResponse)
            return result(INVALID_ARGUMENT, "Invalid BluetoothLeWriteType value");

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
        final int androidWriteType = write_type == BluetoothLeWriteType.WithoutResponse
            ? BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
            : BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT;

        LePendingOp op = new LePendingOp();
        op.kind = LePendingOp.KIND_WRITE_CHARACTERISTIC;
        op.callback = callback;
        op.targetHandle = characteristic;
        op.startFailureMessage = "writeCharacteristic() failed to start";
        op.start = () ->
        {
            gattCharacteristic.setWriteType(androidWriteType);
            gattCharacteristic.setValue(payload);
            return gatt.writeCharacteristic(gattCharacteristic);
        };

        enqueueGattOp(connEntry, op);

        return OK;
    }


    @Override
    public BluetoothError bluetooth_le_characteristic_subscribe(
        long characteristic,
        BluetoothLeSubscribeMode subscribeMode,
        GMFunction callback)
    {
        final int mode = subscribeMode != null ? subscribeMode.value() : -1;

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

        LePendingOp op = new LePendingOp();
        op.kind = LePendingOp.KIND_SUBSCRIBE;
        op.callback = callback;
        op.targetHandle = characteristic;
        op.subscribeMode = mode;
        op.startFailureMessage = "CCCD write failed to start";
        op.start = () ->
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
            return gatt.writeDescriptor(cccd);
        };

        enqueueGattOp(connEntry, op);

        return OK;
    }


    @Override
    public BluetoothError bluetooth_le_descriptor_read(long descriptor, GMFunction callback)
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

        LePendingOp op = new LePendingOp();
        op.kind = LePendingOp.KIND_READ_DESCRIPTOR;
        op.callback = callback;
        op.targetHandle = descriptor;
        op.startFailureMessage = "readDescriptor() failed to start";
        op.start = () -> gatt.readDescriptor(gattDescriptor);

        enqueueGattOp(connEntry, op);

        return OK;
    }


    @Override
    public BluetoothError bluetooth_le_descriptor_write(
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

        // The CCCD has one writer on every platform, bluetooth_le_characteristic_subscribe.
        if (descEntry.gattDescriptor != null && CCCD_UUID.equals(descEntry.gattDescriptor.getUuid()))
            return result(
                INVALID_ARGUMENT,
                "The CCCD is written by bluetooth_le_characteristic_subscribe");

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

        LePendingOp op = new LePendingOp();
        op.kind = LePendingOp.KIND_WRITE_DESCRIPTOR;
        op.callback = callback;
        op.targetHandle = descriptor;
        op.startFailureMessage = "writeDescriptor() failed to start";
        op.start = () ->
        {
            gattDescriptor.setValue(payload);
            return gatt.writeDescriptor(gattDescriptor);
        };

        enqueueGattOp(connEntry, op);

        return OK;
    }


    @Override
    public BluetoothError bluetooth_le_value_copy(long value, ByteBuffer out_data, int offset)
    {
        synchronized (leValueLock)
        {
            byte[] bytes = leValues.get(value);

            if (bytes == null)
                return result(INVALID_HANDLE, "Unknown or already freed BLE value");

            if (bufferRangeInvalid(out_data, offset, bytes.length))
                return result(
                    INVALID_ARGUMENT,
                    "The buffer cannot take the value: " + bytes.length +
                        " bytes are needed at offset " + offset);

            if (bytes.length > 0)
            {
                ByteBuffer view = out_data.duplicate();
                view.position(offset);
                view.put(bytes, 0, bytes.length);
            }

            leValues.remove(value);
        }

        return OK;
    }


    @Override
    public BluetoothError bluetooth_le_value_release(long value)
    {
        synchronized (leValueLock)
        {
            if (leValues.remove(value) == null)
                return result(INVALID_HANDLE, "Unknown or already freed BLE value");
        }

        return OK;
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


    private static byte[] toBytes(java.util.List<Byte> values)
    {
        if (values == null)
            return new byte[0];

        byte[] bytes = new byte[values.size()];

        for (int i = 0; i < bytes.length; i++)
        {
            Byte value = values.get(i);
            bytes[i] = value != null ? value : 0;
        }

        return bytes;
    }


    private static int advertiseTxPowerLevel(BluetoothLeAdvertiseTxPower power)
    {
        switch (power)
        {
            case UltraLow:
                return AdvertiseSettings.ADVERTISE_TX_POWER_ULTRA_LOW;
            case Low:
                return AdvertiseSettings.ADVERTISE_TX_POWER_LOW;
            case High:
                return AdvertiseSettings.ADVERTISE_TX_POWER_HIGH;
            default:
                return AdvertiseSettings.ADVERTISE_TX_POWER_MEDIUM;
        }
    }


    private static BluetoothError advertiseFailureError(int errorCode)
    {
        switch (errorCode)
        {
            case AdvertiseCallback.ADVERTISE_FAILED_DATA_TOO_LARGE:
                return INVALID_ARGUMENT;
            case AdvertiseCallback.ADVERTISE_FAILED_TOO_MANY_ADVERTISERS:
            case AdvertiseCallback.ADVERTISE_FAILED_ALREADY_STARTED:
                return BUSY;
            case AdvertiseCallback.ADVERTISE_FAILED_FEATURE_UNSUPPORTED:
                return NOT_SUPPORTED;
            default:
                return OPERATION_FAILED;
        }
    }


    private static String advertiseFailureName(int errorCode)
    {
        switch (errorCode)
        {
            case AdvertiseCallback.ADVERTISE_FAILED_DATA_TOO_LARGE:
                return "ADVERTISE_FAILED_DATA_TOO_LARGE";
            case AdvertiseCallback.ADVERTISE_FAILED_TOO_MANY_ADVERTISERS:
                return "ADVERTISE_FAILED_TOO_MANY_ADVERTISERS";
            case AdvertiseCallback.ADVERTISE_FAILED_ALREADY_STARTED:
                return "ADVERTISE_FAILED_ALREADY_STARTED";
            case AdvertiseCallback.ADVERTISE_FAILED_FEATURE_UNSUPPORTED:
                return "ADVERTISE_FAILED_FEATURE_UNSUPPORTED";
            case AdvertiseCallback.ADVERTISE_FAILED_INTERNAL_ERROR:
                return "ADVERTISE_FAILED_INTERNAL_ERROR";
            default:
                return "error " + errorCode;
        }
    }


    @Override
    public BluetoothError bluetooth_le_advertise_start(
        BluetoothLeAdvertiseSettings settings,
        BluetoothLeAdvertiseData data,
        GMFunction callback)
    {
        if (!initialized || adapter == null)
            return result(NOT_INITIALIZED, "Bluetooth is not initialized");

        if (settings == null || data == null)
            return result(INVALID_ARGUMENT, "settings and data cannot be undefined");

        // Checked as the native core checks them, before anything starts.
        ArrayList<UUID> serviceUuids = new ArrayList<>();

        if (data.service_uuids() != null)
        {
            for (String text : data.service_uuids())
            {
                try
                {
                    serviceUuids.add(parseUuid(text));
                }
                catch (Throwable throwable)
                {
                    return result(INVALID_ARGUMENT, "Invalid UUID in service_uuids: " + text);
                }
            }
        }

        ArrayList<UUID> serviceDataUuids = new ArrayList<>();
        ArrayList<byte[]> serviceDataBytes = new ArrayList<>();

        if (data.service_data() != null)
        {
            for (BluetoothLeAdvertiseServiceData entry : data.service_data())
            {
                if (entry == null)
                    return result(INVALID_ARGUMENT, "A service_data entry is undefined");

                UUID uuid;

                try
                {
                    uuid = parseUuid(entry.uuid());
                }
                catch (Throwable throwable)
                {
                    return result(INVALID_ARGUMENT, "Invalid UUID in service_data: " + entry.uuid());
                }

                // Its UUID is advertised as a UUID too, on every platform.
                if (!serviceUuids.contains(uuid))
                    return result(
                        INVALID_ARGUMENT,
                        "service_data uuid " + entry.uuid() + " is not in service_uuids");

                serviceDataUuids.add(uuid);
                serviceDataBytes.add(toBytes(entry.data()));
            }
        }

        if (data.manufacturer_data() != null)
        {
            for (BluetoothLeAdvertiseManufacturerData entry : data.manufacturer_data())
            {
                if (entry == null)
                    return result(INVALID_ARGUMENT, "A manufacturer_data entry is undefined");

                if (entry.company_id() < 0 || entry.company_id() > 0xFFFF)
                    return result(
                        INVALID_ARGUMENT,
                        "company_id " + entry.company_id() + " is outside 0-65535");
            }
        }

        if (!hasAdvertisePermission())
            return result(
                PERMISSION_DENIED,
                "Bluetooth advertise permission is not granted");

        if (!adapterEnabled())
            return result(BLUETOOTH_DISABLED, "Bluetooth is disabled");

        if (leAdvertising.get())
            return OK;

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

        AdvertiseData.Builder dataBuilder = new AdvertiseData.Builder()
            .setIncludeDeviceName(data.include_name())
            .setIncludeTxPowerLevel(data.include_tx_power());

        for (UUID uuid : serviceUuids)
            dataBuilder.addServiceUuid(new ParcelUuid(uuid));

        for (int i = 0; i < serviceDataUuids.size(); i++)
            dataBuilder.addServiceData(new ParcelUuid(serviceDataUuids.get(i)), serviceDataBytes.get(i));

        if (data.manufacturer_data() != null)
        {
            for (BluetoothLeAdvertiseManufacturerData entry : data.manufacturer_data())
                dataBuilder.addManufacturerData(entry.company_id(), toBytes(entry.data()));
        }

        int txPowerLevel = settings.tx_power() != null && settings.tx_power().isPresent()
            ? advertiseTxPowerLevel(settings.tx_power().get())
            : AdvertiseSettings.ADVERTISE_TX_POWER_MEDIUM;

        AdvertiseSettings advertiseSettings = new AdvertiseSettings.Builder()
            .setAdvertiseMode(AdvertiseSettings.ADVERTISE_MODE_LOW_LATENCY)
            .setTxPowerLevel(txPowerLevel)
            .setConnectable(settings.connectable())
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
                    advertiseFailureError(errorCode),
                    "Advertise start failed: " + advertiseFailureName(errorCode));
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

        return OK;
    }


    @Override
    public BluetoothError bluetooth_le_advertise_stop()
    {
        if (!initialized)
            return result(NOT_INITIALIZED, "Bluetooth is not initialized");

        stopLeAdvertiseInternal();
        return OK;
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
            // The server callback reports every LE link, the ones this app
            // opened with connectGatt too; those are client connections.
            if (hasClientConnection(deviceHandle))
                return;

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

            // Its requests can no longer be answered, and its prepared writes
            // will never be executed.
            dropLeServerRequestsOf(device);

            if (connection != null)
            {
                eraseLeConnection(connection);
                dispatchLeServerConnectionStateChanged(connection, false, deviceHandle);
            }
        }
    }


    private boolean hasClientConnection(long deviceHandle)
    {
        synchronized (leConnectionLock)
        {
            for (LeConnectionEntry entry : leConnections.values())
            {
                if (!entry.serverRole && entry.device == deviceHandle)
                    return true;
            }
        }

        return false;
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

                LeServerAddEntry head;

                synchronized (leServerAddServiceLock)
                {
                    head = leServerAddQueue.peekFirst();

                    if (head != null && head.inFlight)
                        leServerAddQueue.pollFirst();
                    else
                        head = null;
                }

                if (head != null && head.cancelled)
                {
                    BluetoothGattServer server = gattServer;

                    if (status == BluetoothGatt.GATT_SUCCESS && server != null && service != null)
                    {
                        try
                        {
                            server.removeService(service);
                        }
                        catch (Throwable ignored)
                        {
                        }
                    }
                }
                else if (head != null)
                {
                    if (status == BluetoothGatt.GATT_SUCCESS)
                        invoke(head.callback, OK, "");
                    else
                        invoke(
                            head.callback,
                            OPERATION_FAILED,
                            "onServiceAdded status " + status);
                }

                startNextServiceAdd();
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

                // An initial value is served here and GML never sees the
                // request, as on Windows and Apple.
                byte[] initialValue;

                synchronized (leServerInitialValueLock)
                {
                    initialValue = leServerInitialValues.get(characteristic);
                }

                if (initialValue != null)
                {
                    if (offset < 0 || offset > initialValue.length)
                        sendServerResponse(device, requestId, ATT_INVALID_OFFSET, offset, null);
                    else
                        sendServerResponse(
                            device,
                            requestId,
                            BluetoothGatt.GATT_SUCCESS,
                            offset,
                            Arrays.copyOfRange(initialValue, offset, initialValue.length));
                    return;
                }

                handleServerReadRequest(
                    device,
                    requestId,
                    offset,
                    characteristic,
                    "");
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

                if (preparedWrite)
                {
                    handlePreparedWrite(device, requestId, characteristic, null, responseNeeded, offset, value);
                    return;
                }

                handleServerWriteRequest(
                    device,
                    requestId,
                    characteristic,
                    "",
                    responseNeeded,
                    value);
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

                String descriptorUuid = descriptor != null && descriptor.getUuid() != null
                    ? descriptor.getUuid().toString()
                    : "";

                handleServerReadRequest(
                    device,
                    requestId,
                    offset,
                    parent,
                    descriptorUuid);
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
                        sendServerResponse(device, requestId, BluetoothGatt.GATT_SUCCESS, offset, value);

                    return;
                }

                if (preparedWrite)
                {
                    handlePreparedWrite(device, requestId, parent, descriptor, responseNeeded, offset, value);
                    return;
                }

                String descriptorUuid = descriptor != null && descriptor.getUuid() != null
                    ? descriptor.getUuid().toString()
                    : "";

                handleServerWriteRequest(
                    device,
                    requestId,
                    parent,
                    descriptorUuid,
                    responseNeeded,
                    value);
            }


            // The end of a prepared (long) write: each attribute the central
            // prepared reaches GML as one write request with its fragments
            // assembled, and the execute is answered once, when GML has
            // answered all of them.
            @Override
            public void onExecuteWrite(
                BluetoothDevice device,
                int requestId,
                boolean execute)
            {
                if (generation.get() != workerGeneration)
                    return;

                LinkedHashMap<Object, LeServerPreparedWrite> prepared;

                synchronized (leServerRequestLock)
                {
                    prepared = leServerPreparedWrites.remove(safeAddress(device));
                }

                // Cancelled, or nothing was prepared: nothing to deliver.
                if (!execute || prepared == null || prepared.isEmpty())
                {
                    sendServerResponse(device, requestId, BluetoothGatt.GATT_SUCCESS, 0, null);
                    return;
                }

                GMFunction callback = callbackLeServerWriteRequest;

                if (callback == null)
                {
                    sendServerResponse(device, requestId, ATT_REQUEST_NOT_SUPPORTED, 0, null);
                    return;
                }

                long connection = resolveServerConnection(device);

                LeServerWriteBatch batch = new LeServerWriteBatch();
                batch.device = device;
                batch.stackRequestId = requestId;
                batch.remaining = prepared.size();

                ArrayList<Integer> ids = new ArrayList<>();
                ArrayList<LeServerRequestEntry> requests = new ArrayList<>();

                synchronized (leServerRequestLock)
                {
                    for (LeServerPreparedWrite write : prepared.values())
                    {
                        LeServerRequestEntry request = new LeServerRequestEntry();
                        request.stackRequestId = requestId;
                        request.connection = connection;
                        request.device = device;
                        request.serviceUuid = characteristicServiceUuid(write.characteristic);
                        request.characteristicUuid = characteristicUuidOf(write.characteristic);
                        request.descriptorUuid = write.descriptor != null && write.descriptor.getUuid() != null
                            ? write.descriptor.getUuid().toString()
                            : "";
                        request.isWrite = true;
                        request.responseNeeded = true;
                        request.writeValue = write.value;
                        request.batch = batch;

                        ids.add(storeLeServerRequest(request));
                        requests.add(request);
                    }
                }

                for (int i = 0; i < ids.size(); i++)
                {
                    LeServerRequestEntry request = requests.get(i);
                    invoke(
                        callback,
                        ids.get(i),
                        (double) connection,
                        request.serviceUuid,
                        request.characteristicUuid,
                        request.descriptorUuid);
                }
            }
        };
    }


    private void sendServerResponse(
        BluetoothDevice device,
        int stackRequestId,
        int status,
        int offset,
        byte[] value)
    {
        BluetoothGattServer server = gattServer;

        if (server == null || device == null)
            return;

        try
        {
            server.sendResponse(device, stackRequestId, status, offset, value);
        }
        catch (Throwable ignored)
        {
        }
    }


    // A read GML answers. With no handler registered nobody will, so the
    // central hears now instead of waiting out the ATT timeout.
    private void handleServerReadRequest(
        BluetoothDevice device,
        int stackRequestId,
        int offset,
        BluetoothGattCharacteristic characteristic,
        String descriptorUuid)
    {
        GMFunction callback = callbackLeServerReadRequest;

        if (callback == null)
        {
            sendServerResponse(device, stackRequestId, ATT_REQUEST_NOT_SUPPORTED, offset, null);
            return;
        }

        long connection = resolveServerConnection(device);

        LeServerRequestEntry request = new LeServerRequestEntry();
        request.stackRequestId = stackRequestId;
        request.connection = connection;
        request.device = device;
        request.serviceUuid = characteristicServiceUuid(characteristic);
        request.characteristicUuid = characteristicUuidOf(characteristic);
        request.descriptorUuid = descriptorUuid;
        request.isWrite = false;
        request.responseNeeded = true;

        int requestId;

        synchronized (leServerRequestLock)
        {
            requestId = storeLeServerRequest(request);
        }

        invoke(
            callback,
            requestId,
            (double) connection,
            request.serviceUuid,
            request.characteristicUuid,
            request.descriptorUuid,
            offset);
    }


    // A write that is not part of a prepared write.
    private void handleServerWriteRequest(
        BluetoothDevice device,
        int stackRequestId,
        BluetoothGattCharacteristic characteristic,
        String descriptorUuid,
        boolean responseNeeded,
        byte[] value)
    {
        GMFunction callback = callbackLeServerWriteRequest;

        if (callback == null)
        {
            if (responseNeeded)
                sendServerResponse(device, stackRequestId, ATT_REQUEST_NOT_SUPPORTED, 0, null);
            return;
        }

        long connection = resolveServerConnection(device);

        LeServerRequestEntry request = new LeServerRequestEntry();
        request.stackRequestId = stackRequestId;
        request.connection = connection;
        request.device = device;
        request.serviceUuid = characteristicServiceUuid(characteristic);
        request.characteristicUuid = characteristicUuidOf(characteristic);
        request.descriptorUuid = descriptorUuid;
        request.isWrite = true;
        request.responseNeeded = responseNeeded;
        request.writeValue = value != null ? value : new byte[0];

        int requestId;

        synchronized (leServerRequestLock)
        {
            requestId = storeLeServerRequest(request);
        }

        invoke(
            callback,
            requestId,
            (double) connection,
            request.serviceUuid,
            request.characteristicUuid,
            request.descriptorUuid);
    }


    // One fragment of a prepared write: laid out at its offset in the
    // attribute's buffer and echoed back, until the Execute Write.
    private void handlePreparedWrite(
        BluetoothDevice device,
        int stackRequestId,
        BluetoothGattCharacteristic characteristic,
        BluetoothGattDescriptor descriptor,
        boolean responseNeeded,
        int offset,
        byte[] value)
    {
        byte[] fragment = value != null ? value : new byte[0];
        boolean tooLong = offset < 0 || offset + fragment.length > MAX_ATTRIBUTE_LENGTH;

        if (!tooLong)
        {
            synchronized (leServerRequestLock)
            {
                String key = safeAddress(device);
                LinkedHashMap<Object, LeServerPreparedWrite> writes = leServerPreparedWrites.get(key);

                if (writes == null)
                {
                    writes = new LinkedHashMap<>();
                    leServerPreparedWrites.put(key, writes);
                }

                Object attribute = descriptor != null ? descriptor : characteristic;
                LeServerPreparedWrite prepared = writes.get(attribute);

                if (prepared == null)
                {
                    prepared = new LeServerPreparedWrite();
                    prepared.characteristic = characteristic;
                    prepared.descriptor = descriptor;
                    writes.put(attribute, prepared);
                }

                int length = Math.max(prepared.value.length, offset + fragment.length);

                if (length > prepared.value.length)
                    prepared.value = Arrays.copyOf(prepared.value, length);

                System.arraycopy(fragment, 0, prepared.value, offset, fragment.length);
            }
        }

        if (responseNeeded)
            sendServerResponse(
                device,
                stackRequestId,
                tooLong ? ATT_INVALID_ATTRIBUTE_LENGTH : BluetoothGatt.GATT_SUCCESS,
                offset,
                tooLong ? null : fragment);
    }


    // Caller holds leServerRequestLock. Stores a request under a new extension
    // id, after dropping the ones past their lifetime.
    private int storeLeServerRequest(LeServerRequestEntry request)
    {
        expireLeServerRequests();

        int requestId = nextLeServerRequestId++;

        if (nextLeServerRequestId <= 0)
            nextLeServerRequestId = 1;

        leServerRequests.put(requestId, request);
        return requestId;
    }


    // Caller holds leServerRequestLock. Runs on each new request, so neither
    // the no-response writes of a streaming central nor requests nobody
    // answered can pile up.
    private void expireLeServerRequests()
    {
        long now = System.nanoTime();
        Iterator<Map.Entry<Integer, LeServerRequestEntry>> iterator =
            leServerRequests.entrySet().iterator();

        while (iterator.hasNext())
        {
            LeServerRequestEntry request = iterator.next().getValue();
            long ttl = request.responseNeeded
                ? SERVER_REQUEST_TTL_NANOS
                : NO_RESPONSE_WRITE_TTL_NANOS;

            if (now - request.receivedAtNanos > ttl)
                iterator.remove();
        }
    }


    // A central that disconnected can no longer be answered.
    private void dropLeServerRequestsOf(BluetoothDevice device)
    {
        String address = safeAddress(device);

        synchronized (leServerRequestLock)
        {
            leServerPreparedWrites.remove(address);

            Iterator<Map.Entry<Integer, LeServerRequestEntry>> iterator =
                leServerRequests.entrySet().iterator();

            while (iterator.hasNext())
            {
                LeServerRequestEntry request = iterator.next().getValue();

                if (address.equals(safeAddress(request.device)))
                    iterator.remove();
            }
        }
    }


    // Answers every request still waiting, before the server stops or drops
    // its services. An executed prepared write is answered once.
    private void answerPendingLeServerRequests()
    {
        ArrayList<LeServerRequestEntry> pending;

        synchronized (leServerRequestLock)
        {
            pending = new ArrayList<>(leServerRequests.values());
            leServerRequests.clear();
            leServerPreparedWrites.clear();
        }

        Set<LeServerWriteBatch> answeredBatches =
            java.util.Collections.newSetFromMap(new IdentityHashMap<LeServerWriteBatch, Boolean>());

        for (LeServerRequestEntry request : pending)
        {
            if (!request.responseNeeded)
                continue;

            if (request.batch != null && !answeredBatches.add(request.batch))
                continue;

            sendServerResponse(request.device, request.stackRequestId, ATT_UNLIKELY_ERROR, 0, null);
        }
    }


    // One attribute of an executed prepared write has been answered; the
    // execute itself is answered after the last one, with the first error.
    private void finishLeServerWriteBatchPart(LeServerWriteBatch batch, int status)
    {
        boolean done;

        synchronized (leServerRequestLock)
        {
            if (status != BluetoothGatt.GATT_SUCCESS && batch.status == BluetoothGatt.GATT_SUCCESS)
                batch.status = status;

            batch.remaining--;
            done = batch.remaining == 0;
        }

        if (done)
            sendServerResponse(batch.device, batch.stackRequestId, batch.status, 0, null);
    }


    private void stopLeServerInternal(BluetoothError addError, String addMessage)
    {
        leServerRunning.set(false);

        // Answered while the server can still send; it forgets them on close.
        answerPendingLeServerRequests();

        synchronized (leServerInitialValueLock)
        {
            leServerInitialValues.clear();
        }

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
            leServerPreparedWrites.clear();
        }

        synchronized (leServerSubscriberLock)
        {
            leServerSubscribers.clear();
        }

        // The server is closed, so no onServiceAdded will come for any of them.
        failServiceAdds(addError, addMessage, true);
    }


    // Hands the head of the add queue to the stack unless one is in flight. An
    // add the stack refuses is failed here and the next one tried.
    private void startNextServiceAdd()
    {
        while (true)
        {
            LeServerAddEntry head;

            synchronized (leServerAddServiceLock)
            {
                head = leServerAddQueue.peekFirst();

                if (head == null || head.inFlight)
                    return;

                head.inFlight = true;
            }

            BluetoothGattServer server = gattServer;
            String failure;

            try
            {
                failure = server != null && server.addService(head.service)
                    ? null
                    : "addService() failed to start";
            }
            catch (Throwable throwable)
            {
                failure = throwableMessage(throwable);
            }

            if (failure == null)
                return;

            boolean fire;

            synchronized (leServerAddServiceLock)
            {
                // A stop may have failed and dropped it meanwhile.
                if (leServerAddQueue.peekFirst() != head)
                    return;

                leServerAddQueue.pollFirst();
                fire = !head.cancelled;
            }

            if (fire)
                invoke(head.callback, OPERATION_FAILED, failure);
        }
    }


    // Fails every pending add once. While the server stays open, an add the
    // stack already holds stays at the head, cancelled, because the stack takes
    // no other add until its onServiceAdded arrives.
    private void failServiceAdds(BluetoothError error, String message, boolean serverClosed)
    {
        ArrayList<LeServerAddEntry> failed = new ArrayList<>();

        synchronized (leServerAddServiceLock)
        {
            LeServerAddEntry head = leServerAddQueue.peekFirst();

            for (LeServerAddEntry entry : leServerAddQueue)
            {
                if (!entry.cancelled)
                    failed.add(entry);
            }

            leServerAddQueue.clear();

            if (!serverClosed && head != null && head.inFlight)
            {
                head.cancelled = true;
                leServerAddQueue.addFirst(head);
            }
        }

        for (LeServerAddEntry entry : failed)
            invoke(entry.callback, error, message);
    }


    @Override
    public BluetoothError bluetooth_le_server_start()
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
            return OK;

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

            return OK;
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
    public BluetoothError bluetooth_le_server_stop()
    {
        if (!initialized)
            return result(NOT_INITIALIZED, "Bluetooth is not initialized");

        stopLeServerInternal(
            OPERATION_FAILED,
            "BLE server stopped before the service was added");
        return OK;
    }


    @Override
    public boolean bluetooth_le_server_is_running()
    {
        return initialized && leServerRunning.get();
    }


    @Override
    public BluetoothError bluetooth_le_server_add_service(BluetoothLeServiceDefinition service, GMFunction callback)
    {
        if (!initialized)
            return result(NOT_INITIALIZED, "Bluetooth is not initialized");

        if (!leServerRunning.get() || gattServer == null)
            return result(OPERATION_FAILED, "BLE server is not running");

        if (service == null)
            return result(INVALID_ARGUMENT, "service cannot be null");

        final BluetoothGattService gattService;
        final String serviceUuid;
        final IdentityHashMap<BluetoothGattCharacteristic, byte[]> initialValues = new IdentityHashMap<>();

        try
        {
            serviceUuid = objectString(service, "uuid");
            if (serviceUuid == null || serviceUuid.isEmpty())
                return result(INVALID_ARGUMENT, "service.uuid cannot be empty");

            UUID parsedServiceUuid = parseUuid(serviceUuid);

            gattService = new BluetoothGattService(
                parsedServiceUuid,
                BluetoothGattService.SERVICE_TYPE_PRIMARY);

            Object[] characteristics = objectArray(service, "characteristics");
            for (Object characteristicObject : characteristics)
            {
                if (characteristicObject == null)
                    continue;

                String characteristicUuid = objectString(characteristicObject, "uuid");
                int properties = objectInt(characteristicObject, "properties", 0);
                int permissions = objectInt(characteristicObject, "permissions", 0);

                BluetoothGattCharacteristic characteristic =
                    new BluetoothGattCharacteristic(
                        parseUuid(characteristicUuid),
                        properties,
                        permissions);

                String initialValue = objectNullableString(characteristicObject, "value");
                if (initialValue != null && !initialValue.isEmpty())
                {
                    byte[] bytes = decodeBase64(initialValue);
                    characteristic.setValue(bytes);
                    initialValues.put(characteristic, bytes);
                }

                Object[] descriptors = objectArray(characteristicObject, "descriptors");
                for (Object descriptorObject : descriptors)
                {
                    if (descriptorObject == null)
                        continue;

                    String descriptorUuid = objectString(descriptorObject, "uuid");
                    characteristic.addDescriptor(
                        new BluetoothGattDescriptor(
                            parseUuid(descriptorUuid),
                            BluetoothGattDescriptor.PERMISSION_READ |
                                BluetoothGattDescriptor.PERMISSION_WRITE));
                }

                boolean supportsNotifyOrIndicate =
                    (properties &
                        (BluetoothGattCharacteristic.PROPERTY_NOTIFY |
                            BluetoothGattCharacteristic.PROPERTY_INDICATE)) != 0;

                if (supportsNotifyOrIndicate &&
                    characteristic.getDescriptor(CCCD_UUID) == null)
                {
                    characteristic.addDescriptor(
                        new BluetoothGattDescriptor(
                            CCCD_UUID,
                            BluetoothGattDescriptor.PERMISSION_READ |
                                BluetoothGattDescriptor.PERMISSION_WRITE));
                }

                gattService.addCharacteristic(characteristic);
            }
        }
        catch (Throwable throwable)
        {
            return result(
                INVALID_ARGUMENT,
                "Invalid service definition: " + throwableMessage(throwable));
        }

        synchronized (leServerInitialValueLock)
        {
            leServerInitialValues.putAll(initialValues);
        }

        // A failure from here on, the stack refusing the add included, is
        // reported through the callback.
        LeServerAddEntry entry = new LeServerAddEntry();
        entry.service = gattService;
        entry.callback = callback;

        synchronized (leServerAddServiceLock)
        {
            leServerAddQueue.addLast(entry);
        }

        startNextServiceAdd();

        return OK;
    }


    @Override
    public BluetoothError bluetooth_le_server_clear_services()
    {
        if (!initialized)
            return result(NOT_INITIALIZED, "Bluetooth is not initialized");

        BluetoothGattServer server = gattServer;

        if (server == null)
            return result(OPERATION_FAILED, "BLE server is not running");

        // The services the requests name are going away.
        answerPendingLeServerRequests();

        synchronized (leServerInitialValueLock)
        {
            leServerInitialValues.clear();
        }

        try
        {
            server.clearServices();
        }
        catch (Throwable throwable)
        {
            return result(OPERATION_FAILED, throwableMessage(throwable));
        }

        failServiceAdds(
            OPERATION_FAILED,
            "Services were cleared before the service was added",
            false);

        return OK;
    }


    @Override
    public BluetoothError bluetooth_le_server_respond_read(
        int request_id,
        int error_code,
        ByteBuffer data,
        int offset,
        int size)
    {
        if (!initialized)
            return result(NOT_INITIALIZED, "Bluetooth is not initialized");

        // Everything is checked before the request is removed, so a bad call
        // can be retried instead of leaving the central to time out.
        LeServerRequestEntry request;

        synchronized (leServerRequestLock)
        {
            request = leServerRequests.get(request_id);
        }

        if (request == null)
            return result(INVALID_HANDLE, "Unknown or expired request_id");

        if (request.isWrite)
            return result(
                INVALID_ARGUMENT,
                "request_id is a write request; answer it with bluetooth_le_server_respond_write");

        if (gattServer == null)
            return result(OPERATION_FAILED, "BLE server is not running");

        byte[] payload = new byte[0];

        if (error_code == OK.value() && size > 0)
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

        synchronized (leServerRequestLock)
        {
            if (leServerRequests.remove(request_id) == null)
                return result(INVALID_HANDLE, "Unknown or expired request_id");
        }

        int status = error_code == OK.value()
            ? BluetoothGatt.GATT_SUCCESS
            : BluetoothGatt.GATT_FAILURE;

        BluetoothGattServer server = gattServer;

        if (server == null)
            return result(OPERATION_FAILED, "BLE server is not running");

        try
        {
            // The response payload above is already the exact slice the
            // caller intends to answer with, so the ATT-level offset
            // handed back to Android is always 0.
            server.sendResponse(request.device, request.stackRequestId, status, 0, payload);
            return OK;
        }
        catch (Throwable throwable)
        {
            return result(OPERATION_FAILED, throwableMessage(throwable));
        }
    }


    @Override
    public BluetoothError bluetooth_le_server_respond_write(int request_id, int error_code)
    {
        if (!initialized)
            return result(NOT_INITIALIZED, "Bluetooth is not initialized");

        LeServerRequestEntry request;

        synchronized (leServerRequestLock)
        {
            request = leServerRequests.get(request_id);
        }

        if (request == null)
            return result(INVALID_HANDLE, "Unknown or expired request_id");

        if (!request.isWrite)
            return result(
                INVALID_ARGUMENT,
                "request_id is a read request; answer it with bluetooth_le_server_respond_read");

        if (gattServer == null)
            return result(OPERATION_FAILED, "BLE server is not running");

        synchronized (leServerRequestLock)
        {
            if (leServerRequests.remove(request_id) == null)
                return result(INVALID_HANDLE, "Unknown or expired request_id");
        }

        // A write without response was never waiting on an answer.
        if (!request.responseNeeded)
            return OK;

        int status = error_code == OK.value()
            ? BluetoothGatt.GATT_SUCCESS
            : BluetoothGatt.GATT_FAILURE;

        if (request.batch != null)
        {
            finishLeServerWriteBatchPart(request.batch, status);
            return OK;
        }

        BluetoothGattServer server = gattServer;

        if (server == null)
            return result(OPERATION_FAILED, "BLE server is not running");

        try
        {
            server.sendResponse(request.device, request.stackRequestId, status, 0, request.writeValue);
            return OK;
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
    public BluetoothError bluetooth_le_server_notify_value(
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
            service = server.getService(parseUuid(service_uuid));
        }
        catch (Throwable throwable)
        {
            return result(INVALID_ARGUMENT, "Invalid service_uuid");
        }

        if (service == null)
            return result(INVALID_HANDLE, "Unknown local service_uuid");

        try
        {
            characteristic = service.getCharacteristic(parseUuid(characteristic_uuid));
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
            // Keyed as the CCCD interception keys it, from the resolved
            // objects, so the caller's spelling of the UUIDs does not matter.
            String key = subscriberKey(
                characteristicServiceUuid(characteristic),
                characteristicUuidOf(characteristic));

            synchronized (leServerSubscriberLock)
            {
                HashSet<Long> subscribers = leServerSubscribers.get(key);

                if (subscribers != null)
                    targets.addAll(subscribers);
            }
        }

        if (targets.isEmpty())
            return OK;

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

        return OK;
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


    // =========================================================================
    // Pairing (bonding)
    // =========================================================================

    private final BroadcastReceiver bondReceiver = new BroadcastReceiver()
    {
        @Override
        public void onReceive(Context receiverContext, Intent intent)
        {
            if (!initialized || intent == null)
                return;

            if (!BluetoothDevice.ACTION_BOND_STATE_CHANGED.equals(intent.getAction()))
                return;

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

            int bondState = intent.getIntExtra(
                BluetoothDevice.EXTRA_BOND_STATE,
                BluetoothDevice.BOND_NONE);

            // Still in progress - wait for the terminal BOND_BONDED/BOND_NONE state.
            if (bondState == BluetoothDevice.BOND_BONDING)
                return;

            String address = device.getAddress();
            long deviceHandle;
            GMFunction callback;

            synchronized (pairLock)
            {
                Long handle = pairDeviceHandles.remove(address);

                if (handle == null)
                    return;

                deviceHandle = handle;
                callback = pairCallbacks.remove(address);
            }

            if (bondState == BluetoothDevice.BOND_BONDED)
            {
                invoke(callback, OK, "", (double) deviceHandle);
            }
            else
            {
                invoke(
                    callback,
                    OPERATION_FAILED,
                    "Pairing failed or was rejected",
                    (double) deviceHandle);
            }
        }
    };


    private void ensureBondReceiver()
    {
        if (bondReceiverRegistered)
            return;

        Activity current = activity();

        if (current == null)
            return;

        IntentFilter filter = new IntentFilter(BluetoothDevice.ACTION_BOND_STATE_CHANGED);

        //noinspection deprecation
        current.registerReceiver(bondReceiver, filter);
        bondReceiverRegistered = true;
    }


    private void unregisterBondReceiver()
    {
        if (!bondReceiverRegistered)
            return;

        Activity current = activity();

        if (current != null)
        {
            try
            {
                current.unregisterReceiver(bondReceiver);
            }
            catch (Throwable ignored)
            {
            }
        }

        bondReceiverRegistered = false;
    }


    @Override
    public boolean bluetooth_pairing_is_supported(long device)
    {
        if (!initialized || adapter == null)
            return false;

        DeviceEntry entry = copyDevice(device);
        if (entry == null)
            return false;

        // Android exposes explicit bonding through BluetoothDevice.createBond()
        // for both Classic and BLE devices.
        return entry.androidDevice != null ||
            (entry.address != null && !entry.address.isEmpty());
    }


    @Override
    public BluetoothError bluetooth_pair(long device, GMFunction callback)
    {
        if (!initialized || adapter == null)
            return result(
                NOT_INITIALIZED,
                "Bluetooth is not initialized");

        DeviceEntry deviceEntry = copyDevice(device);

        if (deviceEntry == null)
            return result(
                INVALID_HANDLE,
                "Invalid device handle");

        if (!hasConnectPermission())
            return result(
                PERMISSION_DENIED,
                "Bluetooth connect permission is not granted");

        if (!adapterEnabled())
            return result(
                BLUETOOTH_DISABLED,
                "Bluetooth is disabled");

        BluetoothDevice androidDevice = deviceEntry.androidDevice;

        if (androidDevice == null)
        {
            if (deviceEntry.address == null || deviceEntry.address.isEmpty())
                return result(
                    INVALID_ARGUMENT,
                    "Bluetooth device has no usable address");

            androidDevice = adapter.getRemoteDevice(deviceEntry.address);
        }

        if (androidDevice.getBondState() == BluetoothDevice.BOND_BONDED)
        {
            invoke(callback, OK, "", (double) device);
            return OK;
        }

        ensureBondReceiver();

        String address = androidDevice.getAddress();

        synchronized (pairLock)
        {
            // createBond() refuses a second request while bonding, so a second
            // call would only replace the first callback and then fail.
            if (pairDeviceHandles.containsKey(address))
                return result(BUSY, "Pairing is already in progress for this device");

            pairDeviceHandles.put(address, device);

            if (callback != null)
                pairCallbacks.put(address, callback);
        }

        boolean started;

        try
        {
            started = androidDevice.createBond();
        }
        catch (Throwable throwable)
        {
            synchronized (pairLock)
            {
                pairDeviceHandles.remove(address);
                pairCallbacks.remove(address);
            }

            return result(
                OPERATION_FAILED,
                "createBond() threw: " + throwableMessage(throwable));
        }

        if (!started)
        {
            synchronized (pairLock)
            {
                pairDeviceHandles.remove(address);
                pairCallbacks.remove(address);
            }

            return result(
                OPERATION_FAILED,
                "createBond() returned false");
        }

        return OK;
    }


    @Override
    public boolean bluetooth_device_is_paired(long device)
    {
        if (!initialized || adapter == null)
            return false;

        DeviceEntry deviceEntry = copyDevice(device);

        if (deviceEntry == null)
            return false;

        BluetoothDevice androidDevice = deviceEntry.androidDevice;

        if (androidDevice == null)
        {
            if (deviceEntry.address == null || deviceEntry.address.isEmpty())
                return false;

            androidDevice = adapter.getRemoteDevice(deviceEntry.address);
        }

        return androidDevice.getBondState() == BluetoothDevice.BOND_BONDED;
    }


    @Override
    public BluetoothError bluetooth_classic_scan_start()
    {
        if (!initialized || adapter == null)
            return result(
                NOT_INITIALIZED,
                "Bluetooth is not initialized");

        if (!hasScanPermission() || !hasConnectPermission())
            return result(
                PERMISSION_DENIED,
                "Bluetooth scan/connect permission is not granted");

        if (scanNeedsLocationOn())
            return result(PERMISSION_DENIED, LOCATION_OFF_MESSAGE);

        if (!adapterEnabled())
            return result(
                BLUETOOTH_DISABLED,
                "Bluetooth is disabled");

        if (classicScanning.get())
            return OK;

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
            return OK;
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
    public BluetoothError bluetooth_classic_scan_stop()
    {
        if (!initialized)
            return result(
                NOT_INITIALIZED,
                "Bluetooth is not initialized");

        if (!classicScanning.get())
            return OK;

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

        return OK;
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
    public BluetoothTransport bluetooth_device_get_transport(long device)
    {
        DeviceEntry entry = copyDevice(device);
        return BluetoothTransport.from(entry != null ? entry.transport : TRANSPORT_UNKNOWN);
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
            uuid = parseUuid(service_uuid);
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


    // Sends leave the game thread: bluetooth_classic_send queues and this
    // thread writes. RFCOMM is credit-based, so write() blocks for as long as
    // the peer does not read. A failed write closes the socket, and the read
    // loop reports the end through classic_disconnected as for any socket
    // error.
    private void startWriteLoop(
        final ConnectionEntry entry,
        final BluetoothSocket socket,
        final long workerGeneration)
    {
        Thread thread = new Thread(
            () ->
            {
                try
                {
                    OutputStream output = socket.getOutputStream();

                    while (true)
                    {
                        byte[] chunk;

                        synchronized (entry.sendLock)
                        {
                            while (
                                entry.sendQueue.isEmpty() &&
                                !entry.closeAfterSend &&
                                !entry.finished &&
                                initialized &&
                                generation.get() == workerGeneration)
                            {
                                entry.sendLock.wait();
                            }

                            if (
                                entry.finished ||
                                !initialized ||
                                generation.get() != workerGeneration)
                            {
                                entry.sendQueue.clear();
                                entry.sendQueuedBytes = 0;
                                return;
                            }

                            chunk = entry.sendQueue.pollFirst();

                            // Asked to close, and everything queued before
                            // the ask has been written.
                            if (chunk == null)
                                break;
                        }

                        output.write(chunk);

                        synchronized (entry.sendLock)
                        {
                            entry.sendQueuedBytes -= chunk.length;
                        }
                    }
                }
                catch (IOException | InterruptedException ignored)
                {
                    // Reported by the read loop once the close below ends it.
                }

                try
                {
                    socket.close();
                }
                catch (Throwable ignored)
                {
                }
            },
            "GMBluetooth-RFCOMM-Write-" + entry.handle);

        thread.setDaemon(true);
        thread.start();
    }


    private void startReadLoop(
        final long connection,
        final BluetoothSocket socket,
        final long workerGeneration)
    {
        final ConnectionEntry readEntry = getConnection(connection);

        if (readEntry != null)
            startWriteLoop(readEntry, socket, workerGeneration);

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

                    // Shutdown and a stale generation skip dispatchDisconnected;
                    // the writer still has to learn the link is gone.
                    if (readEntry != null)
                    {
                        readEntry.finished = true;
                        wakeWriter(readEntry);
                    }
                }
            },
            "GMBluetooth-RFCOMM-Read-" + connection);

        thread.setDaemon(true);
        thread.start();
    }


    @Override
    public BluetoothError bluetooth_classic_disconnect(long connection)
    {
        ConnectionEntry entry = getConnection(connection);

        if (entry == null)
            return result(
                INVALID_HANDLE,
                "Invalid Bluetooth Classic connection handle");

        if (entry.finished)
        {
            // The peer already hung up; the game is done with its unread bytes.
            eraseConnection(connection);
            return OK;
        }

        final BluetoothSocket socket = entry.socket;

        if (socket == null)
            return result(
                INVALID_HANDLE,
                "Bluetooth Classic socket is not connected");

        entry.manualClosing = true;

        if (!entry.connected)
        {
            // Still connecting: there is no writer and nothing queued yet, and
            // closing the socket is what ends the connect attempt.
            try
            {
                socket.close();
                return OK;
            }
            catch (IOException exception)
            {
                return result(
                    OPERATION_FAILED,
                    throwableMessage(exception));
            }
        }

        // Bytes already queued go out first: the writer closes the socket once
        // its queue is empty. A peer that stopped reading cannot hold the link
        // open past CLOSE_AFTER_SEND_TIMEOUT_MS.
        boolean pending;

        synchronized (entry.sendLock)
        {
            entry.closeAfterSend = true;
            pending = entry.sendQueuedBytes > 0;
            entry.sendLock.notifyAll();
        }

        if (pending)
        {
            Thread closer = new Thread(
                () ->
                {
                    try
                    {
                        Thread.sleep(CLOSE_AFTER_SEND_TIMEOUT_MS);
                    }
                    catch (InterruptedException ignored)
                    {
                    }

                    try
                    {
                        socket.close();
                    }
                    catch (Throwable ignored)
                    {
                    }
                },
                "GMBluetooth-RFCOMM-Close-" + connection);

            closer.setDaemon(true);
            closer.start();
        }

        return OK;
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
    public BluetoothError bluetooth_classic_send(
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

        if (socket == null || !entry.connected || entry.finished)
            return result(
                DISCONNECTED,
                "Classic connection is not connected");

        if (bufferRangeInvalid(data, offset, size))
            return result(
                INVALID_ARGUMENT,
                "Invalid buffer offset/size for bluetooth_classic_send");

        if (size == 0)
            return OK;

        byte[] chunk = new byte[size];

        ByteBuffer view = data.duplicate();
        view.position(offset);
        view.get(chunk, 0, size);

        synchronized (entry.sendLock)
        {
            if (entry.closeAfterSend)
                return result(
                    DISCONNECTED,
                    "Classic connection is closing");

            // One send larger than the limit still goes when nothing else is
            // waiting, or it could never go at all.
            if (
                entry.sendQueuedBytes > 0 &&
                (long) entry.sendQueuedBytes + size > MAX_QUEUED_SEND_BYTES)
            {
                return result(
                    BUSY,
                    "Too many bytes are waiting to be sent on this Classic connection");
            }

            entry.sendQueue.addLast(chunk);
            entry.sendQueuedBytes += size;
            entry.sendLock.notifyAll();
        }

        return OK;
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
        boolean drained;

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
            drained = entry.finished && entry.receiveAvailable == 0;
        }

        // The last bytes of a connection the peer closed: nothing is left to
        // keep it for.
        if (drained)
            eraseConnection(connection);

        ByteBuffer view = out_data.duplicate();
        view.position(offset);
        view.put(copiedBytes);

        return copiedBytes.length;
    }


    // =========================================================================
    // Classic RFCOMM server
    // =========================================================================

    @Override
    public BluetoothError bluetooth_classic_server_start(
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
            return OK;

        if (service_uuid == null || service_uuid.isEmpty())
            return result(
                INVALID_ARGUMENT,
                "service_uuid cannot be empty");

        final UUID uuid;

        try
        {
            uuid = parseUuid(service_uuid);
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

        return OK;
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
    public BluetoothError bluetooth_classic_server_stop()
    {
        if (!initialized)
            return result(
                NOT_INITIALIZED,
                "Bluetooth is not initialized");

        stopServerInternal();
        return OK;
    }


    @Override
    public boolean bluetooth_classic_server_is_running()
    {
        return initialized && serverRunning.get();
    }


    @Override
    public BluetoothError bluetooth_classic_discoverable_start(int duration_seconds)
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

        return OK;
    }


    @Override
    public BluetoothError bluetooth_classic_discoverable_stop()
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
    public boolean bluetooth_set_callback_state_changed(GMFunction callback)
    {
        callbackStateChanged = callback;
        ensureStateReceiver();
        invoke(callbackStateChanged, currentBluetoothState());
        return true;
    }


    @Override
    public boolean bluetooth_remove_callback_state_changed()
    {
        callbackStateChanged = null;
        return true;
    }


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
