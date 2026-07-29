#include <XPLLink.h>

xpllink::XPLLink link(Serial);

void setup()
{
    Serial.begin(115200);
    link.begin("XPLLink Basic Serial");
}

void loop()
{
    link.xloop();
}
