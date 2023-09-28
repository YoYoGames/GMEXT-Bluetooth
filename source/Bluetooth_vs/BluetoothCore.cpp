
#include "BluetoothCore.h"
#include "BluetoothTools.h"

YYRunnerInterface gs_runnerInterface;
YYRunnerInterface* g_pYYRunnerInterface;

#ifdef OS_Windows
extern "C" __declspec(dllexport) void PreGraphicsInitialisation(char* arg1) {}
#endif
#if defined(OS_Linux) || defined(OS_MacOs)
extern "C" void PreGraphicsInitialisation(char* arg1) {}
#endif

HANDLE m_radioHandle = nullptr;

YYEXPORT void YYExtensionInitialise(const struct YYRunnerInterface* _pFunctions, size_t _functions_size)
{
    //copy out all the functions 
    memcpy(&gs_runnerInterface, _pFunctions, sizeof(YYRunnerInterface));
    g_pYYRunnerInterface = &gs_runnerInterface;

    if (_functions_size < sizeof(YYRunnerInterface)) {
        DebugConsoleOutput("ERROR : runner interface mismatch in extension DLL\n ");
    } // end if

    DebugConsoleOutput("YYExtensionInitialise CONFIGURED \n ");
}

YYEXPORT void bt_init(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    static bool initialized = false;

    if (initialized) return;

    if (bluetoothClassicInit()) {
        m_radioHandle = createRadioHandle(__FUNCTION__);
    }

    bluetoothLeInit();
}

YYEXPORT void bt_end(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    HANDLE radio = getRadioHandle(__FUNCTION__);
    if (radio != nullptr) {
        CloseHandle(radio);
    }
    WSACleanup();
}

YYEXPORT void bt_get_address(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    YYCreateString(&Result, "");

    HANDLE hRadio = getRadioHandle(__FUNCTION__);

    if (hRadio == nullptr) return;

    BLUETOOTH_RADIO_INFO radioInfo = { sizeof(BLUETOOTH_RADIO_INFO), 0, };
    DWORD result = BluetoothGetRadioInfo(hRadio, &radioInfo);

    if (result == ERROR_SUCCESS) {
        YYCreateString(&Result, AddressToString(radioInfo.address.rgBytes).c_str());
    }
}

YYEXPORT void bt_get_name(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    HANDLE hRadio = getRadioHandle(__FUNCTION__);

    if (hRadio == nullptr) return;

    BLUETOOTH_RADIO_INFO radioInfo = { sizeof(BLUETOOTH_RADIO_INFO), 0, };
    DWORD result = BluetoothGetRadioInfo(hRadio, &radioInfo);

    if (result == ERROR_SUCCESS) {
        char name[256];
        sprintf(name, "%ls", radioInfo.szName);
        YYCreateString(&Result, name);

        printf("Bluetooth Adapter Name: %s\n", name);
    }
}

YYEXPORT void bt_get_paired_devices(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    YYCreateArray(&Result);

    HANDLE hRadio = getRadioHandle(__FUNCTION__);
    if (hRadio == nullptr) return;

    BLUETOOTH_DEVICE_SEARCH_PARAMS deviceSearchParams = { 0 };
    deviceSearchParams.dwSize = sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS);
    deviceSearchParams.fReturnAuthenticated = TRUE;
    deviceSearchParams.fReturnRemembered = FALSE;
    deviceSearchParams.fReturnUnknown = FALSE;
    deviceSearchParams.fReturnConnected = FALSE;
    deviceSearchParams.fIssueInquiry = FALSE;
    deviceSearchParams.cTimeoutMultiplier = 0;

    BLUETOOTH_DEVICE_INFO deviceInfo = { 0 };
    deviceInfo.dwSize = sizeof(BLUETOOTH_DEVICE_INFO);

    deviceSearchParams.hRadio = hRadio;

    HBLUETOOTH_DEVICE_FIND hFind;
    hFind = BluetoothFindFirstDevice(&deviceSearchParams, &deviceInfo);

    int count = 0;
    if (hFind != NULL) {
        do {
            RValue deviceStruct = { 0 };
            YYStructCreate(&deviceStruct);

            // Build the data struct and add it to result array
            createBluetoothDeviceStruct(deviceStruct, deviceInfo);
            SET_RValue(&Result, &deviceStruct, NULL, count++);

        } while (BluetoothFindNextDevice(hFind, &deviceInfo));

        BluetoothFindDeviceClose(hFind);
    }
}

YYEXPORT void bt_is_enabled(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    Result.kind = VALUE_BOOL;
    Result.val = true;
}

YYEXPORT void bt_request_enable(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    Result.kind = VALUE_BOOL;
    Result.val = false;
}

// PRIVATE

void winrtInit() {
    static bool initialized = false;

    if (initialized) return;
    initialized = true;

    // Initialize winrt
    winrt::init_apartment(apartment_type::single_threaded);
}

