
#include "BluetoothClassic.h"

std::unique_ptr<BluetoothConnectionManager> connectionManager = std::make_unique<BluetoothConnectionManager>();
std::unique_ptr<BluetoothDiscovery> discovery = nullptr;
std::unique_ptr<BluetoothServer> server = nullptr;

// Bluetooth Server

void BluetoothServer::start(GUID guid, std::string name, std::string description) {
    m_running = true;
    m_serverSocket = setupServer(name, description, guid);
    m_serverThread = std::jthread([this](std::stop_token stopToken) {
        this->run(stopToken);
        });
}

void BluetoothServer::stop() {
    m_running = false;
    m_serverThread.request_stop();
    m_serverThread.join();
}

SOCKET BluetoothServer::setupServer(std::string name, std::string description, GUID mServiceGuid) {
    // Initialize listener socket
    SOCKET listenSocket = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
    if (listenSocket == INVALID_SOCKET) {
        NotificationManager::notifyServerCreateError(GetLastError());
        return INVALID_SOCKET;
    }

    // Socket address information
    SOCKADDR_BTH addr = { 0 };
    addr.addressFamily = AF_BTH;
    addr.btAddr = BTH_ADDR_NULL;
    addr.serviceClassId = GUID_NULL;
    addr.port = BT_PORT_ANY;

    // Bind socket - Necessary before listening
    if (bind(listenSocket, (sockaddr*)&addr, sizeof(SOCKADDR_BTH)) != 0) {
        NotificationManager::notifyServerCreateError(GetLastError());
        return INVALID_SOCKET;
    }

    // Set socket to listen mode
    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        NotificationManager::notifyServerCreateError(GetLastError());
        return SOCKET_ERROR;
    }

    int size = sizeof(SOCKADDR_BTH);
    if (getsockname(listenSocket, (sockaddr*)&addr, &size) != 0) {
        NotificationManager::notifyServerCreateError(GetLastError());
        return INVALID_SOCKET;
    }

    // Convert to widechar strings
    std::wstring wName = std::wstring(name.begin(), name.end());
    std::wstring wDescription = std::wstring(description.begin(), description.end());

    // Set WSA service
    WSAQUERYSET wsaQuery = { 0 };
    wsaQuery.dwSize = sizeof(WSAQUERYSET);
    wsaQuery.lpszServiceInstanceName = const_cast<LPWSTR>(wName.c_str());
    wsaQuery.lpszComment = const_cast<LPWSTR>(wDescription.c_str());
    wsaQuery.lpServiceClassId = &mServiceGuid;
    wsaQuery.dwNameSpace = NS_BTH;
    wsaQuery.dwNumberOfCsAddrs = 1;

    // Contains information about the bluetooth broadcast service
    CSADDR_INFO addrInfo = { { (sockaddr*)&addr, size }, { nullptr, 0 }, SOCK_STREAM, BTHPROTO_RFCOMM };

    // Initialize bluetooth service
    wsaQuery.lpcsaBuffer = &addrInfo;
    if (WSASetService(&wsaQuery, RNRSERVICE_REGISTER, 0) != 0) {
        NotificationManager::notifyServerCreateError(GetLastError());
        return INVALID_SOCKET;
    }

    NotificationManager::notifyServerCreateSuccess(name.c_str(), description.c_str());

    u_long mode = 0;  // 0 to enable blocking socket
    ioctlsocket(listenSocket, FIONBIO, &mode);

    return listenSocket;
}

