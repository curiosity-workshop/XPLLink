#include "XPLLink.h"

#include <string.h>

namespace phoenix
{
    XPLLink::XPLLink(Stream& stream)
        : stream_(&stream)
    {
        stream_->setTimeout(XPL_RX_TIMEOUT);
    }

    XPLLink::XPLLink(Stream* stream)
        : stream_(stream)
    {
        if (stream_ != nullptr)
        {
            stream_->setTimeout(XPL_RX_TIMEOUT);
        }
    }

    void XPLLink::begin(const char* deviceName)
    {
        begin(deviceName, nullptr, nullptr, nullptr);
    }

    void XPLLink::begin(
        const char* deviceName,
        void (*initFunction)(void),
        void (*stopFunction)(void),
        void (*inboundHandler)(XPLLinkData*))
    {
        deviceName_ = deviceName;
        connectionStatus_ = false;
        outboundTrafficEnabled_ = false;
        receiveBuffer_[0] = 0;
        registerFlag_ = false;
        initFunction_ = initFunction;
        stopFunction_ = stopFunction;
        inboundHandler_ = inboundHandler;
        resetCachedRegistration();
    }

    int XPLLink::xloop()
    {
        processSerial();

        if (registerFlag_)
        {
            if (initFunction_ != nullptr)
            {
                initFunction_();
            }

            outboundTrafficEnabled_ = true;
            registerFlag_ = false;
        }

        return linkedForOutboundTraffic();
    }

    int XPLLink::connectionStatus()
    {
        return connectionStatus_;
    }

    bool XPLLink::cachedRegistrationAccepted() const
    {
        return cachedRegistrationAccepted_;
    }

    int XPLLink::commandTrigger(cmd_handle commandHandle)
    {
        return commandTrigger(commandHandle, 1);
    }

    int XPLLink::commandTrigger(cmd_handle commandHandle, int triggerCount)
    {
        if (!linkedForOutboundTraffic() || commandHandle < 0)
        {
            return XPL_HANDLE_INVALID;
        }

        sprintf(
            sendBuffer_,
            "%c%c,%i,%i%c",
            XPL_PACKETHEADER,
            XPLCMD_COMMANDTRIGGER,
            commandHandle,
            triggerCount,
            XPL_PACKETTRAILER);
        transmitPacket();
        return 0;
    }

    int XPLLink::commandStart(cmd_handle commandHandle)
    {
        if (!linkedForOutboundTraffic() || commandHandle < 0)
        {
            return XPL_HANDLE_INVALID;
        }

        sendPacketVoid(XPLCMD_COMMANDSTART, commandHandle);
        return 0;
    }

    int XPLLink::commandEnd(cmd_handle commandHandle)
    {
        if (!linkedForOutboundTraffic() || commandHandle < 0)
        {
            return XPL_HANDLE_INVALID;
        }

        sendPacketVoid(XPLCMD_COMMANDEND, commandHandle);
        return 0;
    }

    int XPLLink::sendDebugMessage(const char* message)
    {
        if (!linkedForOutboundTraffic())
        {
            return XPL_HANDLE_INVALID;
        }

        sendPacketString(XPLCMD_PRINTDEBUG, message);
        return 1;
    }

    int XPLLink::sendSpeakMessage(const char* message)
    {
        if (!linkedForOutboundTraffic())
        {
            return XPL_HANDLE_INVALID;
        }

        sendPacketString(XPLCMD_SPEAK, message);
        return 1;
    }

    void XPLLink::dataFlowPause()
    {
        if (!linkedForOutboundTraffic())
        {
            return;
        }

        sendPacketVoid(XPLCMD_DATAFLOWPAUSE, getBufferStatus());
    }

    void XPLLink::dataFlowResume()
    {
        if (!linkedForOutboundTraffic())
        {
            return;
        }

        sendPacketVoid(XPLCMD_DATAFLOWRESUME, getBufferStatus());
    }

    int XPLLink::getBufferStatus()
    {
        if (stream_ == nullptr)
        {
            return 0;
        }

        return stream_->available();
    }

