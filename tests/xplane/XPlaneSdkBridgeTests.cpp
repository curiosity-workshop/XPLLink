#include <xpllink/xplane/sdk/XPlaneSdkBridge.h>

#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    class FakeXPlaneSdkApi final
        : public xpllink::xplane::sdk::IXPlaneSdkApi
    {
    public:
        xpllink::xplane::sdk::NativeDataRefLookup findDataRef(
            std::string_view name) override
        {
            const auto found =
                dataRefs.find(std::string{ name });

            if (found == dataRefs.end())
            {
                return {};
            }

            return {
                found->second,
                dataRefTypes[std::string{ name }]
            };
        }

        xpllink::xplane::sdk::NativeCommandLookup findCommand(
            std::string_view name) override
        {
            const auto found =
                commands.find(std::string{ name });

            if (found == commands.end())
            {
                return {};
            }

            return { found->second };
        }

        void writeDataRef(
            xpllink::xplane::sdk::NativeDataRefHandle,
            const xpllink::xplane::DataRefWrite& write) override
        {
            writes.push_back(write.value);
            writeTypes.push_back(write.valueType);
        }

        xpllink::xplane::DataRefReadResult readDataRef(
            xpllink::xplane::sdk::NativeDataRefHandle,
            const xpllink::xplane::DataRefReadRequest&) override
        {
            return {
                true,
                xpllink::xplane::DataRefTypeInt,
                "1",
                std::nullopt
            };
        }

        void touchDataRef(
            xpllink::xplane::sdk::NativeDataRefHandle,
            std::string_view,
            int) override
        {
            ++touchCount;
        }

        void commandBegin(
            xpllink::xplane::sdk::NativeCommandHandle handle) override
        {
            commandEvents.push_back(
                "begin:" + handleName(handle));
        }

        void commandEnd(
            xpllink::xplane::sdk::NativeCommandHandle handle) override
        {
            commandEvents.push_back(
                "end:" + handleName(handle));
        }

        void commandOnce(
            xpllink::xplane::sdk::NativeCommandHandle handle) override
        {
            commandEvents.push_back(
                "once:" + handleName(handle));
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
            ++resetCount;
        }

        static std::string handleName(
            xpllink::xplane::sdk::NativeCommandHandle handle)
        {
            return std::to_string(
                reinterpret_cast<std::uintptr_t>(handle));
        }

        std::map<std::string, xpllink::xplane::sdk::NativeDataRefHandle> dataRefs;
        std::map<std::string, int> dataRefTypes;
        std::map<std::string, xpllink::xplane::sdk::NativeCommandHandle> commands;
        std::vector<std::string> writes;
        std::vector<int> writeTypes;
        std::vector<std::string> commandEvents;
        std::vector<std::string> debugMessages;
        std::vector<std::string> spokenMessages;
        int touchCount = 0;
        int resetCount = 0;
    };

    class FakeTelemetry final
        : public xpllink::xplane::sdk::IXPlaneSdkInteractionTelemetry
    {
    public:
        void dataRefReceived(
            const xpllink::xplane::DataRefWrite& write) override
        {
            lastDataRefReceived =
                write.name + "=" + write.value + ":" +
                std::to_string(write.valueType);
        }

        void dataRefSent(
            const xpllink::xplane::DataRefReadRequest& request,
            const xpllink::xplane::DataRefReadResult& result) override
        {
            lastDataRefSent =
                request.name + "=" + result.value;
        }

        void commandAction(
            std::string_view action,
            std::string_view name,
            int handle,
            int count) override
        {
            commandActions.push_back(
                std::string{ action } + ":" +
                std::string{ name } + ":" +
                std::to_string(handle) + ":" +
                std::to_string(count));
        }

        void lookupFailure(
            std::string_view kind,
            std::string_view name) override
        {
            lookupFailures.push_back(
                std::string{ kind } + ":" +
                std::string{ name });
        }

        std::string lastDataRefReceived;
        std::string lastDataRefSent;
        std::vector<std::string> commandActions;
        std::vector<std::string> lookupFailures;
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
    using xpllink::xplane::DataRefTypeInt;
    using xpllink::xplane::sdk::XPlaneSdkBridge;

    bool passed = true;

    int nativeDataRef = 0;
    int nativeCommand = 0;
    FakeXPlaneSdkApi api;
    api.dataRefs["sim/test/dataref"] = &nativeDataRef;
    api.dataRefTypes["sim/test/dataref"] =
        xpllink::xplane::DataRefTypeInt;
    api.commands["sim/test/command"] = &nativeCommand;
    FakeTelemetry telemetry;

    XPlaneSdkBridge bridge{
        api,
        &telemetry
    };

    const auto dataRef =
        bridge.findDataRef("sim/test/dataref");

    passed &= expect(dataRef.found, "dataref lookup should resolve");
    passed &= expect(dataRef.type == DataRefTypeInt, "dataref type should pass through");

    bridge.writeDataRef({
        "sim/test/dataref",
        0,
        DataRefTypeInt,
        "42",
        std::nullopt
    });

    passed &= expect(
        api.writes.size() == 1 && api.writes[0] == "42",
        "dataref write should reach sdk api");
    passed &= expect(
        telemetry.lastDataRefReceived == "sim/test/dataref=42:1",
        "dataref write telemetry should record received value");

    const auto read =
        bridge.readDataRef({
            "sim/test/dataref",
            0,
            DataRefTypeInt,
            DataRefTypeInt,
            std::nullopt
        });

    passed &= expect(read.found && read.value == "1", "dataref read should reach sdk api");
    passed &= expect(
        telemetry.lastDataRefSent == "sim/test/dataref=1",
        "dataref read telemetry should record sent value");

    const auto command =
        bridge.findCommand("sim/test/command");

    passed &= expect(command.found, "command lookup should resolve");

    bridge.commandBegin("sim/test/command", 0);
    bridge.commandEnd("sim/test/command", 0);
    bridge.commandTrigger("sim/test/command", 0, 3);

    passed &= expect(
        bridge.pendingCommandTriggerCount() == 3,
        "trigger count should be queued");

    passed &= expect(
        bridge.dispatchCommandTriggersForCycle() == 1,
        "first cycle should dispatch one queued command occurrence");
    passed &= expect(
        bridge.pendingCommandTriggerCount() == 2,
        "two command occurrences should remain");
    passed &= expect(
        bridge.dispatchCommandTriggersForCycle() == 1,
        "second cycle should dispatch one queued command occurrence");
    passed &= expect(
        bridge.dispatchCommandTriggersForCycle() == 1,
        "third cycle should dispatch one queued command occurrence");
    passed &= expect(
        bridge.pendingCommandTriggerCount() == 0,
        "all command occurrences should be drained");

    passed &= expect(
        api.commandEvents.size() == 5,
        "begin/end/trigger events should be recorded");
    passed &= expect(
        telemetry.commandActions.size() == 6,
        "command telemetry should include begin, end, queue, and dispatches");

    bridge.findDataRef("sim/test/missing-dataref");
    bridge.findCommand("sim/test/missing-command");

    passed &= expect(
        telemetry.lookupFailures.size() == 2,
        "lookup failures should be recorded");

    {
        int floatDataRef = 0;
        FakeXPlaneSdkApi writeApi;
        writeApi.dataRefs["sim/test/float"] = &floatDataRef;
        writeApi.dataRefTypes["sim/test/float"] =
            xpllink::xplane::DataRefTypeFloat;

        XPlaneSdkBridge writeBridge{ writeApi };
        writeBridge.findDataRef("sim/test/float");
        writeBridge.writeDataRef({
            "sim/test/float",
            0,
            xpllink::xplane::DataRefTypeInt,
            "7",
            std::nullopt
        });

        passed &= expect(
            writeApi.writeTypes.size() == 1 &&
            writeApi.writeTypes[0] == xpllink::xplane::DataRefTypeFloat,
            "bridge should coerce dataref writes to an X-Plane-compatible type");
    }

    {
        FakeXPlaneSdkApi cadenceApi;
        int commandA = 0;
        int commandB = 0;
        cadenceApi.commands["sim/test/a"] = &commandA;
        cadenceApi.commands["sim/test/b"] = &commandB;

        XPlaneSdkBridge cadenceBridge{ cadenceApi };
        cadenceBridge.findCommand("sim/test/a");
        cadenceBridge.findCommand("sim/test/b");
        cadenceBridge.commandTrigger("sim/test/a", 0, 1);
        cadenceBridge.commandTrigger("sim/test/b", 1, 1);

        passed &= expect(
            cadenceBridge.pendingCommandTriggerCount() == 2,
            "two queued commands should be pending");
        passed &= expect(
            cadenceBridge.dispatchCommandTriggersForCycle() == 1,
            "one bridge should dispatch only one command occurrence per cycle");
        passed &= expect(
            cadenceBridge.pendingCommandTriggerCount() == 1,
            "one command should remain for the next cycle");
    }

    {
        FakeXPlaneSdkApi factoryApi;
        int firstCommand = 0;
        int secondCommand = 0;
        factoryApi.commands["sim/test/first"] = &firstCommand;
        factoryApi.commands["sim/test/second"] = &secondCommand;

        xpllink::xplane::sdk::XPlaneSdkBridgeFactory factory{
            factoryApi
        };

        auto first =
            factory.createBridge("COM1", "First", "1");
        auto second =
            factory.createBridge("COM2", "Second", "1");

        first->findCommand("sim/test/first");
        second->findCommand("sim/test/second");
        first->commandTrigger("sim/test/first", 0, 2);
        second->commandTrigger("sim/test/second", 0, 1);

        passed &= expect(
            factory.pendingCommandTriggerCount() == 3,
            "factory should count pending triggers across bridges");
        passed &= expect(
            factory.dispatchCommandTriggersForCycle() == 2,
            "factory should dispatch one trigger per bridge on a cycle");
        passed &= expect(
            factory.pendingCommandTriggerCount() == 1,
            "factory should retain remaining repeated triggers");
        passed &= expect(
            factory.dispatchCommandTriggersForCycle() == 1,
            "factory should drain remaining trigger on the next cycle");
    }

    return passed ? 0 : 1;
}
