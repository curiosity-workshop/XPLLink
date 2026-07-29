#include <xpllink/dev/DevelopmentDeviceLoop.h>

#include <xpllink/logging/Log.h>
#include <xpllink/protocol/legacy/LegacyMessage.h>
#include <xpllink/xplane/IXPlaneBridge.h>

#include <chrono>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

namespace xpllink::dev
{
    namespace
    {
        class DevelopmentXPlaneBridge final
            : public xplane::IXPlaneBridge
        {
        public:
            xplane::DataRefLookupResult findDataRef(
                std::string_view name) override
            {
                std::ostringstream message;

                message
                    << "    X-Plane emulator accepted dataref: "
                    << name
                    << '\n';

                logging::info(message.str());

                return {
                    true,
                    xplane::DataRefTypeInt |
                    xplane::DataRefTypeFloat |
                    xplane::DataRefTypeIntArray |
                    xplane::DataRefTypeFloatArray |
                    xplane::DataRefTypeData
                };
            }

            xplane::CommandLookupResult findCommand(
                std::string_view name) override
            {
                std::ostringstream message;

                message
                    << "    X-Plane emulator accepted command: "
                    << name
                    << '\n';

                logging::info(message.str());

                return { true };
            }

            void writeDataRef(
                const xplane::DataRefWrite& write) override
            {
                std::ostringstream message;

                message
                    << "    Device wrote dataref "
                    << write.handle
                    << " ("
                    << write.name
                    << ") = "
                    << write.value;

                if (write.element)
                {
                    message
                        << " element "
                        << *write.element;
                }

                message << '\n';

                logging::info(message.str());
            }

            xplane::DataRefReadResult readDataRef(
                const xplane::DataRefReadRequest& request) override
            {
                const auto now =
                    std::chrono::steady_clock::now().time_since_epoch();
                const auto seconds =
                    std::chrono::duration_cast<std::chrono::seconds>(now);
                const bool state =
                    (seconds.count() / 2) % 2 == 0;

                const int valueType =
                    request.preferredType.value_or(xplane::DataRefTypeInt);

                if (valueType == xplane::DataRefTypeFloat ||
                    valueType == xplane::DataRefTypeDouble ||
                    valueType == xplane::DataRefTypeFloatArray)
                {
                    return {
                        true,
                        valueType,
                        state ? "1.0000" : "0.0000",
                        request.element
                    };
                }

                if (valueType == xplane::DataRefTypeData)
                {
                    return {
                        true,
                        valueType,
                        state ? "1" : "0",
                        request.element
                    };
                }

                return {
                    true,
                    request.element ?
                        xplane::DataRefTypeIntArray :
                        xplane::DataRefTypeInt,
                    state ? "1" : "0",
                    request.element
                };
            }

            void touchDataRef(
                std::string_view name,
                int handle) override
            {
                std::ostringstream message;

                message
                    << "    Device requested dataref refresh "
                    << handle
                    << " ("
                    << name
                    << ")\n";

                logging::info(message.str());
            }

            void commandBegin(
                std::string_view name,
                int handle) override
            {
                logCommandAction("begin", name, handle);
            }

            void commandEnd(
                std::string_view name,
                int handle) override
            {
                logCommandAction("end", name, handle);
            }

            void commandTrigger(
                std::string_view name,
                int handle,
                int triggerCount) override
            {
                std::ostringstream message;

                message
                    << "    Device triggered command "
                    << handle
                    << " ("
                    << name
                    << ") "
                    << triggerCount
                    << " time(s)\n";

                logging::info(message.str());
            }

            void debugMessage(
                std::string_view messageText) override
            {
                std::ostringstream message;

                message
                    << "    Device debug: "
                    << messageText
                    << '\n';

                logging::info(message.str());
            }

            void speak(
                std::string_view messageText) override
            {
                std::ostringstream message;

                message
                    << "    Device speak request: "
                    << messageText
                    << '\n';

                logging::info(message.str());
            }

            void resetRequested() override
            {
                logging::info(
                    "    Device requested reset.\n");
            }

        private:
            void logCommandAction(
                std::string_view action,
                std::string_view name,
                int handle)
            {
                std::ostringstream message;

                message
                    << "    Device command "
                    << action
                    << ' '
                    << handle
                    << " ("
                    << name
                    << ")\n";

                logging::info(message.str());
            }
        };

        class DevelopmentXPlaneBridgeFactory final
            : public runtime::IXPlaneBridgeFactory
        {
        public:
            std::unique_ptr<xplane::IXPlaneBridge> createBridge(
                std::string_view,
                std::string_view,
                std::string_view) override
            {
                return std::make_unique<DevelopmentXPlaneBridge>();
            }
        };

