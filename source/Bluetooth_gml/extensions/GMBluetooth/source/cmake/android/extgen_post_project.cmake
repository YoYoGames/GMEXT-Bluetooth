# Android post-project hook: code_gen source discovery, includes, defines, link libraries
#
# Android is an ExtGen Java implementation (GMBluetooth extends GMBluetoothInternal,
# see AndroidSource/Java/GMBluetooth.java and third_party/CMakeLists.txt). The
# code_gen/native/*.cpp sources define the __EXT_NATIVE__* entry points that call
# into the free bluetooth_* functions implemented in src/native/GMBluetooth_native.cpp
# for the JNI-backed platforms (Windows/Apple/Linux) — they must NOT be compiled here,
# since nothing on Android defines those free functions and the link would fail with
# undefined references. Only the generic wire-format core is needed.

file(GLOB SRC_CORE CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/code_gen/core/*.cpp
)
file(GLOB SRC_COMMON CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/code_gen/*.cpp
)

target_sources(${PROJECT_NAME} PRIVATE ${SRC_CORE} ${SRC_COMMON})
target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/code_gen
  ${CMAKE_CURRENT_SOURCE_DIR}/code_gen/core
)
target_compile_definitions(${PROJECT_NAME} PRIVATE OS_ANDROID EXTGEN_HAS_JNI=0)

# The generated core (GMExtUtils.cpp) logs via __android_log_print on Android.
find_library(_log_lib log)
target_link_libraries(${PROJECT_NAME} PRIVATE ${_log_lib})

message(STATUS "Android ABI=${CMAKE_ANDROID_ARCH_ABI}, STL=c++_static (Java-only implementation)")
