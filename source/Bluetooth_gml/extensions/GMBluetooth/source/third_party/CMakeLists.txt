cmake_minimum_required(VERSION 3.18)

# =============================================================================
# GMBluetooth platform integration
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
# iOS: Add native implementation and frameworks
# =============================================================================
elseif(CMAKE_OSX_SYSROOT MATCHES "iphone")
    target_sources(${EXT_MAIN_TARGET}
        PRIVATE
            "${_GMBLUETOOTH_COMMON_CPP}"
            "${_GMBLUETOOTH_APPLE_MM}")

    # Link required iOS frameworks
    target_link_libraries(ext_third_party
        INTERFACE
            "-framework CoreBluetooth"
            "-framework Foundation"
            "-framework CoreLocation")

    target_link_libraries(${EXT_MAIN_TARGET}
        PRIVATE
            "-framework CoreBluetooth"
            "-framework Foundation"
            "-framework CoreLocation")

    # Framework search paths for Xcode
    set_target_properties(${EXT_MAIN_TARGET} PROPERTIES
        XCODE_ATTRIBUTE_FRAMEWORK_SEARCH_PATHS "${CMAKE_OSX_SYSROOT}/System/Library/Frameworks")

    target_link_options(${EXT_MAIN_TARGET}
        PRIVATE
            "-F${CMAKE_OSX_SYSROOT}/System/Library/Frameworks")

# -----------------------------------------------------------------------------
# macOS
# -----------------------------------------------------------------------------
elseif(APPLE AND NOT EXTGEN_IS_APPLE_MOBILE)
    # Sources are NOT added here. Unlike iOS - whose src/ branch only picks up
    # src/ios/* - the macOS branch of src/CMakeLists.txt already globs
    # native/*.cpp and native/apple/*.mm. Adding them again would be redundant.
    #
    # macOS ships as a .dylib rather than a static archive, so it records its
    # own framework dependencies at link time. That is why macOS needs no
    # equivalent of the iOS "iosSystemFrameworkEntries" in GMBluetooth.yy.
    target_link_libraries(ext_third_party
        INTERFACE
            "-framework CoreBluetooth"
            "-framework Foundation"
            "-framework CoreLocation")

    target_link_libraries(${EXT_MAIN_TARGET}
        PRIVATE
            "-framework CoreBluetooth"
            "-framework Foundation"
            "-framework CoreLocation")

endif()
