#pragma once

#include <xpllink/runtime/LegacyDeviceController.h>
#include <xpllink/runtime/LegacyDeviceSession.h>
#include <xpllink/xplane/IXPlaneBridge.h>

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace xpllink::runtime
{
    struct LegacyUpdateSchedulerTickResult
    {
        std::size_t updatesQueued = 0;
        std::size_t bytesWritten = 0;
        bool budgetExhausted = false;
        std::string lastDataRefName;
        std::string lastDataRefValue;
        std::optional<int> lastDataRefElement;
    };

    struct LegacyUpdateSchedulerOptions
    {
        std::size_t maxFramesPerTick = 0;
        std::size_t maxBytesPerTick = 0;
    };

    class LegacyUpdateScheduler
    {
    public:
        LegacyUpdateScheduler(
            LegacyDeviceSession& session,
            LegacyDeviceController& controller,
            xplane::IXPlaneBridge& xplane,
            LegacyUpdateSchedulerOptions options = {});

        LegacyUpdateSchedulerTickResult tick(
            std::chrono::steady_clock::time_point now);

    private:
        struct SubscriptionState
        {
            int handle = -1;
            std::optional<int> requestedType;
            std::optional<int> element;
            std::chrono::steady_clock::time_point nextDue{};
            bool hasSentValue = false;
            std::string lastValue;
        };

        const LegacyDataRefBinding* findDataRef(int handle) const;
        SubscriptionState& stateFor(
            const LegacyUpdateSubscription& subscription,
            std::chrono::steady_clock::time_point now);
        bool shouldSend(
            const SubscriptionState& state,
            const std::string& valueToSend,
            bool forceUpdate) const;
        struct QueuedUpdate
        {
            std::vector<std::byte> bytes;
            std::string value;
        };

        QueuedUpdate makeUpdate(
            const xplane::DataRefReadResult& value,
            int handle,
            const std::string& valueToSend);
        bool fitsBudget(
            const LegacyUpdateSchedulerTickResult& result,
            std::size_t pendingBytes) const;

        LegacyDeviceSession& session_;
        LegacyDeviceController& controller_;
        xplane::IXPlaneBridge& xplane_;
        LegacyUpdateSchedulerOptions options_;
        std::vector<SubscriptionState> states_;
    };
}
