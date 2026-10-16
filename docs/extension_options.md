@title Extension Options

# Extension Options

You can open the options for the **GMBluetooth** extension from the Asset Browser, either by double-clicking the extension or by right-clicking it and choosing **Properties**. Both options are used only when you build for iOS; the other platforms ignore them.

## iOS Configuration

| Option | Default | What it does |
|---|---|---|
| **Bluetooth usage description** | "This app uses Bluetooth to discover, connect to, and communicate with nearby devices." | The text iOS shows the player when it asks for Bluetooth access. It is written into the game's `Info.plist` as both `NSBluetoothAlwaysUsageDescription` and `NSBluetoothPeripheralUsageDescription`. You should say why your game needs Bluetooth. |
| **Bluetooth background modes** | None | The Bluetooth background modes the game declares in `UIBackgroundModes`. **Central** keeps scans and connections running while the game is in the background, **Peripheral** keeps advertising and the GATT server running, and **Both** declares the two. |

[[Important: Only declare the background modes your game actually uses. App Review rejects a game that declares a Bluetooth background mode it does not use.]]

macOS has no extension options: see ${page.platform_setup} for the keys you must add to a macOS build yourself.
