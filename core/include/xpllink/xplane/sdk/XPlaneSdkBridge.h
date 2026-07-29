#pragma once

#include <xpllink/runtime/DeviceRuntimeManager.h>
#include <xpllink/xplane/IXPlaneBridge.h>

#include <cstddef>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace xpllink::xplane::sdk
{
    using NativeDataRefHandle = void*;
    using NativeCommandHandle = void*;

    struct NativeDataRefLookup
    {
        NativeDataRefHandle handle = nullptr;
        int type = DataRefTypeUnknown;
    };

    struct NativeCommandLookup
    {
        NativeCommandHandle handle = nullptr;
    };

    class IXPlaneSdkApi
    {
    public:
        virtual ~IXPlaneSdkApi() = default;

        virtual NativeDataRefLookup findDataRef(
            std::string_view name) = 0;

        virtual NativeCommandLookup findCommand(
            std::string_view name) = 0;

        virtual void writeDataRef(
            NativeDataRefHandle handle,
            const DataRefWrite& write) = 0;

        virtual DataRefReadResult readDataRef(
            NativeDataRefHandle handle,
            const DataRefReadRequest& request) = 0;

        virtual void touchDataRef(
            NativeDataRefHandle handle,
            std::string_view name,
            int runtimeHandle) = 0;

        virtual void commandBegin(
            NativeCommandHandle handle) = 0;

        virtual void commandEnd(
            NativeCommandHandle handle) = 0;

        virtual void commandOnce(
            NativeCommandHandle handle) = 0;

        virtual void debugMessage(
            std::string_view message) = 0;

        virtual void speak(
            std::string_view message) = 0;

        virtual void resetRequested() = 0;
    };

    struct XPlaneSdkInteractionTelemetrySnapshot
    {
        std::string lastDataRefReceived;
        std::string lastDataRefSent;
        std::string lastCommandAction;
        std::string lastLookupFailure;
    };

    class IXPlaneSdkInteractionTelemetry
    {
    public:
        virtual ~IXPlaneSdkInteractionTelemetry() = default;

        virtual void dataRefReceived(
            const DataRefWrite& write) = 0;

        virtual void dataRefSent(
            const DataRefReadRequest& request,
            const DataRefReadResult& result) = 0;

        virtual void commandAction(
            std::string_view action,
            std::string_view name,
            int handle,
            int count = 1) = 0;

        virtual void lookupFailure(
            std::string_view kind,
            std::string_view name) = 0;
    };

    class XPlaneSdkBridge final
        : public IXPlaneBridge
    {
    public:
        explicit XPlaneSdkBridge(
            IXPlaneSdkApi& api,
            IXPlaneSdkInteractionTelemetry* telemetry = nullptr);

        DataRefLookupResult findDataRef(
            std::string_view name) override;

        CommandLookupResult findCommand(
            std::string_view name) override;

        void writeDataRef(
            const DataRefWrite& write) override;

        DataRefReadResult readDataRef(
            const DataRefReadRequest& request) override;

        void touchDataRef(
            std::string_view name,
            int handle) override;

        void commandBegin(
            std::string_view name,
            int handle) override;

        void commandEnd(
            std::string_view name,
            int handle) override;

        void commandTrigger(
            std::string_view name,
            int handle,
            int triggerCount) override;

        void debugMessage(
            std::string_view message) override;

        void speak(
            std::string_view message) override;

        void resetRequested() override;

        std::size_t dispatchCommandTriggersForCycle();
        std::size_t pendingCommandTriggerCount() const;

    private:
        struct DataRefCacheEntry
        {
            NativeDataRefHandle nativeHandle = nullptr;
            int type = DataRefTypeUnknown;
        };

        struct CommandCacheEntry
        {
            NativeCommandHandle nativeHandle = nullptr;
        };

        struct PendingCommandTrigger
        {
            NativeCommandHandle nativeHandle = nullptr;
            std::string name;
            int runtimeHandle = -1;
            int remaining = 0;
        };

        std::optional<DataRefCacheEntry> cachedDataRef(
            std::string_view name);
        std::optional<CommandCacheEntry> cachedCommand(
            std::string_view name);

        IXPlaneSdkApi& api_;
        IXPlaneSdkInteractionTelemetry* telemetry_ = nullptr;
        std::unordered_map<std::string, DataRefCacheEntry> dataRefs_;
        std::unordered_map<std::string, CommandCacheEntry> commands_;
        std::deque<PendingCommandTrigger> pendingCommandTriggers_;
    };

    class XPlaneSdkBridgeFactory final
        : public runtime::IXPlaneBridgeFactory
    {
    public:
        explicit XPlaneSdkBridgeFactory(
            IXPlaneSdkApi& api,
            IXPlaneSdkInteractionTelemetry* telemetry = nullptr);

        std::unique_ptr<IXPlaneBridge> createBridge(
            std::string_view portName,
            std::string_view deviceName,
            std::string_view deviceVersion) override;

        std::size_t dispatchCommandTriggersForCycle();
        std::size_t pendingCommandTriggerCount() const;
        void clearBridges();

    private:
        IXPlaneSdkApi& api_;
        IXPlaneSdkInteractionTelemetry* telemetry_ = nullptr;
        std::vector<XPlaneSdkBridge*> bridges_;
    };
}
