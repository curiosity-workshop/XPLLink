#include <xpllink/runtime/LegacyDeviceController.h>

#include <algorithm>
#include <cstddef>
#include <deque>
#include <iostream>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    class FakeTransport final : public xpllink::transport::IByteTransport
    {
    public:
        bool open() override
        {
            open_ = true;
            return true;
        }

        void close() override
        {
            open_ = false;
        }

        bool isOpen() const override
        {
            return open_;
        }

        std::size_t read(std::span<std::byte> buffer) override
        {
            if (incoming_.empty())
            {
                return 0;
            }

            const auto chunk = incoming_.front();
            incoming_.pop_front();

            const std::size_t bytesToRead =
                std::min(buffer.size(), chunk.size());

            std::copy_n(
                chunk.begin(),
                bytesToRead,
                buffer.begin());

            if (bytesToRead < chunk.size())
            {
                incoming_.push_front({
                    chunk.begin() + bytesToRead,
                    chunk.end()
                });
            }

            return bytesToRead;
        }

        std::size_t write(std::span<const std::byte> data) override
        {
            written_.insert(
                written_.end(),
                data.begin(),
                data.end());

            return data.size();
        }

        void pushIncoming(std::string_view text)
        {
            std::vector<std::byte> bytes;
            bytes.reserve(text.size());

            for (const char character : text)
            {
                bytes.push_back(static_cast<std::byte>(character));
            }

            incoming_.push_back(std::move(bytes));
        }

        std::string writtenText() const
        {
            std::string result;
            result.reserve(written_.size());

            for (const std::byte value : written_)
            {
                result.push_back(
                    static_cast<char>(
                        std::to_integer<unsigned char>(value)));
            }

            return result;
        }

    private:
        bool open_ = true;
        std::deque<std::vector<std::byte>> incoming_;
        std::vector<std::byte> written_;
    };

    class FakeXPlaneBridge final : public xpllink::xplane::IXPlaneBridge
    {
    public:
        xpllink::xplane::DataRefLookupResult findDataRef(
            std::string_view name) override
        {
            const auto found =
                dataRefs.find(std::string{ name });

            if (found == dataRefs.end())
            {
                return {};
            }

            return {
                true,
                found->second
            };
        }

        xpllink::xplane::CommandLookupResult findCommand(
            std::string_view name) override
        {
            return {
                std::find(
                    commands.begin(),
                    commands.end(),
                    std::string{ name }) != commands.end()
            };
        }

        void writeDataRef(
            const xpllink::xplane::DataRefWrite& write) override
        {
            dataRefWrites.push_back(write);
        }

        xpllink::xplane::DataRefReadResult readDataRef(
            const xpllink::xplane::DataRefReadRequest&) override
        {
            return {};
        }

        void touchDataRef(
            std::string_view name,
            int handle) override
        {
            touchedDataRefs.push_back({
                std::string{ name },
                handle
            });
        }

        void commandBegin(
            std::string_view name,
            int handle) override
        {
            commandEvents.push_back(
                "begin:" + std::string{ name } + ":" + std::to_string(handle));
        }

        void commandEnd(
            std::string_view name,
            int handle) override
        {
            commandEvents.push_back(
                "end:" + std::string{ name } + ":" + std::to_string(handle));
        }

        void commandTrigger(
            std::string_view name,
            int handle,
            int triggerCount) override
        {
            commandEvents.push_back(
                "trigger:" + std::string{ name } + ":" +
                std::to_string(handle) + ":" +
                std::to_string(triggerCount));
        }

        void debugMessage(std::string_view message) override
        {
            debugMessages.push_back(std::string{ message });
        }

        void speak(std::string_view message) override
        {
            spokenMessages.push_back(std::string{ message });
        }

        void resetRequested() override
        {
            resetCount++;
        }

        std::map<std::string, int> dataRefs;
        std::vector<std::string> commands;
        std::vector<xpllink::xplane::DataRefWrite> dataRefWrites;
        std::vector<std::pair<std::string, int>> touchedDataRefs;
        std::vector<std::string> commandEvents;
        std::vector<std::string> debugMessages;
        std::vector<std::string> spokenMessages;
        int resetCount = 0;
    };

    class FakeLegacyDeviceObserver final
        : public xpllink::runtime::ILegacyDeviceObserver
    {
    public:
        void microcontrollerRequestedDataRef(
            std::string_view name) override
        {
            requests.push_back(
                "dataref:" + std::string{ name });
        }

        void microcontrollerRequestedCommand(
            std::string_view name) override
        {
            requests.push_back(
                "command:" + std::string{ name });
        }

        void microcontrollerRequestedUpdates(
            const xpllink::protocol::legacy::UpdatesRequest& request) override
        {
            requests.push_back(
                "updates:" +
                std::to_string(request.handle) + ":" +
                std::to_string(request.rate) + ":" +
                request.precision);
        }

        void microcontrollerRequestedScaling(
            const xpllink::protocol::legacy::ScalingRequest& request) override
        {
            requests.push_back(
                "scaling:" +
                std::to_string(request.handle) + ":" +
                std::to_string(request.fromLow) + ":" +
                std::to_string(request.fromHigh) + ":" +
                std::to_string(request.toLow) + ":" +
                std::to_string(request.toHigh));
        }

        void dataRefReceivedFromDevice(
            std::string_view name,
            std::string_view serialValue,
            std::string_view xplaneValue,
            std::optional<int> element) override
        {
            received.push_back(
                std::string{ name } + ":" +
                std::string{ serialValue } + ":" +
                std::string{ xplaneValue } + ":" +
                (element ? std::to_string(*element) : "none"));
        }

        void dataRefSentToDevice(
            std::string_view name,
            std::string_view value,
            std::optional<int>) override
        {
            requests.push_back(
                "sent:" + std::string{ name } + ":" +
                std::string{ value });
        }

        std::vector<std::string> requests;
        std::vector<std::string> received;
    };

    bool expect(bool condition, std::string_view message)
    {
        if (!condition)
        {
            std::cerr << message << '\n';
            return false;
        }

        return true;
    }
}

