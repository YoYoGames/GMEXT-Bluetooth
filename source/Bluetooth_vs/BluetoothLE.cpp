

#include "BluetoothCore.h"
#include "BluetoothTools.h"

std::map<uint32_t, event_token> tokens = {};

std::mutex devicesMutex;
std::map<std::string, BluetoothLEDevice> devices = {};

std::wstring advertisementTypeToString(BluetoothLEAdvertisementType advertisementType)
{
	switch (advertisementType)
	{
		case BluetoothLEAdvertisementType::ConnectableUndirected:return L"ConnectableUndirected";break;
		case BluetoothLEAdvertisementType::ConnectableDirected:return L"ConnectableDirected";break;
		case BluetoothLEAdvertisementType::ScannableUndirected:return L"ScannableUndirected";break;
		case BluetoothLEAdvertisementType::NonConnectableUndirected:return L"NonConnectableUndirected";break;
		case BluetoothLEAdvertisementType::ScanResponse:return L"ScanResponse";break;
		default:return NULL;
	}
}

BluetoothLEAdvertisementPublisher publisher = nullptr;

std::string GuidToString(winrt::guid const& uuid) {
	char buffer[37]; // length of a GUID string with null-terminating character
	snprintf(buffer, sizeof(buffer),
		"%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
		(unsigned long)uuid.Data1, uuid.Data2, uuid.Data3,
		uuid.Data4[0], uuid.Data4[1], uuid.Data4[2], uuid.Data4[3],
		uuid.Data4[4], uuid.Data4[5], uuid.Data4[6], uuid.Data4[7]);

	return std::string(buffer);
}

void notifyScanResult(BluetoothLEAdvertisementReceivedEventArgs const& args)
{
	int map = CreateDsMap(0, 0);
	DsMapAddString(map, "type", "bt_le_scan_result");
	DsMapAddString(map, "name", to_string(args.Advertisement().LocalName()).c_str());
	DsMapAddString(map, "address", AddressToString(args.BluetoothAddress()).c_str());
	DsMapAddDouble(map, "raw_signal", (double)args.RawSignalStrengthInDBm());
	DsMapAddBool(map, "is_connectable", args.IsConnectable());

	RValue uuidsArray = { 0 };
	YYCreateArray(&uuidsArray);

	auto uuids = args.Advertisement().ServiceUuids();
	for (int32_t i = uuids.Size() - 1; i >= 0; --i) {
		auto uuid = uuids.GetAt(i);

		RValue uuidRValue = { 0 };
		YYCreateString(&uuidRValue, GuidToString(uuid).c_str());

		SET_RValue(&uuidsArray, &uuidRValue, NULL, i);
	}

	DsMapAddRValue(map, "uuids", &uuidsArray);

	CreateAsyncEventWithDSMap(map, 70);
}

void notifyScanStopSuccess() {
	int map = CreateDsMap(0, 0);
	DsMapAddString(map, "type", "bt_le_scan_stop");
	DsMapAddBool(map, "success", true);
	CreateAsyncEventWithDSMap(map, 70);
}

void notifyScanStopError(int32_t errorCode) {
	int map = CreateDsMap(0, 0);
	DsMapAddString(map, "type", "bt_le_scan_stop");
	DsMapAddBool(map, "success", false);
	CreateAsyncEventWithDSMap(map, 70);
}

BluetoothLEAdvertisementWatcher watcher;

void bluetoothLeInit() {
	static bool initialized = false;

	if (initialized) return;
	initialized = true;

	winrtInit();

	// Configure watcher
	watcher.ScanningMode(BluetoothLEScanningMode::Active);

	watcher.Received([](const BluetoothLEAdvertisementWatcher &watcher, const BluetoothLEAdvertisementReceivedEventArgs &eventArgs)
		{
			notifyScanResult(eventArgs);
		});

	watcher.Stopped([](const BluetoothLEAdvertisementWatcher &watcher, const BluetoothLEAdvertisementWatcherStoppedEventArgs &eventArgs)
		{
			BluetoothError error = eventArgs.Error();
			if (error != BluetoothError::Success) {
				notifyScanStopError((int32_t)error);
			}
			else notifyScanStopSuccess();
		});
}

// This will be used for an internal representation of the server
// The server will read and write from here.

struct GMDescriptor {
	winrt::guid guid;
	Storage::Streams::IBuffer value;
};

struct GMCharacteristic {
	winrt::guid guid;
	Storage::Streams::IBuffer value;
	hstring description;

	GattCharacteristicProperties properties;
	uint32_t permissions = 1 | 16;
	std::vector<GMDescriptor> descriptors;
};

struct GMService {
	winrt::guid guid;
	std::vector<GMCharacteristic> characteristics;
};

// This map will hold the event registration tokens for each characteristic's ValueChanged event
std::mutex subscriptionsMutex;
std::map<std::string, GattCharacteristic> subscriptions;

// This map will hold service providers to allow Gatt peripheral behavior
std::mutex serviceProvidersMutex;
std::map<std::string, GattServiceProvider> serviceProviders = {};
std::map<std::string, GMService> serviceData = {};

// MANAGEMENT

YYEXPORT void bt_le_init(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
}

YYEXPORT void bt_le_end(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	watcher.Stop();

	for (auto& pair : devices) {
		BluetoothLEDevice& device = pair.second;
		device.Close();
	}

	devices.clear();
	tokens.clear();
	serviceProviders.clear();
}

// ADAPTER

YYEXPORT void bluetooth_is_enabled(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	bool enabled = false;

	run_sync(BluetoothAdapter::GetDefaultAsync(), [&enabled](IAsyncOperation<BluetoothAdapter> asyncInfo) {
		
		BluetoothAdapter adapter = asyncInfo.GetResults();
		if (adapter == nullptr) {
			enabled = false;
		}
		else enabled = true;
		return true;
		});

	Result.kind = VALUE_BOOL;
	Result.val = enabled;
}

YYEXPORT void bt_le_is_supported(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg) {

	bool supported = false;

	run_sync(BluetoothAdapter::GetDefaultAsync(), [&supported](IAsyncOperation<BluetoothAdapter> asyncInfo) {

		BluetoothAdapter adapter = asyncInfo.GetResults();
		if (adapter == nullptr) {
			supported = false;
		}
		else supported = adapter.IsLowEnergySupported();
		return true;
		});

	Result.kind = VALUE_BOOL;
	Result.val = supported;
}

// SCANNER

std::atomic_bool isScanning = false;

YYEXPORT void bt_le_scan_start(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_INT64;
	Result.v64 = -1;

	if (isScanning) return;

	watcher.Start();
	isScanning = true;

	int64_t asyncId = getNextAsyncId();

	notifyAsyncOperationSuccess(__FUNCTION__, asyncId);
	Result.v64 = asyncId;
}

YYEXPORT void bt_le_scan_stop(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_INT64;
	Result.v64 = -1;

	if (!isScanning) return;

	watcher.Stop();
	isScanning = false;

	int64_t asyncId = getNextAsyncId();

	notifyAsyncOperationSuccess(__FUNCTION__, asyncId);
	Result.v64 = asyncId;
}

YYEXPORT void bt_le_scan_is_active(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_BOOL;
	Result.val = watcher.Status() == BluetoothLEAdvertisementWatcherStatus::Started;
}

YYEXPORT void bt_le_get_paired_devices(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	auto selector = BluetoothLEDevice::GetDeviceSelector();
	run_sync(DeviceInformation::FindAllAsync(selector), [&Result](IAsyncOperation<DeviceInformationCollection> asyncInfo) {
		auto devices = asyncInfo.GetResults();

		for (const auto& device : devices) {
			run_sync(BluetoothLEDevice::FromIdAsync(device.Id()), [&Result](IAsyncOperation<BluetoothLEDevice> asyncInfo) {

				RValue infoStruct = { 0 };
				YYStructCreate(&infoStruct);

				auto device = asyncInfo.GetResults();
				YYStructAddString(&infoStruct, "type", "bluetooth_win_le_open_device");
				YYStructAddString(&infoStruct, "name", to_string(device.DeviceInformation().Name()).c_str());
				YYStructAddString(&infoStruct, "id", to_string(device.DeviceInformation().Id()).c_str());
				YYStructAddBool(&infoStruct, "default", device.DeviceInformation().IsDefault());
				YYStructAddBool(&infoStruct, "enabled", device.DeviceInformation().IsEnabled());
				YYStructAddDouble(&infoStruct, "kind", (double)device.DeviceInformation().Kind());
				YYStructAddBool(&infoStruct, "pairing", device.DeviceInformation().Pairing().IsPaired());
				YYStructAddString(&infoStruct, "address", AddressToString(device.BluetoothAddress()).c_str());
				YYStructAddBool(&infoStruct, "connected", device.ConnectionStatus() == BluetoothConnectionStatus::Connected);
				YYStructAddString(&infoStruct, "device_id", to_string(device.DeviceId()).c_str());
				});


		}
		});
}

// ADVERTISER

