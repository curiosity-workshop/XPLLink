#pragma once

#include <xpllink/protocol/legacy/LegacyFrame.h>

#include <optional>
#include <string>
#include <variant>

namespace xpllink::protocol::legacy
{
    enum class DataRefValueType
    {
        Int,
        Float,
        IntArray,
        FloatArray,
        String
    };

    enum class CommandAction
    {
        Start,
        End,
        Trigger
    };

    struct IdentityRequest {};
    struct RegistrationRequest {};
    struct DataFlowPause {};
    struct DataFlowResume {};
    struct ResetRequest {};
    struct Exiting {};

    struct DeviceName
    {
        std::string value;
    };

    struct DeviceVersion
    {
        std::string value;
    };

    struct RegisterDataRef
    {
        std::string name;
    };

    struct RegisterCommand
    {
        std::string name;
    };

    struct DataRefHandle
    {
        int handle = -1;
        std::optional<std::string> name;
    };

    struct CommandHandle
    {
        int handle = -1;
        std::optional<std::string> name;
    };

    struct DataRefUpdate
    {
        DataRefValueType type = DataRefValueType::Int;
        int handle = -1;
        std::string value;
        std::optional<int> element;
    };

    struct StringDataRefUpdate
    {
        int handle = -1;
        int byteCount = 0;
        std::string value;
    };

    struct DataRefTouch
    {
        int handle = -1;
    };

    struct UpdatesRequest
    {
        int handle = -1;
        std::optional<int> requestedType;
        int rate = 0;
        std::string precision;
        std::optional<int> element;
    };

    struct ScalingRequest
    {
        int handle = -1;
        long fromLow = 0;
        long fromHigh = 0;
        long toLow = 0;
        long toHigh = 0;
    };

    struct CommandEvent
    {
        CommandAction action = CommandAction::Trigger;
        int handle = -1;
        int triggerCount = 1;
    };

    struct DebugMessage
    {
        std::string value;
    };

    struct SpeakMessage
    {
        std::string value;
    };

    struct SetDataFlowSpeed
    {
        long bytesPerSecond = 0;
    };

    struct UnknownMessage
    {
        LegacyFrame frame;
    };

    using LegacyMessage = std::variant<
        IdentityRequest,
        RegistrationRequest,
        DataFlowPause,
        DataFlowResume,
        ResetRequest,
        Exiting,
        DeviceName,
        DeviceVersion,
        RegisterDataRef,
        RegisterCommand,
        DataRefHandle,
        CommandHandle,
        DataRefUpdate,
        StringDataRefUpdate,
        DataRefTouch,
        UpdatesRequest,
        ScalingRequest,
        CommandEvent,
        DebugMessage,
        SpeakMessage,
        SetDataFlowSpeed,
        UnknownMessage>;

    LegacyMessage decodeFrame(const LegacyFrame& frame);
}