int main()
{
    using xpllink::runtime::LegacyDeviceController;
    using xpllink::runtime::LegacyDeviceSession;
    using xpllink::runtime::LegacyDeviceSessionOptions;
    using xpllink::xplane::DataRefTypeData;
    using xpllink::xplane::DataRefTypeFloat;
    using xpllink::xplane::DataRefTypeInt;

    bool passed = true;

    FakeTransport transport;
    FakeXPlaneBridge xplane;
    FakeLegacyDeviceObserver observer;

    xplane.dataRefs["sim/test/int"] = DataRefTypeInt;
    xplane.dataRefs["sim/test/data"] = DataRefTypeData;
    xplane.dataRefs["sim/test/float"] = DataRefTypeFloat;
    xplane.commands.push_back("sim/test/command");

    LegacyDeviceSession session{
        transport,
        LegacyDeviceSessionOptions{
            .readBufferSize = 64,
            .maximumReadPasses = 8
        }
    };

    LegacyDeviceController controller{
        session,
        xplane,
        observer
    };

    controller.requestRegistrations();
    auto tick = controller.tick();

    passed &= expect(controller.registrationsRequested(), "registration phase should be tracked");
    passed &= expect(tick.session.bytesWritten == 3, "registration query should flush");
    passed &= expect(transport.writtenText() == "[Q]", "registration query frame failed");

    transport.pushIncoming("[b,\"sim/test/int\"]");
    tick = controller.tick();

    passed &= expect(tick.messagesProcessed == 1, "dataref registration should process");
    passed &= expect(transport.writtenText() == "[Q][D,0]", "dataref registration response failed");
    passed &= expect(controller.dataRefs().size() == 1, "dataref binding should be retained");
    passed &= expect(
        controller.dataRefs()[0].name == "sim/test/int",
        "dataref binding name failed");
    passed &= expect(
        observer.requests.size() == 1 &&
        observer.requests[0] == "dataref:sim/test/int",
        "dataref request should be observable");

    transport.pushIncoming("[m,\"sim/test/command\"]");
    tick = controller.tick();

    passed &= expect(tick.messagesProcessed == 1, "command registration should process");
    passed &= expect(
        transport.writtenText() == "[Q][D,0][C,0]",
        "command registration response failed");
    passed &= expect(controller.commands().size() == 1, "command binding should be retained");
    passed &= expect(
        observer.requests.size() == 2 &&
        observer.requests[1] == "command:sim/test/command",
        "command request should be observable");

    transport.pushIncoming("[1,0,42]");
    tick = controller.tick();

    passed &= expect(tick.messagesProcessed == 1, "dataref update should process");
    passed &= expect(xplane.dataRefWrites.size() == 1, "dataref update should reach xplane");
    passed &= expect(
        xplane.dataRefWrites[0].name == "sim/test/int",
        "dataref update name failed");
    passed &= expect(xplane.dataRefWrites[0].value == "42", "dataref update value failed");

    transport.pushIncoming("[b,\"sim/test/data\"]");
    tick = controller.tick();

    passed &= expect(
        tick.messagesProcessed == 1,
        "data dataref registration should process");

    const std::string binaryUpdate{
        {'[', '9', ',', '1', ',', '6', ']', 'A', '\0', 'B', 'C', '\0', 'D'}
    };
    transport.pushIncoming(binaryUpdate);
    tick = controller.tick();

    passed &= expect(
        tick.messagesProcessed == 1,
        "binary dataref update should process");
    passed &= expect(
        xplane.dataRefWrites.size() == 2,
        "binary dataref update should reach xplane");
    passed &= expect(
        xplane.dataRefWrites[1].name == "sim/test/data",
        "binary dataref update name failed");
    passed &= expect(
        xplane.dataRefWrites[1].valueType == DataRefTypeData,
        "binary dataref update type failed");
    passed &= expect(
        xplane.dataRefWrites[1].value.size() == 6,
        "binary dataref update size failed");
    passed &= expect(
        xplane.dataRefWrites[1].value[1] == '\0' &&
        xplane.dataRefWrites[1].value[4] == '\0',
        "binary dataref update should preserve embedded nulls");

    transport.pushIncoming("[r,0,100,0.0000]");
    tick = controller.tick();

    passed &= expect(tick.messagesProcessed == 1, "updates request should process");
    passed &= expect(
        controller.updateSubscriptions().size() == 1,
        "updates request should be retained");
    passed &= expect(
        controller.updateSubscriptions()[0].forceUpdate,
        "updates request should force initial update");
    passed &= expect(
        observer.requests.size() == 4 &&
        observer.requests[3] == "updates:0:100:0.0000",
        "updates request should be observable");

    transport.pushIncoming("[r,0,1000,10.0000]");
    tick = controller.tick();

    passed &= expect(
        tick.messagesProcessed == 1,
        "replacement updates request should process");
    passed &= expect(
        controller.updateSubscriptions().size() == 1,
        "replacement updates request should not duplicate");
    passed &= expect(
        controller.updateSubscriptions()[0].rate == 1000 &&
        controller.updateSubscriptions()[0].precision == "10.0000",
        "replacement updates request should update rate and precision");

    transport.pushIncoming("[u,0,0,1024,0,1]");
    tick = controller.tick();

    passed &= expect(tick.messagesProcessed == 1, "scaling request should process");
    passed &= expect(controller.dataRefs()[0].scalingActive, "scaling should be active");
    passed &= expect(controller.dataRefs()[0].scaleFromHigh == 1024, "scaling range failed");
    passed &= expect(
        observer.requests.size() == 6 &&
        observer.requests[5] == "scaling:0:0:1024:0:1",
        "scaling request should be observable");

    transport.pushIncoming("[1,0,512]");
    tick = controller.tick();

    passed &= expect(tick.messagesProcessed == 1, "scaled dataref write should process");
    passed &= expect(
        xplane.dataRefWrites.size() >= 3 &&
        xplane.dataRefWrites.back().value == "0.5",
        "scaled dataref write should map raw values before X-Plane write");
    passed &= expect(
        !observer.received.empty() &&
        observer.received.back() == "sim/test/int:512:0.5:none",
        "scaled dataref telemetry should include serial and X-Plane values");

    transport.pushIncoming("[k,0,3]");
    tick = controller.tick();

    passed &= expect(tick.messagesProcessed == 1, "command trigger should process");
    passed &= expect(
        xplane.commandEvents.size() == 1 &&
        xplane.commandEvents[0] == "trigger:sim/test/command:0:3",
        "command trigger should reach xplane");

    transport.pushIncoming("[g,\"hello\"]");
    transport.pushIncoming("[s,\"speak\"]");
    transport.pushIncoming("[z,0]");
    transport.pushIncoming("[p]");
    transport.pushIncoming("[q]");
    tick = controller.tick();

    passed &= expect(tick.messagesProcessed == 5, "control messages should process");
    passed &= expect(
        xplane.debugMessages.size() == 1 &&
        xplane.debugMessages[0] == "hello",
        "debug message should reach xplane");
    passed &= expect(
        xplane.spokenMessages.size() == 1 &&
        xplane.spokenMessages[0] == "speak",
        "speak message should reach xplane");
    passed &= expect(xplane.resetCount == 1, "reset should reach xplane");
    passed &= expect(!controller.dataFlowPaused(), "flow should be resumed");

    transport.pushIncoming("[b,\"sim/missing\"]");
    tick = controller.tick();

    passed &= expect(tick.messagesProcessed == 1, "missing dataref should process");
    passed &= expect(
        transport.writtenText().ends_with("[D,-1,\"sim/missing\"]"),
        "missing dataref response failed");
    passed &= expect(
        controller.dataRefs().size() == 2,
        "missing dataref should not create a binding");
    passed &= expect(
        controller.hasRegistrationFailures(),
        "missing dataref should mark the session non-cacheable");

    const auto subscriptionsBeforeRejectedSequence =
        controller.updateSubscriptions().size();
    transport.pushIncoming(
        "[r,2,250,0.8000]"
        "[b,\"sim/missing/next\"]");
    tick = controller.tick();

    passed &= expect(
        tick.messagesProcessed == 2,
        "interleaved rejected dataref sequence should process");
    passed &= expect(
        transport.writtenText().ends_with(
            "[D,-1,\"sim/missing/next\"]"),
        "interleaved rejected dataref response failed");
    passed &= expect(
        controller.dataRefs().size() == 2 &&
        controller.updateSubscriptions().size() ==
            subscriptionsBeforeRejectedSequence,
        "rejected handle should not create a binding or subscription");

    transport.pushIncoming("[m,\"sim/missing/command\"]");
    tick = controller.tick();

    passed &= expect(tick.messagesProcessed == 1, "missing command should process");
    passed &= expect(
        transport.writtenText().ends_with("[C,-1,\"sim/missing/command\"]"),
        "missing command response failed");
    passed &= expect(
        controller.commands().size() == 1,
        "missing command should not create a binding");

    {
        FakeTransport profileTransport;
        FakeXPlaneBridge profileXPlane;

        profileXPlane.dataRefs["sim/profile/dataref"] = DataRefTypeInt;
        profileXPlane.commands.push_back("sim/profile/command");

        LegacyDeviceSession profileSession{
            profileTransport,
            LegacyDeviceSessionOptions{
                .readBufferSize = 64,
                .maximumReadPasses = 8
            }
        };

        LegacyDeviceController profileController{
            profileSession,
            profileXPlane
        };

        xpllink::profile::DeviceProfile profile;
        profile.dataRefs.push_back({
            0,
            "sim/profile/dataref",
            DataRefTypeFloat,
            true
        });
        profile.dataRefs[0].updates.push_back({
            0,
            DataRefTypeFloat,
            250,
            "0.0100",
            std::nullopt
        });
        profile.commands.push_back({
            0,
            "sim/profile/command",
            true
        });

        passed &= expect(
            profileController.tryLoadProfile(profile),
            "profile should load when entries resolve");
        passed &= expect(
            profileController.dataRefs().size() == 1,
            "profile dataref should be retained");
        passed &= expect(
            profileController.dataRefs()[0].xplaneType == DataRefTypeInt,
            "profile should use the current xplane dataref type");
        passed &= expect(
            profileController.commands().size() == 1,
            "profile command should be retained");
        passed &= expect(
            profileController.updateSubscriptions().size() == 1,
            "profile update subscription should be retained");
        passed &= expect(
            profileController.updateSubscriptions()[0].rate == 250,
            "profile update subscription rate should be retained");
        passed &= expect(
            profileController.updateSubscriptions()[0].forceUpdate,
            "profile update subscription should force refresh after cache load");

        profileTransport.pushIncoming("[y,0,2,100,0.0000]");
        auto profileTick = profileController.tick();

        passed &= expect(
            profileTick.messagesProcessed == 1,
            "profile update request should process");
        passed &= expect(
            profileController.updateSubscriptions().size() == 1,
            "profile update request should replace matching subscription");
        passed &= expect(
            profileController.updateSubscriptions()[0].rate == 100,
            "profile update request should update matching subscription rate");

        profileTransport.pushIncoming("[b,\"sim/profile/dataref\"]");
        profileTick = profileController.tick();

        passed &= expect(
            profileTick.messagesProcessed == 1,
            "legacy registration should process after profile fallback");
        passed &= expect(
            profileController.dataRefs().size() == 1,
            "legacy registration should clear preloaded profile before adding");
        passed &= expect(
            profileController.updateSubscriptions().empty(),
            "legacy fallback should clear profile-mode subscriptions");
        passed &= expect(
            profileTransport.writtenText().ends_with("[D,0]"),
            "legacy fallback should reply with sequential handle zero");
    }

    return passed ? 0 : 1;
}