bool tryGetIBuffer(const char* data, Storage::Streams::IBuffer& buffer, const std::string& functionName) {
	try {
		// Decode the base64 string into a buffer
		buffer = CryptographicBuffer::DecodeFromBase64String(to_hstring(data));
		return true;
	}
	catch (winrt::hresult_error const& ex)
	{
		// Print or log the error message
		DebugConsoleOutput("%s :: %s\n", functionName, to_string(ex.message()).c_str());
	}
	return false;
}

bool tryGetUuidFromRValue(RValue* input, winrt::guid& result, const char* functionName) {
	if (input == nullptr) {
		return false;
	}
	else if (input->kind != VALUE_STRING || input->pRefString == NULL) {
		YYError("%s :: Invalid type 'guid' must be a string", functionName);
	}
	else {
		try {
			result = winrt::guid(input->GetString());
			return true;
		}
		catch (hresult_invalid_argument const&) {
			YYError("%s :: Invalid 'uuid' value", functionName);
		}
	}
	return false;
}

YYEXPORT void bt_le_advertise_start(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_INT64;
	Result.v64 = -1;

	// Only create a publisher the first time around
	if (publisher == nullptr) {
		// Initialize the publisher

		publisher = BluetoothLEAdvertisementPublisher();

		// Set up a callback function for the StatusChanged event
		publisher.StatusChanged([](BluetoothLEAdvertisementPublisher const& publisher, BluetoothLEAdvertisementPublisherStatusChangedEventArgs const& args) {
			auto status = args.Status();
			switch (status) {
			case BluetoothLEAdvertisementPublisherStatus::Aborted:
				// The publisher was aborted
				DebugConsoleOutput("Publisher was aborted with error: %d\n", args.Error());
				break;
			case BluetoothLEAdvertisementPublisherStatus::Started:
				// The publisher started successfully
				DebugConsoleOutput("Publisher started successfully\n");
				break;
			case BluetoothLEAdvertisementPublisherStatus::Stopped:
				// The publisher stopped
				DebugConsoleOutput("Publisher stopped\n");
				break;
			case BluetoothLEAdvertisementPublisherStatus::Waiting:
				// The publisher is waiting to transmit
				DebugConsoleOutput("Publisher is waiting to transmit\n");
				break;
			default:
				break;
			}
			});
	}

	// Settings

	RValue* settingsStruct = YYGetStruct(arg, 0);
	RValue* txPowerLevel = YYStructGetMember(settingsStruct, "txPowerLevel");
	if (txPowerLevel != NULL) {
		if ((txPowerLevel->kind != VALUE_REAL) && (txPowerLevel->kind != VALUE_INT32) && (txPowerLevel->kind != VALUE_INT64)) {
			YYError("%s :: Settings 'txPowerLevel' member should be numeric", __FUNCTION__);
			return;
		}
		publisher.PreferredTransmitPowerLevelInDBm((int16_t)YYGetReal(txPowerLevel, 0));
	}

	bool isConnectable = true;
	RValue* connectableBool = YYStructGetMember(settingsStruct, "connectable");
	if (connectableBool != NULL) {
		if ((connectableBool->kind != VALUE_BOOL)) {
			YYError("%s :: Settings 'connectable' member should be boolean", __FUNCTION__);
			return;
		}
		isConnectable = YYGetBool(connectableBool, 0);
	}

	bool isDiscoverable = true;
	RValue* discoverableBool = YYStructGetMember(settingsStruct, "discoverable");
	if (discoverableBool != NULL) {
		if ((discoverableBool->kind != VALUE_BOOL)) {
			YYError("%s :: Settings 'discoverable' member should be boolean", __FUNCTION__);
			return;
		}
		isDiscoverable = YYGetBool(discoverableBool, 0);
	}

	// Data

	auto advertisement = publisher.Advertisement();

	RValue* dataStruct = YYGetStruct(arg, 1);
	//RValue* name = YYStructGetMember(dataStruct, "name");
	//if (name != nullptr && name->pRefString != nullptr) {
	//	if (name->kind != VALUE_STRING) {
	//		YYError("%s :: Data 'name' member should be string", __FUNCTION__);
	//		return;
	//	}
	//	advertisement.LocalName(to_hstring(YYGetString(name, 0)));
	//}

	RValue* includeName = YYStructGetMember(dataStruct, "includeName");
	if (includeName != nullptr) {
		if (includeName->kind != VALUE_BOOL) {
			YYError("%s :: Data 'includeName' member should be boolean", __FUNCTION__);
			return;
		}
		publisher.IsAnonymous(!YYGetBool(includeName, 0));
	}

	RValue* includePowerLevel = YYStructGetMember(dataStruct, "includePowerLevel");
	if (includePowerLevel != nullptr) {
		if (includePowerLevel->kind != VALUE_BOOL) {
			YYError("%s :: Data 'includePowerLevel' member should be boolean", __FUNCTION__);
			return;
		}
		publisher.IncludeTransmitPowerLevel(!YYGetBool(includePowerLevel, 0));
	}

	std::map<std::string, GattServiceProviderAdvertisingParameters> params;
	RValue* services = YYStructGetMember(dataStruct, "services");
	if (services != nullptr) {

		publisher.Advertisement().ServiceUuids().Clear();

		if (services->kind != VALUE_ARRAY) {
			YYError("%s :: Data 'services' member should be an array", __FUNCTION__);
			return;
		}

		RValue service;
		for (int i = 0; GET_RValue(&service, services, NULL, i); ++i) {

			// Check for a valid entry (must be struct)
			if (service.kind != VALUE_OBJECT) {
				YYError("%s :: Services array entries should be of type struct", __FUNCTION__);
				return;
			}

			RValue* uuidString = YYStructGetMember(&service, "uuid");
			RValue* data = YYStructGetMember(&service, "data");

			// Member 'uuid' must exist
			if (uuidString == NULL) {
				YYError("%s :: Services array entries must contain 'uuid' member", __FUNCTION__);
				return;
			}

			if (data == NULL) continue;

			// The 'uuid' should be a valid guid
			winrt::guid uuid;
			if (!tryGetUuidFromRValue(uuidString, uuid, __FUNCTION__)) {
				return;
			}

			// The 'data' must be a valid base64 string
			Storage::Streams::IBuffer buffer;
			if (!tryGetIBuffer(YYGetString(data, 0), buffer, __FUNCTION__)) {
				return;
			}

			// Create params and store in temporary map
			GattServiceProviderAdvertisingParameters param;
			param.IsConnectable(isConnectable);
			param.IsDiscoverable(isDiscoverable);
			param.ServiceData(buffer);

			params.insert({ GuidToString(uuid), param });
			publisher.Advertisement().ServiceUuids().Append(uuid);
		}

		publisher.IncludeTransmitPowerLevel(!YYGetBool(includePowerLevel, 0));
	}

	bool startPublisher = false;

	RValue* manufacturer = YYStructGetMember(dataStruct, "manufacturer");
	if (manufacturer != NULL) {

		DebugConsoleOutput("HERE20\n");

		if (manufacturer->kind != VALUE_OBJECT) {
			YYError("%s :: Data 'manufacturer' member should be a struct", __FUNCTION__);
			return;
		}

		RValue* id = YYStructGetMember(manufacturer, "id");
		// The 'id' must be a numeric value
		if (id == NULL || ((id->kind != VALUE_REAL) && (id->kind != VALUE_INT32) && (id->kind != VALUE_INT64))) {
			YYError("%s :: Manufacturer struct should contain a numeric 'id' member", __FUNCTION__);
			return;
		}

		RValue* data = YYStructGetMember(manufacturer, "data");
		if (data == NULL) {
			YYError("%s :: Manufacturer struct should contain a base64 string 'data' member", __FUNCTION__);
			return;
		}

		// The 'data' must be a valid base64 string
		Storage::Streams::IBuffer buffer;
		if (!tryGetIBuffer(YYGetString(data, 0), buffer, __FUNCTION__)) {
			return;
		}

		BluetoothLEManufacturerData manufacturerData;
		manufacturerData.CompanyId(YYGetInt32(id, 0));
		manufacturerData.Data(buffer);

		publisher.Advertisement().ManufacturerData().Clear();
		publisher.Advertisement().ManufacturerData().Append(manufacturerData);

		// The publisher should only start if there is manufacturer data (else there is a crash)
		startPublisher = true;
	}

	// Start advertising
	try {
		// The publisher should only start if there is manufacturer data (else there is a crash)
		if (startPublisher) {
			publisher.Start();
		}
	}
	catch (const winrt::hresult_error& e) {
		DebugConsoleOutput("WinRT Error: %s (code: %d)\n", to_string(e.message()).c_str(), e.code().value);
	}
	catch (const std::exception& e) {
		DebugConsoleOutput("Standard Error: %s\n", e.what());
	}
	catch (...) {
		DebugConsoleOutput("Unknown Error: %s\n");
	}


	for (auto& pair : serviceProviders) {
		GattServiceProvider serviceProvider = pair.second;
	
		// Check if there are previously cached params
		if (params.count(pair.first) != 0) {
			serviceProvider.StartAdvertising(params[pair.first]);
		}
		else {
			// Create a new param and set it to default values
			GattServiceProviderAdvertisingParameters param;
			param.IsConnectable(isConnectable);
			param.IsDiscoverable(isDiscoverable);
			serviceProvider.StartAdvertising(param);
		}
	}

	int64_t asyncId = getNextAsyncId();

	notifyAsyncOperationSuccess(__FUNCTION__, asyncId);
	Result.v64 = asyncId;
}