    void XPLLink::setDataFlowSpeed(unsigned long bytesPerSecond)
    {
        if (!linkedForOutboundTraffic())
        {
            return;
        }

        sprintf(
            sendBuffer_,
            "%c%c,%lu%c",
            XPL_PACKETHEADER,
            XPLCMD_SETDATAFLOWSPEED,
            bytesPerSecond,
            XPL_PACKETTRAILER);
        transmitPacket();
    }

    void XPLLink::datarefWrite(dref_handle handle, int value)
    {
        if (!linkedForOutboundTraffic() || handle < 0)
        {
            return;
        }

        sprintf(
            sendBuffer_,
            "%c%c,%i,%i%c",
            XPL_PACKETHEADER,
            XPLCMD_DATAREFUPDATEINT,
            handle,
            value,
            XPL_PACKETTRAILER);
        transmitPacket();
    }

    void XPLLink::datarefWrite(dref_handle handle, int value, int arrayElement)
    {
        if (!linkedForOutboundTraffic() || handle < 0)
        {
            return;
        }

        sprintf(
            sendBuffer_,
            "%c%c,%i,%i,%i%c",
            XPL_PACKETHEADER,
            XPLCMD_DATAREFUPDATEINTARRAY,
            handle,
            value,
            arrayElement,
            XPL_PACKETTRAILER);
        transmitPacket();
    }

    void XPLLink::datarefWrite(dref_handle handle, long value)
    {
        if (!linkedForOutboundTraffic() || handle < 0)
        {
            return;
        }

        sprintf(
            sendBuffer_,
            "%c%c,%i,%ld%c",
            XPL_PACKETHEADER,
            XPLCMD_DATAREFUPDATEINT,
            handle,
            value,
            XPL_PACKETTRAILER);
        transmitPacket();
    }

    void XPLLink::datarefWrite(dref_handle handle, long value, int arrayElement)
    {
        if (!linkedForOutboundTraffic() || handle < 0)
        {
            return;
        }

        sprintf(
            sendBuffer_,
            "%c%c,%i,%ld,%i%c",
            XPL_PACKETHEADER,
            XPLCMD_DATAREFUPDATEINTARRAY,
            handle,
            value,
            arrayElement,
            XPL_PACKETTRAILER);
        transmitPacket();
    }

    void XPLLink::datarefWrite(dref_handle handle, float value)
    {
        if (!linkedForOutboundTraffic() || handle < 0)
        {
            return;
        }

        char* cursor = sendBuffer_;
        cursor += sprintf(
            sendBuffer_,
            "%c%c,%i,",
            XPL_PACKETHEADER,
            XPLCMD_DATAREFUPDATEFLOAT,
            handle);
        cursor += xdtostrf(value, 0, XPL_FLOATPRECISION, cursor);
        sprintf(cursor, "%c", XPL_PACKETTRAILER);
        transmitPacket();
    }

    void XPLLink::datarefWrite(dref_handle handle, float value, int arrayElement)
    {
        if (!linkedForOutboundTraffic() || handle < 0)
        {
            return;
        }

        char* cursor = sendBuffer_;
        cursor += sprintf(
            sendBuffer_,
            "%c%c,%i,",
            XPL_PACKETHEADER,
            XPLCMD_DATAREFUPDATEFLOATARRAY,
            handle);
        cursor += xdtostrf(value, 0, XPL_FLOATPRECISION, cursor);
        sprintf(cursor, ",%i%c", arrayElement, XPL_PACKETTRAILER);
        transmitPacket();
    }

    void XPLLink::datarefTouch(dref_handle handle)
    {
        if (!linkedForOutboundTraffic() || handle < 0)
        {
            return;
        }

        sprintf(
            sendBuffer_,
            "%c%c,%i%c",
            XPL_PACKETHEADER,
            XPLREQUEST_DATAREFTOUCH,
            handle,
            XPL_PACKETTRAILER);
        transmitPacket();
    }

    void XPLLink::sendName()
    {
        if (deviceName_ != nullptr)
        {
            sendPacketString(XPLRESPONSE_NAME, deviceName_);
        }
    }

