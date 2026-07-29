#include <xpllink/runtime/DeviceRuntimeManager.h>

#include <xpllink/logging/Log.h>
#include <xpllink/profile/JsonDeviceProfileStore.h>
#include <xpllink/protocol/legacy/LegacyFrame.h>
#include <xpllink/runtime/LegacyDeviceSession.h>

#include <filesystem>
#include <sstream>
#include <thread>
#include <utility>

namespace xpllink::runtime
{
    struct DeviceRuntimeManager::DeviceContext
    {
        DeviceContext(
            std::string portNameValue,
            std::string deviceNameValue,
            std::string deviceVersionValue,
            std::unique_ptr<transport::IByteTransport> transportValue,
            logging::ISerialTraceSink& serialTrace,
            IXPlaneBridgeFactory& bridgeFactory,
            ILegacyDeviceObserverFactory* observerFactory,
            const DeviceRuntimeManagerOptions& options)
            : portName(std::move(portNameValue))
            , deviceName(std::move(deviceNameValue))
            , deviceVersion(std::move(deviceVersionValue))
            , transport(std::move(transportValue))
            , xplane(
                bridgeFactory.createBridge(
                    portName,
                    deviceName,
                    deviceVersion))
            , observer(
                observerFactory == nullptr ?
                    nullptr :
                    observerFactory->createObserver(
                        portName,
                        deviceName,
                        deviceVersion))
            , session(
                std::make_unique<LegacyDeviceSession>(
                    *transport,
                    LegacyDeviceSessionOptions{
                        .readBufferSize = options.readBufferSize,
                        .maximumReadPasses = options.maximumReadPasses,
                        .serialTrace = &serialTrace,
                        .tracePortName = portName
                    }))
        {
            if (observer)
            {
                controller =
                    std::make_unique<LegacyDeviceController>(
                        *session,
                        *xplane,
                        *observer);
            }
            else
            {
                controller =
                    std::make_unique<LegacyDeviceController>(
                        *session,
                        *xplane);
            }

            updateScheduler =
                std::make_unique<LegacyUpdateScheduler>(
                    *session,
                    *controller,
                    *xplane,
                    options.updateScheduler);
        }

        ~DeviceContext()
        {
            if (transport && transport->isOpen())
            {
                transport->close();
            }
        }

        std::string portName;
        std::string deviceName;
        std::string deviceVersion;
        std::unique_ptr<transport::IByteTransport> transport;
        std::unique_ptr<xplane::IXPlaneBridge> xplane;
        std::unique_ptr<ILegacyDeviceObserver> observer;
        std::unique_ptr<LegacyDeviceSession> session;
        std::unique_ptr<LegacyDeviceController> controller;
        std::unique_ptr<LegacyUpdateScheduler> updateScheduler;
        bool profileAccepted = false;
    };

    namespace
    {
        void logTickActivity(
            const DeviceRuntimeManager::DeviceContext& device,
            const LegacyDeviceControllerTickResult& tick)
        {
            if (tick.session.bytesRead == 0 &&
                tick.session.bytesWritten == 0 &&
                tick.messagesProcessed == 0 &&
                tick.responseBytesWritten == 0)
            {
                return;
            }

            std::ostringstream message;

            message
                << "    "
                << device.portName
                << " tick: read "
                << tick.session.bytesRead
                << " byte(s), processed "
                << tick.messagesProcessed
                << " message(s), wrote "
                << tick.session.bytesWritten +
                    tick.responseBytesWritten
                << " byte(s).\n";

            logging::info(message.str());
        }

        void logUpdateSchedulerActivity(
            const DeviceRuntimeManager::DeviceContext& device,
            const LegacyUpdateSchedulerTickResult& tick)
        {
            if (tick.updatesQueued == 0 && tick.bytesWritten == 0)
            {
                return;
            }

            std::ostringstream message;

            message
                << "    "
                << device.portName
                << " update scheduler: queued "
                << tick.updatesQueued
                << " update(s), wrote "
                << tick.bytesWritten
                << " byte(s)";

            if (tick.budgetExhausted)
            {
                message << ", budget exhausted";
            }

            message << ".\n";

            logging::info(message.str());
        }