YYEXPORT void bt_le_advertise_stop(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	if (publisher != nullptr && publisher.Status() == BluetoothLEAdvertisementPublisherStatus::Started) {
		publisher.Stop();
	}

	for (auto& pair : serviceProviders) {
		GattServiceProvider& serviceProvider = pair.second;
		serviceProvider.StopAdvertising();
	}

	int64_t asyncId = getNextAsyncId();

	notifyAsyncOperationSuccess(__FUNCTION__, asyncId);
	Result.v64 = asyncId;
}

YYEXPORT void bt_le_advertise_is_active(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_BOOL;
	Result.val = (publisher != nullptr && publisher.Status() == BluetoothLEAdvertisementPublisherStatus::Started);
}

// SERVER

class PendingRequestResponse {
protected:  // changed from private to protected
	Deferral deferral;
	long long timestamp;

public:
	PendingRequestResponse(const Deferral _deferral) : deferral(_deferral), timestamp(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count()) {
	}
};

class PendingWriteRequestResponse : public PendingRequestResponse {
	const GattWriteRequest request;
public:
	PendingWriteRequestResponse(const Deferral _deferral, const GattWriteRequest _request)
		: PendingRequestResponse(_deferral), request(_request) {
	}

	void respond(int8_t status) {
		if (status == 0)
			request.Respond();
		else request.RespondWithProtocolError(status);
		deferral.Complete();
	}
};

class PendingReadRequestResponse : public PendingRequestResponse {
	const GattReadRequest request;
public:
	PendingReadRequestResponse(const Deferral _deferral, const GattReadRequest _request)
		: PendingRequestResponse(_deferral), request(_request) {
	}

	void respond(int8_t status, const char *value) {
		if (status == 0) {
			Storage::Streams::IBuffer dataBuffer;
			if (tryGetIBuffer(value, dataBuffer, "bt_le_server_respond_read")) {
				request.RespondWithValue(dataBuffer);
			}
		}
		else request.RespondWithProtocolError(status);
		
		deferral.Complete();
	}
};

std::atomic_bool isServerOpen = false;
std::mutex pendingWriteRequestResponsesMutex;
std::map<int64_t, PendingWriteRequestResponse> pendingWriteRequestResponses;
std::mutex pendingReadRequestResponsesMutex;
std::map<int64_t, PendingReadRequestResponse> pendingReadRequestResponses;

void characteristicSetRequestEvents(GattLocalCharacteristic characteristic, winrt::guid serviceUuid) {

	// No need to add a ReadRequested listener if that operation is not allowed
	characteristic.ReadRequested([serviceUuid](GattLocalCharacteristic const& characteristic, GattReadRequestedEventArgs args) {
				
		auto deferral = args.GetDeferral();

		// Get the request information.  This requires device access before an app can access the device's request. 
		args.GetRequestAsync()
			.Completed([deferral, serviceUuid, characteristic](IAsyncOperation<GattReadRequest> const& asyncResult, Foundation::AsyncStatus status)
				{
					if (status != Foundation::AsyncStatus::Completed) return false;

					GattReadRequest request = asyncResult.GetResults();

					if (request.Offset() != 0) {
						request.RespondWithProtocolError(GattProtocolError::InvalidOffset());
						deferral.Complete();
						return true;
					}

					if ((characteristic.CharacteristicProperties() & GattCharacteristicProperties::Read) == GattCharacteristicProperties::None) {
						request.RespondWithProtocolError(GattProtocolError::ReadNotPermitted());
						deferral.Complete();
						return true;
					}

					if (request == nullptr)
					{
						// No access allowed to the device.  Application should indicate this to the user.
						deferral.Complete();
						return false;
					}

					auto async_id = getNextAsyncId();
					{
						std::lock_guard<std::mutex> guard(pendingReadRequestResponsesMutex);
						pendingReadRequestResponses.insert({ async_id, PendingReadRequestResponse(deferral, request) });
					}

					int map = CreateDsMap(0, 0);
					DsMapAddString(map, "type", "bt_le_server_characteristic_read_request");
					DsMapAddInt64(map, "request_id", async_id);
					DsMapAddString(map, "service_uuid", GuidToString(serviceUuid).c_str());
					DsMapAddString(map, "characteristic_uuid", GuidToString(characteristic.Uuid()).c_str());
					CreateAsyncEventWithDSMap(map, 70);

					return true;
				});
		});

	characteristic.WriteRequested([serviceUuid](GattLocalCharacteristic const& characteristic, GattWriteRequestedEventArgs args)
		{
			auto deferral = args.GetDeferral();

			args.GetRequestAsync()
				.Completed([deferral, serviceUuid, characteristic](IAsyncOperation<GattWriteRequest> const& asyncResult, Foundation::AsyncStatus status)
					{
						if (status != Foundation::AsyncStatus::Completed) return false;

						GattWriteRequest request = asyncResult.GetResults();

						if (request.Offset() != 0) {
							request.RespondWithProtocolError(GattProtocolError::InvalidOffset());
							deferral.Complete();
							return true;
						}

						if ((characteristic.CharacteristicProperties() & GattCharacteristicProperties::Write) == GattCharacteristicProperties::None) {
							request.RespondWithProtocolError(GattProtocolError::ReadNotPermitted());
							deferral.Complete();
							return true;
						}

						int map = CreateDsMap(0, 0);

						auto async_id = getNextAsyncId();

						if (request.Option() == GattWriteOption::WriteWithResponse) {
							DsMapAddString(map, "type", "bt_le_server_characteristic_write_request");
							DsMapAddInt64(map, "request_id", 0);

							std::lock_guard<std::mutex> guard(pendingWriteRequestResponsesMutex);
							pendingWriteRequestResponses.insert({ async_id, PendingWriteRequestResponse(deferral, request) });
						}
						else {
							DsMapAddString(map, "type", "bt_le_server_characteristic_write_command");
							deferral.Complete();
						}
						DsMapAddString(map, "service_uuid", GuidToString(serviceUuid).c_str());
						DsMapAddString(map, "characteristic_uuid", GuidToString(characteristic.Uuid()).c_str());
						DsMapAddString(map, "value", to_string(CryptographicBuffer::EncodeToBase64String(request.Value())).c_str());
						CreateAsyncEventWithDSMap(map, 70);

						return true;
					});
		});

}

void descriptorSetRequestEvents(GattLocalDescriptor descriptor, winrt::guid serviceUuid, winrt::guid characteristicUuid) {

	descriptor.ReadRequested([serviceUuid, characteristicUuid](GattLocalDescriptor const& descriptor, GattReadRequestedEventArgs args) {

		auto deferral = args.GetDeferral();

		// Get the request information.  This requires device access before an app can access the device's request. 
		args.GetRequestAsync()
			.Completed([deferral, serviceUuid, characteristicUuid, descriptor](IAsyncOperation<GattReadRequest> const& asyncResult, Foundation::AsyncStatus status)
				{
					if (status != Foundation::AsyncStatus::Completed) return false;

					GattReadRequest request = asyncResult.GetResults();

					if (request == nullptr)
					{
						// No access allowed to the device.  Application should indicate this to the user.
						deferral.Complete();
						return false;
					}

					if (request.Offset() != 0) {
						request.RespondWithProtocolError(GattProtocolError::InvalidOffset());
						deferral.Complete();
						return true;
					}

					auto async_id = getNextAsyncId();

					int map = CreateDsMap(0, 0);
					DsMapAddString(map, "type", "bt_le_server_characteristic_read_request");
					DsMapAddInt64(map, "request_id", async_id);
					DsMapAddString(map, "service_uuid", GuidToString(serviceUuid).c_str());
					DsMapAddString(map, "characteristic_uuid", GuidToString(characteristicUuid).c_str());
					DsMapAddString(map, "descriptor_uuid", GuidToString(descriptor.Uuid()).c_str());
					CreateAsyncEventWithDSMap(map, 70);

					std::lock_guard<std::mutex> guard(pendingReadRequestResponsesMutex);
					pendingReadRequestResponses.insert({ async_id, PendingReadRequestResponse(deferral, request) });

					return true;
				});
		});

	descriptor.WriteRequested([serviceUuid, characteristicUuid](GattLocalDescriptor const& descriptor, GattWriteRequestedEventArgs args)
		{
			auto deferral = args.GetDeferral();

			args.GetRequestAsync()
				.Completed([deferral, serviceUuid, characteristicUuid, descriptor](IAsyncOperation<GattWriteRequest> const& asyncResult, Foundation::AsyncStatus status)
					{
						if (status != Foundation::AsyncStatus::Completed) return false;

						GattWriteRequest request = asyncResult.GetResults();

						if (request.Offset() != 0) {
							request.RespondWithProtocolError(GattProtocolError::InvalidOffset());
							deferral.Complete();
							return true;
						}

						int map = CreateDsMap(0, 0);

						auto async_id = getNextAsyncId();

						DsMapAddString(map, "type", "bt_le_server_descriptor_write_request");
						DsMapAddInt64(map, "request_id", 0);
						DsMapAddString(map, "service_uuid", GuidToString(serviceUuid).c_str());
						DsMapAddString(map, "characteristic_uuid", GuidToString(characteristicUuid).c_str());
						DsMapAddString(map, "descriptor_uuid", GuidToString(descriptor.Uuid()).c_str());
						DsMapAddString(map, "value", to_string(CryptographicBuffer::EncodeToBase64String(request.Value())).c_str());
						CreateAsyncEventWithDSMap(map, 70);

						std::lock_guard<std::mutex> guard(pendingWriteRequestResponsesMutex); 
						pendingWriteRequestResponses.insert({ async_id, PendingWriteRequestResponse(deferral, request) });

						return true;
					});
		});

}