    void XPLLink::sendVersion()
    {
        if (deviceName_ != nullptr)
        {
            char version[25];
            sprintf(version, "%s %s", __DATE__, __TIME__);
            sendPacketString(XPLRESPONSE_VERSION, version);
        }
    }

    void XPLLink::sendResetRequest()
    {
        if (linkedForOutboundTraffic())
        {
            sendPacketVoid(XPLCMD_RESET, 0);
        }
    }

    void XPLLink::processSerial()
    {
        if (stream_ == nullptr)
        {
            return;
        }

        while (stream_->available() && receiveBuffer_[0] != XPL_PACKETHEADER)
        {
            receiveBuffer_[0] = static_cast<char>(stream_->read());
        }

        if (receiveBuffer_[0] != XPL_PACKETHEADER)
        {
            return;
        }

        receiveBufferBytesReceived_ =
            stream_->readBytesUntil(
                XPL_PACKETTRAILER,
                &receiveBuffer_[1],
                XPLMAX_PACKETSIZE_RECEIVE - 1);

        if (receiveBufferBytesReceived_ == 0)
        {
            receiveBuffer_[0] = 0;
            return;
        }

        receiveBuffer_[++receiveBufferBytesReceived_] = XPL_PACKETTRAILER;
        receiveBuffer_[++receiveBufferBytesReceived_] = 0;

        processPacket();
    }

    int XPLLink::receiveNSerial(int size)
    {
        if (stream_ == nullptr)
        {
            return 0;
        }

        if (size <= 0)
        {
            return 0;
        }

        const int bytesToStore =
            size > XPLMAX_PACKETSIZE_RECEIVE ?
                XPLMAX_PACKETSIZE_RECEIVE :
                size;

        const int bytesRead =
            stream_->readBytes(receiveBuffer_, bytesToStore);

        for (int index = bytesToStore;
            index < size;
            ++index)
        {
            stream_->read();
        }

        if (bytesRead < XPLMAX_PACKETSIZE_RECEIVE)
        {
            receiveBuffer_[bytesRead] = 0;
        }

        return bytesRead;
    }

    void XPLLink::processPacket()
    {
        if (receiveBuffer_[0] != XPL_PACKETHEADER)
        {
            return;
        }

        switch (receiveBuffer_[1])
        {
        case XPL_EXITING:
            connectionStatus_ = false;
            outboundTrafficEnabled_ = false;
            if (stopFunction_ != nullptr)
            {
                stopFunction_();
            }
            break;

        case XPLCMD_SENDNAME:
            resetCachedRegistration();
            sendVersion();
            sendName();
            connectionStatus_ = true;
            outboundTrafficEnabled_ = false;
            registerFlag_ = false;
            break;

        case XPLCMD_PROFILEACCEPTED:
            parseInt(&cachedDataRefCount_, receiveBuffer_, 2);
            parseInt(&cachedCommandCount_, receiveBuffer_, 3);
            cachedRegistrationAccepted_ = true;
            cachedDataRefNext_ = 0;
            cachedCommandNext_ = 0;
            registerFlag_ = true;
            break;

        case XPLCMD_SENDREQUEST:
            registerFlag_ = true;
            break;

        case XPLRESPONSE_DATAREF:
        case XPLRESPONSE_COMMAND:
            parseInt(&handleAssignment_, receiveBuffer_, 2);
            break;

        case XPLCMD_DATAREFUPDATEINT:
            parseInt(&inboundData_.handle, receiveBuffer_, 2);
            parseInt(&inboundData_.inLong, receiveBuffer_, 3);
            inboundData_.type = xplmType_Int;
            inboundData_.inFloat = 0;
            inboundData_.element = 0;
            if (inboundHandler_ != nullptr)
            {
                inboundHandler_(&inboundData_);
            }
            break;

        case XPLCMD_DATAREFUPDATEINTARRAY:
            parseInt(&inboundData_.handle, receiveBuffer_, 2);
            parseInt(&inboundData_.inLong, receiveBuffer_, 3);
            parseInt(&inboundData_.element, receiveBuffer_, 4);
            inboundData_.type = xplmType_IntArray;
            inboundData_.inFloat = 0;
            if (inboundHandler_ != nullptr)
            {
                inboundHandler_(&inboundData_);
            }
            break;

        case XPLCMD_DATAREFUPDATEFLOAT:
            parseInt(&inboundData_.handle, receiveBuffer_, 2);
            parseFloat(&inboundData_.inFloat, receiveBuffer_, 3);
            inboundData_.type = xplmType_Float;
            inboundData_.inLong = 0;
            inboundData_.element = 0;
            if (inboundHandler_ != nullptr)
            {
                inboundHandler_(&inboundData_);
            }
            break;

        case XPLCMD_DATAREFUPDATEFLOATARRAY:
            parseInt(&inboundData_.handle, receiveBuffer_, 2);
            parseFloat(&inboundData_.inFloat, receiveBuffer_, 3);
            parseInt(&inboundData_.element, receiveBuffer_, 4);
            inboundData_.type = xplmType_FloatArray;
            inboundData_.inLong = 0;
            if (inboundHandler_ != nullptr)
            {
                inboundHandler_(&inboundData_);
            }
            break;

        case XPLCMD_DATAREFUPDATESTRING:
            parseInt(&inboundData_.handle, receiveBuffer_, 2);
            parseInt(&inboundData_.strLength, receiveBuffer_, 3);
            inboundData_.strLength =
                receiveNSerial(inboundData_.strLength);
            inboundData_.type = xplmType_Data;
            inboundData_.inStr = receiveBuffer_;
            if (inboundHandler_ != nullptr)
            {
                inboundHandler_(&inboundData_);
            }
            break;

        default:
            break;
        }

        receiveBuffer_[0] = 0;
    }

