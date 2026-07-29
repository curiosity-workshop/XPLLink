#pragma once

#include <Arduino.h>

namespace xpllink::platform
{
    inline unsigned long milliseconds()
    {
        return millis();
    }
}