GMDescriptor getDescriptorFromRValue(RValue* descriptorData, const char*functionName) {

	GMDescriptor descriptor;

	RValue* uuid = YYStructGetMember(descriptorData, "uuid");
	if (!tryGetUuidFromRValue(uuid, descriptor.guid, functionName)) {
		YYError("%s :: descriptor must contain a 'uuid' member", functionName);
	}

	return descriptor;
}

GMCharacteristic getCharacteristicFromRValue(RValue* characteristicData, const char* functionName) {

	GMCharacteristic characteristic;

	RValue* uuid = YYStructGetMember(characteristicData, "uuid");
	if (!tryGetUuidFromRValue(uuid, characteristic.guid, functionName)) {
		YYError("%s :: characteristic must contain a 'uuid' member", functionName);
	}

	RValue* description = YYStructGetMember(characteristicData, "description");
	if (description != nullptr) {
		characteristic.description = to_hstring(YYGetString(description, 0));
	}

	RValue* properties = YYStructGetMember(characteristicData, "properties");
	if (properties != nullptr) {
		characteristic.properties = (GattCharacteristicProperties)REAL_RValue(properties);
	}

	RValue* permissions = YYStructGetMember(characteristicData, "permissions");
	if (permissions != nullptr) {
		characteristic.permissions = static_cast<uint32_t>(REAL_RValue(permissions));
	}

	RValue* descriptors = YYStructGetMember(characteristicData, "descriptors");
	if (descriptors != nullptr) {
		if (descriptors->kind != VALUE_ARRAY) {
			YYError("%s :: Invalid characteristic structure (characteristic 'descriptors' member must be an array)", functionName);
		}
		RValue descriptor;
		for (int i = 0; GET_RValue(&descriptor, descriptors, NULL, i); ++i) {
			characteristic.descriptors.push_back(getDescriptorFromRValue(&descriptor, functionName));
		}
	}

	return characteristic;
}

GMService getServiceFromRValue(RValue* serviceData, const char* functionName) {

	GMService service;

	RValue* uuid = YYStructGetMember(serviceData, "uuid");
	if (!tryGetUuidFromRValue(uuid, service.guid, functionName)) {
		YYError("%s :: service must contain a 'uuid' member", functionName);
	}

	RValue* characteristics = YYStructGetMember(serviceData, "characteristics");
	if (characteristics != nullptr) {
		if (characteristics->kind != VALUE_ARRAY) {
			YYError("%s :: Invalid service structure (service 'characteristics' member must be an array)", functionName);
		}
		RValue characteristic;
		for (int i = 0; GET_RValue(&characteristic, characteristics, NULL, i); ++i) {
			service.characteristics.push_back(getCharacteristicFromRValue(&characteristic, functionName));
		}
	}

	return service;
}

YYEXPORT void bt_le_server_open(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_INT64;
	Result.v64 = -1;
	
	if (isServerOpen) return;
	isServerOpen = true;

	int64_t asyncId = getNextAsyncId();

	notifyAsyncOperationSuccess(__FUNCTION__, asyncId);

	Result.v64 = asyncId;
}

YYEXPORT void bt_le_server_close(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_INT64;
	Result.v64 = -1;

	if (!isServerOpen) return;
	isServerOpen = false;

	{
		std::lock_guard<std::mutex> guard(subscriptionsMutex);
		tokens.clear();
	}
	{
		std::lock_guard<std::mutex> guard(serviceProvidersMutex);
		serviceProviders.clear();
	}

	int64_t asyncId = getNextAsyncId();

	notifyAsyncOperationSuccess(__FUNCTION__, asyncId);

	Result.v64 = asyncId;
}

IAsyncOperation<bool> AddDescriptorAsync(const char* functionName, int64_t asyncId, GattLocalService service, GattLocalCharacteristic characteristic, const GMDescriptor &descriptorData) {

	try
	{
		GattLocalDescriptorParameters descriptorParameters;
		if (descriptorData.value != nullptr)
			descriptorParameters.StaticValue(descriptorData.value);

		auto descriptorResult = co_await characteristic.CreateDescriptorAsync(descriptorData.guid, descriptorParameters);

		if (descriptorResult.Error() != BluetoothError::Success)
		{
			notifyAsyncOperationError(functionName, asyncId, (int32_t)descriptorResult.Error(), {
							{ "error_message", "Bluetooth error creating a descriptor with guid: '" + GuidToString(descriptorData.guid) + "'" }
				});
			co_return false;
		}

		auto descriptor = descriptorResult.Descriptor();
		descriptorSetRequestEvents(descriptor, service.Uuid(), characteristic.Uuid());

		co_return true;
	}
	catch (const winrt::hresult_error& e)
	{
		notifyAsyncOperationError(functionName, asyncId, (int32_t)e.code(), {
							{ "error_message", to_string(e.message()) }
			});
		co_return false;
	}
}

IAsyncOperation<bool> AddCharacteristicAsync(const char* functionName, int64_t asyncId, GattLocalService service, const GMCharacteristic &characteristicData) {
	
	try
	{
		GattLocalCharacteristicParameters characteristicParameters;

		if (characteristicData.value != nullptr)
			characteristicParameters.StaticValue(characteristicData.value);

		if (!characteristicData.description.empty())
			characteristicParameters.UserDescription(characteristicData.description);

		if (characteristicData.properties != GattCharacteristicProperties::None)
			characteristicParameters.CharacteristicProperties(characteristicData.properties);

		uint32_t permissions = characteristicData.permissions;
		if (permissions & 1) characteristicParameters.ReadProtectionLevel(GattProtectionLevel::Plain);
		else if (permissions & 2) characteristicParameters.ReadProtectionLevel(GattProtectionLevel::EncryptionRequired);
		else if (permissions & 4) characteristicParameters.ReadProtectionLevel(GattProtectionLevel::EncryptionAndAuthenticationRequired);

		if (permissions & 16) characteristicParameters.WriteProtectionLevel(GattProtectionLevel::Plain);
		else if (permissions & 32) characteristicParameters.WriteProtectionLevel(GattProtectionLevel::EncryptionRequired);
		else if (permissions & (64 | 128 | 256)) characteristicParameters.WriteProtectionLevel(GattProtectionLevel::EncryptionAndAuthenticationRequired);

		auto characteristicResult = co_await service.CreateCharacteristicAsync(characteristicData.guid, characteristicParameters);
		if (characteristicResult.Error() != BluetoothError::Success)
		{
			notifyAsyncOperationError(functionName, asyncId, (int32_t)characteristicResult.Error(), {
							{ "error_message", "Bluetooth error creating a characteristic with guid: '" + GuidToString(characteristicData.guid) + "'" }
				});
			co_return false;
		}

		auto characteristic = characteristicResult.Characteristic();
		for (const auto& descriptorData : characteristicData.descriptors)
		{
			auto result = co_await AddDescriptorAsync(functionName, asyncId, service, characteristic, descriptorData);
			if (!result) co_return false;
		}

		characteristicSetRequestEvents(characteristic, service.Uuid());

		co_return true;
	}
	catch (const winrt::hresult_error& e)
	{
		notifyAsyncOperationError(functionName, asyncId, (int32_t)e.code(), {
							{ "error_message", to_string(e.message()) }
			});
		co_return false;
	}
}

IAsyncOperation<bool> AddServiceAsync(const char* functionName, int64_t asyncId, const GMService serviceData)
{
	try
	{
		auto serviceProviderResult = co_await GattServiceProvider::CreateAsync(winrt::guid(serviceData.guid));
		if (serviceProviderResult.Error() != BluetoothError::Success)
		{
			notifyAsyncOperationError(functionName, asyncId, (int32_t)serviceProviderResult.Error(), {
					{ "error_message", "Framework error creating a service with guid: '" + GuidToString(serviceData.guid) + "'" }
				});
			co_return false;
		}

		auto serviceProvider = serviceProviderResult.ServiceProvider();
		auto service = serviceProvider.Service();

		for (const auto& characteristicData : serviceData.characteristics)
		{
			auto result = co_await AddCharacteristicAsync(functionName, asyncId, service, characteristicData);
			if (!result) co_return false;
		}

		notifyAsyncOperationSuccess(functionName, asyncId, { { "service_uuid", GuidToString(service.Uuid())} });
		{
			std::lock_guard<std::mutex> guard(serviceProvidersMutex);
			serviceProviders.insert_or_assign(GuidToString(service.Uuid()), serviceProvider);
		}

		co_return true;
	}
	catch (const winrt::hresult_error& e)
	{
		notifyAsyncOperationError(functionName, asyncId, (int32_t)e.code(), {
							{ "error_message", to_string(e.message()) }
			});
		co_return false;
	}
}

