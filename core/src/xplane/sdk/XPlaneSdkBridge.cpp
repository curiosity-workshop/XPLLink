#include <xpllink/xplane/sdk/XPlaneSdkBridge.h>

#include <algorithm>
#include <utility>

namespace xpllink::xplane::sdk
{
    namespace
    {
        bool supportsType(
            int xplaneType,
            int requestedType)
        {
            return (xplaneType & requestedType) != 0;
        }

        int compatibleWriteType(
            int xplaneType,
            int requestedType,
            bool hasElement)
        {
            if (supportsType(xplaneType, requestedType))
            {
                return requestedType;
            }

            if (hasElement)
            {
                if (supportsType(xplaneType, DataRefTypeIntArray))
                {
                    return DataRefTypeIntArray;
                }

                if (supportsType(xplaneType, DataRefTypeFloatArray))
                {
                    return DataRefTypeFloatArray;
                }
            }

            if (supportsType(xplaneType, DataRefTypeInt))
            {
                return DataRefTypeInt;
            }

            if (supportsType(xplaneType, DataRefTypeFloat))
            {
                return DataRefTypeFloat;
            }

            if (supportsType(xplaneType, DataRefTypeDouble))
            {
                return DataRefTypeDouble;
            }

            if (supportsType(xplaneType, DataRefTypeData))
            {
                return DataRefTypeData;
            }

            return requestedType;
        }
    }

    XPlaneSdkBridge::XPlaneSdkBridge(
        IXPlaneSdkApi& api,
        IXPlaneSdkInteractionTelemetry* telemetry)
        : api_(api)
        , telemetry_(telemetry)
    {
    }

    DataRefLookupResult XPlaneSdkBridge::findDataRef(
        std::string_view name)
    {
        const auto lookup =
            api_.findDataRef(name);

        if (lookup.handle == nullptr)
        {
            if (telemetry_ != nullptr)
            {
                telemetry_->lookupFailure(
                    "dataref",
                    name);
            }

            return {};
        }

        dataRefs_[std::string{ name }] = {
            lookup.handle,
            lookup.type
        };

        return {
            true,
            lookup.type
        };
    }

    CommandLookupResult XPlaneSdkBridge::findCommand(
        std::string_view name)
    {
        const auto lookup =
            api_.findCommand(name);

        if (lookup.handle == nullptr)
        {
            if (telemetry_ != nullptr)
            {
                telemetry_->lookupFailure(
                    "command",
                    name);
            }

            return {};
        }

        commands_[std::string{ name }] = {
            lookup.handle
        };

        return { true };
    }

    void XPlaneSdkBridge::writeDataRef(
        const DataRefWrite& write)
    {
        const auto binding =
            cachedDataRef(write.name);

        if (!binding)
        {
            return;
        }

        auto sdkWrite =
            write;
        sdkWrite.valueType =
            compatibleWriteType(
                binding->type,
                write.valueType,
                write.element.has_value());

        api_.writeDataRef(
            binding->nativeHandle,
            sdkWrite);

        if (telemetry_ != nullptr)
        {
            telemetry_->dataRefReceived(sdkWrite);
        }
    }

    DataRefReadResult XPlaneSdkBridge::readDataRef(
        const DataRefReadRequest& request)
    {
        const auto binding =
            cachedDataRef(request.name);

        if (!binding)
        {
            return {};
        }

        auto result =
            api_.readDataRef(
            binding->nativeHandle,
            request);

        if (telemetry_ != nullptr && result.found)
        {
            telemetry_->dataRefSent(
                request,
                result);
        }

        return result;
    }

    void XPlaneSdkBridge::touchDataRef(
        std::string_view name,
        int handle)
    {
        const auto binding =
            cachedDataRef(name);

        if (!binding)
        {
            return;
        }

        api_.touchDataRef(
            binding->nativeHandle,
            name,
            handle);
    }

    void XPlaneSdkBridge::commandBegin(
        std::string_view name,
        int handle)
    {
        const auto binding =
            cachedCommand(name);

        if (binding)
        {
            api_.commandBegin(binding->nativeHandle);

            if (telemetry_ != nullptr)
            {
                telemetry_->commandAction(
                    "begin",
                    name,
                    handle);
            }
        }
    }

    void XPlaneSdkBridge::commandEnd(
        std::string_view name,
        int handle)
    {
        const auto binding =
            cachedCommand(name);

        if (binding)
        {
            api_.commandEnd(binding->nativeHandle);

            if (telemetry_ != nullptr)
            {
                telemetry_->commandAction(
                    "end",
                    name,
                    handle);
            }
        }
    }

