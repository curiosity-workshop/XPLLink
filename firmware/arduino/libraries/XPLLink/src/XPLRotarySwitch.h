//   XPLRotarySwitch.h - XPLLink add-on Library for rotary switch connections
//   Created by Curiosity Workshop, Michael Gerlicher,  2024
//
//   To report problems, download updates and examples, suggest enhancements or get technical support, please visit:
//      discord:  https://discord.gg/gzXetjEST4
//      patreon:  www.patreon.com/curiosityworkshop

#ifndef XPLRotarySwitch_h
#define XPLRotarySwitch_h

#include "XPLLink.h"

// Parameters around the interface

#define XPLSWITCHES_DEBOUNCETIME 50
#define XPLSWITCHES_PRESSED      0
#define XPLSWITCHES_RELEASED     1

#ifndef XPLROTARYSWITCH_MAXPINS
#define XPLROTARYSWITCH_MAXPINS     6                  //Default to 6.
#endif


/// @brief Core class for the XPLLink switches add-on
class XPLRotarySwitch
{
public:
    /// @brief Constructor
    /// @param switchHandler, Function called when pin activity is detected, or NULL if not needed
    XPLRotarySwitch(void (*switchHandler)(int pin, int switchValue));

    /// <summary>
    /// @brief begin
    /// </summary>
    /// <param name="link"></param>
    void begin(XPLLink* link);



    int addPin(int inPin, int inValue, int inHandle);
    int addPin(int inPin, int inValue, int inHandle, int inElement);

    int getHandle(int inPin);


    /// @brief Scan pins and call handler if any changes are detected.  Run regularly
    void check(void);

    void clear(void);

private:

    XPLLink* link_;

    int _switchCount;             // how many are registered


    void (*_switchHandler)(int inSwitchID, int inSwitchValue) = NULL;  // this function will be called when activity is detected on the mux, if not NULL


    struct XPLSwitch
    {
        int arduinoPin;                // connected pin
        byte prevStatus;              //  last known status
        byte mode;
        int  handle;
        int element;
        long int prevTime;            //  time of last change

    };

    struct XPLSwitch _switches[XPLROTARYSWITCH_MAXPINS];

};


XPLRotarySwitch::XPLRotarySwitch(void (*switchHandler)(int inSwitchID, int inValue))
{


    _switchHandler = switchHandler;


};

void XPLRotarySwitch::begin(XPLLink* link)
{
    link_ = link;
    clear();

}

void XPLRotarySwitch::clear(void)           // call this prior to adding pins if not the first run
{
    _switchCount = 0;

}

int XPLRotarySwitch::addPin(int inPin, int inMode, int inHandle)
{


    return addPin(inPin, inMode, inHandle, 0);


}

int XPLRotarySwitch::addPin(int inPin, int inMode, int inHandle, int inElement)
{
    if (_switchCount >= XPLROTARYSWITCH_MAXPINS) return -1;

    _switches[_switchCount].arduinoPin = inPin;
    _switches[_switchCount].mode = inMode;
    _switches[_switchCount].handle = inHandle;
    _switches[_switchCount].element = inElement;
    _switches[_switchCount].prevStatus = -1;        // This will force update to the plugin
    pinMode(inPin, INPUT_PULLUP);

    return _switchCount++;


}

int XPLRotarySwitch::getHandle(int inPin)
{
    for (int i = 0; i < XPLROTARYSWITCH_MAXPINS; i++) if (_switches[i].arduinoPin == inPin) return _switches[i].handle;
    return -1;

}


void XPLRotarySwitch::check(void)
{

    unsigned long timeNow = millis();


    for (int i = 0; i < _switchCount; i++)
    {
        int pinValue = digitalRead(_switches[i].arduinoPin);

        if (pinValue != _switches[i].prevStatus && timeNow - _switches[i].prevTime >= XPLSWITCHES_DEBOUNCETIME)
        {

            _switches[i].prevStatus = pinValue;
            _switches[i].prevTime = timeNow;

            switch (_switches[i].mode)
            {

            case XPLSWITCHES_DATAREFWRITE:
                link_->datarefWrite(_switches[i].handle, pinValue, _switches[i].element);
                break;

            case XPLSWITCHES_DATAREFWRITE_INVERT:
                link_->datarefWrite(_switches[i].handle, !pinValue, _switches[i].element);
                break;

            case XPLSWITCHES_COMMANDTRIGGER:
                if (pinValue == XPLSWITCHES_PRESSED) link_->commandTrigger(_switches[i].handle);
                break;

            case XPLSWITCHES_COMMANDSTARTEND:
                if (pinValue == XPLSWITCHES_PRESSED)     link_->commandStart(_switches[i].handle);
                if (pinValue == XPLSWITCHES_RELEASED)    link_->commandEnd(_switches[i].handle);
                break;


            }

            if (_switchHandler != NULL) _switchHandler(_switches[i].arduinoPin, pinValue);

        }
    }

}

#endif