HANDLE createRadioHandle(const char* methodName)
{
    if (m_radioHandle == NULL)
    {
        BLUETOOTH_FIND_RADIO_PARAMS btFindParams = { sizeof(BLUETOOTH_FIND_RADIO_PARAMS) };
        HBLUETOOTH_RADIO_FIND btFind = BluetoothFindFirstRadio(&btFindParams, &m_radioHandle);

        if (btFind == nullptr)
        {
            int error_id = GetLastError();
            if (error_id == ERROR_NO_MORE_ITEMS) {
                DebugConsoleOutput("%s :: Bluetooth is disabled or not supported by this device.\n", methodName);
            }
        }
        else BluetoothFindRadioClose(btFind); // Close the find handle.
    }
    return m_radioHandle;
}

HANDLE getRadioHandle(const char* methodName)
{
    if (m_radioHandle == nullptr)
    {
        DebugConsoleOutput("%s :: Bluetooth is disabled or not supported by this device.\n", methodName);
    }
    return m_radioHandle;
}

void createBluetoothDeviceStruct(RValue& Result, const BLUETOOTH_DEVICE_INFO& deviceInfo) {

    // Add name
    char name[256];
    sprintf(name, "%ls", deviceInfo.szName);
    YYStructAddString(&Result, "name", name);

    // Add addressed
    char address[18];
    sprintf(address, "%02X:%02X:%02X:%02X:%02X:%02X",
        deviceInfo.Address.rgBytes[5],
        deviceInfo.Address.rgBytes[4],
        deviceInfo.Address.rgBytes[3],
        deviceInfo.Address.rgBytes[2],
        deviceInfo.Address.rgBytes[1],
        deviceInfo.Address.rgBytes[0]);
    YYStructAddString(&Result, "address", address);

    // Add device class
    YYStructAddInt64(&Result, "class", deviceInfo.ulClassofDevice);

    // Is the device paired (bonded) ?
    YYStructAddBool(&Result, "paired", deviceInfo.fAuthenticated);

    // Is the device connected right now ?
    YYStructAddBool(&Result, "connected", deviceInfo.fConnected);

    // Is the device remembered ?
    YYStructAddBool(&Result, "remembered", deviceInfo.fRemembered);

}

int64_t getNextAsyncId() {
    static int64_t asyncId = 0;
    return asyncId++;
}

void notifyOperation(const std::string& functionName, const std::map<std::string, std::variant<std::string, int, int64_t, bool, double, RValue*>> extraParams) {
    int map = CreateDsMap(0, 0);

    DsMapAddString(map, "type", functionName.c_str());

    for (auto& [key, value] : extraParams) {
        std::visit([&map, &key](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::string>)
                DsMapAddString(map, key.c_str(), arg.c_str());
            else if constexpr (std::is_same_v<T, int>)
                DsMapAddDouble(map, key.c_str(), static_cast<double>(arg));
            else if constexpr (std::is_same_v<T, int64_t>)
                DsMapAddInt64(map, key.c_str(), arg);
            else if constexpr (std::is_same_v<T, bool>)
                DsMapAddBool(map, key.c_str(), arg);
            else if constexpr (std::is_same_v<T, double>)
                DsMapAddDouble(map, key.c_str(), arg);
            else if constexpr (std::is_same_v<T, RValue*>)
                DsMapAddRValue(map, key.c_str(), arg);
            }, value);
    }

    CreateAsyncEventWithDSMap(map, 70);
}

void notifyAsyncOperation(const std::string& functionName, int64_t asyncId, const std::map<std::string, std::variant<std::string, int, int64_t, bool, double, RValue*>> extraParams) {

    std::map<std::string, std::variant<std::string, int, int64_t, bool, double, RValue*>> params(extraParams);
    params.insert({ "async_id", asyncId });

    notifyOperation(functionName, params);
}

void notifyAsyncOperation(const std::string& functionName, int64_t asyncId) {
    std::map<std::string, std::variant<std::string, int, int64_t, bool, double, RValue*>> params;
    params.insert({ "async_id", asyncId });

    notifyOperation(functionName, params);
}

void notifyAsyncOperationError(const std::string& functionName, int64_t asyncId, uint32_t errorCode, const std::map<std::string, std::variant<std::string, int, int64_t, bool, double, RValue*>> extraParams) {

    std::map<std::string, std::variant<std::string, int, int64_t, bool, double, RValue*>> params(extraParams);
    params.insert({ "success", false });
    params.insert({ "error_code", errorCode });

    notifyAsyncOperation(functionName, asyncId, params);
}

void notifyAsyncOperationError(const std::string& functionName, int64_t asyncId, uint32_t errorCode) {

    std::map<std::string, std::variant<std::string, int, int64_t, bool, double, RValue*>> params;
    params.insert({ "success", false });
    params.insert({ "error_code", errorCode });

    notifyAsyncOperation(functionName, asyncId, params);
}

void notifyAsyncOperationSuccess(const std::string& functionName, int64_t asyncId, const std::map<std::string, std::variant<std::string, int, int64_t, bool, double, RValue*>> extraParams) {

    std::map<std::string, std::variant<std::string, int, int64_t, bool, double, RValue*>> params(extraParams);
    params.insert({ "success", true });

    notifyAsyncOperation(functionName, asyncId, params);
}

void notifyAsyncOperationSuccess(const std::string& functionName, int64_t asyncId) {

    std::map<std::string, std::variant<std::string, int, int64_t, bool, double, RValue*>> params;
    params.insert({ "success", true });

    notifyAsyncOperation(functionName, asyncId, params);
}
