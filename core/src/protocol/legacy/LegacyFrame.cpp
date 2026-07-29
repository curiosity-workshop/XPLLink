#include <xpllink/protocol/legacy/LegacyFrame.h>

namespace xpllink::protocol::legacy
{
    std::vector<std::byte> makeFrame(
        char command,
        std::string_view payload)
    {
        std::vector<std::byte> frame;
        frame.reserve(payload.size() + 3);

        frame.push_back(static_cast<std::byte>(packetHeader));
        frame.push_back(static_cast<std::byte>(command));

        for (const char character : payload)
        {
            frame.push_back(static_cast<std::byte>(character));
        }

        frame.push_back(static_cast<std::byte>(packetTrailer));

        return frame;
    }
}
