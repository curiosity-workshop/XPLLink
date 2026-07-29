#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace xpllink::protocol::legacy
{
    constexpr char packetHeader = '[';
    constexpr char packetTrailer = ']';
    constexpr char sendNameCommand = 'N';
    constexpr char sendRequestCommand = 'Q';
    constexpr char profileAcceptedCommand = 'A';
    constexpr char dataFlowPauseCommand = 'p';
    constexpr char dataFlowResumeCommand = 'q';
    constexpr char setDataFlowSpeedCommand = 'f';
    constexpr char registerDataRefRequestCommand = 'b';
    constexpr char registerCommandRequestCommand = 'm';
    constexpr char dataRefResponseCommand = 'D';
    constexpr char commandResponseCommand = 'C';
    constexpr char printDebugCommand = 'g';
    constexpr char speakCommand = 's';
    constexpr char dataRefTouchRequestCommand = 'd';
    constexpr char updatesRequestCommand = 'r';
    constexpr char updatesArrayRequestCommand = 't';
    constexpr char updatesTypeRequestCommand = 'y';
    constexpr char updatesTypeArrayRequestCommand = 'w';
    constexpr char scalingRequestCommand = 'u';
    constexpr char resetCommand = 'z';
    constexpr char dataRefUpdateIntCommand = '1';
    constexpr char dataRefUpdateFloatCommand = '2';
    constexpr char dataRefUpdateIntArrayCommand = '3';
    constexpr char dataRefUpdateFloatArrayCommand = '4';
    constexpr char dataRefUpdateStringCommand = '9';
    constexpr char commandTriggerCommand = 'k';
    constexpr char commandStartCommand = 'i';
    constexpr char commandEndCommand = 'j';
    constexpr char exitingCommand = 'X';
    constexpr char nameResponseCommand = 'n';
    constexpr char versionResponseCommand = 'v';
    constexpr std::size_t maximumFrameSize = 200;

    struct LegacyFrame
    {
        char command = '\0';
        std::string payload;
        std::string binaryPayload;
        std::string raw;
    };

    std::vector<std::byte> makeFrame(
        char command,
        std::string_view payload = {});
}
