#include <xpllink/runtime/LegacyDeviceSession.h>

#include <xpllink/logging/Log.h>
#include <xpllink/protocol/legacy/LegacyFrame.h>

#include <algorithm>

namespace xpllink::runtime
{
    LegacyDeviceSession::LegacyDeviceSession(
        transport::IByteTransport& transport,
        LegacyDeviceSessionOptions options)
        : transport_(transport),
        options_(options)
    {
        if (options_.readBufferSize == 0)
        {
            options_.readBufferSize = 256;
        }

        if (options_.maximumReadPasses == 0)
        {
            options_.maximumReadPasses =
                LegacyDeviceSessionOptions{}.maximumReadPasses;
        }
    }

    LegacyDeviceSessionTickResult LegacyDeviceSession::tick()
    {
        LegacyDeviceSessionTickResult result;
        result.transportOpen = transport_.isOpen();

        if (!result.transportOpen)
        {
            return result;
        }

        result.bytesRead = pollIncoming(result.messages);
        result.bytesWritten = flushPendingOutput();

        return result;
    }

    void LegacyDeviceSession::queueFrame(
        char command,
        std::string_view payload)
    {
        const auto frame =
            protocol::legacy::makeFrame(command, payload);

        queueBytes(frame);
    }

    void LegacyDeviceSession::queueBytes(
        std::span<const std::byte> bytes)
    {
        outgoing_.insert(
            outgoing_.end(),
            bytes.begin(),
            bytes.end());
    }

    bool LegacyDeviceSession::hasPendingOutput() const
    {
        return !outgoing_.empty();
    }

    bool LegacyDeviceSession::hasPartialInputFrame() const
    {
        return parser_.hasPartialFrame();
    }

    std::size_t LegacyDeviceSession::flushPendingOutput()
    {
        std::size_t totalBytesWritten = 0;

        while (!outgoing_.empty())
        {
            std::vector<std::byte> contiguous;
            contiguous.reserve(outgoing_.size());

            for (const std::byte value : outgoing_)
            {
                contiguous.push_back(value);
            }

            const std::size_t bytesWritten =
                transport_.write(contiguous);

            if (bytesWritten == 0)
            {
                break;
            }

            const std::size_t bytesToRemove =
                std::min(bytesWritten, outgoing_.size());

            if (options_.serialTrace != nullptr)
            {
                options_.serialTrace->bytes(
                    logging::ByteDumpDirection::Transmit,
                    options_.tracePortName,
                    std::span<const std::byte>{
                        contiguous.data(),
                        bytesToRemove
                    });
            }

            for (std::size_t index = 0;
                index < bytesToRemove;
                ++index)
            {
                outgoing_.pop_front();
            }

            totalBytesWritten += bytesToRemove;
        }

        return totalBytesWritten;
    }

    std::size_t LegacyDeviceSession::pollIncoming(
        std::vector<protocol::legacy::LegacyMessage>& messages)
    {
        std::vector<std::byte> readBuffer(options_.readBufferSize);
        std::size_t totalBytesRead = 0;

        for (std::size_t pass = 0;
            pass < options_.maximumReadPasses;
            ++pass)
        {
            const std::size_t bytesRead =
                transport_.read(readBuffer);

            if (bytesRead == 0)
            {
                break;
            }

            totalBytesRead += bytesRead;

            if (options_.serialTrace != nullptr)
            {
                options_.serialTrace->bytes(
                    logging::ByteDumpDirection::Receive,
                    options_.tracePortName,
                    std::span<const std::byte>{
                        readBuffer.data(),
                        bytesRead
                    });
            }

            const auto frames =
                parser_.push(
                    std::span<const std::byte>{
                        readBuffer.data(),
                        bytesRead
                    });

            for (const auto& frame : frames)
            {
                messages.push_back(
                    protocol::legacy::decodeFrame(frame));
            }
        }

        return totalBytesRead;
    }

}
