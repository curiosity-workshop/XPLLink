#pragma once

#include <cstddef>
#include <span>
#include <string_view>

namespace xpllink::logging
{
    enum class ByteDumpDirection
    {
        Transmit,
        Receive
    };

    void info(std::string_view message);
    void warning(std::string_view message);
    void error(std::string_view message);
    void debug(std::string_view message);

    void bytes(
        ByteDumpDirection direction,
        std::span<const std::byte> data);
}
