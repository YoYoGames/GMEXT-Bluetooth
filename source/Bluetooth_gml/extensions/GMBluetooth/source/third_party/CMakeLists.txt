cmake_minimum_required(VERSION 3.18)

# =============================================================================
# GMBluetooth platform integration
#
# Android uses the ExtGen Java template implementation:
#   AndroidSource/Java/GMBluetooth.java
#
# iOS uses the ExtGen Objective-C++ template implementation:
#   source/src/ios/GMBluetooth_ios.mm
# which delegates to the common native core and shared Apple CoreBluetooth
# backend.
#
# macOS uses the same common native core + Apple CoreBluetooth backend directly.
# Linux is intentionally omitted for this milestone.
# =============================================================================

add_library(ext_third_party INTERFACE)

set(_GMBLUETOOTH_NATIVE_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../src/native")

set(_GMBLUETOOTH_COMMON_CPP
    "${_GMBLUETOOTH_NATIVE_DIR}/GMBluetooth_native.cpp")

set(_GMBLUETOOTH_APPLE_MM
    "${_GMBLUETOOTH_NATIVE_DIR}/apple/GMBluetooth_apple.mm")

target_include_directories(ext_third_party
    INTERFACE
        "${_GMBLUETOOTH_NATIVE_DIR}")

# -----------------------------------------------------------------------------
# Windows
# -----------------------------------------------------------------------------
if(WIN32)
    target_sources(${EXT_MAIN_TARGET}
        PRIVATE
            "${_GMBLUETOOTH_COMMON_CPP}"
            "${_GMBLUETOOTH_NATIVE_DIR}/windows/GMBluetooth_windows.cpp")

    target_link_libraries(ext_third_party
        INTERFACE
            windowsapp
            Ws2_32
            Bthprops
            Ole32)

    target_compile_definitions(ext_third_party
        INTERFACE
            _WIN32_WINNT=0x0A00)

# -----------------------------------------------------------------------------
# Android
# -----------------------------------------------------------------------------
elseif(ANDROID)
    # Intentionally empty. Android is an ExtGen Java implementation, matching
    # the supplied GMBluetooth.java template:
    #
    #   public class GMBluetooth extends GMBluetoothInternal
    #
    # Do not route Android through C++/JNI again.

# -----------------------------------------------------------------------------
# Apple: iOS + macOS
# -----------------------------------------------------------------------------
elseif(APPLE)
    # ExtGen's macOS native build already discovers src/native/*.cpp. The iOS
    # native-framework path does not, so add the shared common core explicitly
    # for iOS only.
    if(CMAKE_OSX_SYSROOT MATCHES "iphone")
        target_sources(${EXT_MAIN_TARGET}
            PRIVATE
                "${_GMBLUETOOTH_COMMON_CPP}")
    endif()

    # Both iOS and macOS use the mature old-extension CoreBluetooth transport.
    target_sources(${EXT_MAIN_TARGET}
        PRIVATE
            "${_GMBLUETOOTH_APPLE_MM}")

    find_library(COREBLUETOOTH_FRAMEWORK CoreBluetooth REQUIRED)
    find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
    find_library(CORELOCATION_FRAMEWORK CoreLocation REQUIRED)

    target_link_libraries(ext_third_party
        INTERFACE
            "${COREBLUETOOTH_FRAMEWORK}"
            "${FOUNDATION_FRAMEWORK}"
            "${CORELOCATION_FRAMEWORK}")
endif()
