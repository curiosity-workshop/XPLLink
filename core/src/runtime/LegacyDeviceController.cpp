#include <phoenix/runtime/LegacyDeviceController.h>

#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <variant>

namespace phoenix::runtime
{
    namespace
    {
        int toXPlaneType(
            protocol::legacy::DataRefValueType type)
        {
            using protocol::legacy::DataRefValueType;

            switch (type)
            {
            case DataRefValueType::Int:
                return xplane::DataRefTypeInt;

            case DataRefValueType::Float:
                return xplane::DataRefTypeFloat;

            case DataRefValueType::IntArray:
                return xplane::DataRefTypeIntArray;

            case DataRefValueType::FloatArray:
                return xplane::DataRefTypeFloatArray;

            case DataRefValueType::String:
                return xplane::DataRefTypeData;

            default:
                return xplane::DataRefTypeUnknown;
            }
        }

        std::string responsePayload(int handle)
        {
            std::ostringstream payload;
            payload << ',' << handle;
            return payload.str();
        }

        std::string failedLookupPayload(std::string_view name)
        {
            std::ostringstream payload;
            payload << ",-1,\"" << name << '"';
            return payload.str();
        }

        std::string formatScaledValue(double value)
        {
            std::ostringstream output;
            output
                << std::fixed
                << std::setprecision(6)
                << value;

            auto result =
                output.str();

            while (result.size() > 1 &&
                result.back() == '0')
            {
                result.pop_back();
            }

            if (!result.empty() &&
                result.back() == '.')
            {
                result.pop_back();
            }

            return result;
        }

        std::string applyScaling(
            const LegacyDataRefBinding& binding,
            std::string_view value)
        {
            if (!binding.scalingActive ||
                binding.scaleFromHigh == binding.scaleFromLow)
            {
                return std::string{ value };
            }

            try
            {
                const double input =
                    std::stod(std::string{ value });
                const double normalized =
                    (input -
                        static_cast<double>(binding.scaleFromLow)) /
                    (static_cast<double>(binding.scaleFromHigh) -
                        static_cast<double>(binding.scaleFromLow));
                const double scaled =
                    static_cast<double>(binding.scaleToLow) +
                    normalized *
                    (static_cast<double>(binding.scaleToHigh) -
                        static_cast<double>(binding.scaleToLow));

                return formatScaledValue(scaled);
            }
            catch (const std::exception&)
            {
                return std::string{ value };
            }
        }
    }

    LegacyDeviceController::LegacyDeviceController(
        LegacyDeviceSession& session,
        xplane::IXPlaneBridge& xplane)
        : session_(session),
        xplane_(xplane)
    {
    }

    LegacyDeviceController::LegacyDeviceController(
        LegacyDeviceSession& session,
        xplane::IXPlaneBridge& xplane,
        ILegacyDeviceObserver& observer)
        : session_(session),
        xplane_(xplane),
        observer_(&observer)
    {
    }

    LegacyDeviceControllerTickResult LegacyDeviceController::tick()
    {
        LegacyDeviceControllerTickResult result;
        result.session = session_.tick();

        for (const auto& message : result.session.messages)
        {
            processMessage(message);
            ++result.messagesProcessed;
        }

        result.responseBytesWritten =
            session_.flushPendingOutput();

        return result;
    }

    void LegacyDeviceController::requestRegistrations()
    {
        session_.queueFrame(
            protocol::legacy::sendRequestCommand);

        registrationsRequested_ = true;
    }

    const std::vector<LegacyDataRefBinding>&
        LegacyDeviceController::dataRefs() const
    {
        return dataRefs_;
    }

    const std::vector<LegacyCommandBinding>&
        LegacyDeviceController::commands() const
    {
        return commands_;
    }

    const std::vector<LegacyUpdateSubscription>&
        LegacyDeviceController::updateSubscriptions() const
    {
        return updateSubscriptions_;
    }

    std::vector<LegacyUpdateSubscription>&
        LegacyDeviceController::updateSubscriptions()
    {
        return updateSubscriptions_;
    }

    bool LegacyDeviceController::registrationsRequested() const
    {
        return registrationsRequested_;
    }

    bool LegacyDeviceController::dataFlowPaused() const
    {
        return dataFlowPaused_;
    }

    long LegacyDeviceController::maxBytesPerSecond() const
    {
        return maxBytesPerSecond_;
    }

