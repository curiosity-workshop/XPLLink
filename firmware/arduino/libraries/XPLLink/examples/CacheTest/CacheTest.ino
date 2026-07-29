#include <XPLLink.h>

XPLLink link(Serial);

dref_handle drefNavLights = XPL_HANDLE_INVALID;

void xplRegister()
{
    drefNavLights =
        link.registerDataRef(F("sim/cockpit2/switches/navigation_lights_on"));

    link.requestUpdates(drefNavLights, 100, 0.0000);
}

void xplShutdown()
{
    drefNavLights = XPL_HANDLE_INVALID;
}

void xplInboundHandler(XPLLinkData* inData)
{
    if (inData->handle != drefNavLights)
    {
        return;
    }

    digitalWrite(LED_BUILTIN, inData->inLong ? HIGH : LOW);
}

void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    Serial.begin(XPL_BAUDRATE);
    link.begin(
        "XPLLink Cache Test",
        &xplRegister,
        &xplShutdown,
        &xplInboundHandler);
}

void loop()
{
    link.xloop();
}
