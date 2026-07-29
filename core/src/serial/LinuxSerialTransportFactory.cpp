#include <xpllink/serial/LinuxSerialTransportFactory.h>

#include <string>

namespace xpllink::serial
{
    std::unique_ptr<transport::IByteTransport>
        LinuxSerialTransportFactory::create(
            std::string_view portName,
            std::uint32_t baudRate) const
    {
        return std::make_unique<LinuxSerialTransport>(
            std::string{ portName },
            baudRate);
    }

    std::unique_ptr<transport::IByteTransport>
        LinuxSerialTransportFactory::create(
            std::string_view portName,
            std::uint32_t baudRate,
            LinuxSerialControlMode controlMode) const
    {
        return std::make_unique<LinuxSerialTransport>(
            std::string{ portName },
            baudRate,
            controlMode);
    }
}
