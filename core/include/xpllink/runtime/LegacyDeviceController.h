#pragma once

#include <xpllink/profile/DeviceProfile.h>
#include <xpllink/protocol/legacy/LegacyMessage.h>
#include <xpllink/runtime/LegacyDeviceSession.h>
#include <xpllink/xplane/IXPlaneBridge.h>

#include <optional>
#include <string>
#include <vector>

namespace xpllink::runtime
{
    class ILegacyDeviceObserver
    {
    public:
        virtual ~ILegacyDeviceObserver() = default;

        virtual void microcontrollerRequestedDataRef(
            std::string_view name) = 0;

        virtual void microcontrollerRequestedCommand(
            std::string_view name) = 0;

        virtual void microcontrollerRequestedUpdates(
            const protocol::legacy::UpdatesRequest& request) = 0;

        virtual void microcontrollerRequestedScaling(
            const protocol::legacy::ScalingRequest& request) = 0;

        virtual void dataRefReceivedFromDevice(
            std::string_view name,
            std::string_view serialValue,
            std::string_view xplaneValue,
            std::optional<int> element) = 0;

        virtual void dataRefSentToDevice(
            std::string_view name,
            std::string_view value,
            std::optional<int> element) = 0;
    };

    struct LegacyDataRefBinding
    {
        int handle = -1;
        std::string name;
        int xplaneType = xplane::DataRefTypeUnknown;
        bool active = false;

        bool scalingActive = false;
        long scaleFromLow = 0;
        long scaleFromHigh = 0;
        long scaleToLow = 0;
        long scaleToHigh = 0;
    };

    struct LegacyCommandBinding
    {
        int handle = -1;
        std::string name;
        bool active = false;
    };

    struct LegacyUpdateSubscription
    {
        int handle = -1;
        std::optional<int> requestedType;
        int rate = 0;
        std::string precision;
        std::optional<int> element;
        bool forceUpdate = true;
    };

    struct LegacyDeviceControllerTickResult
    {
        LegacyDeviceSessionTickResult session;
        std::size_t messagesProcessed = 0;
        std::size_t responseBytesWritten = 0;
    };

    class LegacyDeviceController
    {
    public:
        LegacyDeviceController(
            LegacyDeviceSession& session,
            xplane::IXPlaneBridge& xplane);

        LegacyDeviceController(
            LegacyDeviceSession& session,
            xplane::IXPlaneBridge& xplane,
            ILegacyDeviceObserver& observer);

        LegacyDeviceControllerTickResult tick();
        void requestRegistrations();
        bool tryLoadProfile(
            const profile::DeviceProfile& profile);

        const std::vector<LegacyDataRefBinding>& dataRefs() const;
        const std::vector<LegacyCommandBinding>& commands() const;
        const std::vector<LegacyUpdateSubscription>& updateSubscriptions() const;
        std::vector<LegacyUpdateSubscription>& updateSubscriptions();

        bool registrationsRequested() const;
        bool hasRegistrationFailures() const;
        bool dataFlowPaused() const;
        long maxBytesPerSecond() const;

    private:
        void processMessage(
            const protocol::legacy::LegacyMessage& message);

        void handleRegisterDataRef(
            const protocol::legacy::RegisterDataRef& message);

        void handleRegisterCommand(
            const protocol::legacy::RegisterCommand& message);

        void handleDataRefUpdate(
            const protocol::legacy::DataRefUpdate& message);

        void handleUpdatesRequest(
            const protocol::legacy::UpdatesRequest& message);

        void handleScalingRequest(
            const protocol::legacy::ScalingRequest& message);

        void handleCommandEvent(
            const protocol::legacy::CommandEvent& message);

        void requestDataRefRefresh(int handle);

        void clearLoadedProfile();

        LegacyDataRefBinding* findDataRef(int handle);
        LegacyCommandBinding* findCommand(int handle);

        LegacyDeviceSession& session_;
        xplane::IXPlaneBridge& xplane_;
        ILegacyDeviceObserver* observer_ = nullptr;
        std::vector<LegacyDataRefBinding> dataRefs_;
        std::vector<LegacyCommandBinding> commands_;
        std::vector<LegacyUpdateSubscription> updateSubscriptions_;
        bool profilePreloaded_ = false;
        bool registrationsRequested_ = false;
        bool registrationFailures_ = false;
        bool dataFlowPaused_ = false;
        long maxBytesPerSecond_ = 0;
    };
}
