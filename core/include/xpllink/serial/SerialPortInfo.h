#pragma once

#include <string>
#include <xpllink/serial/SerialDeviceKind.h>

namespace xpllink::serial
{
    struct SerialPortInfo
    {
        std::string portName;
        std::string displayName;
        std::string manufacturer;
        std::string hardwareId;

        SerialDeviceKind kind = SerialDeviceKind::Unknown;
    };
}