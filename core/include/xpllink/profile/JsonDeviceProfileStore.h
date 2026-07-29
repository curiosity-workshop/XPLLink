#pragma once

#include <xpllink/profile/DeviceProfile.h>

#include <filesystem>
#include <optional>

namespace xpllink::profile
{
    class JsonDeviceProfileStore
    {
    public:
        explicit JsonDeviceProfileStore(
            std::filesystem::path profileDirectory);

        const std::filesystem::path& profileDirectory() const;

        std::filesystem::path profilePathFor(
            const DeviceProfile& profile) const;

        bool save(
            const DeviceProfile& profile) const;

        std::optional<DeviceProfile> load(
            const std::filesystem::path& path) const;

    private:
        std::filesystem::path profileDirectory_;
    };
}
