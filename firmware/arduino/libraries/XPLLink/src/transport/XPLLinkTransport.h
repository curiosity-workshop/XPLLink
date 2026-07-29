#pragma once

#include <Arduino.h>

namespace xpllink::transport
{
    class XPLLinkTransport
    {
    public:
        virtual ~XPLLinkTransport() = default;

        virtual int available() = 0;
        virtual int read() = 0;
        virtual size_t write(const uint8_t* data, size_t size) = 0;
    };
}
