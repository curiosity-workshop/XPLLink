#pragma once

#include <xpllink/xplane/sdk/XPlaneSdkBridge.h>

namespace xpllink::xplane::sdk
{
    class XPlaneSdkApi final
        : public IXPlaneSdkApi
    {
    public:
        NativeDataRefLookup findDataRef(
            std::string_view name) override;

        NativeCommandLookup findCommand(
            std::string_view name) override;

        void writeDataRef(
            NativeDataRefHandle handle,
            const DataRefWrite& write) override;

        DataRefReadResult readDataRef(
            NativeDataRefHandle handle,
            const DataRefReadRequest& request) override;

        void touchDataRef(
            NativeDataRefHandle handle,
            std::string_view name,
            int runtimeHandle) override;

        void commandBegin(
            NativeCommandHandle handle) override;

        void commandEnd(
            NativeCommandHandle handle) override;

        void commandOnce(
            NativeCommandHandle handle) override;

        void debugMessage(
            std::string_view message) override;

        void speak(
            std::string_view message) override;

        void resetRequested() override;
    };
}
