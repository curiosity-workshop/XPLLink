#pragma once

#include <Arduino.h>

#ifndef XPL_FLOATPRECISION
#define XPL_FLOATPRECISION 4
#endif

#ifndef XPL_RESPONSE_TIMEOUT
#define XPL_RESPONSE_TIMEOUT 90000
#endif

#ifndef XPL_USE_PROGMEM
#ifdef __AVR_ARCH__
#define XPL_USE_PROGMEM 1
#else
#define XPL_USE_PROGMEM 0
#endif
#endif

#ifndef XPLMAX_PACKETSIZE_TRANSMIT
#define XPLMAX_PACKETSIZE_TRANSMIT 200
#endif

#ifndef XPLMAX_PACKETSIZE_RECEIVE
#define XPLMAX_PACKETSIZE_RECEIVE 512
#endif

#if XPL_USE_PROGMEM
typedef const __FlashStringHelper XPString_t;
#else
typedef const char XPString_t;
#endif

typedef int dref_handle;
typedef int cmd_handle;
typedef int XPLMCommandKeyID;
typedef int XPLMCommandButtonID;

#define XPL_BAUDRATE 115200
#define XPL_RX_TIMEOUT 500
#define XPL_PACKETHEADER '['
#define XPL_PACKETTRAILER ']'
#define XPL_HANDLE_INVALID -1

#define XPLCMD_SENDNAME 'N'
#define XPLRESPONSE_NAME 'n'
#define XPLRESPONSE_VERSION 'v'
#define XPLCMD_SENDREQUEST 'Q'
#define XPLCMD_PROFILEACCEPTED 'A'
#define XPLCMD_DATAFLOWPAUSE 'p'
#define XPLCMD_DATAFLOWRESUME 'q'
#define XPLCMD_SETDATAFLOWSPEED 'f'
#define XPLREQUEST_REGISTERDATAREF 'b'
#define XPLREQUEST_REGISTERCOMMAND 'm'
#define XPLRESPONSE_DATAREF 'D'
#define XPLRESPONSE_COMMAND 'C'
#define XPLCMD_PRINTDEBUG 'g'
#define XPLCMD_SPEAK 's'
#define XPLREQUEST_DATAREFTOUCH 'd'
#define XPLREQUEST_UPDATES 'r'
#define XPLREQUEST_UPDATESARRAY 't'
#define XPLREQUEST_UPDATES_TYPE 'y'
#define XPLREQUEST_UPDATES_TYPE_ARRAY 'w'
#define xplmType_Unknown 0
#define xplmType_Int 1
#define xplmType_Float 2
#define xplmType_Double 4
#define xplmType_FloatArray 8
#define xplmType_IntArray 16
#define xplmType_Data 32

#define XPLREQUEST_SCALING 'u'
#define XPLCMD_RESET 'z'
#define XPLCMD_DATAREFUPDATEINT '1'
#define XPLCMD_DATAREFUPDATEFLOAT '2'
#define XPLCMD_DATAREFUPDATEINTARRAY '3'
#define XPLCMD_DATAREFUPDATEFLOATARRAY '4'
#define XPLCMD_DATAREFUPDATESTRING '9'
#define XPLCMD_COMMANDTRIGGER 'k'
#define XPLCMD_COMMANDSTART 'i'
#define XPLCMD_COMMANDEND 'j'
#define XPL_EXITING 'X'

struct XPLLinkData
{
    dref_handle handle;
    int type;
    int element;
    long inLong;
    float inFloat;
    int strLength;
    char* inStr;
};

namespace phoenix
{
    class XPLLink
    {
    public:
        explicit XPLLink(Stream& stream);
        explicit XPLLink(Stream* stream);

        void begin(const char* deviceName);

        void begin(
            const char* deviceName,
            void (*initFunction)(void),
            void (*stopFunction)(void),
            void (*inboundHandler)(XPLLinkData*));

        int connectionStatus();
        bool cachedRegistrationAccepted() const;

        int commandTrigger(cmd_handle commandHandle);
        int commandTrigger(cmd_handle commandHandle, int triggerCount);
        int commandStart(cmd_handle commandHandle);
        int commandEnd(cmd_handle commandHandle);

        void datarefWrite(dref_handle handle, int value);
        void datarefWrite(dref_handle handle, long value);
        void datarefWrite(dref_handle handle, int value, int arrayElement);
        void datarefWrite(dref_handle handle, long value, int arrayElement);
        void datarefWrite(dref_handle handle, float value);
        void datarefWrite(dref_handle handle, float value, int arrayElement);
        void datarefTouch(dref_handle handle);
        void requestDataRefRefresh(dref_handle handle);

        void requestUpdates(dref_handle handle, int rate, float precision);
        void requestUpdates(dref_handle handle, int rate, float precision, int arrayElement);
        void requestUpdatesType(dref_handle handle, int type, int rate, float precision);
        void requestUpdatesType(dref_handle handle, int type, int rate, float precision, int arrayElement);
        void setScaling(dref_handle handle, long inLow, long inHigh, long outLow, long outHigh);

        int registerDataRef(XPString_t* datarefName);
        int registerCommand(XPString_t* commandName);

        int sendDebugMessage(const char* message);
        int sendSpeakMessage(const char* message);
        void sendResetRequest();

        void dataFlowPause();
        void dataFlowResume();
        void setDataFlowSpeed(unsigned long bytesPerSecond);
        int getBufferStatus();

        int xloop();

    private:
        void processSerial();
        int receiveNSerial(int size);
        void processPacket();
        void transmitPacket();
        void sendName();
        void sendVersion();
        bool linkedForOutboundTraffic() const;
        void sendPacketVoid(int command, int handle);
        void sendPacketVoid(int command);
        void sendPacketString(int command, const char* value);
        int parseInt(int* target, char* buffer, int parameter);
        int parseInt(long* target, char* buffer, int parameter);
        int parseFloat(float* target, char* buffer, int parameter);
        int parseString(char* output, char* buffer, int parameter, int maxSize);
        int xdtostrf(double value, signed char width, unsigned char precision, char* output);

        int nextCachedDataRefHandle();
        int nextCachedCommandHandle();
        void resetCachedRegistration();

        Stream* stream_ = nullptr;
        const char* deviceName_ = nullptr;
        bool registerFlag_ = false;
        bool connectionStatus_ = false;
        bool outboundTrafficEnabled_ = false;
        bool cachedRegistrationAccepted_ = false;
        bool handleAssignmentReceived_ = false;
        int cachedDataRefCount_ = 0;
        int cachedCommandCount_ = 0;
        int cachedDataRefNext_ = 0;
        int cachedCommandNext_ = 0;

        XPLLinkData inboundData_{};
        char sendBuffer_[XPLMAX_PACKETSIZE_TRANSMIT]{};
        char receiveBuffer_[XPLMAX_PACKETSIZE_RECEIVE]{};
        size_t receiveBufferBytesReceived_ = 0;

        void (*initFunction_)(void) = nullptr;
        void (*stopFunction_)(void) = nullptr;
        void (*inboundHandler_)(XPLLinkData*) = nullptr;

        dref_handle handleAssignment_ = XPL_HANDLE_INVALID;
    };
}

using XPLLink = phoenix::XPLLink;
using inStruct = XPLLinkData;