    void XPlaneSdkBridge::commandTrigger(
        std::string_view name,
        int handle,
        int triggerCount)
    {
        if (triggerCount <= 0)
        {
            return;
        }

        const auto binding =
            cachedCommand(name);

        if (!binding)
        {
            return;
        }

        pendingCommandTriggers_.push_back({
            binding->nativeHandle,
            std::string{ name },
            handle,
            triggerCount
        });

        if (telemetry_ != nullptr)
        {
                telemetry_->commandAction(
                    "trigger queued",
                    name,
                    handle,
                    triggerCount);
            }
        }

    void XPlaneSdkBridge::debugMessage(
        std::string_view message)
    {
        api_.debugMessage(message);
    }

    void XPlaneSdkBridge::speak(
        std::string_view message)
    {
        api_.speak(message);
    }

    void XPlaneSdkBridge::resetRequested()
    {
        api_.resetRequested();
    }

    std::size_t XPlaneSdkBridge::dispatchCommandTriggersForCycle()
    {
        std::size_t dispatched = 0;

        for (auto& trigger : pendingCommandTriggers_)
        {
            if (trigger.remaining <= 0 ||
                trigger.nativeHandle == nullptr)
            {
                continue;
            }

            api_.commandOnce(trigger.nativeHandle);
            if (telemetry_ != nullptr)
            {
                telemetry_->commandAction(
                    "trigger dispatched",
                    trigger.name,
                    trigger.runtimeHandle);
            }
            --trigger.remaining;
            ++dispatched;
            break;
        }

        pendingCommandTriggers_.erase(
            std::remove_if(
                pendingCommandTriggers_.begin(),
                pendingCommandTriggers_.end(),
                [](const PendingCommandTrigger& trigger)
                {
                    return trigger.remaining <= 0;
                }),
            pendingCommandTriggers_.end());

        return dispatched;
    }

    std::size_t XPlaneSdkBridge::pendingCommandTriggerCount() const
    {
        std::size_t count = 0;

        for (const auto& trigger : pendingCommandTriggers_)
        {
            if (trigger.remaining > 0)
            {
                count += static_cast<std::size_t>(trigger.remaining);
            }
        }

        return count;
    }

    std::optional<XPlaneSdkBridge::DataRefCacheEntry>
        XPlaneSdkBridge::cachedDataRef(
            std::string_view name)
    {
        const auto key =
            std::string{ name };
        const auto found =
            dataRefs_.find(key);

        if (found != dataRefs_.end())
        {
            return found->second;
        }

        const auto lookup =
            api_.findDataRef(name);

        if (lookup.handle == nullptr)
        {
            if (telemetry_ != nullptr)
            {
                telemetry_->lookupFailure(
                    "dataref",
                    name);
            }

            return std::nullopt;
        }

        auto [inserted, _] =
            dataRefs_.emplace(
                key,
                DataRefCacheEntry{
                    lookup.handle,
                    lookup.type
                });

        return inserted->second;
    }

    std::optional<XPlaneSdkBridge::CommandCacheEntry>
        XPlaneSdkBridge::cachedCommand(
            std::string_view name)
    {
        const auto key =
            std::string{ name };
        const auto found =
            commands_.find(key);

        if (found != commands_.end())
        {
            return found->second;
        }

        const auto lookup =
            api_.findCommand(name);

        if (lookup.handle == nullptr)
        {
            if (telemetry_ != nullptr)
            {
                telemetry_->lookupFailure(
                    "command",
                    name);
            }

            return std::nullopt;
        }

        auto [inserted, _] =
            commands_.emplace(
                key,
                CommandCacheEntry{
                    lookup.handle
                });

        return inserted->second;
    }

    XPlaneSdkBridgeFactory::XPlaneSdkBridgeFactory(
        IXPlaneSdkApi& api,
        IXPlaneSdkInteractionTelemetry* telemetry)
        : api_(api)
        , telemetry_(telemetry)
    {
    }

    std::unique_ptr<IXPlaneBridge>
        XPlaneSdkBridgeFactory::createBridge(
            std::string_view,
            std::string_view,
            std::string_view)
    {
        auto bridge =
            std::make_unique<XPlaneSdkBridge>(
            api_,
            telemetry_);

        bridges_.push_back(bridge.get());
        return bridge;
    }

    std::size_t XPlaneSdkBridgeFactory::dispatchCommandTriggersForCycle()
    {
        std::size_t dispatched = 0;

        for (auto* bridge : bridges_)
        {
            if (bridge != nullptr)
            {
                dispatched +=
                    bridge->dispatchCommandTriggersForCycle();
            }
        }

        return dispatched;
    }

    std::size_t XPlaneSdkBridgeFactory::pendingCommandTriggerCount() const
    {
        std::size_t count = 0;

        for (const auto* bridge : bridges_)
        {
            if (bridge != nullptr)
            {
                count += bridge->pendingCommandTriggerCount();
            }
        }

        return count;
    }

    void XPlaneSdkBridgeFactory::clearBridges()
    {
        bridges_.clear();
    }
}