    bool LegacyDeviceController::tryLoadProfile(
        const profile::DeviceProfile& profile)
    {
        std::vector<LegacyDataRefBinding> dataRefs;
        std::vector<LegacyCommandBinding> commands;
        std::vector<LegacyUpdateSubscription> updateSubscriptions;

        for (const auto& profileDataRef : profile.dataRefs)
        {
            if (profileDataRef.handle !=
                static_cast<int>(dataRefs.size()))
            {
                return false;
            }

            const auto lookup =
                xplane_.findDataRef(profileDataRef.name);

            if (!lookup.found)
            {
                return false;
            }

            dataRefs.push_back({
                profileDataRef.handle,
                profileDataRef.name,
                lookup.type,
                profileDataRef.active,
                profileDataRef.scalingActive,
                profileDataRef.scaleFromLow,
                profileDataRef.scaleFromHigh,
                profileDataRef.scaleToLow,
                profileDataRef.scaleToHigh
            });

            for (const auto& profileUpdate :
                profileDataRef.updates)
            {
                if (profileUpdate.handle !=
                    profileDataRef.handle)
                {
                    return false;
                }

                updateSubscriptions.push_back({
                    profileUpdate.handle,
                    profileUpdate.requestedType,
                    profileUpdate.rate,
                    profileUpdate.precision,
                    profileUpdate.element,
                    true
                });
            }
        }

        for (const auto& profileCommand : profile.commands)
        {
            if (profileCommand.handle !=
                static_cast<int>(commands.size()))
            {
                return false;
            }

            const auto lookup =
                xplane_.findCommand(profileCommand.name);

            if (!lookup.found)
            {
                return false;
            }

            commands.push_back({
                profileCommand.handle,
                profileCommand.name,
                profileCommand.active
            });
        }

        dataRefs_ = std::move(dataRefs);
        commands_ = std::move(commands);
        updateSubscriptions_ = std::move(updateSubscriptions);
        profilePreloaded_ = true;

        return true;
    }

    void LegacyDeviceController::processMessage(
        const protocol::legacy::LegacyMessage& message)
    {
        std::visit(
            [this](const auto& value)
            {
                using Message = std::decay_t<decltype(value)>;

                if constexpr (std::is_same_v<Message, protocol::legacy::RegisterDataRef>)
                {
                    handleRegisterDataRef(value);
                }
                else if constexpr (std::is_same_v<Message, protocol::legacy::RegisterCommand>)
                {
                    handleRegisterCommand(value);
                }
                else if constexpr (std::is_same_v<Message, protocol::legacy::DataRefUpdate>)
                {
                    handleDataRefUpdate(value);
                }
                else if constexpr (std::is_same_v<Message, protocol::legacy::StringDataRefUpdate>)
                {
                    handleDataRefUpdate({
                        protocol::legacy::DataRefValueType::String,
                        value.handle,
                        value.value,
                        std::nullopt
                    });
                }
                else if constexpr (std::is_same_v<Message, protocol::legacy::UpdatesRequest>)
                {
                    handleUpdatesRequest(value);
                }
                else if constexpr (std::is_same_v<Message, protocol::legacy::ScalingRequest>)
                {
                    handleScalingRequest(value);
                }
                else if constexpr (std::is_same_v<Message, protocol::legacy::CommandEvent>)
                {
                    handleCommandEvent(value);
                }
                else if constexpr (std::is_same_v<Message, protocol::legacy::DataRefTouch>)
                {
                    if (const auto* binding = findDataRef(value.handle))
                    {
                        xplane_.touchDataRef(binding->name, binding->handle);
                        requestDataRefRefresh(binding->handle);
                    }
                }
                else if constexpr (std::is_same_v<Message, protocol::legacy::DataFlowPause>)
                {
                    dataFlowPaused_ = true;
                }
                else if constexpr (std::is_same_v<Message, protocol::legacy::DataFlowResume>)
                {
                    dataFlowPaused_ = false;
                }
                else if constexpr (std::is_same_v<Message, protocol::legacy::SetDataFlowSpeed>)
                {
                    maxBytesPerSecond_ = value.bytesPerSecond;
                }
                else if constexpr (std::is_same_v<Message, protocol::legacy::DebugMessage>)
                {
                    xplane_.debugMessage(value.value);
                }
                else if constexpr (std::is_same_v<Message, protocol::legacy::SpeakMessage>)
                {
                    xplane_.speak(value.value);
                }
                else if constexpr (std::is_same_v<Message, protocol::legacy::ResetRequest>)
                {
                    xplane_.resetRequested();
                }
            },
            message);
    }

    void LegacyDeviceController::handleRegisterDataRef(
        const protocol::legacy::RegisterDataRef& message)
    {
        clearLoadedProfile();

        if (observer_ != nullptr)
        {
            observer_->microcontrollerRequestedDataRef(
                message.name);
        }

        const auto lookup = xplane_.findDataRef(message.name);

        if (!lookup.found)
        {
            session_.queueFrame(
                protocol::legacy::dataRefResponseCommand,
                failedLookupPayload(message.name));
            return;
        }

        const int handle =
            static_cast<int>(dataRefs_.size());

        dataRefs_.push_back({
            handle,
            message.name,
            lookup.type,
            true
        });

        session_.queueFrame(
            protocol::legacy::dataRefResponseCommand,
            responsePayload(handle));
    }

