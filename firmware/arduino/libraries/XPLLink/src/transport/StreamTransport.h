#pragma once

#include "XPLLinkTransport.h"

namespace xpllink::transport
{
    class StreamTransport final : public XPLLinkTransport
    {
    public:
        explicit StreamTransport(Stream& stream);

        int available() override;
        int read() override;
        size_t write(const uint8_t* data, size_t size) override;

    private:
        Stream& stream_;
    };
}
