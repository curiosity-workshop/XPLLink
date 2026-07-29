#include <xpllink/serial/MacSerialEnumerator.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace xpllink::serial
{
    namespace
    {
        bool isSerialDevicePath(
            const std::string& filename)
        {
            return filename.starts_with("cu.") ||
                filename.starts_with("tty.");
        }

        bool isPreferredDevicePath(
            const std::string& filename)
        {
            return filename.starts_with("cu.");
        }
    }

    std::vector<SerialPortInfo>
        MacSerialEnumerator::enumerate() const
    {
        std::vector<SerialPortInfo> ports;
        const std::filesystem::path deviceDirectory{ "/dev" };

        if (!std::filesystem::exists(deviceDirectory))
        {
            return ports;
        }

        for (const auto& entry :
            std::filesystem::directory_iterator{ deviceDirectory })
        {
            const std::string filename =
                entry.path().filename().string();

            if (!isSerialDevicePath(filename))
            {
                continue;
            }

            SerialPortInfo port;
            port.portName = entry.path().string();
            port.displayName = filename;
            port.hardwareId = filename;
            ports.push_back(std::move(port));
        }

        std::sort(
            ports.begin(),
            ports.end(),
            [](const SerialPortInfo& left, const SerialPortInfo& right)
            {
                const bool leftPreferred =
                    isPreferredDevicePath(
                        std::filesystem::path{
                            left.portName }.filename().string());
                const bool rightPreferred =
                    isPreferredDevicePath(
                        std::filesystem::path{
                            right.portName }.filename().string());

                if (leftPreferred != rightPreferred)
                {
                    return leftPreferred;
                }

                return left.portName < right.portName;
            });

        return ports;
    }
}
