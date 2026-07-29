#include <xpllink/logging/SerialTraceLogger.h>

#include <cctype>
#include <iomanip>

namespace xpllink::logging
{
    SerialTraceLogger::SerialTraceLogger(
        const std::filesystem::path& path)
        : stream_(path, std::ios::out | std::ios::trunc)
    {
        if (stream_.is_open())
        {
            stream_
                << "XPLLink serial logger. Unprintable characters are "
                << "represented by hex value (0xXX).\n"
                << std::flush;

            stream_ << std::unitbuf;
        }
    }

    bool SerialTraceLogger::isOpen() const
    {
        return stream_.is_open();
    }

    void SerialTraceLogger::bytes(
        ByteDumpDirection direction,
        std::string_view portName,
        std::span<const std::byte> data)
    {
        if (!stream_.is_open())
        {
            return;
        }

        stream_
            << (direction == ByteDumpDirection::Transmit ? "tx" : "rx")
            << " port: "
            << portName
            << " length: "
            << std::setw(3)
            << std::setfill('0')
            << data.size()
            << " packet: ";

        for (const std::byte value : data)
        {
            const auto character =
                static_cast<unsigned char>(
                    std::to_integer<unsigned char>(value));

            if (std::isprint(character))
            {
                stream_ << static_cast<char>(character);
            }
            else
            {
                stream_
                    << "(0x"
                    << std::uppercase
                    << std::hex
                    << std::setw(2)
                    << std::setfill('0')
                    << static_cast<int>(character)
                    << std::dec
                    << std::nouppercase
                    << ')';
            }
        }

        stream_
            << std::setfill(' ')
            << '\n';
    }
}
