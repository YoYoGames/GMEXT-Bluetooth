

#include "BluetoothCore.h"
#include "BluetoothTools.h"

#include <thread>
#include <mutex>
#include <vector>

void createBluetoothRadioStruct(RValue& Result, const BLUETOOTH_RADIO_INFO& radioInfo);

class BluetoothServer {
public:
    BluetoothServer(HANDLE radio)
        : m_radio(radio), m_guid(GUID()), m_name(""), m_description(""), m_running(false), m_serverSocket(INVALID_SOCKET) {
    };

    ~BluetoothServer() {
        if (m_running) {
            stop();
        }
    }

    void start(GUID guid, std::string name, std::string description);

    bool isRunning() { return m_running; }

    void stop();

private:
    void run(std::stop_token stopToken);

    SOCKET setupServer(std::string mName, std::string mDescription, GUID mServiceGuid);

    HANDLE m_radio;
    GUID m_guid;

    // Default values
    std::string m_name;
    std::string m_description;

    std::atomic<bool> m_running;
    SOCKET m_serverSocket;

    std::jthread m_serverThread;
};

class BluetoothConnection {
public:
    BluetoothConnection(SOCKET socket) : m_socket(socket) {};
    ~BluetoothConnection() { }

    int disconnect();

    int send(void* buff, int length);
    int receive(void* buff, int length, int& errorCode);

private:
    SOCKET m_socket;
    std::mutex m_socketMutex;
};

class BluetoothConnectionManager {
public:
    BluetoothConnectionManager() {};
    ~BluetoothConnectionManager() {
        // Close all existing connections
        closeAllConnections();
    };

    void createConnection(BTH_ADDR address, GUID guid, int64_t asyncId);

    int64_t addConnection(std::shared_ptr<BluetoothConnection> connection);

    std::shared_ptr<BluetoothConnection> getConnection(int64_t socketId, const char* methodName);

    void closeConnection(int64_t socketId);

    void closeAllConnections();

private:

    int64_t nextSocketId();
    SOCKET connectToServer(BTH_ADDR address, GUID guid);

    std::mutex m_socketIdMutex;
    int64_t m_socketId = 0;

    std::mutex connectionsMutex; // Mutex to protect access to sockets
    std::map<int64_t, std::shared_ptr<BluetoothConnection>> connections = {};
};

class BluetoothDiscovery {
public:
    BluetoothDiscovery(HANDLE radio)
        : m_radio(radio) {}

    ~BluetoothDiscovery() {
        if (m_discovering) {
            cancelDiscovery();
        }
    }

    void startDiscovery();

    void cancelDiscovery();

    bool isDiscovering() const;

private:
    void discoverDevices(std::stop_token stopToken);

    std::atomic<bool> m_discovering;

    HANDLE m_radio;
    std::jthread m_discoveryThread;
};

class NotificationManager {
public:
    static void notifyServerCreateError(int errorCode);
    static void notifyServerCreateSuccess(const char* name, const char* description);

    static void notifyServerAcceptedConnection(SOCKADDR_BTH client, int64_t socketId);

    static void notifyServerCloseError(int errorCode);
    static void notifyServerCloseSuccess();

    static void notifyClientJoinError(BTH_ADDR address, GUID guid, int errorCode);
    static void notifyClientJoinSuccess(BTH_ADDR address, GUID guid, int64_t socketId);

    static void notifyReceiverFound(const BLUETOOTH_DEVICE_INFO& deviceInfo);

    static void notifyDiscoveryStarted();

    static void notifyDiscoveryFinished();

    static void notifySocketError(int64_t socketId, int errorCode);
    static void notifySocketRemotelyClosed(int64_t socketId);

};

extern std::unique_ptr<BluetoothConnectionManager> connectionManager;
extern std::unique_ptr<BluetoothDiscovery> discovery;
extern std::unique_ptr<BluetoothServer> server;
