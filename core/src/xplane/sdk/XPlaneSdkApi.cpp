#include <xpllink/xplane/sdk/XPlaneSdkApi.h>

#include <XPLMUtilities.h>
#include <XPLMDataAccess.h>

#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace xpllink::xplane::sdk
{
    namespace
    {
        XPLMDataRef toDataRef(
            NativeDataRefHandle handle)
        {
            return reinterpret_cast<XPLMDataRef>(handle);
        }

        XPLMCommandRef toCommandRef(
            NativeCommandHandle handle)
        {
            return reinterpret_cast<XPLMCommandRef>(handle);
        }

        NativeDataRefHandle fromDataRef(
            XPLMDataRef handle)
        {
            return reinterpret_cast<NativeDataRefHandle>(handle);
        }

        NativeCommandHandle fromCommandRef(
            XPLMCommandRef handle)
        {
            return reinterpret_cast<NativeCommandHandle>(handle);
        }

        std::string nullTerminated(
            std::string_view value)
        {
            return std::string{ value };
        }

        bool supportsType(
            int xplaneType,
            int requestedType)
        {
            return (xplaneType & requestedType) != 0;
        }

        int requestedType(
            const DataRefReadRequest& request)
        {
            if (request.preferredType &&
                supportsType(
                    request.xplaneType,
                    *request.preferredType))
            {
                return *request.preferredType;
            }

            if (request.element)
            {
                if ((request.xplaneType & DataRefTypeIntArray) != 0)
                {
                    return DataRefTypeIntArray;
                }

                if ((request.xplaneType & DataRefTypeFloatArray) != 0)
                {
                    return DataRefTypeFloatArray;
                }
            }

            if ((request.xplaneType & DataRefTypeInt) != 0)
            {
                return DataRefTypeInt;
            }

            if ((request.xplaneType & DataRefTypeFloat) != 0)
            {
                return DataRefTypeFloat;
            }

            if ((request.xplaneType & DataRefTypeDouble) != 0)
            {
                return DataRefTypeDouble;
            }

            if ((request.xplaneType & DataRefTypeData) != 0)
            {
                return DataRefTypeData;
            }

            return DataRefTypeUnknown;
        }

        std::string formatFloat(
            double value)
        {
            std::ostringstream output;
            output
                << std::setprecision(8)
                << value;
            return output.str();
        }

        int intValue(std::string_view value)
        {
            return std::atoi(std::string{ value }.c_str());
        }

        float floatValue(std::string_view value)
        {
            return static_cast<float>(
                std::atof(std::string{ value }.c_str()));
        }

        double doubleValue(std::string_view value)
        {
            return std::atof(std::string{ value }.c_str());
        }

        int nonNegativeElement(
            const std::optional<int>& element)
        {
            return (std::max)(
                0,
                element.value_or(0));
        }
    }

    NativeDataRefLookup XPlaneSdkApi::findDataRef(
        std::string_view name)
    {
        const auto stableName =
            nullTerminated(name);
        const XPLMDataRef dataRef =
            XPLMFindDataRef(stableName.c_str());

        if (dataRef == nullptr)
        {
            return {};
        }

        return {
            fromDataRef(dataRef),
            XPLMGetDataRefTypes(dataRef)
        };
    }

    NativeCommandLookup XPlaneSdkApi::findCommand(
        std::string_view name)
    {
        const auto stableName =
            nullTerminated(name);
        const XPLMCommandRef command =
            XPLMFindCommand(stableName.c_str());

        if (command == nullptr)
        {
            return {};
        }

        return {
            fromCommandRef(command)
        };
    }

    void XPlaneSdkApi::writeDataRef(
        NativeDataRefHandle handle,
        const DataRefWrite& write)
    {
        const XPLMDataRef dataRef =
            toDataRef(handle);

        if (dataRef == nullptr)
        {
            return;
        }

        switch (write.valueType)
        {
        case DataRefTypeInt:
            XPLMSetDatai(dataRef, intValue(write.value));
            break;

        case DataRefTypeFloat:
            XPLMSetDataf(dataRef, floatValue(write.value));
            break;

        case DataRefTypeDouble:
            XPLMSetDatad(dataRef, doubleValue(write.value));
            break;

        case DataRefTypeIntArray:
        {
            int value =
                intValue(write.value);
            XPLMSetDatavi(
                dataRef,
                &value,
                nonNegativeElement(write.element),
                1);
            break;
        }

        case DataRefTypeFloatArray:
        {
            float value =
                floatValue(write.value);
            XPLMSetDatavf(
                dataRef,
                &value,
                nonNegativeElement(write.element),
                1);
            break;
        }

        case DataRefTypeData:
            XPLMSetDatab(
                dataRef,
                const_cast<char*>(write.value.data()),
                0,
                static_cast<int>(write.value.size()));
            break;

        default:
            break;
        }
    }

    DataRefReadResult XPlaneSdkApi::readDataRef(
        NativeDataRefHandle handle,
        const DataRefReadRequest& request)
    {
        const XPLMDataRef dataRef =
            toDataRef(handle);

        if (dataRef == nullptr)
        {
            return {};
        }

        const int type =
            requestedType(request);

        switch (type)
        {
        case DataRefTypeInt:
            return {
                true,
                DataRefTypeInt,
                std::to_string(XPLMGetDatai(dataRef)),
                std::nullopt
            };

        case DataRefTypeFloat:
            return {
                true,
                DataRefTypeFloat,
                formatFloat(XPLMGetDataf(dataRef)),
                std::nullopt
            };

        case DataRefTypeDouble:
            return {
                true,
                DataRefTypeDouble,
                formatFloat(XPLMGetDatad(dataRef)),
                std::nullopt
            };

        case DataRefTypeIntArray:
        {
            int value = 0;
            XPLMGetDatavi(
                dataRef,
                &value,
                nonNegativeElement(request.element),
                1);

            return {
                true,
                DataRefTypeIntArray,
                std::to_string(value),
                nonNegativeElement(request.element)
            };
        }

        case DataRefTypeFloatArray:
        {
            float value = 0.0f;
            XPLMGetDatavf(
                dataRef,
                &value,
                nonNegativeElement(request.element),
                1);

            return {
                true,
                DataRefTypeFloatArray,
                formatFloat(value),
                nonNegativeElement(request.element)
            };
        }

        case DataRefTypeData:
        {
            const int byteCount =
                XPLMGetDatab(dataRef, nullptr, 0, 0);

            if (byteCount <= 0)
            {
                return {
                    true,
                    DataRefTypeData,
                    {},
                    std::nullopt
                };
            }

            std::vector<char> bytes(
                static_cast<std::size_t>(byteCount));
            XPLMGetDatab(
                dataRef,
                bytes.data(),
                0,
                byteCount);

            return {
                true,
                DataRefTypeData,
                std::string{ bytes.begin(), bytes.end() },
                std::nullopt
            };
        }

        default:
            return {};
        }
    }

    void XPlaneSdkApi::touchDataRef(
        NativeDataRefHandle handle,
        std::string_view,
        int)
    {
        const XPLMDataRef dataRef =
            toDataRef(handle);

        if (dataRef != nullptr)
        {
            static_cast<void>(XPLMGetDataRefTypes(dataRef));
        }
    }

    void XPlaneSdkApi::commandBegin(
        NativeCommandHandle handle)
    {
        if (const XPLMCommandRef command = toCommandRef(handle))
        {
            XPLMCommandBegin(command);
        }
    }

    void XPlaneSdkApi::commandEnd(
        NativeCommandHandle handle)
    {
        if (const XPLMCommandRef command = toCommandRef(handle))
        {
            XPLMCommandEnd(command);
        }
    }

    void XPlaneSdkApi::commandOnce(
        NativeCommandHandle handle)
    {
        if (const XPLMCommandRef command = toCommandRef(handle))
        {
            XPLMCommandOnce(command);
        }
    }

    void XPlaneSdkApi::debugMessage(
        std::string_view message)
    {
        const auto stableMessage =
            nullTerminated(message);
        XPLMDebugString(stableMessage.c_str());
    }

    void XPlaneSdkApi::speak(
        std::string_view message)
    {
        const auto stableMessage =
            nullTerminated(message);
        XPLMSpeakString(stableMessage.c_str());
    }

    void XPlaneSdkApi::resetRequested()
    {
        XPLMDebugString(
            "XPLLink: device requested reset.\n");
    }
}
