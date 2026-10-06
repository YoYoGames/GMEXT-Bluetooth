package ${YYAndroidPackageName};

import ${YYAndroidPackageName}.GMExtWire.GMFunction;
import ${YYAndroidPackageName}.enums.BluetoothAttError;
import ${YYAndroidPackageName}.enums.BluetoothError;
import ${YYAndroidPackageName}.enums.BluetoothFeature;
import ${YYAndroidPackageName}.enums.BluetoothLeAdvertiseTxPower;
import ${YYAndroidPackageName}.enums.BluetoothLeConnectionPriority;
import ${YYAndroidPackageName}.enums.BluetoothLeSubscribeMode;
import ${YYAndroidPackageName}.enums.BluetoothLeWriteType;
import ${YYAndroidPackageName}.enums.BluetoothPermissionStatus;
import ${YYAndroidPackageName}.enums.BluetoothTransport;
import ${YYAndroidPackageName}.records.BluetoothLeAdvertiseData;
import ${YYAndroidPackageName}.records.BluetoothLeAdvertiseManufacturerData;
import ${YYAndroidPackageName}.records.BluetoothLeAdvertiseServiceData;
import ${YYAndroidPackageName}.records.BluetoothLeAdvertiseSettings;
import ${YYAndroidPackageName}.records.BluetoothLeAdvertisement;
import ${YYAndroidPackageName}.records.BluetoothLeScanFilter;
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
import android.bluetooth.BluetoothStatusCodes;
import android.bluetooth.le.AdvertiseCallback;
import android.bluetooth.le.AdvertiseData;
import android.bluetooth.le.AdvertiseSettings;
import android.bluetooth.le.BluetoothLeAdvertiser;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanRecord;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.location.LocationManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.ParcelUuid;
import android.provider.Settings;
import android.util.SparseArray;