    void LegacyDeviceController::handleRegisterCommand(
        const protocol::legacy::RegisterCommand& message)
    {
        clearLoadedProfile();

        if (observer_ != nullptr)
        {
            observer_->microcontrollerRequestedCommand(
                message.name);
        }

        const auto lookup = xplane_.findCommand(message.name);

        if (!lookup.found)
        {
            session_.queueFrame(
                protocol::legacy::commandResponseCommand,
                failedLookupPayload(message.name));
            return;
        }

        const int handle =
            static_cast<int>(commands_.size());

        commands_.push_back({
            handle,
            message.name,
            true
        });

        session_.queueFrame(
            protocol::legacy::commandResponseCommand,
            responsePayload(handle));
    }

    void LegacyDeviceController::handleDataRefUpdate(
        const protocol::legacy::DataRefUpdate& message)
    {
        const auto* binding = findDataRef(message.handle);

        if (binding == nullptr)
        {
            return;
        }

        xplane_.writeDataRef({
            binding->name,
            binding->handle,
            toXPlaneType(message.type),
            message.type == protocol::legacy::DataRefValueType::String ?
                message.value :
                applyScaling(*binding, message.value),
            message.element
        });
    }

    void LegacyDeviceController::handleUpdatesRequest(
        const protocol::legacy::UpdatesRequest& message)
    {
        if (observer_ != nullptr)
        {
            observer_->microcontrollerRequestedUpdates(message);
        }

        if (findDataRef(message.handle) == nullptr)
        {
            return;
        }

        for (auto& subscription : updateSubscriptions_)
        {
            if (subscription.handle == message.handle &&
                subscription.requestedType == message.requestedType &&
                subscription.element == message.element)
            {
                subscription.rate = message.rate;
                subscription.precision = message.precision;
                subscription.forceUpdate = true;
                return;
            }
        }

        updateSubscriptions_.push_back({
            message.handle,
            message.requestedType,
            message.rate,
            message.precision,
            message.element,
            true
        });
    }

    void LegacyDeviceController::handleScalingRequest(
        const protocol::legacy::ScalingRequest& message)
    {
        if (observer_ != nullptr)
        {
            observer_->microcontrollerRequestedScaling(message);
        }

        auto* binding = findDataRef(message.handle);

        if (binding == nullptr)
        {
            return;
        }

        binding->scalingActive = true;
        binding->scaleFromLow = message.fromLow;
        binding->scaleFromHigh = message.fromHigh;
        binding->scaleToLow = message.toLow;
        binding->scaleToHigh = message.toHigh;
    }

    void LegacyDeviceController::handleCommandEvent(
        const protocol::legacy::CommandEvent& message)
    {
        const auto* binding = findCommand(message.handle);

        if (binding == nullptr)
        {
            return;
        }

        switch (message.action)
        {
        case protocol::legacy::CommandAction::Start:
            xplane_.commandBegin(binding->name, binding->handle);
            break;

        case protocol::legacy::CommandAction::End:
            xplane_.commandEnd(binding->name, binding->handle);
            break;

        case protocol::legacy::CommandAction::Trigger:
            xplane_.commandTrigger(
                binding->name,
                binding->handle,
                message.triggerCount);
            break;
        }
    }

    void LegacyDeviceController::requestDataRefRefresh(int handle)
    {
        for (auto& subscription : updateSubscriptions_)
        {
            if (subscription.handle == handle)
            {
                subscription.forceUpdate = true;
            }
        }
    }

    void LegacyDeviceController::clearLoadedProfile()
    {
        if (!profilePreloaded_)
        {
            return;
        }

        dataRefs_.clear();
        commands_.clear();
        updateSubscriptions_.clear();
        profilePreloaded_ = false;
    }

    LegacyDataRefBinding* LegacyDeviceController::findDataRef(
        int handle)
    {
        if (handle < 0 ||
            static_cast<std::size_t>(handle) >= dataRefs_.size())
        {
            return nullptr;
        }

        return &dataRefs_[static_cast<std::size_t>(handle)];
    }

    LegacyCommandBinding* LegacyDeviceController::findCommand(
        int handle)
    {
        if (handle < 0 ||
            static_cast<std::size_t>(handle) >= commands_.size())
        {
            return nullptr;
        }

        return &commands_[static_cast<std::size_t>(handle)];
    }
}