void BluetoothServer::run(std::stop_token stopToken) {
    // Prepare the fd_set structure for select
    fd_set fds = {0};
    FD_ZERO(&fds);
    FD_SET(m_serverSocket, &fds);

    // Timeout for select
    timeval timeout = { 0 };
    timeout.tv_sec = 1;  // 1 second
    timeout.tv_usec = 0; // 0 microseconds

    // Loop to accept connections until stop signal is received
    while (!stopToken.stop_requested()) {
        // Use select to wait for a connection
        fd_set read_fds = fds;
        int ret = select((int)m_serverSocket + 1, &read_fds, NULL, NULL, &timeout);

        if (ret > 0 && FD_ISSET(m_serverSocket, &read_fds)) {
            // A connection is pending
            SOCKADDR_BTH client = { 0 };
            int clientSize = sizeof(client);
            SOCKET clientSocket = accept(m_serverSocket, (sockaddr*)&client, &clientSize);
            if (clientSocket != INVALID_SOCKET) {

                u_long mode = 1;  // 1 to enable non-blocking socket
                ioctlsocket(clientSocket, FIONBIO, &mode);

                // Create a new BluetoothConnection with the client socket
                std::shared_ptr<BluetoothConnection> connection = std::make_shared<BluetoothConnection>(clientSocket);

                // Add the new connection to the connections map
                int64_t socketId = connectionManager->addConnection(connection);

                // Report success
                NotificationManager::notifyServerAcceptedConnection(client, socketId);
            }
        }
    }

    // Close the server socket when we're done accepting connections
    if (closesocket(m_serverSocket) == SOCKET_ERROR) {
        int errorCode = WSAGetLastError();
        DebugConsoleOutput("BluetoothServer.run :: Failed to close the socket. Error code: %d\n", errorCode);
        NotificationManager::notifyServerCloseError(errorCode);
    }
    else NotificationManager::notifyServerCloseSuccess();
}
// Bluetooth Connection

int BluetoothConnection::disconnect() {

    std::lock_guard<std::mutex> guard(m_socketMutex);

    int errorCode = 0;

    // Shut down the socket for both sending and receiving
    if (shutdown(m_socket, SD_BOTH) == SOCKET_ERROR) {
        errorCode = WSAGetLastError();
        DebugConsoleOutput("BluetoothConnection.disconnect :: Failed to shutdown socket data transfer. Error code: %d\n", errorCode);
    }

    // Close the socket
    if (closesocket(m_socket) == SOCKET_ERROR) {
        errorCode = WSAGetLastError();
        DebugConsoleOutput("BluetoothConnection.disconnect :: Failed to close the socket. Error code: %d\n", errorCode); 
    }

    return errorCode;
}

int BluetoothConnection::send(void* buff, int length) {
    std::lock_guard<std::mutex> guard(m_socketMutex);
    return ::send(m_socket, (const char*)buff, (int)length, 0 /*flags*/);
}

int BluetoothConnection::receive(void* buff, int length, int& errorCode) {
    std::lock_guard<std::mutex> guard(m_socketMutex);
    int result = recv(m_socket, (char*)buff, length, 0 /*flags*/);

    if (result <= 0) {
        errorCode = WSAGetLastError();
    }
    return result;
}

// Bluetooth Connection Manager

void BluetoothConnectionManager::createConnection(BTH_ADDR address, GUID guid, int64_t asyncId) {
    std::thread([=]() {
        // Connect to server and get a socket
        SOCKET socket = connectToServer(address, guid);

        // If the socket is not valid, return null
        if (socket == INVALID_SOCKET) {
            // Here you could add error reporting
            return;
        }

        // Create a new BluetoothConnection with the connected socket
        std::shared_ptr<BluetoothConnection> connection = std::make_shared<BluetoothConnection>(socket);

        // Add the new connection to the connections map
        int64_t socketId = addConnection(connection);

        // Report success
        notifyAsyncOperationSuccess("bt_classic_socket_open", asyncId, {
            { "address", AddressToString(address).c_str() },
            { "guid", GUIDtoString(guid) },
            { "socket_id", socketId }
            });

        }).detach(); // Detach the thread to let it run independently
}