    void XPLLink::sendPacketVoid(int command, int handle)
    {
        if (handle < 0)
        {
            return;
        }

        sprintf(
            sendBuffer_,
            "%c%c,%i%c",
            XPL_PACKETHEADER,
            command,
            handle,
            XPL_PACKETTRAILER);
        transmitPacket();
    }

    bool XPLLink::linkedForOutboundTraffic() const
    {
        return connectionStatus_ &&
            (registerFlag_ || outboundTrafficEnabled_);
    }

    void XPLLink::sendPacketVoid(int command)
    {
        sprintf(
            sendBuffer_,
            "%c%c%c",
            XPL_PACKETHEADER,
            command,
            XPL_PACKETTRAILER);
        transmitPacket();
    }

    void XPLLink::sendPacketString(int command, const char* value)
    {
        sprintf(
            sendBuffer_,
            "%c%c,\"%s\"%c",
            XPL_PACKETHEADER,
            command,
            value,
            XPL_PACKETTRAILER);
        transmitPacket();
    }

    void XPLLink::transmitPacket()
    {
        if (stream_ == nullptr)
        {
            return;
        }

        stream_->write(sendBuffer_);

        if (strlen(sendBuffer_) == 64)
        {
            stream_->print(" ");
        }

        stream_->flush();
    }

    int XPLLink::parseString(char* output, char* buffer, int parameter, int maxSize)
    {
        int begin;
        int position = 0;
        int length;

        for (int index = 1; index < parameter; ++index)
        {
            while (buffer[position] != ',' && buffer[position] != 0)
            {
                ++position;
            }
            ++position;
        }

        while (buffer[position] != '"' && buffer[position] != 0)
        {
            ++position;
        }

        begin = ++position;

        while (buffer[position] != '"' && buffer[position] != 0)
        {
            ++position;
        }

        length = position - begin;

        if (length > maxSize)
        {
            length = maxSize;
        }

        strncpy(output, &buffer[begin], length);
        output[length] = 0;

        return 0;
    }

    int XPLLink::parseInt(int* target, char* buffer, int parameter)
    {
        int begin;
        int position = 0;

        for (int index = 1; index < parameter; ++index)
        {
            while (buffer[position] != ',' && buffer[position] != 0)
            {
                ++position;
            }
            ++position;
        }

        begin = position;

        while (buffer[position] != ',' &&
            buffer[position] != 0 &&
            buffer[position] != XPL_PACKETTRAILER)
        {
            ++position;
        }

        const char hold = buffer[position];
        buffer[position] = 0;
        *target = atoi(&buffer[begin]);
        buffer[position] = hold;

        return 0;
    }