YYEXPORT void bt_le_server_add_service(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_INT64;
	Result.v64 = -1;

	if (!isServerOpen) return;

	auto functionName = __FUNCTION__;

	GMService serviceData = getServiceFromRValue(YYGetStruct(arg, 0), functionName);

	auto asyncId = getNextAsyncId();

	AddServiceAsync(functionName, asyncId, serviceData);

	Result.v64 = asyncId;
}

YYEXPORT void bt_le_server_clear_services(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	if (!isServerOpen) return;

	{
		std::lock_guard<std::mutex> guard(subscriptionsMutex);
		tokens.clear();
	}
	{
		std::lock_guard<std::mutex> guard(serviceProvidersMutex);
		for (auto& pair : serviceProviders) {
			GattServiceProvider& serviceProvider = pair.second;
			if (serviceProvider.AdvertisementStatus() != GattServiceProviderAdvertisementStatus::Stopped) {
				serviceProvider.StopAdvertising();
			}
		}
		serviceProviders.clear();
	}

}

YYEXPORT void bt_le_server_respond_read(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_BOOL;
	Result.val = false;

	if (!isServerOpen) return;

	int64_t requestId = YYGetInt64(arg, 0);
	int8_t status = (int8_t)YYGetReal(arg, 1);
	const char* value = YYGetString(arg, 2);

	std::lock_guard<std::mutex> guard(pendingReadRequestResponsesMutex);

	auto it = pendingReadRequestResponses.find(requestId);
	if (it == pendingReadRequestResponses.end()) return;
	
	PendingReadRequestResponse pendingReadRequestResponse = it->second;
	pendingReadRequestResponse.respond(status, value);

	pendingReadRequestResponses.erase(it);

	Result.val = true;
}

YYEXPORT void bt_le_server_respond_write(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_BOOL;
	Result.val = false;

	if (!isServerOpen) return;

	int64_t requestId = YYGetInt64(arg, 0);
	int8_t status = (int8_t)YYGetReal(arg, 1);

	std::lock_guard<std::mutex> guard(pendingWriteRequestResponsesMutex);

	auto it = pendingWriteRequestResponses.find(requestId);
	if (it == pendingWriteRequestResponses.end()) return;

	PendingWriteRequestResponse pendingWriteRequestResponse = it->second;
	pendingWriteRequestResponse.respond(status);

	pendingWriteRequestResponses.erase(it);

	Result.val = true;
}

YYEXPORT void bt_le_server_notify_value(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_INT64;
	Result.v64 = -1;

	if (!isServerOpen) return;

	std::string functionName = __FUNCTION__;

	auto serviceUUID = YYGetString(arg, 0);
	auto characteristicUUID = YYGetString(arg, 1);
	auto data = YYGetString(arg, 2);

	Storage::Streams::IBuffer dataBuffer;
	if (!tryGetIBuffer(data, dataBuffer, __FUNCTION__)) return;

	std::lock_guard<std::mutex> guard(serviceProvidersMutex);
	auto it = serviceProviders.find(serviceUUID);
	if (it == serviceProviders.end()) return;

	auto service = it->second.Service();

	winrt::guid characteristicGuid(characteristicUUID);

	auto asyncId = getNextAsyncId();

	// Traverse all characteristics
	for (auto characteristic : service.Characteristics()) {
		// Check if the UUIDs match
		if (characteristic.Uuid() == characteristicGuid) {

			characteristic.NotifyValueAsync(dataBuffer).Completed([asyncId, functionName](IAsyncOperation<IVectorView<GattClientNotificationResult>> asyncResults, Foundation::AsyncStatus status) {

				auto results = asyncResults.GetResults();

				RValue notifiedDevices = { 0 };
				YYCreateArray(&notifiedDevices);

				for (int i = results.Size() - 1; i >= 0; --i)
				{
					RValue notifiedDevice = { 0 };
					YYStructCreate(&notifiedDevice);

					auto result = results.GetAt(i);

					// Use session deviceId to extract connection address
					BluetoothDeviceId deviceId = result.SubscribedClient().Session().DeviceId();
					std::string identifier = to_string(deviceId.Id());
					std::string connectionAddress = identifier.substr(identifier.find("-") + 1);

					YYStructAddString(&notifiedDevice, "address", connectionAddress.c_str());
					YYStructAddInt32(&notifiedDevice, "status", (int32_t)result.Status());

					SET_RValue(&notifiedDevices, &notifiedDevice, NULL, i);
				}

				notifyAsyncOperationSuccess(functionName, asyncId, {
					{ "devices", &notifiedDevices }
					});

				return true;
				});
		}
	}

	Result.v64 = asyncId;
}

// CLIENT

bool tryGetPeripheral(const std::string& peripheralAddress, BluetoothLEDevice& device, const std::string& functionName)
{
	std::lock_guard<std::mutex> lock(devicesMutex);

	auto it = devices.find(peripheralAddress);
	if (it == devices.end()) {
		DebugConsoleOutput("%s :: Peripheral not found (you should aquire it first).\n", functionName.c_str());
		return false;
	}

	device = it->second;
	return true;
}

YYEXPORT void bt_le_peripheral_open(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_INT64;
	Result.v64 = -1;

	std::string functionName = __FUNCTION__;

	auto peripheralAddress = YYGetString(arg, 0);

	BluetoothLEDevice device(nullptr);
	if (tryGetPeripheral(peripheralAddress, device, functionName)) return;

	auto asyncId = getNextAsyncId();

	BluetoothLEDevice::FromBluetoothAddressAsync(StringToAddress(peripheralAddress))
		.Completed([functionName, asyncId](IAsyncOperation<BluetoothLEDevice> asyncResult, Foundation::AsyncStatus status)
			{
				BluetoothLEDevice device = asyncResult.GetResults();
				{
					std::lock_guard<std::mutex> lock(devicesMutex);
					devices.insert({ AddressToString(device.BluetoothAddress()), device });
				}

				device.ConnectionStatusChanged([](BluetoothLEDevice sender, auto)
					{
						int map = CreateDsMap(0, 0);
						DsMapAddString(map, "type", "bt_le_peripheral_connection_status_changed");
						DsMapAddString(map, "name", to_string(sender.DeviceInformation().Name()).c_str());
						DsMapAddBool(map, "is_paired", sender.DeviceInformation().Pairing().IsPaired());
						DsMapAddString(map, "address", AddressToString(sender.BluetoothAddress()).c_str());
						DsMapAddBool(map, "is_connected", sender.ConnectionStatus() == BluetoothConnectionStatus::Connected);
						CreateAsyncEventWithDSMap(map, 70);
					});

				//device.ConnectionPhyChanged([](BluetoothLEDevice sender, auto)
				//	{
				//		int map = CreateDsMap(0, 0);
				//		DsMapAddString(map, "type", "bt_le_peripheral_phy_changed");
				//		DsMapAddString(map, "name", to_string(sender.DeviceInformation().Name()).c_str());
				//		DsMapAddString(map, "address", AddressToString(sender.BluetoothAddress()).c_str());
				//		
				//		auto connectionPhy = sender.GetConnectionPhy();
				//		auto receiceInfo = connectionPhy.ReceiveInfo();
				//		uint8_t rxPhy = receiceInfo.IsCodedPhy() ? 3 : (receiceInfo.IsUncoded2MPhy() ? 2 : (receiceInfo.IsUncoded1MPhy() ? 1 : 0));
				//
				//		auto transmitInfo = connectionPhy.TransmitInfo();
				//		uint8_t txPhy = transmitInfo.IsCodedPhy() ? 3 : (transmitInfo.IsUncoded2MPhy() ? 2 : (transmitInfo.IsUncoded1MPhy() ? 1 : 0));
				//		
				//		DsMapAddDouble(map, "rx_phy", rxPhy);
				//		DsMapAddDouble(map, "tx_phy", txPhy);
				//
				//		CreateAsyncEventWithDSMap(map, 70);
				//	});

				device.GattServicesChanged([](BluetoothLEDevice sender, auto)
					{
						int map = CreateDsMap(0, 0);
						DsMapAddString(map, "type", "bt_le_peripheral_services_changed");
						DsMapAddString(map, "name", to_string(sender.DeviceInformation().Name()).c_str());
						DsMapAddString(map, "address", AddressToString(sender.BluetoothAddress()).c_str());
						CreateAsyncEventWithDSMap(map, 70);
					});

				notifyAsyncOperationSuccess(functionName, asyncId, {
						{"address", AddressToString(device.BluetoothAddress())},
						{"name", to_string(device.DeviceInformation().Name())},
						{"is_paired", device.DeviceInformation().Pairing().IsPaired()},
					});
				return true;
			});

	Result.v64 = asyncId;
}

