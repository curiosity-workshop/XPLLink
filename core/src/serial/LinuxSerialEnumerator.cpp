#include <xpllink/serial/LinuxSerialEnumerator.h>

#include <algorithm>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace xpllink::serial
{
    namespace
    {
        bool startsWithAny(
            const std::string& value,
            const std::vector<std::string>& prefixes)
        {
            return std::any_of(
                prefixes.begin(),
                prefixes.end(),
                [&value](const std::string& prefix)
                {
                    return value.starts_with(prefix);
                });
        }

        void addPort(
            std::vector<SerialPortInfo>& ports,
            std::set<std::string>& seen,
            const std::filesystem::path& path,
            const std::string& displayName = {})
        {
            std::error_code error;
            const std::filesystem::path resolved =
                std::filesystem::weakly_canonical(path, error);

            const std::string portName =
                error ? path.string() : resolved.string();

            if (!seen.insert(portName).second)
            {
                return;
            }

            SerialPortInfo port;
            port.portName = portName;
            port.displayName =
                displayName.empty() ?
                    path.filename().string() :
                    displayName;
            port.hardwareId =
                displayName.empty() ?
                    path.filename().string() :
                    displayName;
            ports.push_back(std::move(port));
        }

        void addDirectoryPorts(
            std::vector<SerialPortInfo>& ports,
            std::set<std::string>& seen,
            const std::filesystem::path& directory)
        {
            std::error_code error;

            if (!std::filesystem::exists(directory, error))
            {
                return;
            }

            for (const auto& entry :
                std::filesystem::directory_iterator{ directory, error })
            {
                if (error)
                {
                    return;
                }

                addPort(
                    ports,
                    seen,
                    entry.path(),
                    entry.path().filename().string());
            }
        }
    }

    std::vector<SerialPortInfo>
        LinuxSerialEnumerator::enumerate() const
    {
        std::vector<SerialPortInfo> ports;
        std::set<std::string> seen;

        addDirectoryPorts(
            ports,
            seen,
            "/dev/serial/by-id");

        const std::filesystem::path deviceDirectory{ "/dev" };
        std::error_code error;

        if (std::filesystem::exists(deviceDirectory, error))
        {
            for (const auto& entry :
                std::filesystem::directory_iterator{
                    deviceDirectory,
                    error })
            {
                if (error)
                {
                    break;
                }

                const std::string filename =
                    entry.path().filename().string();

                if (!startsWithAny(
                    filename,
                    { "ttyACM", "ttyUSB" }))
                {
                    continue;
                }

                addPort(
                    ports,
                    seen,
                    entry.path());
            }
        }

        std::sort(
            ports.begin(),
            ports.end(),
            [](const SerialPortInfo& left, const SerialPortInfo& right)
            {
                return left.portName < right.portName;
            });

        return ports;
    }
}