        profile::DeviceProfile buildProfile(
            const DeviceRuntimeManager::DeviceContext& device)
        {
            profile::DeviceProfile profile;
            profile.cacheSchemaVersion = 1;
            profile.deviceName = device.deviceName;
            profile.deviceVersion = device.deviceVersion;

            for (const auto& binding :
                device.controller->dataRefs())
            {
                profile::DeviceProfileDataRef dataRef;
                dataRef.handle = binding.handle;
                dataRef.name = binding.name;
                dataRef.xplaneType = binding.xplaneType;
                dataRef.active = binding.active;
                dataRef.scalingActive = binding.scalingActive;
                dataRef.scaleFromLow = binding.scaleFromLow;
                dataRef.scaleFromHigh = binding.scaleFromHigh;
                dataRef.scaleToLow = binding.scaleToLow;
                dataRef.scaleToHigh = binding.scaleToHigh;

                for (const auto& subscription :
                    device.controller->updateSubscriptions())
                {
                    if (subscription.handle != binding.handle)
                    {
                        continue;
                    }

                    dataRef.updates.push_back({
                        subscription.handle,
                        subscription.requestedType,
                        subscription.rate,
                        subscription.precision,
                        subscription.element
                    });
                }

                profile.dataRefs.push_back(std::move(dataRef));
            }

            for (const auto& binding :
                device.controller->commands())
            {
                profile.commands.push_back({
                    binding.handle,
                    binding.name,
                    binding.active
                });
            }

            return profile;
        }

        std::string profileAcceptedPayload(
            std::size_t dataRefCount,
            std::size_t commandCount)
        {
            std::ostringstream payload;

            payload
                << ','
                << dataRefCount
                << ','
                << commandCount;

            return payload.str();
        }

        bool tryAcceptStoredProfile(
            const profile::JsonDeviceProfileStore& store,
            DeviceRuntimeManager::DeviceContext& device)
        {
            profile::DeviceProfile identity;
            identity.deviceName = device.deviceName;
            identity.deviceVersion = device.deviceVersion;

            const auto path =
                store.profilePathFor(identity);

            const auto loadedProfile =
                store.load(path);

            if (!loadedProfile)
            {
                std::ostringstream message;

                message
                    << "  No stored profile matched "
                    << device.portName
                    << " ("
                    << device.deviceName
                    << ", "
                    << device.deviceVersion
                    << ").\n";

                logging::info(message.str());
                return false;
            }

            if (loadedProfile->cacheSchemaVersion != 1)
            {
                std::ostringstream message;

                message
                    << "  Stored profile uses an older cache schema for "
                    << device.portName
                    << "; using live registration.\n";

                logging::warning(message.str());
                return false;
            }

            if (!device.controller->tryLoadProfile(*loadedProfile))
            {
                std::ostringstream message;

                message
                    << "  Stored profile could not be resolved for "
                    << device.portName
                    << "; using legacy registration.\n";

                logging::warning(message.str());
                return false;
            }

            device.profileAccepted = true;
            device.session->queueFrame(
                protocol::legacy::profileAcceptedCommand,
                profileAcceptedPayload(
                    loadedProfile->dataRefs.size(),
                    loadedProfile->commands.size()));

            std::ostringstream message;

            message
                << "  Stored profile accepted for "
                << device.portName
                << ": "
                << loadedProfile->dataRefs.size()
                << " dataref(s), "
                << loadedProfile->commands.size()
                << " command(s).\n";

            logging::info(message.str());
            return true;
        }

        void saveProfile(
            const profile::JsonDeviceProfileStore& store,
            const DeviceRuntimeManager::DeviceContext& device)
        {
            const auto profile =
                buildProfile(device);

            const auto path =
                store.profilePathFor(profile);

            std::ostringstream message;

            if (device.controller->hasRegistrationFailures())
            {
                std::error_code error;
                std::filesystem::remove(path, error);

                message
                    << "  Profile caching disabled for "
                    << device.portName
                    << " because one or more requested bindings were unavailable.\n";

                logging::warning(message.str());
                return;
            }

            if (store.save(profile))
            {
                message
                    << "  Saved device profile for "
                    << device.portName
                    << ": "
                    << path.string()
                    << '\n';

                logging::info(message.str());
            }
            else
            {
                message
                    << "  Unable to save device profile for "
                    << device.portName
                    << ": "
                    << path.string()
                    << '\n';

                logging::warning(message.str());
            }
        }
    }

    DeviceRuntimeManager::DeviceRuntimeManager(
        logging::ISerialTraceSink& serialTrace,
        IXPlaneBridgeFactory& bridgeFactory,
        DeviceRuntimeManagerOptions options,
        ILegacyDeviceObserverFactory* observerFactory)
        : serialTrace_(serialTrace)
        , bridgeFactory_(bridgeFactory)
        , observerFactory_(observerFactory)
        , options_(std::move(options))
    {
    }