YYEXPORT void bt_le_peripheral_is_open(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_BOOL;
	Result.val = false;

	std::string functionName = __FUNCTION__;

	auto peripheralAddress = YYGetString(arg, 0);

	BluetoothLEDevice device(nullptr);
	if (!tryGetPeripheral(peripheralAddress, device, functionName)) return;

	Result.val = true;
}

YYEXPORT void bt_le_peripheral_close_all(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	{
		std::lock_guard<std::mutex> lock(devicesMutex);
		for (auto& pair : devices) {
			BluetoothLEDevice& device = pair.second;
			device.Close();
		}
	}
	{
		std::lock_guard<std::mutex> lock(subscriptionsMutex);
		subscriptions.clear();
	}

}

YYEXPORT void bt_le_peripheral_close(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_BOOL;
	Result.val = false;

	auto peripheralAddress = YYGetString(arg, 0);

	BluetoothLEDevice device(nullptr);
	if (!tryGetPeripheral(peripheralAddress, device, __FUNCTION__)) return;

	// CLose the device
	device.Close();

	// Loop through the event tokens map
	{
		std::lock_guard<std::mutex> lock(subscriptionsMutex);
		for (auto it = subscriptions.begin(); it != subscriptions.end(); )
		{
			// If the key starts with the peripheral address, this token is associated with the disconnected device
			if (it->first.rfind(peripheralAddress, 0) == 0) {
				// Remove this entry from the event tokens map
				it = subscriptions.erase(it);
			}
			else {
				++it;
			}
		}
	}

	// Remove this entry from the devices map
	std::lock_guard<std::mutex> lock(devicesMutex);
	devices.erase(peripheralAddress);

	Result.val = true;
}

YYEXPORT void bt_le_peripheral_is_connected(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_BOOL;
	Result.val = false;

	auto peripheralAddress = YYGetString(arg, 0);

	BluetoothLEDevice device(nullptr);
	if (!tryGetPeripheral(peripheralAddress, device, __FUNCTION__)) return;

	Result.val = device.ConnectionStatus() == BluetoothConnectionStatus::Connected;
}

YYEXPORT void bt_le_peripheral_is_paired(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_BOOL;
	Result.val = false;

	auto peripheralAddress = YYGetString(arg, 0);
	
	BluetoothLEDevice device(nullptr);
	if (!tryGetPeripheral(peripheralAddress, device, __FUNCTION__)) return;

	Result.val = device.DeviceInformation().Pairing().IsPaired();
}

YYEXPORT void bt_le_peripheral_get_services(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_INT64;
	Result.v64 = -1;

	std::string functionName = __FUNCTION__;

	std::string peripheralAddress = YYGetString(arg, 0);

	BluetoothLEDevice device(nullptr);
	if (!tryGetPeripheral(peripheralAddress, device, functionName)) return;

	auto asyncId = getNextAsyncId();

	device.GetGattServicesAsync(BluetoothCacheMode::Uncached).Completed([functionName, asyncId](IAsyncOperation<GattDeviceServicesResult> asyncInfo, auto&& asyncStatus) {

		auto servicesResult = asyncInfo.GetResults();
		if (servicesResult.Status() != GattCommunicationStatus::Success) {
			DebugConsoleOutput("%s :: Failed with error code: %d\n", functionName.c_str(), servicesResult.Status());
			notifyAsyncOperationError(functionName, asyncId, (uint32_t)servicesResult.Status());
			return false;
		}

		auto services = servicesResult.Services();

		RValue servicesArray = { 0 };
		YYCreateArray(&servicesArray);

		for (int32_t i = services.Size() - 1; i >= 0; --i) {
			auto service = services.GetAt(i);

			RValue serviceData = { 0 };
			YYStructCreate(&serviceData);
			YYStructAddString(&serviceData, "uuid", GuidToString(service.Uuid()).c_str());

			SET_RValue(&servicesArray, &serviceData, NULL, i);
		}

		int map = CreateDsMap(0, 0);
		DsMapAddString(map, "type", functionName.c_str());
		DsMapAddBool(map, "success", true);
		DsMapAddInt64(map, "async_id", asyncId);
		DsMapAddRValue(map, "services", &servicesArray);
		CreateAsyncEventWithDSMap(map, 70);
		
		return true;
		});

	Result.v64 = asyncId;
}

auto constexpr ERROR_BLE_SERVICE_NOT_FOUND = -1;

bool tryGetServiceAsync(const std::string& peripheralAddress, const std::string& serviceUUID, std::function<void(GattDeviceService)> callback, const std::string& functionName, int64_t asyncId)
{
	BluetoothLEDevice device(nullptr);
	if (!tryGetPeripheral(peripheralAddress, device, functionName)) {
		return false;
	}

	winrt::guid serviceUuid(serviceUUID);
	device.GetGattServicesForUuidAsync(serviceUuid).Completed([callback, functionName, asyncId](IAsyncOperation<GattDeviceServicesResult> asyncInfo, auto&& asyncStatus)
		{
			auto result = asyncInfo.GetResults();
			if (result.Status() != GattCommunicationStatus::Success) {
				DebugConsoleOutput("%s :: Failed with error code: %d\n", functionName.c_str(), result.Status());
				notifyAsyncOperationError(functionName, asyncId, (uint32_t)result.Status());
				return false;
			}

			auto services = result.Services();
			if (services.Size() == 0) {
				DebugConsoleOutput("%s :: Service not found.\n", functionName.c_str());
				notifyAsyncOperationError(functionName, asyncId, ERROR_BLE_SERVICE_NOT_FOUND);
				return false;
			}

			auto service = services.GetAt(0);
			callback(service);

			return true;
		});

	return true;
}

YYEXPORT void bt_le_service_get_characteristics(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_INT64;
	Result.v64 = -1;

	std::string functionName = __FUNCTION__;

	auto peripheralAddress = YYGetString(arg, 0);
	auto serviceUUID = YYGetString(arg, 1);

	BluetoothLEDevice device(nullptr);
	if (!tryGetPeripheral(peripheralAddress, device, functionName)) return;

	auto asyncId = getNextAsyncId();

	if (!tryGetServiceAsync(peripheralAddress, serviceUUID, [functionName, asyncId](GattDeviceService service)
		{
			service.GetCharacteristicsAsync(BluetoothCacheMode::Uncached).Completed([functionName, asyncId](IAsyncOperation<GattCharacteristicsResult> asyncInfo, auto&& asyncStatus)
				{
					auto characteristicsResult = asyncInfo.GetResults();
					if (characteristicsResult.Status() != GattCommunicationStatus::Success) {
						DebugConsoleOutput("%s :: Failed with error code: %d\n", functionName.c_str(), characteristicsResult.Status());
						notifyAsyncOperationError(functionName, asyncId, (uint32_t)characteristicsResult.Status());
						return false;
					}

					auto characteristics = characteristicsResult.Characteristics();

					RValue characteristicsArray = { 0 };
					YYCreateArray(&characteristicsArray);

					for (int32_t i = characteristics.Size() - 1; i >= 0; --i) {
						auto characteristic = characteristics.GetAt(i);

						RValue characteristicsStruct = { 0 };
						YYStructCreate(&characteristicsStruct);
						YYStructAddString(&characteristicsStruct, "uuid", GuidToString(characteristic.Uuid()).c_str());
						YYStructAddString(&characteristicsStruct, "description", to_string(characteristic.UserDescription()).c_str());
						YYStructAddInt64(&characteristicsStruct, "properties", (int64_t)characteristic.CharacteristicProperties());

						SET_RValue(&characteristicsArray, &characteristicsStruct, NULL, i);
					}

					int map = CreateDsMap(0, 0);
					DsMapAddString(map, "type", functionName.c_str());
					DsMapAddBool(map, "success", true);
					DsMapAddInt64(map, "async_id", asyncId);
					DsMapAddRValue(map, "characteristics", &characteristicsArray);
					CreateAsyncEventWithDSMap(map, 70);

					return true;
				});
		}, functionName, asyncId)) {
		return;
	}

	Result.v64 = asyncId;
}

auto constexpr ERROR_BLE_CHARACTERISTIC_NOT_FOUND = -2;

bool tryGetCharacteristicAsync(const std::string& peripheralAddress, const std::string& serviceUUID, const std::string& characteristicUUID, std::function<void(GattCharacteristic)> callback, const std::string& functionName, int64_t asyncId)
{
	return tryGetServiceAsync(peripheralAddress, serviceUUID, [characteristicUUID, callback, functionName, asyncId](GattDeviceService service)
		{
			winrt::guid characteristicUuid(characteristicUUID);
			service.GetCharacteristicsForUuidAsync(characteristicUuid).Completed([callback, functionName, asyncId](IAsyncOperation<GattCharacteristicsResult> asyncInfo, auto&& asyncStatus)
				{
					auto characteristicsResult = asyncInfo.GetResults();
					if (characteristicsResult.Status() != GattCommunicationStatus::Success) {
						DebugConsoleOutput("%s :: Failed with error code: %d\n", functionName.c_str(), characteristicsResult.Status());
						notifyAsyncOperationError(functionName, asyncId, (uint32_t)characteristicsResult.Status());
						return false;
					}

					auto characteristics = characteristicsResult.Characteristics();
					if (characteristics.Size() == 0) {
						DebugConsoleOutput("%s :: Characteristic not found.\n", functionName.c_str());
						notifyAsyncOperationError(functionName, asyncId, ERROR_BLE_CHARACTERISTIC_NOT_FOUND);
						return false;
					}

					auto characteristic = characteristics.GetAt(0);
					callback(characteristic);
					return true;
				});
		}, functionName, asyncId);
}

