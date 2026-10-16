/**
 * @module home
 * @title Bluetooth
 *
 * @section Extension's Features
 * @desc
 *
 * * Scan for Bluetooth Low Energy (LE) devices, with filters, and read their advertisements
 * * Connect to LE devices as a client and discover, read, write and subscribe to their GATT services, characteristics and descriptors
 * * Run a GATT server of your own, advertise it, and answer the read and write requests of connected devices
 * * Scan for, connect to and host Bluetooth Classic (RFCOMM) serial connections
 * * Pair with devices, list paired devices and reconnect to a device you saw before without scanning
 * * Ask for the Bluetooth permissions and query what the current platform supports
 *
 * @section_end
 *
 * @section Introduction
 *
 * @desc
 *
 * This extension gives you access to Bluetooth on **Windows, macOS, iOS and Android** through one API.
 * Bluetooth Low Energy works on all four platforms. Bluetooth Classic works on Windows, Android and
 * macOS; on iOS ${function.bluetooth_classic_is_supported} returns `false` and the Classic functions
 * that start something fail with ${constant.BluetoothError}.NotSupported. The extension requires
 * GameMaker 2026.0 or later.
 *
 * Not every platform can do everything: advertising, for example, is limited to a subset of the
 * advertisement on Apple platforms. You should call ${function.bluetooth_feature_is_supported} after
 * ${function.bluetooth_initialize} to find out what the current platform and adapter can do, rather
 * than checking `os_type`.
 *
 * You must call ${function.bluetooth_initialize} once before anything else, and on Android and Apple
 * platforms ask for permission with ${function.bluetooth_permission_request} before you scan, connect
 * or advertise. ${page.getting_started} walks through a first scan and connection, and
 * ${page.platform_setup} lists what each platform needs from your project.
 *
 * Every function that can fail reports it in one of two places. A function **returns** a
 * ${constant.BluetoothError} straight away for anything that can be rejected before the operation
 * starts, such as an invalid handle, a missing permission or the radio being off, and when that value
 * is not `Ok` its callback is never called. You can read the reason with
 * ${function.bluetooth_last_error_message}. Once the operation has started, the callback is called
 * exactly once, and its first two arguments are an `error_code` and a `message` describing the
 * outcome. The two connect functions are the exception to the first rule: they return a connection
 * handle, or 0 when the connect did not start.
 *
 * Devices, connections and GATT attributes are all identified by handles, which are numbers that the
 * extension gives you. Byte data that arrives from a device is held by the extension and given to you
 * as a value handle and a size, which you copy into a buffer of your own with
 * ${function.bluetooth_value_fetch}.
 *
 * @section_end
 *
 * @section Guides
 * @desc Guides for the Bluetooth extension.
 * @reference page.getting_started
 * @reference page.platform_setup
 * @reference page.extension_options
 * @section_end
 *
 * @section Modules
 * @desc The following are the available modules for the Bluetooth extension:
 *
 * @reference module.core
 * @reference module.devices
 * @reference module.le_client
 * @reference module.le_server
 * @reference module.classic
 *
 * @section_end
 *
 * @module_end
 */
