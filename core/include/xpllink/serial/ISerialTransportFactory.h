#pragma once

#include <xpllink/transport/IByteTransport.h>

#include <cstdint>
#include <memory>
#include <string_view>

namespace xpllink::serial
{
    class ISerialTransportFactory
    {
    public:
        virtual ~ISerialTransportFactory() = default;

        virtual std::unique_ptr<transport::IByteTransport> create(
            std::string_view portName,
            std::uint32_t baudRate) const = 0;
    };
}