YYEXPORT void bt_le_characteristic_get_descriptors(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_INT64;
	Result.v64 = -1;

	std::string functionName = __FUNCTION__;

	auto peripheralAddress = YYGetString(arg, 0);
	auto serviceUUID = YYGetString(arg, 1);
	auto characteristicUUID = YYGetString(arg, 2);

	BluetoothLEDevice device(nullptr);
	if (!tryGetPeripheral(peripheralAddress, device, functionName)) return;

	auto asyncId = getNextAsyncId();

	if (!tryGetCharacteristicAsync(peripheralAddress, serviceUUID, characteristicUUID, [functionName, asyncId](GattCharacteristic characteristic) {

		characteristic.GetDescriptorsAsync(BluetoothCacheMode::Uncached).Completed([functionName, asyncId](IAsyncOperation<GattDescriptorsResult> asyncInfo, auto&& asyncStatus)
			{
				auto descriptorsResult = asyncInfo.GetResults();
				if (descriptorsResult.Status() != GattCommunicationStatus::Success) {
					DebugConsoleOutput("%s :: Failed with error code: %d\n", functionName.c_str(), descriptorsResult.Status());
					notifyAsyncOperationError(functionName, asyncId, (uint32_t)descriptorsResult.Status());
					return false;
				}

				auto descriptors = descriptorsResult.Descriptors();

				RValue descriptorsArray = { 0 };
				YYCreateArray(&descriptorsArray);

				for (int32_t i = descriptors.Size() - 1; i >= 0; --i) {
					auto descriptor = descriptors.GetAt(i);

					RValue descriptorData = { 0 };
					YYStructCreate(&descriptorData);
					YYStructAddString(&descriptorData, "uuid", GuidToString(descriptor.Uuid()).c_str());

					SET_RValue(&descriptorsArray, &descriptorData, NULL, i);
				}

				int map = CreateDsMap(0, 0);
				DsMapAddString(map, "type", functionName.c_str());
				DsMapAddBool(map, "success", true);
				DsMapAddInt64(map, "async_id", asyncId);
				DsMapAddRValue(map, "descriptors", &descriptorsArray);
				CreateAsyncEventWithDSMap(map, 70);

				return true;
			});

		}, functionName, asyncId)) {
		return;
	}

	Result.v64 = asyncId;
}

YYEXPORT void bt_le_characteristic_read(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_INT64;
	Result.v64 = -1;

	std::string functionName = __FUNCTION__;

	auto peripheralAddress = YYGetString(arg, 0);
	auto serviceUUID = YYGetString(arg, 1);
	auto characteristicUUID = YYGetString(arg, 2);

	auto asyncId = getNextAsyncId();

	if (!tryGetCharacteristicAsync(peripheralAddress, serviceUUID, characteristicUUID, [functionName, asyncId](GattCharacteristic characteristic) {

		characteristic.ReadValueAsync().Completed([functionName, asyncId](IAsyncOperation<GattReadResult> asyncInfo, auto&& asyncStatus)
			{
				auto readResult = asyncInfo.GetResults();
				if (readResult.Status() != GattCommunicationStatus::Success) {
					DebugConsoleOutput("%s :: Failed with error code: %d\n", functionName.c_str(), readResult.Status());
					notifyAsyncOperationError(functionName, asyncId, (uint32_t)readResult.Status());
					return;
				}

				auto characteristicValue = readResult.Value();
				auto base64String = CryptographicBuffer::EncodeToBase64String(characteristicValue);

				notifyAsyncOperationSuccess(functionName, asyncId, {
					{ "value", to_string(base64String) }
					});

				// Use the dataReader to read the data
				DebugConsoleOutput("%s :: Succeeded!\n", functionName.c_str());
			});

		}, functionName, asyncId)) {
		return;
	}

	Result.v64 = asyncId;
}

YYEXPORT void bt_le_characteristic_write_request(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_INT64;
	Result.v64 = -1;

	std::string functionName = __FUNCTION__;

	auto peripheralAddress = YYGetString(arg, 0);
	auto serviceUUID = YYGetString(arg, 1);
	auto characteristicUUID = YYGetString(arg, 2);
	auto data = YYGetString(arg, 3);

	Storage::Streams::IBuffer dataBuffer;
	if (!tryGetIBuffer(data, dataBuffer, functionName)) return;

	auto asyncId = getNextAsyncId();

	if (!tryGetCharacteristicAsync(peripheralAddress, serviceUUID, characteristicUUID, [dataBuffer, functionName, asyncId](GattCharacteristic characteristic) {

		characteristic.WriteValueWithResultAsync(dataBuffer).Completed([functionName, asyncId](IAsyncOperation<GattWriteResult> asyncInfo, auto&& asyncStatus)
			{
				auto writeResult = asyncInfo.GetResults();
				if (writeResult.Status() != GattCommunicationStatus::Success) {
					DebugConsoleOutput("%s :: Failed with error code: %d\n", functionName.c_str(), writeResult.Status());
					notifyAsyncOperationError(functionName, asyncId, (uint32_t)writeResult.Status());
					return false;
				}
				DebugConsoleOutput("%s :: Succeeded!\n", functionName.c_str());
				notifyAsyncOperationSuccess(functionName, asyncId);
				return true;
			});

		}, functionName, asyncId)) {
		return;
	}

	Result.v64 = asyncId;
}

YYEXPORT void bt_le_characteristic_write_command(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_INT64;
	Result.v64 = -1;

	std::string functionName = __FUNCTION__;

	auto peripheralAddress = YYGetString(arg, 0);
	auto serviceUUID = YYGetString(arg, 1);
	auto characteristicUUID = YYGetString(arg, 2);
	auto data = YYGetString(arg, 3);

	Storage::Streams::IBuffer dataBuffer;
	if (!tryGetIBuffer(data, dataBuffer, functionName)) return;

	auto asyncId = getNextAsyncId();

	if (!tryGetCharacteristicAsync(peripheralAddress, serviceUUID, characteristicUUID, [dataBuffer, functionName, asyncId](GattCharacteristic characteristic) {

		characteristic.WriteValueAsync(dataBuffer).Completed([functionName, asyncId](IAsyncOperation<GattCommunicationStatus> asyncInfo, auto&& asyncStatus)
			{
				auto communicationStatus = asyncInfo.GetResults();
				if (communicationStatus != GattCommunicationStatus::Success) {
					DebugConsoleOutput("%s :: Failed with error code: %d\n", functionName.c_str(), communicationStatus);
					notifyAsyncOperationError(functionName, asyncId, (uint32_t)communicationStatus);
					return false;
				}
				DebugConsoleOutput("%s :: Succeeded!\n", functionName.c_str());
				notifyAsyncOperationSuccess(functionName, asyncId);
				return true;
			});

		}, functionName, asyncId)) {
		return;
	}

	Result.v64 = asyncId;
}

std::string buildTokenKey(GattCharacteristic characteristic) {
	auto service = characteristic.Service();
	auto device = service.Device();

	auto characteristicUuid = GuidToString(characteristic.Uuid());
	auto serviceUuid = GuidToString(service.Uuid());
	auto peripheralAddress = AddressToString(device.BluetoothAddress());
	return peripheralAddress + "|" + serviceUuid + "|" + characteristicUuid;
}

void characteristicSubscribe(GattCharacteristic characteristic, GattCharacteristicProperties propertyCheck, GattClientCharacteristicConfigurationDescriptorValue descriptorValue, const std::string &functionName, int64_t asyncId) {

	uint32_t characteristicProp = (uint32_t)characteristic.CharacteristicProperties();
	if ((characteristicProp & (uint32)propertyCheck) == 0) {
		DebugConsoleOutput("%s :: Cannot subscribe to characteristic.\n", functionName.c_str());
		return;
	}

	auto subscriptionKey = buildTokenKey(characteristic);
	{
		std::lock_guard<std::mutex> lock(subscriptionsMutex);

		auto eventIt = subscriptions.find(subscriptionKey);
		if (eventIt != subscriptions.end()) {
			subscriptions.erase(subscriptionKey);
		}

		characteristic.ValueChanged([](GattCharacteristic sender, GattValueChangedEventArgs eventArgs)
			{
				auto characteristicValue = eventArgs.CharacteristicValue();
				auto base64String = CryptographicBuffer::EncodeToBase64String(characteristicValue);

				int map = CreateDsMap(0, 0);
				DsMapAddString(map, "type", "bt_le_characteristic_value_changed");
				DsMapAddString(map, "characteristic_uuid", GuidToString(sender.Uuid()).c_str());
				DsMapAddString(map, "service_uuid", GuidToString(sender.Service().Uuid()).c_str());
				DsMapAddString(map, "peripheral_address", AddressToString(sender.Service().Device().BluetoothAddress()).c_str());
				DsMapAddString(map, "value", to_string(base64String).c_str());
				CreateAsyncEventWithDSMap(map, 70);
			});

		subscriptions.insert_or_assign(subscriptionKey, characteristic);
	}

	characteristic.WriteClientCharacteristicConfigurationDescriptorWithResultAsync(descriptorValue)
		.Completed([functionName, asyncId](IAsyncOperation<GattWriteResult> asyncInfo, auto&& asyncStatus) {


		auto writeResult = asyncInfo.GetResults();
		if (writeResult.Status() != GattCommunicationStatus::Success) {
			DebugConsoleOutput("%s :: Failed with error code: %d\n", functionName.c_str(), writeResult.Status());
			notifyAsyncOperationError(functionName, asyncId, (uint32_t)writeResult.Status());
			return false;
		}
		DebugConsoleOutput("%s :: Succeeded\n", functionName.c_str());
		notifyAsyncOperationSuccess(functionName, asyncId);
		return true;
			});

}

