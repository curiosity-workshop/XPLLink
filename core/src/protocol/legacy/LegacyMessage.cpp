#include <xpllink/protocol/legacy/LegacyMessage.h>

#include <cstdlib>
#include <string_view>
#include <vector>

namespace xpllink::protocol::legacy
{
    namespace
    {
        std::vector<std::string_view> splitPayload(
            std::string_view payload)
        {
            std::vector<std::string_view> fields;

            if (payload.starts_with(','))
            {
                payload.remove_prefix(1);
            }

            std::size_t start = 0;
            bool inQuote = false;

            for (std::size_t index = 0;
                index < payload.size();
                ++index)
            {
                if (payload[index] == '"')
                {
                    inQuote = !inQuote;
                    continue;
                }

                if (payload[index] != ',' || inQuote)
                {
                    continue;
                }

                fields.push_back(
                    payload.substr(start, index - start));
                start = index + 1;
            }

            fields.push_back(payload.substr(start));

            return fields;
        }

        std::string unquote(std::string_view value)
        {
            if (value.size() >= 2 &&
                value.front() == '"' &&
                value.back() == '"')
            {
                value.remove_prefix(1);
                value.remove_suffix(1);
            }

            return std::string{ value };
        }

        int toInt(std::string_view value)
        {
            return std::atoi(std::string{ value }.c_str());
        }

        long toLong(std::string_view value)
        {
            return std::atol(std::string{ value }.c_str());
        }

        std::optional<std::string> optionalStringField(
            const std::vector<std::string_view>& fields,
            std::size_t index)
        {
            if (index >= fields.size() || fields[index].empty())
            {
                return std::nullopt;
            }

            return unquote(fields[index]);
        }
    }

