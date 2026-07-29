cmake_minimum_required(VERSION 3.20)

project(XPLLink
    VERSION 0.1.0
    LANGUAGES CXX)

set(XPLLINK_PLUGIN_NAME
    "XPLLink"
    CACHE STRING
    "User-facing X-Plane plugin name and output .xpl base name")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(XPLLINK_BUILD_XPLANE_PLUGIN
    "Build the X-Plane plugin target"
    OFF)

set(XPLLINK_XPLANE_SDK_PATH
    ""
    CACHE PATH
    "Path to the X-Plane SDK root")

set(XPLLINK_XPLANE_ROOT
    ""
    CACHE PATH
    "Optional local X-Plane installation root for plugin deployment")

set(XPLLINK_XPLANE_PLUGIN_DEPLOY_DIR
    ""
    CACHE PATH
    "Optional exact directory where the plugin .xpl should be copied after build")

set(XPLLINK_DISTRIBUTION_DIR
    "${CMAKE_SOURCE_DIR}/dist/${XPLLINK_PLUGIN_NAME}"
    CACHE PATH
    "Directory where distributable plugin files are copied after build")

option(XPLLINK_DISTRIBUTE_DEBUG_BUILDS
    "Allow Debug plugin builds to update the distributable dist/XPLLink folder"
    OFF)

if(NOT XPLLINK_XPLANE_SDK_PATH AND DEFINED ENV{XPLANE_SDK_ROOT})
    set(XPLLINK_XPLANE_SDK_PATH
        "$ENV{XPLANE_SDK_ROOT}"
        CACHE PATH
        "Path to the X-Plane SDK root"
        FORCE)
endif()

if(NOT XPLLINK_XPLANE_SDK_PATH AND EXISTS "${CMAKE_SOURCE_DIR}/third_party/xplane_sdk/CHeaders/XPLM/XPLMDataAccess.h")
    set(XPLLINK_XPLANE_SDK_PATH
        "${CMAKE_SOURCE_DIR}/third_party/xplane_sdk"
        CACHE PATH
        "Path to the X-Plane SDK root"
        FORCE)
endif()

set(XPLLINK_SERIAL_SOURCES
    core/src/serial/SerialDeviceClassifier.cpp
)

if(WIN32)
    list(APPEND XPLLINK_SERIAL_SOURCES
        core/src/serial/WindowsSerialEnumerator.cpp
        core/src/serial/WindowsSerialTransport.cpp
        core/src/serial/WindowsSerialTransportFactory.cpp
    )
elseif(APPLE)
    list(APPEND XPLLINK_SERIAL_SOURCES
        core/src/serial/MacSerialEnumerator.cpp
        core/src/serial/MacSerialTransport.cpp
        core/src/serial/MacSerialTransportFactory.cpp
    )
else()
    list(APPEND XPLLINK_SERIAL_SOURCES
        core/src/serial/LinuxSerialEnumerator.cpp
        core/src/serial/LinuxSerialTransport.cpp
        core/src/serial/LinuxSerialTransportFactory.cpp
    )
endif()

add_executable(XPLLink
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
    ${XPLLINK_SERIAL_SOURCES}

    # Discovery
    core/src/discovery/LegacyXplproProbe.cpp

    # X-Plane SDK skeleton
    core/src/xplane/sdk/XPlaneSdkBridge.cpp
)

target_include_directories(XPLLink
    PRIVATE
        core/include
)

