#pragma once

#include <xpllink/discovery/DiscoveredDevice.h>
#include <xpllink/logging/SerialTraceLogger.h>
#include <xpllink/transport/IByteTransport.h>

#include <chrono>
#include <optional>
#include <string_view>

namespace xpllink::discovery
{
    struct LegacyXplproProbeTrace
    {
        logging::ISerialTraceSink* serialTrace = nullptr;
        std::string_view portName;
    };

    class LegacyXplproProbe
    {
    public:
        explicit LegacyXplproProbe(
            std::chrono::milliseconds retryInterval =
            std::chrono::milliseconds{ 250 },
            std::chrono::milliseconds timeout =
            std::chrono::seconds{ 4 },
            std::chrono::milliseconds openSettleDelay =
            std::chrono::seconds{ 0 });

        std::optional<DiscoveredDevice> probe(
            transport::IByteTransport& transport,
            LegacyXplproProbeTrace trace = {}) const;

    private:
        std::chrono::milliseconds retryInterval_;
        std::chrono::milliseconds timeout_;
        std::chrono::milliseconds openSettleDelay_;
    };
}
