set(SRC_DIR ${CMAKE_CURRENT_LIST_DIR})

# =============================================================================
# GameMaker iOS integration (optional)
# =============================================================================
set(EXT_GM_XCODEPROJ_OVERRIDE "/ABS/PATH/TO/GAME.xcodeproj" CACHE STRING "" FORCE)
set(EXT_GM_APP_TARGET_OVERRIDE "" CACHE STRING "" FORCE)

# =============================================================================
# Public headers for Apple XCFramework packaging
# Users should edit these to match their project structure.
# The packager will copy these into a headers folder passed to xcodebuild.
# =============================================================================
set(EXT_PUBLIC_HEADERS_IOS
    "${CMAKE_SOURCE_DIR}/src/ios/${PROJECT_NAME}_ios.h;${CMAKE_SOURCE_DIR}/code_gen/ios/${PROJECT_NAME}Internal_ios.h"
    CACHE STRING "Semicolon-separated list of public headers to embed in the iOS xcframework")

# =============================================================================
# User implementation sources per platform (editable template)
# =============================================================================
if(WIN32)
  file(GLOB TARGET_SRCS CONFIGURE_DEPENDS "${SRC_DIR}/native/*.cpp")

elseif(CMAKE_OSX_SYSROOT MATCHES "iphone")
  # iOS: only add iOS wrapper sources, native implementation added by third_party
  file(GLOB TARGET_SRCS CONFIGURE_DEPENDS "${SRC_DIR}/ios/*.mm" "${SRC_DIR}/ios/*.cpp")

  # Pinned, or the slices are built for the host's newest SDK and fail to link
  # into an older app. Set here because the xcframework packager reconfigures
  # from scratch and reads only this file, not the presets.
  set_target_properties(${PROJECT_NAME} PROPERTIES XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "12.0")

elseif(APPLE)
  # macOS: include both native implementation and Apple backend
  file(GLOB APPLE_SRCS CONFIGURE_DEPENDS "${SRC_DIR}/native/apple/*.mm" "${SRC_DIR}/native/apple/*.cpp")
  file(GLOB TARGET_SRCS CONFIGURE_DEPENDS "${SRC_DIR}/native/*.mm" "${SRC_DIR}/native/*.cpp" "${SRC_DIR}/native/*.swift")
  list(APPEND TARGET_SRCS ${APPLE_SRCS})

elseif(ANDROID)
  # Android is implemented entirely in Java (GMBluetooth extends GMBluetoothInternal,
  # see AndroidSource/Java/GMBluetooth.java): no native or JNI sources.
  set(TARGET_SRCS "")

elseif(UNIX)
  file(GLOB TARGET_SRCS CONFIGURE_DEPENDS "${SRC_DIR}/native/*.cpp")
endif()

target_sources(${PROJECT_NAME} PRIVATE ${TARGET_SRCS})
target_include_directories(${PROJECT_NAME} PRIVATE "${SRC_DIR}")
