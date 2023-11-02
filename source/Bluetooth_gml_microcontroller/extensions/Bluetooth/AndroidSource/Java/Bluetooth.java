package ${YYAndroidPackageName};

import ${YYAndroidPackageName}.R;
import com.yoyogames.runner.RunnerJNILib;

import android.app.Activity;
import android.widget.Toast;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothServerSocket;
import android.bluetooth.BluetoothGattServerCallback;

import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattDescriptor;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanRecord;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.BluetoothLeAdvertiser;
import android.bluetooth.le.AdvertisingSetParameters;
import android.bluetooth.le.AdvertisingSetCallback;
import android.bluetooth.le.AdvertiseData;
import android.bluetooth.le.AdvertisingSet;
import android.bluetooth.le.AdvertiseCallback;
import android.bluetooth.le.AdvertiseSettings;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothGattServer;
import android.bluetooth.BluetoothStatusCodes;

import android.os.ParcelUuid;

import java.nio.charset.Charset;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Build;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import android.content.pm.PackageManager;
import android.Manifest;

import java.util.HashMap;
import java.util.Set;
import java.util.UUID;
import java.util.Iterator;
import java.util.concurrent.ConcurrentHashMap;

import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.function.BiConsumer;
import java.util.function.Consumer;
import java.util.function.Supplier;
import java.util.logging.LogManager;

import android.util.Log;
import java.util.List;
import java.util.Map;
import java.lang.Thread;
import java.lang.Exception;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

// JSON imports
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.nio.ByteBuffer;

import java.util.Arrays;
import java.util.Base64;

import android.os.Handler;
import java.nio.charset.StandardCharsets;
import java.security.cert.TrustAnchor;
import java.util.Map.Entry;

public class Bluetooth extends RunnerSocial {

	public Bluetooth() { }

	private static Activity activity = RunnerActivity.CurrentActivity;

	BluetoothManager bluetoothManager = (BluetoothManager) activity.getSystemService(Context.BLUETOOTH_SERVICE);

	private AtomicInteger asyncTokenGenerator = new AtomicInteger(0);
	private AtomicInteger socketIdGenerator = new AtomicInteger(0);

	private static final int REQUEST_ENABLE_BT = 2134;

	private static final String LOG_TAG = "yoyo";

	private static final int EVENT_OTHER_SOCIAL = 70;

	private static final double TRUE = 1.0;
	private static final double FALSE = 0.0;
	
	public void notifyOperation(String functionName, Map<String, Object> extraParams) {
		
		int dsMapIndex = RunnerJNILib.jCreateDsMap(null, null, null);
		RunnerJNILib.DsMapAddString(dsMapIndex, "type", functionName);

		if (extraParams != null) {
			for (Map.Entry<String, Object> entry : extraParams.entrySet()) {
				String key = entry.getKey();
				Object value = entry.getValue();

				if (value instanceof String) {
					RunnerJNILib.DsMapAddString(dsMapIndex, key, (String) value);
				} else if (value instanceof Integer) {
					RunnerJNILib.DsMapAddDouble(dsMapIndex, key, ((Integer) value).doubleValue());
				} else if (value instanceof Long) {
					RunnerJNILib.DsMapAddDouble(dsMapIndex, key, ((Long) value).doubleValue());
				} else if (value instanceof Boolean) {
					RunnerJNILib.DsMapAddDouble(dsMapIndex, key, ((Boolean) value) ? TRUE : FALSE );
				} else if (value instanceof Double) {
					RunnerJNILib.DsMapAddDouble(dsMapIndex, key, (Double) value);
				} else if (value instanceof byte[]) {
					RunnerJNILib.DsMapAddString(dsMapIndex, key, Base64.getEncoder().encodeToString((byte[]) value));
				} else if (value instanceof UUID) {
					RunnerJNILib.DsMapAddString(dsMapIndex, key, ((UUID) value).toString());
				} else if (value instanceof JSONArray) {
					RunnerJNILib.DsMapAddString(dsMapIndex, key, ((JSONArray) value).toString());
				} else if (value instanceof JSONObject) {
					RunnerJNILib.DsMapAddString(dsMapIndex, key, ((JSONObject) value).toString());
				} else {
					Log.i(LOG_TAG, "notifyAsyncOperation :: value type not supported for key '" + key + "', ignoring!");
				}
			}
		}

		RunnerJNILib.CreateAsynEventWithDSMap(dsMapIndex, EVENT_OTHER_SOCIAL);
	}

	public void notifyAsyncOperation(String functionName, int asyncId, Map<String, Object> extraParams) {

		Map<String, Object> params = new HashMap<>();
		params.put("async_id", asyncId);
		
		if (extraParams != null) {
			params.putAll(extraParams);
		}

		notifyOperation(functionName, params);
	}

	public void notifyAsyncOperationError(String functionName, int asyncId, int errorCode, Map<String, Object> extraParams) {

		Map<String, Object> params = new HashMap<>();
		params.put("success", false);
		params.put("error_code", errorCode);

		if (extraParams != null) {
			params.putAll(extraParams);
		}

		notifyAsyncOperation(functionName, asyncId, params);
	}

	public void notifyAsyncOperationSuccess(String functionName, int asyncId, Map<String, Object> extraParams) {
		
		Map<String, Object> params = new HashMap<>();
		params.put("success", true);
	
		if (extraParams != null) {
			params.putAll(extraParams);
		}

		notifyAsyncOperation(functionName, asyncId, params);
	}

	// INTERNAL

	public double bt_init() {
		registerBroadcastReceiver();
		return 0;
	}

	public double bt_end() {
		unregisterBroadcastReceiver();
		return 0;
	}

	//#region Bluetooth Core

	public String bt_get_address() {
		BluetoothAdapter bluetoothAdapter = getBluetoothAdapter("bt_get_address");
		if (bluetoothAdapter == null)
			return "";
	
		return bluetoothAdapter.getAddress();
	}
	
	public String bt_get_name() {
		BluetoothAdapter bluetoothAdapter = getBluetoothAdapter("bt_get_name");
		if (bluetoothAdapter == null)
			return "";
	
		return bluetoothAdapter.getName();
	}
	
	public String bt_get_paired_devices() {
		BluetoothAdapter bluetoothAdapter = getBluetoothAdapter("bt_get_paired_devices");
		if (bluetoothAdapter == null)
			return "[]";
	
		Set<BluetoothDevice> pairedDevices = bluetoothAdapter.getBondedDevices();
		if (pairedDevices == null) {
			Log.i(LOG_TAG, "bt_get_paired_devices :: Unable to retrieve bounded devices.");
			return "[]";
		}
	
		JSONArray devicesJsonArray = new JSONArray();
		// Try to build return data
		for (BluetoothDevice device : pairedDevices) {
			devicesJsonArray.put(createBluetoothDeviceJson(device));
		}
	
		// Finally return the JSONArray as a string
		return devicesJsonArray.toString();
	}	

	public double bt_is_enabled() {
		return isBluetoothEnabled("bt_is_enabled") ? TRUE : FALSE;
	}

	public double bt_request_enable() {
		if (!isBluetoothSupported("bt_request_enable"))
			return FALSE;
	
		Intent enableBluetoothIntent = new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE);
		activity.startActivityForResult(enableBluetoothIntent, REQUEST_ENABLE_BT);