YYEXPORT void bt_le_characteristic_notify(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_INT64;
	Result.v64 = -1;

	std::string functionName = __FUNCTION__;

	auto peripheralAddress = YYGetString(arg, 0);
	auto serviceUUID = YYGetString(arg, 1);
	auto characteristicUUID = YYGetString(arg, 2);

	auto asyncId = getNextAsyncId();

	if (!tryGetCharacteristicAsync(peripheralAddress, serviceUUID, characteristicUUID, [functionName, asyncId](GattCharacteristic characteristic) {

		characteristicSubscribe(characteristic, GattCharacteristicProperties::Notify, GattClientCharacteristicConfigurationDescriptorValue::Notify, functionName, asyncId);

		}, functionName, asyncId)) {
		return;
	}

	Result.v64 = asyncId;
}

YYEXPORT void bt_le_characteristic_indicate(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_INT64;
	Result.v64 = -1;

	std::string functionName = __FUNCTION__;

	auto peripheralAddress = YYGetString(arg, 0);
	auto serviceUUID = YYGetString(arg, 1);
	auto characteristicUUID = YYGetString(arg, 2);

	auto asyncId = getNextAsyncId();

	if (!tryGetCharacteristicAsync(peripheralAddress, serviceUUID, characteristicUUID, [functionName, asyncId](GattCharacteristic characteristic) {

		characteristicSubscribe(characteristic, GattCharacteristicProperties::Indicate, GattClientCharacteristicConfigurationDescriptorValue::Indicate, functionName, asyncId);

		}, functionName, asyncId)) {
		return;
	}

	Result.v64 = asyncId;
}

YYEXPORT void bt_le_characteristic_unsubscribe(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_INT64;
	Result.v64 = -1;

	std::string functionName = __FUNCTION__;

	auto peripheralAddress = YYGetString(arg, 0);
	auto serviceUUID = YYGetString(arg, 1);
	auto characteristicUUID = YYGetString(arg, 2);

	auto asyncId = getNextAsyncId();

	if (!tryGetCharacteristicAsync(peripheralAddress, serviceUUID, characteristicUUID, [functionName, asyncId](GattCharacteristic characteristic) {

		auto subscriptionKey = buildTokenKey(characteristic);
		{
			std::lock_guard<std::mutex> lock(subscriptionsMutex);

			auto eventIt = subscriptions.find(subscriptionKey);
			if (eventIt != subscriptions.end()) {
				subscriptions.erase(subscriptionKey);
			}
		}

		characteristic.WriteClientCharacteristicConfigurationDescriptorWithResultAsync(GattClientCharacteristicConfigurationDescriptorValue::None)
			.Completed([functionName, asyncId](IAsyncOperation<GattWriteResult> asyncInfo, auto&& asyncStatus) {

			auto writeResult = asyncInfo.GetResults();
			if (writeResult.Status() != GattCommunicationStatus::Success) {
				notifyAsyncOperationError(functionName, asyncId, (uint32_t)writeResult.Status());
				DebugConsoleOutput("%s :: Failed with error code: %d\n", functionName.c_str(), writeResult.Status());
				return false;
			}
			DebugConsoleOutput("%s :: Succeeded\n", functionName.c_str());
			notifyAsyncOperationSuccess(functionName, asyncId);

			return true;
				});

		}, functionName, asyncId)) {
		return;
	}

	Result.v64 = asyncId;
}

auto constexpr ERROR_BLE_DESCRIPTOR_NOT_FOUND = -3;

bool tryGetDescriptorAsync(const std::string& peripheralAddress, const std::string& serviceUUID, const std::string& characteristicUUID, const std::string& descriptorUUID, std::function<void(GattDescriptor)> callback, const std::string& functionName, int64_t asyncId)
{
	return tryGetCharacteristicAsync(peripheralAddress, serviceUUID, characteristicUUID,
		[descriptorUUID, callback, functionName, asyncId](GattCharacteristic characteristic)
		{
			winrt::guid descriptorUuid(descriptorUUID);
			characteristic.GetDescriptorsForUuidAsync(descriptorUuid).Completed([callback, functionName, asyncId](IAsyncOperation<GattDescriptorsResult> asyncInfo, auto&& asyncStatus)
				{
					auto descriptorsResult = asyncInfo.GetResults();
					if (descriptorsResult.Status() != GattCommunicationStatus::Success) {
						DebugConsoleOutput("%s :: Failed with error code: %d\n", functionName.c_str(), descriptorsResult.Status());
						notifyAsyncOperationError(functionName, asyncId, (uint32_t)descriptorsResult.Status());
						return false;
					}

					auto descriptors = descriptorsResult.Descriptors();
					if (descriptors.Size() == 0) {
						DebugConsoleOutput("%s :: Descriptor not found.\n", functionName.c_str());
						notifyAsyncOperationError(functionName, asyncId, ERROR_BLE_DESCRIPTOR_NOT_FOUND);
						return false;
					}

					auto descriptor = descriptors.GetAt(0);

					callback(descriptor);
					return true;
				});
		}, functionName, asyncId);
}

YYEXPORT void bt_le_descriptor_read(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_INT64;
	Result.v64 = -1;

	std::string functionName = __FUNCTION__;

	auto peripheralAddress = YYGetString(arg, 0);
	auto serviceUUID = YYGetString(arg, 1);
	auto characteristicUUID = YYGetString(arg, 2);
	auto descriptorUUID = YYGetString(arg, 3);

	auto asyncId = getNextAsyncId();

	if (!tryGetDescriptorAsync(peripheralAddress, serviceUUID, characteristicUUID, descriptorUUID, [asyncId, functionName](GattDescriptor descriptor) {

		descriptor.ReadValueAsync().Completed([asyncId, functionName](IAsyncOperation<GattReadResult> asyncInfo, auto&& asyncStatus)
			{
				auto readResult = asyncInfo.GetResults();
				if (readResult.Status() != GattCommunicationStatus::Success) {
					DebugConsoleOutput("%s :: Failed with error code: %d\n", functionName.c_str(), readResult.Status());
					notifyAsyncOperationError(functionName, asyncId, (uint32_t)readResult.Status());
					return false;
				}

				auto descriptorValue = readResult.Value();
				auto base64String = CryptographicBuffer::EncodeToBase64String(descriptorValue);

				notifyAsyncOperationSuccess(functionName, asyncId, {
					{ "value", to_string(base64String) }
					});

				DebugConsoleOutput("%s :: Succeeded!\n", functionName.c_str());

				return true;
			});

		}, functionName, asyncId)) {
		return;
	}

	Result.v64 = asyncId;
}

YYEXPORT void bt_le_descriptor_write(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
	Result.kind = VALUE_INT64;
	Result.v64 = -1;

	std::string functionName = __FUNCTION__;

	auto peripheralAddress = YYGetString(arg, 0);
	auto serviceUUID = YYGetString(arg, 1);
	auto characteristicUUID = YYGetString(arg, 2);
	auto descriptorUUID = YYGetString(arg, 3);
	auto data = YYGetString(arg, 4);

	Storage::Streams::IBuffer dataBuffer;
	if (!tryGetIBuffer(data, dataBuffer, functionName)) return;

	if (dataBuffer == nullptr) return;

	auto asyncId = getNextAsyncId();

	if (!tryGetDescriptorAsync(peripheralAddress, serviceUUID, characteristicUUID, descriptorUUID, [dataBuffer, asyncId, functionName](GattDescriptor descriptor) {

		descriptor.WriteValueWithResultAsync(nullptr).Completed([asyncId, functionName](IAsyncOperation<GattWriteResult> asyncInfo, auto&& asyncStatus)
			{
				auto writeResult = asyncInfo.GetResults();
				if (writeResult.Status() != GattCommunicationStatus::Success) {
					DebugConsoleOutput("%s :: Failed with error code: %d\n", functionName.c_str(), writeResult.Status());
					notifyAsyncOperationError(functionName, asyncId, (uint32_t)writeResult.Status());
					return false;
				}
				notifyAsyncOperationSuccess(functionName, asyncId);
				DebugConsoleOutput("%s :: Succeeded!\n", functionName.c_str());
				return true;
			});

		}, functionName, asyncId)) {
		return;
	}

	Result.v64 = asyncId;
}