    int XPLLink::parseInt(long* target, char* buffer, int parameter)
    {
        int begin;
        int position = 0;

        for (int index = 1; index < parameter; ++index)
        {
            while (buffer[position] != ',' && buffer[position] != 0)
            {
                ++position;
            }
            ++position;
        }

        begin = position;

        while (buffer[position] != ',' &&
            buffer[position] != 0 &&
            buffer[position] != XPL_PACKETTRAILER)
        {
            ++position;
        }

        const char hold = buffer[position];
        buffer[position] = 0;
        *target = atol(&buffer[begin]);
        buffer[position] = hold;

        return 0;
    }

    int XPLLink::parseFloat(float* target, char* buffer, int parameter)
    {
        int begin;
        int position = 0;

        for (int index = 1; index < parameter; ++index)
        {
            while (buffer[position] != ',' && buffer[position] != 0)
            {
                ++position;
            }
            ++position;
        }

        begin = position;

        while (buffer[position] != ',' &&
            buffer[position] != 0 &&
            buffer[position] != XPL_PACKETTRAILER)
        {
            ++position;
        }

        const char hold = buffer[position];
        buffer[position] = 0;
        *target = atof(&buffer[begin]);
        buffer[position] = hold;

        return 0;
    }

    int XPLLink::registerDataRef(XPString_t* datarefName)
    {
        if (!registerFlag_)
        {
            return XPL_HANDLE_INVALID;
        }

        if (cachedRegistrationAccepted_)
        {
            const int cachedHandle =
                nextCachedDataRefHandle();

            if (cachedHandle != XPL_HANDLE_INVALID)
            {
                return cachedHandle;
            }
        }

#if XPL_USE_PROGMEM
        sprintf(
            sendBuffer_,
            "%c%c,\"%S\"%c",
            XPL_PACKETHEADER,
            XPLREQUEST_REGISTERDATAREF,
            (wchar_t*)datarefName,
            XPL_PACKETTRAILER);
#else
        sprintf(
            sendBuffer_,
            "%c%c,\"%s\"%c",
            XPL_PACKETHEADER,
            XPLREQUEST_REGISTERDATAREF,
            (char*)datarefName,
            XPL_PACKETTRAILER);
#endif
        transmitPacket();

        handleAssignment_ = XPL_HANDLE_INVALID;
        const unsigned long startTime = millis();

        while (millis() - startTime < XPL_RESPONSE_TIMEOUT &&
            handleAssignment_ < 0)
        {
            processSerial();
        }

        return handleAssignment_;
    }

    int XPLLink::registerCommand(XPString_t* commandName)
    {
        if (!registerFlag_)
        {
            return XPL_HANDLE_INVALID;
        }

        if (cachedRegistrationAccepted_)
        {
            const int cachedHandle =
                nextCachedCommandHandle();

            if (cachedHandle != XPL_HANDLE_INVALID)
            {
                return cachedHandle;
            }
        }

#if XPL_USE_PROGMEM
        sprintf(
            sendBuffer_,
            "%c%c,\"%S\"%c",
            XPL_PACKETHEADER,
            XPLREQUEST_REGISTERCOMMAND,
            (wchar_t*)commandName,
            XPL_PACKETTRAILER);
#else
        sprintf(
            sendBuffer_,
            "%c%c,\"%s\"%c",
            XPL_PACKETHEADER,
            XPLREQUEST_REGISTERCOMMAND,
            (char*)commandName,
            XPL_PACKETTRAILER);
#endif
        transmitPacket();

        handleAssignment_ = XPL_HANDLE_INVALID;
        const unsigned long startTime = millis();

        while (millis() - startTime < XPL_RESPONSE_TIMEOUT &&
            handleAssignment_ < 0)
        {
            processSerial();
        }

        return handleAssignment_;
    }

