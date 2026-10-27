package ${YYAndroidPackageName};

import ${YYAndroidPackageName}.GMExtWire.GMFunction;
import ${YYAndroidPackageName}.enums.BluetoothAttError;
import ${YYAndroidPackageName}.enums.BluetoothError;
import ${YYAndroidPackageName}.enums.BluetoothFeature;
import ${YYAndroidPackageName}.enums.BluetoothLeAdvertiseTxPower;
import ${YYAndroidPackageName}.enums.BluetoothLeConnectionPriority;
import ${YYAndroidPackageName}.enums.BluetoothLeScanMode;
import ${YYAndroidPackageName}.enums.BluetoothLeSubscribeMode;
import ${YYAndroidPackageName}.enums.BluetoothLeWriteType;
import ${YYAndroidPackageName}.enums.BluetoothPermissionStatus;
import ${YYAndroidPackageName}.enums.BluetoothTransport;
import ${YYAndroidPackageName}.records.BluetoothConnectOptions;
import ${YYAndroidPackageName}.records.BluetoothLeAdvertiseData;
import ${YYAndroidPackageName}.records.BluetoothLeAdvertiseManufacturerData;
import ${YYAndroidPackageName}.records.BluetoothLeAdvertiseServiceData;
import ${YYAndroidPackageName}.records.BluetoothLeAdvertiseSettings;
import ${YYAndroidPackageName}.records.BluetoothLeAdvertisement;
import ${YYAndroidPackageName}.records.BluetoothLeCharacteristicDefinition;
import ${YYAndroidPackageName}.records.BluetoothLeDescriptorDefinition;
import ${YYAndroidPackageName}.records.BluetoothLeDiscoverOptions;
import ${YYAndroidPackageName}.records.BluetoothLeScanFilter;
import ${YYAndroidPackageName}.records.BluetoothLeScanSettings;
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
import android.os.SystemClock;
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
import java.util.concurrent.atomic.AtomicReference;


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
    private static final long HANDLE_TYPE_LE_SERVER_SERVICE = 0x07L;
    private static final long HANDLE_TYPE_LE_SERVER_CHARACTERISTIC = 0x08L;
    private static final long HANDLE_TYPE_LE_SERVER_DESCRIPTOR = 0x09L;

    private static final int REQUEST_CODE_BLUETOOTH = 0xB710;
    private static final int REQUEST_CODE_ENABLE = 0xB711;
    private static final int REQUEST_CODE_DISCOVERABLE = 0xB712;

    // BluetoothLeSubscribeMode (spec.gmidl), as the subscriber bookkeeping
    // stores it, 0 for none.
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

    // One scan's callbacks and what it has announced. Whoever takes the
    // session out of its reference ends the scan, so it ends once; a result
    // or broadcast for a session that is no longer current is dropped.
    private static final class ScanSession
    {
        final int transport;
        final GMFunction onFound;
        final GMFunction onUpdated;
        final GMFunction onStopped;
        final long updateIntervalMs;
        final List<BluetoothLeScanFilter> filters;

        // Under deviceLock: when each device was last announced, and the keys
        // of the devices that have matched a filter during this scan.
        final HashMap<Long, Long> announcedAt = new HashMap<>();
        final HashSet<String> matched = new HashSet<>();

        // LE only: what stopScan needs.
        BluetoothLeScanner scanner;
        ScanCallback callback;

        ScanSession(
            int transport,
            GMFunction onFound,
            GMFunction onUpdated,
            GMFunction onStopped,
            long updateIntervalMs,
            List<BluetoothLeScanFilter> filters)
        {
            this.transport = transport;
            this.onFound = onFound;
            this.onUpdated = onUpdated;
            this.onStopped = onStopped;
            this.updateIntervalMs = updateIntervalMs;
            this.filters = filters != null ? new ArrayList<>(filters) : new ArrayList<>();
        }
    }

    private final AtomicReference<ScanSession> leScan = new AtomicReference<>(null);
    private final AtomicReference<ScanSession> classicScan = new AtomicReference<>(null);

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
    private static final String RADIO_OFF_MESSAGE = "Bluetooth was turned off";
    private static final String REMOTE_DISCONNECTED_MESSAGE = "Remote device disconnected";

    // The permission, enable and discoverable requests need one to launch from.
    private static final String NO_ACTIVITY_MESSAGE = "No foreground activity is available";


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
        String key = "";
        String id = "";
        String name = "";
        String address = "";
        boolean addressAvailable = false;
        int rssi = 0;
        boolean rssiAvailable = false;
        boolean connectable = false;
        BluetoothDevice androidDevice = null;

        // SystemClock.elapsedRealtime() of the last scan sighting, -1 when no
        // scan has seen it.
        long lastSeenMs = -1;

        // What the device has advertised, merged across packets as the native
        // core merges it (R1-138). UUIDs canonical lowercase; guarded by
        // deviceLock like the rest of the entry.
        final ArrayList<String> advServiceUuids = new ArrayList<>();
        final LinkedHashMap<String, byte[]> advServiceData = new LinkedHashMap<>();
        final LinkedHashMap<Integer, byte[]> advManufacturerData = new LinkedHashMap<>();
        Integer advTxPower = null;
        String advLocalName = "";
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

    private static final int MAX_PENDING_SCAN_DEVICES = 256;

    // The devices a filtered LE scan has heard that match no filter yet, by
    // key, oldest first, each one's packets merged as the core's
    // ScanFilterState merges them. Under deviceLock.
    private final LinkedHashMap<String, DeviceEntry> pendingScanDevices = new LinkedHashMap<>();


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

                int adapterState = intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, -1);

                // Before state_changed, so the game hears each subject end
                // first, as on every platform.
                if (adapterState == BluetoothAdapter.STATE_TURNING_OFF ||
                    adapterState == BluetoothAdapter.STATE_OFF)
                    endEverythingForRadioOff();

                dispatchState();
            }
            catch (Throwable ignored)
            {
            }
        }
    };


    // Radio off ends everything running, each reporting BluetoothDisabled,
    // and nothing restarts when it comes back. The stack drops scans,
    // advertising and links without telling each owner, so every subject is
    // ended here; taking it out of its record first makes it report once,
    // and a late stack event finds nothing. TURNING_OFF and OFF both arrive,
    // so the second finds nothing left.
    private void endEverythingForRadioOff()
    {
        ScanSession le = leScan.getAndSet(null);

        if (le != null)
        {
            stopLeScanner(le);
            invoke(le.onStopped, BLUETOOTH_DISABLED, RADIO_OFF_MESSAGE);
        }

        ScanSession classic = classicScan.getAndSet(null);

        if (classic != null)
        {
            cancelDiscoveryQuietly();
            invoke(classic.onStopped, BLUETOOTH_DISABLED, RADIO_OFF_MESSAGE);
        }

        // A start still waiting hears why; a running advertisement reports
        // its end.
        GMFunction advertiseStopped = stopLeAdvertiseInternal(BLUETOOTH_DISABLED, RADIO_OFF_MESSAGE);
        invoke(advertiseStopped, BLUETOOTH_DISABLED, RADIO_OFF_MESSAGE);

        endLeClientConnectionsForRadioOff();

        LeServerSession server = stopLeServerInternal(BLUETOOTH_DISABLED, RADIO_OFF_MESSAGE, true);

        if (server != null)
            invoke(server.onStopped, BLUETOOTH_DISABLED, RADIO_OFF_MESSAGE);

        endClassicConnectionsForRadioOff();

        ClassicServer classicServer = takeClassicServer();

        if (classicServer != null)
            invoke(classicServer.onStopped, BLUETOOTH_DISABLED, RADIO_OFF_MESSAGE);

        // A dialog still up is answered by its result.
        DiscoverableWindow window = discoverable.get();

        if (window != null && window.granted)
            endDiscoverableWindow(window, BLUETOOTH_DISABLED, RADIO_OFF_MESSAGE);
    }


    // Every client link: a connect in progress fails, an open link reports
    // on_disconnected, and its waiting ops fail first, as for any drop.
    private void endLeClientConnectionsForRadioOff()
    {
        ArrayList<LeConnectionEntry> clients = new ArrayList<>();

        synchronized (leConnectionLock)
        {
            for (LeConnectionEntry entry : leConnections.values())
            {
                if (!entry.serverRole)
                    clients.add(entry);
            }
        }

        for (LeConnectionEntry entry : clients)
        {
            boolean pending = entry.connectPending.getAndSet(false);
            boolean wasConnected = entry.connected;

            entry.manualClosing = true;
            entry.connected = false;

            if (pending)
                removeConnectTimeout(entry);

            eraseLeConnection(entry.handle);
            closeGatt(entry.gatt);

            if (pending)
            {
                GMFunction connectCallback = entry.connectCallback;
                entry.connectCallback = null;

                invoke(
                    connectCallback,
                    BLUETOOTH_DISABLED,
                    RADIO_OFF_MESSAGE,
                    (double) entry.handle,
                    (double) entry.device);
            }
            else if (wasConnected)
            {
                invoke(entry.onDisconnected, BLUETOOTH_DISABLED, RADIO_OFF_MESSAGE, (double) entry.handle);
            }
        }
    }


    // A connect in progress fails here; an open link's read loop reports the
    // reason set here once the closed socket ends it.
    private void endClassicConnectionsForRadioOff()
    {
        ArrayList<ConnectionEntry> open = new ArrayList<>();

        synchronized (connectionLock)
        {
            open.addAll(connections.values());
        }

        for (ConnectionEntry entry : open)
        {
            if (dispatchConnectResult(entry, BLUETOOTH_DISABLED, RADIO_OFF_MESSAGE))
            {
                entry.manualClosing = true;
                closeQuietly(entry.socket);
                continue;
            }

            if (entry.finished)
                continue;

            entry.endError = BLUETOOTH_DISABLED;
            entry.endMessage = RADIO_OFF_MESSAGE;
            closeQuietly(entry.socket);
            wakeReader(entry);
        }
    }


    private void cancelDiscoveryQuietly()
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
        volatile Runnable connectTimeout = null;

        // The game's callbacks for this link: its connect's, or for an
        // inbound link the server's.
        volatile GMFunction onData = null;
        volatile GMFunction onDisconnected = null;

        // Why the link was ended from outside the read loop (radio off);
        // null when the loop's own error stands.
        volatile BluetoothError endError = null;
        volatile String endMessage = "";

        // Guarded by receiveLock: the bytes of this connection's values not
        // yet fetched, released or expired. The read loop waits on it while
        // they reach MAX_QUEUED_RECEIVE_BYTES.
        final Object receiveLock = new Object();
        long receiveOutstanding = 0;

        // Set once, by the first of its values to expire: the connection is
        // ended rather than leave a gap in the stream.
        final AtomicBoolean receiveExpired = new AtomicBoolean(false);

        // Set when the read loop has ended.
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

    // How often a read loop paused at MAX_QUEUED_RECEIVE_BYTES runs the value
    // expiry, so an unfetched chunk ends its link at most this long after
    // VALUE_TTL_NANOS.
    private static final long RECEIVE_PAUSED_EXPIRY_CHECK_MILLIS = 5_000L;

    private static final String RECEIVE_EXPIRED_MESSAGE = "receive data expired";

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

        final GMFunction onConnected;
        final GMFunction onData;
        final GMFunction onDisconnected;
        final GMFunction onStopped;

        // Why the accept loop gave up, reported through on_stopped.
        volatile BluetoothError endError = OPERATION_FAILED;
        volatile String endMessage = "The Classic server stopped listening";

        ClassicServer(
            String name,
            UUID uuid,
            GMFunction onConnected,
            GMFunction onData,
            GMFunction onDisconnected,
            GMFunction onStopped)
        {
            this.name = name;
            this.uuid = uuid;
            this.onConnected = onConnected;
            this.onData = onData;
            this.onDisconnected = onDisconnected;
            this.onStopped = onStopped;
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
        long subscribeToken;
        boolean withoutResponse;
        GattOpStart start;
        String startFailureMessage = "";

        // A service discovery's options: how deep it goes, and the services
        // it goes deeper into (null for every one).
        boolean discoverCharacteristics;
        boolean discoverDescriptors;
        ArrayList<UUID> discoverServiceUuids;

        // Posted while the op is the current one; whoever takes the op
        // removes it.
        Runnable timeout;
    }

    private static final String LE_OP_DISCONNECTED_MESSAGE =
        "LE connection closed before the operation completed";
    private static final String SHUTDOWN_MESSAGE =
        "Bluetooth was shut down before the operation completed";

    // BluetoothConnectOptions.timeout_ms: the whole connect window, pairing
    // prompt included.
    private static final int CONNECT_TIMEOUT_DEFAULT_MS = 15_000;
    private static final int CONNECT_TIMEOUT_MIN_MS = 1_000;
    private static final int CONNECT_TIMEOUT_MAX_MS = 300_000;

    // The ATT transaction timeout: an op the stack has not completed by then
    // has stalled the link, which takes no other op until it does.
    private static final long GATT_OP_TIMEOUT_MS = 30_000;

    private static final int MAX_PENDING_WRITES_WITHOUT_RESPONSE = 64;

    private static final class LeConnectionEntry
    {
        long handle;
        long device;
        boolean serverRole;

        volatile BluetoothGatt gatt = null;
        volatile BluetoothDevice remoteDevice = null;
        volatile boolean connected = false;
        volatile boolean manualClosing = false;

        // The ATT MTU: the default until onMtuChanged (the client's or, for a
        // server connection, the server's) reports another.
        volatile int mtu = 23;

        // A client connect in progress, reported once by whoever clears
        // connectPending first: the stack, the connect window, a cancelling
        // disconnect or shutdown.
        final AtomicBoolean connectPending = new AtomicBoolean(false);
        volatile GMFunction connectCallback = null;
        volatile Runnable connectTimeout = null;

        // The game's: its connect's for a client link, the server's for a
        // central.
        volatile GMFunction onDisconnected = null;

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

        // What the peripheral last confirmed for its CCCD.
        volatile int subscribeMode = SUBSCRIBE_MODE_UNSUBSCRIBE;

        // Under leEntityLock. subscribed is the game's side, set by subscribe
        // and cleared by unsubscribe or a failed subscribe; the token tells a
        // late subscribe completion it was overtaken. onValue receives the
        // subscription's values, null once it has ended.
        boolean subscribed = false;
        long subscribeToken = 0;
        GMFunction onValue = null;

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
        boolean isWrite;
        boolean responseNeeded;

        // A write's bytes: GML's copy is the value, freed with the request;
        // this one is echoed back by sendResponse.
        long value = 0;
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
    private static final int ATT_INVALID_OFFSET = 0x07;
    private static final int ATT_ATTRIBUTE_NOT_FOUND = 0x0A;
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

    // The GATT server's own attributes, also guarded by leEntityLock. Minted
    // by add_service in definition order, child lists filled there, and kept
    // until its add fails, clear_services, stop or shutdown.
    private final HashMap<Long, LeServiceEntry> leServerServices = new HashMap<>();
    private final HashMap<Long, LeCharacteristicEntry> leServerCharacteristics = new HashMap<>();
    private final HashMap<Long, LeDescriptorEntry> leServerDescriptors = new HashMap<>();
    private final IdentityHashMap<BluetoothGattCharacteristic, Long> leServerCharacteristicHandleByObject =
        new IdentityHashMap<>();
    private final IdentityHashMap<BluetoothGattDescriptor, Long> leServerDescriptorHandleByObject =
        new IdentityHashMap<>();
    private long nextLeServerAttributeId = 1;

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
    private GMFunction leAdvertiseOnStopped = null;

    // The running GATT server's callbacks; null while it is stopped. Its
    // server callback holds the session it was opened with.
    private static final class LeServerSession
    {
        final GMFunction onConnected;
        final GMFunction onDisconnected;
        final GMFunction onRead;
        final GMFunction onWrite;
        final GMFunction onStopped;

        LeServerSession(
            GMFunction onConnected,
            GMFunction onDisconnected,
            GMFunction onRead,
            GMFunction onWrite,
            GMFunction onStopped)
        {
            this.onConnected = onConnected;
            this.onDisconnected = onDisconnected;
            this.onRead = onRead;
            this.onWrite = onWrite;
            this.onStopped = onStopped;
        }
    }

    private volatile BluetoothGattServer gattServer = null;
    private final AtomicReference<LeServerSession> leServerSession = new AtomicReference<>(null);
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
        long serviceHandle;
        GMFunction callback;
        boolean inFlight = false;

        // Its characteristics' initial values and its descriptors' static
        // ones, served only once the stack has added the service: a failed or
        // cancelled add takes them with it.
        IdentityHashMap<BluetoothGattCharacteristic, byte[]> initialValues;
        IdentityHashMap<BluetoothGattDescriptor, byte[]> descriptorValues;

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
    private final IdentityHashMap<BluetoothGattDescriptor, byte[]> leServerDescriptorValues =
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
        LeServerNotifyCall call;
    }

    // One bluetooth_le_server_notify_value call: its notifications still
    // queued or in flight, one per subscribed central.
    private static final class LeServerNotifyCall
    {
        int remaining;
    }

    // Counted in calls, as Windows and Apple count them, not in centrals.
    private static final int MAX_QUEUED_NOTIFICATIONS = 64;

    // Per device address, the notification the stack holds and the ones
    // waiting behind it, in order. An address with an in-flight entry has its
    // sending turn taken; onNotificationSent hands it on. The count is of the
    // calls with a notification in any queue or in flight, kept by
    // releaseLeServerNotification. Guarded by leServerNotifyLock.
    private final Object leServerNotifyLock = new Object();
    private final HashMap<String, ArrayDeque<LeServerNotification>> leServerNotifyQueues = new HashMap<>();
    private final HashMap<String, LeServerNotification> leServerNotifyInFlight = new HashMap<>();
    private int leServerNotifyCount = 0;

    // Bumped when the server stops, so a callback of a closed server, which
    // the stack may still deliver, is not taken for the next server's.
    private final AtomicLong leServerGeneration = new AtomicLong(1);


    // =========================================================================
    // Values
    // =========================================================================

    // Bytes a callback delivered, held for GML under their own id until
    // bluetooth_value_fetch or _release frees them. Values outlive their link,
    // so one delivered just before a disconnect can still be fetched. Each
    // expires VALUE_TTL_NANOS after it was stored; the cap drops the oldest LE
    // read or notification value, a server write's goes with its request, a
    // Classic one counts toward its connection's receive limit, and shutdown
    // frees the rest.
    private static final int VALUE_LE = 0;
    private static final int VALUE_SERVER_WRITE = 1;
    private static final int VALUE_CLASSIC = 2;

    private static final class StoredValue
    {
        int owner;
        byte[] bytes;
        long storedAtNanos;

        // The Classic connection whose receive limit the bytes count toward.
        ConnectionEntry connection;
    }

    private static final int LE_VALUE_LIMIT = 256;
    private static final long VALUE_TTL_NANOS = 30_000_000_000L;

    // values is in the order stored, so the oldest come first.
    private final Object valueLock = new Object();
    private long nextValueId = 1;
    private final LinkedHashMap<Long, StoredValue> values = new LinkedHashMap<>();
    private int leValueCount = 0;


    // =========================================================================
    // GML callbacks
    // =========================================================================

    // The one global slot: every other event belongs to the call that
    // started its subject.
    private volatile GMFunction callbackStateChanged = null;


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
    // UUID.fromString alone takes only the 36-character form. Anything
    // isValidUuid refuses, surrounding spaces included, throws
    // IllegalArgumentException, which every caller answers as InvalidArgument.
    private static UUID parseUuid(String text)
    {
        if (!isValidUuid(text))
            throw new IllegalArgumentException("Invalid UUID: " + text);

        String value = text;

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
            copy.key = source.key;
            copy.id = source.id;
            copy.lastSeenMs = source.lastSeenMs;
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


    // A device that was not discovered - a central on the GATT server, a
    // queried or paired device, an inbound RFCOMM peer, one from an id -
    // enters the cache without being announced or marked seen.
    private long upsertDevice(
        int transport,
        BluetoothDevice androidDevice,
        int rssi,
        boolean hasRssi,
        boolean connectable)
    {
        return upsertDevice(
            transport,
            deviceKey(transport, androidDevice),
            deviceId(transport, androidDevice),
            safeName(androidDevice),
            safeAddress(androidDevice),
            rssi,
            hasRssi,
            connectable,
            androidDevice,
            null,
            null,
            null);
    }


    // Adds a device or merges what is new about it, as the native core does:
    // the transport is the first sighting's, a name only when it is not
    // empty, an RSSI only when one was measured, and connectable once seen
    // stays so. A typed LE id replaces an untyped one, never the reverse.
    // scan is the session the sighting came from, null when no scan saw it:
    // it marks the device seen and is the only one told, on_found the first
    // time this scan sees the device, cached before or not, and on_updated
    // after that, at most once per its interval counted from the last
    // announcement.
    // A scan result also folds its advertisement in, under the same lock and
    // before the announcement, so the game never reads a found device's
    // record without its first packet. A device a filtered scan held back
    // brings every packet it was held for in merged, which already holds
    // record.
    private long upsertDevice(
        int transport,
        String key,
        String id,
        String name,
        String address,
        int rssi,
        boolean hasRssi,
        boolean connectable,
        BluetoothDevice androidDevice,
        ScanSession scan,
        ScanRecord record,
        DeviceEntry merged)
    {
        String safeKey = key != null ? key : "";
        String safeId = id != null ? id : "";
        String safeName = name != null ? name : "";
        String safeAddress = address != null ? address : "";

        boolean created = false;
        long handle;
        GMFunction announce = null;

        synchronized (deviceLock)
        {
            Long existing = deviceById.get(safeKey);
            DeviceEntry entry;

            if (existing == null)
            {
                handle = makeHandle(HANDLE_TYPE_DEVICE, nextDeviceId++);

                entry = new DeviceEntry();
                entry.handle = handle;
                entry.key = safeKey;
                entry.id = safeId;
                entry.transport = transport;

                devices.put(handle, entry);
                deviceById.put(safeKey, handle);
                deviceOrder.add(handle);
                created = true;
            }
            else
            {
                handle = existing;
                entry = devices.get(handle);
                if (entry == null)
                    return 0;

                if (!safeId.isEmpty() && (isTypedLeId(safeId) || !isTypedLeId(entry.id)))
                    entry.id = safeId;
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

            if (merged != null)
                foldAdvertisement(entry, merged);
            else if (record != null)
                mergeAdvertisement(entry, record);

            if (scan != null)
            {
                long now = SystemClock.elapsedRealtime();
                entry.lastSeenMs = now;

                Long last = scan.announcedAt.get(handle);

                if (last == null)
                {
                    announce = scan.onFound;
                    scan.announcedAt.put(handle, now);
                }
                else if (scan.onUpdated != null && now - last >= scan.updateIntervalMs)
                {
                    announce = scan.onUpdated;
                    scan.announcedAt.put(handle, now);
                }
            }
        }

        if (announce != null && scanRunning(scan))
            invoke(announce, (double) handle);

        return handle;
    }


    // Whether the session is still its transport's running scan.
    private boolean scanRunning(ScanSession scan)
    {
        if (scan == null)
            return false;

        return (scan.transport == TRANSPORT_LE ? leScan.get() : classicScan.get()) == scan;
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

        String localName = record.getDeviceName();

        if (localName != null && !localName.isEmpty())
            entry.advLocalName = localName;
    }


    // mergeAdvertisement for an already merged record. Under deviceLock.
    private static void foldAdvertisement(DeviceEntry entry, DeviceEntry from)
    {
        for (String uuid : from.advServiceUuids)
        {
            if (!entry.advServiceUuids.contains(uuid) &&
                entry.advServiceUuids.size() < MAX_ADVERTISEMENT_ENTRIES)
                entry.advServiceUuids.add(uuid);
        }

        for (Map.Entry<String, byte[]> item : from.advServiceData.entrySet())
        {
            if (entry.advServiceData.containsKey(item.getKey()) ||
                entry.advServiceData.size() < MAX_ADVERTISEMENT_ENTRIES)
                entry.advServiceData.put(item.getKey(), item.getValue());
        }

        for (Map.Entry<Integer, byte[]> item : from.advManufacturerData.entrySet())
        {
            if (entry.advManufacturerData.containsKey(item.getKey()) ||
                entry.advManufacturerData.size() < MAX_ADVERTISEMENT_ENTRIES)
                entry.advManufacturerData.put(item.getKey(), item.getValue());
        }

        if (from.advTxPower != null)
            entry.advTxPower = from.advTxPower;

        if (!from.advLocalName.isEmpty())
            entry.advLocalName = from.advLocalName;
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


    private static final String ID_PREFIX_CLASSIC = "classic:";
    private static final String ID_PREFIX_LE = "le:";
    private static final String ID_PREFIX_LE_PUBLIC = "le:public:";
    private static final String ID_PREFIX_LE_RANDOM = "le:random:";
    private static final String ID_PREFIX_LE_APPLE = "le:apple:";

    // BluetoothDevice.ADDRESS_TYPE_PUBLIC and _RANDOM.
    private static final int LE_ADDRESS_PUBLIC = 0;
    private static final int LE_ADDRESS_RANDOM = 1;


    private static boolean isTypedLeId(String id)
    {
        return id != null && (id.startsWith(ID_PREFIX_LE_PUBLIC) || id.startsWith(ID_PREFIX_LE_RANDOM));
    }


    // The cache key: one entry per transport and address, whatever address
    // type a later sighting reports. A device with no address has no id, so
    // its object keys it.
    private static String deviceKey(int transport, String address, BluetoothDevice device)
    {
        String prefix = transport == TRANSPORT_LE ? ID_PREFIX_LE : ID_PREFIX_CLASSIC;

        if (address != null && !address.isEmpty())
            return prefix + address.toUpperCase(java.util.Locale.ROOT);

        return prefix + "object:" + System.identityHashCode(device);
    }


    private static String deviceKey(int transport, BluetoothDevice device)
    {
        return deviceKey(transport, safeAddress(device), device);
    }


    // "classic:AA:BB:...", "le:public:..." or "le:random:..." when the stack
    // says the type (API 35), "le:..." when it does not; empty with no address.
    private static String deviceId(int transport, BluetoothDevice device)
    {
        String address = safeAddress(device).toUpperCase(java.util.Locale.ROOT);

        if (address.isEmpty())
            return "";

        if (transport != TRANSPORT_LE)
            return ID_PREFIX_CLASSIC + address;

        switch (leAddressType(device))
        {
            case LE_ADDRESS_PUBLIC:
                return ID_PREFIX_LE_PUBLIC + address;
            case LE_ADDRESS_RANDOM:
                return ID_PREFIX_LE_RANDOM + address;
            default:
                return ID_PREFIX_LE + address;
        }
    }


    // BluetoothDevice.getAddressType (API 35), called by name so an older
    // compile SDK still builds; -1 when it is not known.
    private static int leAddressType(BluetoothDevice device)
    {
        if (device == null || Build.VERSION.SDK_INT < 35)
            return -1;

        try
        {
            Object type = BluetoothDevice.class.getMethod("getAddressType").invoke(device);
            return type instanceof Integer ? (Integer) type : -1;
        }
        catch (Throwable ignored)
        {
            return -1;
        }
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


    // Reports a client connect, once: false when a cancelling disconnect, the
    // connect window, radio off or shutdown reported it already, and the
    // caller drops the socket.
    private boolean dispatchConnectResult(
        ConnectionEntry entry,
        BluetoothError error,
        String message)
    {
        if (!entry.connectPending.getAndSet(false))
            return false;

        Runnable timeout = entry.connectTimeout;

        if (timeout != null)
            timeoutHandler.removeCallbacks(timeout);

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
            entry.onDisconnected,
            error,
            safeMessage,
            (double) entry.handle);

        // Every byte received is already a value of its own, which outlives
        // the entry.
        entry.finished = true;
        entry.connected = false;

        wakeWriter(entry);
        eraseConnection(entry.handle);
    }


    private static void wakeWriter(ConnectionEntry entry)
    {
        synchronized (entry.sendLock)
        {
            entry.sendLock.notifyAll();
        }
    }


    // The read loop waits on receiveLock while the connection's values hold
    // too many bytes.
    private static void wakeReader(ConnectionEntry entry)
    {
        synchronized (entry.receiveLock)
        {
            entry.receiveLock.notifyAll();
        }
    }


    // A Classic value has left the store: its bytes no longer hold the read
    // loop back.
    private static void releaseReceived(ConnectionEntry entry, int size)
    {
        synchronized (entry.receiveLock)
        {
            entry.receiveOutstanding -= size;
            entry.receiveLock.notifyAll();
        }
    }


    // on_data fires for every chunk, each its own value.
    private void dispatchClassicData(ConnectionEntry entry, byte[] data)
    {
        GMFunction callback = entry.onData;

        // No handler, no one to free the value; and nobody reads a link
        // bluetooth_classic_disconnect retired.
        if (callback == null || data == null || data.length == 0 ||
            getConnection(entry.handle) != entry)
            return;

        synchronized (entry.receiveLock)
        {
            entry.receiveOutstanding += data.length;
        }

        long value = storeValue(VALUE_CLASSIC, data, entry);

        invoke(
            callback,
            (double) entry.handle,
            (double) value,
            data.length);
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


    // The getters walk a client service or a server one alike.
    private LeServiceEntry getAnyLeService(long handle)
    {
        synchronized (leEntityLock)
        {
            if (isHandleType(handle, HANDLE_TYPE_LE_SERVICE))
                return leServices.get(handle);

            if (isHandleType(handle, HANDLE_TYPE_LE_SERVER_SERVICE))
                return leServerServices.get(handle);

            return null;
        }
    }


    private LeCharacteristicEntry getAnyLeCharacteristic(long handle)
    {
        synchronized (leEntityLock)
        {
            if (isHandleType(handle, HANDLE_TYPE_LE_CHARACTERISTIC))
                return leCharacteristics.get(handle);

            if (isHandleType(handle, HANDLE_TYPE_LE_SERVER_CHARACTERISTIC))
                return leServerCharacteristics.get(handle);

            return null;
        }
    }


    private LeDescriptorEntry getAnyLeDescriptor(long handle)
    {
        synchronized (leEntityLock)
        {
            if (isHandleType(handle, HANDLE_TYPE_LE_DESCRIPTOR))
                return leDescriptors.get(handle);

            if (isHandleType(handle, HANDLE_TYPE_LE_SERVER_DESCRIPTOR))
                return leServerDescriptors.get(handle);

            return null;
        }
    }


    // A client call given a handle it cannot use. A server attribute is this
    // app's own, and no client call reaches it.
    private BluetoothError invalidAttribute(long handle, String message)
    {
        boolean server =
            isHandleType(handle, HANDLE_TYPE_LE_SERVER_SERVICE) ||
            isHandleType(handle, HANDLE_TYPE_LE_SERVER_CHARACTERISTIC) ||
            isHandleType(handle, HANDLE_TYPE_LE_SERVER_DESCRIPTOR);

        return result(
            INVALID_HANDLE,
            server ? "A GATT server attribute handle cannot be used for a client operation" : message);
    }


    // Handles for a service being added, in definition order. The CCCD is
    // the extension's own, whether the definition lists it or it was
    // appended, and gets none. Returns the service handle.
    private long createLeServerHandles(BluetoothGattService gattService)
    {
        synchronized (leEntityLock)
        {
            LeServiceEntry service = new LeServiceEntry();
            service.handle = makeHandle(HANDLE_TYPE_LE_SERVER_SERVICE, nextLeServerAttributeId++);
            service.gattService = gattService;
            service.uuid = gattService.getUuid().toString();
            leServerServices.put(service.handle, service);

            for (BluetoothGattCharacteristic gattCharacteristic : gattService.getCharacteristics())
            {
                LeCharacteristicEntry characteristic = new LeCharacteristicEntry();
                characteristic.handle = makeHandle(
                    HANDLE_TYPE_LE_SERVER_CHARACTERISTIC,
                    nextLeServerAttributeId++);
                characteristic.service = service.handle;
                characteristic.gattCharacteristic = gattCharacteristic;
                characteristic.uuid = gattCharacteristic.getUuid().toString();
                leServerCharacteristics.put(characteristic.handle, characteristic);
                leServerCharacteristicHandleByObject.put(gattCharacteristic, characteristic.handle);
                service.characteristicHandles.add(characteristic.handle);

                for (BluetoothGattDescriptor gattDescriptor : gattCharacteristic.getDescriptors())
                {
                    if (CCCD_UUID.equals(gattDescriptor.getUuid()))
                        continue;

                    LeDescriptorEntry descriptor = new LeDescriptorEntry();
                    descriptor.handle = makeHandle(
                        HANDLE_TYPE_LE_SERVER_DESCRIPTOR,
                        nextLeServerAttributeId++);
                    descriptor.characteristic = characteristic.handle;
                    descriptor.gattDescriptor = gattDescriptor;
                    descriptor.uuid = gattDescriptor.getUuid().toString();
                    leServerDescriptors.put(descriptor.handle, descriptor);
                    leServerDescriptorHandleByObject.put(gattDescriptor, descriptor.handle);
                    characteristic.descriptorHandles.add(descriptor.handle);
                }
            }

            return service.handle;
        }
    }


    // A service whose add failed or was cancelled takes its handles with it.
    private void eraseLeServerHandles(long serviceHandle)
    {
        synchronized (leEntityLock)
        {
            LeServiceEntry service = leServerServices.remove(serviceHandle);
            if (service == null)
                return;

            for (long characteristicHandle : service.characteristicHandles)
            {
                LeCharacteristicEntry characteristic = leServerCharacteristics.remove(characteristicHandle);
                if (characteristic == null)
                    continue;

                leServerCharacteristicHandleByObject.remove(characteristic.gattCharacteristic);

                for (long descriptorHandle : characteristic.descriptorHandles)
                {
                    LeDescriptorEntry descriptor = leServerDescriptors.remove(descriptorHandle);
                    if (descriptor != null)
                        leServerDescriptorHandleByObject.remove(descriptor.gattDescriptor);
                }
            }
        }
    }


    private void clearLeServerHandles()
    {
        synchronized (leEntityLock)
        {
            leServerServices.clear();
            leServerCharacteristics.clear();
            leServerDescriptors.clear();
            leServerCharacteristicHandleByObject.clear();
            leServerDescriptorHandleByObject.clear();
        }
    }


    // The handles a server request names: its characteristic's, and its
    // descriptor's or 0. Null for an attribute that has none - one of a
    // service no longer added.
    private long[] serverRequestHandles(
        BluetoothGattCharacteristic characteristic,
        BluetoothGattDescriptor descriptor)
    {
        synchronized (leEntityLock)
        {
            if (descriptor != null)
            {
                Long handle = leServerDescriptorHandleByObject.get(descriptor);
                LeDescriptorEntry entry = handle != null ? leServerDescriptors.get(handle) : null;
                return entry != null ? new long[] { entry.characteristic, entry.handle } : null;
            }

            Long handle = characteristic != null
                ? leServerCharacteristicHandleByObject.get(characteristic)
                : null;
            return handle != null ? new long[] { handle, 0 } : null;
        }
    }


    // Copies a value in the GATT callback - the stack and the write path both
    // reuse the shared characteristic object - and holds it for GML. Returns
    // its id.
    private long storeLeValue(byte[] value)
    {
        return storeValue(VALUE_LE, value != null ? value.clone() : new byte[0], null);
    }


    // Holds bytes the caller owns no longer. Returns the value's id.
    private long storeValue(int owner, byte[] bytes, ConnectionEntry connection)
    {
        StoredValue value = new StoredValue();
        value.owner = owner;
        value.bytes = bytes;
        value.connection = connection;

        ArrayList<StoredValue> expired;
        long id;

        synchronized (valueLock)
        {
            expired = expireValues();

            id = nextValueId++;
            value.storedAtNanos = System.nanoTime();
            values.put(id, value);

            if (owner == VALUE_LE && ++leValueCount > LE_VALUE_LIMIT)
            {
                Iterator<StoredValue> iterator = values.values().iterator();

                while (iterator.hasNext())
                {
                    if (iterator.next().owner == VALUE_LE)
                    {
                        iterator.remove();
                        leValueCount--;
                        break;
                    }
                }
            }
        }

        endExpiredReceives(expired);
        return id;
    }


    // Caller holds valueLock.
    private StoredValue removeValue(long id)
    {
        StoredValue value = values.remove(id);

        if (value != null && value.owner == VALUE_LE)
            leValueCount--;

        return value;
    }


    // Frees a value the extension owns, such as a server write's with its
    // request. Unknown ids are ignored: the game may have freed it already.
    private void discardValue(long id)
    {
        StoredValue value;

        synchronized (valueLock)
        {
            value = removeValue(id);
        }

        if (value != null && value.connection != null)
            releaseReceived(value.connection, value.bytes.length);
    }


    // Drops the values past their lifetime without storing one.
    private void expireStoredValues()
    {
        ArrayList<StoredValue> expired;

        synchronized (valueLock)
        {
            expired = expireValues();
        }

        endExpiredReceives(expired);
    }


    // Caller holds valueLock. Drops the values past their lifetime and
    // returns the Classic ones among them, which endExpiredReceives settles
    // once the lock is released; null when there are none.
    private ArrayList<StoredValue> expireValues()
    {
        ArrayList<StoredValue> expired = null;
        long now = System.nanoTime();
        Iterator<StoredValue> iterator = values.values().iterator();

        while (iterator.hasNext())
        {
            StoredValue value = iterator.next();

            if (now - value.storedAtNanos <= VALUE_TTL_NANOS)
                break;

            iterator.remove();

            if (value.owner == VALUE_LE)
                leValueCount--;

            if (value.connection != null)
            {
                if (expired == null)
                    expired = new ArrayList<>();

                expired.add(value);
            }
        }

        return expired;
    }


    // An expired Classic value would leave a gap in its stream, so the first
    // one ends its connection: the read loop reports it through
    // on_disconnected, after the last on_data.
    private void endExpiredReceives(ArrayList<StoredValue> expired)
    {
        if (expired == null)
            return;

        for (StoredValue value : expired)
        {
            ConnectionEntry entry = value.connection;

            if (!entry.finished && !entry.manualClosing && entry.receiveExpired.compareAndSet(false, true))
                closeQuietly(entry.socket);

            releaseReceived(entry, value.bytes.length);
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


    // A failed subscribe ends the game's subscription, unless an unsubscribe
    // or a newer subscribe overtook it.
    private void endFailedSubscribe(LePendingOp op)
    {
        if (op.kind != LePendingOp.KIND_SUBSCRIBE || op.subscribeMode == SUBSCRIBE_MODE_UNSUBSCRIBE)
            return;

        synchronized (leEntityLock)
        {
            LeCharacteristicEntry entry = leCharacteristics.get(op.targetHandle);

            if (entry != null && entry.subscribeToken == op.subscribeToken)
            {
                entry.subscribed = false;
                entry.onValue = null;
            }
        }
    }


    private void failOp(LePendingOp op, BluetoothError error, String message)
    {
        endFailedSubscribe(op);

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


    private int pendingWritesWithoutResponse(LeConnectionEntry connection)
    {
        synchronized (connection.opLock)
        {
            int count = 0;

            if (connection.currentOp != null && connection.currentOp.withoutResponse)
                count++;

            for (LePendingOp op : connection.opQueue)
            {
                if (op.withoutResponse)
                    count++;
            }

            return count;
        }
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
            case BluetoothGatt.GATT_SUCCESS: return REMOTE_DISCONNECTED_MESSAGE;
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

    // A value for a characteristic whose subscription has ended, or never
    // began, reaches nobody, so it is not stored.
    private void dispatchLeCharacteristicValueChanged(
        long characteristic,
        long connection,
        byte[] value)
    {
        GMFunction onValue;

        synchronized (leEntityLock)
        {
            LeCharacteristicEntry entry = leCharacteristics.get(characteristic);
            onValue = entry != null ? entry.onValue : null;
        }

        if (onValue == null)
            return;

        int size = value != null ? value.length : 0;
        long id = storeLeValue(value);

        invoke(
            onValue,
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

        // Every event callback is dropped unfired.
        ScanSession le = leScan.getAndSet(null);

        if (le != null)
            stopLeScanner(le);

        if (classicScan.getAndSet(null) != null)
            cancelDiscoveryQuietly();

        synchronized (deviceLock)
        {
            pendingScanDevices.clear();
        }

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
        // A granted window goes without firing, like every other subject.
        DiscoverableWindow window = discoverable.getAndSet(null);
        if (window != null)
        {
            releaseDiscoverableWindow(window);
            if (!window.granted)
                invoke(window.callback, NOT_INITIALIZED, SHUTDOWN_MESSAGE, 0);
        }

        takeClassicServer();
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

        clearLeServerHandles();

        synchronized (leServerRequestLock)
        {
            leServerRequests.clear();
            leServerPreparedWrites.clear();
        }

        synchronized (leServerInitialValueLock)
        {
            leServerInitialValues.clear();
            leServerDescriptorValues.clear();
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

            dispatchConnectResult(entry, NOT_INITIALIZED, SHUTDOWN_MESSAGE);

            closeQuietly(entry.socket);
            wakeReader(entry);
            wakeWriter(entry);
        }

        // After the connections, so no read loop stores a chunk behind it.
        synchronized (valueLock)
        {
            values.clear();
            leValueCount = 0;
        }

        synchronized (deviceLock)
        {
            deviceById.clear();
            devices.clear();
            deviceOrder.clear();
        }

        // The state_changed callback and the last error stay: the callback
        // serves the next bluetooth_initialize, and the error is still the
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

    private boolean leSupported()
    {
        if (!initialized || adapter == null)
            return false;

        Context current = context();

        return current != null &&
            current.getPackageManager().hasSystemFeature(
                PackageManager.FEATURE_BLUETOOTH_LE);
    }


    private boolean leAdvertiseSupported()
    {
        if (!leSupported() || Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP)
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


    private boolean leServerSupported()
    {
        if (!leSupported() || Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP)
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


    private boolean classicSupported()
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
            // ScanSettings.setScanMode takes each BluetoothLeScanMode.
            case LeCentral:
            case LeScanMode:
                return leSupported();

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
                return leAdvertiseSupported();

            // Requests of descriptors without a static value reach GML (the
            // CCCD excepted), the signed write permissions pass to the stack,
            // and onConnectionStateChange reports each central.
            case LeServer:
            case LeServerDescriptorRequests:
            case LeServerSignedWrite:
            case LeServerConnectionEvents:
                return leServerSupported();

            // createBond() takes a device of either transport.
            case LePairing:
                return leSupported();

            case Classic:
            case ClassicServer:
            case ClassicPairing:
            case ClassicDiscoverable:
                return classicSupported();

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
                return leSupported();

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
            return result(OPERATION_FAILED, NO_ACTIVITY_MESSAGE);

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

    // One discoverable request at a time, from its dialog to the end of the
    // window it was granted; a start while one exists is Busy (R2-155).
    private static final class DiscoverableWindow
    {
        GMFunction callback;
        GMFunction onStopped;
        int requestedSeconds;
        volatile boolean granted;
        // The scan mode has been CONNECTABLE_DISCOVERABLE since the dialog.
        volatile boolean seenDiscoverable;
        Runnable expiry;
    }

    private final AtomicReference<DiscoverableWindow> discoverable = new AtomicReference<>();
    private boolean discoverableReceiverRegistered = false;


    // The window ends when the scan mode leaves CONNECTABLE_DISCOVERABLE, which
    // ColorOS does 48 ms after granting it. Registered when the dialog opens:
    // the stack applies the mode asynchronously, so the mode read at the
    // dialog's answer may not have arrived yet, or may already be gone.
    private final BroadcastReceiver discoverableReceiver = new BroadcastReceiver()
    {
        @Override
        public void onReceive(Context receiverContext, Intent intent)
        {
            try
            {
                DiscoverableWindow window = discoverable.get();

                if (!initialized || window == null || intent == null ||
                    !BluetoothAdapter.ACTION_SCAN_MODE_CHANGED.equals(intent.getAction()))
                    return;

                int mode = intent.getIntExtra(BluetoothAdapter.EXTRA_SCAN_MODE, -1);

                if (mode == BluetoothAdapter.SCAN_MODE_CONNECTABLE_DISCOVERABLE)
                    window.seenDiscoverable = true;
                else if (window.granted && window.seenDiscoverable)
                    endDiscoverableWindow(window);
            }
            catch (Throwable ignored)
            {
            }
        }
    };


    // Ends the window once: Ok, or BluetoothDisabled when the radio is not on.
    private void endDiscoverableWindow(DiscoverableWindow window)
    {
        if (!adapterEnabled())
            endDiscoverableWindow(window, BLUETOOTH_DISABLED, RADIO_OFF_MESSAGE);
        else
            endDiscoverableWindow(window, OK, "The system ended discoverability");
    }


    private void endDiscoverableWindow(DiscoverableWindow window, BluetoothError error, String message)
    {
        if (!discoverable.compareAndSet(window, null))
            return;

        releaseDiscoverableWindow(window);
        invoke(window.onStopped, error, message);
    }


    private void releaseDiscoverableWindow(DiscoverableWindow window)
    {
        if (window.expiry != null)
            timeoutHandler.removeCallbacks(window.expiry);

        synchronized (enableLock)
        {
            if (discoverableReceiverRegistered)
            {
                unregisterSystemReceiver(discoverableReceiver);
                discoverableReceiverRegistered = false;
            }
        }
    }


    // The dialog answered: the callback hears it, and a granted window ends
    // by the mode or, as a backstop and the only way without BLUETOOTH_SCAN,
    // by the clock.
    private void answerDiscoverableRequest(int resultCode)
    {
        DiscoverableWindow window = discoverable.get();

        if (window == null || window.granted)
            return;

        if (resultCode == Activity.RESULT_CANCELED)
        {
            if (discoverable.compareAndSet(window, null))
            {
                releaseDiscoverableWindow(window);
                invoke(window.callback, PERMISSION_DENIED, "The user declined", 0);
            }
            return;
        }

        // The granted duration comes back as the result code; a stack that
        // answers RESULT_OK instead granted what was asked.
        int seconds = resultCode > 0 ? resultCode : window.requestedSeconds;

        window.granted = true;
        window.expiry = () -> endDiscoverableWindow(window, OK, "The discoverable time ran out");
        invoke(window.callback, OK, "", seconds);
        timeoutHandler.postDelayed(window.expiry, seconds * 1000L);

        // Already withdrawn before the answer came.
        if (window.seenDiscoverable && !scanModeIsDiscoverable())
            endDiscoverableWindow(window);
    }


    private boolean scanModeIsDiscoverable()
    {
        BluetoothAdapter current = adapter;

        try
        {
            return current != null &&
                current.getScanMode() == BluetoothAdapter.SCAN_MODE_CONNECTABLE_DISCOVERABLE;
        }
        catch (Throwable ignored)
        {
            return false;
        }
    }


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
    // enable and discoverable dialogs' are ours.
    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data)
    {
        if (!initialized)
            return;

        if (requestCode == REQUEST_CODE_ENABLE)
        {
            if (resultCode == Activity.RESULT_OK)
                fireEnableCallbacks(OK, "");
            else
                fireEnableCallbacks(BLUETOOTH_DISABLED, "the user declined");
        }
        else if (requestCode == REQUEST_CODE_DISCOVERABLE)
        {
            answerDiscoverableRequest(resultCode);
        }
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
            return result(OPERATION_FAILED, NO_ACTIVITY_MESSAGE);

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

    // Each scan has its own callback, so a result the stack still delivers
    // for a stopped scan is told apart from the next scan's.
    private ScanCallback createScanCallback(final ScanSession session)
    {
        return new ScanCallback()
        {
            @Override
            public void onScanResult(int callbackType, ScanResult scanResult)
            {
                if (!initialized || scanResult == null || leScan.get() != session)
                    return;

                handleScanResult(session, scanResult);
            }


            @Override
            public void onScanFailed(int errorCode)
            {
                if (!initialized || !leScan.compareAndSet(session, null))
                    return;

                invoke(session.onStopped, OPERATION_FAILED, "Android BLE scan failed: " + errorCode);
            }
        };
    }


    private void handleScanResult(ScanSession session, ScanResult scanResult)
    {
        BluetoothDevice device = scanResult.getDevice();

        if (device == null)
            return;

        boolean connectable = true;

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
            connectable = scanResult.isConnectable();

        int rssi = scanResult.getRssi();
        String address = safeAddress(device);
        String key = deviceKey(TRANSPORT_LE, address, device);
        ScanRecord record = scanResult.getScanRecord();
        DeviceEntry admitted = null;

        synchronized (deviceLock)
        {
            if (!session.filters.isEmpty() && !session.matched.contains(key))
            {
                admitted = admitScanResult(session, key, record);
                if (admitted == null)
                    return;
            }
        }

        String name = safeName(device);

        if (name.isEmpty() && record != null && record.getDeviceName() != null)
            name = record.getDeviceName();

        upsertDevice(
            TRANSPORT_LE,
            key,
            deviceId(TRANSPORT_LE, device),
            name,
            address,
            rssi,
            rssi != 127,
            connectable,
            device,
            session,
            record,
            admitted);
    }


    private void stopLeScanner(ScanSession session)
    {
        try
        {
            if (session.scanner != null && session.callback != null && hasScanPermission())
                session.scanner.stopScan(session.callback);
        }
        catch (Throwable ignored)
        {
        }
    }


    private static boolean present(Optional<?> value)
    {
        return value != null && value.isPresent();
    }


    private static <T> T orNull(Optional<T> value)
    {
        return value != null && value.isPresent() ? value.get() : null;
    }


    // One ScanFilter per field a GML filter sets, so the stack reports every
    // advertisement that carries any filtered field and keeps the scan running
    // with the screen off. It sees an advertisement with its scan response
    // but never merges across advertisements, so admitScanResult matches the
    // whole filters. name_prefix has no ScanFilter field: a filter that sets
    // nothing else leaves the stack unfiltered (an empty list). Assumes the
    // filters passed checkScanFilters.
    private static List<ScanFilter> buildScanFilters(List<BluetoothLeScanFilter> filters)
    {
        ArrayList<ScanFilter> out = new ArrayList<>();

        for (BluetoothLeScanFilter filter : filters)
        {
            boolean narrowed = false;

            if (present(filter.service_uuid()))
            {
                narrowed = true;
                addScanFilter(out, new ScanFilter.Builder()
                    .setServiceUuid(new ParcelUuid(parseUuid(filter.service_uuid().get())))
                    .build());
            }

            if (present(filter.name()))
            {
                narrowed = true;
                addScanFilter(out, new ScanFilter.Builder()
                    .setDeviceName(filter.name().get())
                    .build());
            }

            // An empty data prefix matches any payload for the key.
            if (present(filter.company_id()))
            {
                narrowed = true;
                byte[] data = toBytes(orNull(filter.manufacturer_data()));
                byte[] mask = present(filter.manufacturer_data_mask())
                    ? toBytes(filter.manufacturer_data_mask().get())
                    : null;

                addScanFilter(out, new ScanFilter.Builder()
                    .setManufacturerData(filter.company_id().get(), data, mask)
                    .build());
            }

            if (present(filter.service_data_uuid()))
            {
                narrowed = true;
                byte[] data = toBytes(orNull(filter.service_data()));
                byte[] mask = present(filter.service_data_mask())
                    ? toBytes(filter.service_data_mask().get())
                    : null;

                addScanFilter(out, new ScanFilter.Builder()
                    .setServiceData(new ParcelUuid(parseUuid(filter.service_data_uuid().get())), data, mask)
                    .build());
            }

            if (!narrowed)
                return new ArrayList<>();
        }

        return out;
    }


    private static void addScanFilter(List<ScanFilter> filters, ScanFilter filter)
    {
        if (!filters.contains(filter))
            filters.add(filter);
    }


    // The core's ScanFilterState.admit, per scan: folds the result into the
    // device's held record - seeded from the cache for a device an earlier
    // scan cached - and hands that record back, removed, once it matches one
    // of this scan's filters; null while it matches none. Under deviceLock.
    private DeviceEntry admitScanResult(ScanSession session, String key, ScanRecord record)
    {
        DeviceEntry pending = pendingScanDevices.get(key);

        if (pending == null)
        {
            if (pendingScanDevices.size() >= MAX_PENDING_SCAN_DEVICES)
            {
                Iterator<String> oldest = pendingScanDevices.keySet().iterator();
                oldest.next();
                oldest.remove();
            }

            pending = new DeviceEntry();

            Long cached = deviceById.get(key);
            DeviceEntry entry = cached != null ? devices.get(cached) : null;

            if (entry != null)
                foldAdvertisement(pending, entry);

            pendingScanDevices.put(key, pending);
        }

        if (record != null)
            mergeAdvertisement(pending, record);

        for (BluetoothLeScanFilter filter : session.filters)
        {
            if (scanFilterMatches(filter, pending))
            {
                pendingScanDevices.remove(key);
                session.matched.add(key);
                return pending;
            }
        }

        return null;
    }


    private static boolean scanFilterMatches(BluetoothLeScanFilter filter, DeviceEntry device)
    {
        if (present(filter.service_uuid()) &&
            !device.advServiceUuids.contains(parseUuid(filter.service_uuid().get()).toString()))
            return false;

        if (present(filter.name()) && !filter.name().get().equals(device.advLocalName))
            return false;

        if (present(filter.name_prefix()) && !device.advLocalName.startsWith(filter.name_prefix().get()))
            return false;

        if (present(filter.company_id()))
        {
            byte[] data = device.advManufacturerData.get(filter.company_id().get());

            if (data == null ||
                !dataMatches(data, orNull(filter.manufacturer_data()), orNull(filter.manufacturer_data_mask())))
                return false;
        }

        if (present(filter.service_data_uuid()))
        {
            byte[] data = device.advServiceData.get(parseUuid(filter.service_data_uuid().get()).toString());

            if (data == null ||
                !dataMatches(data, orNull(filter.service_data()), orNull(filter.service_data_mask())))
                return false;
        }

        return true;
    }


    // The advertised bytes start with expected under mask (every bit when
    // there is none); no expected bytes match any payload.
    private static boolean dataMatches(byte[] advertised, List<Byte> expected, List<Byte> mask)
    {
        if (expected == null)
            return true;

        byte[] data = toBytes(expected);
        byte[] bits = mask != null ? toBytes(mask) : null;

        if (advertised.length < data.length)
            return false;

        for (int i = 0; i < data.length; i++)
        {
            int m = bits != null ? bits[i] : 0xFF;

            if ((advertised[i] & m) != (data[i] & m))
                return false;
        }

        return true;
    }


    // The core's filter pre-flight, in its order: a filter must set a field,
    // a UUID must be well formed, a company id must fit 16 bits, data needs
    // its key and a mask its data, of the same length. Sets the last error
    // when it fails.
    private BluetoothError checkScanFilters(List<BluetoothLeScanFilter> filters)
    {
        for (BluetoothLeScanFilter filter : filters)
        {
            if (filter == null ||
                (!present(filter.service_uuid()) && !present(filter.name()) &&
                 !present(filter.name_prefix()) && !present(filter.company_id()) &&
                 !present(filter.manufacturer_data()) && !present(filter.manufacturer_data_mask()) &&
                 !present(filter.service_data_uuid()) && !present(filter.service_data()) &&
                 !present(filter.service_data_mask())))
                return result(INVALID_ARGUMENT, "A scan filter must set at least one field");

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

            if (present(filter.manufacturer_data()) && !present(filter.company_id()))
                return result(INVALID_ARGUMENT, "A scan filter's manufacturer_data needs company_id");

            BluetoothError mask = checkScanFilterMask(
                orNull(filter.manufacturer_data()),
                orNull(filter.manufacturer_data_mask()),
                "manufacturer_data");
            if (mask != OK)
                return mask;

            if (present(filter.service_data_uuid()) && !isValidUuid(filter.service_data_uuid().get()))
                return result(INVALID_ARGUMENT, "Invalid UUID: " + filter.service_data_uuid().get());

            if (present(filter.service_data()) && !present(filter.service_data_uuid()))
                return result(INVALID_ARGUMENT, "A scan filter's service_data needs service_data_uuid");

            mask = checkScanFilterMask(
                orNull(filter.service_data()),
                orNull(filter.service_data_mask()),
                "service_data");
            if (mask != OK)
                return mask;
        }

        return OK;
    }


    private BluetoothError checkScanFilterMask(List<Byte> data, List<Byte> mask, String field)
    {
        if (mask == null)
            return OK;

        if (data == null)
            return result(INVALID_ARGUMENT, "A scan filter's " + field + "_mask needs " + field);

        if (mask.size() != data.size())
            return result(
                INVALID_ARGUMENT,
                "A scan filter's " + field + "_mask must be as long as " + field);

        return OK;
    }


    private static final long SCAN_UPDATE_INTERVAL_DEFAULT_MS = 1000;
    private static final int SCAN_UPDATE_INTERVAL_MAX_MS = 60_000;

    private static final String SCAN_BUSY_MESSAGE = "A scan is already running; stop it first";


    @Override
    public BluetoothError bluetooth_le_scan_start(
        Optional<BluetoothLeScanSettings> settings,
        Optional<List<BluetoothLeScanFilter>> filters,
        GMFunction on_found,
        Optional<GMFunction> on_updated,
        Optional<GMFunction> on_stopped)
    {
        if (!initialized)
            return result(NOT_INITIALIZED, NOT_INITIALIZED_MESSAGE);

        // The scanner has no passive mode, so active is ignored. The default
        // mode is LowLatency: low power samples a fraction of the time, and a
        // game scanning wants to find the device now.
        long updateIntervalMs = SCAN_UPDATE_INTERVAL_DEFAULT_MS;
        int scanMode = ScanSettings.SCAN_MODE_LOW_LATENCY;
        BluetoothLeScanSettings scanSettings = orNull(settings);

        if (scanSettings != null)
        {
            if (present(scanSettings.update_interval_ms()))
            {
                int interval = scanSettings.update_interval_ms().get();

                if (interval < 0 || interval > SCAN_UPDATE_INTERVAL_MAX_MS)
                    return result(INVALID_ARGUMENT, "update_interval_ms must be 0-60000");

                updateIntervalMs = interval;
            }

            BluetoothLeScanMode mode = orNull(scanSettings.mode());

            if (mode == BluetoothLeScanMode.LowPower)
                scanMode = ScanSettings.SCAN_MODE_LOW_POWER;
            else if (mode == BluetoothLeScanMode.Balanced)
                scanMode = ScanSettings.SCAN_MODE_BALANCED;
        }

        List<BluetoothLeScanFilter> filterList =
            present(filters) ? filters.get() : new ArrayList<BluetoothLeScanFilter>();

        // Bad filters fail before the radio is looked at, as in the core.
        BluetoothError checked = checkScanFilters(filterList);
        if (checked != OK)
            return checked;

        BluetoothError ready = requireAdapter();
        if (ready != OK)
            return ready;

        if (leScan.get() != null)
            return result(BUSY, SCAN_BUSY_MESSAGE);

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

        ScanSession session = new ScanSession(
            TRANSPORT_LE,
            on_found,
            orNull(on_updated),
            orNull(on_stopped),
            updateIntervalMs,
            filterList);

        try
        {
            session.scanner = adapter.getBluetoothLeScanner();

            if (session.scanner == null)
                return result(
                    NOT_SUPPORTED,
                    "Bluetooth LE scanner is unavailable");

            session.callback = createScanCallback(session);

            ScanSettings nativeSettings = new ScanSettings.Builder()
                .setScanMode(scanMode)
                .build();

            // No filters is null: every device is reported.
            List<ScanFilter> scanFilters = buildScanFilters(session.filters);

            // In place before the stack starts: results can arrive before it
            // returns.
            synchronized (deviceLock)
            {
                pendingScanDevices.clear();
            }

            if (!leScan.compareAndSet(null, session))
                return result(BUSY, SCAN_BUSY_MESSAGE);

            session.scanner.startScan(
                scanFilters.isEmpty() ? null : scanFilters,
                nativeSettings,
                session.callback);

            return OK;
        }
        catch (SecurityException exception)
        {
            leScan.compareAndSet(session, null);
            return result(
                PERMISSION_DENIED,
                throwableMessage(exception));
        }
        catch (Throwable throwable)
        {
            leScan.compareAndSet(session, null);
            return result(
                OPERATION_FAILED,
                throwableMessage(throwable));
        }
    }


    // Fires no on_stopped: the game asked for it.
    @Override
    public BluetoothError bluetooth_le_scan_stop()
    {
        if (!initialized)
            return result(
                NOT_INITIALIZED,
                "Bluetooth is not initialized");

        ScanSession session = leScan.getAndSet(null);

        if (session == null)
            return OK;

        stopLeScanner(session);

        synchronized (deviceLock)
        {
            pendingScanDevices.clear();
        }

        return OK;
    }


    @Override
    public boolean bluetooth_le_scan_is_running()
    {
        return initialized && leScan.get() != null;
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
                // link itself, so a closed link reports nothing more. An
                // entry erased before its gatt was set leaves this client
                // to close here (R2-161).
                LeConnectionEntry entry = getLeConnection(connection);
                if (entry == null)
                {
                    closeGatt(gatt);
                    return;
                }
                if (entry.manualClosing)
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

                    // Fails the ops still waiting, before on_disconnected, as
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
                        invoke(
                            entry.onDisconnected,
                            DISCONNECTED,
                            connectionStatusMessage(status),
                            (double) connection);
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

                    if (op != null && op.discoverCharacteristics)
                        discoverDeeper(entry, op);

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
                        endFailedSubscribe(op);

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


    // options.timeout_ms, or -1 with the last error set when it is out of
    // range.
    private long connectTimeoutMs(Optional<BluetoothConnectOptions> options)
    {
        BluetoothConnectOptions value = orNull(options);

        if (value == null || !present(value.timeout_ms()))
            return CONNECT_TIMEOUT_DEFAULT_MS;

        int timeout = value.timeout_ms().get();

        if (timeout < CONNECT_TIMEOUT_MIN_MS || timeout > CONNECT_TIMEOUT_MAX_MS)
        {
            setLastError(INVALID_ARGUMENT, "timeout_ms must be 1000-300000");
            return -1;
        }

        return timeout;
    }


    @Override
    public long bluetooth_le_connect(
        long device,
        Optional<BluetoothConnectOptions> options,
        GMFunction on_disconnected,
        GMFunction callback)
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

        final long timeoutMs = connectTimeoutMs(options);

        if (timeoutMs < 0)
            return 0;

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
        entry.onDisconnected = on_disconnected;
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

        // Radio off erased the entry before its gatt was set, and has fired
        // the callback; the client is this call's to close (R2-161).
        if (getLeConnection(connection) != entry)
        {
            closeGatt(entry.gatt);
            return connection;
        }

        // connectGatt with autoConnect off gives up on its own only after
        // about 30 seconds, and some stacks never do; the window is the
        // game's on every platform.
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

        timeoutHandler.postDelayed(entry.connectTimeout, timeoutMs);

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
    // callback ConnectionFailed; an established link fires no on_disconnected.
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


    // These four answer for a server connection too.
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

        LeConnectionEntry entry = getLeConnection(connection);

        if (entry == null)
            return 0;

        boolean connected = entry.serverRole ? entry.connected : leConnected(entry);
        return connected ? entry.mtu : 0;
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
    public BluetoothError bluetooth_le_services_discover(
        long connection,
        Optional<BluetoothLeDiscoverOptions> options,
        GMFunction callback)
    {
        if (!initialized)
            return result(NOT_INITIALIZED, "Bluetooth is not initialized");

        LeConnectionEntry connEntry = getLeConnection(connection);

        if (connEntry == null || connEntry.gatt == null)
            return result(INVALID_HANDLE, "Invalid BLE connection handle");

        LePendingOp op = new LePendingOp();
        BluetoothLeDiscoverOptions discover = orNull(options);

        if (discover != null)
        {
            op.discoverDescriptors = Boolean.TRUE.equals(orNull(discover.descriptors()));
            op.discoverCharacteristics =
                op.discoverDescriptors || Boolean.TRUE.equals(orNull(discover.characteristics()));

            List<String> uuids = orNull(discover.service_uuids());

            // An empty list filters nothing, as an undefined one.
            if (uuids != null && !uuids.isEmpty())
            {
                op.discoverServiceUuids = new ArrayList<>();

                for (String uuid : uuids)
                {
                    if (!isValidUuid(uuid))
                        return result(INVALID_ARGUMENT, "Invalid UUID in service_uuids: " + uuid);

                    op.discoverServiceUuids.add(parseUuid(uuid));
                }
            }
        }

        if (!connEntry.connected)
            return result(DISCONNECTED, "BLE connection is not connected");

        final BluetoothGatt gatt = connEntry.gatt;

        op.kind = LePendingOp.KIND_DISCOVER_SERVICES;
        op.callback = callback;
        op.targetHandle = connection;
        op.startFailureMessage = "discoverServices() failed to start";
        op.start = () -> gatt.discoverServices();

        enqueueGattOp(connEntry, op);

        return OK;
    }


    // discoverServices() already brought the whole database, so the levels
    // the options ask for are listed here, before the one callback.
    private void discoverDeeper(LeConnectionEntry connection, LePendingOp op)
    {
        ArrayList<Long> services;

        synchronized (connection.serviceListLock)
        {
            services = new ArrayList<>(connection.serviceHandles);
        }

        synchronized (leEntityLock)
        {
            for (long serviceHandle : services)
            {
                LeServiceEntry service = leServices.get(serviceHandle);

                if (service == null || service.gattService == null)
                    continue;

                if (op.discoverServiceUuids != null &&
                    !op.discoverServiceUuids.contains(service.gattService.getUuid()))
                    continue;

                listLeCharacteristics(service);

                if (!op.discoverDescriptors)
                    continue;

                for (long characteristicHandle : service.characteristicHandles)
                {
                    LeCharacteristicEntry characteristic = leCharacteristics.get(characteristicHandle);

                    if (characteristic != null && characteristic.gattCharacteristic != null)
                        listLeDescriptors(characteristic);
                }
            }
        }
    }


    // Caller holds leEntityLock.
    private void listLeCharacteristics(LeServiceEntry entry)
    {
        if (entry.characteristicsDiscovered)
            return;

        entry.characteristicHandles.clear();

        for (BluetoothGattCharacteristic characteristic : entry.gattService.getCharacteristics())
        {
            entry.characteristicHandles.add(
                findOrCreateCharacteristicHandle(
                    entry.handle,
                    entry.connection,
                    characteristic));
        }

        entry.characteristicsDiscovered = true;
    }


    // Caller holds leEntityLock.
    private void listLeDescriptors(LeCharacteristicEntry entry)
    {
        if (entry.descriptorsDiscovered)
            return;

        entry.descriptorHandles.clear();

        for (BluetoothGattDescriptor descriptor : entry.gattCharacteristic.getDescriptors())
            entry.descriptorHandles.add(findOrCreateDescriptorHandle(entry.handle, descriptor));

        entry.descriptorsDiscovered = true;
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
        LeServiceEntry entry = getAnyLeService(service);
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
            return invalidAttribute(service, "Invalid BLE service handle");

        LeConnectionEntry connEntry = getLeConnection(entry.connection);

        if (connEntry == null || connEntry.gatt == null || !connEntry.connected)
            return result(DISCONNECTED, "BLE connection is not connected");

        // The complete GATT database (services, characteristics and
        // descriptors) is already downloaded by the single discoverServices()
        // round trip that already ran - there is no further remote operation
        // to perform here, so the callback fires synchronously.
        synchronized (leEntityLock)
        {
            listLeCharacteristics(entry);
        }

        invoke(callback, OK, "", (double) service);
        return OK;
    }


    @Override
    public int bluetooth_le_characteristic_get_count(long service)
    {
        LeServiceEntry entry = getAnyLeService(service);

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
        LeServiceEntry entry = getAnyLeService(service);

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
        LeCharacteristicEntry entry = getAnyLeCharacteristic(characteristic);
        return entry != null ? entry.uuid : "";
    }


    @Override
    public int bluetooth_le_characteristic_get_properties(long characteristic)
    {
        LeCharacteristicEntry entry = getAnyLeCharacteristic(characteristic);

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
            return invalidAttribute(characteristic, "Invalid BLE characteristic handle");

        LeConnectionEntry connEntry = getLeConnection(entry.connection);

        if (connEntry == null || connEntry.gatt == null || !connEntry.connected)
            return result(DISCONNECTED, "BLE connection is not connected");

        synchronized (leEntityLock)
        {
            listLeDescriptors(entry);
        }

        invoke(callback, OK, "", (double) characteristic);
        return OK;
    }


    @Override
    public int bluetooth_le_descriptor_get_count(long characteristic)
    {
        LeCharacteristicEntry entry = getAnyLeCharacteristic(characteristic);

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
        LeCharacteristicEntry entry = getAnyLeCharacteristic(characteristic);

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
        LeDescriptorEntry entry = getAnyLeDescriptor(descriptor);
        return entry != null ? entry.uuid : "";
    }


    // =========================================================================
    // BLE GATT - find by UUID
    //
    // The first match in discovery order (definition order for the server's
    // own attributes), any spelling of the UUID: the entries hold the
    // canonical lowercase form. Every 0 sets the last error, NotFound for a
    // plain miss.
    // =========================================================================

    // The canonical form of uuid, or null with the last error set.
    private String findCheck(String uuid)
    {
        if (!isValidUuid(uuid))
        {
            setLastError(INVALID_ARGUMENT, "Invalid UUID: " + uuid);
            return null;
        }

        return parseUuid(uuid).toString();
    }


    @Override
    public long bluetooth_le_service_find(long connection, String uuid)
    {
        if (!initialized)
        {
            setLastError(NOT_INITIALIZED, NOT_INITIALIZED_MESSAGE);
            return 0;
        }

        LeConnectionEntry entry = getLeConnection(connection);

        if (entry == null)
        {
            setLastError(INVALID_HANDLE, "Invalid BLE connection handle");
            return 0;
        }

        String wanted = findCheck(uuid);

        if (wanted == null)
            return 0;

        ArrayList<Long> services;

        synchronized (entry.serviceListLock)
        {
            services = new ArrayList<>(entry.serviceHandles);
        }

        synchronized (leEntityLock)
        {
            for (long handle : services)
            {
                LeServiceEntry service = leServices.get(handle);

                if (service != null && wanted.equals(service.uuid))
                    return handle;
            }
        }

        setLastError(NOT_FOUND, "No discovered service has UUID " + uuid);
        return 0;
    }


    @Override
    public long bluetooth_le_characteristic_find(long service, String uuid)
    {
        if (!initialized)
        {
            setLastError(NOT_INITIALIZED, NOT_INITIALIZED_MESSAGE);
            return 0;
        }

        LeServiceEntry entry = getAnyLeService(service);

        if (entry == null)
        {
            setLastError(INVALID_HANDLE, "Invalid BLE service handle");
            return 0;
        }

        String wanted = findCheck(uuid);

        if (wanted == null)
            return 0;

        synchronized (leEntityLock)
        {
            for (long handle : entry.characteristicHandles)
            {
                LeCharacteristicEntry characteristic = getAnyLeCharacteristic(handle);

                if (characteristic != null && wanted.equals(characteristic.uuid))
                    return handle;
            }
        }

        setLastError(NOT_FOUND, "No discovered characteristic of the service has UUID " + uuid);
        return 0;
    }


    @Override
    public long bluetooth_le_descriptor_find(long characteristic, String uuid)
    {
        if (!initialized)
        {
            setLastError(NOT_INITIALIZED, NOT_INITIALIZED_MESSAGE);
            return 0;
        }

        LeCharacteristicEntry entry = getAnyLeCharacteristic(characteristic);

        if (entry == null)
        {
            setLastError(INVALID_HANDLE, "Invalid BLE characteristic handle");
            return 0;
        }

        String wanted = findCheck(uuid);

        if (wanted == null)
            return 0;

        synchronized (leEntityLock)
        {
            for (long handle : entry.descriptorHandles)
            {
                LeDescriptorEntry descriptor = getAnyLeDescriptor(handle);

                if (descriptor != null && wanted.equals(descriptor.uuid))
                    return handle;
            }
        }

        setLastError(NOT_FOUND, "No discovered descriptor of the characteristic has UUID " + uuid);
        return 0;
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
            return invalidAttribute(characteristic, "Invalid BLE characteristic handle");

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

        LeCharacteristicEntry charEntry = getLeCharacteristic(characteristic);

        if (charEntry == null)
            return invalidAttribute(characteristic, "Invalid BLE characteristic handle");

        if (write_type != BluetoothLeWriteType.WithResponse &&
            write_type != BluetoothLeWriteType.WithoutResponse)
            return result(INVALID_ARGUMENT, "Invalid BluetoothLeWriteType value");

        if (bufferRangeInvalid(data, offset, size))
            return result(
                INVALID_ARGUMENT,
                "Invalid buffer offset/size for bluetooth_le_characteristic_write");

        final boolean withoutResponse = write_type == BluetoothLeWriteType.WithoutResponse;

        LeConnectionEntry connEntry = getLeConnection(charEntry.connection);

        if (withoutResponse)
        {
            if ((charEntry.gattCharacteristic.getProperties() &
                 BluetoothGattCharacteristic.PROPERTY_WRITE_NO_RESPONSE) == 0)
                return result(NOT_SUPPORTED, "The characteristic does not take writes without response");

            if (connEntry != null &&
                pendingWritesWithoutResponse(connEntry) >= MAX_PENDING_WRITES_WITHOUT_RESPONSE)
                return result(BUSY, "Too many writes without response are waiting to be sent");
        }

        if (connEntry == null || connEntry.gatt == null || !connEntry.connected)
            return result(DISCONNECTED, "BLE connection is not connected");

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
        final int androidWriteType = withoutResponse
            ? BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
            : BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT;

        LePendingOp op = new LePendingOp();
        op.kind = LePendingOp.KIND_WRITE_CHARACTERISTIC;
        op.callback = callback;
        op.targetHandle = characteristic;
        op.withoutResponse = withoutResponse;
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
        GMFunction on_value,
        GMFunction callback)
    {
        // null is a value the generated decoder did not know.
        final int mode = subscribeMode != null ? subscribeMode.value() : -1;

        if (!initialized)
            return result(NOT_INITIALIZED, "Bluetooth is not initialized");

        final LeCharacteristicEntry charEntry = getLeCharacteristic(characteristic);

        if (charEntry == null)
            return invalidAttribute(characteristic, "Invalid BLE characteristic handle");

        if (mode != SUBSCRIBE_MODE_NOTIFY && mode != SUBSCRIBE_MODE_INDICATE)
            return result(INVALID_ARGUMENT, "Invalid BluetoothLeSubscribeMode value");

        final BluetoothGattCharacteristic gattCharacteristic =
            charEntry.gattCharacteristic;

        int properties = gattCharacteristic.getProperties();

        if (mode == SUBSCRIBE_MODE_NOTIFY &&
            (properties & BluetoothGattCharacteristic.PROPERTY_NOTIFY) == 0)
            return result(NOT_SUPPORTED, "The characteristic does not support notifications");

        if (mode == SUBSCRIBE_MODE_INDICATE &&
            (properties & BluetoothGattCharacteristic.PROPERTY_INDICATE) == 0)
            return result(NOT_SUPPORTED, "The characteristic does not support indications");

        synchronized (leEntityLock)
        {
            if (charEntry.subscribed)
                return result(BUSY, "The characteristic is already subscribed or subscribing");
        }

        LeConnectionEntry connEntry = getLeConnection(charEntry.connection);

        if (connEntry == null || connEntry.gatt == null || !connEntry.connected)
            return result(DISCONNECTED, "BLE connection is not connected");

        final BluetoothGattDescriptor cccd =
            gattCharacteristic.getDescriptor(CCCD_UUID);

        if (cccd == null)
            return result(
                NOT_SUPPORTED,
                "Characteristic has no client characteristic configuration descriptor");

        final byte[] cccdValue = mode == SUBSCRIBE_MODE_INDICATE
            ? BluetoothGattDescriptor.ENABLE_INDICATION_VALUE
            : BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE;

        LePendingOp op = newCccdOp(connEntry, charEntry, mode, cccdValue, callback);

        // Values can arrive before the CCCD write completes, so the receiver
        // is in place from the call.
        synchronized (leEntityLock)
        {
            if (charEntry.subscribed)
                return result(BUSY, "The characteristic is already subscribed or subscribing");

            charEntry.subscribed = true;
            charEntry.onValue = on_value;
            op.subscribeToken = ++charEntry.subscribeToken;
        }

        enqueueGattOp(connEntry, op);

        return OK;
    }


    // on_value stops here, at the call; the CCCD write goes out behind any
    // subscribe still queued. With nothing subscribed it fires Ok and sends
    // nothing.
    @Override
    public BluetoothError bluetooth_le_characteristic_unsubscribe(long characteristic, GMFunction callback)
    {
        if (!initialized)
            return result(NOT_INITIALIZED, "Bluetooth is not initialized");

        final LeCharacteristicEntry charEntry = getLeCharacteristic(characteristic);

        if (charEntry == null)
            return invalidAttribute(characteristic, "Invalid BLE characteristic handle");

        boolean subscribed;

        synchronized (leEntityLock)
        {
            subscribed = charEntry.subscribed;
        }

        if (!subscribed)
        {
            invoke(callback, OK, "", (double) characteristic);
            return OK;
        }

        LeConnectionEntry connEntry = getLeConnection(charEntry.connection);

        if (connEntry == null || connEntry.gatt == null || !connEntry.connected)
            return result(DISCONNECTED, "BLE connection is not connected");

        BluetoothGattDescriptor cccd = charEntry.gattCharacteristic.getDescriptor(CCCD_UUID);

        if (cccd == null)
            return result(
                NOT_SUPPORTED,
                "Characteristic has no client characteristic configuration descriptor");

        LePendingOp op = newCccdOp(
            connEntry,
            charEntry,
            SUBSCRIBE_MODE_UNSUBSCRIBE,
            BluetoothGattDescriptor.DISABLE_NOTIFICATION_VALUE,
            callback);

        synchronized (leEntityLock)
        {
            charEntry.subscribed = false;
            charEntry.onValue = null;
            op.subscribeToken = ++charEntry.subscribeToken;
        }

        enqueueGattOp(connEntry, op);

        return OK;
    }


    // A CCCD write for mode, completed by onDescriptorWrite.
    private LePendingOp newCccdOp(
        LeConnectionEntry connEntry,
        final LeCharacteristicEntry charEntry,
        final int mode,
        final byte[] cccdValue,
        GMFunction callback)
    {
        final BluetoothGatt gatt = connEntry.gatt;
        final BluetoothGattCharacteristic gattCharacteristic = charEntry.gattCharacteristic;
        final BluetoothGattDescriptor cccd = gattCharacteristic.getDescriptor(CCCD_UUID);

        final LePendingOp op = new LePendingOp();
        op.kind = LePendingOp.KIND_SUBSCRIBE;
        op.callback = callback;
        op.targetHandle = charEntry.handle;
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

        return op;
    }


    @Override
    public BluetoothError bluetooth_le_descriptor_read(long descriptor, GMFunction callback)
    {
        if (!initialized)
            return result(NOT_INITIALIZED, "Bluetooth is not initialized");

        LeDescriptorEntry descEntry = getLeDescriptor(descriptor);

        if (descEntry == null)
            return invalidAttribute(descriptor, "Invalid BLE descriptor handle");

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
            return invalidAttribute(descriptor, "Invalid BLE descriptor handle");

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


    // Copies the whole value. Too small a buffer copies nothing and keeps the
    // value, so the call can be retried.
    @Override
    public BluetoothError bluetooth_value_fetch(long value, ByteBuffer out_data, int offset)
    {
        StoredValue stored;
        boolean fits = false;
        ArrayList<StoredValue> expired;

        synchronized (valueLock)
        {
            expired = expireValues();
            stored = values.get(value);

            if (stored != null && !bufferRangeInvalid(out_data, offset, stored.bytes.length))
            {
                fits = true;

                if (stored.bytes.length > 0)
                {
                    ByteBuffer view = out_data.duplicate();
                    view.position(offset);
                    view.put(stored.bytes, 0, stored.bytes.length);
                }

                removeValue(value);
            }
        }

        endExpiredReceives(expired);

        if (stored == null)
            return result(INVALID_HANDLE, "Unknown, fetched, released or expired value " + value);

        if (!fits)
            return result(
                INVALID_ARGUMENT,
                "Buffer too small for value " + value + ": it needs " +
                    stored.bytes.length + " bytes at offset " + offset);

        if (stored.connection != null)
            releaseReceived(stored.connection, stored.bytes.length);

        return OK;
    }


    @Override
    public BluetoothError bluetooth_value_release(long value)
    {
        StoredValue stored;
        ArrayList<StoredValue> expired;

        synchronized (valueLock)
        {
            expired = expireValues();
            stored = removeValue(value);
        }

        endExpiredReceives(expired);

        if (stored == null)
            return result(INVALID_HANDLE, "Unknown, fetched, released or expired value " + value);

        if (stored.connection != null)
            releaseReceived(stored.connection, stored.bytes.length);

        return OK;
    }


    // =========================================================================
    // BLE advertise
    // =========================================================================

    // Back to IDLE. A start still waiting for the stack fails with
    // startError, since its AdvertiseCallback will no longer be heard.
    // Returns a running advertisement's on_stopped, for a caller whose stop
    // the game did not ask for; null otherwise.
    private GMFunction stopLeAdvertiseInternal(BluetoothError startError, String startMessage)
    {
        BluetoothLeAdvertiser advertiser;
        AdvertiseCallback callback;
        GMFunction startCallback;
        GMFunction onStopped;
        boolean wasStarting;
        boolean wasRunning;

        synchronized (leAdvertiseLock)
        {
            wasStarting = leAdvertiseState == ADVERTISE_STARTING;
            wasRunning = leAdvertiseState == ADVERTISE_RUNNING;
            advertiser = leAdvertiser;
            callback = leAdvertiseCallback;
            startCallback = leAdvertiseStartCallback;
            onStopped = leAdvertiseOnStopped;

            leAdvertiseState = ADVERTISE_IDLE;
            leAdvertiser = null;
            leAdvertiseCallback = null;
            leAdvertiseStartCallback = null;
            leAdvertiseOnStopped = null;
        }

        stopAdvertiser(advertiser, callback);

        if (wasStarting)
            invoke(startCallback, startError, startMessage);

        return wasRunning ? onStopped : null;
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


    // A record's array field the game left undefined reads as empty.
    private static <T> List<T> orEmpty(Optional<List<T>> values)
    {
        return values != null && values.isPresent() && values.get() != null
            ? values.get()
            : new ArrayList<T>();
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
        Optional<BluetoothLeAdvertiseSettings> settings,
        BluetoothLeAdvertiseData data,
        Optional<GMFunction> on_stopped,
        GMFunction callback)
    {
        BluetoothError ready = requireAdapter();
        if (ready != OK)
            return ready;

        if (data == null)
            return result(INVALID_ARGUMENT, "data cannot be undefined");

        // Left undefined: connectable, at the platform's default power.
        BluetoothLeAdvertiseSettings advertise = orNull(settings);
        BluetoothLeAdvertiseTxPower txPower = advertise != null ? orNull(advertise.tx_power()) : null;
        boolean connectable = advertise == null || !Boolean.FALSE.equals(orNull(advertise.connectable()));

        // Checked as the native core checks them, before anything starts.
        ArrayList<UUID> serviceUuids = new ArrayList<>();

        if (present(data.service_uuids()))
        {
            for (String text : data.service_uuids().get())
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

        for (BluetoothLeAdvertiseServiceData entry : orEmpty(data.service_data()))
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

        List<BluetoothLeAdvertiseManufacturerData> manufacturerData = orEmpty(data.manufacturer_data());

        for (BluetoothLeAdvertiseManufacturerData entry : manufacturerData)
        {
            if (entry == null)
                return result(INVALID_ARGUMENT, "A manufacturer_data entry is undefined");

            if (entry.company_id() < 0 || entry.company_id() > 0xFFFF)
                return result(
                    INVALID_ARGUMENT,
                    "company_id " + entry.company_id() + " is outside 0-65535");
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
            .setIncludeDeviceName(Boolean.TRUE.equals(orNull(data.include_name())))
            .setIncludeTxPowerLevel(Boolean.TRUE.equals(orNull(data.include_tx_power())));

        for (UUID uuid : serviceUuids)
            dataBuilder.addServiceUuid(new ParcelUuid(uuid));

        for (int i = 0; i < serviceDataUuids.size(); i++)
            dataBuilder.addServiceData(new ParcelUuid(serviceDataUuids.get(i)), serviceDataBytes.get(i));

        for (BluetoothLeAdvertiseManufacturerData entry : manufacturerData)
            dataBuilder.addManufacturerData(entry.company_id(), toBytes(entry.data()));

        int txPowerLevel = txPower != null
            ? advertiseTxPowerLevel(txPower)
            : AdvertiseSettings.ADVERTISE_TX_POWER_MEDIUM;

        AdvertiseSettings advertiseSettings = new AdvertiseSettings.Builder()
            .setAdvertiseMode(AdvertiseSettings.ADVERTISE_MODE_LOW_LATENCY)
            .setTxPowerLevel(txPowerLevel)
            .setConnectable(connectable)
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
                        leAdvertiseOnStopped = null;
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
            leAdvertiseOnStopped = orNull(on_stopped);
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
                    leAdvertiseOnStopped = null;
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
        LeServerSession session,
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

            if (existing != null)
                return;

            long connection = createLeConnection(deviceHandle, true);
            LeConnectionEntry entry = getLeConnection(connection);

            if (entry != null)
            {
                entry.remoteDevice = device;
                entry.onDisconnected = session.onDisconnected;
                entry.connected = true;
            }

            synchronized (leConnectionLock)
            {
                leServerConnectionByDevice.put(deviceHandle, connection);
            }

            invoke(session.onConnected, (double) connection, (double) deviceHandle);
        }
        else if (newState == BluetoothProfile.STATE_DISCONNECTED)
        {
            Long connection;

            synchronized (leConnectionLock)
            {
                connection = leServerConnectionByDevice.remove(deviceHandle);
            }

            LeConnectionEntry entry = connection != null ? getLeConnection(connection) : null;

            // The link is marked gone and leaves the subscribers first, so a
            // notify racing this queues nothing once its notifications are
            // dropped below.
            if (connection != null)
            {
                if (entry != null)
                    entry.connected = false;

                dropLeServerSubscriber(connection);
                eraseLeConnection(connection);
            }

            // Its requests can no longer be answered, its prepared writes
            // will never be executed, and nothing more is sent to it: no
            // onNotificationSent comes for a dead link, so the one in flight
            // goes too.
            dropLeServerRequestsOf(device);
            dropLeServerNotifications(safeAddress(device), false);

            if (entry != null)
                invoke(
                    entry.onDisconnected,
                    DISCONNECTED,
                    connectionStatusMessage(status),
                    (double) entry.handle);
        }
    }


    // A central reaching the GATT server enters the device cache without
    // being announced or changing what a scan recorded.
    private long serverDeviceHandle(BluetoothDevice device)
    {
        return upsertDevice(TRANSPORT_LE, device, 0, false, false);
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


    // serverGeneration is leServerGeneration when this server opened: once it
    // stops, every event of its callback is ignored, as is every event after
    // shutdown.
    private BluetoothGattServerCallback createGattServerCallback(
        final long workerGeneration,
        final long serverGeneration,
        final LeServerSession session)
    {
        return new BluetoothGattServerCallback()
        {
            private boolean stale()
            {
                return
                    generation.get() != workerGeneration ||
                    leServerGeneration.get() != serverGeneration;
            }


            @Override
            public void onConnectionStateChange(
                BluetoothDevice device,
                int status,
                int newState)
            {
                if (stale())
                    return;

                handleServerConnectionStateChange(session, device, status, newState);
            }


            // The ATT MTU the central negotiated, for get_mtu.
            @Override
            public void onMtuChanged(BluetoothDevice device, int mtu)
            {
                if (stale())
                    return;

                LeConnectionEntry entry = getLeConnection(resolveServerConnection(device));

                if (entry != null)
                    entry.mtu = mtu;
            }


            @Override
            public void onServiceAdded(int status, BluetoothGattService service)
            {
                if (stale())
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

                if (head != null && (head.cancelled || status != BluetoothGatt.GATT_SUCCESS))
                    eraseLeServerHandles(head.serviceHandle);

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
                    {
                        synchronized (leServerInitialValueLock)
                        {
                            if (head.initialValues != null)
                                leServerInitialValues.putAll(head.initialValues);

                            if (head.descriptorValues != null)
                                leServerDescriptorValues.putAll(head.descriptorValues);
                        }

                        invoke(head.callback, OK, "", (double) head.serviceHandle);
                    }
                    else
                        invoke(
                            head.callback,
                            mapAttError(status),
                            attErrorMessage(status),
                            0.0);
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
                if (stale())
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
                    sendServerValue(device, requestId, offset, initialValue);
                    return;
                }

                handleServerReadRequest(
                    session,
                    device,
                    requestId,
                    offset,
                    characteristic,
                    null);
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
                if (stale())
                    return;

                if (preparedWrite)
                {
                    handlePreparedWrite(device, requestId, characteristic, null, responseNeeded, offset, value);
                    return;
                }

                handleServerWriteRequest(
                    session,
                    device,
                    requestId,
                    characteristic,
                    null,
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
                if (stale())
                    return;

                BluetoothGattCharacteristic parent =
                    descriptor != null ? descriptor.getCharacteristic() : null;

                // The CCCD is the extension's: each central reads back what
                // it wrote, and GML never sees the request.
                if (descriptor != null && parent != null && CCCD_UUID.equals(descriptor.getUuid()))
                {
                    sendServerValue(device, requestId, offset, leServerCccdValue(parent, resolveServerConnection(device)));
                    return;
                }

                // A static value is served here, as on Windows and Apple.
                byte[] staticValue;

                synchronized (leServerInitialValueLock)
                {
                    staticValue = descriptor != null ? leServerDescriptorValues.get(descriptor) : null;
                }

                if (staticValue != null)
                {
                    sendServerValue(device, requestId, offset, staticValue);
                    return;
                }

                handleServerReadRequest(
                    session,
                    device,
                    requestId,
                    offset,
                    parent,
                    descriptor);
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
                if (stale())
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

                handleServerWriteRequest(
                    session,
                    device,
                    requestId,
                    parent,
                    descriptor,
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
                if (stale())
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

                GMFunction callback = session.onWrite;
                long connection = resolveServerConnection(device);

                LeServerWriteBatch batch = new LeServerWriteBatch();
                batch.device = device;
                batch.stackRequestId = requestId;

                ArrayList<LeServerRequestEntry> requests = new ArrayList<>();
                ArrayList<long[]> attributes = new ArrayList<>();
                ArrayList<Integer> offsets = new ArrayList<>();
                ArrayList<Integer> sizes = new ArrayList<>();

                for (LeServerPreparedWrite write : prepared.values())
                {
                    // An attribute without a handle is not reported; the
                    // execute answers its error.
                    long[] handles = serverRequestHandles(write.characteristic, write.descriptor);

                    if (handles == null)
                    {
                        batch.status = ATT_ATTRIBUTE_NOT_FOUND;
                        continue;
                    }

                    LeServerRequestEntry request = new LeServerRequestEntry();
                    request.stackRequestId = requestId;
                    request.connection = connection;
                    request.device = device;
                    request.isWrite = true;
                    request.responseNeeded = true;
                    request.batch = batch;

                    // Fragments are laid out from 0; GML gets them from the
                    // first fragment's offset, as a single write. The execute
                    // is answered without an echo.
                    int start = Math.min(write.offset, write.value.length);
                    byte[] bytes = Arrays.copyOfRange(write.value, start, write.value.length);
                    request.value = storeValue(VALUE_SERVER_WRITE, bytes, null);

                    requests.add(request);
                    attributes.add(handles);
                    offsets.add(start);
                    sizes.add(bytes.length);
                }

                if (requests.isEmpty())
                {
                    sendServerResponse(device, requestId, batch.status, 0, null);
                    return;
                }

                batch.remaining = requests.size();

                ArrayList<Integer> ids = new ArrayList<>();

                expireLeServerRequests();

                synchronized (leServerRequestLock)
                {
                    for (LeServerRequestEntry request : requests)
                        ids.add(storeLeServerRequest(request));
                }

                for (int i = 0; i < ids.size(); i++)
                {
                    invoke(
                        callback,
                        ids.get(i),
                        (double) connection,
                        (double) attributes.get(i)[0],
                        (double) attributes.get(i)[1],
                        offsets.get(i),
                        true,
                        (double) requests.get(i).value,
                        sizes.get(i));
                }
            }


            // The stack has sent the device's notification in flight (or an
            // indication was confirmed); the next one waiting goes now.
            @Override
            public void onNotificationSent(BluetoothDevice device, int status)
            {
                if (stale())
                    return;

                String address = safeAddress(device);

                synchronized (leServerNotifyLock)
                {
                    LeServerNotification sent = leServerNotifyInFlight.remove(address);

                    // Dropped meanwhile: nothing of this device's is in flight.
                    if (sent == null)
                        return;

                    releaseLeServerNotification(sent);
                }

                sendLeServerNotifications(address);
            }
        };
    }


    // Sends the device's next notification unless one is in flight: the head
    // of its queue moves to leServerNotifyInFlight, and one the stack refuses
    // is dropped and the next tried. Safe to call from anywhere; the in-flight
    // entry is the device's sending turn.
    private void sendLeServerNotifications(String address)
    {
        while (true)
        {
            LeServerNotification next;

            synchronized (leServerNotifyLock)
            {
                if (leServerNotifyInFlight.containsKey(address))
                    return;

                ArrayDeque<LeServerNotification> queue = leServerNotifyQueues.get(address);
                next = queue != null ? queue.pollFirst() : null;

                if (queue != null && queue.isEmpty())
                    leServerNotifyQueues.remove(address);

                if (next == null)
                    return;

                leServerNotifyInFlight.put(address, next);
            }

            if (sendLeServerNotification(next))
                return;

            synchronized (leServerNotifyLock)
            {
                // A drop may have taken it meanwhile, and another send its
                // place.
                if (leServerNotifyInFlight.get(address) != next)
                    return;

                leServerNotifyInFlight.remove(address);
                releaseLeServerNotification(next);
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


    // Drops what waits for a device, or for every device when address is
    // null. keepInFlight (clear_services) leaves a send the stack holds in
    // place, so a later notify still waits for its onNotificationSent; a
    // disconnect or a stop drops it too, since no callback will come for it.
    private void dropLeServerNotifications(String address, boolean keepInFlight)
    {
        synchronized (leServerNotifyLock)
        {
            if (address == null)
            {
                for (ArrayDeque<LeServerNotification> queue : leServerNotifyQueues.values())
                    for (LeServerNotification notification : queue)
                        releaseLeServerNotification(notification);

                leServerNotifyQueues.clear();

                if (!keepInFlight)
                {
                    for (LeServerNotification notification : leServerNotifyInFlight.values())
                        releaseLeServerNotification(notification);

                    leServerNotifyInFlight.clear();
                }

                return;
            }

            ArrayDeque<LeServerNotification> queue = leServerNotifyQueues.remove(address);

            if (queue != null)
                for (LeServerNotification notification : queue)
                    releaseLeServerNotification(notification);

            if (!keepInFlight)
            {
                LeServerNotification inFlight = leServerNotifyInFlight.remove(address);

                if (inFlight != null)
                    releaseLeServerNotification(inFlight);
            }
        }
    }


    // Caller holds leServerNotifyLock. A notification left its queue or the
    // stack, sent or not; its call stops counting with the last one.
    private void releaseLeServerNotification(LeServerNotification notification)
    {
        if (--notification.call.remaining == 0)
            leServerNotifyCount--;
    }


    // False when the response was not sent.
    private boolean sendServerResponse(
        BluetoothDevice device,
        int stackRequestId,
        int status,
        int offset,
        byte[] value)
    {
        BluetoothGattServer server = gattServer;

        if (server == null || device == null)
            return false;

        try
        {
            return server.sendResponse(device, stackRequestId, status, offset, value);
        }
        catch (Throwable ignored)
        {
            return false;
        }
    }


    // A value the extension serves itself, from offset on.
    private void sendServerValue(BluetoothDevice device, int stackRequestId, int offset, byte[] value)
    {
        if (offset < 0 || offset > value.length)
            sendServerResponse(device, stackRequestId, ATT_INVALID_OFFSET, offset, null);
        else
            sendServerResponse(
                device,
                stackRequestId,
                BluetoothGatt.GATT_SUCCESS,
                offset,
                Arrays.copyOfRange(value, offset, value.length));
    }


    // A read GML answers through the server's on_read.
    private void handleServerReadRequest(
        LeServerSession session,
        BluetoothDevice device,
        int stackRequestId,
        int offset,
        BluetoothGattCharacteristic characteristic,
        BluetoothGattDescriptor descriptor)
    {
        GMFunction callback = session.onRead;

        long[] handles = serverRequestHandles(characteristic, descriptor);

        if (handles == null)
        {
            sendServerResponse(device, stackRequestId, ATT_ATTRIBUTE_NOT_FOUND, offset, null);
            return;
        }

        long connection = resolveServerConnection(device);

        LeServerRequestEntry request = new LeServerRequestEntry();
        request.stackRequestId = stackRequestId;
        request.connection = connection;
        request.device = device;
        request.isWrite = false;
        request.responseNeeded = true;

        int requestId;

        expireLeServerRequests();

        synchronized (leServerRequestLock)
        {
            requestId = storeLeServerRequest(request);
        }

        invoke(
            callback,
            requestId,
            (double) connection,
            (double) handles[0],
            (double) handles[1],
            offset);
    }


    // A write that is not part of a prepared write.
    private void handleServerWriteRequest(
        LeServerSession session,
        BluetoothDevice device,
        int stackRequestId,
        BluetoothGattCharacteristic characteristic,
        BluetoothGattDescriptor descriptor,
        boolean responseNeeded,
        int offset,
        byte[] value)
    {
        GMFunction callback = session.onWrite;

        long[] handles = serverRequestHandles(characteristic, descriptor);

        if (handles == null)
        {
            if (responseNeeded)
                sendServerResponse(device, stackRequestId, ATT_ATTRIBUTE_NOT_FOUND, 0, null);
            return;
        }

        long connection = resolveServerConnection(device);

        LeServerRequestEntry request = new LeServerRequestEntry();
        request.stackRequestId = stackRequestId;
        request.connection = connection;
        request.device = device;
        request.isWrite = true;
        request.responseNeeded = responseNeeded;
        byte[] bytes = value != null ? value.clone() : new byte[0];
        request.writeValue = bytes;
        request.value = storeValue(VALUE_SERVER_WRITE, bytes.clone(), null);

        int requestId;

        expireLeServerRequests();

        synchronized (leServerRequestLock)
        {
            requestId = storeLeServerRequest(request);
        }

        invoke(
            callback,
            requestId,
            (double) connection,
            (double) handles[0],
            (double) handles[1],
            offset,
            responseNeeded,
            (double) request.value,
            bytes.length);
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
    // id; the caller runs expireLeServerRequests first.
    private int storeLeServerRequest(LeServerRequestEntry request)
    {
        int requestId = nextLeServerRequestId++;

        if (nextLeServerRequestId <= 0)
            nextLeServerRequestId = 1;

        leServerRequests.put(requestId, request);
        return requestId;
    }


    // Runs on each new request, so neither the no-response writes of a
    // streaming central nor requests nobody answered can pile up. Those a
    // central still waits on are answered, with no lock held.
    private void expireLeServerRequests()
    {
        long now = System.nanoTime();
        ArrayList<LeServerRequestEntry> toAnswer = new ArrayList<>();

        synchronized (leServerRequestLock)
        {
            Iterator<Map.Entry<Integer, LeServerRequestEntry>> iterator =
                leServerRequests.entrySet().iterator();

            while (iterator.hasNext())
            {
                LeServerRequestEntry request = iterator.next().getValue();
                long ttl = request.responseNeeded
                    ? SERVER_REQUEST_TTL_NANOS
                    : NO_RESPONSE_WRITE_TTL_NANOS;

                if (now - request.receivedAtNanos > ttl)
                {
                    iterator.remove();
                    discardValue(request.value);

                    if (request.responseNeeded)
                        toAnswer.add(request);
                }
            }
        }

        for (LeServerRequestEntry request : toAnswer)
        {
            if (request.batch != null)
                finishLeServerWriteBatchPart(request.batch, ATT_UNLIKELY_ERROR);
            else
                sendServerResponse(request.device, request.stackRequestId, ATT_UNLIKELY_ERROR, 0, null);
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
                {
                    iterator.remove();
                    discardValue(request.value);
                }
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
            discardValue(request.value);

            if (!request.responseNeeded)
                continue;

            if (request.batch != null && !answeredBatches.add(request.batch))
                continue;

            sendServerResponse(request.device, request.stackRequestId, ATT_UNLIKELY_ERROR, 0, null);
        }
    }


    // One attribute of an executed prepared write has been answered; the
    // execute itself is answered after the last one, with the first error.
    // False when this part was the last and the Execute answer was not sent.
    private boolean finishLeServerWriteBatchPart(LeServerWriteBatch batch, int status)
    {
        boolean done;

        synchronized (leServerRequestLock)
        {
            if (status != BluetoothGatt.GATT_SUCCESS && batch.status == BluetoothGatt.GATT_SUCCESS)
                batch.status = status;

            batch.remaining--;
            done = batch.remaining == 0;
        }

        return !done || sendServerResponse(batch.device, batch.stackRequestId, batch.status, 0, null);
    }


    // Queued adds fail with addError. reportDisconnects (radio off) tells
    // each central's on_disconnected the same; bluetooth_le_server_stop and
    // shutdown retire them silently. Returns the session that was running,
    // null when none was.
    private LeServerSession stopLeServerInternal(BluetoothError addError, String addMessage, boolean reportDisconnects)
    {
        LeServerSession session = leServerSession.getAndSet(null);

        // From here the closing server's callback ignores whatever the stack
        // still delivers.
        leServerGeneration.incrementAndGet();

        // Answered while the server can still send; it forgets them on close.
        answerPendingLeServerRequests();

        synchronized (leServerInitialValueLock)
        {
            leServerInitialValues.clear();
            leServerDescriptorValues.clear();
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
            for (LeServerRequestEntry request : leServerRequests.values())
                discardValue(request.value);

            leServerRequests.clear();
            leServerPreparedWrites.clear();
        }

        synchronized (leServerSubscriberLock)
        {
            leServerSubscribers.clear();
        }

        dropLeServerNotifications(null, false);

        for (LeConnectionEntry entry : serverConnections)
        {
            eraseLeConnection(entry.handle);

            if (reportDisconnects && entry.connected)
                invoke(entry.onDisconnected, addError, addMessage, (double) entry.handle);

            entry.connected = false;
        }

        // The server is closed, so no onServiceAdded will come for any of them.
        failServiceAdds(addError, addMessage, true);

        synchronized (leServerAddServiceLock)
        {
            leServerServiceUuids.clear();
        }

        clearLeServerHandles();
        return session;
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

            eraseLeServerHandles(head.serviceHandle);

            if (fire)
                invoke(head.callback, OPERATION_FAILED, failure, 0.0);
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
        {
            eraseLeServerHandles(entry.serviceHandle);
            invoke(entry.callback, error, message, 0.0);
        }
    }


    @Override
    public BluetoothError bluetooth_le_server_start(
        GMFunction on_connected,
        GMFunction on_disconnected,
        GMFunction on_read,
        GMFunction on_write,
        Optional<GMFunction> on_stopped)
    {
        BluetoothError ready = requireAdapter();
        if (ready != OK)
            return ready;

        if (leServerSession.get() != null)
            return result(BUSY, "The GATT server is already running; stop it first");

        if (!hasConnectPermission())
            return result(
                PERMISSION_DENIED,
                "Bluetooth connect permission is not granted");

        if (!adapterEnabled())
            return result(BLUETOOTH_DISABLED, "Bluetooth is disabled");

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
            LeServerSession session = new LeServerSession(
                on_connected,
                on_disconnected,
                on_read,
                on_write,
                orNull(on_stopped));

            BluetoothGattServer server = manager.openGattServer(
                current,
                createGattServerCallback(workerGeneration, leServerGeneration.get(), session));

            if (server == null)
                return result(
                    NOT_SUPPORTED,
                    "Bluetooth LE peripheral role is unavailable");

            if (!leServerSession.compareAndSet(null, session))
            {
                closeQuietly(server::close);
                return result(BUSY, "The GATT server is already running; stop it first");
            }

            gattServer = server;
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

        // Advertising ends with the server, on every platform; neither
        // fires anything.
        if (stopLeServerInternal(
            OPERATION_FAILED,
            "BLE server stopped before the service was added",
            false) != null)
            stopLeAdvertiseInternal(OPERATION_FAILED, "Advertising stopped before it started");

        return OK;
    }


    @Override
    public boolean bluetooth_le_server_is_running()
    {
        return initialized && leServerSession.get() != null;
    }


    @Override
    public BluetoothError bluetooth_le_server_add_service(BluetoothLeServiceDefinition service, GMFunction callback)
    {
        if (!initialized)
            return result(NOT_INITIALIZED, "Bluetooth is not initialized");

        if (service == null)
            return result(INVALID_ARGUMENT, "service cannot be null");

        final BluetoothGattService gattService;
        final String serviceUuid;
        final IdentityHashMap<BluetoothGattCharacteristic, byte[]> initialValues = new IdentityHashMap<>();
        final IdentityHashMap<BluetoothGattDescriptor, byte[]> descriptorValues = new IdentityHashMap<>();

        try
        {
            serviceUuid = service.uuid();
            if (serviceUuid == null || serviceUuid.isEmpty())
                return result(INVALID_ARGUMENT, "service.uuid cannot be empty");

            UUID parsedServiceUuid = parseUuid(serviceUuid);

            gattService = new BluetoothGattService(
                parsedServiceUuid,
                BluetoothGattService.SERVICE_TYPE_PRIMARY);

            for (BluetoothLeCharacteristicDefinition definition : orEmpty(service.characteristics()))
            {
                if (definition == null)
                    continue;

                String characteristicUuid = definition.uuid();
                int properties = definition.properties();
                int permissions = definition.permissions();

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
                byte[] initialValue = toBytes(orNull(definition.value()));
                if (initialValue.length > MAX_ATTRIBUTE_LENGTH)
                    return result(
                        INVALID_ARGUMENT,
                        "Characteristic " + characteristicUuid + ": the initial value is " +
                            initialValue.length + " bytes; an attribute value holds at most " +
                            MAX_ATTRIBUTE_LENGTH);

                // Served from leServerInitialValues by the read request, once
                // the stack has added the service.
                if (initialValue.length > 0)
                    initialValues.put(characteristic, initialValue);

                for (BluetoothLeDescriptorDefinition descriptor : orEmpty(definition.descriptors()))
                {
                    if (descriptor == null)
                        continue;

                    UUID descriptorUuid = parseUuid(descriptor.uuid());
                    byte[] staticValue = present(descriptor.value()) ? toBytes(descriptor.value().get()) : null;

                    if (staticValue != null && staticValue.length > MAX_ATTRIBUTE_LENGTH)
                        return result(
                            INVALID_ARGUMENT,
                            "Descriptor " + descriptor.uuid() + ": the value is " +
                                staticValue.length + " bytes; an attribute value holds at most " +
                                MAX_ATTRIBUTE_LENGTH);

                    // A static value is read-only, as on Apple and Windows; the
                    // CCCD stays the extension's, whatever value it was given.
                    boolean served = staticValue != null && !CCCD_UUID.equals(descriptorUuid);

                    BluetoothGattDescriptor gattDescriptor = new BluetoothGattDescriptor(
                        descriptorUuid,
                        served
                            ? BluetoothGattDescriptor.PERMISSION_READ
                            : BluetoothGattDescriptor.PERMISSION_READ | BluetoothGattDescriptor.PERMISSION_WRITE);

                    if (served)
                        descriptorValues.put(gattDescriptor, staticValue);

                    characteristic.addDescriptor(gattDescriptor);
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

        // Validated first, as the core does.
        if (leServerSession.get() == null || gattServer == null)
            return result(OPERATION_FAILED, "BLE server is not running");

        // A failure from here on, the stack refusing the add included, is
        // reported through the callback.
        LeServerAddEntry entry = new LeServerAddEntry();
        entry.service = gattService;
        entry.serviceHandle = createLeServerHandles(gattService);
        entry.callback = callback;
        entry.initialValues = initialValues;
        entry.descriptorValues = descriptorValues;

        boolean busy;

        synchronized (leServerAddServiceLock)
        {
            busy = !leServerServiceUuids.add(gattService.getUuid());

            if (!busy)
                leServerAddQueue.addLast(entry);
        }

        if (busy)
        {
            eraseLeServerHandles(entry.serviceHandle);
            return result(BUSY, "A service with UUID " + serviceUuid + " has already been added");
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
            leServerDescriptorValues.clear();
        }

        synchronized (leServerSubscriberLock)
        {
            leServerSubscribers.clear();
        }

        // A notification the stack holds stays in flight until its
        // onNotificationSent, so a new notify waits behind it.
        dropLeServerNotifications(null, true);

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

        clearLeServerHandles();

        return OK;
    }


    @Override
    public BluetoothError bluetooth_le_server_respond_read(
        int request_id,
        ByteBuffer data,
        int offset,
        int size)
    {
        if (!initialized)
            return result(NOT_INITIALIZED, "Bluetooth is not initialized");

        BluetoothError checked = checkLeServerReadRequest(request_id);
        if (checked != OK)
            return checked;

        if (size < 0 || (size > 0 && bufferRangeInvalid(data, offset, size)))
            return result(
                INVALID_ARGUMENT,
                "Invalid buffer offset/size for bluetooth_le_server_respond_read");

        byte[] payload = new byte[0];

        if (size > 0)
        {
            payload = new byte[size];

            ByteBuffer view = data.duplicate();
            view.position(offset);
            view.get(payload, 0, size);
        }

        // The payload is already the slice the caller answers with, from the
        // request's offset on, so the ATT-level offset handed back is 0.
        return answerLeServerReadRequest(request_id, BluetoothGatt.GATT_SUCCESS, payload);
    }


    @Override
    public BluetoothError bluetooth_le_server_respond_read_error(int request_id, BluetoothAttError error_code)
    {
        if (!initialized)
            return result(NOT_INITIALIZED, "Bluetooth is not initialized");

        if (error_code == null || error_code == BluetoothAttError.Success)
            return result(
                INVALID_ARGUMENT,
                "error_code must be a BluetoothAttError other than Success; " +
                    "answer with bluetooth_le_server_respond_read");

        BluetoothError checked = checkLeServerReadRequest(request_id);
        if (checked != OK)
            return checked;

        // The ATT code itself.
        return answerLeServerReadRequest(request_id, error_code.value(), null);
    }


    // Everything is checked before the request is removed, so a bad call can
    // be retried instead of leaving the central to time out.
    private BluetoothError checkLeServerReadRequest(int request_id)
    {
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

        return OK;
    }


    private static final String RESPONSE_REFUSED_MESSAGE =
        "The system refused the response: the request is gone or the link is down";


    private BluetoothError answerLeServerReadRequest(int request_id, int status, byte[] payload)
    {
        LeServerRequestEntry request;

        synchronized (leServerRequestLock)
        {
            request = leServerRequests.remove(request_id);
        }

        if (request == null)
            return result(INVALID_HANDLE, "Unknown or expired request_id");

        BluetoothGattServer server = gattServer;

        if (server == null)
            return result(OPERATION_FAILED, "BLE server is not running");

        try
        {
            if (!server.sendResponse(request.device, request.stackRequestId, status, 0, payload))
                return result(OPERATION_FAILED, RESPONSE_REFUSED_MESSAGE);
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

        discardValue(request.value);

        // A write without response was never waiting on an answer.
        if (!request.responseNeeded)
            return OK;

        // The ATT code itself; Success is GATT_SUCCESS (0).
        int status = error_code.value();

        if (request.batch != null)
        {
            if (!finishLeServerWriteBatchPart(request.batch, status))
                return result(OPERATION_FAILED, RESPONSE_REFUSED_MESSAGE);
            return OK;
        }

        BluetoothGattServer server = gattServer;

        if (server == null)
            return result(OPERATION_FAILED, "BLE server is not running");

        try
        {
            if (!server.sendResponse(
                    request.device,
                    request.stackRequestId,
                    status,
                    0,
                    status == BluetoothGatt.GATT_SUCCESS ? request.writeValue : null))
                return result(OPERATION_FAILED, RESPONSE_REFUSED_MESSAGE);
            return OK;
        }
        catch (Throwable throwable)
        {
            return result(OPERATION_FAILED, throwableMessage(throwable));
        }
    }


    @Override
    public BluetoothError bluetooth_le_server_notify_value(
        long characteristic,
        long connection,
        ByteBuffer data,
        int offset,
        int size)
    {
        if (!initialized)
            return result(NOT_INITIALIZED, "Bluetooth is not initialized");

        BluetoothGattCharacteristic gattCharacteristic = null;

        if (isHandleType(characteristic, HANDLE_TYPE_LE_SERVER_CHARACTERISTIC))
        {
            synchronized (leEntityLock)
            {
                LeCharacteristicEntry entry = leServerCharacteristics.get(characteristic);
                if (entry != null)
                    gattCharacteristic = entry.gattCharacteristic;
            }
        }

        if (gattCharacteristic == null)
            return result(INVALID_HANDLE, "Not a live GATT server characteristic handle");

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

        // A stop clears the handles, so only a stop racing this call gets here.
        if (gattServer == null)
            return result(OPERATION_FAILED, "BLE server is not running");

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
            HashMap<Long, Integer> subscribers = leServerSubscribers.get(gattCharacteristic);

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
        ArrayList<LeConnectionEntry> links = new ArrayList<>();
        LeServerNotifyCall call = new LeServerNotifyCall();

        for (Map.Entry<Long, Integer> target : targets.entrySet())
        {
            LeConnectionEntry entry = getLeConnection(target.getKey());

            if (entry == null || entry.remoteDevice == null || !entry.connected)
                continue;

            LeServerNotification notification = new LeServerNotification();
            notification.device = entry.remoteDevice;
            notification.characteristic = gattCharacteristic;
            notification.value = payload;
            notification.confirm = target.getValue() == SUBSCRIBE_MODE_INDICATE;
            notification.call = call;
            notifications.add(notification);
            links.add(entry);
        }

        if (notifications.isEmpty())
            return OK;

        // Queued whole or not at all; each device with nothing in flight
        // starts sending.
        ArrayList<String> start = new ArrayList<>();

        synchronized (leServerNotifyLock)
        {
            if (leServerNotifyCount >= MAX_QUEUED_NOTIFICATIONS)
                return result(BUSY, "Too many GATT notifications are waiting to be sent");

            for (int i = 0; i < notifications.size(); i++)
            {
                // A disconnect marks the link gone before it drops the
                // device's notifications under this lock, so one that slipped
                // past the check above is not queued for a dead link.
                if (!links.get(i).connected)
                    continue;

                LeServerNotification notification = notifications.get(i);
                String address = safeAddress(notification.device);
                ArrayDeque<LeServerNotification> queue = leServerNotifyQueues.get(address);

                if (queue == null)
                {
                    queue = new ArrayDeque<>();
                    leServerNotifyQueues.put(address, queue);
                }

                queue.addLast(notification);
                call.remaining++;

                if (!leServerNotifyInFlight.containsKey(address) && !start.contains(address))
                    start.add(address);
            }

            if (call.remaining > 0)
                leServerNotifyCount++;
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

                // A device is announced only to a Classic scan this game
                // runs: another app's discovery reaches this receiver too.
                if (BluetoothDevice.ACTION_FOUND.equals(action) ||
                    BluetoothDevice.ACTION_NAME_CHANGED.equals(action))
                {
                    ScanSession session = classicScan.get();
                    BluetoothDevice device = deviceExtra(intent);

                    if (session == null || device == null)
                        return;

                    if (BluetoothDevice.ACTION_FOUND.equals(action))
                    {
                        short rssi = intent.getShortExtra(
                            BluetoothDevice.EXTRA_RSSI,
                            Short.MIN_VALUE);

                        upsertClassicSighting(session, device, rssi, rssi != Short.MIN_VALUE, safeName(device));
                        return;
                    }

                    // A name resolved after the device answered: on_updated,
                    // for a device this scan has found.
                    String name = intent.getStringExtra(BluetoothDevice.EXTRA_NAME);
                    String key = deviceKey(TRANSPORT_CLASSIC, device);
                    boolean found;

                    synchronized (deviceLock)
                    {
                        Long handle = deviceById.get(key);
                        found = handle != null && session.announcedAt.containsKey(handle);
                    }

                    if (found)
                        upsertClassicSighting(session, device, 0, false, name != null ? name : safeName(device));
                }
                else if (BluetoothAdapter.ACTION_DISCOVERY_STARTED.equals(action))
                {
                    if (classicScan.get() != null)
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

                    // The inquiry ran its course.
                    ScanSession session = classicScan.getAndSet(null);

                    if (session != null)
                        invoke(session.onStopped, OK, "");
                }
            }
            catch (Throwable ignored)
            {
            }
        }
    };


    private void upsertClassicSighting(
        ScanSession session,
        BluetoothDevice device,
        int rssi,
        boolean hasRssi,
        String name)
    {
        upsertDevice(
            TRANSPORT_CLASSIC,
            deviceKey(TRANSPORT_CLASSIC, device),
            deviceId(TRANSPORT_CLASSIC, device),
            name,
            safeAddress(device),
            rssi,
            hasRssi,
            true,
            device,
            session,
            null,
            null);
    }


    private boolean ensureClassicReceiver()
    {
        if (receiverRegistered)
            return true;

        IntentFilter filter = new IntentFilter();
        filter.addAction(BluetoothDevice.ACTION_FOUND);
        filter.addAction(BluetoothDevice.ACTION_NAME_CHANGED);
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
    public BluetoothError bluetooth_classic_scan_start(
        GMFunction on_found,
        Optional<GMFunction> on_updated,
        Optional<GMFunction> on_stopped)
    {
        BluetoothError ready = requireAdapter();
        if (ready != OK)
            return ready;

        if (classicScan.get() != null)
            return result(BUSY, SCAN_BUSY_MESSAGE);

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

        if (!ensureClassicReceiver())
            return result(OPERATION_FAILED, RECEIVER_FAILED_MESSAGE);

        // No throttle: every later answer is an on_updated.
        ScanSession session = new ScanSession(
            TRANSPORT_CLASSIC,
            on_found,
            orNull(on_updated),
            orNull(on_stopped),
            0,
            null);

        try
        {
            if (adapter.isDiscovering())
                adapter.cancelDiscovery();

            // Armed by this discovery's ACTION_DISCOVERY_STARTED.
            classicDiscoveryStarted = false;

            if (!classicScan.compareAndSet(null, session))
                return result(BUSY, SCAN_BUSY_MESSAGE);

            if (!adapter.startDiscovery())
            {
                classicScan.compareAndSet(session, null);
                return result(
                    OPERATION_FAILED,
                    "Android Bluetooth discovery could not start");
            }

            return OK;
        }
        catch (SecurityException exception)
        {
            classicScan.compareAndSet(session, null);
            return result(
                PERMISSION_DENIED,
                throwableMessage(exception));
        }
        catch (Throwable throwable)
        {
            classicScan.compareAndSet(session, null);
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

        // Fires no on_stopped: the game asked for it.
        if (classicScan.getAndSet(null) != null)
            cancelDiscoveryQuietly();

        return OK;
    }


    @Override
    public boolean bluetooth_classic_scan_is_running()
    {
        return initialized && classicScan.get() != null;
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
            pendingScanDevices.clear();

            // A running scan finds every device again, and holds it back
            // until it matches anew.
            for (ScanSession session : new ScanSession[] { leScan.get(), classicScan.get() })
            {
                if (session != null)
                {
                    session.announcedAt.clear();
                    session.matched.clear();
                }
            }
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
    public Optional<String> bluetooth_device_get_address(long device)
    {
        DeviceEntry entry = copyDevice(device);
        return entry != null && entry.addressAvailable
            ? Optional.of(entry.address.toUpperCase(java.util.Locale.ROOT))
            : Optional.<String>empty();
    }


    @Override
    public Optional<Integer> bluetooth_device_get_rssi(long device)
    {
        DeviceEntry entry = copyDevice(device);
        return entry != null && entry.rssiAvailable
            ? Optional.of(entry.rssi)
            : Optional.<Integer>empty();
    }


    @Override
    public Optional<Integer> bluetooth_device_get_last_seen_age(long device)
    {
        DeviceEntry entry = copyDevice(device);

        if (entry == null || entry.lastSeenMs < 0)
            return Optional.empty();

        long age = SystemClock.elapsedRealtime() - entry.lastSeenMs;
        return Optional.of((int) Math.min(Math.max(age, 0), Integer.MAX_VALUE));
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


    @Override
    public long bluetooth_device_from_id(String id)
    {
        if (!initialized)
        {
            setLastError(NOT_INITIALIZED, NOT_INITIALIZED_MESSAGE);
            return 0;
        }

        String key = id != null ? id : "";
        int transport;
        int addressType = -1;
        String address;

        if (key.startsWith(ID_PREFIX_CLASSIC))
        {
            transport = TRANSPORT_CLASSIC;
            address = key.substring(ID_PREFIX_CLASSIC.length());
        }
        else if (key.startsWith(ID_PREFIX_LE_APPLE))
        {
            setLastError(INVALID_ARGUMENT, "An Apple peripheral identifier cannot be used on Android: " + key);
            return 0;
        }
        else if (key.startsWith(ID_PREFIX_LE_PUBLIC))
        {
            transport = TRANSPORT_LE;
            addressType = LE_ADDRESS_PUBLIC;
            address = key.substring(ID_PREFIX_LE_PUBLIC.length());
        }
        else if (key.startsWith(ID_PREFIX_LE_RANDOM))
        {
            transport = TRANSPORT_LE;
            addressType = LE_ADDRESS_RANDOM;
            address = key.substring(ID_PREFIX_LE_RANDOM.length());
        }
        else if (key.startsWith(ID_PREFIX_LE))
        {
            transport = TRANSPORT_LE;
            address = key.substring(ID_PREFIX_LE.length());
        }
        else
        {
            setLastError(INVALID_ARGUMENT, "Not a Bluetooth device id: " + key);
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

        String cacheKey = deviceKey(transport, address, null);

        synchronized (deviceLock)
        {
            Long existing = deviceById.get(cacheKey);
            if (existing != null)
                return existing;
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
            if (addressType >= 0 && Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU)
                device = current.getRemoteLeDevice(
                    address,
                    addressType == LE_ADDRESS_RANDOM
                        ? BluetoothDevice.ADDRESS_TYPE_RANDOM
                        : BluetoothDevice.ADDRESS_TYPE_PUBLIC);
            else
                device = current.getRemoteDevice(address);
        }
        catch (Throwable throwable)
        {
            setLastError(INVALID_ARGUMENT, throwableMessage(throwable));
            return 0;
        }

        // The id is the one given, canonical: getRemoteDevice assumes a public
        // address, which an untyped id does not say. Not discovered, so no
        // scan hears of it.
        String canonicalId = transport == TRANSPORT_CLASSIC
            ? ID_PREFIX_CLASSIC + address
            : (addressType == LE_ADDRESS_PUBLIC
                ? ID_PREFIX_LE_PUBLIC
                : addressType == LE_ADDRESS_RANDOM ? ID_PREFIX_LE_RANDOM : ID_PREFIX_LE) + address;

        long handle = upsertDevice(
            transport,
            cacheKey,
            canonicalId,
            safeName(device),
            address,
            0,
            false,
            true,
            device,
            null,
            null,
            null);

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


    // A device a query found enters the cache without a scan callback; the
    // handles keep the stack's order, each once.
    private void addQueriedDevice(int transport, BluetoothDevice device, ArrayList<Double> handles)
    {
        long handle = upsertDevice(transport, device, 0, false, true);

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
        if (service_uuids == null || service_uuids.isEmpty())
            return result(INVALID_ARGUMENT, "service_uuids must list at least one UUID");

        for (String uuid : service_uuids)
        {
            if (!isValidUuid(uuid))
                return result(INVALID_ARGUMENT, "Invalid UUID: " + uuid);
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
        Optional<BluetoothConnectOptions> options,
        GMFunction on_data,
        GMFunction on_disconnected,
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

        final long timeoutMs = connectTimeoutMs(options);

        if (timeoutMs < 0)
            return 0;

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
        entry.onData = on_data;
        entry.onDisconnected = on_disconnected;
        entry.connectPending.set(true);

        // One window from the call, pairing prompt included: closing the
        // socket ends the attempt, and the thread drops out on manualClosing.
        entry.connectTimeout = () ->
        {
            if (generation.get() != workerGeneration)
                return;

            if (dispatchConnectResult(entry, TIMEOUT, "Connection timed out"))
            {
                entry.manualClosing = true;
                closeQuietly(entry.socket);
            }
        };

        timeoutHandler.postDelayed(entry.connectTimeout, timeoutMs);

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
    // loop reports the end through on_disconnected as for any socket
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


    // Hands each chunk read to GML as a value until the link ends. While the
    // connection's values hold MAX_QUEUED_RECEIVE_BYTES the loop stops
    // reading, so the peer is held back by RFCOMM flow control instead of the
    // values growing; a fetch, release or expiry, a disconnect and shutdown
    // wake it.
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
                String message = REMOTE_DISCONNECTED_MESSAGE;

                try
                {
                    InputStream input = socket.getInputStream();
                    byte[] buffer = new byte[4096];

                    while (
                        initialized &&
                        generation.get() == workerGeneration)
                    {
                        // Values expire only when one is stored, fetched or
                        // released, so a paused reader whose game never
                        // fetches runs the expiry itself on a timer: an
                        // expired chunk still ends the link.
                        for (;;)
                        {
                            boolean paused;

                            synchronized (entry.receiveLock)
                            {
                                paused =
                                    entry.receiveOutstanding >= MAX_QUEUED_RECEIVE_BYTES &&
                                    !entry.manualClosing &&
                                    !entry.receiveExpired.get() &&
                                    entry.endError == null &&
                                    initialized &&
                                    generation.get() == workerGeneration;

                                if (paused)
                                    entry.receiveLock.wait(RECEIVE_PAUSED_EXPIRY_CHECK_MILLIS);
                            }

                            if (!paused)
                                break;

                            expireStoredValues();
                        }

                        if (entry.manualClosing || entry.receiveExpired.get() || entry.endError != null)
                            break;

                        int count = input.read(buffer);

                        if (count < 0)
                            break;

                        if (count == 0)
                            continue;

                        dispatchClassicData(
                            entry,
                            Arrays.copyOf(buffer, count));
                    }
                }
                catch (IOException exception)
                {
                    // A peer's clean close never reads -1: AOSP throws
                    // "bt socket closed, read return: -1" (R2-157).
                    String text = throwableMessage(exception);
                    if (!text.startsWith("bt socket closed"))
                        message = "read failed: " + text;
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

                // An expired value or the radio ended the link, whatever the
                // read saw.
                if (entry.endError != null)
                {
                    error = entry.endError;
                    message = entry.endMessage;
                }
                else if (entry.receiveExpired.get())
                {
                    error = DISCONNECTED;
                    message = RECEIVE_EXPIRED_MESSAGE;
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
    // ConnectionFailed; an established link fires no on_disconnected.
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
            // The peer already hung up.
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

        if (bufferRangeInvalid(data, offset, size))
            return result(
                INVALID_ARGUMENT,
                "Invalid buffer offset/size for bluetooth_classic_send");

        BluetoothSocket socket = entry.socket;

        if (socket == null || !entry.connected || entry.finished)
            return result(
                DISCONNECTED,
                "Classic connection is not connected");

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

            if ((long) entry.sendQueuedBytes + size > MAX_QUEUED_SEND_BYTES)
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


    // =========================================================================
    // Classic RFCOMM server
    // =========================================================================

    @Override
    public BluetoothError bluetooth_classic_server_start(
        String name,
        String service_uuid,
        GMFunction on_connected,
        GMFunction on_data,
        GMFunction on_disconnected,
        Optional<GMFunction> on_stopped)
    {
        BluetoothError ready = requireAdapter();
        if (ready != OK)
            return ready;

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

        if (!hasConnectPermission())
            return result(
                PERMISSION_DENIED,
                "Bluetooth connect permission is not granted");

        if (!adapterEnabled())
            return result(
                BLUETOOTH_DISABLED,
                "Bluetooth is disabled");

        final ClassicServer server = new ClassicServer(
            name == null || name.isEmpty() ? "GMBluetooth RFCOMM" : name,
            uuid,
            on_connected,
            on_data,
            on_disconnected,
            orNull(on_stopped));

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
                    // Still current, it ended on its own and says why.
                    server.running = false;
                    closeQuietly(server.socket);

                    boolean ended;

                    synchronized (classicServerLock)
                    {
                        ended = currentServer == server;

                        if (ended)
                            currentServer = null;
                    }

                    if (ended && initialized && generation.get() == workerGeneration)
                        invoke(server.onStopped, server.endError, server.endMessage);
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
                acceptConnection(server, socket, workerGeneration);
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

        if (!serverActive(server, workerGeneration))
            return false;

        if (!adapterEnabled())
        {
            server.endError = BLUETOOTH_DISABLED;
            server.endMessage = RADIO_OFF_MESSAGE;
            return false;
        }

        try
        {
            server.socket = adapter.listenUsingRfcommWithServiceRecord(server.name, server.uuid);
        }
        catch (Throwable throwable)
        {
            server.endMessage = throwableMessage(throwable);
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


    private void acceptConnection(ClassicServer server, BluetoothSocket socket, long workerGeneration)
    {
        BluetoothDevice remote = socket.getRemoteDevice();

        long device = upsertDevice(TRANSPORT_CLASSIC, remote, 0, false, true);

        long connection = createConnection(device);
        ConnectionEntry entry = getConnection(connection);

        if (entry == null)
        {
            closeQuietly(socket);
            return;
        }

        entry.socket = socket;
        entry.onData = server.onData;
        entry.onDisconnected = server.onDisconnected;
        entry.connected = true;

        invoke(
            server.onConnected,
            (double) connection,
            (double) device);

        startReadLoop(entry, socket, workerGeneration);
    }


    // Stops the running server, if any, and hands it back; whoever takes it
    // decides whether its on_stopped fires.
    private ClassicServer takeClassicServer()
    {
        ClassicServer server;

        synchronized (classicServerLock)
        {
            server = currentServer;
            currentServer = null;
        }

        if (server == null)
            return null;

        server.running = false;
        closeQuietly(server.socket);
        return server;
    }


    @Override
    public BluetoothError bluetooth_classic_server_stop()
    {
        if (!initialized)
            return result(
                NOT_INITIALIZED,
                "Bluetooth is not initialized");

        takeClassicServer();
        return OK;
    }


    @Override
    public boolean bluetooth_classic_server_is_running()
    {
        ClassicServer server = currentServer;
        return initialized && server != null && server.running;
    }


    // Android caps a discoverable request at an hour and has no indefinite
    // mode, so 0 or less is the default, on every platform.
    private static final int DISCOVERABLE_DEFAULT_SECONDS = 120;
    private static final int DISCOVERABLE_MAX_SECONDS = 3600;


    @Override
    public BluetoothError bluetooth_classic_discoverable_start(
        int duration_seconds,
        GMFunction callback,
        Optional<GMFunction> on_stopped)
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
            return result(OPERATION_FAILED, NO_ACTIVITY_MESSAGE);

        DiscoverableWindow window = new DiscoverableWindow();
        window.callback = callback;
        window.onStopped = on_stopped.orElse(null);
        window.requestedSeconds = requestedDuration;

        if (scanModeIsDiscoverable() || !discoverable.compareAndSet(null, window))
            return result(BUSY, "The device is already discoverable");

        // getScanMode and the broadcast need BLUETOOTH_SCAN from Android 12;
        // without it the clock alone ends the window.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S || hasScanPermission())
        {
            synchronized (enableLock)
            {
                discoverableReceiverRegistered = registerSystemReceiver(
                    discoverableReceiver,
                    new IntentFilter(BluetoothAdapter.ACTION_SCAN_MODE_CHANGED));
            }
        }

        // For a result, so the answer reaches onActivityResult.
        try
        {
            Intent discoverableIntent =
                new Intent(BluetoothAdapter.ACTION_REQUEST_DISCOVERABLE);
            discoverableIntent.putExtra(
                BluetoothAdapter.EXTRA_DISCOVERABLE_DURATION,
                requestedDuration);
            current.startActivityForResult(discoverableIntent, REQUEST_CODE_DISCOVERABLE);
        }
        catch (Throwable throwable)
        {
            // A pre-flight failure fires nothing.
            if (discoverable.compareAndSet(window, null))
                releaseDiscoverableWindow(window);

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
}
