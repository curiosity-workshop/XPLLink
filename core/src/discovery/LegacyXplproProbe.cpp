#include <xpllink/discovery/LegacyXplproProbe.h>
#include <xpllink/protocol/legacy/LegacyFrame.h>
#include <xpllink/protocol/legacy/LegacyFrameParser.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <string>
#include <thread>

namespace xpllink::discovery
{
    namespace
    {
        constexpr int maximumProbeReadPasses = 16;

        std::string extractQuotedString(std::string_view payload)
        {
            const std::size_t firstQuote = payload.find('"');

            if (firstQuote == std::string_view::npos)
            {
                return {};
            }

            const std::size_t secondQuote =
                payload.find('"', firstQuote + 1);

            if (secondQuote == std::string_view::npos)
            {
                return {};
            }

            return std::string{
                payload.substr(
                firstQuote + 1,
                secondQuote - firstQuote - 1)
            };
        }
    }

    LegacyXplproProbe::LegacyXplproProbe(
        std::chrono::milliseconds retryInterval,
        std::chrono::milliseconds timeout,
        std::chrono::milliseconds openSettleDelay)
        : retryInterval_(retryInterval),
        timeout_(timeout),
        openSettleDelay_(openSettleDelay)
    {
    }

    std::optional<DiscoveredDevice>
        LegacyXplproProbe::probe(
            transport::IByteTransport& transport,
            LegacyXplproProbeTrace trace) const
    {
        if (!transport.isOpen())
        {
            return std::nullopt;
        }

        DiscoveredDevice discoveredDevice;

        std::array<std::byte, 256> readBuffer{};
        protocol::legacy::LegacyFrameParser parser;

        const auto request =
            protocol::legacy::makeFrame(
                protocol::legacy::sendNameCommand);

        const auto startTime =
            std::chrono::steady_clock::now();

        const auto deadline =
            startTime + openSettleDelay_ + timeout_;

        auto nextRequestTime = startTime + openSettleDelay_;

        while (std::chrono::steady_clock::now() < deadline)
        {
            const auto currentTime =
                std::chrono::steady_clock::now();

            if (currentTime >= nextRequestTime)
            {
                const std::size_t bytesWritten =
                    transport.write(request);

                if (trace.serialTrace != nullptr &&
                    bytesWritten > 0)
                {
                    trace.serialTrace->bytes(
                        logging::ByteDumpDirection::Transmit,
                        trace.portName,
                        std::span<const std::byte>{
                            request.data(),
                            bytesWritten
                        });
                }

                nextRequestTime =
                    currentTime + retryInterval_;
            }

            for (int pass = 0;
                pass < maximumProbeReadPasses;
                ++pass)
            {
                const std::size_t bytesRead =
                    transport.read(readBuffer);

                if (bytesRead == 0)
                {
                    break;
                }

                if (trace.serialTrace != nullptr)
                {
                    trace.serialTrace->bytes(
                        logging::ByteDumpDirection::Receive,
                        trace.portName,
                        std::span<const std::byte>{
                            readBuffer.data(),
                            bytesRead
                        });
                }

                const auto frames =
                    parser.push(
                        std::span<const std::byte>{
                            readBuffer.data(),
                            bytesRead
                        });

                for (const auto& frame : frames)
                {
                    if (frame.command ==
                        protocol::legacy::nameResponseCommand)
                    {
                        discoveredDevice.name =
                            extractQuotedString(frame.payload);
                    }
                    else if (frame.command ==
                        protocol::legacy::versionResponseCommand)
                    {
                        discoveredDevice.version =
                            extractQuotedString(frame.payload);
                    }

                    /*
                     * The name response is enough to prove that this is
                     * an XPLPro device. Version is useful but optional.
                     */
                    if (!discoveredDevice.name.empty())
                    {
                        return discoveredDevice;
                    }
                }
            }

            /*
             * Avoid consuming an entire processor core while still
             * checking often enough for responsive serial handling.
             */
            std::this_thread::sleep_for(
                std::chrono::milliseconds{ 10 });
        }

        return std::nullopt;
    }
}
