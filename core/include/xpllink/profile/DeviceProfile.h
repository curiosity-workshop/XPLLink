#pragma once

#include <optional>
#include <string>
#include <vector>

namespace xpllink::profile
{
    struct DeviceProfileUpdateSubscription
    {
        int handle = -1;
        std::optional<int> requestedType;
        int rate = 0;
        std::string precision;
        std::optional<int> element;
    };

    struct DeviceProfileDataRef
    {
        int handle = -1;
        std::string name;
        int xplaneType = 0;
        bool active = false;

        bool scalingActive = false;
        long scaleFromLow = 0;
        long scaleFromHigh = 0;
        long scaleToLow = 0;
        long scaleToHigh = 0;

        std::vector<DeviceProfileUpdateSubscription> updates;
    };

    struct DeviceProfileCommand
    {
        int handle = -1;
        std::string name;
        bool active = false;
    };

    struct DeviceProfile
    {
        int cacheSchemaVersion = 0;
        std::string protocol = "xplpro-legacy";
        std::string deviceName;
        std::string deviceVersion;
        std::vector<DeviceProfileDataRef> dataRefs;
        std::vector<DeviceProfileCommand> commands;
    };
}