target_compile_definitions(XPLLink
    PRIVATE
        XPLLINK_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

if(WIN32)
    target_link_libraries(XPLLink
        PRIVATE
            Setupapi
    )
endif()

enable_testing()

add_executable(XPLLinkProtocolTests
    tests/protocol/LegacyFrameParserTests.cpp
    core/src/protocol/legacy/LegacyFrame.cpp
    core/src/protocol/legacy/LegacyFrameParser.cpp
    core/src/protocol/legacy/LegacyMessage.cpp
)

target_include_directories(XPLLinkProtocolTests
    PRIVATE
        core/include
)

add_test(
    NAME XPLLinkProtocolTests
    COMMAND XPLLinkProtocolTests
)

add_executable(XPLLinkRuntimeTests
    tests/runtime/LegacyDeviceSessionTests.cpp
    core/src/logging/SerialTraceLogger.cpp
    core/src/protocol/legacy/LegacyFrame.cpp
    core/src/protocol/legacy/LegacyFrameParser.cpp
    core/src/protocol/legacy/LegacyMessage.cpp
    core/src/runtime/LegacyDeviceSession.cpp
)

target_include_directories(XPLLinkRuntimeTests
    PRIVATE
        core/include
)

add_test(
    NAME XPLLinkRuntimeTests
    COMMAND XPLLinkRuntimeTests
)

add_executable(XPLLinkDeviceControllerTests
    tests/runtime/LegacyDeviceControllerTests.cpp
    core/src/logging/SerialTraceLogger.cpp
    core/src/protocol/legacy/LegacyFrame.cpp
    core/src/protocol/legacy/LegacyFrameParser.cpp
    core/src/protocol/legacy/LegacyMessage.cpp
    core/src/runtime/LegacyDeviceController.cpp
    core/src/runtime/LegacyDeviceSession.cpp
)

target_include_directories(XPLLinkDeviceControllerTests
    PRIVATE
        core/include
)

add_test(
    NAME XPLLinkDeviceControllerTests
    COMMAND XPLLinkDeviceControllerTests
)

add_executable(XPLLinkUpdateSchedulerTests
    tests/runtime/LegacyUpdateSchedulerTests.cpp
    core/src/logging/SerialTraceLogger.cpp
    core/src/protocol/legacy/LegacyFrame.cpp
    core/src/protocol/legacy/LegacyFrameParser.cpp
    core/src/protocol/legacy/LegacyMessage.cpp
    core/src/runtime/LegacyDeviceController.cpp
    core/src/runtime/LegacyDeviceSession.cpp
    core/src/runtime/LegacyUpdateScheduler.cpp
)

target_include_directories(XPLLinkUpdateSchedulerTests
    PRIVATE
        core/include
)

add_test(
    NAME XPLLinkUpdateSchedulerTests
    COMMAND XPLLinkUpdateSchedulerTests
)

add_executable(XPLLinkProfileTests
    tests/profile/JsonDeviceProfileStoreTests.cpp
    core/src/profile/JsonDeviceProfileStore.cpp
)

target_include_directories(XPLLinkProfileTests
    PRIVATE
        core/include
)

add_test(
    NAME XPLLinkProfileTests
    COMMAND XPLLinkProfileTests
)

add_executable(XPLLinkXPlaneSdkBridgeTests
    tests/xplane/XPlaneSdkBridgeTests.cpp
    core/src/xplane/sdk/XPlaneSdkBridge.cpp
)

target_include_directories(XPLLinkXPlaneSdkBridgeTests
    PRIVATE
        core/include
)

add_test(
    NAME XPLLinkXPlaneSdkBridgeTests
    COMMAND XPLLinkXPlaneSdkBridgeTests
)

if(XPLLINK_BUILD_XPLANE_PLUGIN)
    if(NOT XPLLINK_XPLANE_SDK_PATH)
        message(FATAL_ERROR
            "XPLLINK_XPLANE_SDK_PATH must be set when XPLLINK_BUILD_XPLANE_PLUGIN is ON. "
            "Set it directly, set XPLANE_SDK_ROOT, or place the SDK at third_party/xplane_sdk.")
    endif()

    message(STATUS
        "Using X-Plane SDK: ${XPLLINK_XPLANE_SDK_PATH}")

    set(XPLLINK_XPLANE_SDK_INCLUDE_DIRS
        "${XPLLINK_XPLANE_SDK_PATH}/CHeaders/XPLM"
        "${XPLLINK_XPLANE_SDK_PATH}/CHeaders/Widgets"
    )

    add_library(XPLLinkXPlanePlugin MODULE
        plugin/XPlanePlugin.cpp
        plugin/XPLLinkStatusWindow.cpp

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
        ${XPLLINK_SERIAL_SOURCES}

        # X-Plane SDK adapter
        core/src/xplane/sdk/XPlaneSdkApi.cpp
        core/src/xplane/sdk/XPlaneSdkBridge.cpp
    )

    target_include_directories(XPLLinkXPlanePlugin
        PRIVATE
            core/include
            ${XPLLINK_XPLANE_SDK_INCLUDE_DIRS}
    )

    target_compile_definitions(XPLLinkXPlanePlugin
        PRIVATE
            XPLM200=1
            XPLM210=1
            XPLM300=1
            XPLM301=1
            XPLM400=1
    )

    set_target_properties(XPLLinkXPlanePlugin
        PROPERTIES
            PREFIX ""
            OUTPUT_NAME "${XPLLINK_PLUGIN_NAME}"
            SUFFIX ".xpl"
    )

    if(WIN32)
        target_compile_definitions(XPLLinkXPlanePlugin
            PRIVATE
                IBM=1
                APL=0
                LIN=0
        )

        target_link_libraries(XPLLinkXPlanePlugin
            PRIVATE
                "${XPLLINK_XPLANE_SDK_PATH}/Libraries/Win/XPLM_64.lib"
                "${XPLLINK_XPLANE_SDK_PATH}/Libraries/Win/XPWidgets_64.lib"
                Setupapi
        )

        set(XPLLINK_XPLANE_PLUGIN_ABI_DIR "win_x64")
    elseif(APPLE)
        target_compile_definitions(XPLLinkXPlanePlugin
            PRIVATE
                IBM=0
                APL=1
                LIN=0
        )

        target_link_libraries(XPLLinkXPlanePlugin
            PRIVATE
                "${XPLLINK_XPLANE_SDK_PATH}/Libraries/Mac/XPLM.framework/Versions/C/XPLM"
                "${XPLLINK_XPLANE_SDK_PATH}/Libraries/Mac/XPWidgets.framework/Versions/C/XPWidgets"
        )

        set(XPLLINK_XPLANE_PLUGIN_ABI_DIR "mac_x64")
    else()
        target_compile_definitions(XPLLinkXPlanePlugin
            PRIVATE
                IBM=0
                APL=0
                LIN=1
        )

        target_link_libraries(XPLLinkXPlanePlugin
            PRIVATE
                "${XPLLINK_XPLANE_SDK_PATH}/Libraries/Lin/XPLM_64.so"
                "${XPLLINK_XPLANE_SDK_PATH}/Libraries/Lin/XPWidgets_64.so"
        )

        set(XPLLINK_XPLANE_PLUGIN_ABI_DIR "lin_x64")
    endif()

    if(NOT XPLLINK_XPLANE_PLUGIN_DEPLOY_DIR AND XPLLINK_XPLANE_ROOT)
        set(XPLLINK_XPLANE_PLUGIN_DEPLOY_DIR
            "${XPLLINK_XPLANE_ROOT}/Resources/plugins/${XPLLINK_PLUGIN_NAME}/${XPLLINK_XPLANE_PLUGIN_ABI_DIR}"
            CACHE PATH
            "Optional exact directory where the plugin .xpl should be copied after build"
            FORCE)
    endif()

    if(XPLLINK_XPLANE_PLUGIN_DEPLOY_DIR)
        add_custom_command(
            TARGET XPLLinkXPlanePlugin
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                    "${XPLLINK_XPLANE_PLUGIN_DEPLOY_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:XPLLinkXPlanePlugin>"
                    "${XPLLINK_XPLANE_PLUGIN_DEPLOY_DIR}/$<TARGET_FILE_NAME:XPLLinkXPlanePlugin>"
            COMMENT
                    "Deploying ${XPLLINK_PLUGIN_NAME}.xpl to ${XPLLINK_XPLANE_PLUGIN_DEPLOY_DIR}"
        )
    endif()

    set(XPLLINK_CAN_UPDATE_DISTRIBUTION TRUE)

    if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND
        NOT XPLLINK_DISTRIBUTE_DEBUG_BUILDS)
        set(XPLLINK_CAN_UPDATE_DISTRIBUTION FALSE)
    endif()

    if(XPLLINK_DISTRIBUTION_DIR AND XPLLINK_CAN_UPDATE_DISTRIBUTION)
        set(XPLLINK_DISTRIBUTION_PLUGIN_DIR
            "${XPLLINK_DISTRIBUTION_DIR}/${XPLLINK_XPLANE_PLUGIN_ABI_DIR}")

        add_custom_command(
            TARGET XPLLinkXPlanePlugin
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                    "${XPLLINK_DISTRIBUTION_PLUGIN_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:XPLLinkXPlanePlugin>"
                    "${XPLLINK_DISTRIBUTION_PLUGIN_DIR}/$<TARGET_FILE_NAME:XPLLinkXPlanePlugin>"
            COMMENT
                    "Updating distributable ${XPLLINK_PLUGIN_NAME} plugin at ${XPLLINK_DISTRIBUTION_PLUGIN_DIR}"
        )
    endif()
endif()