    LegacyMessage decodeFrame(const LegacyFrame& frame)
    {
        const auto fields = splitPayload(frame.payload);

        switch (frame.command)
        {
        case sendNameCommand:
            return IdentityRequest{};

        case sendRequestCommand:
            return RegistrationRequest{};

        case dataFlowPauseCommand:
            return DataFlowPause{};

        case dataFlowResumeCommand:
            return DataFlowResume{};

        case resetCommand:
            return ResetRequest{};

        case exitingCommand:
            return Exiting{};

        case nameResponseCommand:
            return DeviceName{ optionalStringField(fields, 0).value_or("") };

        case versionResponseCommand:
            return DeviceVersion{ optionalStringField(fields, 0).value_or("") };

        case registerDataRefRequestCommand:
            return RegisterDataRef{ optionalStringField(fields, 0).value_or("") };

        case registerCommandRequestCommand:
            return RegisterCommand{ optionalStringField(fields, 0).value_or("") };

        case dataRefResponseCommand:
            return DataRefHandle{
                fields.empty() ? -1 : toInt(fields[0]),
                optionalStringField(fields, 1)
            };

        case commandResponseCommand:
            return CommandHandle{
                fields.empty() ? -1 : toInt(fields[0]),
                optionalStringField(fields, 1)
            };

        case dataRefUpdateIntCommand:
            return DataRefUpdate{
                DataRefValueType::Int,
                fields.size() > 0 ? toInt(fields[0]) : -1,
                fields.size() > 1 ? std::string{ fields[1] } : "",
                std::nullopt
            };

        case dataRefUpdateFloatCommand:
            return DataRefUpdate{
                DataRefValueType::Float,
                fields.size() > 0 ? toInt(fields[0]) : -1,
                fields.size() > 1 ? std::string{ fields[1] } : "",
                std::nullopt
            };

        case dataRefUpdateIntArrayCommand:
            return DataRefUpdate{
                DataRefValueType::IntArray,
                fields.size() > 0 ? toInt(fields[0]) : -1,
                fields.size() > 1 ? std::string{ fields[1] } : "",
                fields.size() > 2 ?
                    std::optional<int>{ toInt(fields[2]) } :
                    std::nullopt
            };

        case dataRefUpdateFloatArrayCommand:
            return DataRefUpdate{
                DataRefValueType::FloatArray,
                fields.size() > 0 ? toInt(fields[0]) : -1,
                fields.size() > 1 ? std::string{ fields[1] } : "",
                fields.size() > 2 ?
                    std::optional<int>{ toInt(fields[2]) } :
                    std::nullopt
            };

        case dataRefUpdateStringCommand:
            return StringDataRefUpdate{
                fields.size() > 0 ? toInt(fields[0]) : -1,
                fields.size() > 1 ? toInt(fields[1]) : 0,
                frame.binaryPayload
            };

        case dataRefTouchRequestCommand:
            return DataRefTouch{
                fields.empty() ? -1 : toInt(fields[0])
            };

        case updatesRequestCommand:
            return UpdatesRequest{
                fields.size() > 0 ? toInt(fields[0]) : -1,
                std::nullopt,
                fields.size() > 1 ? toInt(fields[1]) : 0,
                fields.size() > 2 ? std::string{ fields[2] } : "",
                std::nullopt
            };

        case updatesArrayRequestCommand:
            return UpdatesRequest{
                fields.size() > 0 ? toInt(fields[0]) : -1,
                std::nullopt,
                fields.size() > 1 ? toInt(fields[1]) : 0,
                fields.size() > 2 ? std::string{ fields[2] } : "",
                fields.size() > 3 ?
                    std::optional<int>{ toInt(fields[3]) } :
                    std::nullopt
            };

        case updatesTypeRequestCommand:
            return UpdatesRequest{
                fields.size() > 0 ? toInt(fields[0]) : -1,
                fields.size() > 1 ?
                    std::optional<int>{ toInt(fields[1]) } :
                    std::nullopt,
                fields.size() > 2 ? toInt(fields[2]) : 0,
                fields.size() > 3 ? std::string{ fields[3] } : "",
                std::nullopt
            };

        case updatesTypeArrayRequestCommand:
            return UpdatesRequest{
                fields.size() > 0 ? toInt(fields[0]) : -1,
                fields.size() > 1 ?
                    std::optional<int>{ toInt(fields[1]) } :
                    std::nullopt,
                fields.size() > 2 ? toInt(fields[2]) : 0,
                fields.size() > 3 ? std::string{ fields[3] } : "",
                fields.size() > 4 ?
                    std::optional<int>{ toInt(fields[4]) } :
                    std::nullopt
            };

        case scalingRequestCommand:
            return ScalingRequest{
                fields.size() > 0 ? toInt(fields[0]) : -1,
                fields.size() > 1 ? toLong(fields[1]) : 0,
                fields.size() > 2 ? toLong(fields[2]) : 0,
                fields.size() > 3 ? toLong(fields[3]) : 0,
                fields.size() > 4 ? toLong(fields[4]) : 0
            };

        case commandStartCommand:
            return CommandEvent{
                CommandAction::Start,
                fields.size() > 0 ? toInt(fields[0]) : -1,
                1
            };

        case commandEndCommand:
            return CommandEvent{
                CommandAction::End,
                fields.size() > 0 ? toInt(fields[0]) : -1,
                1
            };

        case commandTriggerCommand:
            return CommandEvent{
                CommandAction::Trigger,
                fields.size() > 0 ? toInt(fields[0]) : -1,
                fields.size() > 1 ? toInt(fields[1]) : 1
            };

        case printDebugCommand:
            return DebugMessage{ optionalStringField(fields, 0).value_or("") };

        case speakCommand:
            return SpeakMessage{ optionalStringField(fields, 0).value_or("") };

        case setDataFlowSpeedCommand:
            return SetDataFlowSpeed{
                fields.empty() ? 0 : toLong(fields[0])
            };

        default:
            return UnknownMessage{ frame };
        }
    }
}
