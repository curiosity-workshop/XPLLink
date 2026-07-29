#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace xpllink::xplane
{
    enum DataRefType
    {
        DataRefTypeUnknown = 0,
        DataRefTypeInt = 1,
        DataRefTypeFloat = 2,
        DataRefTypeDouble = 4,
        DataRefTypeFloatArray = 8,
        DataRefTypeIntArray = 16,
        DataRefTypeData = 32
    };

    struct DataRefLookupResult
    {
        bool found = false;
        int type = DataRefTypeUnknown;
    };

    struct CommandLookupResult
    {
        bool found = false;
    };

    struct DataRefWrite
    {
        std::string name;
        int handle = -1;
        int valueType = DataRefTypeUnknown;
        std::string value;
        std::optional<int> element;
    };

    struct DataRefReadRequest
    {
        std::string name;
        int handle = -1;
        int xplaneType = DataRefTypeUnknown;
        std::optional<int> preferredType;
        std::optional<int> element;
    };

    struct DataRefReadResult
    {
        bool found = false;
        int valueType = DataRefTypeUnknown;
        std::string value;
        std::optional<int> element;
    };

    class IXPlaneBridge
    {
    public:
        virtual ~IXPlaneBridge() = default;

        virtual DataRefLookupResult findDataRef(
            std::string_view name) = 0;

        virtual CommandLookupResult findCommand(
            std::string_view name) = 0;

        virtual void writeDataRef(
            const DataRefWrite& write) = 0;

        virtual DataRefReadResult readDataRef(
            const DataRefReadRequest& request) = 0;

        virtual void touchDataRef(
            std::string_view name,
            int handle) = 0;

        virtual void commandBegin(
            std::string_view name,
            int handle) = 0;

        virtual void commandEnd(
            std::string_view name,
            int handle) = 0;

        virtual void commandTrigger(
            std::string_view name,
            int handle,
            int triggerCount) = 0;

        virtual void debugMessage(
            std::string_view message) = 0;

        virtual void speak(
            std::string_view message) = 0;

        virtual void resetRequested() = 0;
    };
}
