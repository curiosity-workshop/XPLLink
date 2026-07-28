#include <phoenix/serial/SerialDeviceClassifier.h>

#include <algorithm>
#include <cctype>
#include <string>

namespace phoenix::serial
{
    namespace
    {
        std::string toUpper(std::string value)
        {
            std::transform(
                value.begin(),
                value.end(),
                value.begin(),
                [](unsigned char character)
                {
                    return static_cast<char>(
                        std::toupper(character));
                });

            return value;
        }

        bool contains(
            const std::string& text,
            const std::string& value)
        {
            return text.find(value) != std::string::npos;
        }
    }

    SerialDeviceKind SerialDeviceClassifier::classify(
        const SerialPortInfo& port)
    {
        const std::string hardwareId =
            toUpper(port.hardwareId);

        const std::string displayName =
            toUpper(port.displayName);

        const std::string manufacturer =
            toUpper(port.manufacturer);

        if (contains(hardwareId, "BTHENUM\\") ||
            contains(displayName, "BLUETOOTH"))
        {
            return SerialDeviceKind::Bluetooth;
        }

        if (contains(hardwareId, "ACPI\\VEN_PNP&DEV_0501") ||
            contains(displayName, "COMMUNICATIONS PORT"))
        {
            return SerialDeviceKind::BuiltInSerial;
        }

        /*
         * Some simulator controls expose a broken/generic USB serial
         * interface with no real vendor/product identity. Treat those as
         * suspect instead of spending probe time on them.
         */
        if (contains(hardwareId, "VID_0000") &&
            contains(hardwareId, "PID_0000"))
        {
            return SerialDeviceKind::Suspect;
        }

        /*
         * Official Arduino USB vendor ID.
         *
         * This identifies likely Arduino hardware, not necessarily
         * firmware running XPLPro.
         */
        if (contains(hardwareId, "VID_2341"))
        {
            return SerialDeviceKind::ArduinoCompatible;
        }

        /*
         * Common Arduino-compatible USB-to-serial interfaces.
         * These can be expanded as real devices are encountered.
         */
        if (contains(hardwareId, "VID_2A03") || // Arduino.org
            contains(hardwareId, "VID_1A86") || // CH340
            contains(hardwareId, "VID_10C4") || // CP210x
            contains(hardwareId, "VID_0403") || // FTDI
            contains(hardwareId, "USBMODEM") ||
            contains(hardwareId, "USBSERIAL") ||
            contains(hardwareId, "WCHUSB") ||
            contains(hardwareId, "SLAB_USBTOUART"))
        {
            return SerialDeviceKind::ArduinoCompatible;
        }

        if (contains(hardwareId, "USB\\") ||
            contains(displayName, "USB SERIAL") ||
            contains(displayName, "USBSERIAL") ||
            contains(manufacturer, "FTDI") ||
            contains(manufacturer, "SILICON LABS"))
        {
            return SerialDeviceKind::UsbSerial;
        }

        return SerialDeviceKind::Unknown;
    }
}
