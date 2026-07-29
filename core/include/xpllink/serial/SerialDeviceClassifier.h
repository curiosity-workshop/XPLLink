#pragma once

#include <xpllink/serial/SerialDeviceKind.h>
#include <xpllink/serial/SerialPortInfo.h>

namespace xpllink::serial
{
    class SerialDeviceClassifier
    {
    public:
        static SerialDeviceKind classify(
            const SerialPortInfo& port);
    };
}