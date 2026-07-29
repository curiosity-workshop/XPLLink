#pragma once

#include <string>

namespace xpllink::discovery
{
    struct DiscoveredDevice
    {
        std::string name;
        std::string version;
    };
}