		return TRUE;
	}
	
	public void onActivityResult(int requestCode, int resultCode, Intent data) {
		switch (requestCode) {
			case REQUEST_ENABLE_BT:
				notifyOperation("bt_request_enable", Map.of(
					"success", resultCode == Activity.RESULT_OK
				));
				break;
		}
	}
	
	//#endregion

	//#region Bluetooth Classic

	private BroadcastReceiver bluetoothEventReceiver = null;

	private ConcurrentHashMap<Integer, BluetoothSocket> activeSockets = new ConcurrentHashMap<>();
	private ConcurrentHashMap<Integer, ConcurrentLinkedQueue<byte[]>> socketDataQueues = new ConcurrentHashMap<>();
	private ConcurrentHashMap<Integer, Thread> activeThreads = new ConcurrentHashMap<>();
	
	private BluetoothServerSocket serverSocket = null;
	private volatile Thread serverThread = null;
	private volatile boolean isServerRunning = false;
	
	public double bt_classic_is_supported() {
		return isBluetoothSupported("bt_classic_is_supported") ? TRUE : FALSE;
	}

	public double bt_classic_scan_start() {
		BluetoothAdapter bluetoothAdapter = getBluetoothAdapter("bt_classic_scan_start");
		if (bluetoothAdapter == null)
			return -1;
	
		if (bluetoothAdapter.isDiscovering()) {
			Log.i(LOG_TAG, "bt_classic_scan_start :: Discovery has already started.");
			return -1;
		}
	
		int asyncId = asyncTokenGenerator.getAndIncrement();

		RunnerActivity.ViewHandler.post(new Runnable() {
			public void run() {
				try {
					Boolean discoveryStarted = bluetoothAdapter.startDiscovery();
	
					if (!discoveryStarted) {
						notifyAsyncOperationError("bt_classic_scan_start", asyncId, ERROR_OPERATION_NOT_STARTED, null);
					} else {
						notifyAsyncOperationSuccess("bt_classic_scan_start", asyncId, null);
					}
	
				} catch (Exception e) {
					notifyAsyncOperationError("bt_classic_scan_start", asyncId, e.hashCode(), null);
				}
			}
		});
	
		return asyncId;
	}
	
	public double bt_classic_scan_is_active() {
		BluetoothAdapter bluetoothAdapter = getBluetoothAdapter("bt_classic_scan_is_active");
		if (bluetoothAdapter == null)
			return FALSE;
	
		return bluetoothAdapter.isDiscovering() ? TRUE : FALSE;
	}
	
	public double bt_classic_scan_stop() {
		BluetoothAdapter bluetoothAdapter = getBluetoothAdapter("bt_classic_scan_stop");
		if (bluetoothAdapter == null)
			return -1;
	
		if (!bluetoothAdapter.cancelDiscovery()) return -1;

		int asyncId = asyncTokenGenerator.getAndIncrement();

		notifyAsyncOperationSuccess("bt_classic_scan_stop", asyncId, null);

		return asyncId;
	}
	
	public double bt_classic_discoverability_enable(double seconds) {
		RunnerActivity.ViewHandler.post(new Runnable() {
			public void run() {
				Intent discoverableIntent = new Intent(BluetoothAdapter.ACTION_REQUEST_DISCOVERABLE);
				discoverableIntent.putExtra(BluetoothAdapter.EXTRA_DISCOVERABLE_DURATION, (int) seconds);
				activity.startActivity(discoverableIntent);
			}
		});
		return TRUE;
	}
	
	public double bt_classic_discoverability_is_active() {
		BluetoothAdapter bluetoothAdapter = getBluetoothAdapter("bt_classic_discoverability_is_active");
		
		if (bluetoothAdapter == null) return FALSE;
	
		int mode = bluetoothAdapter.getScanMode();
		return mode == bluetoothAdapter.SCAN_MODE_CONNECTABLE_DISCOVERABLE ? TRUE: FALSE;
	}
	
	public double bt_classic_server_start(String name, String uuidString, double insecure) {
		BluetoothServerSocket temporarySocket = null;
	
		BluetoothAdapter bluetoothAdapter = getBluetoothAdapter("bt_classic_server_start");
		if (bluetoothAdapter == null) return -1;
	
		if (isServerRunning) {
			Log.i(LOG_TAG, "bt_classic_server_start :: Server is already running");
			return -1; // Server already running
		}
	
		UUID uuid = getUUIDFromString(uuidString, "bt_classic_server_start");
		if (uuid == null) return -1;

		try {	
			if (insecure > .5) {
				temporarySocket = bluetoothAdapter.listenUsingInsecureRfcommWithServiceRecord(name, uuid);
			} else {
				temporarySocket = bluetoothAdapter.listenUsingRfcommWithServiceRecord(name, uuid);
			}
		} catch (IOException e) {
			Log.e(LOG_TAG, "bt_classic_server_start :: Error creating socket", e);
			return -1; // Socket creation failed
		}
	
		int asyncId = asyncTokenGenerator.getAndIncrement();

		notifyAsyncOperationSuccess("bt_classic_server_start", asyncId, null);

		serverSocket = temporarySocket;
		isServerRunning = true;
	
		// Store the server thread this will allow to interrupt it later (if needed)
		serverThread = new Thread(new Runnable() {
			public void run() {
				while (isServerRunning) {
	
					Map<String, Object> extraParams = new HashMap<String,Object>();
					
					try {
						BluetoothSocket socket = serverSocket.accept();
	
						if (socket != null) {

							int socketId = socketIdGenerator.getAndIncrement();

							extraParams.put("success", true);
							extraParams.put("socket_id", socketId);
							handleBluetoothSocket(socket, socketId);
	
						} else {
							extraParams.put("success", false);
						}
					} catch (Exception e) {
						extraParams.put("success", false);
					}
	
					notifyOperation("bt_classic_server_accept", extraParams);
				}
			}
		});
		serverThread.start();

		return asyncId;
	}
	
	public double bt_classic_server_stop() {
		try {
			// Set flag to false to signal the server thread to stop accepting connections
			isServerRunning = false;
	
			// Interrupt the server thread to stop accepting new connections immediately
			if (serverThread != null) {
				serverThread.interrupt();
			}
	
			// Close the server socket
			if (serverSocket != null) {
				serverSocket.close();
			} else {
				Log.i(LOG_TAG, "bt_classic_server_stop :: There was no previously opened connection.");
			}
	
			int asyncId = asyncTokenGenerator.getAndIncrement();
			notifyAsyncOperationSuccess("bt_classic_server_stop", asyncId, null);

			return asyncId;
		} catch (Exception e) {
			Log.e(LOG_TAG, "bt_classic_server_stop :: Error when closing server.", e);
			return -1;
		}
	}
	
	public double bt_classic_socket_receive(double socketIndex, double bufferId, double offset) {

		ByteBuffer bytes = RunnerJNILib.bufferGetByteBuffer((int)bufferId);

		if (bytes == null) return -1;

		ConcurrentLinkedQueue<byte[]> dataQueue = socketDataQueues.get((int) socketIndex);
		if (dataQueue == null) {
			return -1.0; // Return -1.0 if no such socketIndex exists
		}
	
		// Get the next value in queue
		byte[] array = dataQueue.poll();
		
		if (array == null)
			return 0.0;
		else {
			bytes.put(array, (int)offset, array.length);
			return array.length;
		}
		
	}
	
	public double bt_classic_socket_send(double socketIndex, double bufferId, double offset, double length) {

		// Prepare the byte array to send
		ByteBuffer bytes = RunnerJNILib.bufferGetByteBuffer((int)bufferId);

		if (bytes == null) return -1;

		bytes.position((int) offset);
		byte[] arr = new byte[(int) length];
		bytes.get(arr, 0, (int) length);
	
		BluetoothSocket socket = activeSockets.get((int) socketIndex);
		if (socket == null) {
			Log.i(LOG_TAG, "bt_classic_socket_send :: Socket index doesn't exist.");
			return -3.0; // Return -3.0 if no such socketIndex exists
		}
	
		try {
			// Write the byte array to the socket's OutputStream
			socket.getOutputStream().write(arr);
			return TRUE;
		} catch (IOException e) {
			notifyOperation("bt_classic_socket_remotely_closed", Map.of("socket_id", socketIndex));
			return -1.0; // return -1 for IOException
		} catch (Exception e) {
			Log.e(LOG_TAG, "bt_classic_socket_send :: Unknown error.", e);
			return -2.0; // return -2 for other exceptions
		}
	}
	
	public double bt_classic_socket_open(final String address, final String uuidString, double insecure) {
		BluetoothSocket temporarySocket = null;
	
		// Check if socket is already opened for the given address
		for (BluetoothSocket socket : activeSockets.values()) {
			if (socket.getRemoteDevice().getAddress().equalsIgnoreCase(address)) {
				temporarySocket = socket;
				break;
			}
		}

		// Socket is already opened
		if (temporarySocket != null) return -1;

		BluetoothDevice device = getBluetoothDevice(address, "bt_classic_socket_open");
		if (device == null) return -1; // Device not found
	

		// If device is currently discovering stop it (this is good practice)
		if (bt_classic_scan_is_active() == TRUE) {
			bt_classic_scan_stop();
		}

		UUID uuid = getUUIDFromString(uuidString, "bt_classic_socket_open");
		if (uuid == null) return -1;

		try {
			if (insecure > .5) {
				temporarySocket = device.createInsecureRfcommSocketToServiceRecord(uuid);
			} else {
				temporarySocket = device.createRfcommSocketToServiceRecord(uuid);
			}
		} catch (IOException e) {
			Log.e(LOG_TAG, "bt_classic_socket_open :: Error creating socket", e);
			return -1; // Socket creation failed
		}

		final BluetoothSocket socket = temporarySocket;
		final int asyncId = asyncTokenGenerator.getAndIncrement();

		new Thread(new Runnable() {
			public void run() {
				Map<String, Object> extraParams = new HashMap<String,Object>();
				
				// Publish the Async Event to the runner
				extraParams.putAll(Map.of(
					"address", address,
					"uuid", uuidString
				));
				
				try {
					// Try to connect
					socket.connect();
					int socketId = socketIdGenerator.getAndIncrement();

					// Connection was successful
					extraParams.put("socket_id", socketId);

					notifyAsyncOperationSuccess("bt_classic_socket_open", asyncId, extraParams);

					// Store socket onto HashMap
					handleBluetoothSocket(socket, socketId);
				} catch (IOException connectException) {
					// Connection failed
					extraParams.put("error_message", connectException.getMessage());
					notifyAsyncOperationError("bt_classic_socket_open", asyncId, connectException.hashCode(), extraParams);
				}
			}
		}).start();

		return asyncId; // Process started successfully
	}
		
	public double bt_classic_socket_close(double socketIndex) {
		try {
			// Close the socket and remove it from activeSockets
			BluetoothSocket socket = activeSockets.remove((int) socketIndex);
			if (socket != null) {
				socket.close();
			}
	
			// Remove the socket's buffer queue from socketDataQueues
			socketDataQueues.remove((int) socketIndex);
	
			// Interrupt the handling thread for this socket
			Thread thread = activeThreads.remove((int) socketIndex);
			if (thread != null) {
				thread.interrupt();
			}
	
			return TRUE;
		} catch (IOException e) {
			Log.e(LOG_TAG, "bt_classic_socket_close :: Error closing Bluetooth socket", e);
			return -1.0; // return -1 for IOException
		} catch (Exception e) {
			Log.e(LOG_TAG, "bt_classic_socket_close :: Unknown error", e);
			return -2.0; // return -2 for other exceptions
		}
	}
	
	public double bt_classic_socket_close_all() {
		for (Integer socketIndex : activeSockets.keySet()) {
			bt_classic_socket_close(socketIndex);
		}
		return TRUE;
	}
	
	// PRIVATE METHODS
	
	private void registerBroadcastReceiver() {
		bluetoothEventReceiver = new BroadcastReceiver() {
			public void onReceive(Context context, Intent intent) {
		
				String action = intent.getAction();
				BluetoothDevice device;
	
				switch (action) {
					case BluetoothDevice.ACTION_FOUND:
	
						// Get Device information from intent
						device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
	
						String name = device.getName();

						// Publish the Async Event to the runner
						notifyOperation("bt_classic_scan_result", Map.of(
							"success", true,
							"name", name == null ? "" : name,
							"address", device.getAddress(),
							"connected", false,
							"authenticated", device.getBondState() == BluetoothDevice.BOND_BONDED,
							"remembered", device.getBondState() == BluetoothDevice.BOND_BONDED
						));
						break;
	
					case BluetoothDevice.ACTION_PAIRING_REQUEST:
	
						// Publish the Async Event to the runner
						notifyOperation("bt_classic_pairing_request", null);
						break;
	
					case BluetoothDevice.ACTION_ACL_CONNECTED:
	
						// Get Device information from intent
						device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
	
						// Publish the Async Event to the runner
						notifyOperation("bt_classic_socket_state_changed", Map.of(
							"connected", true,
							"device", createBluetoothDeviceJson(device)
						));
						break;
	
					case BluetoothDevice.ACTION_ACL_DISCONNECTED:
	
						// Get Device information from intent
						device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
	
						// Publish the Async Event to the runner
						notifyOperation("bt_classic_socket_state_changed", Map.of(
							"connected", false,
							"device", createBluetoothDeviceJson(device)
						));
						break;
	
					case BluetoothDevice.ACTION_ACL_DISCONNECT_REQUESTED:
	
						// Publish the Async Event to the runner
						// notifyOperation("bt_classic_receiver_acl_disconnect_requested", null);
						break;
	
					case BluetoothAdapter.ACTION_DISCOVERY_STARTED:
	
						// Publish the Async Event to the runner
						notifyOperation("bt_classic_scan_started", null);
						break;
	
					case BluetoothAdapter.ACTION_DISCOVERY_FINISHED:
	
						// Publish the Async Event to the runner
						notifyOperation("bt_classic_scan_finished", null);
						break;
				}
			}
		};
	
		IntentFilter filter;
	
		filter = new IntentFilter(BluetoothDevice.ACTION_FOUND);
		activity.registerReceiver(bluetoothEventReceiver, filter);
	
		filter = new IntentFilter(BluetoothDevice.ACTION_PAIRING_REQUEST);
		activity.registerReceiver(bluetoothEventReceiver, filter);
	
		filter = new IntentFilter(BluetoothDevice.ACTION_ACL_CONNECTED);
		activity.registerReceiver(bluetoothEventReceiver, filter);
	
		filter = new IntentFilter(BluetoothDevice.ACTION_ACL_DISCONNECTED);
		activity.registerReceiver(bluetoothEventReceiver, filter);
	
		filter = new IntentFilter(BluetoothDevice.ACTION_ACL_DISCONNECT_REQUESTED);
		activity.registerReceiver(bluetoothEventReceiver, filter);
	
		filter = new IntentFilter(BluetoothAdapter.ACTION_DISCOVERY_STARTED);
		activity.registerReceiver(bluetoothEventReceiver, filter);
	
		filter = new IntentFilter(BluetoothAdapter.ACTION_DISCOVERY_FINISHED);
		activity.registerReceiver(bluetoothEventReceiver, filter);
	}
	
	private void unregisterBroadcastReceiver() {
		try {
			activity.unregisterReceiver(bluetoothEventReceiver);
		} catch (Exception e) {
			Log.e(LOG_TAG, "unregisterBroadcastReceiver :: Error while unregistering from the events", e);
		}
	}
	
	private Boolean isBluetoothSupported(final String methodNamge) {
		if (activity.getPackageManager().hasSystemFeature(PackageManager.FEATURE_BLUETOOTH))
			return true;
	
		Log.i(LOG_TAG, methodNamge + " :: Your device doesn't support bluetooth.");
		return false;
	}
		
	private void handleBluetoothSocket(final BluetoothSocket socket, final int socketId) {
	
		activeSockets.put(socketId, socket);
		ConcurrentLinkedQueue<byte[]> bufferQueue = new ConcurrentLinkedQueue<>();
		socketDataQueues.put(socketId, bufferQueue);
	
		Thread thread = new Thread(new Runnable() {
			public void run() {
				try {
					// Run while the socket is opened or thread was not interrupted
					while (socketDataQueues.containsKey(socketId) && !Thread.currentThread().isInterrupted()) {
						byte[] buffer = new byte[1024];
	
						// Read from the socket's InputStream (this will lock execution)
						int numBytes = socket.getInputStream().read(buffer);
						if (numBytes == -1) {
							notifyOperation("bt_classic_socket_remotely_closed", Map.of(
								"socket_id", socketId
							));
							break;
						}

						// Copy the read bytes into a new buffer and add it to the queue
						byte[] receivedBytes = Arrays.copyOfRange(buffer, 0, numBytes);
	
						socketDataQueues.get(socketId).add(receivedBytes);
					}
				} catch (Exception e) {
					// If there's an error, log the error message and report to runner
					notifyOperation("bt_classic_socket_error", Map.of(
						"socket_id", socketId
					));
				}
			}
		});
		thread.start();
		activeThreads.put(socketId, thread);
	}
	
	//#endregion

	//#region Bluetooth Low Energy

	private static final int ERROR_OPERATION_NOT_STARTED = -10;

	private static final String CCCD_UUID_STRING = "00002902-0000-1000-8000-00805f9b34fb";
	private static final UUID CCCD_UUID = UUID.fromString(CCCD_UUID_STRING);

	// TASK SYSTEM

	public abstract class QueueableTask {
		String functionName;
		int asyncId;

		public QueueableTask(String functionName, int asyncId) {
			this.functionName = functionName;
			this.asyncId = asyncId;
		}

		public abstract int run();
	}

	public abstract class BLEGattTask<G, T> extends QueueableTask {
		G gatt;
		T target;
	
		public BLEGattTask(G gatt, T target, String functionName, int asyncId) {
			super(functionName, asyncId);
			this.gatt = gatt;
			this.target = target;
		}
	}

	public abstract class BLEGattDataTask<G, T> extends BLEGattTask<G, T> {
		byte[] data;

		public BLEGattDataTask(G gatt, T target, byte[] data, String functionName, int asyncId) {
			super(gatt, target, functionName, asyncId);
			this.data = data;
		}
	}

	private <T extends QueueableTask> void handleTaskQueue(ConcurrentLinkedQueue<T> taskQueue, int status, Map<String, Object> data) {
		T task = taskQueue.remove();

		String functionName = task.functionName;
		int asyncId = task.asyncId;

		if (status == BluetoothGatt.GATT_SUCCESS) {
			notifyAsyncOperationSuccess(functionName, asyncId, data);
		}
		else notifyAsyncOperationError(functionName, asyncId, status, null);

		handleNextTask(taskQueue);
	}

	private <T extends QueueableTask> void handleNextTask(ConcurrentLinkedQueue<T> queue) {
		T task = queue.peek();
		while (task != null) {
			int errorCode = task.run();
			if (errorCode == BluetoothStatusCodes.SUCCESS) break;
			
			// Publish the Async Event to the runner
			notifyAsyncOperationError(task.functionName, task.asyncId, errorCode, null);
			
			queue.poll();
			task = queue.peek();
		}
	}

	private <T extends QueueableTask> double createTaskAndHandleQueue(ConcurrentLinkedQueue<T> taskQueue, Supplier<T> taskSupplier) {
		// Generate a new async identifier
		T task = taskSupplier.get();
		taskQueue.add(task);

		if (taskQueue.size() == 1) {
			handleNextTask(taskQueue);
		}

		return (double)task.asyncId;
	}

	// GENERAL

	public double bt_le_is_supported() {
		return isBluetoothLeSupported("bluetoothle_android_is_supported") ? TRUE : FALSE;
	}

	// SCANNER

	private class ScanManager {

		private boolean isScanning = false;

		// TASKS

		public class BLEScanStartTask extends QueueableTask {

			private BluetoothLeScanner bluetoothLeScanner;
			private ScanCallback scanCallback;

			public BLEScanStartTask(BluetoothLeScanner bluetoothLeScanner, ScanCallback scanCallback, String functionName, int asyncId) {
				super(functionName, asyncId);
				this.bluetoothLeScanner = bluetoothLeScanner;
				this.scanCallback = scanCallback;
			}

			public int run() {
				bluetoothLeScanner.startScan(scanCallback);
				return 0;
			}
		}

		public ConcurrentLinkedQueue<BLEScanStartTask> scanStartTasks = new ConcurrentLinkedQueue<>();

		// CALLBACK HANDLER

		private class ScanCallbackHandler extends ScanCallback {

			private ScanManager manager;

			public ScanCallbackHandler(ScanManager manager) {
				this.manager = manager;
			}

			@Override
			public void onScanResult(int callbackType, ScanResult result) {

				if (manager.scanStartTasks.size() > 0) {
					handleTaskQueue(manager.scanStartTasks, 0, null);
				}

				BluetoothDevice device = result.getDevice();

				JSONArray uuids = new JSONArray();
				ScanRecord scanRecord = result.getScanRecord();
				if (scanRecord != null) {
					List<ParcelUuid> parcelUuids = scanRecord.getServiceUuids();
					if (parcelUuids != null) {
						for (ParcelUuid uuid : parcelUuids) {
							uuids.put(uuid.toString());
						}
					}
				}

				// Publish the Async Event to the runner
				notifyOperation("bt_le_scan_result", Map.of(
					"name", device.getName() != null ? device.getName() : "", // optional
					"address", device.getAddress(),
					"raw_signal", result.getRssi(),
					"is_connectable", result.isConnectable(),
					"uuids", uuids
				));
			}

			@Override
			public void onScanFailed(int errorCode) {
				handleTaskQueue(manager.scanStartTasks, errorCode, null);
			}

			@Override
			public void onBatchScanResults(List<ScanResult> results) {
				// Windows doesn't have this information
			}
		}

		public ScanCallbackHandler scanCallbackHandler = new ScanCallbackHandler(this);

		// INTERNAL

		private BluetoothLeScanner getBluetoothLeScanner(String functionName) {

			BluetoothAdapter bluetoothAdapter = getBluetoothAdapter(functionName);
			if (bluetoothAdapter == null) return null;

			BluetoothLeScanner bluetoothLeScanner = bluetoothAdapter.getBluetoothLeScanner();
			if (bluetoothLeScanner == null) {
				Log.i(LOG_TAG, functionName + " :: Bluetooth scanner is not available.");
			}
			return bluetoothLeScanner;
		}

		private double queueScanStartTask(BluetoothLeScanner bluetoothLeScanner, ScanCallback scanCallback, String functionName) {
			int asyncId = asyncTokenGenerator.getAndIncrement();
			return createTaskAndHandleQueue(scanStartTasks, () -> new BLEScanStartTask(bluetoothLeScanner, scanCallback, functionName, asyncId));
		}

		// PUBLIC API

		private double scanStartAsync(String functionName) {
			
			if (isScanning == true) return -1;

			BluetoothLeScanner bluetoothLeScanner = getBluetoothLeScanner(functionName);
			if (bluetoothLeScanner == null) return -1;
							
			isScanning = true;
			return queueScanStartTask(bluetoothLeScanner, scanCallbackHandler, functionName);
		}

		private double scanStopAsync(String functionName) {

			if (isScanning == false) return -1;

			BluetoothLeScanner bluetoothLeScanner = getBluetoothLeScanner(functionName);
			if (bluetoothLeScanner == null) return -1;

			bluetoothLeScanner.stopScan(scanCallbackHandler);
			isScanning = false;

			int asyncId = asyncTokenGenerator.getAndIncrement();
			notifyAsyncOperationSuccess(functionName, asyncId, null);
			return (double)asyncId;
		}
		
		private double scanIsActiveAsync(String functionName) {
			return isScanning ? TRUE : FALSE;
		}

	}

	ScanManager scanManager = new ScanManager();

	public double bt_le_scan_start() {
		return scanManager.scanStartAsync("bt_le_scan_start");
	}

	public double bt_le_scan_stop() {
		return scanManager.scanStopAsync("bt_le_scan_stop");
	}
	
	public double bt_le_scan_is_active() {
		return scanManager.scanIsActiveAsync("bt_le_scan_is_active");
	}

	// ADVERTISER

	private class AdvertiseManager {

		public boolean isAdvertising = false;

		// TASKS

		private class BLEAdvertiseStartTask extends QueueableTask {

			BluetoothLeAdvertiser bluetoothLeAdvertiser;
			AdvertiseSettings advertiseSettings;
			AdvertiseData advertiseData;
			AdvertiseCallback advertiseCallback;
			
			public BLEAdvertiseStartTask(BluetoothLeAdvertiser bluetoothLeAdvertiser, AdvertiseSettings advertiseSettings, AdvertiseData advertiseData, AdvertiseCallback advertiseCallback, String functionName, int asyncId) {
				super(functionName, asyncId);
				this.bluetoothLeAdvertiser = bluetoothLeAdvertiser;
				this.advertiseSettings = advertiseSettings;
				this.advertiseData = advertiseData;
				this.advertiseCallback = advertiseCallback;
			}

			public int run() {
				bluetoothLeAdvertiser.startAdvertising(advertiseSettings, advertiseData, advertiseCallback);
				return 0;
			}
		}

		public ConcurrentLinkedQueue<BLEAdvertiseStartTask> advertiseStartTasks = new ConcurrentLinkedQueue<>();

		// CALLBACK HANDLER

		private class AdvertiseCallbackHandler extends AdvertiseCallback {

			AdvertiseManager manager = null;

			public AdvertiseCallbackHandler(AdvertiseManager manager) {
				this.manager = manager;
			}

			@Override
			public void onStartSuccess(AdvertiseSettings settingsInEffect) {
				manager.isAdvertising = true;
				handleTaskQueue(manager.advertiseStartTasks, 0, null);
			}

			@Override
			public void onStartFailure(int errorCode) {
				handleTaskQueue(manager.advertiseStartTasks, errorCode, null);
			}
		}

		public AdvertiseCallbackHandler advertiseCallbackHandler = new AdvertiseCallbackHandler(this);

		// INTERNAL

		private boolean isAdvertisingSupported(String functionName) {
			BluetoothAdapter bluetoothAdapter = getBluetoothAdapter(functionName);
			
			if (bluetoothAdapter == null) return false;
			
			if (!isBluetoothEnabled(functionName)) return false;
			
			if (!isBluetoothLeSupported(functionName)) return false;
			
			if (!bluetoothAdapter.isMultipleAdvertisementSupported()) {
				Log.i(LOG_TAG, functionName + " :: Bluetooth LE advertising is not supported on this device.");
				return false;
			}
			
			if (ContextCompat.checkSelfPermission(activity, Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
				Log.i(LOG_TAG, functionName + " :: Fine location permission is not granted.");
				return false;
			}
			
			return true;
		}

		private BluetoothLeAdvertiser getBluetoothLeAdvertiser(String functionName) {

			boolean supported = isAdvertisingSupported(functionName);
			if (supported == false) return null;

			BluetoothAdapter bluetoothAdapter = getBluetoothAdapter(functionName);
			if (bluetoothAdapter == null) return null;

			BluetoothLeAdvertiser bluetoothLeAdvertiser = bluetoothAdapter.getBluetoothLeAdvertiser();
			if (bluetoothLeAdvertiser == null) {
				Log.i(LOG_TAG, functionName + " :: Bluetooth advertiser is not available.");
			}
			return bluetoothLeAdvertiser;
		}

		private AdvertiseSettings createAdvertiseSettings(JSONObject advertiseSettingsJSON, String functionName) {

			int advertiseMode = advertiseSettingsJSON.optInt("advertiseMode", AdvertiseSettings.ADVERTISE_MODE_BALANCED);
			boolean connectable = advertiseSettingsJSON.optBoolean("connectable", true);
			boolean discoverable = advertiseSettingsJSON.optBoolean("discoverable", true);
			int timeout = advertiseSettingsJSON.optInt("timeout", 10000);
			int txPowerLevel = advertiseSettingsJSON.optInt("txPowerLevel", AdvertiseSettings.ADVERTISE_TX_POWER_MEDIUM);

			AdvertiseSettings.Builder builder = new AdvertiseSettings.Builder()
				.setAdvertiseMode(advertiseMode)
				.setConnectable(connectable)
				.setTimeout(timeout)
				.setTxPowerLevel(txPowerLevel);

			//if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
			//	builder.setDiscoverable(discoverable);
			//}

			return builder.build();
		}

		private AdvertiseData createAdvertiseData(JSONObject advertiseDataJSON, String functionName) {

			AdvertiseData.Builder advertiseDataBuilder = new AdvertiseData.Builder();

			boolean includeName = advertiseDataJSON.optBoolean("includeName", true);
			boolean includePowerLevel = advertiseDataJSON.optBoolean("includePowerLevel", false);

			advertiseDataBuilder.setIncludeDeviceName(includeName).setIncludeTxPowerLevel(includePowerLevel);

			// Add service (uuid + data) to the AdvertiseData
			if (advertiseDataJSON.has("services")) {
				JSONArray servicesArray = advertiseDataJSON.optJSONArray("services");
				if (servicesArray != null) {
					for (int i = 0; i < servicesArray.length(); i++) {
						JSONObject serviceEntry = servicesArray.optJSONObject(i);
						if (serviceEntry == null) {
							Log.i(LOG_TAG, functionName + " :: Invalid services entry (each entry should be a struct)");
							return null;
						}
						
						String uuidString = serviceEntry.optString("uuid");
						UUID uuid = getUUIDFromString(uuidString, functionName);
						if (uuid == null) {
							Log.i(LOG_TAG, functionName + " :: Invalid services entry 'uuid' member (should be a valid uuid)");
							return null;
						}

						if (serviceEntry.has("data")) {
							String dataString = serviceEntry.optString("data");
							byte[] data = decodeBase64String(dataString, functionName);
							if (data == null) {
								Log.i(LOG_TAG, functionName + " :: Invalid services entry 'data' member (should be a valid base64 string)");
								return null;
							}
							advertiseDataBuilder.addServiceData(new ParcelUuid(uuid), data);
						}
						else {
							advertiseDataBuilder.addServiceUuid(new ParcelUuid(uuid));
						}
					}
				}
				else {
					Log.i(LOG_TAG, functionName + " :: Invalid 'services' member (member must be an array)");
				}
			}

			// Add manufacturer data to the AdvertiseData
			if (advertiseDataJSON.has("manufacturer")) {
				JSONObject manufacturerObject = advertiseDataJSON.optJSONObject("manufacturer");
				if (manufacturerObject != null) {

					int id = manufacturerObject.optInt("id", -1);
					if (id == -1) {
						Log.i(LOG_TAG, functionName + " :: Invalid manufacturer 'id' member (should be a valid integer)");
						return null;
					}

					String dataString = manufacturerObject.optString("data");
					byte[] data = decodeBase64String(dataString, functionName);
					if (data == null) {
						Log.i(LOG_TAG, functionName + " :: Invalid manufacturer 'data' member (should be a valid base64 string)");
						return null;
					}

					advertiseDataBuilder.addManufacturerData(id, data);
				}
				else {
					Log.i(LOG_TAG, functionName + " :: Invalid 'manufacturerData' member (member must be a struct)");
				}
			} 

			return advertiseDataBuilder.build();
		}

		private double queueAdvertiseStartTask(BluetoothLeAdvertiser bluetoothLeAdvertiser, AdvertiseSettings advertiseSettings, AdvertiseData advertiseData, AdvertiseCallback advertiseCallback, String functionName) {
			int asyncId = asyncTokenGenerator.getAndIncrement();
			return createTaskAndHandleQueue(advertiseStartTasks, () -> new BLEAdvertiseStartTask(bluetoothLeAdvertiser, advertiseSettings, advertiseData, advertiseCallback, functionName, asyncId));
		}

		// PUBLIC API

		public double advertiseStartAsync(String settings, String data, String functionName) {

			if (isAdvertising == true) return -1;

			BluetoothLeAdvertiser bluetoothLeAdvertiser = getBluetoothLeAdvertiser(functionName);
			if (bluetoothLeAdvertiser == null) return -1;

			AdvertiseSettings advertiseSettings = null;
			AdvertiseData advertiseData = null;
			
			try {
				JSONObject settingsJSON = new JSONObject(settings);
				advertiseSettings = createAdvertiseSettings(settingsJSON, functionName);
			} catch (JSONException e) {
				Log.e(LOG_TAG, functionName + " :: Invalid settings json string.", e);
				return FALSE;
			}
			
			try {
				JSONObject dataJSON = new JSONObject(data);
				advertiseData = createAdvertiseData(dataJSON, functionName);
			} catch (JSONException e) {
				Log.e(LOG_TAG, functionName + " :: Invalid data json string.", e);
				return FALSE;
			}

			if (advertiseSettings == null || advertiseData == null) {
				return -1;
			}
			
			return queueAdvertiseStartTask(bluetoothLeAdvertiser, advertiseSettings, advertiseData, advertiseCallbackHandler, functionName);
		}

		public double advertiseStopAsync(String functionName) {

			if (isAdvertising == false) return -1;

			BluetoothLeAdvertiser bluetoothLeAdvertiser = getBluetoothLeAdvertiser(functionName);
			if (bluetoothLeAdvertiser == null) return -1;

			bluetoothLeAdvertiser.stopAdvertising(advertiseCallbackHandler);
			isAdvertising = false;

			int asyncId = asyncTokenGenerator.getAndIncrement();
			notifyAsyncOperationSuccess(functionName, asyncId, null);
			return (double)asyncId;
		}

		public double advertiseIsActive(String functionName) {
			return isAdvertising ? TRUE : FALSE;
		}
	}

	AdvertiseManager advertiseManager = new AdvertiseManager();

	public double bt_le_advertise_start(String settings, String data) { 
		return advertiseManager.advertiseStartAsync(settings, data, "bt_le_advertise_start");
	}
	
	public double bt_le_advertise_stop() {
		return advertiseManager.advertiseStopAsync("bt_le_advertise_stop");
	}
	
	public double bt_le_advertise_is_active() { 
		return advertiseManager.advertiseIsActive("bt_le_advertise_is_active");
	}

	// SERVER

	private class BluetoothGattServerManager {

		public BluetoothGattServer bluetoothGattServer = null;

		// TASKS

		private class BLEServiceAddTask extends BLEGattTask<BluetoothGattServer, BluetoothGattService> {
		
			public BLEServiceAddTask(BluetoothGattServer gattServer, BluetoothGattService service, String functionName, int asyncId) {
				super(gattServer, service, functionName, asyncId);
			}
		
			public int run() {
				return gatt.addService(target) ? BluetoothStatusCodes.SUCCESS : ERROR_OPERATION_NOT_STARTED;
			}
		}

		private class BLENotificationTask extends BLEGattDataTask<BluetoothGattServer, BluetoothDevice> {

			BluetoothGattCharacteristic characteristic;
		
			public BLENotificationTask(BluetoothGattServer gattServer, BluetoothDevice device, BluetoothGattCharacteristic characteristic, byte[] data, String functionName, int asyncId) {
				super(gattServer, device, data, functionName, asyncId);
				this.characteristic = characteristic;
			}
		
			public int run() {
				if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
					return gatt.notifyCharacteristicChanged(target, characteristic, true, data);
				} else {
					characteristic.setValue(data);
					return gatt.notifyCharacteristicChanged(target, characteristic, true) ? 0 : ERROR_OPERATION_NOT_STARTED;
				}
			}
		}

		public ConcurrentLinkedQueue<BLEServiceAddTask> serviceAddTasks = new ConcurrentLinkedQueue<>();
		public ConcurrentLinkedQueue<BLENotificationTask> notificationTasks = new ConcurrentLinkedQueue<>();

		// REQUEST RESPONSE

		private class PendingRequestResponse {

			public static final int READ = 0;
			public static final int WRITE = 1;

			BluetoothDevice device;
			Integer type;
			long timestamp;

			public PendingRequestResponse(BluetoothDevice device, int type) {
				this.device = device;
				this.type = type;
				this.timestamp = System.currentTimeMillis();
			}
		}

		public ConcurrentHashMap<Integer, PendingRequestResponse> pendingRequestResponses = new ConcurrentHashMap<>();

		// CALLBACK HANDLER

		private class BluetoothGattServerCallbackHandler extends BluetoothGattServerCallback {

			private BluetoothGattServerManager manager;

			private JSONArray notifiedDevices = new JSONArray();

			private void createPendingRequestResponse(int identifier, BluetoothDevice device, int type) {
				manager.pendingRequestResponses.put(identifier, new PendingRequestResponse(device, type));
			}

			private void sendResponse(BluetoothDevice device, int requestId, int status, int offset, byte[] data) {
				manager.bluetoothGattServer.sendResponse(device, requestId, status, offset, data);
			}

			public BluetoothGattServerCallbackHandler(BluetoothGattServerManager manager) {
				this.manager = manager;
			}

			@Override
			public void onCharacteristicReadRequest(BluetoothDevice device, int requestId, int offset, BluetoothGattCharacteristic characteristic) {

				// Data offset (not supported)
				if (offset != 0) {
					sendResponse(device, requestId, BluetoothGatt.GATT_INVALID_OFFSET, offset, null);
					return;
				}

				// Get properties
				int properties = characteristic.getProperties();
				if ((properties & BluetoothGattCharacteristic.PROPERTY_READ) == 0) {
					sendResponse(device, requestId, BluetoothGatt.GATT_READ_NOT_PERMITTED, 0, null);
					return;
				}

				// Get permissions
				int permissions = characteristic.getPermissions();
				if (((permissions & BluetoothGattCharacteristic.PERMISSION_READ_ENCRYPTED) != 0) && device.getBondState() == BluetoothDevice.BOND_NONE) {
					sendResponse(device, requestId, BluetoothGatt.GATT_INSUFFICIENT_AUTHENTICATION, 0, null);
					return;
				}

				// Publish the Async Event to the runner
				notifyOperation("bt_le_server_characteristic_read_request", Map.of(
					"request_id", requestId,
					"service_uuid", characteristic.getService().getUuid().toString().toUpperCase(),
					"characteristic_uuid", characteristic.getUuid().toString().toUpperCase()
				));

				createPendingRequestResponse(requestId, device, PendingRequestResponse.READ);
			}

			@Override
			public void onCharacteristicWriteRequest(BluetoothDevice device, int requestId, BluetoothGattCharacteristic characteristic, boolean preparedWrite, boolean responseNeeded, int offset, byte[] value) {

				// Data offset (not supported)
				if (offset != 0) {
					sendResponse(device, requestId, BluetoothGatt.GATT_INVALID_OFFSET, offset, null);
					return;
				}

				// Write later (not supported)
				if (preparedWrite) {
					if (responseNeeded) {
						sendResponse(device, requestId, BluetoothGatt.GATT_REQUEST_NOT_SUPPORTED, 0, null);
					}
					return;
				}

				// Get properties
				int properties = characteristic.getProperties();
				if ((properties & BluetoothGattCharacteristic.PROPERTY_WRITE) == 0) {
					sendResponse(device, requestId, BluetoothGatt.GATT_READ_NOT_PERMITTED, 0, null);
					return;
				}

				// Get permissions
				int permissions = characteristic.getPermissions();
				if (((permissions & BluetoothGattCharacteristic.PERMISSION_WRITE_ENCRYPTED) != 0) && device.getBondState() == BluetoothDevice.BOND_NONE) {
					sendResponse(device, requestId, BluetoothGatt.GATT_INSUFFICIENT_AUTHENTICATION, 0, null);
					return;
				}

				// Publish the Async Event to the runner
				String operationType;
				Map<String, Object> extraParams = new HashMap<String,Object>();

				if (responseNeeded) {
					operationType = "bt_le_server_characteristic_write_request";
					extraParams.put("request_id", requestId);

					// If a response is required respond to the requesting device
					createPendingRequestResponse(requestId, device, PendingRequestResponse.WRITE);
				}
				else {
					operationType = "bt_le_server_characteristic_write_command";
				}

				extraParams.putAll(Map.of(
					"service_uuid", characteristic.getService().getUuid().toString().toUpperCase(),
					"characteristic_uuid", characteristic.getUuid().toString().toUpperCase(),
					"value", value
				));
				notifyOperation(operationType, extraParams);
			}

			@Override
			public void onConnectionStateChange(BluetoothDevice device, int status, int newState) {

				notifyOperation("bt_le_server_connection_state_changed", Map.of(
					"success", status == BluetoothGatt.GATT_SUCCESS,
					"connected", newState == BluetoothProfile.STATE_CONNECTED,
					"device", createBluetoothDeviceJson(device)
				));
			}

			@Override
			public void onDescriptorReadRequest(BluetoothDevice device, int requestId, int offset, BluetoothGattDescriptor descriptor) {

				// Data offset (not supported)
				if (offset != 0) {
					sendResponse(device, requestId, BluetoothGatt.GATT_INVALID_OFFSET, offset, null);
					return;
				}

				notifyOperation("bt_le_server_descriptor_read_request", Map.of(
					"request_id", requestId,
					"service_uuid", descriptor.getCharacteristic().getService().getUuid().toString().toUpperCase(),
					"characteristic_uuid", descriptor.getCharacteristic().getUuid().toString().toUpperCase(),
					"descriptor_uuid", descriptor.getUuid().toString().toUpperCase()
				));

				createPendingRequestResponse(requestId, device, PendingRequestResponse.READ);
			}

			@Override
			public void onDescriptorWriteRequest(BluetoothDevice device, int requestId, BluetoothGattDescriptor descriptor, boolean preparedWrite, boolean responseNeeded, int offset, byte[] value) {

				// Data offset (not supported)
				if (offset != 0) {
					sendResponse(device, requestId, BluetoothGatt.GATT_INVALID_OFFSET, offset, null);
					return;
				}

				// Write later (not supported)
				if (preparedWrite) {
					if (responseNeeded) {
						sendResponse(device, requestId, BluetoothGatt.GATT_REQUEST_NOT_SUPPORTED, 0, null);
					}
					return;
				}

				// Get permissions
				int permissions = descriptor.getPermissions();
				if (((permissions & BluetoothGattDescriptor.PERMISSION_WRITE_ENCRYPTED) != 0) && device.getBondState() == BluetoothDevice.BOND_NONE) {
					sendResponse(device, requestId, BluetoothGatt.GATT_INSUFFICIENT_AUTHENTICATION, 0, null);
					return;
				}

				// Publish the Async Event to the runner
				String operationType;
				Map<String, Object> extraParams = new HashMap<String,Object>();

				if (responseNeeded) {
					operationType = "bt_le_server_descriptor_write_request";
					extraParams.put("request_id", requestId);

					// If a response is required respond to the requesting device
					createPendingRequestResponse(requestId, device, PendingRequestResponse.WRITE);
				}
				else {
					operationType = "bt_le_server_descriptor_write_command";
				}

				extraParams.putAll(Map.of(
					"service_uuid", descriptor.getCharacteristic().getService().getUuid().toString().toUpperCase(),
					"characteristic_uuid", descriptor.getCharacteristic().getUuid().toString().toUpperCase(),
					"descriptor_uuid", descriptor.getUuid().toString().toUpperCase(),
					"value", value
				));
				notifyOperation(operationType, extraParams);
			}

			@Override
			public void onExecuteWrite(BluetoothDevice device, int requestId, boolean execute) {

			}

			@Override
			public void onMtuChanged(BluetoothDevice device, int mtu) {
			}

			@Override
			public void onNotificationSent(BluetoothDevice device, int status) {

				QueueableTask queueableTask = manager.notificationTasks.remove();  // remove the completed operation from the queue

				String functionName = queueableTask.functionName;
				int asyncId = queueableTask.asyncId;

				// Add device to list of notified devices
				try {
					JSONObject notifiedDevice = new JSONObject();
					notifiedDevice.put("address", device.getAddress());
					notifiedDevice.put("status", status);
					notifiedDevices.put(notifiedDevice);
				} catch (JSONException e) {
					Log.e(LOG_TAG, "Failed to create notified bluetooth device JSON object", e);
				}

				// Notify success if this is the end of the asyncId group
				queueableTask = manager.notificationTasks.peek();
				if (queueableTask == null || queueableTask.asyncId != asyncId) {
					notifyAsyncOperationSuccess(functionName, asyncId, Map.of(
						"devices", notifiedDevices
						));

					// Clear data array
					notifiedDevices = new JSONArray();
				}

				handleNextTask(manager.notificationTasks);
			}

			@Override
			public void onPhyRead(BluetoothDevice device, int txPhy, int rxPhy, int status) {

			}

			@Override
			public void onPhyUpdate(BluetoothDevice device, int txPhy, int rxPhy, int status) {

			}

			@Override
			public void onServiceAdded(int status, BluetoothGattService service) {
				handleTaskQueue(manager.serviceAddTasks, status, Map.of(
					"service_uuid", service.getUuid()
					));
			}
		}

		private BluetoothGattServerCallbackHandler bluetoothGattServerCallbackHandler = new BluetoothGattServerCallbackHandler(this);

		// INTERNAL

		private static final long CLEANUP_INTERVAL_MS = 2000; // 2 second
		private final Handler handler = new Handler();
		
		private final Runnable cleanupRunnable = this::executeCleanup;

		private void executeCleanup() {
			cleanupOldRequests();
			handler.postDelayed(cleanupRunnable, CLEANUP_INTERVAL_MS);
		}
		
		private void startCleanupTask() {
			handler.postDelayed(cleanupRunnable, CLEANUP_INTERVAL_MS);
		}
		
		private void stopCleanupTask() {
			handler.removeCallbacks(cleanupRunnable);
		}
		
		private void cleanupOldRequests() {
			// Iterate through the map and remove old entries
			long currentTime = System.currentTimeMillis();
			for (Iterator<Map.Entry<Integer, PendingRequestResponse>> it = pendingRequestResponses.entrySet().iterator(); it.hasNext(); ) {
				Map.Entry<Integer, PendingRequestResponse> entry = it.next();
				if (currentTime - entry.getValue().timestamp > 10000) {
					it.remove();
				}
			}
		}

		private BluetoothGattService getBluetoothGattService(BluetoothGattServer gattServer, String service, String functionName) {
			
			UUID serviceUUID = getUUIDFromString(service, functionName);
			if (serviceUUID == null) return null;

			BluetoothGattService gattService = gattServer.getService(serviceUUID);
			if (gattService == null) {
				Log.i(LOG_TAG, functionName + " :: Service with uuid '" + service + "' not found.");
			}		

			return gattService;
		}

		private BluetoothGattCharacteristic getBluetoothGattCharateristic(BluetoothGattServer gattServer, String service, String characteristic, String functionName) {

			BluetoothGattService gattService = getBluetoothGattService(gattServer, service, functionName);
			if (gattService == null) return null;

			UUID characteristicUUID = getUUIDFromString(characteristic, functionName);
			if (characteristic == null) return null;

			BluetoothGattCharacteristic gattCharacteristic = gattService.getCharacteristic(characteristicUUID);
			if (gattCharacteristic == null) {
				Log.i(LOG_TAG, functionName + " :: Characteristic with uuid '" + characteristic + "' not found.");
			}
		
			return gattCharacteristic;
		}

		private BluetoothGattDescriptor getBluetoothGattDescriptor(BluetoothGattServer gattServer, String service, String characteristic, String descriptor, String functionName) {

			BluetoothGattCharacteristic gattCharacteristic = getBluetoothGattCharateristic(gattServer, service, characteristic, functionName);
			if (gattCharacteristic == null) return null;

			UUID descriptorUUID = getUUIDFromString(descriptor, functionName);
			if (descriptorUUID == null) return null;

			BluetoothGattDescriptor gattDescriptor = gattCharacteristic.getDescriptor(descriptorUUID);
			if (gattDescriptor == null) {
				Log.i(LOG_TAG, functionName + " :: Descriptor with uuid '" + descriptor + "' not found.");
			}

			return gattDescriptor;
		}

		private BluetoothGattDescriptor createGattDescriptor(JSONObject gattDescriptorJSON, String functionName) throws Exception {

			String uuidString = gattDescriptorJSON.optString("uuid");
			UUID uuid = getUUIDFromString(uuidString, functionName);

			if (uuid == null) {
				throw new Exception("Invalid descriptor uuid: '" + uuidString + "'.");
			}

			int permissions = gattDescriptorJSON.optInt("permissions", 0);

			return new BluetoothGattDescriptor(uuid, permissions);
		}

		private BluetoothGattCharacteristic createGattCharacteristic(JSONObject gattCharacteristicJSON, String functionName) throws Exception {

			String uuidString = gattCharacteristicJSON.optString("uuid");
			UUID uuid = getUUIDFromString(uuidString, functionName);

			if (uuid == null) {
				throw new Exception("Invalid characteristic uuid: '" + uuidString + "'.");
			}

			int properties = gattCharacteristicJSON.optInt("properties", 0);
			int permissions = gattCharacteristicJSON.optInt("permissions", 0);

			BluetoothGattCharacteristic gattCharacteristic = new BluetoothGattCharacteristic(uuid, properties, permissions);

			JSONArray descriptors = gattCharacteristicJSON.optJSONArray("descriptors");
			if (descriptors != null) {
				for (int i = 0; i < descriptors.length(); i++) {
					JSONObject descriptorData = descriptors.optJSONObject(i);
					if (descriptorData == null) {
						throw new Exception("Invalid descriptor structure (a descriptor must be a struct).");
					}
					gattCharacteristic.addDescriptor(createGattDescriptor(descriptorData, functionName));
				}
			}

			if (((properties & BluetoothGattCharacteristic.PROPERTY_NOTIFY) | (properties & BluetoothGattCharacteristic.PROPERTY_INDICATE)) != 0) {
				permissions = BluetoothGattDescriptor.PERMISSION_READ | BluetoothGattDescriptor.PERMISSION_WRITE;
				gattCharacteristic.addDescriptor(new BluetoothGattDescriptor(CCCD_UUID, permissions));
			}

			return gattCharacteristic;
		}

		private BluetoothGattService createGattService(JSONObject gattServiceJSON, String functionName) throws Exception {

			String uuidString = gattServiceJSON.optString("uuid");
			UUID uuid = getUUIDFromString(uuidString, functionName);

			if (uuid == null) {
				throw new Exception("Invalid service uuid: '" + uuidString + "'.");
			}

			BluetoothGattService gattService = new BluetoothGattService(uuid, BluetoothGattService.SERVICE_TYPE_PRIMARY);

			JSONArray characteristics = gattServiceJSON.optJSONArray("characteristics");
			if (characteristics != null) {
				for (int i = 0; i < characteristics.length(); i++) {
					JSONObject characteristicData = characteristics.optJSONObject(i);
					if (characteristicData == null) {
						throw new Exception("Invalid characteristic structure (a characteristic must be a struct).");
					}
					gattService.addCharacteristic(createGattCharacteristic(characteristicData, functionName));
				}
			}

			return gattService;
		}

		private double queueServiceAddTask(BluetoothGattServer bluetoothGattServer, BluetoothGattService bluetoothGattService, String functionName) {
			int asyncId = asyncTokenGenerator.getAndIncrement();
			return createTaskAndHandleQueue(serviceAddTasks, () -> new BLEServiceAddTask(bluetoothGattServer, bluetoothGattService, functionName, asyncId));
		}

		// PUBLIC API

		public double gattServerOpenAsync(String functionName) {
			if (bluetoothGattServer != null) return -1;

			startCleanupTask();

			bluetoothGattServer = bluetoothManager.openGattServer(activity, bluetoothGattServerCallbackHandler);
			
			int asyncId = asyncTokenGenerator.getAndIncrement();
			notifyAsyncOperationSuccess(functionName, asyncId, null);
			return (double) asyncId;
		}

		public double gattServerAddServiceAsync(String data, String functionName) {
			if (bluetoothGattServer == null) return -1;

			JSONObject serviceData = null;
			try {
				serviceData = new JSONObject(data);
			} catch (Exception e) {
				Log.e(LOG_TAG, functionName + " :: Error parsing service structure.");
				return -1;
			}

			BluetoothGattService bluetoothGattService;
			try {
				bluetoothGattService = createGattService(serviceData, functionName);
			} catch (Exception e) {
				Log.e(LOG_TAG, functionName + " :: " + e.getMessage());
				return -1;
			}

			return queueServiceAddTask(bluetoothGattServer, bluetoothGattService, functionName);
		}

		public double gattServerClearServices(String functionName) {
			if (bluetoothGattServer == null) return FALSE;
			bluetoothGattServer.clearServices();
			return TRUE;
		}

		public double gattServerCloseAsync(String functionName) {
			if (bluetoothGattServer == null) return -1;

			bluetoothGattServer.close();
			bluetoothGattServer.clearServices();
			bluetoothGattServer = null;

			pendingRequestResponses.clear();
			stopCleanupTask();
			
			int asyncId = asyncTokenGenerator.getAndIncrement();
			notifyAsyncOperationSuccess(functionName, asyncId, null);
			return (double) asyncId;
		}

		public double gattServerRespondRead(int requestId, int status, String value, String functionName) {
			if (bluetoothGattServer == null) return FALSE;

			PendingRequestResponse pendingRequestResponse = pendingRequestResponses.get(requestId);
			if (pendingRequestResponse == null) return FALSE;

			if (pendingRequestResponse.type == PendingRequestResponse.WRITE) return FALSE;

			pendingRequestResponses.remove(requestId);

			byte[] data = null;
			
			// If the status is not valid (send null value)
			if (status == BluetoothGatt.GATT_SUCCESS) {
				data = decodeBase64String(value, functionName);
			}

			return bluetoothGattServer.sendResponse(pendingRequestResponse.device, requestId, status, 0, data) ? TRUE : FALSE;
		}

		public double gattServerRespondWrite(int requestId, int status, String functionName) {
			if (bluetoothGattServer == null) return FALSE;

			PendingRequestResponse pendingRequestResponse = pendingRequestResponses.get(requestId);
			if (pendingRequestResponse == null) return FALSE;

			if (pendingRequestResponse.type == PendingRequestResponse.READ) return FALSE;

			pendingRequestResponses.remove(requestId);

			return bluetoothGattServer.sendResponse(pendingRequestResponse.device, requestId, status, 0, null) ? TRUE : FALSE;
		}

		public double gattServerNotifyValueAsync(String service, String characteristic, String value, String functionName) {

			if (bluetoothGattServer == null) return -1;

			BluetoothGattCharacteristic gattCharacteristic = getBluetoothGattCharateristic(bluetoothGattServer, service, characteristic, functionName);
			if (gattCharacteristic == null) return -1;

			byte[] data = decodeBase64String(value, functionName);
			if (data == null) return -1;

			// Generate a new async identifier
			int asyncId = asyncTokenGenerator.getAndIncrement();

			// Notify subscribed devices of the change (TODO)
			for (BluetoothDevice connectedDevice : bluetoothManager.getConnectedDevices(BluetoothProfile.GATT_SERVER)) {
				createTaskAndHandleQueue(notificationTasks, () ->
					new BLENotificationTask(bluetoothGattServer, connectedDevice, gattCharacteristic, data, functionName, asyncId)
				);
			}

			return asyncId;
		}

	}

	BluetoothGattServerManager bluetoothGattServerManager = new BluetoothGattServerManager();

	public double bt_le_server_open() {
		return bluetoothGattServerManager.gattServerOpenAsync("bt_le_server_open");
	}

	public double bt_le_server_add_service(String data) {
		return bluetoothGattServerManager.gattServerAddServiceAsync(data, "bt_le_server_add_service");
	}

	public double bt_le_server_clear_services() {
		return bluetoothGattServerManager.gattServerClearServices("bt_le_server_clear_services");
	}

	public double bt_le_server_close() {
		return bluetoothGattServerManager.gattServerCloseAsync("bt_le_server_close");
	}

	public double bt_le_server_respond_read(double requestId, double status, String value) { 
		return bluetoothGattServerManager.gattServerRespondRead((int)requestId, (int)status, value, "bt_le_server_respond_read");
	}

	public double bt_le_server_respond_write(double requestId, double status) { 
		return bluetoothGattServerManager.gattServerRespondWrite((int)requestId, (int)status, "bt_le_server_respond_write");
	}

	public double bt_le_server_notify_value(String service, String characteristic, String value) {
		return bluetoothGattServerManager.gattServerNotifyValueAsync(service, characteristic, value, "bt_le_server_notify_value");
	}

	// CLIENT

	private class BluetoothGattManager {

		public ConcurrentHashMap<String, BluetoothGatt> activeGattConnections = new ConcurrentHashMap<>();

		// TASKS

		private class BLECharacteristicReadTask extends BLEGattTask<BluetoothGatt, BluetoothGattCharacteristic> {

			public BLECharacteristicReadTask(BluetoothGatt gatt, BluetoothGattCharacteristic target, String functionName, int asyncId) {
				super(gatt, target, functionName, asyncId);
			}

			public int run() {
				return gatt.readCharacteristic(target) ? BluetoothStatusCodes.SUCCESS : ERROR_OPERATION_NOT_STARTED;
			}
		}

		private class BLECharacteristicWriteTask extends BLEGattDataTask<BluetoothGatt, BluetoothGattCharacteristic> {

			int writeType;

			public BLECharacteristicWriteTask(BluetoothGatt gatt, BluetoothGattCharacteristic target, byte[] data, int writeType, String functionName, int asyncId) {
				super(gatt, target, data, functionName, asyncId);
				this.writeType = writeType;
			}

			public int run() {
				return setCharacteristicValue(gatt, target, data, writeType);
			}
		}

		private class BLEDescriptorReadTask extends BLEGattTask<BluetoothGatt, BluetoothGattDescriptor> {

			public BLEDescriptorReadTask(BluetoothGatt gatt, BluetoothGattDescriptor target, String functionName, int asyncId) {
				super(gatt, target, functionName, asyncId);
			}

			public int run() {
				return gatt.readDescriptor(target) ? BluetoothStatusCodes.SUCCESS : ERROR_OPERATION_NOT_STARTED;
			}
		}

		private class BLEDescriptorWriteTask extends BLEGattDataTask<BluetoothGatt, BluetoothGattDescriptor> {

			public BLEDescriptorWriteTask(BluetoothGatt gatt, BluetoothGattDescriptor target, byte[] data, String functionName, int asyncId) {
				super(gatt, target, data, functionName, asyncId);
			}

			public int run() {
				return setDescriptorValue(gatt, target, data);
			}
		}

		private class BLEGattOpenTask extends BLEGattTask<BluetoothGatt, BluetoothDevice> {

			BluetoothGattCallback bluetoothGattCallback;

			public BLEGattOpenTask(BluetoothDevice device, BluetoothGattCallback bluetoothGattCallback, String functionName, int asyncId) {
				super(null, device, functionName, asyncId);
				this.bluetoothGattCallback = bluetoothGattCallback;
			}

			public int run() {
				gatt = target.connectGatt(activity, false, bluetoothGattCallback, BluetoothDevice.TRANSPORT_LE);
				return BluetoothStatusCodes.SUCCESS;
			}
		}

		// TASK QUEUES

		public ConcurrentLinkedQueue<BLECharacteristicReadTask> characteristicReadTasks = new ConcurrentLinkedQueue<>();
		public ConcurrentLinkedQueue<BLECharacteristicWriteTask> characteristicWriteTasks = new ConcurrentLinkedQueue<>(); 

		public ConcurrentLinkedQueue<BLEDescriptorReadTask> descriptorReadTasks = new ConcurrentLinkedQueue<>();
		public ConcurrentLinkedQueue<BLEDescriptorWriteTask> descriptorWriteTasks = new ConcurrentLinkedQueue<>();

		public ConcurrentLinkedQueue<BLEGattOpenTask> gattOpenTasks = new ConcurrentLinkedQueue<>();

		// CALLBACK HANDLER

		private class BluetoothGattCallbackHandler extends BluetoothGattCallback {

			BluetoothGattManager gattManager;

			public BluetoothGattCallbackHandler(BluetoothGattManager gattManager) {
				this.gattManager = gattManager;
			}

			@Override
			public void onCharacteristicChanged(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic, byte[] value) {
				
				notifyOperation("bt_le_characteristic_value_changed", Map.of(
					"characteristic_uuid", characteristic.getUuid(),
					"service_uuid", characteristic.getService().getUuid(),
					"address", gatt.getDevice().getAddress(),
					"value", value
				));
			}

			@Override
			public void onCharacteristicRead(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic, byte[] value, int status) {
				handleTaskQueue(gattManager.characteristicReadTasks, status, Map.of(
					"value", value
					));
			}

			@Override
			public void onCharacteristicWrite(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic, int status) {
				handleTaskQueue(gattManager.characteristicWriteTasks, status, null);
			}

			@Override
			public void onConnectionStateChange(BluetoothGatt gatt, int status, int newState) {
				
				BluetoothDevice device = gatt.getDevice();

				// Publish the Async Event to the runner
				notifyOperation("bt_le_peripheral_connection_state_changed", Map.of(
					"is_connected", newState == BluetoothProfile.STATE_CONNECTED,
					"is_paired", device.getBondState() == BluetoothDevice.BOND_BONDED,
					"address", device.getAddress(),
					"name", device.getName() != null ? device.getName() : "" // Optional
				));

				BLEGattTask<BluetoothGatt, BluetoothDevice> openTask = gattManager.gattOpenTasks.peek();
				if (openTask == null) return;

				// Check if it is an openTask
				if (openTask.gatt.getDevice().getAddress() == gatt.getDevice().getAddress()) {
					
					// Start discovery
					if (newState == BluetoothProfile.STATE_CONNECTED) {
						gatt.discoverServices();
					}
					else {
						gatt.close();

						BLEGattOpenTask task = gattOpenTasks.remove();
						notifyAsyncOperationError(task.functionName, task.asyncId, status, null);
						
						// Start next one
						handleNextTask(gattOpenTasks);
					}
				}
			}
			
			@Override
			public void onDescriptorRead(BluetoothGatt gatt, BluetoothGattDescriptor descriptor, int status, byte[] value) {
				handleTaskQueue(gattManager.descriptorReadTasks, status, Map.of(
					"value", value
					));
			}

			@Override
			public void onDescriptorWrite(BluetoothGatt gatt, BluetoothGattDescriptor descriptor, int status) {
				handleTaskQueue(gattManager.descriptorWriteTasks, status, null);
			}

			@Override
			public void onMtuChanged(BluetoothGatt gatt, int mtu, int status) {

				// Windows doesn't have this information
			}

			@Override
			public void onPhyRead(BluetoothGatt gatt, int txPhy, int rxPhy, int status) {

				// Windows doesn't have this information
			}

			@Override
			public void onPhyUpdate(BluetoothGatt gatt, int txPhy, int rxPhy, int status) {
				
				// Windows doesn't have this information
			}

			@Override
			public void onReadRemoteRssi(BluetoothGatt gatt, int rssi, int status) {

				// Windows doesn't have this information
			}

			@Override
			public void onReliableWriteCompleted(BluetoothGatt gatt, int status) {

				// We don't support reliable writes
			}

			@Override
			public void onServiceChanged(BluetoothGatt gatt) {

				BluetoothDevice device = gatt.getDevice();

				// Publish the Async Event to the runner
				notifyOperation("bt_le_peripheral_service_changed", Map.of(
					"address", device.getAddress(),
					"name", device.getName()
				));
			}

			@Override
			public void onServicesDiscovered(BluetoothGatt gatt, int status) {
				
				if (status == BluetoothGatt.GATT_SUCCESS) {
					gattManager.activeGattConnections.put(gatt.getDevice().getAddress(), gatt);
				}
				else gatt.close();

				BluetoothDevice device = gatt.getDevice();

				handleTaskQueue(gattManager.gattOpenTasks, status, Map.of(
								"is_paired", device.getBondState() == BluetoothDevice.BOND_BONDED,
								"address", device.getAddress(),
								"name", device.getName() != null ? device.getName() : "" // Optional
							));
			}
		}

		private BluetoothGattCallbackHandler bluetoothGattCallbackHandler = new BluetoothGattCallbackHandler(this);

		// INTERNAL

		private int setCharacteristicValue(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic, byte[] value, int writeType) {
			if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
				return gatt.writeCharacteristic(characteristic, value, writeType);
			} else {
				characteristic.setValue(value);
				return gatt.writeCharacteristic(characteristic) ? BluetoothStatusCodes.SUCCESS : BluetoothStatusCodes.ERROR_UNKNOWN;
			}
		}
	
		private int setDescriptorValue(BluetoothGatt gatt, BluetoothGattDescriptor descriptor, byte[] value) {
			if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
				return gatt.writeDescriptor(descriptor, value);
			} else {
				descriptor.setValue(value);
				return gatt.writeDescriptor(descriptor) ? BluetoothStatusCodes.SUCCESS : BluetoothStatusCodes.ERROR_UNKNOWN;
			}
		}

		private BluetoothGatt getBluetoothGatt(String address, String functionName) {



			BluetoothGatt gatt = activeGattConnections.get(address);
			if (gatt == null) {
				Log.i(LOG_TAG, functionName + " :: Gatt with address '" + address + "' not found.");
			}
			return gatt;
		}

		private BluetoothGattService getBluetoothGattService(BluetoothGatt gatt, String service, String functionName) {
			
			UUID serviceUUID = getUUIDFromString(service, functionName);
			if (serviceUUID == null) return null;

			BluetoothGattService gattService = gatt.getService(serviceUUID);
			if (gattService == null) {
				Log.i(LOG_TAG, functionName + " :: Service with uuid '" + service + "' not found.");
			}		

			return gattService;
		}

		private BluetoothGattCharacteristic getBluetoothGattCharateristic(BluetoothGatt gatt, String service, String characteristic, String functionName) {

			BluetoothGattService gattService = getBluetoothGattService(gatt, service, functionName);
			if (gattService == null) return null;

			UUID characteristicUUID = getUUIDFromString(characteristic, functionName);
			if (characteristic == null) return null;

			BluetoothGattCharacteristic gattCharacteristic = gattService.getCharacteristic(characteristicUUID);
			if (gattCharacteristic == null) {
				Log.i(LOG_TAG, functionName + " :: Characteristic with uuid '" + characteristic + "' not found.");
			}
		
			return gattCharacteristic;
		}

		private BluetoothGattDescriptor getBluetoothGattDescriptor(BluetoothGatt gatt, String service, String characteristic, String descriptor, String functionName) {

			BluetoothGattCharacteristic gattCharacteristic = getBluetoothGattCharateristic(gatt, service, characteristic, functionName);
			if (gattCharacteristic == null) return null;

			UUID descriptorUUID = getUUIDFromString(descriptor, functionName);
			if (descriptorUUID == null) return null;

			BluetoothGattDescriptor gattDescriptor = gattCharacteristic.getDescriptor(descriptorUUID);
			if (gattDescriptor == null) {
				Log.i(LOG_TAG, functionName + " :: Descriptor with uuid '" + descriptor + "' not found.");
			}

			return gattDescriptor;
		}

		private double queueGattOpenTask(BluetoothDevice device, String functionName) {
			int asyncId = asyncTokenGenerator.getAndIncrement();
			createTaskAndHandleQueue(gattOpenTasks, () -> new BLEGattOpenTask(device, bluetoothGattCallbackHandler, functionName, asyncId));
			return asyncId;
		}

		private double queueCharacteristicReadTask(BluetoothGatt gatt, BluetoothGattCharacteristic gattCharacteristic, String functionName) {
			int asyncId = asyncTokenGenerator.getAndIncrement();
			createTaskAndHandleQueue(characteristicReadTasks, () -> new BLECharacteristicReadTask(gatt, gattCharacteristic, functionName, asyncId));
			return asyncId;
		}

		private double queueCharacteristicWriteTask(BluetoothGatt gatt, BluetoothGattCharacteristic gattCharacteristic, byte[] data, int writeType, String functionName) {
			int asyncId = asyncTokenGenerator.getAndIncrement();
			createTaskAndHandleQueue(characteristicWriteTasks, () -> new BLECharacteristicWriteTask(gatt, gattCharacteristic, data, writeType, functionName, asyncId));
			return asyncId;
		}

		private double queueDescriptorReadTask(BluetoothGatt gatt, BluetoothGattDescriptor gattDescriptor, String functionName) {
			int asyncId = asyncTokenGenerator.getAndIncrement();
			createTaskAndHandleQueue(descriptorReadTasks, () -> new BLEDescriptorReadTask(gatt, gattDescriptor, functionName, asyncId));
			return asyncId;
		}

		private double queueDescriptorWriteTask(BluetoothGatt gatt, BluetoothGattDescriptor gattDescriptor, byte[] data, String functionName) {
			int asyncId = asyncTokenGenerator.getAndIncrement();
			createTaskAndHandleQueue(descriptorWriteTasks, () -> new BLEDescriptorWriteTask(gatt, gattDescriptor, data, functionName, asyncId));
			return asyncId;
		}

		// PUBLIC API

		public double characteristicReadAsync(String address, String service, String characteristic, String functionName) {

			BluetoothGatt gatt = getBluetoothGatt(address, functionName);
			if (gatt == null) return -1;

			BluetoothGattCharacteristic gattCharacteristic = getBluetoothGattCharateristic(gatt, service, characteristic, functionName);
			if (gattCharacteristic == null) return -1;

			return queueCharacteristicReadTask(gatt, gattCharacteristic, functionName);
		}

		public double characteristicWriteAsync(String address, String service, String characteristic, String value, int writeType, String functionName) {

			BluetoothGatt gatt = getBluetoothGatt(address, functionName);
			if (gatt == null) return -1;

			BluetoothGattCharacteristic gattCharacteristic = getBluetoothGattCharateristic(gatt, service, characteristic, functionName);
			if (gattCharacteristic == null) return -1;

			byte[] data = decodeBase64String(value, functionName);
			if (data == null) return -1;

			return queueCharacteristicWriteTask(gatt, gattCharacteristic, data, writeType, functionName);
		}

		public double characteristicSubscribeAsync(String address, String service, String characteristic, byte[] data, String functionName) {

			BluetoothGatt gatt = getBluetoothGatt(address, functionName);
			if (gatt == null) return -1;

			BluetoothGattCharacteristic gattCharacteristic = getBluetoothGattCharateristic(gatt, service, characteristic, functionName);
			if (gattCharacteristic == null) return -1;

			BluetoothGattDescriptor gattDescriptor = gattCharacteristic.getDescriptor(CCCD_UUID);
			if (gattDescriptor == null) return -1;

			// Enable local notifications (if not a disable action)
			gatt.setCharacteristicNotification(gattCharacteristic, !Arrays.equals(data, BluetoothGattDescriptor.DISABLE_NOTIFICATION_VALUE));

			return queueDescriptorWriteTask(gatt, gattDescriptor, data, functionName);
		}

		public double descriptorReadAsync(String address, String service, String characteristic, String descriptor, String functionName) {

			BluetoothGatt gatt = getBluetoothGatt(address, functionName);
			if (gatt == null) return -1;

			BluetoothGattDescriptor gattDescriptor = getBluetoothGattDescriptor(gatt, service, characteristic, descriptor, functionName);
			if (gattDescriptor == null) return -1;

			return queueDescriptorReadTask(gatt, gattDescriptor, functionName);
		}

		public double descriptorWriteAsync(String address, String service, String characteristic, String descriptor, String value, String functionName) {

			BluetoothGatt gatt = getBluetoothGatt(address, functionName);
			if (gatt == null) return -1;

			BluetoothGattDescriptor gattDescriptor = getBluetoothGattDescriptor(gatt, service, characteristic, descriptor, functionName);
			if (gattDescriptor == null) return -1;

			byte[] data = decodeBase64String(value, functionName);
			if (data == null) return -1;

			return queueDescriptorWriteTask(gatt, gattDescriptor, data, functionName);
		}

		public double getServicesAsync(String address, String functionName) {

			BluetoothGatt gatt = getBluetoothGatt(address, functionName);
			if (gatt == null) return -1;

			List<BluetoothGattService> services = gatt.getServices();
			JSONArray servicesJSONArray = createServicesJsonArray(services);

			int asyncId = asyncTokenGenerator.getAndIncrement();
			notifyAsyncOperationSuccess(functionName, asyncId, Map.of("services", servicesJSONArray.toString()));
			return (double)asyncId;
		}

		public double getCharacteristicsAsync(String address, String service, String functionName) {

			BluetoothGatt gatt = getBluetoothGatt(address, functionName);
			if (gatt == null) return -1;

			BluetoothGattService gattService = getBluetoothGattService(gatt, service, functionName);
			if (gattService == null) return -1;

			List<BluetoothGattCharacteristic> characteristics = gattService.getCharacteristics();
			JSONArray characteristicsJSONArray = createCharacteristicsJsonArray(characteristics);

			int asyncId = asyncTokenGenerator.getAndIncrement();
			notifyAsyncOperationSuccess(functionName, asyncId, Map.of("characteristics", characteristicsJSONArray));
			return (double)asyncId;
		}

		public double getDescriptorsAsync(String address, String service, String characteristic, String functionName) {

			BluetoothGatt gatt = getBluetoothGatt(address, functionName);
			if (gatt == null) return -1;

			BluetoothGattCharacteristic gattCharacteristic = getBluetoothGattCharateristic(gatt, service, characteristic, functionName);
			if (gattCharacteristic == null) return -1;

			List<BluetoothGattDescriptor> descriptors = gattCharacteristic.getDescriptors();
			JSONArray descriptorsJSONArray = createDescriptorJsonArray(descriptors);

			int asyncId = asyncTokenGenerator.getAndIncrement();
			notifyAsyncOperationSuccess(functionName, asyncId, Map.of("descriptors", descriptorsJSONArray));
			return (double)asyncId;
		}

		public double gattOpenAsync(String address, String functionName) {

			if (activeGattConnections.containsKey(address)) return -1;

			BluetoothDevice device = getBluetoothDevice(address,  functionName);
			if (device == null) return -1;

			return queueGattOpenTask(device, functionName);
		}

		public double gattIsOpen(String address) {
			return activeGattConnections.containsKey(address) ? TRUE : FALSE;
		}

		public double gattClose(String address, String functionName) {

			BluetoothGatt gatt = getBluetoothGatt(address, functionName);
			if (gatt == null) return FALSE;

			gatt.close();
			activeGattConnections.remove(address);
			return TRUE;
		}

		public double gattCloseAll() {
			for (BluetoothGatt gatt : activeGattConnections.values()) {
				gatt.close();
			}
			activeGattConnections.clear();
			return TRUE;
		}

	}

	private BluetoothGattManager bluetoothGattManager = new BluetoothGattManager();
	
	public double bt_le_peripheral_open(String address) {
		return bluetoothGattManager.gattOpenAsync(address, "bt_le_peripheral_open");
	}

	public double bt_le_peripheral_is_open(String address) {
		return bluetoothGattManager.gattIsOpen(address);
	}

	public double bt_le_peripheral_close_all() {
		return bluetoothGattManager.gattCloseAll();
	}
	
	public double bt_le_peripheral_close(String address) {
		return bluetoothGattManager.gattClose(address, "bt_le_peripheral_close");
	}

	public double bt_le_peripheral_is_connected(String address) {
		List<BluetoothDevice> devices = bluetoothManager.getConnectedDevices(BluetoothProfile.GATT);

		for (BluetoothDevice device : devices) {
			if (device.getAddress().equals(address)) {
				return TRUE;
			}
		}
		return FALSE;
	}
	
	public double bt_le_peripheral_is_paired(String address) {
		BluetoothAdapter bluetoothAdapter = getBluetoothAdapter("bt_le_peripheral_is_paired");
		BluetoothDevice device = bluetoothAdapter.getRemoteDevice(address);

		return device.getBondState() == BluetoothDevice.BOND_BONDED ? TRUE : FALSE;
	}
	
	public double bt_le_peripheral_get_services(String address) {
		return bluetoothGattManager.getServicesAsync(address, "bt_le_peripheral_get_services");
	}

	public double bt_le_service_get_characteristics(String address, String service) {
		return bluetoothGattManager.getCharacteristicsAsync(address, service, "bt_le_service_get_characteristics");
	}

	public double bt_le_characteristic_get_descriptors(String address, String service, String characteristic) {
		return bluetoothGattManager.getDescriptorsAsync(address, service, characteristic, "bt_le_characteristic_get_descriptors");
	}
	
	public double bt_le_characteristic_read(String address, String service, String characteristic) {
		return bluetoothGattManager.characteristicReadAsync(address, service, characteristic, "bt_le_characteristic_read");
	}

	public double bt_le_characteristic_write_request(String address, String service, String characteristic, String value) {
		return bluetoothGattManager.characteristicWriteAsync(address, service, characteristic, value, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT, "bt_le_characteristic_write_request");
	}

	public double bt_le_characteristic_write_command(String address, String service, String characteristic, String value) {
		return bluetoothGattManager.characteristicWriteAsync(address, service, characteristic, value, BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE, "bt_le_characteristic_write_command");
	}

	public double bt_le_characteristic_notify(String address, String service, String characteristic) {
		return bluetoothGattManager.characteristicSubscribeAsync(address, service, characteristic, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE, "bt_le_characteristic_notify");
	}

	public double bt_le_characteristic_indicate(String address, String service, String characteristic) {
		return bluetoothGattManager.characteristicSubscribeAsync(address, service, characteristic, BluetoothGattDescriptor.ENABLE_INDICATION_VALUE, "bt_le_characteristic_indicate");
	}

	public double bt_le_characteristic_unsubscribe(String address, String service, String characteristic) {
		return bluetoothGattManager.characteristicSubscribeAsync(address, service, characteristic, BluetoothGattDescriptor.DISABLE_NOTIFICATION_VALUE, "bt_le_characteristic_unsubscribe");
	}

	public double bt_le_descriptor_read(String address, String service, String characteristic, String descriptor) {
		return bluetoothGattManager.descriptorReadAsync(address, service, characteristic, descriptor, "bt_le_descriptor_read");
	}

	public double bt_le_descriptor_write(String address, String service, String characteristic, String descriptor, String value) {
		return bluetoothGattManager.descriptorWriteAsync(address, service, characteristic, descriptor, value, "bt_le_descriptor_write");
	}

	// PRIVATE METHODS
	
	private UUID getUUIDFromString(String uuid, String functionName) {
		try {
			return UUID.fromString(uuid);
		} catch (IllegalArgumentException e) {
			Log.e(LOG_TAG, functionName + " :: Error invalid UUID string", e);
		}
		return null;
	}

	private byte[] decodeBase64String(String value, String functionName) {
		try {
			return Base64.getDecoder().decode(value);
		} catch (IllegalArgumentException e) {
			Log.e(LOG_TAG, functionName + " :: Error invalid base64 string", e);
		}
		return null;
	}

	private boolean isBluetoothEnabled(String functionName) 
	{
		BluetoothAdapter bluetoothAdapter = getBluetoothAdapter(functionName);
		if (bluetoothAdapter == null) return false;
		
		if (!bluetoothAdapter.isEnabled()) {
			Log.i(LOG_TAG, functionName + " :: Bluetooth is not enabled.");
			return false;
		}
		return true;
	}

	private boolean isBluetoothLeSupported(String functionName) 
	{
		if (activity.getPackageManager().hasSystemFeature(PackageManager.FEATURE_BLUETOOTH_LE)) return true;
		
		Log.i(LOG_TAG, functionName + " :: Bluetooth LE is not supported on this device.");
		return false;
	}

	private BluetoothAdapter getBluetoothAdapter(String functionName) {
		BluetoothAdapter bluetoothAdapter = bluetoothManager.getAdapter();
		if (bluetoothAdapter == null) {
			Log.i(LOG_TAG, functionName + " :: Bluetooth is not supported on this device.");
		}
		return bluetoothAdapter;
	}

	private BluetoothDevice getBluetoothDevice(String address, String functionName) {

		BluetoothAdapter bluetoothAdapter = getBluetoothAdapter(functionName);
		if (bluetoothAdapter == null) return null;

		BluetoothDevice device = bluetoothAdapter.getRemoteDevice(address);
		if (device == null) {
			Log.i(LOG_TAG, functionName + " :: Device with address '" + address + "' not found.");
		}
		return device;
	}

	private JSONObject createBluetoothDeviceJson(BluetoothDevice device) {
		JSONObject deviceJson = new JSONObject();
		try {
			deviceJson.put("name", device.getName() != null ? device.getName() : "");
			deviceJson.put("address", device.getAddress());
			deviceJson.put("is_paired", device.getBondState() == BluetoothDevice.BOND_BONDED);
			deviceJson.put("type", device.getType());
			deviceJson.put("alias", device.getAlias());
		} catch (JSONException e) {
			Log.e(LOG_TAG, "Failed to create bluetooth device JSON object", e);
		}
		return deviceJson;

	}

	private JSONArray createServicesJsonArray(List<BluetoothGattService> services) {
		JSONArray servicesJsonList = new JSONArray();
		for (BluetoothGattService service : services) {
			servicesJsonList.put(createServiceJson(service));
		}
		return servicesJsonList;
	}

	private JSONObject createServiceJson(BluetoothGattService service) {
		JSONObject serviceJson = new JSONObject();
		try {
			serviceJson.put("type", service.getType());
			serviceJson.put("uuid", service.getUuid().toString().toUpperCase());
		} catch (JSONException e) {
			Log.e(LOG_TAG, "Failed to create bluetooth gatt service JSON object", e);
		}

		return serviceJson;
	}

	private JSONArray createCharacteristicsJsonArray(List<BluetoothGattCharacteristic> characteristics) {
		JSONArray characteristicsJsonList = new JSONArray();
		for (BluetoothGattCharacteristic characteristic : characteristics) {
			characteristicsJsonList.put(createCharacteristicJson(characteristic));
		}

		return characteristicsJsonList;
	}

	private JSONObject createCharacteristicJson(BluetoothGattCharacteristic characteristic) {
		JSONObject characteristicJson = new JSONObject();
		try {
			characteristicJson.put("permissions", characteristic.getPermissions());
			characteristicJson.put("uuid", characteristic.getUuid().toString().toUpperCase());
			characteristicJson.put("properties", characteristic.getProperties());
		} catch (JSONException e) {
			Log.e(LOG_TAG, "Failed to create bluetooth gatt characteristic JSON object", e);
		}
		return characteristicJson;
	}

	private JSONArray createDescriptorJsonArray(List<BluetoothGattDescriptor> descriptors) {
		JSONArray descriptorJsonList = new JSONArray();
		for (BluetoothGattDescriptor descriptor : descriptors) {
			descriptorJsonList.put(createDescriptorJson(descriptor));
		}

		return descriptorJsonList;
	}

	private JSONObject createDescriptorJson(BluetoothGattDescriptor descriptor) {
		JSONObject descriptorJson = new JSONObject();
		try {
			descriptorJson.put("uuid", descriptor.getUuid().toString().toUpperCase());
		} catch (JSONException e) {
			Log.e(LOG_TAG, "Failed to create bluetooth gatt descriptor JSON object", e);
		}
		return descriptorJson;
	}

	//#endregion

}