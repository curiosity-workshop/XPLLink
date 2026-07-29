#pragma once

#if defined(_WIN32)
#include <xpllink/serial/WindowsSerialEnumerator.h>
#include <xpllink/serial/WindowsSerialTransportFactory.h>
#elif defined(__APPLE__)
#include <xpllink/serial/MacSerialEnumerator.h>
#include <xpllink/serial/MacSerialTransportFactory.h>
#else
#include <xpllink/serial/LinuxSerialEnumerator.h>
#include <xpllink/serial/LinuxSerialTransportFactory.h>
#endif

namespace xpllink::serial
{
#if defined(_WIN32)
    using NativeSerialControlMode = WindowsSerialControlMode;
    using NativeSerialEnumerator = WindowsSerialEnumerator;
    using NativeSerialTransportFactory = WindowsSerialTransportFactory;
#elif defined(__APPLE__)
    using NativeSerialControlMode = MacSerialControlMode;
    using NativeSerialEnumerator = MacSerialEnumerator;
    using NativeSerialTransportFactory = MacSerialTransportFactory;
#else
    using NativeSerialControlMode = LinuxSerialControlMode;
    using NativeSerialEnumerator = LinuxSerialEnumerator;
    using NativeSerialTransportFactory = LinuxSerialTransportFactory;
#endif
}