SOCKET BluetoothConnectionManager::connectToServer(BTH_ADDR address, GUID guid) {
    // This method is similar to your previous connectToServer
    // Initialize variables
    SOCKET clientSocket = INVALID_SOCKET;

    // Set up local endpoint
    SOCKADDR_BTH localEndpoint = {};
    localEndpoint.port = BT_PORT_ANY;
    localEndpoint.addressFamily = AF_BTH;
    localEndpoint.btAddr = BTH_ADDR_NULL;
    localEndpoint.serviceClassId = StringToGUID((char*)"00000000-0000-0000-0000-000000000000");

    // Set up remote endpoint
    SOCKADDR_BTH remoteEndpoint = {};
    remoteEndpoint.port = BT_PORT_ANY;
    remoteEndpoint.addressFamily = AF_BTH;
    remoteEndpoint.btAddr = address;
    remoteEndpoint.serviceClassId = guid;

    // Create a socket
    clientSocket = ::socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
    if (clientSocket == INVALID_SOCKET) {
        NotificationManager::notifyClientJoinError(address, guid, WSAGetLastError());
        return INVALID_SOCKET;
    }

    // Bind the socket
    if (::bind(clientSocket, (SOCKADDR*)&localEndpoint, sizeof(localEndpoint)) == SOCKET_ERROR) {
        NotificationManager::notifyClientJoinError(address, guid, WSAGetLastError());
        closesocket(clientSocket);  // Close the socket before returning
        return INVALID_SOCKET;
    }

    // Connect the socket
    if (::connect(clientSocket, (SOCKADDR*)&remoteEndpoint, sizeof(remoteEndpoint)) == INVALID_SOCKET) {
        NotificationManager::notifyClientJoinError(address, guid, WSAGetLastError());
        closesocket(clientSocket);  // Close the socket before returning
        return INVALID_SOCKET;
    }

    // Set non-blocking mode
    u_long mode = 1;
    ioctlsocket(clientSocket, FIONBIO, &mode);

    return clientSocket;
}

int64_t BluetoothConnectionManager::addConnection(std::shared_ptr<BluetoothConnection> connection) {

    // Add the new connection to the connections map
    int64_t socketId = nextSocketId();
    std::lock_guard<std::mutex> guard(connectionsMutex);
    connections.insert({ socketId, connection });

    return socketId;
}

void BluetoothConnectionManager::closeConnection(int64_t socketId) {
    std::lock_guard<std::mutex> guard(connectionsMutex);

    // Find the connection in the map
    auto it = connections.find(socketId);

    // If the connection is found, disconnect it and remove it from the map
    if (it != connections.end()) {
        auto result = it->second->disconnect();
        connections.erase(it);
    }
}

void BluetoothConnectionManager::closeAllConnections() {
    // Create a vector of all keys (socket IDs)
    std::vector<int64_t> socketIds;
    {
        std::lock_guard<std::mutex> guard(connectionsMutex);
        for (auto& [socketId, connection] : connections) {
            socketIds.push_back(socketId);
        }
    }

    // Loop over all socket IDs to disconnect each connection
    for (auto socketId : socketIds) {
        closeConnection(socketId);
    }
}

int64_t BluetoothConnectionManager::nextSocketId() {
    std::lock_guard<std::mutex> guard(m_socketIdMutex);
    return m_socketId++;
}

std::shared_ptr<BluetoothConnection> BluetoothConnectionManager::getConnection(int64_t socketId, const char* methodName) {
    std::lock_guard<std::mutex> guard(connectionsMutex);
    auto it = connections.find(socketId);
    if (it != connections.end()) {
        return it->second;
    }
    else {
        DebugConsoleOutput("%s :: Socket index doesn't exist.\n", methodName);
        return nullptr;
    }
}

// Bluetooth Discovery

void BluetoothDiscovery::startDiscovery() {
    m_discovering = true;
    m_discoveryThread = std::jthread([this](std::stop_token stopToken) {
        this->discoverDevices(stopToken);
        });
}

void BluetoothDiscovery::cancelDiscovery() {
    m_discoveryThread.request_stop();
    m_discovering = false;
}

