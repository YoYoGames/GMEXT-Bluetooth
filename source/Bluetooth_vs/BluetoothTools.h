

#include "BluetoothCore.h"

template <typename AsyncOp, typename Callback>
void run_sync(AsyncOp asyncOp, Callback callback) {
    // Create an event for synchronization
    auto eventHandle = ::CreateEvent(NULL, TRUE, FALSE, NULL);

    if (eventHandle == nullptr) return;

    // Start the async task
    asyncOp.Completed([eventHandle, callback = std::move(callback)](auto&& asyncInfo, auto&& asyncStatus)
        {
            // This will be called when the async operation is completed
            try
            {
                callback(std::forward<decltype(asyncInfo)>(asyncInfo));
            }
            catch (...)
            {
                // Handle error
            }
            ::SetEvent(eventHandle); // Signal that the async operation is done
        });

    // Wait for the async operation to complete
    ::WaitForSingleObject(eventHandle, 10000);
    ::CloseHandle(eventHandle);
}

GUID StringToGUID(char* str);
std::string AddressToString(const BYTE address[]);
BTH_ADDR StringToAddress(const std::string& str);
std::string AddressToString(BTH_ADDR n);
char* GUIDtoString(GUID guid);