        class DevelopmentLegacyDeviceObserver final
            : public runtime::ILegacyDeviceObserver
        {
        public:
            void microcontrollerRequestedDataRef(
                std::string_view name) override
            {
                std::ostringstream message;

                message
                    << "    Microcontroller requested dataref: "
                    << name
                    << '\n';

                logging::info(message.str());
            }

            void microcontrollerRequestedCommand(
                std::string_view name) override
            {
                std::ostringstream message;

                message
                    << "    Microcontroller requested command: "
                    << name
                    << '\n';

                logging::info(message.str());
            }

            void microcontrollerRequestedUpdates(
                const protocol::legacy::UpdatesRequest& request) override
            {
                std::ostringstream message;

                message
                    << "    Microcontroller requested updates for handle "
                    << request.handle
                    << " every "
                    << request.rate
                    << " ms, precision "
                    << request.precision;

                if (request.requestedType)
                {
                    message
                        << ", type "
                        << *request.requestedType;
                }

                if (request.element)
                {
                    message
                        << ", element "
                        << *request.element;
                }

                message << '\n';

                logging::info(message.str());
            }

            void microcontrollerRequestedScaling(
                const protocol::legacy::ScalingRequest& request) override
            {
                std::ostringstream message;

                message
                    << "    Microcontroller requested scaling for handle "
                    << request.handle
                    << ": "
                    << request.fromLow
                    << "-"
                    << request.fromHigh
                    << " -> "
                    << request.toLow
                    << "-"
                    << request.toHigh
                    << '\n';

                logging::info(message.str());
            }

            void dataRefReceivedFromDevice(
                std::string_view name,
                std::string_view serialValue,
                std::string_view xplaneValue,
                std::optional<int> element) override
            {
                std::ostringstream message;

                message
                    << "    Device sent dataref update: "
                    << name
                    << ", serial "
                    << serialValue
                    << " -> X-Plane "
                    << xplaneValue;

                if (element)
                {
                    message
                        << ", element "
                        << *element;
                }

                message << '\n';

                logging::info(message.str());
            }

            void dataRefSentToDevice(
                std::string_view name,
                std::string_view value,
                std::optional<int> element) override
            {
                std::ostringstream message;

                message
                    << "    Device received dataref update: "
                    << name
                    << " = "
                    << value;

                if (element)
                {
                    message
                        << ", element "
                        << *element;
                }

                message << '\n';

                logging::info(message.str());
            }
        };

        class DevelopmentLegacyDeviceObserverFactory final
            : public runtime::ILegacyDeviceObserverFactory
        {
        public:
            std::unique_ptr<runtime::ILegacyDeviceObserver> createObserver(
                std::string_view,
                std::string_view,
                std::string_view) override
            {
                return std::make_unique<DevelopmentLegacyDeviceObserver>();
            }
        };
    }

    struct DevelopmentDeviceManager::Implementation
    {
        Implementation(
            logging::ISerialTraceSink& serialTrace,
            std::filesystem::path profileDirectory)
            : runtime(
                serialTrace,
                bridgeFactory,
                runtime::DeviceRuntimeManagerOptions{
                    .profileDirectory = std::move(profileDirectory),
                    .updateScheduler = runtime::LegacyUpdateSchedulerOptions{
                        .maxFramesPerTick = 8,
                        .maxBytesPerTick = 64
                    },
                    .readBufferSize = 256,
                    .maximumReadPasses = 16
                },
                &observerFactory)
        {
        }

        DevelopmentXPlaneBridgeFactory bridgeFactory;
        DevelopmentLegacyDeviceObserverFactory observerFactory;
        runtime::DeviceRuntimeManager runtime;
    };

    DevelopmentDeviceManager::DevelopmentDeviceManager(
        logging::ISerialTraceSink& serialTrace,
        std::filesystem::path profileDirectory)
        : implementation_(
            std::make_unique<Implementation>(
                serialTrace,
                std::move(profileDirectory)))
    {
    }

    DevelopmentDeviceManager::~DevelopmentDeviceManager() = default;

    void DevelopmentDeviceManager::addDevice(
        std::string portName,
        std::string deviceName,
        std::string deviceVersion,
        std::unique_ptr<transport::IByteTransport> transport)
    {
        implementation_->runtime.addDevice(
            std::move(portName),
            std::move(deviceName),
            std::move(deviceVersion),
            std::move(transport));
    }

    std::size_t DevelopmentDeviceManager::deviceCount() const
    {
        return implementation_->runtime.deviceCount();
    }

    void DevelopmentDeviceManager::runFor(
        std::chrono::steady_clock::duration duration)
    {
        implementation_->runtime.runFor(duration);
    }

    std::size_t DevelopmentDeviceManager::disengageDevices()
    {
        return implementation_->runtime.disengageDevices();
    }
}