bool BluetoothDiscovery::isDiscovering() const {
    return m_discovering;
}

void BluetoothDiscovery::discoverDevices(std::stop_token stopToken) {

    NotificationManager::notifyDiscoveryStarted();

    BLUETOOTH_DEVICE_SEARCH_PARAMS searchParams = { sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS), 1, 0, 1, 1, 1, 15, NULL };
    searchParams.hRadio = m_radio;

    BLUETOOTH_DEVICE_INFO deviceInfo = { sizeof(BLUETOOTH_DEVICE_INFO), 0, };

    HBLUETOOTH_DEVICE_FIND btDev = BluetoothFindFirstDevice(&searchParams, &deviceInfo);
    if (btDev == NULL) {
        
        DebugConsoleOutput("BluetoothDiscovery.discoverDevices :: Bluetooth device find failed with code: %d\n", GetLastError());
        return;
    }

    do {
        if (stopToken.stop_requested()) {
            break;
        }
        NotificationManager::notifyReceiverFound(deviceInfo);
    } while (BluetoothFindNextDevice(btDev, &deviceInfo));

    BluetoothFindDeviceClose(btDev);

    NotificationManager::notifyDiscoveryFinished();
}

// Notification Manager

void NotificationManager::notifyServerCreateError(int errorCode) {
    int map = CreateDsMap(0, 0);
    DsMapAddString(map, "type", "bt_classic_server_create");
    DsMapAddBool(map, "success", false);
    DsMapAddDouble(map, "error_code", errorCode);
    CreateAsyncEventWithDSMap(map, 70);
}

void NotificationManager::notifyServerCreateSuccess(const char* name, const char* description) {
    int map = CreateDsMap(0, 0);
    DsMapAddString(map, "type", "bt_classic_server_create");
    DsMapAddBool(map, "success", true);
    DsMapAddString(map, "name", name);
    DsMapAddString(map, "description", description);
    CreateAsyncEventWithDSMap(map, 70);
}

void NotificationManager::notifyServerAcceptedConnection(SOCKADDR_BTH client, int64_t socketId) {

    // Record the successful server creation and the client address
    int map = CreateDsMap(0, 0);
    DsMapAddString(map, "type", "bt_classic_server_accept");
    DsMapAddString(map, "address", AddressToString(client.btAddr).c_str());
    DsMapAddInt64(map, "socket_id", socketId);
    DsMapAddBool(map, "success", true);
    CreateAsyncEventWithDSMap(map, 70);
}

void NotificationManager::notifyServerCloseError(int errorCode) {
    int map = CreateDsMap(0, 0);
    DsMapAddString(map, "type", "bt_classic_server_close");
    DsMapAddBool(map, "success", false);
    DsMapAddDouble(map, "error_code", errorCode);
    CreateAsyncEventWithDSMap(map, 70);
}

void NotificationManager::notifyServerCloseSuccess() {
    int map = CreateDsMap(0, 0);
    DsMapAddString(map, "type", "bt_classic_server_close");
    DsMapAddBool(map, "success", true);
    CreateAsyncEventWithDSMap(map, 70);
}

void NotificationManager::notifyClientJoinError(BTH_ADDR address, GUID guid, int errorCode) {
    int map = CreateDsMap(0, 0);
    DsMapAddString(map, "type", "bt_classic_socket_open");
    DsMapAddBool(map, "success", false);

    DsMapAddString(map, "address", AddressToString(address).c_str());
    DsMapAddString(map, "uuid", GUIDtoString(guid));
    DsMapAddDouble(map, "error", (double)errorCode);

    CreateAsyncEventWithDSMap(map, 70);
}

