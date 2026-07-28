#include "PhoenixStatusWindow.h"

#include <phoenix/discovery/DiscoveredDevice.h>
#include <phoenix/logging/SerialTraceLogger.h>
#include <phoenix/protocol/legacy/LegacyFrame.h>
#include <phoenix/protocol/legacy/LegacyFrameParser.h>
#include <phoenix/runtime/DeviceRuntimeManager.h>
#include <phoenix/serial/NativeSerial.h>
#include <phoenix/serial/SerialDeviceClassifier.h>
#include <phoenix/serial/SerialDeviceKind.h>
#include <phoenix/serial/SerialPortInfo.h>
#include <phoenix/transport/IByteTransport.h>
#include <phoenix/xplane/sdk/XPlaneSdkApi.h>
#include <phoenix/xplane/sdk/XPlaneSdkBridge.h>

#include <XPLMMenus.h>
#include <XPLMPlugin.h>
#include <XPLMProcessing.h>
#include <XPLMUtilities.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace
{
    enum class PhoenixMenuAction : std::intptr_t
    {
        ToggleStatusWindow = 1,
        EngageDevices = 2,
        DisengageDevices = 3,
        ToggleSerialTrace = 4,
        ClearProfileCache = 5
    };

    enum class DeviceEngagementState
    {
        Idle,
        Scanning,
        Complete
    };

    constexpr const char* PluginDisplayName = "XPLLink";
    constexpr const char* PluginSignature = "com.curiosityworkshop.xpllink";
    constexpr const char* PluginDescription =
        "XPLLink runtime bridge.";

    std::filesystem::path pluginOutputDirectory()
    {
        char systemPath[4096]{};
        XPLMGetSystemPath(systemPath);

        return std::filesystem::path{ systemPath } /
            "Output" /
            PluginDisplayName;
    }

    void ensureDirectory(
        const std::filesystem::path& path)
    {
        std::error_code error;
        std::filesystem::create_directories(
            path,
            error);
    }

    std::filesystem::path preparedPhoenixOutputDirectory()
    {
        auto path =
            pluginOutputDirectory();
        ensureDirectory(path);
        ensureDirectory(path / "profiles");
        return path;
    }

    std::string deviceKindName(
        phoenix::serial::SerialDeviceKind kind)
    {
        using phoenix::serial::SerialDeviceKind;

        switch (kind)
        {
        case SerialDeviceKind::ArduinoCompatible:
            return "Arduino-compatible";

        case SerialDeviceKind::UsbSerial:
            return "USB serial";

        case SerialDeviceKind::Suspect:
            return "suspect serial";

        case SerialDeviceKind::Bluetooth:
            return "Bluetooth";

        case SerialDeviceKind::BuiltInSerial:
            return "built-in serial";

        case SerialDeviceKind::Unknown:
            return "unknown";
        }

        return "unknown";
    }

    std::string controlModeName(
        phoenix::serial::NativeSerialControlMode mode)
    {
        using phoenix::serial::NativeSerialControlMode;

        switch (mode)
        {
        case NativeSerialControlMode::DtrRtsDisabled:
            return "DTR/RTS disabled";

        case NativeSerialControlMode::DtrRtsEnabled:
            return "DTR/RTS enabled";
        }

        return "unknown control mode";
    }

    void logSerialPortInformation(
        const phoenix::serial::SerialPortInfo& port,
        phoenix::serial::SerialDeviceKind kind,
        std::string_view action)
    {
        std::ostringstream message;
        message
            << "Phoenix: serial port "
            << port.portName
            << " "
            << action
            << ".\n"
            << "  Classification: "
            << deviceKindName(kind)
            << "\n"
            << "  Name: "
            << port.displayName
            << "\n"
            << "  Manufacturer: "
            << port.manufacturer
            << "\n"
            << "  Hardware ID: "
            << port.hardwareId
            << "\n";

        XPLMDebugString(message.str().c_str());
    }

    void logProbeEvent(
        const phoenix::serial::SerialPortInfo& port,
        phoenix::serial::NativeSerialControlMode mode,
        std::string_view event)
    {
        std::ostringstream message;
        message
            << "Phoenix: probe "
            << event
            << " on "
            << port.portName
            << " ("
            << deviceKindName(port.kind)
            << ", "
            << controlModeName(mode)
            << ").\n";

        XPLMDebugString(message.str().c_str());
    }

    std::string transportDiagnostic(
        const phoenix::transport::IByteTransport* transport)
    {
#if defined(_WIN32)
        const auto* windowsTransport =
            dynamic_cast<const phoenix::serial::WindowsSerialTransport*>(
                transport);

        if (windowsTransport != nullptr &&
            !windowsTransport->lastErrorMessage().empty())
        {
            return " (" + windowsTransport->lastErrorMessage() + ")";
        }
#else
        (void)transport;
#endif

        return {};
    }

    std::string extractQuotedString(
        std::string_view payload)
    {
        const std::size_t firstQuote =
            payload.find('"');

        if (firstQuote == std::string_view::npos)
        {
            return {};
        }

        const std::size_t secondQuote =
            payload.find('"', firstQuote + 1);

        if (secondQuote == std::string_view::npos)
        {
            return {};
        }

        return std::string{
            payload.substr(
                firstQuote + 1,
                secondQuote - firstQuote - 1)
        };
    }

    class PluginLegacyDeviceObserver final
        : public phoenix::runtime::ILegacyDeviceObserver
    {
    public:
        explicit PluginLegacyDeviceObserver(
            std::string portName,
            std::function<void(
                std::string_view,
                std::string_view,
                std::optional<int>)> dataRefSentCallback)
            : portName_(std::move(portName))
            , dataRefSentCallback_(std::move(dataRefSentCallback))
        {
        }

        void microcontrollerRequestedDataRef(
            std::string_view name) override
        {
            logRequest(
                "dataref",
                name);
        }

        void microcontrollerRequestedCommand(
            std::string_view name) override
        {
            logRequest(
                "command",
                name);
        }

        void microcontrollerRequestedUpdates(
            const phoenix::protocol::legacy::UpdatesRequest&) override
        {
        }

        void microcontrollerRequestedScaling(
            const phoenix::protocol::legacy::ScalingRequest&) override
        {
        }

        void dataRefSentToDevice(
            std::string_view name,
            std::string_view value,
            std::optional<int> element) override
        {
            if (dataRefSentCallback_)
            {
                dataRefSentCallback_(
                    name,
                    value,
                    element);
            }
        }

    private:
        void logRequest(
            std::string_view type,
            std::string_view name) const
        {
            std::ostringstream message;
            message
                << "Phoenix: "
                << portName_
                << " requested "
                << type
                << ": "
                << name
                << ".\n";

            XPLMDebugString(message.str().c_str());
        }

        std::string portName_;
        std::function<void(
            std::string_view,
            std::string_view,
            std::optional<int>)> dataRefSentCallback_;
    };

    class PluginLegacyDeviceObserverFactory final
        : public phoenix::runtime::ILegacyDeviceObserverFactory
    {
    public:
        explicit PluginLegacyDeviceObserverFactory(
            std::function<void(
                std::string_view,
                std::string_view,
                std::optional<int>)> dataRefSentCallback)
            : dataRefSentCallback_(std::move(dataRefSentCallback))
        {
        }

        std::unique_ptr<phoenix::runtime::ILegacyDeviceObserver>
            createObserver(
                std::string_view portName,
                std::string_view,
                std::string_view) override
        {
            return std::make_unique<PluginLegacyDeviceObserver>(
                std::string{ portName },
                dataRefSentCallback_);
        }

    private:
        std::function<void(
            std::string_view,
            std::string_view,
            std::optional<int>)> dataRefSentCallback_;
    };

    class ToggleableSerialTraceSink final
        : public phoenix::logging::ISerialTraceSink
    {
    public:
        explicit ToggleableSerialTraceSink(
            const std::filesystem::path& path)
            : logger_(path)
        {
        }

        void setEnabled(
            bool enabled)
        {
            enabled_ =
                enabled;
        }

        [[nodiscard]] bool isEnabled() const
        {
            return enabled_;
        }

        [[nodiscard]] bool isOpen() const
        {
            return logger_.isOpen();
        }

        void bytes(
            phoenix::logging::ByteDumpDirection direction,
            std::string_view portName,
            std::span<const std::byte> data) override
        {
            if (!enabled_)
            {
                return;
            }

            logger_.bytes(
                direction,
                portName,
                data);
        }

    private:
        phoenix::logging::SerialTraceLogger logger_;
        bool enabled_ = false;
    };

    class PluginInteractionTelemetry final
        : public phoenix::xplane::sdk::IXPlaneSdkInteractionTelemetry
    {
    public:
        void dataRefReceived(
            const phoenix::xplane::DataRefWrite& write) override
        {
            std::ostringstream message;
            message
                << write.name
                << " = "
                << write.value;

            if (write.element)
            {
                message
                    << " element "
                    << *write.element;
            }

            lastDataRefReceived_ =
                message.str();
        }

        void dataRefSent(
            const phoenix::xplane::DataRefReadRequest& request,
            const phoenix::xplane::DataRefReadResult& result) override
        {
        }

        void dataRefSentToDevice(
            std::string_view name,
            std::string_view value,
            std::optional<int> element)
        {
            std::ostringstream message;
            message
                << name
                << " = "
                << value;

            if (element)
            {
                message
                    << " element "
                    << *element;
            }

            lastDataRefSent_ =
                message.str();
        }

        void commandAction(
            std::string_view action,
            std::string_view name,
            int handle,
            int count) override
        {
            std::ostringstream message;
            message
                << name
                << " "
                << action
                << " handle "
                << handle;

            if (count > 1)
            {
                message
                    << " count "
                    << count;
            }

            lastCommandAction_ =
                message.str();
        }

        void lookupFailure(
            std::string_view kind,
            std::string_view name) override
        {
            std::ostringstream message;
            message
                << kind
                << " lookup failed: "
                << name;

            lastLookupFailure_ =
                message.str();
        }

        phoenix::xplane::sdk::XPlaneSdkInteractionTelemetrySnapshot
            snapshot() const
        {
            return {
                lastDataRefReceived_,
                lastDataRefSent_,
                lastCommandAction_,
                lastLookupFailure_
            };
        }

    private:
        std::string lastDataRefReceived_;
        std::string lastDataRefSent_;
        std::string lastCommandAction_;
        std::string lastLookupFailure_;
    };

    struct IncrementalProbe
    {
        phoenix::serial::SerialPortInfo port;
        std::unique_ptr<phoenix::transport::IByteTransport> transport;
        phoenix::protocol::legacy::LegacyFrameParser parser;
        phoenix::discovery::DiscoveredDevice device;
        phoenix::serial::NativeSerialControlMode controlMode =
            phoenix::serial::NativeSerialControlMode::DtrRtsDisabled;
        std::chrono::steady_clock::time_point deadline{};
        std::chrono::steady_clock::time_point nextRequestAt{};
        bool requestsStarted = false;
    };

    class PhoenixPluginRuntime
    {
    public:
        PhoenixPluginRuntime()
            : outputDirectory_(preparedPhoenixOutputDirectory())
            , profileDirectory_(outputDirectory_ / "profiles")
            , serialTracePath_(outputDirectory_ / "XPLLinkSerial.log")
            , settingsPath_(outputDirectory_ / "XPLLinkSettings.txt")
            , serialTrace_(serialTracePath_)
            , deviceRuntime_(
                serialTrace_,
                bridgeFactory_,
                phoenix::runtime::DeviceRuntimeManagerOptions{
                    .profileDirectory = profileDirectory_,
                    .updateScheduler = phoenix::runtime::LegacyUpdateSchedulerOptions{
                        .maxFramesPerTick = 8,
                        .maxBytesPerTick = 64
                    },
                    .readBufferSize = 256,
                    .maximumReadPasses = 8
                },
                &observerFactory_)
            , statusWindow_(
                [this]()
                {
                    return statusWindowLines();
                })
        {
            loadSettings();
            createMenu();
        }

        ~PhoenixPluginRuntime()
        {
            activeProbe_.reset();
            deviceRuntime_.disengageDevices();
            bridgeFactory_.clearBridges();
            statusWindow_.close();
            destroyMenu();
        }

        float flightLoop()
        {
            processRequestedLifecycleActions();
            bridgeFactory_.dispatchCommandTriggersForCycle();
            tickDeviceEngagement();
            deviceRuntime_.tick(
                std::chrono::steady_clock::now());
            return -1.0f;
        }

        void handleMenuAction(
            PhoenixMenuAction action)
        {
            switch (action)
            {
            case PhoenixMenuAction::EngageDevices:
                engageDevices();
                break;

            case PhoenixMenuAction::DisengageDevices:
                disengageDevices();
                break;

            case PhoenixMenuAction::ToggleSerialTrace:
                toggleSerialTrace();
                break;

            case PhoenixMenuAction::ClearProfileCache:
                clearProfileCache();
                break;

            case PhoenixMenuAction::ToggleStatusWindow:
                toggleStatusWindow();
                break;
            }
        }

        void requestAutoEngage(
            std::string reason)
        {
            pendingAutoEngage_ =
                true;
            pendingAutoEngageReason_ =
                std::move(reason);
        }

        void requestDisengage(
            std::string reason)
        {
            pendingDisengage_ =
                true;
            pendingDisengageReason_ =
                std::move(reason);
        }

        void handleXPlaneMessage(
            int message,
            void* parameter)
        {
            const auto aircraftIndex =
                reinterpret_cast<std::intptr_t>(parameter);

            if (message == XPLM_MSG_PLANE_UNLOADED &&
                aircraftIndex == 0)
            {
                requestDisengage(
                    "user aircraft unloaded");
                return;
            }

            if (message == XPLM_MSG_LIVERY_LOADED &&
                aircraftIndex == 0)
            {
                requestAutoEngage(
                    "user aircraft livery loaded");
            }
        }

    private:
        static constexpr auto ProbeRetryInterval =
            std::chrono::milliseconds{ 50 };
        static constexpr auto ProbeTimeout =
            std::chrono::seconds{ 2 };
        static constexpr auto ProbeOpenSettleDelay =
            std::chrono::seconds{ 3 };
        static constexpr auto AutoCloseStatusWindowDelay =
            std::chrono::seconds{ 3 };

        static void menuHandler(
            void* menuRef,
            void* itemRef)
        {
            auto* runtime =
                static_cast<PhoenixPluginRuntime*>(menuRef);

            if (runtime == nullptr)
            {
                return;
            }

            runtime->handleMenuAction(
                static_cast<PhoenixMenuAction>(
                    reinterpret_cast<std::intptr_t>(itemRef)));
        }

        static void* menuItemRef(
            PhoenixMenuAction action)
        {
            return reinterpret_cast<void*>(
                static_cast<std::intptr_t>(action));
        }

        static void debugLog(
            std::string_view message)
        {
            const std::string stableMessage{ message };
            XPLMDebugString(stableMessage.c_str());
        }

        void createMenu()
        {
            XPLMMenuID pluginsMenu =
                XPLMFindPluginsMenu();

            if (pluginsMenu == nullptr)
            {
                debugLog(
                    "XPLLink: unable to find X-Plane plugins menu.\n");
                return;
            }

            pluginsMenuItemIndex_ =
                XPLMAppendMenuItem(
                    pluginsMenu,
                    PluginDisplayName,
                    nullptr,
                    0);

            menu_ =
                XPLMCreateMenu(
                    PluginDisplayName,
                    pluginsMenu,
                    pluginsMenuItemIndex_,
                    menuHandler,
                    this);

            if (menu_ == nullptr)
            {
                debugLog(
                    "XPLLink: unable to create plugin menu.\n");
                return;
            }

            XPLMAppendMenuItem(
                menu_,
                "Status Window",
                menuItemRef(PhoenixMenuAction::ToggleStatusWindow),
                0);
            XPLMAppendMenuItem(
                menu_,
                "Engage Devices",
                menuItemRef(PhoenixMenuAction::EngageDevices),
                0);
            XPLMAppendMenuItem(
                menu_,
                "Disengage Devices",
                menuItemRef(PhoenixMenuAction::DisengageDevices),
                0);
            XPLMAppendMenuSeparator(menu_);
            serialTraceItemIndex_ =
                XPLMAppendMenuItem(
                    menu_,
                    "Serial Trace Logging",
                    menuItemRef(PhoenixMenuAction::ToggleSerialTrace),
                    0);
            XPLMCheckMenuItem(
                menu_,
                serialTraceItemIndex_,
                serialTraceEnabled_ ?
                    xplm_Menu_Checked :
                    xplm_Menu_Unchecked);
            XPLMAppendMenuSeparator(menu_);
            XPLMAppendMenuItem(
                menu_,
                "Clear Device Profile Cache",
                menuItemRef(PhoenixMenuAction::ClearProfileCache),
                0);

            debugLog(
                "XPLLink: plugin menu created.\n");
        }

        void destroyMenu()
        {
            if (menu_ != nullptr)
            {
                XPLMDestroyMenu(menu_);
                menu_ = nullptr;
            }

            if (pluginsMenuItemIndex_ >= 0)
            {
                if (XPLMMenuID pluginsMenu = XPLMFindPluginsMenu())
                {
                    XPLMRemoveMenuItem(
                        pluginsMenu,
                        pluginsMenuItemIndex_);
                }

                pluginsMenuItemIndex_ = -1;
            }
        }

        void toggleStatusWindow()
        {
            statusWindow_.toggle();
            autoOpenedStatusWindow_ = false;
        }

        std::vector<std::string> statusWindowLines() const
        {
            std::vector<std::string> lines;

            {
                std::ostringstream line;
                line
                    << "Devices Detected: "
                    << deviceRuntime_.deviceCount()
                    << ", Registered DataRefs: "
                    << deviceRuntime_.dataRefCount()
                    << ", Registered Commands: "
                    << deviceRuntime_.commandCount()
                    << ", tx: "
                    << 0
                    << ", rx: "
                    << 0;
                lines.push_back(line.str());
            }

            {
                std::ostringstream line;
                line
                    << "Pending command triggers: "
                    << bridgeFactory_.pendingCommandTriggerCount()
                    << ", serial trace: "
                    << (serialTraceEnabled_ ? "enabled" : "disabled");
                lines.push_back(line.str());
            }

            lines.push_back(
                "Device runtime: " + engagementStatus_);

            {
                std::ostringstream line;
                line
                    << "Discovery progress: "
                    << portsScanned_
                    << "/"
                    << totalProbePorts_
                    << " port(s), "
                    << devicesDiscoveredDuringEngagement_
                    << " device(s) found, "
                    << deviceRuntime_.updateSubscriptionCount()
                    << " dataref subscription(s)";

                if (activeProbe_)
                {
                    line
                        << ", probing "
                        << activeProbe_->port.portName;
                }

                lines.push_back(line.str());
            }

            const auto telemetry =
                interactionTelemetry_.snapshot();

            lines.push_back(
                "Last Dataref Received: " +
                (telemetry.lastDataRefReceived.empty() ?
                    std::string{ "none" } :
                    telemetry.lastDataRefReceived));
            lines.push_back(
                "Last Dataref Sent: " +
                (telemetry.lastDataRefSent.empty() ?
                    std::string{ "none" } :
                    telemetry.lastDataRefSent));
            lines.push_back(
                "Last Command Action: " +
                (telemetry.lastCommandAction.empty() ?
                    std::string{ "none" } :
                    telemetry.lastCommandAction));
            lines.push_back(
                "Last Lookup Failure: " +
                (telemetry.lastLookupFailure.empty() ?
                    std::string{ "none" } :
                    telemetry.lastLookupFailure));

            {
                std::ostringstream line;
                line
                    << "Profile Cache: "
                    << profileDirectory_.string();
                lines.push_back(line.str());
            }

            {
                std::ostringstream line;
                line
                    << "Serial Trace: "
                    << serialTracePath_.string();
                lines.push_back(line.str());
            }

            lines.emplace_back(
                "Debug Message: status window active");

            return lines;
        }

        void engageDevices()
        {
            if (engagementState_ == DeviceEngagementState::Scanning)
            {
                debugLog(
                    "Phoenix: engage devices requested while discovery is already active.\n");
                return;
            }

            restartDeviceEngagement(
                "manual engage requested");
        }

        void disengageDevices()
        {
            activeProbe_.reset();
            portsToProbe_.clear();
            portsScanned_ = 0;
            totalProbePorts_ = 0;
            devicesDiscoveredDuringEngagement_ = 0;
            engagementCompletedAt_.reset();
            engagementState_ =
                DeviceEngagementState::Idle;
            engagementStatus_ =
                "Devices disengaged.";

            const auto count =
                deviceRuntime_.disengageDevices();
            bridgeFactory_.clearBridges();

            if (autoOpenedStatusWindow_)
            {
                statusWindow_.close();
                autoOpenedStatusWindow_ = false;
            }

            std::ostringstream message;
            message
                << "Phoenix: disengaged "
                << count
                << " device(s).\n";

            debugLog(message.str());
        }

        void processRequestedLifecycleActions()
        {
            if (pendingDisengage_)
            {
                std::ostringstream message;
                message
                    << "Phoenix: disengage requested because "
                    << pendingDisengageReason_
                    << ".\n";
                debugLog(message.str());

                disengageDevices();
                pendingDisengage_ =
                    false;
                pendingDisengageReason_.clear();
            }

            if (pendingAutoEngage_)
            {
                restartDeviceEngagement(
                    pendingAutoEngageReason_);
                pendingAutoEngage_ =
                    false;
                pendingAutoEngageReason_.clear();
            }
        }

        void restartDeviceEngagement(
            std::string_view reason)
        {
            activeProbe_.reset();
            portsToProbe_.clear();
            deviceRuntime_.disengageDevices();
            bridgeFactory_.clearBridges();

            std::ostringstream message;
            message
                << "Phoenix: engaging devices because "
                << reason
                << ".\n";
            debugLog(message.str());

            startDeviceEngagement();
        }

        void startDeviceEngagement()
        {
            if (!statusWindow_.isOpen())
            {
                statusWindow_.open();
                autoOpenedStatusWindow_ = true;
            }
            else
            {
                autoOpenedStatusWindow_ = false;
            }

            portsToProbe_.clear();
            activeProbe_.reset();
            portsScanned_ = 0;
            totalProbePorts_ = 0;
            devicesDiscoveredDuringEngagement_ = 0;
            engagementCompletedAt_.reset();

            const auto ports =
                serialEnumerator_.enumerate();

            for (const auto& port : ports)
            {
                const auto kind =
                    phoenix::serial::SerialDeviceClassifier::classify(port);

                if (kind == phoenix::serial::SerialDeviceKind::Bluetooth ||
                    kind == phoenix::serial::SerialDeviceKind::Suspect ||
                    kind == phoenix::serial::SerialDeviceKind::BuiltInSerial)
                {
                    logSerialPortInformation(
                        port,
                        kind,
                        "skipped");
                    continue;
                }

                auto probePort =
                    port;
                probePort.kind =
                    kind;
                logSerialPortInformation(
                    probePort,
                    kind,
                    "queued for probing");
                portsToProbe_.push_back(
                    std::move(probePort));
            }

            totalProbePorts_ =
                portsToProbe_.size();
            engagementState_ =
                DeviceEngagementState::Scanning;

            std::ostringstream status;
            status
                << "Engaging devices: "
                << portsToProbe_.size()
                << " candidate port(s).";
            engagementStatus_ =
                status.str();

            std::ostringstream message;
            message
                << "Phoenix: engaging devices, "
                << portsToProbe_.size()
                << " candidate serial port(s).\n";
            debugLog(message.str());
        }

        void tickDeviceEngagement()
        {
            const auto now =
                std::chrono::steady_clock::now();

            if (engagementState_ == DeviceEngagementState::Complete)
            {
                maybeAutoCloseStatusWindow(now);
                return;
            }

            if (engagementState_ != DeviceEngagementState::Scanning)
            {
                return;
            }

            if (!activeProbe_)
            {
                startNextProbe(now);
            }

            if (activeProbe_)
            {
                tickActiveProbe(now);
            }

            if (!activeProbe_ && portsToProbe_.empty())
            {
                completeDeviceEngagement(now);
            }
        }

        void startNextProbe(
            std::chrono::steady_clock::time_point now)
        {
            if (portsToProbe_.empty())
            {
                return;
            }

            IncrementalProbe probe;
            probe.port =
                std::move(portsToProbe_.front());
            portsToProbe_.erase(
                portsToProbe_.begin());
            probe.transport =
                transportFactory_.create(
                    probe.port.portName,
                    115200,
                    probe.controlMode);
            probe.deadline =
                now + ProbeOpenSettleDelay + ProbeTimeout;
            probe.nextRequestAt =
                now + ProbeOpenSettleDelay;

            logProbeEvent(
                probe.port,
                probe.controlMode,
                "starting");

            if (!probe.transport->open())
            {
                const auto diagnostic =
                    transportDiagnostic(
                        probe.transport.get());
                logProbeEvent(
                    probe.port,
                    probe.controlMode,
                    "open failed" + diagnostic);

                if (retryProbeWithAlternateControlMode(
                    probe,
                    now,
                    "initial open failed"))
                {
                    return;
                }

                ++portsScanned_;
                engagementStatus_ =
                    "Unable to open " + probe.port.portName + ".";
                return;
            }

            logProbeEvent(
                probe.port,
                probe.controlMode,
                "opened, waiting for settle");

            engagementStatus_ =
                "Waiting for " + probe.port.portName +
                " to settle (" +
                deviceKindName(probe.port.kind) +
                ", " + controlModeName(probe.controlMode) + ").";
            activeProbe_ =
                std::move(probe);
        }

        void markActiveProbeSendingRequests()
        {
            if (!activeProbe_)
            {
                return;
            }

            if (!activeProbe_->requestsStarted)
            {
                activeProbe_->requestsStarted = true;
                engagementStatus_ =
                    "Probing " + activeProbe_->port.portName +
                    " (" + deviceKindName(activeProbe_->port.kind) +
                    ", " + controlModeName(activeProbe_->controlMode) + ").";
            }
        }

        void tickActiveProbe(
            std::chrono::steady_clock::time_point now)
        {
            if (!activeProbe_)
            {
                return;
            }

            if (now >= activeProbe_->nextRequestAt)
            {
                markActiveProbeSendingRequests();
                sendProbeRequest(*activeProbe_);
                activeProbe_->nextRequestAt =
                    now + ProbeRetryInterval;
            }

            if (readProbeResponse(*activeProbe_))
            {
                acceptActiveProbe();
                return;
            }

            if (now >= activeProbe_->deadline)
            {
                const auto portName =
                    activeProbe_->port.portName;

                logProbeEvent(
                    activeProbe_->port,
                    activeProbe_->controlMode,
                    "timed out");

                if (retryActiveProbeWithAlternateControlMode(
                    now,
                    "no response"))
                {
                    return;
                }

                engagementStatus_ =
                    "No XPLLink response from " +
                    portName + ".";
                closeActiveProbe();
            }
        }

        bool retryActiveProbeWithAlternateControlMode(
            std::chrono::steady_clock::time_point now,
            std::string_view reason)
        {
            if (!activeProbe_)
            {
                return false;
            }

            if (activeProbe_->controlMode ==
                phoenix::serial::NativeSerialControlMode::DtrRtsEnabled)
            {
                return false;
            }

            auto probe =
                std::move(*activeProbe_);
            activeProbe_.reset();

            return retryProbeWithAlternateControlMode(
                probe,
                now,
                reason);
        }

        bool retryProbeWithAlternateControlMode(
            IncrementalProbe& probe,
            std::chrono::steady_clock::time_point now,
            std::string_view reason)
        {
            if (probe.controlMode ==
                phoenix::serial::NativeSerialControlMode::DtrRtsEnabled)
            {
                return false;
            }

            if (probe.transport &&
                probe.transport->isOpen())
            {
                logProbeEvent(
                    probe.port,
                    probe.controlMode,
                    "closing before retry");

                probe.transport->close();
            }

            probe.controlMode =
                phoenix::serial::NativeSerialControlMode::DtrRtsEnabled;
            probe.transport =
                transportFactory_.create(
                    probe.port.portName,
                    115200,
                    probe.controlMode);
            probe.parser =
                phoenix::protocol::legacy::LegacyFrameParser{};
            probe.device =
                {};
            probe.deadline =
                now + ProbeOpenSettleDelay + ProbeTimeout;
            probe.nextRequestAt =
                now + ProbeOpenSettleDelay;
            probe.requestsStarted =
                false;

            logProbeEvent(
                probe.port,
                probe.controlMode,
                "retry starting");

            if (!probe.transport->open())
            {
                const auto diagnostic =
                    transportDiagnostic(
                        probe.transport.get());
                logProbeEvent(
                    probe.port,
                    probe.controlMode,
                    "retry open failed" + diagnostic);

                return false;
            }

            logProbeEvent(
                probe.port,
                probe.controlMode,
                "retry opened, waiting for settle");

            engagementStatus_ =
                "Retrying " + probe.port.portName +
                " (" + controlModeName(probe.controlMode) +
                ", " + std::string{ reason } + ").";
            activeProbe_ =
                std::move(probe);

            return true;
        }

        void sendProbeRequest(
            IncrementalProbe& probe)
        {
            const auto request =
                phoenix::protocol::legacy::makeFrame(
                    phoenix::protocol::legacy::sendNameCommand);

            const std::size_t bytesWritten =
                probe.transport->write(request);

            if (serialTrace_.isEnabled() &&
                serialTrace_.isOpen() &&
                bytesWritten > 0)
            {
                serialTrace_.bytes(
                    phoenix::logging::ByteDumpDirection::Transmit,
                    probe.port.portName,
                    std::span<const std::byte>{
                        request.data(),
                        bytesWritten
                    });
            }
        }

        bool readProbeResponse(
            IncrementalProbe& probe)
        {
            std::array<std::byte, 256> buffer{};
            const std::size_t bytesRead =
                probe.transport->read(buffer);

            if (bytesRead > 0)
            {
                std::ostringstream message;
                message
                    << "Phoenix: probe read "
                    << bytesRead
                    << " byte(s) from "
                    << probe.port.portName
                    << " ("
                    << controlModeName(probe.controlMode)
                    << ").\n";
                debugLog(message.str());
            }

            if (serialTrace_.isEnabled() &&
                serialTrace_.isOpen() &&
                bytesRead > 0)
            {
                serialTrace_.bytes(
                    phoenix::logging::ByteDumpDirection::Receive,
                    probe.port.portName,
                    std::span<const std::byte>{
                        buffer.data(),
                        bytesRead
                    });
            }

            const auto frames =
                probe.parser.push(
                    std::span<const std::byte>{
                        buffer.data(),
                        bytesRead
                    });

            for (const auto& frame : frames)
            {
                if (frame.command ==
                    phoenix::protocol::legacy::nameResponseCommand)
                {
                    probe.device.name =
                        extractQuotedString(frame.payload);

                    std::ostringstream message;
                    message
                        << "Phoenix: probe received name from "
                        << probe.port.portName
                        << ": "
                        << probe.device.name
                        << ".\n";
                    debugLog(message.str());
                }
                else if (frame.command ==
                    phoenix::protocol::legacy::versionResponseCommand)
                {
                    probe.device.version =
                        extractQuotedString(frame.payload);

                    std::ostringstream message;
                    message
                        << "Phoenix: probe received version from "
                        << probe.port.portName
                        << ": "
                        << probe.device.version
                        << ".\n";
                    debugLog(message.str());
                }

                if (!probe.device.name.empty())
                {
                    return true;
                }
            }

            return false;
        }

        void acceptActiveProbe()
        {
            if (!activeProbe_)
            {
                return;
            }

            auto transport =
                std::move(activeProbe_->transport);
            const auto portName =
                activeProbe_->port.portName;
            const auto deviceName =
                activeProbe_->device.name;
            const auto deviceVersion =
                activeProbe_->device.version;

            logProbeEvent(
                activeProbe_->port,
                activeProbe_->controlMode,
                "accepted");

            deviceRuntime_.addDevice(
                portName,
                deviceName,
                deviceVersion,
                std::move(transport));

            ++devicesDiscoveredDuringEngagement_;
            ++portsScanned_;

            std::ostringstream status;
            status
                << "Found "
                << deviceName
                << " on "
                << portName
                << ".";
            engagementStatus_ =
                status.str();

            std::ostringstream message;
            message
                << "Phoenix: discovered "
                << deviceName
                << " on "
                << portName
                << ".\n";
            debugLog(message.str());

            activeProbe_.reset();
        }

        void closeActiveProbe()
        {
            if (!activeProbe_)
            {
                return;
            }

            if (activeProbe_->transport &&
                activeProbe_->transport->isOpen())
            {
                logProbeEvent(
                    activeProbe_->port,
                    activeProbe_->controlMode,
                    "closing");

                activeProbe_->transport->close();
            }

            ++portsScanned_;
            activeProbe_.reset();
        }

        void completeDeviceEngagement(
            std::chrono::steady_clock::time_point now)
        {
            engagementState_ =
                DeviceEngagementState::Complete;
            engagementCompletedAt_ =
                now;

            std::ostringstream status;
            status
                << "Engagement complete: "
                << deviceRuntime_.deviceCount()
                << " device(s) active.";
            engagementStatus_ =
                status.str();

            debugLog(
                "Phoenix: device engagement complete.\n");
        }

        void maybeAutoCloseStatusWindow(
            std::chrono::steady_clock::time_point now)
        {
            if (!autoOpenedStatusWindow_ ||
                !engagementCompletedAt_)
            {
                return;
            }

            if (now - *engagementCompletedAt_ >=
                AutoCloseStatusWindowDelay)
            {
                statusWindow_.close();
                autoOpenedStatusWindow_ = false;
            }
        }

        void loadSettings()
        {
            std::ifstream stream{
                settingsPath_
            };

            if (!stream)
            {
                return;
            }

            std::string line;

            while (std::getline(stream, line))
            {
                if (line == "serialTraceEnabled=1")
                {
                    serialTraceEnabled_ = true;
                    serialTrace_.setEnabled(true);
                }
                else if (line == "serialTraceEnabled=0")
                {
                    serialTraceEnabled_ = false;
                    serialTrace_.setEnabled(false);
                }
            }
        }

        void saveSettings() const
        {
            std::ofstream stream{
                settingsPath_,
                std::ios::trunc
            };

            if (!stream)
            {
                return;
            }

            stream
                << "serialTraceEnabled="
                << (serialTraceEnabled_ ? "1" : "0")
                << '\n';
        }

        void toggleSerialTrace()
        {
            serialTraceEnabled_ =
                !serialTraceEnabled_;
            serialTrace_.setEnabled(
                serialTraceEnabled_);
            saveSettings();

            if (menu_ != nullptr && serialTraceItemIndex_ >= 0)
            {
                XPLMCheckMenuItem(
                    menu_,
                    serialTraceItemIndex_,
                    serialTraceEnabled_ ?
                        xplm_Menu_Checked :
                        xplm_Menu_Unchecked);
            }

            debugLog(
                serialTraceEnabled_ ?
                    "Phoenix: serial trace logging enabled from menu.\n" :
                    "Phoenix: serial trace logging disabled from menu.\n");
        }

        void clearProfileCache()
        {
            std::error_code error;
            std::size_t removed = 0;

            if (!std::filesystem::exists(profileDirectory_, error))
            {
                std::ostringstream message;

                message
                    << "Phoenix: profile cache is already empty: "
                    << profileDirectory_.string()
                    << ".\n";

                debugLog(message.str());
                return;
            }

            for (const auto& entry :
                std::filesystem::directory_iterator{
                    profileDirectory_,
                    error
                })
            {
                if (error)
                {
                    break;
                }

                if (!entry.is_regular_file(error) ||
                    entry.path().extension() != ".json")
                {
                    continue;
                }

                if (std::filesystem::remove(entry.path(), error))
                {
                    ++removed;
                }
            }

            std::ostringstream message;

            if (error)
            {
                message
                    << "Phoenix: unable to clear profile cache: "
                    << error.message()
                    << ".\n";
            }
            else
            {
                message
                    << "Phoenix: cleared "
                    << removed
                    << " profile cache file(s) from "
                    << profileDirectory_.string()
                    << ".\n";
            }

            debugLog(message.str());
        }

        phoenix::xplane::sdk::XPlaneSdkApi api_;
        PluginInteractionTelemetry interactionTelemetry_;
        phoenix::xplane::sdk::XPlaneSdkBridgeFactory bridgeFactory_{
            api_,
            &interactionTelemetry_
        };
        PluginLegacyDeviceObserverFactory observerFactory_{
            [this](
                std::string_view name,
                std::string_view value,
                std::optional<int> element)
            {
                interactionTelemetry_.dataRefSentToDevice(
                    name,
                    value,
                    element);
            }
        };
        std::filesystem::path outputDirectory_;
        std::filesystem::path profileDirectory_;
        std::filesystem::path serialTracePath_;
        std::filesystem::path settingsPath_;
        ToggleableSerialTraceSink serialTrace_;
        phoenix::runtime::DeviceRuntimeManager deviceRuntime_;
        phoenix::serial::NativeSerialEnumerator serialEnumerator_;
        phoenix::serial::NativeSerialTransportFactory transportFactory_;
        phoenix::plugin::PhoenixStatusWindow statusWindow_;
        std::vector<phoenix::serial::SerialPortInfo> portsToProbe_;
        std::optional<IncrementalProbe> activeProbe_;
        DeviceEngagementState engagementState_ =
            DeviceEngagementState::Idle;
        std::string engagementStatus_ =
            "Idle.";
        std::size_t totalProbePorts_ = 0;
        std::size_t portsScanned_ = 0;
        std::size_t devicesDiscoveredDuringEngagement_ = 0;
        bool autoOpenedStatusWindow_ = false;
        std::optional<std::chrono::steady_clock::time_point>
            engagementCompletedAt_;
        bool pendingAutoEngage_ = false;
        std::string pendingAutoEngageReason_;
        bool pendingDisengage_ = false;
        std::string pendingDisengageReason_;
        XPLMMenuID menu_ = nullptr;
        int pluginsMenuItemIndex_ = -1;
        int serialTraceItemIndex_ = -1;
        bool serialTraceEnabled_ = false;
    };

    std::unique_ptr<PhoenixPluginRuntime> runtime;

    float phoenixFlightLoopCallback(
        float,
        float,
        int,
        void*)
    {
        if (runtime)
        {
            return runtime->flightLoop();
        }

        return 0.0f;
    }

    void copyPluginString(
        char* target,
        const char* value)
    {
        std::strncpy(target, value, 255);
        target[255] = '\0';
    }
}

