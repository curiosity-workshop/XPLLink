cmake_minimum_required(VERSION 3.20)

project(Phoenix
    VERSION 0.1.0
    LANGUAGES CXX)

set(PHOENIX_PLUGIN_NAME
    "XPLLink"
    CACHE STRING
    "User-facing X-Plane plugin name and output .xpl base name")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(PHOENIX_BUILD_XPLANE_PLUGIN
    "Build the X-Plane plugin target"
    OFF)

set(PHOENIX_XPLANE_SDK_PATH
    ""
    CACHE PATH
    "Path to the X-Plane SDK root")

set(PHOENIX_XPLANE_ROOT
    ""
    CACHE PATH
    "Optional local X-Plane installation root for plugin deployment")

set(PHOENIX_XPLANE_PLUGIN_DEPLOY_DIR
    ""
    CACHE PATH
    "Optional exact directory where the plugin .xpl should be copied after build")

set(PHOENIX_DISTRIBUTION_DIR
    "${CMAKE_SOURCE_DIR}/dist/${PHOENIX_PLUGIN_NAME}"
    CACHE PATH
    "Directory where distributable plugin files are copied after build")

option(PHOENIX_DISTRIBUTE_DEBUG_BUILDS
    "Allow Debug plugin builds to update the distributable dist/Phoenix folder"
    OFF)

if(NOT PHOENIX_XPLANE_SDK_PATH AND DEFINED ENV{XPLANE_SDK_ROOT})
    set(PHOENIX_XPLANE_SDK_PATH
        "$ENV{XPLANE_SDK_ROOT}"
        CACHE PATH
        "Path to the X-Plane SDK root"
        FORCE)
endif()

if(NOT PHOENIX_XPLANE_SDK_PATH AND EXISTS "${CMAKE_SOURCE_DIR}/third_party/xplane_sdk/CHeaders/XPLM/XPLMDataAccess.h")
    set(PHOENIX_XPLANE_SDK_PATH
        "${CMAKE_SOURCE_DIR}/third_party/xplane_sdk"
        CACHE PATH
        "Path to the X-Plane SDK root"
        FORCE)
endif()

set(PHOENIX_SERIAL_SOURCES
    core/src/serial/SerialDeviceClassifier.cpp
)

if(WIN32)
    list(APPEND PHOENIX_SERIAL_SOURCES
        core/src/serial/WindowsSerialEnumerator.cpp
        core/src/serial/WindowsSerialTransport.cpp
        core/src/serial/WindowsSerialTransportFactory.cpp
    )
elseif(APPLE)
    list(APPEND PHOENIX_SERIAL_SOURCES
        core/src/serial/MacSerialEnumerator.cpp
        core/src/serial/MacSerialTransport.cpp
        core/src/serial/MacSerialTransportFactory.cpp
    )
else()
    list(APPEND PHOENIX_SERIAL_SOURCES
        core/src/serial/LinuxSerialEnumerator.cpp
        core/src/serial/LinuxSerialTransport.cpp
        core/src/serial/LinuxSerialTransportFactory.cpp
    )
endif()

add_executable(Phoenix
    # Application
    core/src/main.cpp
    core/src/dev/DevelopmentDeviceLoop.cpp

    # Logging
    core/src/logging/Log.cpp
    core/src/logging/SerialTraceLogger.cpp

    # Profile
    core/src/profile/JsonDeviceProfileStore.cpp

    # Protocol
    core/src/protocol/legacy/LegacyFrame.cpp
    core/src/protocol/legacy/LegacyFrameParser.cpp
    core/src/protocol/legacy/LegacyMessage.cpp

    # Runtime
    core/src/runtime/DeviceRuntimeManager.cpp
    core/src/runtime/LegacyDeviceController.cpp
    core/src/runtime/LegacyDeviceSession.cpp
    core/src/runtime/LegacyUpdateScheduler.cpp

    # Serial
    ${PHOENIX_SERIAL_SOURCES}

    # Discovery
    core/src/discovery/LegacyXplproProbe.cpp

    # X-Plane SDK skeleton
    core/src/xplane/sdk/XPlaneSdkBridge.cpp
)

target_include_directories(Phoenix
    PRIVATE
        core/include
)