void NotificationManager::notifyClientJoinSuccess(BTH_ADDR address, GUID guid, int64_t socketId) {
    int map = CreateDsMap(0, 0);
    DsMapAddString(map, "type", "bt_classic_socket_open");
    DsMapAddBool(map, "success", true);

    DsMapAddString(map, "address", AddressToString(address).c_str());
    DsMapAddString(map, "uuid", GUIDtoString(guid));
    DsMapAddInt64(map, "socket_id", socketId);

    CreateAsyncEventWithDSMap(map, 70);
}

void NotificationManager::notifyReceiverFound(const BLUETOOTH_DEVICE_INFO& deviceInfo) {
    int map = CreateDsMap(0, 0);
    DsMapAddString(map, "type", "bt_classic_scan_result");

    char name[256];
    sprintf(name, "%ls", deviceInfo.szName);
    DsMapAddString(map, "name", name);
    DsMapAddString(map, "address", AddressToString(deviceInfo.Address.rgBytes).c_str());
    DsMapAddInt64(map, "class", deviceInfo.ulClassofDevice);
    DsMapAddBool(map, "connected", deviceInfo.fConnected);
    DsMapAddBool(map, "authenticated", deviceInfo.fAuthenticated);
    DsMapAddBool(map, "remembered", deviceInfo.fRemembered);
    DsMapAddDouble(map, "success", 1.0);
    CreateAsyncEventWithDSMap(map, 70);
}

void NotificationManager::notifyDiscoveryStarted() {
    int map = CreateDsMap(0, 0);
    DsMapAddString(map, "type", "bt_classic_scan_started");
    CreateAsyncEventWithDSMap(map, 70);
}

void NotificationManager::notifyDiscoveryFinished() {
    int map = CreateDsMap(0, 0);
    DsMapAddString(map, "type", "bt_classic_scan_finished");
    CreateAsyncEventWithDSMap(map, 70);
}

void NotificationManager::notifySocketRemotelyClosed(int64_t socketId) {
    int map = CreateDsMap(0, 0);
    DsMapAddString(map, "type", "bt_classic_socket_remotely_closed");
    DsMapAddBool(map, "socket_id", socketId);
    CreateAsyncEventWithDSMap(map, 70);
}

void NotificationManager::notifySocketError(int64_t socketId, int errorCode) {
    int map = CreateDsMap(0, 0);
    DsMapAddString(map, "type", "bt_classic_socket_error");
    DsMapAddBool(map, "success", false);
    DsMapAddBool(map, "socket_id", socketId);
    DsMapAddDouble(map, "error_code", errorCode);
    CreateAsyncEventWithDSMap(map, 70);
}

// PUBLIC

YYEXPORT void bt_classic_info(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    BLUETOOTH_FIND_RADIO_PARAMS m_bt_find_radio = { sizeof(BLUETOOTH_FIND_RADIO_PARAMS) };
    BLUETOOTH_RADIO_INFO m_bt_info = { sizeof(BLUETOOTH_RADIO_INFO),0, };

    HANDLE hRadio = getRadioHandle(__FUNCTION__);
    if (hRadio == nullptr) return;

    DWORD mbtinfo_ret = BluetoothGetRadioInfo(hRadio, &m_bt_info);

    YYStructCreate(&Result);

    if (mbtinfo_ret == ERROR_SUCCESS)
    {
        createBluetoothRadioStruct(Result, m_bt_info);
    }
    else DebugConsoleOutput("bt_classic_info :: failed with error code %d\n", mbtinfo_ret);
}

YYEXPORT void bt_classic_is_supported(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg) {

    bool supported = false;

    run_sync(BluetoothAdapter::GetDefaultAsync(), [&supported](IAsyncOperation<BluetoothAdapter> asyncInfo) {

        BluetoothAdapter adapter = asyncInfo.GetResults();
        if (adapter == nullptr) {
            supported = false;
        }
        else supported = adapter.IsClassicSupported();
        return true;
        });

    Result.kind = VALUE_BOOL;
    Result.val = supported;
}

