@title Platform Setup

# Platform Setup

This page lists what each target platform needs from your project before the Bluetooth extension can work. Most of it is added for you when the game is built; what is not is marked as a step you must take yourself.

## Android

The extension adds the Bluetooth permissions to the Android manifest for you:

| Android version | Permissions |
|---|---|
| 11 and below | `BLUETOOTH`, `BLUETOOTH_ADMIN`, `ACCESS_FINE_LOCATION` and `ACCESS_COARSE_LOCATION` |
| 12 and above | `BLUETOOTH_SCAN` (declared with `neverForLocation`), `BLUETOOTH_CONNECT` and `BLUETOOTH_ADVERTISE` |

These are runtime permissions, so the player must also grant them: call ${function.bluetooth_permission_request} before you scan, connect or advertise, and every call that needs a missing permission fails with ${constant.BluetoothError}.PermissionDenied until they are granted.

Because `BLUETOOTH_SCAN` is declared with `neverForLocation`, a scan on Android 12 and above never asks for location and cannot be used to derive the player's location. On Android 11 and below, a scan needs the location permission **and** the system Location setting switched on; with Location off, ${function.bluetooth_le_scan_start} and ${function.bluetooth_classic_scan_start} fail with ${constant.BluetoothError}.PermissionDenied and a message that says so.

The extension also declares Bluetooth and Bluetooth LE as optional hardware features, so Google Play does not hide your game from devices without them. On such a device the `*_is_supported` functions return `false`.

[[Note: Android allows an app to start an LE scan at most five times in 30 seconds. A sixth start succeeds but finds nothing, so you should keep a scan running rather than restarting it often.]]

## iOS

The extension adds `NSBluetoothAlwaysUsageDescription` and `NSBluetoothPeripheralUsageDescription` to the game's `Info.plist`, using the text from the **Bluetooth usage description** extension option, and adds the Bluetooth background modes you choose in **Bluetooth background modes**. See ${page.extension_options}.

The system asks the player for Bluetooth access the first time you call ${function.bluetooth_permission_request}, and not before. Until the player has answered, every call that uses Bluetooth fails with ${constant.BluetoothError}.PermissionDenied.

[[Important: Only declare the background modes your game actually uses. App Review rejects a game that declares a Bluetooth background mode it does not use.]]

A scan only keeps running in the background when the **Central** background mode is declared and every ${struct.BluetoothLeScanFilter} you pass sets `service_uuid`.

Bluetooth Classic is not available on iOS.

## macOS

GameMaker has no way for an extension to add keys to the macOS `Info.plist` or entitlements, so on macOS you must add them yourself:

- **`NSBluetoothAlwaysUsageDescription`** in the game's `Info.plist`, with the text the player sees when macOS asks for Bluetooth access. Without it macOS does not let the game use Bluetooth.
- **`com.apple.security.device.bluetooth`**, set to `true` in the game's entitlements, if the game runs in the App Sandbox (as it must for the Mac App Store).

In a YYC build you can add both to the Xcode project that GameMaker generates, under its **Supporting Files**, before building it in Xcode. For a build you sign yourself, add them to the application bundle's `Contents/Info.plist` and to the entitlements you sign it with.

## Windows

Windows needs no setup. Bluetooth LE uses the Windows Runtime Bluetooth APIs and Bluetooth Classic uses Winsock RFCOMM, both of which are part of Windows. ${function.bluetooth_permission_get_status} always returns ${constant.BluetoothPermissionStatus}.Granted.

What the game can do depends on the player's Bluetooth adapter, so ask ${function.bluetooth_feature_is_supported} after ${function.bluetooth_initialize} rather than assuming a feature is there. An adapter that is plugged in later is picked up while the game runs and reported through the state callback (${function.bluetooth_set_callback_state_changed}).
