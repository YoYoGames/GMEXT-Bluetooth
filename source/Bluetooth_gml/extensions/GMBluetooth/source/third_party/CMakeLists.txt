cmake_minimum_required(VERSION 3.18)

# =============================================================================
# iOS-only build (macOS/Android/Windows deferred)
# =============================================================================

add_library(ext_third_party INTERFACE)

set(_GMBLUETOOTH_NATIVE_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../src/native")

set(_GMBLUETOOTH_COMMON_CPP
    "${_GMBLUETOOTH_NATIVE_DIR}/GMBluetooth_native.cpp")

set(_GMBLUETOOTH_APPLE_MM
    "${_GMBLUETOOTH_NATIVE_DIR}/apple/GMBluetooth_apple.mm")

# Include directories
target_include_directories(ext_third_party
    INTERFACE
        "${_GMBLUETOOTH_NATIVE_DIR}")

# iOS: Add native C++ implementation and Apple backend
if(CMAKE_OSX_SYSROOT MATCHES "iphone")
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
endif()
