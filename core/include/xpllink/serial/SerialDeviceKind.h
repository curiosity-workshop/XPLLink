#pragma once

namespace xpllink::serial
{
    enum class SerialDeviceKind
    {
        Unknown,
        ArduinoCompatible,
        UsbSerial,
        Suspect,
        Bluetooth,
        BuiltInSerial
    };
}
