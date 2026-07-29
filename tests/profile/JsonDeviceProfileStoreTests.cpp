#include <xpllink/profile/JsonDeviceProfileStore.h>

#include <cassert>
#include <filesystem>

int main()
{
    const auto directory =
        std::filesystem::temp_directory_path() /
        "XPLLinkJsonDeviceProfileStoreTests";

    xpllink::profile::JsonDeviceProfileStore store{
        directory
    };

    xpllink::profile::DeviceProfile profile;
    profile.cacheSchemaVersion = 1;
    profile.deviceName = "XPLPro 737MCP";
    profile.deviceVersion = "Jul 14 2026 17:31:12";

    xpllink::profile::DeviceProfileDataRef dataRef;
    dataRef.handle = 7;
    dataRef.name = "sim/example/quoted_\"_dataref";
    dataRef.xplaneType = 6;
    dataRef.active = true;
    dataRef.scalingActive = true;
    dataRef.scaleFromLow = 0;
    dataRef.scaleFromHigh = 1024;
    dataRef.scaleToLow = -1;
    dataRef.scaleToHigh = 1;
    dataRef.updates.push_back({
        7,
        2,
        100,
        "0.0150",
        std::nullopt
    });

    profile.dataRefs.push_back(dataRef);
    profile.commands.push_back({
        3,
        "laminar/B738/autopilot/spd_interv",
        true
    });

    assert(store.save(profile));

    const auto loaded =
        store.load(store.profilePathFor(profile));

    assert(loaded);
    assert(loaded->cacheSchemaVersion == 1);
    assert(loaded->protocol == "xplpro-legacy");
    assert(loaded->deviceName == profile.deviceName);
    assert(loaded->deviceVersion == profile.deviceVersion);
    assert(loaded->dataRefs.size() == 1);
    assert(loaded->dataRefs[0].handle == 7);
    assert(loaded->dataRefs[0].name == dataRef.name);
    assert(loaded->dataRefs[0].xplaneType == 6);
    assert(loaded->dataRefs[0].active);
    assert(loaded->dataRefs[0].scalingActive);
    assert(loaded->dataRefs[0].scaleFromHigh == 1024);
    assert(loaded->dataRefs[0].scaleToLow == -1);
    assert(loaded->dataRefs[0].updates.size() == 1);
    assert(loaded->dataRefs[0].updates[0].requestedType == 2);
    assert(loaded->dataRefs[0].updates[0].rate == 100);
    assert(loaded->dataRefs[0].updates[0].precision == "0.0150");
    assert(!loaded->dataRefs[0].updates[0].element);
    assert(loaded->commands.size() == 1);
    assert(loaded->commands[0].handle == 3);
    assert(loaded->commands[0].active);

    return 0;
}
