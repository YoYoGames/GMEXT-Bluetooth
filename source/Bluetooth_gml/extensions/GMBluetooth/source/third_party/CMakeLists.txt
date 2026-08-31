cmake_minimum_required(VERSION 3.18)

# =============================================================================
# GMBluetooth platform integration
#
# IMPORTANT:
#   - Do not edit the generator-owned root/source CMakeLists.txt files.
#   - Handwritten common C++ stays in ../src/native.
#   - Handwritten platform implementations stay in ../src/native/<platform>.
#   - Platform subdirectory sources are injected into EXT_MAIN_TARGET here.
# =============================================================================

add_library(ext_third_party INTERFACE)

set(_GMBLUETOOTH_NATIVE_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../src/native")

# Common headers such as GMBluetooth_backend.h are visible to platform sources.
target_include_directories(ext_third_party
    INTERFACE
        "${_GMBLUETOOTH_NATIVE_DIR}")

# -----------------------------------------------------------------------------
# Windows
# -----------------------------------------------------------------------------
if(WIN32)
    set(_GMBLUETOOTH_WINDOWS_SOURCES
        "${_GMBLUETOOTH_NATIVE_DIR}/windows/GMBluetooth_windows.cpp")

    target_sources(${EXT_MAIN_TARGET}
        PRIVATE
            ${_GMBLUETOOTH_WINDOWS_SOURCES})

    # Windows.Devices.Bluetooth is a Windows Runtime API. C++/WinRT projections
    # are provided by the installed Windows SDK; WindowsApp supplies the WinRT
    # import libraries required by the desktop extension.
    target_link_libraries(ext_third_party
        INTERFACE
            windowsapp)

    target_compile_definitions(ext_third_party
        INTERFACE
            _WIN32_WINNT=0x0A00)
endif()
