#include <xpllink/protocol/legacy/LegacyFrameParser.h>

#include <algorithm>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>

namespace xpllink::protocol::legacy
{
    namespace
    {
        std::size_t expectedBinaryBytes(
            std::string_view payload)
        {
            if (payload.starts_with(','))
            {
                payload.remove_prefix(1);
            }

            const auto comma =
                payload.find(',');

            if (comma == std::string_view::npos)
            {
                return 0;
            }

            const std::string count{
                payload.substr(comma + 1)
            };

            return static_cast<std::size_t>(
                std::max(0, std::atoi(count.c_str())));
        }
    }

    std::vector<LegacyFrame> LegacyFrameParser::push(
        std::span<const std::byte> bytes)
    {
        std::vector<LegacyFrame> frames;

        for (const std::byte value : bytes)
        {
            const char character =
                static_cast<char>(
                    std::to_integer<unsigned char>(value));

            if (pendingBinaryBytes_ > 0)
            {
                consumeBinaryPayload(character, frames);
                continue;
            }

            if (buffer_.empty())
            {
                if (character == packetHeader)
                {
                    buffer_.push_back(character);
                }

                continue;
            }

            if (character == packetHeader)
            {
                buffer_.clear();
                buffer_.push_back(character);
                continue;
            }

            buffer_.push_back(character);

            if (buffer_.size() > maximumFrameSize)
            {
                buffer_.clear();
                continue;
            }

            if (character != packetTrailer)
            {
                continue;
            }

            completeTextFrame(frames);
        }

        return frames;
    }

    void LegacyFrameParser::reset()
    {
        buffer_.clear();
        pendingBinaryFrame_ = {};
        pendingBinaryBytes_ = 0;
    }

    bool LegacyFrameParser::hasPartialFrame() const
    {
        return !buffer_.empty() ||
            pendingBinaryBytes_ > 0;
    }

    void LegacyFrameParser::consumeBinaryPayload(
        char character,
        std::vector<LegacyFrame>& frames)
    {
        pendingBinaryFrame_.binaryPayload.push_back(character);

        --pendingBinaryBytes_;

        if (pendingBinaryBytes_ == 0)
        {
            pendingBinaryFrame_.raw +=
                pendingBinaryFrame_.binaryPayload;
            frames.push_back(
                std::move(pendingBinaryFrame_));
            pendingBinaryFrame_ = {};
        }
    }

    void LegacyFrameParser::completeTextFrame(
        std::vector<LegacyFrame>& frames)
    {
        if (buffer_.size() < 3)
        {
            buffer_.clear();
            return;
        }

        LegacyFrame frame{
            buffer_[1],
            buffer_.substr(2, buffer_.size() - 3),
            {},
            buffer_
        };

        buffer_.clear();

        if (frame.command != dataRefUpdateStringCommand)
        {
            frames.push_back(std::move(frame));
            return;
        }

        pendingBinaryBytes_ =
            expectedBinaryBytes(frame.payload);

        if (pendingBinaryBytes_ == 0)
        {
            frames.push_back(std::move(frame));
            return;
        }

        pendingBinaryFrame_ =
            std::move(frame);
    }
}
