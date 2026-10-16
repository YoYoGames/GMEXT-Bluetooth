# GMEXT-Bluetooth
Repository for GameMaker's Bluetooth Extension

This repository was created with the intent of presenting users with the latest version available of the extension (even previous to marketplace updates) and also provide a way for the community to contribute with bug fixes and feature implementation.

This extension gives you Bluetooth Low Energy (scanning, a GATT client and a GATT server with advertising) and Bluetooth Classic (an RFCOMM client and server) through one API.

This extension works on Windows, macOS, iOS and Android. Bluetooth Classic works on Windows, macOS and Android; iOS has Bluetooth Low Energy only.

> [!IMPORTANT]
> **Version 2.0.0 is a breaking change.** The extension has been rewritten, with the platform
> bindings generated from a single GMIDL spec. Every function is renamed from `bt_*` to
> `bluetooth_*`, and the signatures and callbacks differ from 1.x, so moving an existing project
> across is a migration rather than a drop-in upgrade. Projects that need the old API should stay on
> the final 1.x release. Check [the documentation](../../wiki) for the current API.

The API surface is declared in a single GMIDL spec and the platform bindings are generated from it, so the layout is by role rather than by IDE project:

* API SPEC: `source/Bluetooth_gml/extensions/GMBluetooth/source/spec.gmidl`
* NATIVE C++ (the shared core, and the Windows and Apple backends): `source/Bluetooth_gml/extensions/GMBluetooth/source/src/native/`
* iOS (the Objective-C class the extension registers, which forwards to the core): `source/Bluetooth_gml/extensions/GMBluetooth/source/src/ios/`
* ANDROID (Java): `source/Bluetooth_gml/extensions/GMBluetooth/AndroidSource/Java/`
* GENERATED BINDINGS: `source/Bluetooth_gml/extensions/GMBluetooth/source/code_gen/`, `source/Bluetooth_gml/scripts/GMBluetooth_API/` and `docs/extgen_docs.js` (never edit by hand - regenerate them from the spec with extgen)
* BUILD PRESETS: `source/Bluetooth_gml/extensions/GMBluetooth/source/CMakePresets.json`

After compilation the exported dll/dylib/xcframework is automatically copied into the extension folder inside the included GameMaker project folder.

## Requirements

The extension uses each platform's own Bluetooth APIs, so there is no third-party SDK to download.

The native code is built with **CMake 3.18+** and a **C++17** compiler. On Windows you need Visual Studio 2022 with the Desktop development with C++ workload and a Windows 10 SDK, which provides the WinRT headers. The macOS and iOS presets build on a Mac with Xcode. Android needs no preset: GameMaker builds `AndroidSource/Java` with the game.

From `source/Bluetooth_gml/extensions/GMBluetooth/source/` you can configure and build with one of the provided presets, for example:

```
cmake --preset win-x64-release
cmake --build --preset win-x64-release
```

The other release presets are `macos-release` and `ios-xcframework-native-release`.

> [!IMPORTANT]
> On macOS, GameMaker cannot add the Bluetooth usage description to your game for you. Add
> `NSBluetoothAlwaysUsageDescription` to the game's `Info.plist` and, if the game is sandboxed, the
> `com.apple.security.device.bluetooth` entitlement. The Platform Setup page of the documentation
> explains how.

## Documentation

* Check [the documentation](../../wiki)

The online documentation is regularly updated to ensure it contains the most current information. For those who prefer a different format, we also offer a HTML version. This HTML is directly converted from the GitHub Wiki content, ensuring consistency, although it may follow slightly behind in updates.

We encourage users to refer primarily to the GitHub Wiki for the latest information and updates. The HTML version, included with the extension and within the demo project's data files, serves as a secondary, static reference.

Additionally, if you're contributing new features through PR (Pull Requests), we kindly ask that you also provide accompanying documentation for these features, to maintain the comprehensiveness and usefulness of our resources.
