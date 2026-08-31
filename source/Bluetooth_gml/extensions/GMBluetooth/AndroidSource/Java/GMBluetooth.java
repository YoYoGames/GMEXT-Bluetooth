package ${YYAndroidPackageName};

import ${YYAndroidPackageName}.GMExtWire.GMFunction;

import android.Manifest;
import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothServerSocket;
import android.bluetooth.BluetoothSocket;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanResult;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.os.Build;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentLinkedQueue;
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

    private static final int REQUEST_CODE_BLUETOOTH = 0xB710;


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
    // GML callbacks
    // =========================================================================

    private volatile GMFunction callbackDeviceFound = null;
    private volatile GMFunction callbackScanStopped = null;
    private volatile GMFunction callbackClassicClientConnected = null;
    private volatile GMFunction callbackClassicData = null;
    private volatile GMFunction callbackClassicDisconnected = null;


    // =========================================================================
    // Event queue
    // =========================================================================

    private static final int EVENT_DEVICE_FOUND = 1;
    private static final int EVENT_SCAN_STOPPED = 2;
    private static final int EVENT_CLASSIC_CONNECTED = 3;
    private static final int EVENT_CLASSIC_CLIENT_CONNECTED = 4;
    private static final int EVENT_CLASSIC_DATA = 5;
    private static final int EVENT_CLASSIC_DISCONNECTED = 6;

    private static final class Event
    {
        int type;
        int transport = TRANSPORT_UNKNOWN;
        int error = OK;
        int value = 0;
        long device = 0;
        long connection = 0;
        String message = "";
        GMFunction connectCallback = null;
    }

    private final ConcurrentLinkedQueue<Event> events =
        new ConcurrentLinkedQueue<>();


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
            Event event = new Event();
            event.type = EVENT_DEVICE_FOUND;
            event.transport = transport;
            event.device = handle;
            events.offer(event);
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


    private void enqueueScanStopped(int transport, int error, String message)
    {
        Event event = new Event();
        event.type = EVENT_SCAN_STOPPED;
        event.transport = transport;
        event.error = error;
        event.message = message != null ? message : "";
        events.offer(event);
    }


    private void enqueueConnectResult(
        long connection,
        long device,
        int error,
        String message,
        GMFunction callback)
    {
        Event event = new Event();
        event.type = EVENT_CLASSIC_CONNECTED;
        event.transport = TRANSPORT_CLASSIC;
        event.connection = connection;
        event.device = device;
        event.error = error;
        event.message = message != null ? message : "";
        event.connectCallback = callback;
        events.offer(event);
    }


    private void enqueueDisconnected(
        long connection,
        int error,
        String message)
    {
        Event event = new Event();
        event.type = EVENT_CLASSIC_DISCONNECTED;
        event.transport = TRANSPORT_CLASSIC;
        event.connection = connection;
        event.error = error;
        event.message = message != null ? message : "";
        events.offer(event);
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

        Event event = new Event();
        event.type = EVENT_CLASSIC_DATA;
        event.transport = TRANSPORT_CLASSIC;
        event.connection = connection;
        event.value = available;
        events.offer(event);
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

        events.clear();

        callbackDeviceFound = null;
        callbackScanStopped = null;
        callbackClassicClientConnected = null;
        callbackClassicData = null;
        callbackClassicDisconnected = null;

        adapter = null;
        setLastError(OK, "");
    }


    @Override
    public int bluetooth_update()
    {
        int dispatched = 0;

        while (true)
        {
            Event event = events.poll();

            if (event == null)
                break;

            switch (event.type)
            {
                case EVENT_DEVICE_FOUND:
                    invoke(
                        callbackDeviceFound,
                        (double) event.device);
                    break;

                case EVENT_SCAN_STOPPED:
                    if (event.error != OK)
                        setLastError(event.error, event.message);

                    invoke(
                        callbackScanStopped,
                        event.error,
                        event.message);
                    break;

                case EVENT_CLASSIC_CONNECTED:
                {
                    ConnectionEntry entry = getConnection(event.connection);

                    if (event.error == OK)
                    {
                        if (entry != null)
                            entry.connected = true;
                    }
                    else
                    {
                        eraseConnection(event.connection);
                        setLastError(event.error, event.message);
                    }

                    invoke(
                        event.connectCallback,
                        event.error,
                        event.message,
                        (double) event.connection,
                        (double) event.device);
                    break;
                }

                case EVENT_CLASSIC_CLIENT_CONNECTED:
                {
                    ConnectionEntry entry = getConnection(event.connection);

                    if (entry != null)
                        entry.connected = true;

                    invoke(
                        callbackClassicClientConnected,
                        (double) event.connection,
                        (double) event.device);
                    break;
                }

                case EVENT_CLASSIC_DATA:
                    invoke(
                        callbackClassicData,
                        (double) event.connection,
                        event.value);
                    break;

                case EVENT_CLASSIC_DISCONNECTED:
                    invoke(
                        callbackClassicDisconnected,
                        (double) event.connection,
                        event.error,
                        event.message);

                    eraseConnection(event.connection);

                    if (event.error != OK)
                        setLastError(event.error, event.message);
                    break;

                default:
                    break;
            }

            ++dispatched;
        }

        return dispatched;
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

            enqueueScanStopped(
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

        enqueueScanStopped(TRANSPORT_LE, OK, "");
        return result(OK, "");
    }


    @Override
    public boolean bluetooth_le_scan_is_running()
    {
        return initialized && leScanning.get();
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
                    enqueueScanStopped(
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
            enqueueScanStopped(
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
                            enqueueConnectResult(
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

                    enqueueConnectResult(
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
                        enqueueConnectResult(
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
                        enqueueConnectResult(
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
                        enqueueConnectResult(
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
                        enqueueDisconnected(
                            connection,
                            OK,
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
                        enqueueDisconnected(
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
                        enqueueDisconnected(
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


    @Override
    public int bluetooth_classic_send(
        long connection,
        ByteBuffer data,
        int offset,
        int size)
    {
        // TODO: Implement GameMaker ByteBuffer -> RFCOMM byte transfer.
        //
        // Intentionally left unimplemented until the project's preferred
        // GameMaker buffer convention is supplied.
        //
        // The BluetoothSocket transport itself is implemented and ready.
        setLastError(
            NOT_SUPPORTED,
            "TODO: GameMaker buffer send bridge not implemented yet");

        return NOT_SUPPORTED;
    }


    @Override
    public int bluetooth_classic_receive(
        long connection,
        ByteBuffer out_data,
        int offset,
        int max_size)
    {
        // TODO: Implement RFCOMM receive queue -> GameMaker ByteBuffer transfer.
        //
        // Incoming RFCOMM bytes are already queued by startReadLoop() and
        // bluetooth_classic_receive_available() reports their byte count.
        // Only the final GameMaker buffer-copy convention is deferred.
        setLastError(
            NOT_SUPPORTED,
            "TODO: GameMaker buffer receive bridge not implemented yet");

        return 0;
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

                        Event event = new Event();
                        event.type =
                            EVENT_CLASSIC_CLIENT_CONNECTED;
                        event.transport =
                            TRANSPORT_CLASSIC;
                        event.connection =
                            connection;
                        event.device =
                            device;
                        events.offer(event);

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
}
