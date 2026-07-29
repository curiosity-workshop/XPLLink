#include <xpllink/serial/MacSerialTransportFactory.h>

#include <string>

namespace xpllink::serial
{
    std::unique_ptr<transport::IByteTransport>
        MacSerialTransportFactory::create(
            std::string_view portName,
            std::uint32_t baudRate) const
    {
        return std::make_unique<MacSerialTransport>(
            std::string{ portName },
            baudRate);
    }

    std::unique_ptr<transport::IByteTransport>
        MacSerialTransportFactory::create(
            std::string_view portName,
            std::uint32_t baudRate,
            MacSerialControlMode controlMode) const
    {
        return std::make_unique<MacSerialTransport>(
            std::string{ portName },
            baudRate,
            controlMode);
    }
}