    void XPLLink::requestUpdates(int handle, int rate, float precision)
    {
        if (!linkedForOutboundTraffic() || handle < 0)
        {
            return;
        }

        char* cursor = sendBuffer_;
        cursor += sprintf(
            sendBuffer_,
            "%c%c,%i,%i,",
            XPL_PACKETHEADER,
            XPLREQUEST_UPDATES,
            handle,
            rate);
        cursor += xdtostrf(precision, 0, XPL_FLOATPRECISION, cursor);
        sprintf(cursor, "%c", XPL_PACKETTRAILER);
        transmitPacket();
    }

    void XPLLink::requestUpdates(int handle, int rate, float precision, int arrayElement)
    {
        if (!linkedForOutboundTraffic() || handle < 0)
        {
            return;
        }

        char* cursor = sendBuffer_;
        cursor += sprintf(
            sendBuffer_,
            "%c%c,%i,%i,",
            XPL_PACKETHEADER,
            XPLREQUEST_UPDATESARRAY,
            handle,
            rate);
        cursor += xdtostrf(precision, 0, XPL_FLOATPRECISION, cursor);
        sprintf(cursor, ",%i%c", arrayElement, XPL_PACKETTRAILER);
        transmitPacket();
    }

    void XPLLink::requestUpdatesType(int handle, int type, int rate, float precision)
    {
        if (!linkedForOutboundTraffic() || handle < 0)
        {
            return;
        }

        char* cursor = sendBuffer_;
        cursor += sprintf(
            sendBuffer_,
            "%c%c,%i,%i,%i,",
            XPL_PACKETHEADER,
            XPLREQUEST_UPDATES_TYPE,
            handle,
            type,
            rate);
        cursor += xdtostrf(precision, 0, XPL_FLOATPRECISION, cursor);
        sprintf(cursor, "%c", XPL_PACKETTRAILER);
        transmitPacket();
    }

    void XPLLink::requestUpdatesType(
        int handle,
        int type,
        int rate,
        float precision,
        int arrayElement)
    {
        if (!linkedForOutboundTraffic() || handle < 0)
        {
            return;
        }

        char* cursor = sendBuffer_;
        cursor += sprintf(
            sendBuffer_,
            "%c%c,%i,%i,%i,",
            XPL_PACKETHEADER,
            XPLREQUEST_UPDATES_TYPE_ARRAY,
            handle,
            type,
            rate);
        cursor += xdtostrf(precision, 0, XPL_FLOATPRECISION, cursor);
        sprintf(cursor, ",%i%c", arrayElement, XPL_PACKETTRAILER);
        transmitPacket();
    }

    void XPLLink::setScaling(
        int handle,
        long inLow,
        long inHigh,
        long outLow,
        long outHigh)
    {
        if (!linkedForOutboundTraffic() || handle < 0)
        {
            return;
        }

        sprintf(
            sendBuffer_,
            "%c%c,%i,%ld,%ld,%ld,%ld%c",
            XPL_PACKETHEADER,
            XPLREQUEST_SCALING,
            handle,
            inLow,
            inHigh,
            outLow,
            outHigh,
            XPL_PACKETTRAILER);
        transmitPacket();
    }

    int XPLLink::xdtostrf(
        double value,
        signed char width,
        unsigned char precision,
        char* output)
    {
#ifdef __AVR_ARCH__
        dtostrf(value, width, precision, output);
        return strlen(output);
#else
        char format[20];
        sprintf(format, "%%%d.%df", width, precision);
        return sprintf(output, format, value);
#endif
    }

    int XPLLink::nextCachedDataRefHandle()
    {
        if (cachedDataRefNext_ >= cachedDataRefCount_)
        {
            return XPL_HANDLE_INVALID;
        }

        return cachedDataRefNext_++;
    }

    int XPLLink::nextCachedCommandHandle()
    {
        if (cachedCommandNext_ >= cachedCommandCount_)
        {
            return XPL_HANDLE_INVALID;
        }

        return cachedCommandNext_++;
    }

    void XPLLink::resetCachedRegistration()
    {
        cachedRegistrationAccepted_ = false;
        cachedDataRefCount_ = 0;
        cachedCommandCount_ = 0;
        cachedDataRefNext_ = 0;
        cachedCommandNext_ = 0;
    }
}