YYEXPORT void bt_classic_scan_start(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    Result.kind = VALUE_INT64;
    Result.v64 = -1;

    if (discovery == nullptr) {
        HANDLE hRadio = getRadioHandle(__FUNCTION__);
        if (hRadio == NULL) return;

        discovery = std::make_unique<BluetoothDiscovery>(hRadio);
    }

    if (discovery->isDiscovering()) {
        DebugConsoleOutput("%s :: Scan is already running.\n", __FUNCTION__);
        return;
    }

    discovery->startDiscovery();

    int64_t asyncId = getNextAsyncId();

    notifyAsyncOperationSuccess(__FUNCTION__, asyncId);

    Result.v64 = asyncId;
}

YYEXPORT void bt_classic_scan_is_active(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    Result.kind = VALUE_BOOL;
    Result.val = discovery == nullptr ? false : discovery->isDiscovering();
}

YYEXPORT void bt_classic_scan_stop(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    Result.kind = VALUE_INT64;
    Result.v64 = -1;

    if (discovery == nullptr) return;

    discovery->cancelDiscovery();

    int64_t asyncId = getNextAsyncId();

    notifyAsyncOperationSuccess(__FUNCTION__, asyncId);

    Result.v64 = asyncId;
}

YYEXPORT void bt_classic_discoverability_enable(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    Result.kind = VALUE_BOOL;
    Result.val = false;

    HANDLE hRadio = getRadioHandle(__FUNCTION__);
    if (hRadio == NULL) return;

    double timeout = YYGetReal(arg, 0);

    // Enable discovery
    Result.val = BluetoothEnableDiscovery(hRadio, TRUE);

    // Start a thread that waits for the specified timeout then disables discovery
    std::thread([timeout, hRadio]() {
        std::this_thread::sleep_for(std::chrono::seconds((int)timeout));

        if (hRadio != nullptr)
            BluetoothEnableDiscovery(hRadio, FALSE);
        }).detach();
}

YYEXPORT void bt_classic_discoverability_is_active(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    Result.kind = VALUE_BOOL;
    Result.val = false;

    HANDLE hRadio = getRadioHandle(__FUNCTION__);
    if (hRadio == NULL) return;

    Result.kind = VALUE_BOOL;
    Result.val = BluetoothIsDiscoverable(hRadio);
}

YYEXPORT void bt_classic_server_start(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg) {

    Result.kind = VALUE_INT64;
    Result.v64 = -1;

    if (server == nullptr) {
        HANDLE hRadio = getRadioHandle(__FUNCTION__);
        if (hRadio == NULL) return;

        server = std::make_unique<BluetoothServer>(hRadio);
    }

    if (server->isRunning()) {
        DebugConsoleOutput("bt_classic_server_create :: Server is already running.\n");
        return;
    }

    const char* guid = YYGetString(arg, 0);
    server->start(StringToGUID((char*)guid), "BT Windows", "This is a Windows Bluetooth server!");

    int64_t asyncId = getNextAsyncId();

    notifyAsyncOperationSuccess(__FUNCTION__, asyncId);

    Result.v64 = asyncId;
}

YYEXPORT void bt_classic_server_stop(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg) {
    
    Result.kind = VALUE_INT64;
    Result.v64 = -1;

    if (server != nullptr) {
        server->stop();

        int64_t asyncId = getNextAsyncId();

        notifyAsyncOperationSuccess(__FUNCTION__, asyncId);

        Result.v64 = asyncId;
    }
    else {
        DebugConsoleOutput("bt_classic_server_close :: There was no previously opened connection.\n");
    }
}

YYEXPORT void bt_classic_socket_send(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    Result.kind = VALUE_REAL;
    Result.val = 0;

    int64_t socketId = YYGetInt64(arg, 0);
    int32_t buffId = YYGetInt32(arg, 1);
    int32_t offset = YYGetInt32(arg, 2);
    double length = YYGetReal(arg, 3);

    ::IBuffer* ibuff = BufferGetFromGML(buffId);
    unsigned char* buff = BufferGet(ibuff);
    buff += offset;

    std::shared_ptr<BluetoothConnection> connection = connectionManager->getConnection(socketId, __FUNCTION__);
    if (connection != nullptr) {
        connection->send(buff, (int)length);
    }
}

