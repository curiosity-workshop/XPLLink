#include <xpllink/logging/Log.h>

#include <iomanip>
#include <iostream>

namespace xpllink::logging
{
    namespace
    {
        void write(
            std::ostream& stream,
            std::string_view message)
        {
            stream
                << message
                << std::flush;
        }
    }

    void info(std::string_view message)
    {
        write(std::cout, message);
    }

    void warning(std::string_view message)
    {
        write(std::cout, message);
    }

    void error(std::string_view message)
    {
        write(std::cerr, message);
    }

    void debug(std::string_view message)
    {
        write(std::cout, message);
    }

    void bytes(
        ByteDumpDirection direction,
        std::span<const std::byte> data)
    {
        std::ostream& stream = std::cout;

        stream
            << "[DEBUG] "
            << (direction == ByteDumpDirection::Transmit ? "TX" : "RX")
            << " "
            << data.size()
            << " bytes:";

        for (const std::byte value : data)
        {
            stream
                << ' '
                << std::uppercase
                << std::hex
                << std::setw(2)
                << std::setfill('0')
                << std::to_integer<int>(value);
        }

        stream
            << std::dec
            << std::nouppercase
            << std::setfill(' ')
            << '\n'
            << std::flush;
    }
}