    DeviceRuntimeManager::~DeviceRuntimeManager() = default;

    void DeviceRuntimeManager::addDevice(
        std::string portName,
        std::string deviceName,
        std::string deviceVersion,
        std::unique_ptr<transport::IByteTransport> transport)
    {
        auto device = std::make_unique<DeviceContext>(
            std::move(portName),
            std::move(deviceName),
            std::move(deviceVersion),
            std::move(transport),
            serialTrace_,
            bridgeFactory_,
            observerFactory_,
            options_);

        std::ostringstream message;

        message
            << "  Queued "
            << device->portName
            << " for the runtime loop.\n";

        logging::info(message.str());

        const profile::JsonDeviceProfileStore profileStore{
            options_.profileDirectory
        };

        const bool profileAccepted =
            tryAcceptStoredProfile(
            profileStore,
            *device);

        if (!profileAccepted)
        {
            device->controller->requestRegistrations();
        }

        devices_.push_back(std::move(device));
    }

    void DeviceRuntimeManager::tick(
        std::chrono::steady_clock::time_point now)
    {
        for (auto& device : devices_)
        {
            const auto tickResult =
                device->controller->tick();

            logTickActivity(*device, tickResult);

            const auto updateTick =
                device->updateScheduler->tick(now);

            logUpdateSchedulerActivity(*device, updateTick);

            if (device->observer &&
                !updateTick.lastDataRefName.empty())
            {
                device->observer->dataRefSentToDevice(
                    updateTick.lastDataRefName,
                    updateTick.lastDataRefValue,
                    updateTick.lastDataRefElement);
            }
        }
    }

    void DeviceRuntimeManager::runFor(
        std::chrono::steady_clock::duration duration)
    {
        if (devices_.empty())
        {
            return;
        }

        {
            std::ostringstream message;

            message
                << "\nRunning runtime loop for "
                << devices_.size()
                << " device(s)...\n";

            logging::info(message.str());
        }

        const auto deadline =
            std::chrono::steady_clock::now() +
            duration;

        while (std::chrono::steady_clock::now() < deadline)
        {
            tick(std::chrono::steady_clock::now());

            std::this_thread::sleep_for(
                std::chrono::milliseconds{ 10 });
        }

        logging::info(
            "  Runtime loop complete.\n");

        saveProfiles();
    }

    void DeviceRuntimeManager::saveProfiles() const
    {
        const profile::JsonDeviceProfileStore profileStore{
            options_.profileDirectory
        };

        for (const auto& device : devices_)
        {
            std::ostringstream summary;

            summary
                << "  "
                << device->portName
                << " registered datarefs: "
                << device->controller->dataRefs().size()
                << '\n'
                << "  "
                << device->portName
                << " registered commands: "
                << device->controller->commands().size()
                << '\n'
                << "  "
                << device->portName
                << " update subscriptions: "
                << device->controller->updateSubscriptions().size()
                << '\n';

            logging::info(summary.str());
            saveProfile(profileStore, *device);
        }
    }

    std::size_t DeviceRuntimeManager::disengageDevices()
    {
        const std::size_t count =
            devices_.size();

        if (count == 0)
        {
            return 0;
        }

        saveProfiles();

        for (auto& device : devices_)
        {
            if (device->session && device->transport &&
                device->transport->isOpen())
            {
                device->session->queueFrame(
                    protocol::legacy::exitingCommand);
                device->session->flushPendingOutput();
                device->transport->close();
            }
        }

        devices_.clear();

        std::ostringstream message;

        message
            << "  Disengaged "
            << count
            << " device(s).\n";

        logging::info(message.str());

        return count;
    }

    std::size_t DeviceRuntimeManager::deviceCount() const
    {
        return devices_.size();
    }

    std::size_t DeviceRuntimeManager::dataRefCount() const
    {
        std::size_t count = 0;

        for (const auto& device : devices_)
        {
            count += device->controller->dataRefs().size();
        }

        return count;
    }

    std::size_t DeviceRuntimeManager::commandCount() const
    {
        std::size_t count = 0;

        for (const auto& device : devices_)
        {
            count += device->controller->commands().size();
        }

        return count;
    }

    std::size_t DeviceRuntimeManager::updateSubscriptionCount() const
    {
        std::size_t count = 0;

        for (const auto& device : devices_)
        {
            count += device->controller->updateSubscriptions().size();
        }

        return count;
    }
}