target_compile_definitions(Phoenix
    PRIVATE
        PHOENIX_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

if(WIN32)
    target_link_libraries(Phoenix
        PRIVATE
            Setupapi
    )
endif()

enable_testing()

add_executable(PhoenixProtocolTests
    tests/protocol/LegacyFrameParserTests.cpp
    core/src/protocol/legacy/LegacyFrame.cpp
    core/src/protocol/legacy/LegacyFrameParser.cpp
    core/src/protocol/legacy/LegacyMessage.cpp
)

target_include_directories(PhoenixProtocolTests
    PRIVATE
        core/include
)

add_test(
    NAME PhoenixProtocolTests
    COMMAND PhoenixProtocolTests
)

add_executable(PhoenixRuntimeTests
    tests/runtime/LegacyDeviceSessionTests.cpp
    core/src/logging/SerialTraceLogger.cpp
    core/src/protocol/legacy/LegacyFrame.cpp
    core/src/protocol/legacy/LegacyFrameParser.cpp
    core/src/protocol/legacy/LegacyMessage.cpp
    core/src/runtime/LegacyDeviceSession.cpp
)

target_include_directories(PhoenixRuntimeTests
    PRIVATE
        core/include
)

add_test(
    NAME PhoenixRuntimeTests
    COMMAND PhoenixRuntimeTests
)

add_executable(PhoenixDeviceControllerTests
    tests/runtime/LegacyDeviceControllerTests.cpp
    core/src/logging/SerialTraceLogger.cpp
    core/src/protocol/legacy/LegacyFrame.cpp
    core/src/protocol/legacy/LegacyFrameParser.cpp
    core/src/protocol/legacy/LegacyMessage.cpp
    core/src/runtime/LegacyDeviceController.cpp
    core/src/runtime/LegacyDeviceSession.cpp
)

target_include_directories(PhoenixDeviceControllerTests
    PRIVATE
        core/include
)

add_test(
    NAME PhoenixDeviceControllerTests
    COMMAND PhoenixDeviceControllerTests
)

add_executable(PhoenixUpdateSchedulerTests
    tests/runtime/LegacyUpdateSchedulerTests.cpp
    core/src/logging/SerialTraceLogger.cpp
    core/src/protocol/legacy/LegacyFrame.cpp
    core/src/protocol/legacy/LegacyFrameParser.cpp
    core/src/protocol/legacy/LegacyMessage.cpp
    core/src/runtime/LegacyDeviceController.cpp
    core/src/runtime/LegacyDeviceSession.cpp
    core/src/runtime/LegacyUpdateScheduler.cpp
)

target_include_directories(PhoenixUpdateSchedulerTests
    PRIVATE
        core/include
)

add_test(
    NAME PhoenixUpdateSchedulerTests
    COMMAND PhoenixUpdateSchedulerTests
)

add_executable(PhoenixProfileTests
    tests/profile/JsonDeviceProfileStoreTests.cpp
    core/src/profile/JsonDeviceProfileStore.cpp
)

target_include_directories(PhoenixProfileTests
    PRIVATE
        core/include
)

add_test(
    NAME PhoenixProfileTests
    COMMAND PhoenixProfileTests
)

add_executable(PhoenixXPlaneSdkBridgeTests
    tests/xplane/XPlaneSdkBridgeTests.cpp
    core/src/xplane/sdk/XPlaneSdkBridge.cpp
)

target_include_directories(PhoenixXPlaneSdkBridgeTests
    PRIVATE
        core/include
)

add_test(
    NAME PhoenixXPlaneSdkBridgeTests
    COMMAND PhoenixXPlaneSdkBridgeTests
)

if(PHOENIX_BUILD_XPLANE_PLUGIN)
    if(NOT PHOENIX_XPLANE_SDK_PATH)
        message(FATAL_ERROR
            "PHOENIX_XPLANE_SDK_PATH must be set when PHOENIX_BUILD_XPLANE_PLUGIN is ON. "
            "Set it directly, set XPLANE_SDK_ROOT, or place the SDK at third_party/xplane_sdk.")
    endif()

    message(STATUS
        "Using X-Plane SDK: ${PHOENIX_XPLANE_SDK_PATH}")

    set(PHOENIX_XPLANE_SDK_INCLUDE_DIRS
        "${PHOENIX_XPLANE_SDK_PATH}/CHeaders/XPLM"
        "${PHOENIX_XPLANE_SDK_PATH}/CHeaders/Widgets"
    )

    add_library(PhoenixXPlanePlugin MODULE
        plugin/XPlanePlugin.cpp
        plugin/PhoenixStatusWindow.cpp

        # Logging
        core/src/logging/Log.cpp
        core/src/logging/SerialTraceLogger.cpp

        # Profile
        core/src/profile/JsonDeviceProfileStore.cpp

        # Protocol
        core/src/protocol/legacy/LegacyFrame.cpp
        core/src/protocol/legacy/LegacyFrameParser.cpp
        core/src/protocol/legacy/LegacyMessage.cpp

        # Runtime
        core/src/runtime/DeviceRuntimeManager.cpp
        core/src/runtime/LegacyDeviceController.cpp
        core/src/runtime/LegacyDeviceSession.cpp
        core/src/runtime/LegacyUpdateScheduler.cpp

        # Serial
        ${PHOENIX_SERIAL_SOURCES}

        # X-Plane SDK adapter
        core/src/xplane/sdk/XPlaneSdkApi.cpp
        core/src/xplane/sdk/XPlaneSdkBridge.cpp
    )

    target_include_directories(PhoenixXPlanePlugin
        PRIVATE
            core/include
            ${PHOENIX_XPLANE_SDK_INCLUDE_DIRS}
    )

    target_compile_definitions(PhoenixXPlanePlugin
        PRIVATE
            XPLM200=1
            XPLM210=1
            XPLM300=1
            XPLM301=1
            XPLM400=1
    )

    set_target_properties(PhoenixXPlanePlugin
        PROPERTIES
            PREFIX ""
            OUTPUT_NAME "${PHOENIX_PLUGIN_NAME}"
            SUFFIX ".xpl"
    )

    if(WIN32)
        target_compile_definitions(PhoenixXPlanePlugin
            PRIVATE
                IBM=1
                APL=0
                LIN=0
        )

        target_link_libraries(PhoenixXPlanePlugin
            PRIVATE
                "${PHOENIX_XPLANE_SDK_PATH}/Libraries/Win/XPLM_64.lib"
                "${PHOENIX_XPLANE_SDK_PATH}/Libraries/Win/XPWidgets_64.lib"
                Setupapi
        )

        set(PHOENIX_XPLANE_PLUGIN_ABI_DIR "win_x64")
    elseif(APPLE)
        target_compile_definitions(PhoenixXPlanePlugin
            PRIVATE
                IBM=0
                APL=1
                LIN=0
        )

        target_link_libraries(PhoenixXPlanePlugin
            PRIVATE
                "${PHOENIX_XPLANE_SDK_PATH}/Libraries/Mac/XPLM.framework"
                "${PHOENIX_XPLANE_SDK_PATH}/Libraries/Mac/XPWidgets.framework"
        )

        set(PHOENIX_XPLANE_PLUGIN_ABI_DIR "mac_x64")
    else()
        target_compile_definitions(PhoenixXPlanePlugin
            PRIVATE
                IBM=0
                APL=0
                LIN=1
        )

        target_link_libraries(PhoenixXPlanePlugin
            PRIVATE
                "${PHOENIX_XPLANE_SDK_PATH}/Libraries/Lin/XPLM_64.so"
                "${PHOENIX_XPLANE_SDK_PATH}/Libraries/Lin/XPWidgets_64.so"
        )

        set(PHOENIX_XPLANE_PLUGIN_ABI_DIR "lin_x64")
    endif()

    if(NOT PHOENIX_XPLANE_PLUGIN_DEPLOY_DIR AND PHOENIX_XPLANE_ROOT)
        set(PHOENIX_XPLANE_PLUGIN_DEPLOY_DIR
            "${PHOENIX_XPLANE_ROOT}/Resources/plugins/${PHOENIX_PLUGIN_NAME}/${PHOENIX_XPLANE_PLUGIN_ABI_DIR}"
            CACHE PATH
            "Optional exact directory where the plugin .xpl should be copied after build"
            FORCE)
    endif()

    if(PHOENIX_XPLANE_PLUGIN_DEPLOY_DIR)
        add_custom_command(
            TARGET PhoenixXPlanePlugin
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                    "${PHOENIX_XPLANE_PLUGIN_DEPLOY_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:PhoenixXPlanePlugin>"
                    "${PHOENIX_XPLANE_PLUGIN_DEPLOY_DIR}/$<TARGET_FILE_NAME:PhoenixXPlanePlugin>"
            COMMENT
                    "Deploying ${PHOENIX_PLUGIN_NAME}.xpl to ${PHOENIX_XPLANE_PLUGIN_DEPLOY_DIR}"
        )
    endif()

    set(PHOENIX_CAN_UPDATE_DISTRIBUTION TRUE)

    if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND
        NOT PHOENIX_DISTRIBUTE_DEBUG_BUILDS)
        set(PHOENIX_CAN_UPDATE_DISTRIBUTION FALSE)
    endif()

    if(PHOENIX_DISTRIBUTION_DIR AND PHOENIX_CAN_UPDATE_DISTRIBUTION)
        set(PHOENIX_DISTRIBUTION_PLUGIN_DIR
            "${PHOENIX_DISTRIBUTION_DIR}/${PHOENIX_XPLANE_PLUGIN_ABI_DIR}")

        add_custom_command(
            TARGET PhoenixXPlanePlugin
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                    "${PHOENIX_DISTRIBUTION_PLUGIN_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:PhoenixXPlanePlugin>"
                    "${PHOENIX_DISTRIBUTION_PLUGIN_DIR}/$<TARGET_FILE_NAME:PhoenixXPlanePlugin>"
            COMMENT
                    "Updating distributable ${PHOENIX_PLUGIN_NAME} plugin at ${PHOENIX_DISTRIBUTION_PLUGIN_DIR}"
        )
    endif()
endif()
