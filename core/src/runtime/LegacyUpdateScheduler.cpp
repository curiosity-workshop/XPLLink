#include <xpllink/runtime/LegacyUpdateScheduler.h>

#include <xpllink/protocol/legacy/LegacyFrame.h>

#include <cmath>
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace xpllink::runtime
{
    namespace
    {
        int requestedOrBindingType(
            const LegacyUpdateSubscription& subscription,
            const LegacyDataRefBinding& binding)
        {
            if (subscription.requestedType)
            {
                return *subscription.requestedType;
            }

            if (subscription.element)
            {
                if ((binding.xplaneType & xplane::DataRefTypeIntArray) != 0)
                {
                    return xplane::DataRefTypeIntArray;
                }

                if ((binding.xplaneType & xplane::DataRefTypeFloatArray) != 0)
                {
                    return xplane::DataRefTypeFloatArray;
                }
            }

            if ((binding.xplaneType & xplane::DataRefTypeInt) != 0)
            {
                return xplane::DataRefTypeInt;
            }

            if ((binding.xplaneType & xplane::DataRefTypeFloat) != 0)
            {
                return xplane::DataRefTypeFloat;
            }

            if ((binding.xplaneType & xplane::DataRefTypeData) != 0)
            {
                return xplane::DataRefTypeData;
            }

            return binding.xplaneType;
        }

        std::chrono::milliseconds subscriptionRate(
            const LegacyUpdateSubscription& subscription)
        {
            if (subscription.rate <= 0)
            {
                return std::chrono::milliseconds{ 1 };
            }

            return std::chrono::milliseconds{ subscription.rate };
        }

        std::optional<double> parseDouble(std::string_view value)
        {
            try
            {
                std::size_t processed = 0;
                const double parsed =
                    std::stod(std::string{ value }, &processed);

                if (processed == 0)
                {
                    return std::nullopt;
                }

                return parsed;
            }
            catch (...)
            {
                return std::nullopt;
            }
        }

        std::string formatDouble(double value)
        {
            std::ostringstream stream;
            stream
                << std::fixed
                << std::setprecision(6)
                << value;

            auto result =
                stream.str();

            while (result.find('.') != std::string::npos &&
                !result.empty() &&
                result.back() == '0')
            {
                result.pop_back();
            }

            if (!result.empty() &&
                result.back() == '.')
            {
                result.pop_back();
            }

            if (result == "-0")
            {
                return "0";
            }

            return result;
        }

        std::string bucketedValue(
            const LegacyUpdateSubscription& subscription,
            const std::string& value)
        {
            const auto precision =
                parseDouble(subscription.precision);

            if (!precision || *precision <= 0.0)
            {
                return value;
            }

            const auto numeric =
                parseDouble(value);

            if (!numeric)
            {
                return value;
            }

            return formatDouble(
                std::round(*numeric / *precision) * *precision);
        }

        char commandForValueType(int valueType)
        {
            switch (valueType)
            {
            case xplane::DataRefTypeInt:
                return protocol::legacy::dataRefUpdateIntCommand;

            case xplane::DataRefTypeFloat:
            case xplane::DataRefTypeDouble:
                return protocol::legacy::dataRefUpdateFloatCommand;

            case xplane::DataRefTypeIntArray:
                return protocol::legacy::dataRefUpdateIntArrayCommand;

            case xplane::DataRefTypeFloatArray:
                return protocol::legacy::dataRefUpdateFloatArrayCommand;

            case xplane::DataRefTypeData:
                return protocol::legacy::dataRefUpdateStringCommand;

            default:
                return '\0';
            }
        }

        std::vector<std::byte> bytesFromString(std::string_view value)
        {
            std::vector<std::byte> bytes;
            bytes.reserve(value.size());

            for (char character : value)
            {
                bytes.push_back(
                    static_cast<std::byte>(character));
            }

            return bytes;
        }
    }

    LegacyUpdateScheduler::LegacyUpdateScheduler(
        LegacyDeviceSession& session,
        LegacyDeviceController& controller,
        xplane::IXPlaneBridge& xplane,
        LegacyUpdateSchedulerOptions options)
        : session_(session),
        controller_(controller),
        xplane_(xplane),
        options_(options)
    {
    }

    LegacyUpdateSchedulerTickResult LegacyUpdateScheduler::tick(
        std::chrono::steady_clock::time_point now)
    {
        LegacyUpdateSchedulerTickResult result;

        if (controller_.dataFlowPaused())
        {
            return result;
        }

        for (auto& subscription :
            controller_.updateSubscriptions())
        {
            auto& state =
                stateFor(subscription, now);

            if (!subscription.forceUpdate &&
                state.hasSentValue &&
                now < state.nextDue)
            {
                continue;
            }

            const auto* binding =
                findDataRef(subscription.handle);

            if (binding == nullptr || !binding->active)
            {
                continue;
            }

            const auto value =
                xplane_.readDataRef({
                    binding->name,
                    binding->handle,
                    binding->xplaneType,
                    requestedOrBindingType(subscription, *binding),
                    subscription.element
                });

            const auto valueToSend =
                bucketedValue(subscription, value.value);

            if (!value.found ||
                !shouldSend(
                    state,
                    valueToSend,
                    subscription.forceUpdate))
            {
                subscription.forceUpdate = false;
                state.nextDue =
                    now + subscriptionRate(subscription);
                continue;
            }

            const auto update =
                makeUpdate(value, subscription.handle, valueToSend);

            if (update.bytes.empty())
            {
                continue;
            }

            if (!fitsBudget(result, update.bytes.size()))
            {
                result.budgetExhausted = true;
                break;
            }

            session_.queueBytes(update.bytes);
            subscription.forceUpdate = false;
            state.lastValue = update.value;
            state.hasSentValue = true;
            state.nextDue =
                now + subscriptionRate(subscription);
            result.lastDataRefName =
                binding->name;
            result.lastDataRefValue =
                update.value;
            result.lastDataRefElement =
                value.element;
            result.bytesWritten += update.bytes.size();
            ++result.updatesQueued;
        }

        if (result.updatesQueued > 0)
        {
            result.bytesWritten =
                session_.flushPendingOutput();
        }

        return result;
    }

    const LegacyDataRefBinding* LegacyUpdateScheduler::findDataRef(
        int handle) const
    {
        const auto& bindings =
            controller_.dataRefs();

        if (handle < 0 ||
            static_cast<std::size_t>(handle) >= bindings.size())
        {
            return nullptr;
        }

        return &bindings[static_cast<std::size_t>(handle)];
    }

    LegacyUpdateScheduler::SubscriptionState&
        LegacyUpdateScheduler::stateFor(
            const LegacyUpdateSubscription& subscription,
            std::chrono::steady_clock::time_point now)
    {
        for (auto& state : states_)
        {
            if (state.handle == subscription.handle &&
                state.requestedType == subscription.requestedType &&
                state.element == subscription.element)
            {
                return state;
            }
        }

        states_.push_back({
            subscription.handle,
            subscription.requestedType,
            subscription.element,
            now
        });

        return states_.back();
    }

    bool LegacyUpdateScheduler::shouldSend(
        const SubscriptionState& state,
        const std::string& valueToSend,
        bool forceUpdate) const
    {
        if (forceUpdate)
        {
            return true;
        }

        if (!state.hasSentValue)
        {
            return true;
        }

        return valueToSend != state.lastValue;
    }

    LegacyUpdateScheduler::QueuedUpdate LegacyUpdateScheduler::makeUpdate(
        const xplane::DataRefReadResult& value,
        int handle,
        const std::string& valueToSend)
    {
        const char command =
            commandForValueType(value.valueType);

        if (command == '\0')
        {
            return {};
        }

        std::ostringstream payload;
        payload << ',' << handle;

        if (command == protocol::legacy::dataRefUpdateStringCommand)
        {
            payload << ',' << value.value.size();
            auto bytes =
                protocol::legacy::makeFrame(command, payload.str());

            auto stringBytes = bytesFromString(value.value);
            bytes.insert(
                bytes.end(),
                stringBytes.begin(),
                stringBytes.end());

            return {
                std::move(bytes),
                value.value
            };
        }

        payload << ',' << valueToSend;

        if (value.element)
        {
            payload << ',' << *value.element;
        }

        return {
            protocol::legacy::makeFrame(command, payload.str()),
            valueToSend
        };
    }

    bool LegacyUpdateScheduler::fitsBudget(
        const LegacyUpdateSchedulerTickResult& result,
        std::size_t pendingBytes) const
    {
        if (result.updatesQueued == 0)
        {
            return true;
        }

        if (options_.maxFramesPerTick > 0 &&
            result.updatesQueued >= options_.maxFramesPerTick)
        {
            return false;
        }

        if (options_.maxBytesPerTick > 0 &&
            result.bytesWritten + pendingBytes > options_.maxBytesPerTick)
        {
            return false;
        }

        return true;
    }
}
