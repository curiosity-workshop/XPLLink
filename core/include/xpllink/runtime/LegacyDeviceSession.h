#pragma once

#include <xpllink/logging/SerialTraceLogger.h>
#include <xpllink/protocol/legacy/LegacyFrameParser.h>
#include <xpllink/protocol/legacy/LegacyMessage.h>
#include <xpllink/transport/IByteTransport.h>

#include <cstddef>
#include <deque>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace xpllink::runtime
{
    struct LegacyDeviceSessionOptions
    {
        std::size_t readBufferSize = 256;
        std::size_t maximumReadPasses = 16;
        logging::ISerialTraceSink* serialTrace = nullptr;
        std::string tracePortName;
    };

    struct LegacyDeviceSessionTickResult
    {
        bool transportOpen = false;
        std::size_t bytesRead = 0;
        std::size_t bytesWritten = 0;
        std::vector<protocol::legacy::LegacyMessage> messages;
    };

    class LegacyDeviceSession
    {
    public:
        explicit LegacyDeviceSession(
            transport::IByteTransport& transport,
            LegacyDeviceSessionOptions options = {});

        LegacyDeviceSessionTickResult tick();

        void queueFrame(
            char command,
            std::string_view payload = {});

        void queueBytes(
            std::span<const std::byte> bytes);

        bool hasPendingOutput() const;
        bool hasPartialInputFrame() const;
        std::size_t flushPendingOutput();

    private:
        std::size_t pollIncoming(
            std::vector<protocol::legacy::LegacyMessage>& messages);

        transport::IByteTransport& transport_;
        LegacyDeviceSessionOptions options_;
        protocol::legacy::LegacyFrameParser parser_;
        std::deque<std::byte> outgoing_;
    };
}
