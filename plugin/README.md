# XPLLink X-Plane plugin

This directory contains the X-Plane plugin entry points and status window.
Most protocol and device behavior is implemented in `core/`.

The plugin:

- creates the **Plugins > XPLLink** menu;
- discovers supported serial devices without blocking X-Plane's main loop;
- connects device datarefs and commands through the X-Plane SDK;
- saves registration profiles beneath `Output/XPLLink/profiles`;
- supports cached reconnection; and
- optionally records raw serial traffic in `Output/XPLLink/XPLLinkSerial.log`.

Build it by configuring CMake with `XPLLINK_BUILD_XPLANE_PLUGIN=ON`. The output
file is named `XPLLink.xpl` and is placed in the platform ABI directory when a
distribution or deployment path is configured.
