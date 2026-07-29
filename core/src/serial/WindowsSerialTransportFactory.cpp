#include <xpllink/serial/WindowsSerialTransportFactory.h>

#include <xpllink/serial/WindowsSerialTransport.h>

#include <string>

namespace xpllink::serial
{
    std::unique_ptr<transport::IByteTransport>
        WindowsSerialTransportFactory::create(
            std::string_view portName,
            std::uint32_t baudRate) const
    {
        return std::make_unique<WindowsSerialTransport>(
            std::string{ portName },
            baudRate);
    }

    std::unique_ptr<transport::IByteTransport>
        WindowsSerialTransportFactory::create(
            std::string_view portName,
            std::uint32_t baudRate,
            WindowsSerialControlMode controlMode) const
    {
        return std::make_unique<WindowsSerialTransport>(
            std::string{ portName },
            baudRate,
            controlMode);
    }
}
