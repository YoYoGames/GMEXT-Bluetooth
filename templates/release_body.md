## IMPORTANT

- This extension requires GameMaker **2026.0** or later.
- Requires the bundled **ExtensionCore** dependency.
- Bluetooth Low Energy works with **Windows**, **macOS**, **iOS** and **Android**.
- Bluetooth Classic works with **Windows**, **macOS** and **Android**.
- macOS builds need a Bluetooth usage description and, when sandboxed, the Bluetooth entitlement, added by hand: see the Platform Setup page of the documentation.

## DESCRIPTION

This extension gives you access to Bluetooth Low Energy and Bluetooth Classic on Windows, macOS, iOS and Android through one API.

## CHANGES SINCE ${releaseOldVersion}

https://github.com/YoYoGames/GMEXT-Bluetooth/compare/${releaseOldVersion}...${releaseNewVersion}

## FEATURES

### Bluetooth Low Energy (Windows|macOS|iOS|Android)
- Scan for devices, with filters, and read their advertisement data
- Connect as a client and discover, read, write and subscribe to services, characteristics and descriptors
- Negotiate the MTU and read the RSSI of a connection
- Run a GATT server, advertise it and answer read and write requests
- Reconnect to a known device without scanning

### Bluetooth Classic (Windows|macOS|Android)
- Scan for devices and connect to RFCOMM services
- Run an RFCOMM server that accepts several connections
- Send and receive data on each connection

### Common
- Pairing and paired-device queries
- Permission requests and a per-platform feature query

## DOCUMENTATION

The full documentation of the API is included in the extension asset (included files).