import java.io.Closeable;
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
import java.util.List;
import java.util.Map;
import java.util.Optional;
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
    private static final BluetoothError NOT_PERMITTED      = BluetoothError.NotPermitted;
    private static final BluetoothError INSUFFICIENT_SECURITY = BluetoothError.InsufficientSecurity;

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
    private static final int REQUEST_CODE_ENABLE = 0xB711;

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

    // Set by ACTION_DISCOVERY_STARTED for the discovery the current Classic
    // scan started; a FINISHED before it belongs to an older discovery.
    private volatile boolean classicDiscoveryStarted = false;

    private volatile boolean receiverRegistered = false;
    private volatile boolean stateReceiverRegistered = false;
    private volatile Context receiverContext = null;

    private volatile boolean initialized = false;

    // Incrementing this invalidates callbacks from worker threads belonging to
    // an older initialize/shutdown session.
    private final AtomicLong generation = new AtomicLong(1);

    // The LE connect window and the GATT operation timeouts run here.
    private final Handler timeoutHandler = new Handler(Looper.getMainLooper());

    private static final String NOT_INITIALIZED_MESSAGE = "Bluetooth is not initialized";
    private static final String NO_ADAPTER_MESSAGE = "This device has no Bluetooth adapter";
    private static final String RECEIVER_FAILED_MESSAGE = "Could not register for Bluetooth broadcasts";


    // =========================================================================
    // Error state
    // =========================================================================

    // Code and message change together, so a reader never pairs one call's
    // code with another's message. Only exports set it.
    private static final class LastError
    {
        final BluetoothError code;
        final String message;

        LastError(BluetoothError code, String message)
        {
            this.code = code;
            this.message = message;
        }
    }

    private volatile LastError lastError = new LastError(OK, "");


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

        // What the device has advertised, merged across packets as the native
        // core merges it (R1-138). UUIDs canonical lowercase; guarded by
        // deviceLock like the rest of the entry.
        final ArrayList<String> advServiceUuids = new ArrayList<>();
        final LinkedHashMap<String, byte[]> advServiceData = new LinkedHashMap<>();
        final LinkedHashMap<Integer, byte[]> advManufacturerData = new LinkedHashMap<>();
        Integer advTxPower = null;
    }

    // A device's advertisement keeps at most this many service UUIDs, and as
    // many service data and manufacturer entries, so a peripheral cycling
    // through keys cannot grow it further.
    private static final int MAX_ADVERTISEMENT_ENTRIES = 32;

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
        if (!initialized)
            return STATE_UNKNOWN;

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

    // The state the callback hears is read and delivered under one lock, so
    // the registration's first answer, a broadcast and a permission result
    // cannot reach GML out of order.
    private final Object stateLock = new Object();

    private void dispatchState()
    {
        synchronized (stateLock)
        {
            invoke(callbackStateChanged, currentBluetoothState());
        }
    }


    private final BroadcastReceiver stateReceiver = new BroadcastReceiver()
    {
        @Override
        public void onReceive(Context receiverContext, Intent intent)
        {
            try
            {
                if (!initialized || intent == null ||
                    !BluetoothAdapter.ACTION_STATE_CHANGED.equals(intent.getAction()))
                    return;

                dispatchState();

                int adapterState = intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, -1);

                if (adapterState == BluetoothAdapter.STATE_TURNING_OFF ||
                    adapterState == BluetoothAdapter.STATE_OFF)
                {
                    stopScansForRadioOff();

                    // The stack drops the advertisement with the radio; a start
                    // still waiting hears why.
                    stopLeAdvertiseInternal(BLUETOOTH_DISABLED, "Bluetooth was turned off");
                }
            }
            catch (Throwable ignored)
            {
            }
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

    // Every receiver lives on the application context, which outlives the
    // Activity, and is unregistered through it. From API 33 the export state
    // is explicit. It is RECEIVER_EXPORTED because these broadcasts come from
    // the Bluetooth process, not from the system server: a not-exported
    // receiver would refuse them. Every action registered is a protected
    // broadcast, so no other app can send one.
    private boolean registerSystemReceiver(BroadcastReceiver receiver, IntentFilter filter)
    {
        Context current = context();

        if (current == null)
            return false;

        try
        {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU)
                current.registerReceiver(receiver, filter, Context.RECEIVER_EXPORTED);
            else
                current.registerReceiver(receiver, filter);

            receiverContext = current;
            return true;
        }
        catch (Throwable ignored)
        {
            return false;
        }
    }


    // Unregistered through the context that registered it, which is still
    // there when the Activity is not.
    private void unregisterSystemReceiver(BroadcastReceiver receiver)
    {
        Context current = receiverContext != null ? receiverContext : context();

        if (current == null)
            return;

        try
        {
            current.unregisterReceiver(receiver);
        }
        catch (Throwable ignored)
        {
        }
    }


    // The typed overload is unreliable on API 33, so it is used from 34.
    private static BluetoothDevice deviceExtra(Intent intent)
    {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
            return intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE, BluetoothDevice.class);

        //noinspection deprecation
        return intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
    }


    private void ensureStateReceiver()
    {
        if (stateReceiverRegistered)
            return;

        stateReceiverRegistered = registerSystemReceiver(
            stateReceiver,
            new IntentFilter(BluetoothAdapter.ACTION_STATE_CHANGED));
    }

    private void unregisterStateReceiver()
    {
        if (!stateReceiverRegistered)
            return;

        unregisterSystemReceiver(stateReceiver);
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

        // A client connect still in progress. Whoever clears connectPending
        // first - the connect thread, a cancelling disconnect or shutdown -
        // fires connectCallback, so it fires once.
        final AtomicBoolean connectPending = new AtomicBoolean(false);
        volatile GMFunction connectCallback = null;

        // Guarded by receiveLock. The read loop waits on it while the queue
        // is full; dataPending is set once classic_data has announced the
        // bytes and cleared by bluetooth_classic_receive.
        final Object receiveLock = new Object();
        final ArrayDeque<byte[]> receiveChunks = new ArrayDeque<>();
        int receiveAvailable = 0;
        boolean dataPending = false;

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

    // While this many received bytes wait for the game, the read loop stops
    // reading and RFCOMM's credits hold the peer back.
    private static final int MAX_QUEUED_RECEIVE_BYTES = 1024 * 1024;

    // How long bluetooth_classic_disconnect lets queued bytes drain to a peer
    // that has stopped reading before the socket is closed anyway.
    private static final long CLOSE_AFTER_SEND_TIMEOUT_MS = 2000;

    private final Object connectionLock = new Object();
    private final HashMap<Long, ConnectionEntry> connections = new HashMap<>();
    private long nextConnectionId = 1;

    // One RFCOMM server run: its accept thread works on this instance only,
    // so a stop followed by a new start never shares a socket or a flag
    // with the old thread.
    private static final class ClassicServer
    {
        final String name;
        final UUID uuid;
        volatile BluetoothServerSocket socket = null;
        volatile boolean running = true;

        ClassicServer(String name, UUID uuid)
        {
            this.name = name;
            this.uuid = uuid;
        }
    }

    // How long the accept thread waits before listening again after its
    // listener failed, doubling up to the cap.
    private static final long SERVER_RELISTEN_FIRST_MS = 250;
    private static final long SERVER_RELISTEN_MAX_MS = 2000;

    private final Object classicServerLock = new Object();
    private volatile ClassicServer currentServer = null;


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
        static final int KIND_REQUEST_MTU = 7;
        static final int KIND_READ_RSSI = 8;

        int kind;
        GMFunction callback;
        long targetHandle;
        int subscribeMode;
        GattOpStart start;
        String startFailureMessage = "";

        // Posted while the op is the current one; whoever takes the op
        // removes it.
        Runnable timeout;
    }

    private static final String LE_OP_DISCONNECTED_MESSAGE =
        "LE connection closed before the operation completed";
    private static final String SHUTDOWN_MESSAGE =
        "Bluetooth was shut down before the operation completed";

    // A device that has not answered a connect by then never will.
    private static final long LE_CONNECT_TIMEOUT_MS = 15_000;

    // The ATT transaction timeout: an op the stack has not completed by then
    // has stalled the link, which takes no other op until it does.
    private static final long GATT_OP_TIMEOUT_MS = 30_000;

    private static final class LeConnectionEntry
    {
        long handle;
        long device;
        boolean serverRole;

        volatile BluetoothGatt gatt = null;
        volatile BluetoothDevice remoteDevice = null;
        volatile boolean connected = false;
        volatile boolean manualClosing = false;

        // The ATT MTU: the default until onMtuChanged reports another.
        volatile int mtu = 23;

        // A client connect in progress, reported once by whoever clears
        // connectPending first: the stack, the connect window, a cancelling
        // disconnect or shutdown.
        final AtomicBoolean connectPending = new AtomicBoolean(false);
        volatile GMFunction connectCallback = null;
        volatile Runnable connectTimeout = null;

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

        // The first fragment's offset: the assembled write reaches GML as
        // the bytes from here on, at this offset.
        int offset = 0;
    }

    // A write without response needs no answer, so nothing else removes it.
    // It is kept this long for GML to read its value, then dropped.
    private static final long NO_RESPONSE_WRITE_TTL_NANOS = 5_000_000_000L;

    // The ATT transaction timeout: a request still waiting after this has
    // already cost the central its link.
    private static final long SERVER_REQUEST_TTL_NANOS = 30_000_000_000L;

    // The longest attribute value ATT allows.
    private static final int MAX_ATTRIBUTE_LENGTH = 512;

    // Every BluetoothLeAttributePermission flag (Android's PERMISSION_* bits).
    private static final int ATTRIBUTE_PERMISSION_ALL =
        BluetoothGattCharacteristic.PERMISSION_READ |
        BluetoothGattCharacteristic.PERMISSION_READ_ENCRYPTED |
        BluetoothGattCharacteristic.PERMISSION_READ_ENCRYPTED_MITM |
        BluetoothGattCharacteristic.PERMISSION_WRITE |
        BluetoothGattCharacteristic.PERMISSION_WRITE_ENCRYPTED |
        BluetoothGattCharacteristic.PERMISSION_WRITE_ENCRYPTED_MITM |
        BluetoothGattCharacteristic.PERMISSION_WRITE_SIGNED |
        BluetoothGattCharacteristic.PERMISSION_WRITE_SIGNED_MITM;

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

    // Advertising goes IDLE -> STARTING (startAdvertising called, its
    // AdvertiseCallback not heard yet) -> RUNNING, and back to IDLE on a
    // failed start or a stop. The fields below are guarded by leAdvertiseLock;
    // leAdvertiseCallback is the current start's, and a callback that is not
    // it belongs to a start already stopped.
    private static final int ADVERTISE_IDLE = 0;
    private static final int ADVERTISE_STARTING = 1;
    private static final int ADVERTISE_RUNNING = 2;

    private final Object leAdvertiseLock = new Object();
    private int leAdvertiseState = ADVERTISE_IDLE;
    private BluetoothLeAdvertiser leAdvertiser = null;
    private AdvertiseCallback leAdvertiseCallback = null;
    private GMFunction leAdvertiseStartCallback = null;

    private volatile BluetoothGattServer gattServer = null;
    private final AtomicBoolean leServerRunning = new AtomicBoolean(false);
    private final Object leServerAddServiceLock = new Object();
    // BluetoothGattServer takes one addService at a time, until its
    // onServiceAdded, so adds wait here in order; only the head is in flight.
    private final ArrayDeque<LeServerAddEntry> leServerAddQueue = new ArrayDeque<>();

    // The service UUIDs added or waiting to be, so a second add of one is
    // refused. Guarded by leServerAddServiceLock; cleared by clear and stop.
    private final HashSet<UUID> leServerServiceUuids = new HashSet<>();

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
    // we intercept and auto-acknowledge ourselves. Per characteristic object
    // of the server, the server connections subscribed to it and the
    // BluetoothLeSubscribeMode each wrote; the CCCD reads are answered from
    // here too.
    private final Object leServerSubscriberLock = new Object();
    private final IdentityHashMap<BluetoothGattCharacteristic, HashMap<Long, Integer>> leServerSubscribers =
        new IdentityHashMap<>();

    // A notification waiting for its central. The stack sends one per device
    // at a time and says so in onNotificationSent, which sends the next.
    private static final class LeServerNotification
    {
        BluetoothDevice device;
        BluetoothGattCharacteristic characteristic;
        byte[] value;
        boolean confirm;
    }

    private static final int MAX_QUEUED_NOTIFICATIONS = 64;

    // Per device address, its notifications in order; the head is the one
    // in flight when the address is in leServerNotifySending. The count
    // covers every queue. Guarded by leServerNotifyLock.
    private final Object leServerNotifyLock = new Object();
    private final HashMap<String, ArrayDeque<LeServerNotification>> leServerNotifyQueues = new HashMap<>();
    private final HashSet<String> leServerNotifySending = new HashSet<>();
    private int leServerNotifyCount = 0;


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


    // The spellings the native core's is_valid_uuid takes: 4 or 8 hex digits,
    // or the 36-character 8-4-4-4-12 form. UUID.fromString alone is looser
    // ("1-2-3-4-5" parses), so the new calls check here first and refuse the
    // same strings on every platform.
    private static boolean isValidUuid(String text)
    {
        if (text == null)
            return false;

        int length = text.length();

        if (length != 4 && length != 8 && length != 36)
            return false;

        for (int i = 0; i < length; i++)
        {
            char c = text.charAt(i);
            boolean dash = length == 36 && (i == 8 || i == 13 || i == 18 || i == 23);
            boolean hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');

            if (dash ? c != '-' : !hex)
                return false;
        }

        return true;
    }


    // Called from exports only: an event a worker thread fires carries its
    // own error and leaves the last error of the game's calls alone.
    private void setLastError(BluetoothError code, String message)
    {
        lastError = new LastError(code, message != null ? message : "");
    }


    // An export's result. Last-error is the detail for a failure; a success
    // leaves it alone.
    private BluetoothError result(BluetoothError code, String message)
    {
        if (code != OK)
            setLastError(code, message);

        return code;
    }


    // The first check of every export that uses the radio: NotInitialized
    // before bluetooth_initialize, NotSupported on a device without
    // Bluetooth. Sets the last error when it fails.
    private BluetoothError requireAdapter()
    {
        if (!initialized)
            return result(NOT_INITIALIZED, NOT_INITIALIZED_MESSAGE);

        if (adapter == null)
            return result(NOT_SUPPORTED, NO_ADAPTER_MESSAGE);

        return OK;
    }


    private static void closeQuietly(Closeable closeable)
    {
        if (closeable == null)
            return;

        try
        {
            closeable.close();
        }
        catch (Throwable ignored)
        {
        }
    }


    // Drops the link and frees the stack's client, in that order.
    private static void closeGatt(BluetoothGatt gatt)
    {
        if (gatt == null)
            return;

        try
        {
            gatt.disconnect();
        }
        catch (Throwable ignored)
        {
        }

        try
        {
            gatt.close();
        }
        catch (Throwable ignored)
        {
        }
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


    // Adds a device or merges what is new about it, as the native core does:
    // the transport is the first sighting's, a name only when it is not
    // empty, an RSSI only when one was measured, and connectable once seen
    // stays so. announce is false for a device that was not discovered - a
    // central on the GATT server, a paired device, an inbound RFCOMM peer -
    // so only a scan fires device_found, once per new device.
    private long upsertDevice(
        int transport,
        String id,
        String name,
        String address,
        int rssi,
        boolean hasRssi,
        boolean connectable,
        BluetoothDevice androidDevice,
        boolean announce)
    {
        return upsertDevice(
            transport, id, name, address, rssi, hasRssi, connectable, androidDevice, announce, null);
    }


    // A scan result also folds its advertisement in, under the same lock and
    // before device_found, so the game never reads a found device's record
    // without its first packet.
    private long upsertDevice(
        int transport,
        String id,
        String name,
        String address,
        int rssi,
        boolean hasRssi,
        boolean connectable,
        BluetoothDevice androidDevice,
        boolean announce,
        ScanRecord record)
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
                entry.transport = transport;

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

            if (!safeName.isEmpty())
                entry.name = safeName;

            if (!safeAddress.isEmpty())
            {
                entry.address = safeAddress;
                entry.addressAvailable = true;
            }

            if (hasRssi)
            {
                entry.rssi = rssi;
                entry.rssiAvailable = true;
            }

            entry.connectable = entry.connectable || connectable;

            if (androidDevice != null)
                entry.androidDevice = androidDevice;

            if (record != null)
                mergeAdvertisement(entry, record);
        }

        if (created && announce)
        {
            invoke(callbackDeviceFound, (double) handle);
        }

        return handle;
    }


    // Folds one packet into the entry as the core's merge_advertisement does:
    // service UUIDs collect, a service data or manufacturer entry is replaced
    // by the newer one for its key, and so is the TX power. Under deviceLock.
    // The arrays are copied, so the entry never aliases a ScanRecord.
    private static void mergeAdvertisement(DeviceEntry entry, ScanRecord record)
    {
        List<ParcelUuid> uuids = record.getServiceUuids();

        if (uuids != null)
        {
            for (ParcelUuid uuid : uuids)
            {
                if (uuid == null)
                    continue;

                // UUID.toString() is already the canonical lowercase form.
                String canonical = uuid.getUuid().toString();

                if (!entry.advServiceUuids.contains(canonical) &&
                    entry.advServiceUuids.size() < MAX_ADVERTISEMENT_ENTRIES)
                    entry.advServiceUuids.add(canonical);
            }
        }

        Map<ParcelUuid, byte[]> serviceData = record.getServiceData();

        if (serviceData != null)
        {
            for (Map.Entry<ParcelUuid, byte[]> item : serviceData.entrySet())
            {
                if (item.getKey() == null)
                    continue;

                String canonical = item.getKey().getUuid().toString();
                byte[] data = item.getValue() != null ? item.getValue().clone() : new byte[0];

                if (entry.advServiceData.containsKey(canonical) ||
                    entry.advServiceData.size() < MAX_ADVERTISEMENT_ENTRIES)
                    entry.advServiceData.put(canonical, data);
            }
        }

        SparseArray<byte[]> manufacturerData = record.getManufacturerSpecificData();

        if (manufacturerData != null)
        {
            for (int i = 0; i < manufacturerData.size(); i++)
            {
                Integer companyId = manufacturerData.keyAt(i);
                byte[] value = manufacturerData.valueAt(i);
                byte[] data = value != null ? value.clone() : new byte[0];

                if (entry.advManufacturerData.containsKey(companyId) ||
                    entry.advManufacturerData.size() < MAX_ADVERTISEMENT_ENTRIES)
                    entry.advManufacturerData.put(companyId, data);
            }
        }

        // Integer.MIN_VALUE is the record's "no TX Power Level field".
        int txPower = record.getTxPowerLevel();

        if (txPower != Integer.MIN_VALUE)
            entry.advTxPower = txPower;
    }


    private static List<Byte> toByteList(byte[] bytes)
    {
        ArrayList<Byte> values = new ArrayList<>(bytes != null ? bytes.length : 0);

        if (bytes != null)
        {
            for (byte value : bytes)
                values.add(value);
        }

        return values;
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

        invoke(callbackScanStopped, error, safeMessage, transport);
    }


    // Reports a client connect, once: false when a cancelling disconnect or
    // shutdown reported it already, and the caller drops the socket.
    private boolean dispatchConnectResult(
        ConnectionEntry entry,
        BluetoothError error,
        String message)
    {
        if (!entry.connectPending.getAndSet(false))
            return false;

        GMFunction callback = entry.connectCallback;
        entry.connectCallback = null;

        if (error == OK)
            entry.connected = true;
        else
            eraseConnection(entry.handle);

        invoke(
            callback,
            error,
            message != null ? message : "",
            (double) entry.handle,
            (double) entry.device);

        return true;
    }


    // An established link the game did not close has ended.
    private void dispatchDisconnected(
        ConnectionEntry entry,
        BluetoothError error,
        String message)
    {
        String safeMessage = message != null ? message : "";

        invoke(
            callbackClassicDisconnected,
            error,
            safeMessage,
            (double) entry.handle);

        // Bytes the game has not read yet outlive a remote hang-up: the entry
        // stays until bluetooth_classic_receive drains it,
        // bluetooth_classic_disconnect drops it or shutdown clears it.
        boolean keep;

        synchronized (entry.receiveLock)
        {
            entry.finished = true;
            entry.connected = false;
            keep = !entry.manualClosing && entry.receiveAvailable > 0;
        }

        wakeWriter(entry);

        if (!keep)
            eraseConnection(entry.handle);
    }


    private static void wakeWriter(ConnectionEntry entry)
    {
        synchronized (entry.sendLock)
        {
            entry.sendLock.notifyAll();
        }
    }


    // The read loop waits on receiveLock while the receive queue is full.
    private static void wakeReader(ConnectionEntry entry)
    {
        synchronized (entry.receiveLock)
        {
            entry.receiveLock.notifyAll();
        }
    }


    // classic_data fires for the first bytes after each
    // bluetooth_classic_receive, not for every chunk.
    private void appendReceived(ConnectionEntry entry, byte[] data)
    {
        if (data == null || data.length == 0)
            return;

        // Retired by bluetooth_classic_disconnect: nobody reads these.
        if (getConnection(entry.handle) != entry)
            return;

        int available;
        boolean announce;

        synchronized (entry.receiveLock)
        {
            entry.receiveChunks.addLast(data);
            entry.receiveAvailable += data.length;
            available = entry.receiveAvailable;
            announce = !entry.dataPending;
            entry.dataPending = true;
        }

        if (announce)
            invoke(
                callbackClassicData,
                (double) entry.handle,
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
                eraseLeService(serviceHandle);
        }
    }


    // Caller holds leEntityLock. Erases a service with its characteristics
    // and their descriptors.
    private void eraseLeService(long handle)
    {
        LeServiceEntry service = leServices.remove(handle);
        if (service == null)
            return;

        if (service.gattService != null)
            leServiceHandleByObject.remove(service.gattService);

        for (long characteristicHandle : service.characteristicHandles)
            eraseLeCharacteristic(characteristicHandle);
    }


    // Caller holds leEntityLock.
    private void eraseLeCharacteristic(long handle)
    {
        LeCharacteristicEntry characteristic = leCharacteristics.remove(handle);
        if (characteristic == null)
            return;

        if (characteristic.gattCharacteristic != null)
            leCharacteristicHandleByObject.remove(characteristic.gattCharacteristic);

        for (long descriptorHandle : characteristic.descriptorHandles)
            eraseLeDescriptor(descriptorHandle);
    }


    // Caller holds leEntityLock.
    private void eraseLeDescriptor(long handle)
    {
        LeDescriptorEntry descriptor = leDescriptors.remove(handle);

        if (descriptor != null && descriptor.gattDescriptor != null)
            leDescriptorHandleByObject.remove(descriptor.gattDescriptor);
    }


    // "<uuid>#<n>": the n-th attribute with that UUID among its siblings,
    // which is what identifies it across a re-discovery.
    private static String occurrenceKey(UUID uuid, HashMap<String, Integer> seen)
    {
        String text = uuid != null ? uuid.toString() : "";
        Integer count = seen.get(text);
        int index = count != null ? count : 0;
        seen.put(text, index + 1);
        return text + "#" + index;
    }


    // A re-discovery hands back new objects for the same attributes. Each
    // service is matched to the handle it had by its UUID path and
    // occurrence, so the handles the game holds - and a characteristic's
    // subscription - survive; the entry is rebound to the new object, and
    // what vanished is erased.
    private void rebuildLeServices(
        LeConnectionEntry connection,
        java.util.List<BluetoothGattService> discovered)
    {
        synchronized (connection.serviceListLock)
        {
            synchronized (leEntityLock)
            {
                HashMap<String, Long> previous = new HashMap<>();
                HashMap<String, Integer> seen = new HashMap<>();

                for (long handle : connection.serviceHandles)
                {
                    LeServiceEntry service = leServices.get(handle);
                    if (service != null && service.gattService != null)
                        previous.put(occurrenceKey(service.gattService.getUuid(), seen), handle);
                }

                ArrayList<Long> rebuilt = new ArrayList<>();
                seen.clear();

                for (BluetoothGattService gattService : discovered)
                {
                    Long handle = previous.remove(occurrenceKey(gattService.getUuid(), seen));
                    LeServiceEntry service = handle != null ? leServices.get(handle) : null;

                    if (service == null)
                    {
                        rebuilt.add(findOrCreateServiceHandle(connection.handle, gattService));
                        continue;
                    }

                    leServiceHandleByObject.remove(service.gattService);
                    service.gattService = gattService;
                    leServiceHandleByObject.put(gattService, service.handle);

                    if (service.characteristicsDiscovered)
                        rebuildLeCharacteristics(service, gattService.getCharacteristics());

                    rebuilt.add(service.handle);
                }

                for (long vanished : previous.values())
                    eraseLeService(vanished);

                connection.serviceHandles.clear();
                connection.serviceHandles.addAll(rebuilt);
            }
        }
    }


    // Caller holds leEntityLock. rebuildLeServices for a service's
    // characteristics; the subscribe mode stays on the reused entry.
    private void rebuildLeCharacteristics(
        LeServiceEntry service,
        java.util.List<BluetoothGattCharacteristic> discovered)
    {
        HashMap<String, Long> previous = new HashMap<>();
        HashMap<String, Integer> seen = new HashMap<>();

        for (long handle : service.characteristicHandles)
        {
            LeCharacteristicEntry characteristic = leCharacteristics.get(handle);
            if (characteristic != null && characteristic.gattCharacteristic != null)
                previous.put(occurrenceKey(characteristic.gattCharacteristic.getUuid(), seen), handle);
        }

        ArrayList<Long> rebuilt = new ArrayList<>();
        seen.clear();

        for (BluetoothGattCharacteristic gattCharacteristic : discovered)
        {
            Long handle = previous.remove(occurrenceKey(gattCharacteristic.getUuid(), seen));
            LeCharacteristicEntry characteristic = handle != null ? leCharacteristics.get(handle) : null;

            if (characteristic == null)
            {
                rebuilt.add(findOrCreateCharacteristicHandle(
                    service.handle,
                    service.connection,
                    gattCharacteristic));
                continue;
            }

            leCharacteristicHandleByObject.remove(characteristic.gattCharacteristic);
            characteristic.gattCharacteristic = gattCharacteristic;
            leCharacteristicHandleByObject.put(gattCharacteristic, characteristic.handle);

            if (characteristic.descriptorsDiscovered)
                rebuildLeDescriptors(characteristic, gattCharacteristic.getDescriptors());

            rebuilt.add(characteristic.handle);
        }

        for (long vanished : previous.values())
            eraseLeCharacteristic(vanished);

        service.characteristicHandles.clear();
        service.characteristicHandles.addAll(rebuilt);
    }


    // Caller holds leEntityLock.
    private void rebuildLeDescriptors(
        LeCharacteristicEntry characteristic,
        java.util.List<BluetoothGattDescriptor> discovered)
    {
        HashMap<String, Long> previous = new HashMap<>();
        HashMap<String, Integer> seen = new HashMap<>();

        for (long handle : characteristic.descriptorHandles)
        {
            LeDescriptorEntry descriptor = leDescriptors.get(handle);
            if (descriptor != null && descriptor.gattDescriptor != null)
                previous.put(occurrenceKey(descriptor.gattDescriptor.getUuid(), seen), handle);
        }

        ArrayList<Long> rebuilt = new ArrayList<>();
        seen.clear();

        for (BluetoothGattDescriptor gattDescriptor : discovered)
        {
            Long handle = previous.remove(occurrenceKey(gattDescriptor.getUuid(), seen));
            LeDescriptorEntry descriptor = handle != null ? leDescriptors.get(handle) : null;

            if (descriptor == null)
            {
                rebuilt.add(findOrCreateDescriptorHandle(characteristic.handle, gattDescriptor));
                continue;
            }

            leDescriptorHandleByObject.remove(descriptor.gattDescriptor);
            descriptor.gattDescriptor = gattDescriptor;
            leDescriptorHandleByObject.put(gattDescriptor, descriptor.handle);
            rebuilt.add(descriptor.handle);
        }

        for (long vanished : previous.values())
            eraseLeDescriptor(vanished);

        characteristic.descriptorHandles.clear();
        characteristic.descriptorHandles.addAll(rebuilt);
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


    // An MTU request or RSSI read carries its number after the connection, 0
    // when it failed.
    private static boolean isNumberOp(LePendingOp op)
    {
        return op.kind == LePendingOp.KIND_REQUEST_MTU ||
            op.kind == LePendingOp.KIND_READ_RSSI;
    }


    private void failOp(LePendingOp op, BluetoothError error, String message)
    {
        if (isReadOp(op))
            invoke(op.callback, error, message, (double) op.targetHandle, 0.0, 0);
        else if (isNumberOp(op))
            invoke(op.callback, error, message, (double) op.targetHandle, 0.0);
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
                op.timeout = gattOpTimeout(connection, op);
            }

            // Posted before the start, so a completion that beats this
            // thread finds it to remove.
            timeoutHandler.postDelayed(op.timeout, GATT_OP_TIMEOUT_MS);

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
        LePendingOp op;

        synchronized (connection.opLock)
        {
            op = connection.currentOp;
            connection.currentOp = null;
        }

        if (op != null && op.timeout != null)
            timeoutHandler.removeCallbacks(op.timeout);

        return op;
    }


    // For a callback the stack also fires on its own - onMtuChanged when the
    // peer or the stack renegotiates - takes the current op only when it is
    // the one that asked, so an unsolicited event leaves a read in flight alone.
    private LePendingOp takeCurrentOp(LeConnectionEntry connection, int kind)
    {
        LePendingOp op;

        synchronized (connection.opLock)
        {
            op = connection.currentOp;

            if (op == null || op.kind != kind)
                return null;

            connection.currentOp = null;
        }

        if (op.timeout != null)
            timeoutHandler.removeCallbacks(op.timeout);

        return op;
    }


    // An op still current when its time is up has stalled the link: it fails
    // Timeout and the link is dropped, which fails the ops queued behind it
    // with Disconnected through onConnectionStateChange.
    private Runnable gattOpTimeout(final LeConnectionEntry connection, final LePendingOp op)
    {
        final long workerGeneration = generation.get();

        return () ->
        {
            if (generation.get() != workerGeneration)
                return;

            synchronized (connection.opLock)
            {
                if (connection.currentOp != op)
                    return;

                connection.currentOp = null;
            }

            failOp(op, TIMEOUT, "GATT operation timed out");

            BluetoothGatt gatt = connection.gatt;

            if (gatt != null)
            {
                try
                {
                    gatt.disconnect();
                }
                catch (Throwable ignored)
                {
                }
            }
        };
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
        {
            if (op.timeout != null)
                timeoutHandler.removeCallbacks(op.timeout);

            failOp(op, error, message);
        }
    }


    // =========================================================================
    // GATT status mapping
    // =========================================================================

    // Statuses Android adds above the ATT codes.
    private static final int GATT_STATUS_ERROR = 0x85;
    private static final int GATT_STATUS_CONNECTION_CONGESTED = 0x8F;
    private static final int GATT_STATUS_CONNECTION_TIMEOUT = 0x93;
    private static final int GATT_STATUS_FAILURE = 0x101;

    // The link-layer reason onConnectionStateChange reports for a
    // supervision timeout.
    private static final int HCI_CONNECTION_TIMEOUT = 0x08;

    // A GATT status as the enum, as the native core maps an ATT error: the
    // permission and security codes get their own members, the rest
    // OperationFailed, and the message carries the code.
    private static BluetoothError mapAttError(int status)
    {
        switch (status)
        {
            case 0x00:
                return OK;
            case 0x02:
            case 0x03:
            case 0x06:
                return NOT_PERMITTED;
            case 0x05:
            case 0x08:
            case 0x0C:
            case 0x0F:
                return INSUFFICIENT_SECURITY;
            case GATT_STATUS_CONNECTION_CONGESTED:
                return BUSY;
            default:
                return OPERATION_FAILED;
        }
    }


    // "GATT status 0x05: insufficient authentication".
    private static String attErrorMessage(int status)
    {
        String name;

        switch (status)
        {
            case 0x01: name = "invalid handle"; break;
            case 0x02: name = "read not permitted"; break;
            case 0x03: name = "write not permitted"; break;
            case 0x04: name = "invalid PDU"; break;
            case 0x05: name = "insufficient authentication"; break;
            case 0x06: name = "request not supported"; break;
            case 0x07: name = "invalid offset"; break;
            case 0x08: name = "insufficient authorization"; break;
            case 0x09: name = "prepare queue full"; break;
            case 0x0A: name = "attribute not found"; break;
            case 0x0B: name = "attribute not long"; break;
            case 0x0C: name = "insufficient encryption key size"; break;
            case 0x0D: name = "invalid attribute value length"; break;
            case 0x0E: name = "unlikely error"; break;
            case 0x0F: name = "insufficient encryption"; break;
            case 0x10: name = "unsupported group type"; break;
            case 0x11: name = "insufficient resources"; break;
            case GATT_STATUS_ERROR: name = "GATT error"; break;
            case GATT_STATUS_CONNECTION_CONGESTED: name = "connection congested"; break;
            case GATT_STATUS_FAILURE: name = "GATT failure"; break;
            default:
                name = status >= 0x80 && status <= 0x9F ? "application error" : "unknown error";
                break;
        }

        return String.format("GATT status 0x%02X: %s", status, name);
    }


    // onConnectionStateChange reports a link-layer (HCI) reason or one of
    // Android's GATT statuses, not an ATT error, so the common link reasons
    // are named here before falling back to the GATT table.
    private static String connectionStatusMessage(int status)
    {
        String name;

        switch (status)
        {
            case HCI_CONNECTION_TIMEOUT: name = "connection timeout"; break;
            case 0x13: name = "remote device terminated the connection"; break;
            case 0x16: name = "connection terminated by the local host"; break;
            case 0x22: name = "link layer response timeout"; break;
            case 0x3E: name = "connection failed to be established"; break;
            case GATT_STATUS_CONNECTION_TIMEOUT: name = "connection timeout"; break;
            default:
                return attErrorMessage(status);
        }

        return String.format("GATT status 0x%02X: %s", status, name);
    }


    // A failed connect: a timeout as Timeout, anything else ConnectionFailed.
    private static BluetoothError connectError(int status)
    {
        return status == HCI_CONNECTION_TIMEOUT || status == GATT_STATUS_CONNECTION_TIMEOUT
            ? TIMEOUT
            : CONNECTION_FAILED;
    }


    // =========================================================================
    // BLE GATT dispatch helpers
    // =========================================================================

    private void dispatchLeDisconnected(long connection, BluetoothError error, String message)
    {
        String safeMessage = message != null ? message : "";

        invoke(callbackLeDisconnected, error, safeMessage, (double) connection);
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

    // A uint8[] field, generated as a List<Byte>.
    private static byte[] objectBytes(Object object, String name) throws Exception
    {
        Object[] elements = objectArray(object, name);
        byte[] bytes = new byte[elements.length];

        for (int i = 0; i < elements.length; i++)
        {
            if (!(elements[i] instanceof Number))
                throw new IllegalArgumentException(name + "[" + i + "] is not a byte");
            bytes[i] = ((Number) elements[i]).byteValue();
        }

        return bytes;
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

            // No adapter is a device without Bluetooth: the extension still
            // runs, reports Unsupported and answers NotSupported.
            adapter = manager != null ? manager.getAdapter() : null;

            generation.incrementAndGet();
            initialized = true;

            if (adapter != null)
                ensureStateReceiver();

            // A callback registered before this heard Unknown.
            dispatchState();
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

        // The connect windows and op timeouts of this session; what they
        // guard is failed below.
        timeoutHandler.removeCallbacksAndMessages(null);

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

        // The generation bump above silences every worker and system
        // callback, so each GML callback still waiting is failed here, once.
        HashMap<String, Long> pairHandles;
        HashMap<String, GMFunction> pendingPairs;

        synchronized (pairLock)
        {
            pairHandles = new HashMap<>(pairDeviceHandles);
            pendingPairs = new HashMap<>(pairCallbacks);
            pairDeviceHandles.clear();
            pairCallbacks.clear();
        }

        for (Map.Entry<String, Long> pair : pairHandles.entrySet())
            invoke(
                pendingPairs.get(pair.getKey()),
                NOT_INITIALIZED,
                SHUTDOWN_MESSAGE,
                (double) pair.getValue());

        // A dialog still up answers nobody: its result arrives after shutdown.
        firePermissionCallbacks(NOT_INITIALIZED, SHUTDOWN_MESSAGE, PERMISSION_UNKNOWN);
        fireEnableCallbacks(NOT_INITIALIZED, SHUTDOWN_MESSAGE);

        stopServerInternal();
        stopLeAdvertiseInternal(NOT_INITIALIZED, SHUTDOWN_MESSAGE);
        stopLeServerInternal(NOT_INITIALIZED, SHUTDOWN_MESSAGE, false);

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

            if (entry.connectPending.getAndSet(false))
            {
                GMFunction connectCallback = entry.connectCallback;
                entry.connectCallback = null;

                invoke(
                    connectCallback,
                    NOT_INITIALIZED,
                    SHUTDOWN_MESSAGE,
                    (double) entry.handle,
                    (double) entry.device);
            }

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

            if (entry.connectPending.getAndSet(false))
            {
                GMFunction connectCallback = entry.connectCallback;
                entry.connectCallback = null;

                invoke(
                    connectCallback,
                    NOT_INITIALIZED,
                    SHUTDOWN_MESSAGE,
                    (double) entry.handle,
                    (double) entry.device);
            }

            closeQuietly(entry.socket);
            wakeReader(entry);
            wakeWriter(entry);
        }

        synchronized (deviceLock)
        {
            deviceById.clear();
            devices.clear();
            deviceOrder.clear();
        }

        // The registered callbacks and the last error stay: the callbacks
        // serve the next bluetooth_initialize, and the error is still the
        // answer to the game's last failed call.
        adapter = null;
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
        return lastError.code;
    }


    @Override
    public String bluetooth_last_error_message()
    {
        return lastError.message;
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


    // What this file really does with each feature on this device; null is
    // a value the generated decoder did not know.
    @Override
    public boolean bluetooth_feature_is_supported(BluetoothFeature feature)
    {
        if (feature == null)
            return false;

        switch (feature)
        {
            case LeCentral:
                return bluetooth_le_is_supported();

            // The scanner has no passive mode: active is ignored.
            case LePassiveScan:
                return false;

            // AdvertiseData and AdvertiseSettings carry every part.
            case LeAdvertise:
            case LeAdvertiseName:
            case LeAdvertiseServiceUuids:
            case LeAdvertiseServiceData:
            case LeAdvertiseManufacturerData:
            case LeAdvertiseTxPower:
            case LeAdvertiseIncludeTxPower:
            case LeAdvertiseNonConnectable:
                return bluetooth_le_advertise_is_supported();

            // Descriptor requests reach GML (the CCCD excepted), the signed
            // write permissions pass to the stack, and onConnectionStateChange
            // reports each central.
            case LeServer:
            case LeServerDescriptorRequests:
            case LeServerSignedWrite:
            case LeServerConnectionEvents:
                return bluetooth_le_server_is_supported();

            // createBond() takes a device of either transport.
            case LePairing:
                return bluetooth_le_is_supported();
            case ClassicPairing:
                return initialized && adapter != null;

            case Classic:
            case ClassicDiscoverable:
                return bluetooth_classic_is_supported();
            case ClassicServer:
                return bluetooth_classic_server_is_supported();

            // ACTION_REQUEST_DISCOVERABLE has no counterpart that ends it.
            case ClassicDiscoverableStop:
                return false;

            // Runtime permissions, and so a prompt, start with Android 6.
            case PermissionRequest:
                return Build.VERSION.SDK_INT >= Build.VERSION_CODES.M;

            // requestMtu, readRemoteRssi and requestConnectionPriority come
            // with every LE client.
            case LeMtuRequest:
            case LeReadRssi:
            case LeConnectionPriority:
                return bluetooth_le_is_supported();

            // ACTION_REQUEST_ENABLE and getBondedDevices need only an adapter.
            case RequestEnable:
            case PairedDevicesQuery:
                return initialized && adapter != null;

            default:
                return false;
        }
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

        firePermissionCallbacks(OK, "", bluetooth_permission_get_status());
        dispatchState();
    }


    // Every permission_request callback waiting for the dialog's answer; one
    // answer fires them all, so a request while the dialog is up just waits.
    private final Object permissionLock = new Object();
    private final ArrayList<GMFunction> pendingPermissionCallbacks = new ArrayList<>();


    private void firePermissionCallbacks(
        BluetoothError error,
        String message,
        BluetoothPermissionStatus status)
    {
        ArrayList<GMFunction> callbacks;

        synchronized (permissionLock)
        {
            callbacks = new ArrayList<>(pendingPermissionCallbacks);
            pendingPermissionCallbacks.clear();
        }

        for (GMFunction callback : callbacks)
            invoke(callback, error, message, status.value());
    }


    @Override
    public BluetoothError bluetooth_permission_request(GMFunction callback)
    {
        if (!initialized)
            return result(
                NOT_INITIALIZED,
                "Bluetooth is not initialized");

        if (bluetooth_permission_get_status() == PERMISSION_GRANTED)
        {
            // Already decided: answered at once, with anything still waiting.
            synchronized (permissionLock)
            {
                pendingPermissionCallbacks.add(callback);
            }

            firePermissionCallbacks(OK, "", PERMISSION_GRANTED);
            return OK;
        }

        Activity current = activity();

        if (current == null)
            return result(
                NOT_INITIALIZED,
                "Current Android Activity is unavailable");

        boolean launch;

        synchronized (permissionLock)
        {
            launch = pendingPermissionCallbacks.isEmpty();
            pendingPermissionCallbacks.add(callback);
        }

        if (!launch)
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
            else
            {
                // Granted at install time: no dialog will answer.
                firePermissionCallbacks(OK, "", bluetooth_permission_get_status());
            }

            return OK;
        }
        catch (Throwable throwable)
        {
            // A pre-flight failure fires nothing.
            synchronized (permissionLock)
            {
                pendingPermissionCallbacks.remove(callback);
            }

            return result(
                OPERATION_FAILED,
                throwableMessage(throwable));
        }
    }


    // =========================================================================
    // Request enable
    // =========================================================================

    // Every request_enable callback waiting for the dialog's answer; one
    // answer fires them all, as for the permission dialog.
    private final Object enableLock = new Object();
    private final ArrayList<GMFunction> pendingEnableCallbacks = new ArrayList<>();


    private void fireEnableCallbacks(BluetoothError error, String message)
    {
        ArrayList<GMFunction> callbacks;

        synchronized (enableLock)
        {
            callbacks = new ArrayList<>(pendingEnableCallbacks);
            pendingEnableCallbacks.clear();
        }

        for (GMFunction callback : callbacks)
            invoke(callback, error, message);
    }


    // The runner forwards every activity result to each extension; only the
    // enable dialog's is ours.
    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data)
    {
        if (requestCode != REQUEST_CODE_ENABLE || !initialized)
            return;

        if (resultCode == Activity.RESULT_OK)
            fireEnableCallbacks(OK, "");
        else
            fireEnableCallbacks(BLUETOOTH_DISABLED, "the user declined");
    }


    @Override
    public BluetoothError bluetooth_request_enable(GMFunction callback)
    {
        if (!initialized)
            return result(NOT_INITIALIZED, NOT_INITIALIZED_MESSAGE);

        if (!bluetooth_feature_is_supported(BluetoothFeature.RequestEnable))
            return result(NOT_SUPPORTED, NO_ADAPTER_MESSAGE);

        // Already on: the answer is known, so it fires at once. On but not
        // usable (Android 12+ without the permissions) falls through to the
        // permission check, as the native core orders it.
        if (currentBluetoothState() == STATE_POWERED_ON)
        {
            invoke(callback, OK, "");
            return OK;
        }

        // ACTION_REQUEST_ENABLE needs BLUETOOTH_CONNECT from Android 12.
        if (!hasConnectPermission())
            return result(
                PERMISSION_DENIED,
                "Bluetooth connect permission is not granted");

        Activity current = activity();

        if (current == null)
            return result(
                OPERATION_FAILED,
                "No foreground activity available to request enabling Bluetooth");

        boolean launch;

        synchronized (enableLock)
        {
            launch = pendingEnableCallbacks.isEmpty();
            pendingEnableCallbacks.add(callback);
        }

        // A dialog is already up: this request waits for its answer.
        if (!launch)
            return OK;

        try
        {
            current.startActivityForResult(
                new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE),
                REQUEST_CODE_ENABLE);
            return OK;
        }
        catch (Throwable throwable)
        {
            // A pre-flight failure fires nothing.
            synchronized (enableLock)
            {
                pendingEnableCallbacks.remove(callback);
            }

            if (throwable instanceof SecurityException)
                return result(PERMISSION_DENIED, throwableMessage(throwable));

            return result(OPERATION_FAILED, throwableMessage(throwable));
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
                device,
                true,
                scanResult.getScanRecord());
        }


        @Override
        public void onScanFailed(int errorCode)
        {
            if (!initialized || !leScanning.getAndSet(false))
                return;

            dispatchScanStopped(
                TRANSPORT_LE,
                OPERATION_FAILED,
                "Android BLE scan failed: " + errorCode);
        }
    };


    private static boolean present(Optional<?> value)
    {
        return value != null && value.isPresent();
    }


    // One ScanFilter per GML filter; the stack ORs them and matches each on
    // the combined advertisement and scan response, which is the spec's rule,
    // and keeps a filtered scan running with the screen off. Assumes the
    // filters passed checkScanFilters.
    private static List<ScanFilter> buildScanFilters(List<BluetoothLeScanFilter> filters)
    {
        ArrayList<ScanFilter> out = new ArrayList<>();

        if (filters == null)
            return out;

        for (BluetoothLeScanFilter filter : filters)
        {
            ScanFilter.Builder builder = new ScanFilter.Builder();

            if (present(filter.service_uuid()))
                builder.setServiceUuid(new ParcelUuid(parseUuid(filter.service_uuid().get())));

            if (present(filter.name()))
                builder.setDeviceName(filter.name().get());

            // An empty data prefix matches any payload for the company.
            if (present(filter.company_id()))
                builder.setManufacturerData(filter.company_id().get(), new byte[0]);

            out.add(builder.build());
        }

        return out;
    }


    // The core's filter pre-flight, in its order and with its messages: a
    // filter must set a field, a UUID must be well formed, a company id must
    // fit 16 bits. Sets the last error when it fails.
    private BluetoothError checkScanFilters(List<BluetoothLeScanFilter> filters)
    {
        if (filters == null)
            return OK;

        for (BluetoothLeScanFilter filter : filters)
        {
            if (filter == null ||
                (!present(filter.service_uuid()) && !present(filter.name()) && !present(filter.company_id())))
                return result(
                    INVALID_ARGUMENT,
                    "A scan filter must set service_uuid, name or company_id");

            if (present(filter.service_uuid()) && !isValidUuid(filter.service_uuid().get()))
                return result(INVALID_ARGUMENT, "Invalid UUID: " + filter.service_uuid().get());

            if (present(filter.company_id()))
            {
                int companyId = filter.company_id().get();

                if (companyId < 0 || companyId > 0xFFFF)
                    return result(
                        INVALID_ARGUMENT,
                        "A scan filter's company_id must be 0-65535");
            }
        }

        return OK;
    }


    @Override
    public BluetoothError bluetooth_le_scan_start(boolean active, List<BluetoothLeScanFilter> filters)
    {
        // Android's scanner does not expose a direct active/passive flag in the
        // same sense as Windows. Keep the API argument for cross-platform parity.
        if (!initialized)
            return result(NOT_INITIALIZED, NOT_INITIALIZED_MESSAGE);

        // Bad filters fail before the radio is looked at, as in the core.
        BluetoothError checked = checkScanFilters(filters);
        if (checked != OK)
            return checked;

        BluetoothError ready = requireAdapter();
        if (ready != OK)
            return ready;

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

        // A start while scanning starts nothing new and keeps that scan's
        // filters: stop first to change them.
        if (leScanning.get())
            return OK;

        try
        {
            leScanner = adapter.getBluetoothLeScanner();

            if (leScanner == null)
                return result(
                    NOT_SUPPORTED,
                    "Bluetooth LE scanner is unavailable");

            // The default mode is low power, which samples a fraction of
            // the time; a game scanning wants to find the device now.
            ScanSettings settings = new ScanSettings.Builder()
                .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
                .build();

            // No filters is null: every device is reported.
            List<ScanFilter> scanFilters = buildScanFilters(filters);

            leScanner.startScan(
                scanFilters.isEmpty() ? null : scanFilters,
                settings,
                leScanCallback);
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

                // bluetooth_le_disconnect retires the handle and closes the
                // link itself, so a closed link reports nothing more.
                LeConnectionEntry entry = getLeConnection(connection);
                if (entry == null || entry.manualClosing)
                    return;

                if (newState == BluetoothProfile.STATE_CONNECTED)
                {
                    // The connect window or a cancel reported it already.
                    if (!entry.connectPending.getAndSet(false))
                        return;

                    removeConnectTimeout(entry);

                    boolean success = status == BluetoothGatt.GATT_SUCCESS;

                    GMFunction connectCallback = entry.connectCallback;
                    entry.connectCallback = null;

                    if (success)
                    {
                        entry.connected = true;
                    }
                    else
                    {
                        eraseLeConnection(connection);

                        try
                        {
                            gatt.close();
                        }
                        catch (Throwable ignored)
                        {
                        }
                    }

                    invoke(
                        connectCallback,
                        success ? OK : connectError(status),
                        success ? "" : connectionStatusMessage(status),
                        (double) connection,
                        (double) entry.device);
                }
                else if (newState == BluetoothProfile.STATE_DISCONNECTED)
                {
                    boolean wasConnected = entry.connected;
                    boolean pending = entry.connectPending.getAndSet(false);
                    entry.connected = false;

                    if (pending)
                        removeConnectTimeout(entry);

                    // Fails the ops still waiting, before le_disconnected, as
                    // the native core orders it.
                    eraseLeConnection(connection);

                    try
                    {
                        gatt.close();
                    }
                    catch (Throwable ignored)
                    {
                    }

                    if (pending)
                    {
                        // Disconnected before STATE_CONNECTED ever fired -
                        // report the connect attempt itself as failed.
                        GMFunction connectCallback = entry.connectCallback;
                        entry.connectCallback = null;

                        invoke(
                            connectCallback,
                            connectError(status),
                            connectionStatusMessage(status),
                            (double) connection,
                            (double) entry.device);
                    }
                    else if (wasConnected)
                    {
                        dispatchLeDisconnected(
                            connection,
                            DISCONNECTED,
                            connectionStatusMessage(status));
                    }
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
                    rebuildLeServices(entry, gatt.getServices());
                    invoke(callback, OK, "", (double) connection);
                }
                else
                {
                    invoke(
                        callback,
                        mapAttError(status),
                        attErrorMessage(status),
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
                        failOp(op, mapAttError(status), attErrorMessage(status));
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
                            mapAttError(status),
                            attErrorMessage(status),
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
                        failOp(op, mapAttError(status), attErrorMessage(status));
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
                        // The peripheral kept its old configuration, so local
                        // delivery goes back to match it.
                        if (characteristicEntry != null && characteristicEntry.gattCharacteristic != null)
                        {
                            try
                            {
                                gatt.setCharacteristicNotification(
                                    characteristicEntry.gattCharacteristic,
                                    characteristicEntry.subscribeMode != SUBSCRIBE_MODE_UNSUBSCRIBE);
                            }
                            catch (Throwable ignored)
                            {
                            }
                        }

                        invoke(
                            op.callback,
                            mapAttError(status),
                            attErrorMessage(status),
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
                        mapAttError(status),
                        attErrorMessage(status),
                        (double) op.targetHandle);
                }

                completeGattOp(entry);
            }


            // Also fires unasked when the peer or the stack negotiates the MTU
            // itself: that only updates the stored value.
            @Override
            public void onMtuChanged(BluetoothGatt gatt, int mtu, int status)
            {
                if (generation.get() != workerGeneration)
                    return;

                LeConnectionEntry entry = getLeConnection(connection);
                if (entry == null)
                    return;

                if (status == BluetoothGatt.GATT_SUCCESS)
                    entry.mtu = mtu;

                LePendingOp op = takeCurrentOp(entry, LePendingOp.KIND_REQUEST_MTU);

                if (op == null)
                    return;

                if (status == BluetoothGatt.GATT_SUCCESS)
                    invoke(op.callback, OK, "", (double) connection, (double) mtu);
                else
                    failOp(op, mapAttError(status), attErrorMessage(status));

                completeGattOp(entry);
            }


            @Override
            public void onReadRemoteRssi(BluetoothGatt gatt, int rssi, int status)
            {
                if (generation.get() != workerGeneration)
                    return;

                LeConnectionEntry entry = getLeConnection(connection);
                if (entry == null)
                    return;

                LePendingOp op = takeCurrentOp(entry, LePendingOp.KIND_READ_RSSI);

                if (op == null)
                    return;

                if (status == BluetoothGatt.GATT_SUCCESS)
                    invoke(op.callback, OK, "", (double) connection, (double) rssi);
                else
                    failOp(op, mapAttError(status), attErrorMessage(status));

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
        if (requireAdapter() != OK)
            return 0;

        DeviceEntry deviceEntry = copyDevice(device);

        if (deviceEntry == null)
        {
            setLastError(INVALID_HANDLE, "Invalid device handle");
            return 0;
        }

        if (deviceEntry.transport != TRANSPORT_LE)
        {
            setLastError(INVALID_ARGUMENT, "Expected a BLE device");
            return 0;
        }

        if (hasClientConnection(device))
        {
            setLastError(BUSY, "The device already has a connection");
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

        final LeConnectionEntry entry = getLeConnection(connection);
        entry.connectCallback = callback;
        entry.connectPending.set(true);

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

        // connectGatt with autoConnect off gives up on its own only after
        // about 30 seconds, and some stacks never do; the window is 15 on
        // every platform.
        entry.connectTimeout = () ->
        {
            if (generation.get() != workerGeneration ||
                getLeConnection(connection) != entry ||
                !entry.connectPending.getAndSet(false))
                return;

            entry.manualClosing = true;
            eraseLeConnection(connection);
            closeGatt(entry.gatt);

            GMFunction connectCallback = entry.connectCallback;
            entry.connectCallback = null;

            invoke(
                connectCallback,
                TIMEOUT,
                "Connection timed out",
                (double) connection,
                (double) entry.device);
        };

        timeoutHandler.postDelayed(entry.connectTimeout, LE_CONNECT_TIMEOUT_MS);

        return connection;
    }


    private void removeConnectTimeout(LeConnectionEntry entry)
    {
        Runnable timeout = entry.connectTimeout;

        if (timeout != null)
            timeoutHandler.removeCallbacks(timeout);
    }


    // Synchronous, as on every platform: the handle is retired by the call
    // and the link closed at once. A connect still in progress fires its
    // callback ConnectionFailed; an established link fires no le_disconnected.
    @Override
    public BluetoothError bluetooth_le_disconnect(long connection)
    {
        if (!initialized)
            return result(NOT_INITIALIZED, NOT_INITIALIZED_MESSAGE);

        LeConnectionEntry entry = getLeConnection(connection);

        if (entry == null)
            return result(INVALID_HANDLE, "Invalid BLE connection handle");

        if (entry.serverRole)
            return result(
                INVALID_HANDLE,
                "A server connection ends when the central disconnects; " +
                    "bluetooth_le_disconnect takes client connections");

        boolean pending = entry.connectPending.getAndSet(false);
        entry.manualClosing = true;

        if (pending)
            removeConnectTimeout(entry);

        // The game is done with this link: no op waiting on it outlives the call.
        eraseLeConnection(connection);
        closeGatt(entry.gatt);

        if (pending)
        {
            GMFunction connectCallback = entry.connectCallback;
            entry.connectCallback = null;

            invoke(
                connectCallback,
                CONNECTION_FAILED,
                "Connection cancelled by bluetooth_le_disconnect",
                (double) connection,
                (double) entry.device);
        }

        return OK;
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
    // BLE GATT client - link parameters (MTU, RSSI, priority)
    // =========================================================================

    private static final String INVALID_LE_CLIENT_MESSAGE = "Invalid LE connection handle";
    private static final String LE_NOT_CONNECTED_MESSAGE = "The LE connection is not connected";

    // The core's check_le_client_connection: a server connection is not a
    // client handle, so it is InvalidHandle too. null when it is not one.
    private LeConnectionEntry leClientConnection(long connection)
    {
        LeConnectionEntry entry = getLeConnection(connection);
        return entry != null && !entry.serverRole ? entry : null;
    }


    private static boolean leConnected(LeConnectionEntry entry)
    {
        return entry.connected && entry.gatt != null;
    }


    @Override
    public int bluetooth_le_connection_get_mtu(long connection)
    {
        if (!initialized)
            return 0;

        LeConnectionEntry entry = leClientConnection(connection);
        return entry != null && leConnected(entry) ? entry.mtu : 0;
    }


    @Override
    public BluetoothError bluetooth_le_connection_request_mtu(long connection, int mtu, GMFunction callback)
    {
        if (!initialized)
            return result(NOT_INITIALIZED, NOT_INITIALIZED_MESSAGE);

        LeConnectionEntry connEntry = leClientConnection(connection);

        if (connEntry == null)
            return result(INVALID_HANDLE, INVALID_LE_CLIENT_MESSAGE);

        if (mtu < 23 || mtu > 517)
            return result(INVALID_ARGUMENT, "mtu must be 23-517");

        if (!leConnected(connEntry))
            return result(DISCONNECTED, LE_NOT_CONNECTED_MESSAGE);

        final BluetoothGatt gatt = connEntry.gatt;

        // Queued like any GATT op: the stack takes no request while another is
        // in flight, and onMtuChanged completes it.
        LePendingOp op = new LePendingOp();
        op.kind = LePendingOp.KIND_REQUEST_MTU;
        op.callback = callback;
        op.targetHandle = connection;
        op.startFailureMessage = "requestMtu() failed to start";
        op.start = () -> gatt.requestMtu(mtu);

        enqueueGattOp(connEntry, op);

        return OK;
    }


    @Override
    public BluetoothError bluetooth_le_connection_read_rssi(long connection, GMFunction callback)
    {
        if (!initialized)
            return result(NOT_INITIALIZED, NOT_INITIALIZED_MESSAGE);

        LeConnectionEntry connEntry = leClientConnection(connection);

        if (connEntry == null)
            return result(INVALID_HANDLE, INVALID_LE_CLIENT_MESSAGE);

        if (!bluetooth_feature_is_supported(BluetoothFeature.LeReadRssi))
            return result(
                NOT_SUPPORTED,
                "Reading the RSSI of a connection is not supported on this platform");

        if (!leConnected(connEntry))
            return result(DISCONNECTED, LE_NOT_CONNECTED_MESSAGE);

        final BluetoothGatt gatt = connEntry.gatt;

        LePendingOp op = new LePendingOp();
        op.kind = LePendingOp.KIND_READ_RSSI;
        op.callback = callback;
        op.targetHandle = connection;
        op.startFailureMessage = "readRemoteRssi() failed to start";
        op.start = () -> gatt.readRemoteRssi();

        enqueueGattOp(connEntry, op);

        return OK;
    }


    // Synchronous: Android reports no result for the request, so Ok means
    // only that the stack took it.
    @Override
    public BluetoothError bluetooth_le_connection_request_priority(
        long connection,
        BluetoothLeConnectionPriority priority)
    {
        if (!initialized)
            return result(NOT_INITIALIZED, NOT_INITIALIZED_MESSAGE);

        LeConnectionEntry connEntry = leClientConnection(connection);

        if (connEntry == null)
            return result(INVALID_HANDLE, INVALID_LE_CLIENT_MESSAGE);

        // null is a value the generated decoder did not know.
        if (priority == null)
            return result(
                INVALID_ARGUMENT,
                "priority must be a BluetoothLeConnectionPriority");

        if (!bluetooth_feature_is_supported(BluetoothFeature.LeConnectionPriority))
            return result(
                NOT_SUPPORTED,
                "Connection priority is not supported on this platform");

        if (!leConnected(connEntry))
            return result(DISCONNECTED, LE_NOT_CONNECTED_MESSAGE);

        int value;

        switch (priority)
        {
            case High:
                value = BluetoothGatt.CONNECTION_PRIORITY_HIGH;
                break;
            case LowPower:
                value = BluetoothGatt.CONNECTION_PRIORITY_LOW_POWER;
                break;
            default:
                value = BluetoothGatt.CONNECTION_PRIORITY_BALANCED;
                break;
        }

        try
        {
            if (!connEntry.gatt.requestConnectionPriority(value))
                return result(OPERATION_FAILED, "requestConnectionPriority() was refused");
        }
        catch (SecurityException exception)
        {
            return result(PERMISSION_DENIED, throwableMessage(exception));
        }
        catch (Throwable throwable)
        {
            return result(OPERATION_FAILED, throwableMessage(throwable));
        }

        return OK;
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

        // The GATT properties byte; bits above it are Android's own.
        return entry.gattCharacteristic.getProperties() & 0xFF;
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
            // API 33 takes the value with the call; before, it travels on the
            // shared characteristic object.
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU)
                return gatt.writeCharacteristic(gattCharacteristic, payload, androidWriteType) ==
                    BluetoothStatusCodes.SUCCESS;

            gattCharacteristic.setWriteType(androidWriteType);
            //noinspection deprecation
            gattCharacteristic.setValue(payload);
            //noinspection deprecation
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
        // null is a value the generated decoder did not know.
        final int mode = subscribeMode != null ? subscribeMode.value() : -1;

        if (!initialized)
            return result(NOT_INITIALIZED, "Bluetooth is not initialized");

        if (mode != SUBSCRIBE_MODE_UNSUBSCRIBE &&
            mode != SUBSCRIBE_MODE_NOTIFY &&
            mode != SUBSCRIBE_MODE_INDICATE)
            return result(INVALID_ARGUMENT, "Invalid BluetoothLeSubscribeMode value");

        final LeCharacteristicEntry charEntry = getLeCharacteristic(characteristic);

        if (charEntry == null)
            return result(INVALID_HANDLE, "Invalid BLE characteristic handle");

        LeConnectionEntry connEntry = getLeConnection(charEntry.connection);

        if (connEntry == null || connEntry.gatt == null || !connEntry.connected)
            return result(DISCONNECTED, "BLE connection is not connected");

        final BluetoothGatt gatt = connEntry.gatt;
        final BluetoothGattCharacteristic gattCharacteristic =
            charEntry.gattCharacteristic;

        int properties = gattCharacteristic.getProperties();

        if (mode == SUBSCRIBE_MODE_NOTIFY &&
            (properties & BluetoothGattCharacteristic.PROPERTY_NOTIFY) == 0)
            return result(NOT_SUPPORTED, "The characteristic does not support notifications");

        if (mode == SUBSCRIBE_MODE_INDICATE &&
            (properties & BluetoothGattCharacteristic.PROPERTY_INDICATE) == 0)
            return result(NOT_SUPPORTED, "The characteristic does not support indications");

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

        final LePendingOp op = new LePendingOp();
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
            if (!gatt.setCharacteristicNotification(
                gattCharacteristic,
                mode != SUBSCRIBE_MODE_UNSUBSCRIBE))
            {
                op.startFailureMessage = "setCharacteristicNotification() failed";
                return false;
            }

            if (writeDescriptorValue(gatt, cccd, cccdValue))
                return true;

            // Nothing reached the peripheral: local delivery goes back to
            // what it was.
            gatt.setCharacteristicNotification(
                gattCharacteristic,
                charEntry.subscribeMode != SUBSCRIBE_MODE_UNSUBSCRIBE);
            return false;
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
        op.start = () -> writeDescriptorValue(gatt, gattDescriptor, payload);

        enqueueGattOp(connEntry, op);

        return OK;
    }


    // API 33 takes the value with the call; before, it travels on the shared
    // descriptor object.
    private static boolean writeDescriptorValue(
        BluetoothGatt gatt,
        BluetoothGattDescriptor descriptor,
        byte[] value)
    {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU)
            return gatt.writeDescriptor(descriptor, value) == BluetoothStatusCodes.SUCCESS;

        //noinspection deprecation
        descriptor.setValue(value);
        //noinspection deprecation
        return gatt.writeDescriptor(descriptor);
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

    // Back to IDLE. A start still waiting for the stack fails with
    // startError, since its AdvertiseCallback will no longer be heard.
    private void stopLeAdvertiseInternal(BluetoothError startError, String startMessage)
    {
        BluetoothLeAdvertiser advertiser;
        AdvertiseCallback callback;
        GMFunction startCallback;
        boolean wasStarting;

        synchronized (leAdvertiseLock)
        {
            wasStarting = leAdvertiseState == ADVERTISE_STARTING;
            advertiser = leAdvertiser;
            callback = leAdvertiseCallback;
            startCallback = leAdvertiseStartCallback;

            leAdvertiseState = ADVERTISE_IDLE;
            leAdvertiser = null;
            leAdvertiseCallback = null;
            leAdvertiseStartCallback = null;
        }

        stopAdvertiser(advertiser, callback);

        if (wasStarting)
            invoke(startCallback, startError, startMessage);
    }


    private static void stopAdvertiser(BluetoothLeAdvertiser advertiser, AdvertiseCallback callback)
    {
        if (advertiser == null || callback == null)
            return;

        try
        {
            advertiser.stopAdvertising(callback);
        }
        catch (Throwable ignored)
        {
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


    private static final String ADVERTISE_BUSY_MESSAGE =
        "Bluetooth LE advertising is already running; stop it first";


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
        BluetoothError ready = requireAdapter();
        if (ready != OK)
            return ready;

        if (settings == null || data == null)
            return result(INVALID_ARGUMENT, "settings and data cannot be undefined");

        // Left undefined, the platform default.
        BluetoothLeAdvertiseTxPower txPower =
            settings.tx_power() != null && settings.tx_power().isPresent()
                ? settings.tx_power().get()
                : null;

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

        synchronized (leAdvertiseLock)
        {
            if (leAdvertiseState != ADVERTISE_IDLE)
                return result(BUSY, ADVERTISE_BUSY_MESSAGE);
        }

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

        int txPowerLevel = txPower != null
            ? advertiseTxPowerLevel(txPower)
            : AdvertiseSettings.ADVERTISE_TX_POWER_MEDIUM;

        AdvertiseSettings advertiseSettings = new AdvertiseSettings.Builder()
            .setAdvertiseMode(AdvertiseSettings.ADVERTISE_MODE_LOW_LATENCY)
            .setTxPowerLevel(txPowerLevel)
            .setConnectable(settings.connectable())
            .build();

        final BluetoothLeAdvertiser startAdvertiser = advertiser;
        final long workerGeneration = generation.get();

        // Each start has its own callback, holding its own GML callback. One
        // that is no longer the current start's - stopped, or from before a
        // shutdown - fires nothing, and an advertisement it reports started
        // is stopped again.
        final AdvertiseCallback advertiseCallback = new AdvertiseCallback()
        {
            private boolean isCurrentStart()
            {
                return leAdvertiseCallback == this &&
                    leAdvertiseState == ADVERTISE_STARTING &&
                    generation.get() == workerGeneration;
            }


            @Override
            public void onStartSuccess(AdvertiseSettings settingsInEffect)
            {
                boolean current;

                synchronized (leAdvertiseLock)
                {
                    current = isCurrentStart();

                    if (current)
                    {
                        leAdvertiseState = ADVERTISE_RUNNING;
                        leAdvertiseStartCallback = null;
                    }
                }

                if (!current)
                {
                    stopAdvertiser(startAdvertiser, this);
                    return;
                }

                invoke(callback, OK, "");
            }


            @Override
            public void onStartFailure(int errorCode)
            {
                boolean current;

                synchronized (leAdvertiseLock)
                {
                    current = isCurrentStart();

                    if (current)
                    {
                        leAdvertiseState = ADVERTISE_IDLE;
                        leAdvertiser = null;
                        leAdvertiseCallback = null;
                        leAdvertiseStartCallback = null;
                    }
                }

                if (!current)
                    return;

                invoke(
                    callback,
                    advertiseFailureError(errorCode),
                    "Advertise start failed: " + advertiseFailureName(errorCode));
            }
        };

        synchronized (leAdvertiseLock)
        {
            if (leAdvertiseState != ADVERTISE_IDLE)
                return result(BUSY, ADVERTISE_BUSY_MESSAGE);

            leAdvertiseState = ADVERTISE_STARTING;
            leAdvertiser = advertiser;
            leAdvertiseCallback = advertiseCallback;
            leAdvertiseStartCallback = callback;
        }

        try
        {
            advertiser.startAdvertising(
                advertiseSettings,
                dataBuilder.build(),
                advertiseCallback);
        }
        catch (Throwable throwable)
        {
            // Failed before it started: back to idle, and no callback fires.
            synchronized (leAdvertiseLock)
            {
                if (leAdvertiseCallback == advertiseCallback)
                {
                    leAdvertiseState = ADVERTISE_IDLE;
                    leAdvertiser = null;
                    leAdvertiseCallback = null;
                    leAdvertiseStartCallback = null;
                }
            }

            return result(OPERATION_FAILED, throwableMessage(throwable));
        }

        return OK;
    }


    @Override
    public BluetoothError bluetooth_le_advertise_stop()
    {
        if (!initialized)
            return result(NOT_INITIALIZED, "Bluetooth is not initialized");

        stopLeAdvertiseInternal(OPERATION_FAILED, "Advertising stopped before it started");
        return OK;
    }


    // True from the start call, while the stack is still bringing it up.
    @Override
    public boolean bluetooth_le_advertise_is_running()
    {
        if (!initialized)
            return false;

        synchronized (leAdvertiseLock)
        {
            return leAdvertiseState != ADVERTISE_IDLE;
        }
    }


    // =========================================================================
    // BLE GATT server
    // =========================================================================

    private void handleServerConnectionStateChange(
        BluetoothDevice device,
        int status,
        int newState)
    {
        long deviceHandle = serverDeviceHandle(device);

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

            // Its requests can no longer be answered, its prepared writes
            // will never be executed, and nothing more is sent to it.
            dropLeServerRequestsOf(device);
            dropLeServerNotifications(safeAddress(device));

            if (connection != null)
            {
                dropLeServerSubscriber(connection);
                eraseLeConnection(connection);
                dispatchLeServerConnectionStateChanged(connection, false, deviceHandle);
            }
        }
    }


    // A central reaching the GATT server enters the device cache without
    // being announced or changing what a scan recorded.
    private long serverDeviceHandle(BluetoothDevice device)
    {
        return upsertDevice(
            TRANSPORT_LE,
            deviceId(TRANSPORT_LE, device),
            safeName(device),
            safeAddress(device),
            0,
            false,
            false,
            device,
            false);
    }


    private void dropLeServerSubscriber(long connection)
    {
        synchronized (leServerSubscriberLock)
        {
            for (HashMap<Long, Integer> subscribers : leServerSubscribers.values())
                subscribers.remove(connection);
        }
    }


    // A connection's CCCD value for the characteristic: what it last wrote,
    // or notifications and indications off.
    private byte[] leServerCccdValue(BluetoothGattCharacteristic characteristic, long connection)
    {
        Integer mode;

        synchronized (leServerSubscriberLock)
        {
            HashMap<Long, Integer> subscribers = leServerSubscribers.get(characteristic);
            mode = subscribers != null ? subscribers.get(connection) : null;
        }

        if (mode != null && mode == SUBSCRIBE_MODE_NOTIFY)
            return BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE;

        if (mode != null && mode == SUBSCRIBE_MODE_INDICATE)
            return BluetoothGattDescriptor.ENABLE_INDICATION_VALUE;

        return BluetoothGattDescriptor.DISABLE_NOTIFICATION_VALUE;
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
        long deviceHandle = serverDeviceHandle(device);

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

                    // A failed add frees its UUID for another try.
                    if (head != null && !head.cancelled && status != BluetoothGatt.GATT_SUCCESS)
                        leServerServiceUuids.remove(head.service.getUuid());
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
                            mapAttError(status),
                            attErrorMessage(status));
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
                    offset,
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

                // The CCCD is the extension's: each central reads back what
                // it wrote, and GML never sees the request.
                if (descriptor != null && parent != null && CCCD_UUID.equals(descriptor.getUuid()))
                {
                    byte[] value = leServerCccdValue(parent, resolveServerConnection(device));

                    if (offset < 0 || offset > value.length)
                        sendServerResponse(device, requestId, ATT_INVALID_OFFSET, offset, null);
                    else
                        sendServerResponse(
                            device,
                            requestId,
                            BluetoothGatt.GATT_SUCCESS,
                            offset,
                            Arrays.copyOfRange(value, offset, value.length));
                    return;
                }

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

                if (descriptor != null && parent != null && CCCD_UUID.equals(descriptor.getUuid()))
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

                    // A central with no server connection - the link this
                    // app opened as a client - cannot be notified through
                    // the server.
                    if (connection != 0)
                    {
                        synchronized (leServerSubscriberLock)
                        {
                            HashMap<Long, Integer> subscribers = leServerSubscribers.get(parent);

                            if (mode == SUBSCRIBE_MODE_UNSUBSCRIBE)
                            {
                                if (subscribers != null)
                                    subscribers.remove(connection);
                            }
                            else
                            {
                                if (subscribers == null)
                                {
                                    subscribers = new HashMap<>();
                                    leServerSubscribers.put(parent, subscribers);
                                }

                                subscribers.put(connection, mode);
                            }
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
                    offset,
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
                ArrayList<Integer> offsets = new ArrayList<>();

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
                        request.batch = batch;

                        // Fragments are laid out from 0; GML gets them from
                        // the first fragment's offset, as a single write.
                        int start = Math.min(write.offset, write.value.length);
                        request.writeValue = Arrays.copyOfRange(write.value, start, write.value.length);

                        ids.add(storeLeServerRequest(request));
                        requests.add(request);
                        offsets.add(start);
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
                        request.descriptorUuid,
                        offsets.get(i),
                        true);
                }
            }


            // The stack has sent the device's notification in flight (or an
            // indication was confirmed); the next one waiting goes now.
            @Override
            public void onNotificationSent(BluetoothDevice device, int status)
            {
                if (generation.get() != workerGeneration)
                    return;

                String address = safeAddress(device);

                synchronized (leServerNotifyLock)
                {
                    // Dropped meanwhile: nothing of this device's is in flight.
                    if (!leServerNotifySending.contains(address))
                        return;

                    ArrayDeque<LeServerNotification> queue = leServerNotifyQueues.get(address);

                    if (queue != null && queue.pollFirst() != null)
                        leServerNotifyCount--;
                }

                sendLeServerNotifications(address);
            }
        };
    }


    // Called by whoever holds the device's sending turn: sends the head of
    // its queue, dropping one the stack refuses and trying the next. Returns
    // with the turn handed back once the queue is empty.
    private void sendLeServerNotifications(String address)
    {
        while (true)
        {
            LeServerNotification next;

            synchronized (leServerNotifyLock)
            {
                ArrayDeque<LeServerNotification> queue = leServerNotifyQueues.get(address);
                next = queue != null ? queue.peekFirst() : null;

                if (next == null)
                {
                    leServerNotifyQueues.remove(address);
                    leServerNotifySending.remove(address);
                    return;
                }
            }

            if (sendLeServerNotification(next))
                return;

            synchronized (leServerNotifyLock)
            {
                ArrayDeque<LeServerNotification> queue = leServerNotifyQueues.get(address);

                if (queue != null && queue.peekFirst() == next)
                {
                    queue.pollFirst();
                    leServerNotifyCount--;
                }
            }
        }
    }


    // API 33 takes the value with the call; before, it travels on the shared
    // characteristic object, so it is set right before the send.
    private boolean sendLeServerNotification(LeServerNotification notification)
    {
        BluetoothGattServer server = gattServer;

        if (server == null)
            return false;

        try
        {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU)
                return server.notifyCharacteristicChanged(
                    notification.device,
                    notification.characteristic,
                    notification.confirm,
                    notification.value) == BluetoothStatusCodes.SUCCESS;

            //noinspection deprecation
            notification.characteristic.setValue(notification.value);
            //noinspection deprecation
            return server.notifyCharacteristicChanged(
                notification.device,
                notification.characteristic,
                notification.confirm);
        }
        catch (Throwable ignored)
        {
            return false;
        }
    }


    // Drops what waits for a device that disconnected, or for every device
    // when address is null. A send still in flight completes into nothing.
    private void dropLeServerNotifications(String address)
    {
        synchronized (leServerNotifyLock)
        {
            if (address == null)
            {
                leServerNotifyQueues.clear();
                leServerNotifySending.clear();
                leServerNotifyCount = 0;
                return;
            }

            ArrayDeque<LeServerNotification> queue = leServerNotifyQueues.remove(address);

            if (queue != null)
                leServerNotifyCount -= queue.size();

            leServerNotifySending.remove(address);
        }
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
        int offset,
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
            request.descriptorUuid,
            offset,
            responseNeeded);
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
                    prepared.offset = offset;
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


    // reportDisconnects: bluetooth_le_server_stop tells the game each server
    // connection it ends; shutdown is silent.
    private void stopLeServerInternal(BluetoothError addError, String addMessage, boolean reportDisconnects)
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

        ArrayList<LeConnectionEntry> serverConnections = new ArrayList<>();

        synchronized (leConnectionLock)
        {
            for (long connection : leServerConnectionByDevice.values())
            {
                LeConnectionEntry entry = leConnections.get(connection);
                if (entry != null)
                    serverConnections.add(entry);
            }

            leServerConnectionByDevice.clear();
        }

        synchronized (leServerRequestLock)
        {
            leServerRequests.clear();
            leServerPreparedWrites.clear();
        }

        synchronized (leServerSubscriberLock)
        {
            leServerSubscribers.clear();
        }

        dropLeServerNotifications(null);

        for (LeConnectionEntry entry : serverConnections)
        {
            eraseLeConnection(entry.handle);

            if (reportDisconnects)
                dispatchLeServerConnectionStateChanged(entry.handle, false, entry.device);
        }

        // The server is closed, so no onServiceAdded will come for any of them.
        failServiceAdds(addError, addMessage, true);

        synchronized (leServerAddServiceLock)
        {
            leServerServiceUuids.clear();
        }
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

                if (fire)
                    leServerServiceUuids.remove(head.service.getUuid());
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
        BluetoothError ready = requireAdapter();
        if (ready != OK)
            return ready;

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
            "BLE server stopped before the service was added",
            true);
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

                // BluetoothLeCharacteristicProperty is the GATT properties byte.
                if ((properties & ~0xFF) != 0)
                    return result(
                        INVALID_ARGUMENT,
                        "Characteristic " + characteristicUuid + ": properties " + properties +
                            " has bits that are not BluetoothLeCharacteristicProperty flags");

                BluetoothGattCharacteristic characteristic =
                    new BluetoothGattCharacteristic(
                        parseUuid(characteristicUuid),
                        properties,
                        permissions);

                if ((permissions & ~ATTRIBUTE_PERMISSION_ALL) != 0)
                    return result(
                        INVALID_ARGUMENT,
                        "Characteristic " + characteristicUuid + ": permissions " + permissions +
                            " has bits that are not BluetoothLeAttributePermission flags");

                // An empty array is no initial value.
                byte[] initialValue = objectBytes(characteristicObject, "value");
                if (initialValue.length > MAX_ATTRIBUTE_LENGTH)
                    return result(
                        INVALID_ARGUMENT,
                        "Characteristic " + characteristicUuid + ": the initial value is " +
                            initialValue.length + " bytes; an attribute value holds at most " +
                            MAX_ATTRIBUTE_LENGTH);

                // Served from leServerInitialValues by the read request.
                if (initialValue.length > 0)
                    initialValues.put(characteristic, initialValue);

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

        // A failure from here on, the stack refusing the add included, is
        // reported through the callback.
        LeServerAddEntry entry = new LeServerAddEntry();
        entry.service = gattService;
        entry.callback = callback;

        synchronized (leServerAddServiceLock)
        {
            if (!leServerServiceUuids.add(gattService.getUuid()))
                return result(BUSY, "A service with UUID " + serviceUuid + " has already been added");

            leServerAddQueue.addLast(entry);
        }

        synchronized (leServerInitialValueLock)
        {
            leServerInitialValues.putAll(initialValues);
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

        // A stopped server has no services: nothing to clear.
        if (server == null)
            return OK;

        // The services the requests name are going away.
        answerPendingLeServerRequests();

        synchronized (leServerInitialValueLock)
        {
            leServerInitialValues.clear();
        }

        synchronized (leServerSubscriberLock)
        {
            leServerSubscribers.clear();
        }

        dropLeServerNotifications(null);

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

        synchronized (leServerAddServiceLock)
        {
            leServerServiceUuids.clear();
        }

        return OK;
    }


    @Override
    public BluetoothError bluetooth_le_server_respond_read(
        int request_id,
        BluetoothAttError error_code,
        ByteBuffer data,
        int offset,
        int size)
    {
        if (!initialized)
            return result(NOT_INITIALIZED, "Bluetooth is not initialized");

        if (error_code == null)
            return result(INVALID_ARGUMENT, "error_code is not a BluetoothAttError");

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

        // A failure answer carries no value, so the buffer is neither checked
        // nor read.
        boolean success = error_code == BluetoothAttError.Success;
        byte[] payload = success ? new byte[0] : null;

        if (success && size > 0)
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

        // The ATT code itself; Success is GATT_SUCCESS (0).
        int status = error_code.value();

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
    public BluetoothError bluetooth_le_server_respond_write(int request_id, BluetoothAttError error_code)
    {
        if (!initialized)
            return result(NOT_INITIALIZED, "Bluetooth is not initialized");

        if (error_code == null)
            return result(INVALID_ARGUMENT, "error_code is not a BluetoothAttError");

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

        // The ATT code itself; Success is GATT_SUCCESS (0).
        int status = error_code.value();

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
            server.sendResponse(
                request.device,
                request.stackRequestId,
                status,
                0,
                status == BluetoothGatt.GATT_SUCCESS ? request.writeValue : null);
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
            return result(NOT_FOUND, "Unknown local service_uuid");

        try
        {
            characteristic = service.getCharacteristic(parseUuid(characteristic_uuid));
        }
        catch (Throwable throwable)
        {
            return result(INVALID_ARGUMENT, "Invalid characteristic_uuid");
        }

        if (characteristic == null)
            return result(NOT_FOUND, "Unknown local characteristic_uuid");

        if (connection != 0)
        {
            LeConnectionEntry entry = getLeConnection(connection);

            if (entry == null || !entry.serverRole)
                return result(INVALID_HANDLE, "Invalid BLE server connection handle");
        }

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

        // The subscribers and the mode each wrote: an indication is
        // confirmed by its central, a notification is not.
        HashMap<Long, Integer> targets = new HashMap<>();

        synchronized (leServerSubscriberLock)
        {
            HashMap<Long, Integer> subscribers = leServerSubscribers.get(characteristic);

            if (connection != 0)
            {
                Integer mode = subscribers != null ? subscribers.get(connection) : null;

                if (mode == null)
                    return result(NOT_FOUND, "The central is not subscribed to this characteristic");

                targets.put(connection, mode);
            }
            else if (subscribers != null)
            {
                targets.putAll(subscribers);
            }
        }

        ArrayList<LeServerNotification> notifications = new ArrayList<>();

        for (Map.Entry<Long, Integer> target : targets.entrySet())
        {
            LeConnectionEntry entry = getLeConnection(target.getKey());

            if (entry == null || entry.remoteDevice == null || !entry.connected)
                continue;

            LeServerNotification notification = new LeServerNotification();
            notification.device = entry.remoteDevice;
            notification.characteristic = characteristic;
            notification.value = payload;
            notification.confirm = target.getValue() == SUBSCRIBE_MODE_INDICATE;
            notifications.add(notification);
        }

        if (notifications.isEmpty())
            return OK;

        // Queued whole or not at all; each device whose turn is free starts
        // sending.
        ArrayList<String> start = new ArrayList<>();

        synchronized (leServerNotifyLock)
        {
            if (leServerNotifyCount + notifications.size() > MAX_QUEUED_NOTIFICATIONS)
                return result(BUSY, "Too many GATT notifications are waiting to be sent");

            for (LeServerNotification notification : notifications)
            {
                String address = safeAddress(notification.device);
                ArrayDeque<LeServerNotification> queue = leServerNotifyQueues.get(address);

                if (queue == null)
                {
                    queue = new ArrayDeque<>();
                    leServerNotifyQueues.put(address, queue);
                }

                queue.addLast(notification);
                leServerNotifyCount++;

                if (leServerNotifySending.add(address))
                    start.add(address);
            }
        }

        for (String address : start)
            sendLeServerNotifications(address);

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
            try
            {
                if (!initialized || intent == null)
                    return;

                String action = intent.getAction();

                if (BluetoothDevice.ACTION_FOUND.equals(action))
                {
                    BluetoothDevice device = deviceExtra(intent);

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
                        device,
                        true);
                }
                else if (BluetoothAdapter.ACTION_DISCOVERY_STARTED.equals(action))
                {
                    if (classicScanning.get())
                        classicDiscoveryStarted = true;
                }
                else if (BluetoothAdapter.ACTION_DISCOVERY_FINISHED.equals(action))
                {
                    // A FINISHED before this scan's STARTED is the discovery
                    // scan_start cancelled; one while the adapter still
                    // discovers is stale too.
                    if (!classicDiscoveryStarted)
                        return;

                    BluetoothAdapter current = adapter;

                    if (current != null && current.isDiscovering())
                        return;

                    if (classicScanning.getAndSet(false))
                        dispatchScanStopped(
                            TRANSPORT_CLASSIC,
                            OK,
                            "");
                }
            }
            catch (Throwable ignored)
            {
            }
        }
    };


    private boolean ensureClassicReceiver()
    {
        if (receiverRegistered)
            return true;

        IntentFilter filter = new IntentFilter();
        filter.addAction(BluetoothDevice.ACTION_FOUND);
        filter.addAction(BluetoothAdapter.ACTION_DISCOVERY_STARTED);
        filter.addAction(BluetoothAdapter.ACTION_DISCOVERY_FINISHED);

        receiverRegistered = registerSystemReceiver(classicReceiver, filter);
        return receiverRegistered;
    }


    private void unregisterClassicReceiver()
    {
        if (!receiverRegistered)
            return;

        unregisterSystemReceiver(classicReceiver);
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
            try
            {
                if (!initialized || intent == null)
                    return;

                if (!BluetoothDevice.ACTION_BOND_STATE_CHANGED.equals(intent.getAction()))
                    return;

                BluetoothDevice device = deviceExtra(intent);

                if (device == null)
                    return;

                int bondState = intent.getIntExtra(
                    BluetoothDevice.EXTRA_BOND_STATE,
                    BluetoothDevice.BOND_NONE);

                // Still in progress - wait for the terminal BOND_BONDED/BOND_NONE state.
                if (bondState == BluetoothDevice.BOND_BONDING)
                    return;

                String address = safeAddress(device);
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
            catch (Throwable ignored)
            {
            }
        }
    };


    private boolean ensureBondReceiver()
    {
        if (bondReceiverRegistered)
            return true;

        bondReceiverRegistered = registerSystemReceiver(
            bondReceiver,
            new IntentFilter(BluetoothDevice.ACTION_BOND_STATE_CHANGED));
        return bondReceiverRegistered;
    }


    private void unregisterBondReceiver()
    {
        if (!bondReceiverRegistered)
            return;

        unregisterSystemReceiver(bondReceiver);
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
        BluetoothError ready = requireAdapter();
        if (ready != OK)
            return ready;

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

        if (androidDevice == null &&
            (deviceEntry.address == null || deviceEntry.address.isEmpty()))
            return result(
                INVALID_ARGUMENT,
                "Bluetooth device has no usable address");

        int bondState;

        try
        {
            if (androidDevice == null)
                androidDevice = adapter.getRemoteDevice(deviceEntry.address);

            bondState = androidDevice.getBondState();
        }
        catch (SecurityException exception)
        {
            return result(PERMISSION_DENIED, throwableMessage(exception));
        }
        catch (Throwable throwable)
        {
            return result(OPERATION_FAILED, throwableMessage(throwable));
        }

        if (bondState == BluetoothDevice.BOND_BONDED)
        {
            invoke(callback, OK, "", (double) device);
            return OK;
        }

        if (!ensureBondReceiver())
            return result(OPERATION_FAILED, RECEIVER_FAILED_MESSAGE);

        String address = safeAddress(androidDevice);

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

        // getBondState needs BLUETOOTH_CONNECT from Android 12.
        if (!hasConnectPermission())
        {
            setLastError(PERMISSION_DENIED, "Bluetooth connect permission is not granted");
            return false;
        }

        BluetoothDevice androidDevice = deviceEntry.androidDevice;

        try
        {
            if (androidDevice == null)
            {
                if (deviceEntry.address == null || deviceEntry.address.isEmpty())
                    return false;

                androidDevice = adapter.getRemoteDevice(deviceEntry.address);
            }

            return androidDevice.getBondState() == BluetoothDevice.BOND_BONDED;
        }
        catch (Throwable ignored)
        {
            return false;
        }
    }


    @Override
    public BluetoothError bluetooth_classic_scan_start()
    {
        BluetoothError ready = requireAdapter();
        if (ready != OK)
            return ready;

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

        if (!ensureClassicReceiver())
            return result(OPERATION_FAILED, RECEIVER_FAILED_MESSAGE);

        try
        {
            if (adapter.isDiscovering())
                adapter.cancelDiscovery();

            // Armed by this discovery's ACTION_DISCOVERY_STARTED.
            classicDiscoveryStarted = false;
            classicScanning.set(true);

            if (!adapter.startDiscovery())
            {
                classicScanning.set(false);
                return result(
                    OPERATION_FAILED,
                    "Android Bluetooth discovery could not start");
            }

            return OK;
        }
        catch (SecurityException exception)
        {
            classicScanning.set(false);
            return result(
                PERMISSION_DENIED,
                throwableMessage(exception));
        }
        catch (Throwable throwable)
        {
            classicScanning.set(false);
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


    // Built under deviceLock rather than through copyDevice, which would copy
    // the record for every getter. Empty for an unknown handle, a Classic
    // device or one never seen advertising, as nothing was merged into it.
    @Override
    public BluetoothLeAdvertisement bluetooth_device_get_advertisement(long device)
    {
        ArrayList<String> serviceUuids = new ArrayList<>();
        ArrayList<BluetoothLeAdvertiseServiceData> serviceData = new ArrayList<>();
        ArrayList<BluetoothLeAdvertiseManufacturerData> manufacturerData = new ArrayList<>();
        Integer txPower = null;

        if (isHandleType(device, HANDLE_TYPE_DEVICE))
        {
            synchronized (deviceLock)
            {
                DeviceEntry entry = devices.get(device);

                if (entry != null)
                {
                    serviceUuids.addAll(entry.advServiceUuids);

                    for (Map.Entry<String, byte[]> item : entry.advServiceData.entrySet())
                        serviceData.add(new BluetoothLeAdvertiseServiceData(
                            item.getKey(),
                            toByteList(item.getValue())));

                    for (Map.Entry<Integer, byte[]> item : entry.advManufacturerData.entrySet())
                        manufacturerData.add(new BluetoothLeAdvertiseManufacturerData(
                            item.getKey(),
                            toByteList(item.getValue())));

                    txPower = entry.advTxPower;
                }
            }
        }

        return new BluetoothLeAdvertisement(
            serviceUuids,
            serviceData,
            manufacturerData,
            Optional.ofNullable(txPower));
    }


    private static final String ID_PREFIX_LE = "android:ble:";
    private static final String ID_PREFIX_CLASSIC = "android:classic:";

    @Override
    public long bluetooth_device_from_id(String id)
    {
        if (!initialized)
        {
            setLastError(NOT_INITIALIZED, NOT_INITIALIZED_MESSAGE);
            return 0;
        }

        String key = id != null ? id : "";

        synchronized (deviceLock)
        {
            Long existing = deviceById.get(key);
            if (existing != null)
                return existing;
        }

        int transport;
        String address;

        if (key.startsWith(ID_PREFIX_LE))
        {
            transport = TRANSPORT_LE;
            address = key.substring(ID_PREFIX_LE.length());
        }
        else if (key.startsWith(ID_PREFIX_CLASSIC))
        {
            transport = TRANSPORT_CLASSIC;
            address = key.substring(ID_PREFIX_CLASSIC.length());
        }
        else
        {
            setLastError(INVALID_ARGUMENT, "Not an Android device id: " + key);
            return 0;
        }

        // checkBluetoothAddress takes only upper case; the ids this file issues
        // are, but a game may have stored one lowered.
        address = address.toUpperCase(java.util.Locale.ROOT);

        if (!BluetoothAdapter.checkBluetoothAddress(address))
        {
            setLastError(INVALID_ARGUMENT, "Malformed Bluetooth address in device id: " + key);
            return 0;
        }

        BluetoothAdapter current = adapter;

        if (current == null)
        {
            setLastError(NOT_SUPPORTED, NO_ADAPTER_MESSAGE);
            return 0;
        }

        BluetoothDevice device;

        try
        {
            device = current.getRemoteDevice(address);
        }
        catch (Throwable throwable)
        {
            setLastError(INVALID_ARGUMENT, throwableMessage(throwable));
            return 0;
        }

        // The id is rebuilt from the device, so a lowered id lands on the
        // entry cached under the canonical one. Not discovered: no device_found.
        long handle = upsertDevice(
            transport,
            deviceId(transport, device),
            safeName(device),
            safeAddress(device),
            0,
            false,
            true,
            device,
            false);

        if (handle == 0)
            setLastError(OPERATION_FAILED, "The device could not be added to the cache");

        return handle;
    }


    // The core's check_radio for the device queries: no adapter, the connect
    // permission (the queries read bonds and connections) and the radio.
    private BluetoothError checkQueryRadio()
    {
        if (adapter == null)
            return result(NOT_SUPPORTED, NO_ADAPTER_MESSAGE);

        if (!hasConnectPermission())
            return result(
                PERMISSION_DENIED,
                "Bluetooth connect permission is not granted");

        if (!adapterEnabled())
            return result(BLUETOOTH_DISABLED, "Bluetooth is disabled");

        return OK;
    }


    // A device a query found enters the cache without device_found; the
    // handles keep the stack's order, each once.
    private void addQueriedDevice(int transport, BluetoothDevice device, ArrayList<Double> handles)
    {
        long handle = upsertDevice(
            transport,
            deviceId(transport, device),
            safeName(device),
            safeAddress(device),
            0,
            false,
            true,
            device,
            false);

        if (handle != 0 && !handles.contains((double) handle))
            handles.add((double) handle);
    }


    // Answered at once: getConnectedDevices is synchronous. The handles reach
    // GML as an array of numbers, as the core's vector<double> does.
    @Override
    public BluetoothError bluetooth_le_connected_devices_query(List<String> service_uuids, GMFunction callback)
    {
        if (!initialized)
            return result(NOT_INITIALIZED, NOT_INITIALIZED_MESSAGE);

        // Checked as everywhere, though Android lists every connected device.
        if (service_uuids != null)
        {
            for (String uuid : service_uuids)
            {
                if (!isValidUuid(uuid))
                    return result(INVALID_ARGUMENT, "Invalid UUID: " + uuid);
            }
        }

        BluetoothError ready = checkQueryRadio();
        if (ready != OK)
            return ready;

        Context current = context();

        if (current == null)
            return result(
                OPERATION_FAILED,
                "Android application context is unavailable");

        List<BluetoothDevice> connected;

        try
        {
            BluetoothManager manager =
                (BluetoothManager) current.getSystemService(Context.BLUETOOTH_SERVICE);

            if (manager == null)
                return result(NOT_SUPPORTED, "Bluetooth manager is unavailable");

            connected = manager.getConnectedDevices(BluetoothProfile.GATT);
        }
        catch (SecurityException exception)
        {
            return result(PERMISSION_DENIED, throwableMessage(exception));
        }
        catch (Throwable throwable)
        {
            return result(OPERATION_FAILED, throwableMessage(throwable));
        }

        ArrayList<Double> handles = new ArrayList<>();

        if (connected != null)
        {
            for (BluetoothDevice device : connected)
            {
                if (device != null)
                    addQueriedDevice(TRANSPORT_LE, device, handles);
            }
        }

        invoke(callback, OK, "", handles);
        return OK;
    }


    // getBondedDevices is synchronous too. A bond is typed by getType(): a
    // dual-mode device gets an entry of each transport, an unknown one is
    // taken as Classic.
    @Override
    public BluetoothError bluetooth_paired_devices_query(GMFunction callback)
    {
        if (!initialized)
            return result(NOT_INITIALIZED, NOT_INITIALIZED_MESSAGE);

        if (!bluetooth_feature_is_supported(BluetoothFeature.PairedDevicesQuery))
            return result(
                NOT_SUPPORTED,
                "Listing paired devices is not supported on this platform");

        BluetoothError ready = checkQueryRadio();
        if (ready != OK)
            return ready;

        // Read whole before any entry is added, so a refusal half way leaves
        // the cache as it was.
        ArrayList<BluetoothDevice> bondedDevices = new ArrayList<>();
        ArrayList<Integer> bondedTypes = new ArrayList<>();

        try
        {
            Set<BluetoothDevice> bonded = adapter.getBondedDevices();

            if (bonded != null)
            {
                for (BluetoothDevice device : bonded)
                {
                    if (device == null)
                        continue;

                    bondedDevices.add(device);
                    bondedTypes.add(device.getType());
                }
            }
        }
        catch (SecurityException exception)
        {
            return result(PERMISSION_DENIED, throwableMessage(exception));
        }
        catch (Throwable throwable)
        {
            return result(OPERATION_FAILED, throwableMessage(throwable));
        }

        ArrayList<Double> handles = new ArrayList<>();

        for (int i = 0; i < bondedDevices.size(); i++)
        {
            BluetoothDevice device = bondedDevices.get(i);
            int type = bondedTypes.get(i);

            if (type == BluetoothDevice.DEVICE_TYPE_LE)
            {
                addQueriedDevice(TRANSPORT_LE, device, handles);
            }
            else if (type == BluetoothDevice.DEVICE_TYPE_DUAL)
            {
                addQueriedDevice(TRANSPORT_CLASSIC, device, handles);
                addQueriedDevice(TRANSPORT_LE, device, handles);
            }
            else
            {
                addQueriedDevice(TRANSPORT_CLASSIC, device, handles);
            }
        }

        invoke(callback, OK, "", handles);
        return OK;
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
        if (requireAdapter() != OK)
            return 0;

        final DeviceEntry deviceEntry = copyDevice(device);

        if (deviceEntry == null)
        {
            setLastError(
                INVALID_HANDLE,
                "Invalid device handle");
            return 0;
        }

        if (deviceEntry.transport != TRANSPORT_CLASSIC)
        {
            setLastError(
                INVALID_ARGUMENT,
                "Expected a Bluetooth Classic device");
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
        final ConnectionEntry entry = getConnection(connection);

        entry.connectCallback = callback;
        entry.connectPending.set(true);

        // A bluetooth_classic_disconnect while this runs reports the connect
        // cancelled and retires the handle; the thread then closes what it
        // made and reports nothing (dispatchConnectResult fires once).
        Thread thread = new Thread(
            () ->
            {
                BluetoothSocket socket = null;

                try
                {
                    if (
                        entry.manualClosing ||
                        !initialized ||
                        generation.get() != workerGeneration)
                    {
                        return;
                    }

                    BluetoothAdapter current = adapter;

                    try
                    {
                        if (current.isDiscovering())
                            current.cancelDiscovery();
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
                                entry,
                                INVALID_ARGUMENT,
                                "Bluetooth Classic device has no usable address");
                            return;
                        }

                        androidDevice =
                            current.getRemoteDevice(
                                deviceEntry.address);
                    }

                    socket =
                        androidDevice.createRfcommSocketToServiceRecord(
                            uuid);

                    entry.socket = socket;

                    // A disconnect that ran before the socket was stored
                    // had none to close.
                    if (
                        entry.manualClosing ||
                        generation.get() != workerGeneration)
                    {
                        closeQuietly(socket);
                        return;
                    }

                    socket.connect();

                    if (
                        entry.manualClosing ||
                        !initialized ||
                        generation.get() != workerGeneration ||
                        !dispatchConnectResult(entry, OK, ""))
                    {
                        closeQuietly(socket);
                        return;
                    }

                    startReadLoop(
                        entry,
                        socket,
                        workerGeneration);
                }
                catch (SecurityException exception)
                {
                    closeQuietly(socket);

                    if (generation.get() == workerGeneration)
                        dispatchConnectResult(
                            entry,
                            PERMISSION_DENIED,
                            throwableMessage(exception));
                }
                catch (IOException exception)
                {
                    closeQuietly(socket);

                    if (generation.get() == workerGeneration)
                        dispatchConnectResult(
                            entry,
                            CONNECTION_FAILED,
                            throwableMessage(exception));
                }
                catch (Throwable throwable)
                {
                    closeQuietly(socket);

                    if (generation.get() == workerGeneration)
                        dispatchConnectResult(
                            entry,
                            OPERATION_FAILED,
                            throwableMessage(throwable));
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


    // Reads into the entry's receive queue until the link ends. While the
    // queue holds MAX_QUEUED_RECEIVE_BYTES the loop stops reading, so the
    // peer is held back by RFCOMM flow control instead of the queue growing;
    // bluetooth_classic_receive, a disconnect and shutdown wake it.
    private void startReadLoop(
        final ConnectionEntry entry,
        final BluetoothSocket socket,
        final long workerGeneration)
    {
        startWriteLoop(entry, socket, workerGeneration);

        Thread thread = new Thread(
            () ->
            {
                BluetoothError error = DISCONNECTED;
                String message = "Remote device disconnected";

                try
                {
                    InputStream input = socket.getInputStream();
                    byte[] buffer = new byte[4096];

                    while (
                        initialized &&
                        generation.get() == workerGeneration)
                    {
                        synchronized (entry.receiveLock)
                        {
                            while (
                                entry.receiveAvailable >= MAX_QUEUED_RECEIVE_BYTES &&
                                !entry.manualClosing &&
                                initialized &&
                                generation.get() == workerGeneration)
                            {
                                entry.receiveLock.wait();
                            }
                        }

                        if (entry.manualClosing)
                            break;

                        int count = input.read(buffer);

                        if (count < 0)
                            break;

                        if (count == 0)
                            continue;

                        appendReceived(
                            entry,
                            Arrays.copyOf(buffer, count));
                    }
                }
                catch (IOException exception)
                {
                    message = throwableMessage(exception);
                }
                catch (Throwable throwable)
                {
                    error = OPERATION_FAILED;
                    message = throwableMessage(throwable);
                }
                finally
                {
                    closeQuietly(socket);
                }

                // A link the game closed was retired by its call and reports
                // nothing; shutdown and a stale generation report nothing
                // either. The writer still has to learn the link is gone.
                if (
                    !entry.manualClosing &&
                    initialized &&
                    generation.get() == workerGeneration)
                {
                    dispatchDisconnected(entry, error, message);
                }
                else
                {
                    entry.finished = true;
                    wakeWriter(entry);
                }
            },
            "GMBluetooth-RFCOMM-Read-" + entry.handle);

        thread.setDaemon(true);
        thread.start();
    }


    // Synchronous, as on every platform: the handle is retired by the call. A
    // connect still in progress is cancelled and fires its callback
    // ConnectionFailed; an established link fires no classic_disconnected.
    @Override
    public BluetoothError bluetooth_classic_disconnect(long connection)
    {
        if (!initialized)
            return result(NOT_INITIALIZED, NOT_INITIALIZED_MESSAGE);

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

        entry.manualClosing = true;

        if (entry.connectPending.getAndSet(false))
        {
            // Still connecting: closing the socket, if the connect thread has
            // made one, ends the attempt, and the thread drops out on
            // manualClosing.
            GMFunction connectCallback = entry.connectCallback;
            entry.connectCallback = null;

            eraseConnection(connection);
            closeQuietly(entry.socket);

            invoke(
                connectCallback,
                CONNECTION_FAILED,
                "Connection cancelled by bluetooth_classic_disconnect",
                (double) connection,
                (double) entry.device);

            return OK;
        }

        eraseConnection(connection);
        wakeReader(entry);

        final BluetoothSocket socket = entry.socket;

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

        if (pending && socket != null)
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

                    closeQuietly(socket);
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

        byte[] copiedBytes;
        boolean drained;

        synchronized (entry.receiveLock)
        {
            // The next bytes to arrive are announced again, and a read loop
            // held back by a full queue reads on.
            entry.dataPending = false;
            entry.receiveLock.notifyAll();

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
        BluetoothError ready = requireAdapter();
        if (ready != OK)
            return ready;

        if (!hasConnectPermission())
            return result(
                PERMISSION_DENIED,
                "Bluetooth connect permission is not granted");

        if (!adapterEnabled())
            return result(
                BLUETOOTH_DISABLED,
                "Bluetooth is disabled");

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

        final ClassicServer server = new ClassicServer(
            name == null || name.isEmpty() ? "GMBluetooth RFCOMM" : name,
            uuid);

        synchronized (classicServerLock)
        {
            // A running server is not restarted with the new name and UUID.
            if (currentServer != null)
                return result(
                    BUSY,
                    "A Classic server is already running; stop it first");

            try
            {
                server.socket =
                    adapter.listenUsingRfcommWithServiceRecord(
                        server.name,
                        server.uuid);
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

            currentServer = server;
        }

        final long workerGeneration = generation.get();

        Thread thread = new Thread(
            () ->
            {
                try
                {
                    acceptLoop(server, workerGeneration);
                }
                finally
                {
                    // Only this run's state: a newer server is left alone.
                    server.running = false;
                    closeQuietly(server.socket);

                    synchronized (classicServerLock)
                    {
                        if (currentServer == server)
                            currentServer = null;
                    }
                }
            },
            "GMBluetooth-RFCOMM-Accept");

        thread.setDaemon(true);
        thread.start();

        return OK;
    }


    private boolean serverActive(ClassicServer server, long workerGeneration)
    {
        return server.running &&
            currentServer == server &&
            initialized &&
            generation.get() == workerGeneration;
    }


    // Accepts on the server's own listener until it is stopped. A listener
    // that fails under a running server - the stack restarting, a radio
    // toggle - is replaced, with a growing pause, while the radio is on.
    private void acceptLoop(ClassicServer server, long workerGeneration)
    {
        long relistenDelay = SERVER_RELISTEN_FIRST_MS;

        while (serverActive(server, workerGeneration))
        {
            BluetoothServerSocket listener = server.socket;

            if (listener == null)
                return;

            BluetoothSocket socket;

            try
            {
                socket = listener.accept();
            }
            catch (IOException exception)
            {
                if (!serverActive(server, workerGeneration))
                    return;

                closeQuietly(listener);
                server.socket = null;

                if (!relisten(server, workerGeneration, relistenDelay))
                    return;

                relistenDelay = Math.min(relistenDelay * 2, SERVER_RELISTEN_MAX_MS);
                continue;
            }
            catch (Throwable throwable)
            {
                return;
            }

            relistenDelay = SERVER_RELISTEN_FIRST_MS;

            if (socket == null)
                continue;

            try
            {
                acceptConnection(socket, workerGeneration);
            }
            catch (Throwable throwable)
            {
                closeQuietly(socket);
            }
        }
    }


    // Waits, then listens again; false when the server was stopped meanwhile,
    // the radio is off or the listen failed.
    private boolean relisten(ClassicServer server, long workerGeneration, long delay)
    {
        try
        {
            Thread.sleep(delay);
        }
        catch (InterruptedException ignored)
        {
            return false;
        }

        if (!serverActive(server, workerGeneration) || !adapterEnabled())
            return false;

        try
        {
            server.socket = adapter.listenUsingRfcommWithServiceRecord(server.name, server.uuid);
        }
        catch (Throwable throwable)
        {
            return false;
        }

        // A stop that ran while the listener was being made did not see it.
        if (!server.running)
        {
            closeQuietly(server.socket);
            return false;
        }

        return true;
    }


    private void acceptConnection(BluetoothSocket socket, long workerGeneration)
    {
        BluetoothDevice remote = socket.getRemoteDevice();

        long device = upsertDevice(
            TRANSPORT_CLASSIC,
            deviceId(TRANSPORT_CLASSIC, remote),
            safeName(remote),
            safeAddress(remote),
            0,
            false,
            true,
            remote,
            false);

        long connection = createConnection(device);
        ConnectionEntry entry = getConnection(connection);

        if (entry == null)
        {
            closeQuietly(socket);
            return;
        }

        entry.socket = socket;
        entry.connected = true;

        invoke(
            callbackClassicClientConnected,
            (double) connection,
            (double) device);

        startReadLoop(entry, socket, workerGeneration);
    }


    private void stopServerInternal()
    {
        ClassicServer server;

        synchronized (classicServerLock)
        {
            server = currentServer;
            currentServer = null;
        }

        if (server == null)
            return;

        server.running = false;
        closeQuietly(server.socket);
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
        ClassicServer server = currentServer;
        return initialized && server != null && server.running;
    }


    // Android caps a discoverable request at an hour and has no indefinite
    // mode, so 0 or less is its own default.
    private static final int DISCOVERABLE_DEFAULT_SECONDS = 120;
    private static final int DISCOVERABLE_MAX_SECONDS = 3600;


    @Override
    public BluetoothError bluetooth_classic_discoverable_start(int duration_seconds)
    {
        BluetoothError ready = requireAdapter();
        if (ready != OK)
            return ready;

        if (!hasAdvertisePermission())
            return result(
                PERMISSION_DENIED,
                "Bluetooth advertise permission is not granted");

        if (!adapterEnabled())
            return result(
                BLUETOOTH_DISABLED,
                "Bluetooth is disabled");

        int requestedDuration = duration_seconds > 0
            ? Math.min(duration_seconds, DISCOVERABLE_MAX_SECONDS)
            : DISCOVERABLE_DEFAULT_SECONDS;

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
        if (!initialized)
            return result(NOT_INITIALIZED, NOT_INITIALIZED_MESSAGE);

        return result(
            NOT_SUPPORTED,
            "Android does not provide an API to cancel discoverability early; it expires on its own");
    }


    @Override
    public boolean bluetooth_classic_discoverable_is_running()
    {
        BluetoothAdapter current = adapter;

        if (!initialized || current == null)
            return false;

        // getScanMode needs BLUETOOTH_SCAN from Android 12.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S &&
            !hasPermission(Manifest.permission.BLUETOOTH_SCAN))
        {
            setLastError(PERMISSION_DENIED, "Bluetooth scan permission is not granted");
            return false;
        }

        try
        {
            return current.getScanMode() == BluetoothAdapter.SCAN_MODE_CONNECTABLE_DISCOVERABLE;
        }
        catch (Throwable ignored)
        {
            return false;
        }
    }


    // =========================================================================
    // Callback registration
    // =========================================================================

    @Override
    public boolean bluetooth_set_callback_state_changed(GMFunction callback)
    {
        if (initialized && adapter != null)
            ensureStateReceiver();

        // Swapped under the state lock, so no broadcast reaches the new
        // callback ahead of its first answer.
        synchronized (stateLock)
        {
            callbackStateChanged = callback;
            dispatchState();
        }

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
