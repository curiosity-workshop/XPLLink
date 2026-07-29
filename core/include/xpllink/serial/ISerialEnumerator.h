#pragma once

#include <vector>

#include <xpllink/serial/SerialPortInfo.h>

namespace xpllink::serial
{
    class ISerialEnumerator
    {
    public:
        virtual ~ISerialEnumerator() = default;

        virtual std::vector<SerialPortInfo> enumerate() const = 0;
    };
}