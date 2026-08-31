cmake_minimum_required(VERSION 3.18)

# =============================================================================
# GMBluetooth platform integration
#
# Generator-owned files are not edited. All handwritten native code lives in
# src/native and per-platform subfolders; this is the only handwritten CMake.
# =============================================================================

add_library(ext_third_party INTERFACE)

set(_GMBLUETOOTH_NATIVE_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../src/native")

target_include_directories(ext_third_party
    INTERFACE
        "${_GMBLUETOOTH_NATIVE_DIR}")

# -----------------------------------------------------------------------------
# Windows
# -----------------------------------------------------------------------------
if(WIN32)
    target_sources(${EXT_MAIN_TARGET}
        PRIVATE
            "${_GMBLUETOOTH_NATIVE_DIR}/windows/GMBluetooth_windows.cpp")

    # BLE: Windows Runtime / C++/WinRT
    # Classic: Win32 Bluetooth + Winsock RFCOMM + SDP registration
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
    # Extension Generator's desktop source discovery compiles src/native/*.cpp,
    # but Android needs the portable common native source injected explicitly.
    target_sources(${EXT_MAIN_TARGET}
        PRIVATE
            "${_GMBLUETOOTH_NATIVE_DIR}/GMBluetooth_native.cpp"
            "${_GMBLUETOOTH_NATIVE_DIR}/android/GMBluetooth_android.cpp")

    # ExtGen owns code_gen/android/GMBluetoothInternal_jni.cpp and generates its
    # own JNI_OnLoad. Rename that symbol only for the generated translation unit;
    # GMBluetooth_android.cpp provides the real JNI_OnLoad, captures JavaVM, then
    # delegates to the renamed ExtGen loader.
    set(_GMBLUETOOTH_EXTGEN_JNI
        "${CMAKE_CURRENT_LIST_DIR}/../code_gen/android/GMBluetoothInternal_jni.cpp")

    if(EXISTS "${_GMBLUETOOTH_EXTGEN_JNI}")
        # Source-file properties are directory scoped in CMake. This
        # third_party/CMakeLists.txt runs in a different directory scope than
        # the one where EXT_MAIN_TARGET was created, so explicitly apply the
        # definition in the target's directory scope.
        set_source_files_properties(
            "${_GMBLUETOOTH_EXTGEN_JNI}"
            TARGET_DIRECTORY ${EXT_MAIN_TARGET}
            PROPERTIES
                COMPILE_DEFINITIONS
                    "JNI_OnLoad=GMBluetooth_ExtGen_JNI_OnLoad")
    else()
        message(FATAL_ERROR
            "GMBluetooth Android generated JNI source was not found: ${_GMBLUETOOTH_EXTGEN_JNI}")
    endif()

    target_link_libraries(ext_third_party
        INTERFACE
            android
            log)
endif()