YYEXPORT void bt_classic_socket_receive(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    Result.kind = VALUE_REAL;
    Result.val = 0;

    int64_t socketId = YYGetInt64(arg, 0);
    int32_t buffId = YYGetInt32(arg, 1);
    int32_t offset = YYGetInt32(arg, 2);

    const int bufferSize = 1024;
    unsigned char tempBuffer[bufferSize];

    std::shared_ptr<BluetoothConnection> connection = connectionManager->getConnection(socketId, __FUNCTION__);
    if (connection != nullptr) {
        int errorCode = 0;
        int result = connection->receive((void*)&tempBuffer, 1024, errorCode);

        // The socket was closed remotely
        if (result == 0) {
            NotificationManager::notifySocketRemotelyClosed(socketId);
            connectionManager->closeConnection(socketId);
        }
        // There was an error
        else if (result < 0) {
            // Check if it isn't just lack of available data
            if (errorCode != WSAEWOULDBLOCK) {
                NotificationManager::notifySocketError(socketId, result);
            }
            // Result should never be negative
            result = 0;
        }
        BufferWriteContent(buffId, offset, (void*)&tempBuffer, result);

        Result.val = result;
    }
}

YYEXPORT void bt_classic_socket_open(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    Result.kind = VALUE_INT64;
    Result.v64 = -1;

    const char* address = YYGetString(arg, 0);
    const char* guid = YYGetString(arg, 1);

    int64_t asyncId = getNextAsyncId();

    connectionManager->createConnection(StringToAddress(address), StringToGUID((char*)guid), asyncId);
}

YYEXPORT void bt_classic_socket_close(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    Result.kind = VALUE_INT64;
    Result.v64 = -1;

    int64_t asyncId = getNextAsyncId();

    int64_t socketId = YYGetInt64(arg, 0);
    connectionManager->closeConnection(socketId);

    notifyAsyncOperationSuccess(__FUNCTION__, asyncId);
}

YYEXPORT void bt_classic_socket_close_all(RValue& Result, CInstance* selfinst, CInstance* otherinst, int argc, RValue* arg)
{
    Result.kind = VALUE_BOOL;
    Result.val = true;

    int64_t socketId = YYGetInt64(arg, 0);
    connectionManager->closeAllConnections();
}

// PRIVATE

bool bluetoothClassicInit() {
    DebugConsoleOutput("Initializing Bluetooth...\n");

    ULONG iResult = 0;

    WSADATA WSAData = { 0 };
    WORD wVersionRequested = MAKEWORD(2, 2);

    if ((iResult = WSAStartup(wVersionRequested, &WSAData)) != 0)
    {
        DebugConsoleOutput("WSAStartup failed with error code: %i\n", WSAGetLastError());
        return false;
    }
    return true;
}

void createBluetoothRadioStruct(RValue& Result, const BLUETOOTH_RADIO_INFO& radioInfo) {

    // Add name
    char name[256];
    sprintf(name, "%ls", radioInfo.szName);
    YYStructAddString(&Result, "name", name);

    // Add addressed
    char address[18];
    sprintf(address, "%02X:%02X:%02X:%02X:%02X:%02X",
        radioInfo.address.rgBytes[5],
        radioInfo.address.rgBytes[4],
        radioInfo.address.rgBytes[3],
        radioInfo.address.rgBytes[2],
        radioInfo.address.rgBytes[1],
        radioInfo.address.rgBytes[0]);
    YYStructAddString(&Result, "address", address);

    // Add device class
    YYStructAddInt64(&Result, "class", radioInfo.ulClassofDevice);

    // The manufactor ID
    YYStructAddDouble(&Result, "manufacturer", radioInfo.manufacturer);
}

