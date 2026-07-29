#include <xpllink/discovery/LegacyXplproProbe.h>
#include <xpllink/dev/DevelopmentDeviceLoop.h>
#include <xpllink/logging/Log.h>
#include <xpllink/logging/SerialTraceLogger.h>
#include <xpllink/serial/NativeSerial.h>
#include <xpllink/serial/SerialDeviceClassifier.h>
#include <xpllink/serial/SerialDeviceKind.h>
#include <xpllink/transport/IByteTransport.h>

#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <sstream>
#include <string_view>

namespace
{
    std::string_view deviceKindName(
        xpllink::serial::SerialDeviceKind kind)
    {
        using xpllink::serial::SerialDeviceKind;

        switch (kind)
        {
        case SerialDeviceKind::ArduinoCompatible:
            return "Arduino-compatible";

        case SerialDeviceKind::UsbSerial:
            return "USB serial";

        case SerialDeviceKind::Suspect:
            return "suspect serial";

        case SerialDeviceKind::Bluetooth:
            return "Bluetooth";

        case SerialDeviceKind::BuiltInSerial:
            return "Built-in serial";

        case SerialDeviceKind::Unknown:
        default:
            return "Unknown";
        }
    }

    std::string_view controlModeName(
        xpllink::serial::NativeSerialControlMode mode)
    {
        using xpllink::serial::NativeSerialControlMode;

        switch (mode)
        {
        case NativeSerialControlMode::DtrRtsDisabled:
            return "DTR/RTS disabled";

        case NativeSerialControlMode::DtrRtsEnabled:
            return "DTR/RTS enabled";
        }

        return "Unknown";
    }

    void printPortInformation(
        const xpllink::serial::SerialPortInfo& port,
        xpllink::serial::SerialDeviceKind kind)
    {
        std::ostringstream message;

        message
            << port.portName << '\n'
            << "  Classification: "
            << deviceKindName(kind) << '\n'
            << "  Name:           "
            << port.displayName << '\n'
            << "  Manufacturer:   "
            << port.manufacturer << '\n'
            << "  Hardware ID:    "
            << port.hardwareId << "\n\n";

        xpllink::logging::info(message.str());
    }

    std::filesystem::path sourceRoot()
    {
#ifdef XPLLINK_SOURCE_DIR
        return XPLLINK_SOURCE_DIR;
#else
        return std::filesystem::current_path();
#endif
    }
}

int main()
{
    xpllink::logging::info(
        "XPLLink starting\n"
        "Enumerating serial ports...\n\n");

    xpllink::serial::NativeSerialEnumerator enumerator;
    xpllink::serial::NativeSerialTransportFactory transportFactory;

    const auto ports = enumerator.enumerate();

    {
        std::ostringstream message;

        message
        << "Serial ports found: "
        << ports.size()
        << "\n\n";

        xpllink::logging::info(message.str());
    }

    if (ports.empty())
    {
        xpllink::logging::info(
            "No serial ports were detected.\n"
            "XPLLink stopped");

        return 0;
    }

    for (const auto& port : ports)
    {
        const auto kind =
            xpllink::serial::SerialDeviceClassifier::classify(port);

        printPortInformation(port, kind);
    }

    xpllink::logging::info(
        "Beginning XPLPro device discovery...\n\n");

    xpllink::discovery::LegacyXplproProbe probe{
        std::chrono::milliseconds{ 50 },
        std::chrono::seconds{ 2 },
        std::chrono::seconds{ 3 }
    };
    const auto serialTracePath =
        sourceRoot() / "XPLLinkSerial.log";

    xpllink::logging::SerialTraceLogger serialTrace{
        serialTracePath
    };

    if (serialTrace.isOpen())
    {
        std::ostringstream message;

        message
            << "Serial trace logging enabled: "
            << serialTracePath.string()
            << "\n\n";

        xpllink::logging::info(
            message.str());
    }
    else
    {
        std::ostringstream message;

        message
            << "Unable to open "
            << serialTracePath.string()
            << " for serial tracing.\n\n";

        xpllink::logging::warning(
            message.str());
    }

    std::size_t devicesFound = 0;
    xpllink::dev::DevelopmentDeviceManager deviceManager{
        serialTrace,
        sourceRoot() / "profiles"
    };

    for (const auto& port : ports)
    {
        const auto kind =
            xpllink::serial::SerialDeviceClassifier::classify(port);

        if (kind == xpllink::serial::SerialDeviceKind::Bluetooth ||
            kind == xpllink::serial::SerialDeviceKind::Suspect ||
            kind == xpllink::serial::SerialDeviceKind::BuiltInSerial)
        {
            std::ostringstream message;

            message
                << "Skipping "
                << port.portName
                << " because it is classified as "
                << deviceKindName(kind)
                << ".\n\n";

            xpllink::logging::info(message.str());

            continue;
        }

        {
            std::ostringstream message;

            message
            << "Probing "
            << port.portName
            << "...\n"
            << "  Classification: "
            << deviceKindName(kind)
            << '\n';

            xpllink::logging::info(message.str());
        }

        std::unique_ptr<xpllink::transport::IByteTransport> transport;
        std::optional<xpllink::discovery::DiscoveredDevice> device;

        const xpllink::serial::NativeSerialControlMode controlModes[] = {
            xpllink::serial::NativeSerialControlMode::DtrRtsDisabled,
            xpllink::serial::NativeSerialControlMode::DtrRtsEnabled
        };

        for (const auto controlMode : controlModes)
        {
            std::ostringstream message;

            message
                << "  Trying "
                << controlModeName(controlMode)
                << "...\n";
            xpllink::logging::info(message.str());

            transport =
                transportFactory.create(
                    port.portName,
                    115200,
                    controlMode);

            if (!transport->open())
            {
                xpllink::logging::warning(
                    "  Unable to open port with this control mode.\n");
                continue;
            }

            xpllink::logging::info(
                "  Port opened successfully.\n"
                "  Sending XPLPro identity requests...\n");

            device =
                probe.probe(
                *transport,
                xpllink::discovery::LegacyXplproProbeTrace{
                    .serialTrace = &serialTrace,
                    .portName = port.portName
                });

            if (device)
            {
                break;
            }

            xpllink::logging::info(
                "  No valid XPLPro response received with this control mode.\n");

            transport->close();
        }

        if (device)
        {
            ++devicesFound;

            std::ostringstream message;

            message
                << "  XPLPro device discovered.\n"
                << "  Device name:    "
                << device->name << '\n'
                << "  Device version: "
                << device->version << '\n';

            xpllink::logging::info(message.str());

            deviceManager.addDevice(
                port.portName,
                device->name,
                device->version,
                std::move(transport));
        }
        else
        {
            xpllink::logging::info(
                "  No valid XPLPro response received.\n");

            transport->close();

            xpllink::logging::info("  Port closed.\n\n");
        }
    }

    deviceManager.runFor(std::chrono::seconds{ 8 });

    {
        std::ostringstream message;

        message
        << "Discovery complete.\n"
        << "XPLPro devices found: "
        << devicesFound
        << '\n'
        << "XPLLink stopped\n";

        xpllink::logging::info(message.str());
    }

    return 0;
}
