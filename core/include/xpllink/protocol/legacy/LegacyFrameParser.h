#pragma once

#include <xpllink/protocol/legacy/LegacyFrame.h>

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace xpllink::protocol::legacy
{
    class LegacyFrameParser
    {
    public:
        std::vector<LegacyFrame> push(
            std::span<const std::byte> bytes);

        void reset();

        bool hasPartialFrame() const;

    private:
        void consumeBinaryPayload(
            char character,
            std::vector<LegacyFrame>& frames);

        void completeTextFrame(
            std::vector<LegacyFrame>& frames);

        std::string buffer_;
        LegacyFrame pendingBinaryFrame_;
        std::size_t pendingBinaryBytes_ = 0;
    };
}
