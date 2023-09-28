#pragma once

#include <winsock2.h>
#include <ws2bth.h>
#include <BluetoothAPIs.h>

#include <cstdio>

#include <string>
#include <map>
#include <locale.h>
#include <cmath>
#include <stdexcept>

#include "YYRValue.h"
#include "Extension_Interface.h"

#include <variant>
#include <future>

#include <windows.devices.h>
#include <windows.devices.bluetooth.h>

#include <winrt/windows.foundation.h>
#include <winrt/windows.foundation.collections.h>
#include <winrt/windows.devices.h>
#include <winrt/Windows.Devices.Radios.h>
#include <winrt/windows.devices.bluetooth.h>
#include <winrt/windows.devices.enumeration.h>
#include <winrt/windows.devices.bluetooth.advertisement.h>
#include <winrt/windows.devices.bluetooth.genericattributeprofile.h>
#include <winrt/Windows.security.cryptography.h>
#include <winrt/Windows.security.cryptography.core.h>
#include <winrt/windows.storage.streams.h>

using namespace winrt;
using namespace winrt::Windows;
using namespace winrt::Windows::Foundation;
using namespace Windows::Foundation;
using namespace Windows::Storage::Streams;
using namespace Windows::Foundation::Collections;
using namespace Windows::Devices::Radios;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Bluetooth::Advertisement;
using namespace Windows::Devices::Bluetooth::GenericAttributeProfile;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Security::Cryptography;


void createBluetoothDeviceStruct(RValue& Result, const BLUETOOTH_DEVICE_INFO& deviceInfo);

HANDLE getRadioHandle(const char* methodName);
HANDLE createRadioHandle(const char* methodName);

void winrtInit();
bool bluetoothClassicInit();
void bluetoothLeInit();

int64_t getNextAsyncId();


void notifyOperation(const std::string& functionName, const std::map<std::string, std::variant<std::string, int, int64_t, bool, double, RValue*>> extraParams);

void notifyAsyncOperation(const std::string& functionName, int64_t asyncId, const std::map<std::string, std::variant<std::string, int, int64_t, bool, double, RValue*>> extraParams);

void notifyAsyncOperation(const std::string& functionName, int64_t asyncId);

void notifyAsyncOperationError(const std::string& functionName, int64_t asyncId, uint32_t errorCode, const std::map<std::string, std::variant<std::string, int, int64_t, bool, double, RValue*>> extraParams);

void notifyAsyncOperationError(const std::string& functionName, int64_t asyncId, uint32_t errorCode);

void notifyAsyncOperationSuccess(const std::string& functionName, int64_t asyncId, const std::map<std::string, std::variant<std::string, int, int64_t, bool, double, RValue*>> extraParams);

void notifyAsyncOperationSuccess(const std::string& functionName, int64_t asyncId);
