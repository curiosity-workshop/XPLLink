#pragma once

#include <xpllink/serial/ISerialTransportFactory.h>
#include <xpllink/serial/WindowsSerialTransport.h>

namespace xpllink::serial
{
    class WindowsSerialTransportFactory final
        : public ISerialTransportFactory
    {
    public:
        std::unique_ptr<transport::IByteTransport> create(
            std::string_view portName,
            std::uint32_t baudRate) const override;

        std::unique_ptr<transport::IByteTransport> create(
            std::string_view portName,
            std::uint32_t baudRate,
            WindowsSerialControlMode controlMode) const;
    };
}
