# XPLLink

Arduino-side X-Plane hardware link library. It communicates with the XPLLink
X-Plane plugin over a `Stream`, normally a USB serial connection at 115200 baud.

## Installation

Install this directory as an Arduino ZIP library or copy it into the sketchbook
`libraries` directory.

## Minimal setup

```cpp
#include <XPLLink.h>

XPLLink link(Serial);

void setup()
{
    Serial.begin(XPL_BAUDRATE);
    link.begin("My cockpit panel");
}

void loop()
{
    link.xloop();
}
```

Call `registerDataRef` and `registerCommand` from the initialization callback,
then use the returned handles for writes, update subscriptions, and command
events. See the bundled `BasicSerial` and `CacheTest` examples for complete
flows.

The implementation currently uses the legacy XPLPro-compatible wire protocol.
Protocol and transport details remain internal so additional transports can be
added without splitting the Arduino package.
