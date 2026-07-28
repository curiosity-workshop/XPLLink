#pragma once

namespace phoenix::serial
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
