#pragma once

#include <xpllink/serial/ISerialEnumerator.h>

namespace xpllink::serial
{
    class LinuxSerialEnumerator final : public ISerialEnumerator
    {
    public:
        std::vector<SerialPortInfo> enumerate() const override;
    };
}