extern "C"
{
    PLUGIN_API int XPluginStart(
        char* outName,
        char* outSig,
        char* outDesc)
    {
        copyPluginString(outName, PluginDisplayName);
        copyPluginString(outSig, PluginSignature);
        copyPluginString(outDesc, PluginDescription);

        runtime =
            std::make_unique<PhoenixPluginRuntime>();

        XPLMRegisterFlightLoopCallback(
            phoenixFlightLoopCallback,
            -1.0f,
            nullptr);

        runtime->requestAutoEngage(
            "plugin startup");

        XPLMDebugString(
            "XPLLink: plugin started.\n");

        return 1;
    }

    PLUGIN_API void XPluginStop()
    {
        XPLMUnregisterFlightLoopCallback(
            phoenixFlightLoopCallback,
            nullptr);

        runtime.reset();

        XPLMDebugString(
            "XPLLink: plugin stopped.\n");
    }

    PLUGIN_API int XPluginEnable()
    {
        XPLMDebugString(
            "XPLLink: plugin enabled.\n");
        return 1;
    }

    PLUGIN_API void XPluginDisable()
    {
        XPLMDebugString(
            "XPLLink: plugin disabled.\n");
    }

    PLUGIN_API void XPluginReceiveMessage(
        XPLMPluginID,
        int message,
        void* parameter)
    {
        if (runtime)
        {
            runtime->handleXPlaneMessage(
                message,
                parameter);
        }
    }
}
