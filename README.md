# XPLLink

XPLLink connects Arduino-based cockpit hardware to X-Plane. The X-Plane plugin
discovers compatible serial devices, registers the datarefs and commands they
request, and moves updates between the simulator and each device.

The project currently supports the legacy XPLPro serial protocol so existing
hardware can migrate to XPLLink without changing its wire format.

## Repository layout

- `core/` contains the platform-independent C++20 protocol, runtime, profile,
  serial, and X-Plane bridge code.
- `plugin/` contains the X-Plane plugin entry points, menu, and status window.
- `firmware/arduino/libraries/XPLLink/` contains the Arduino library and
  examples.
- `tests/` contains native unit tests.
- `profiles/` contains sample device-profile caches.
- `third_party/xplane_sdk/` contains the X-Plane SDK files used by the build.
- `dist/` contains packaged plugin binaries.

## Building

XPLLink requires CMake 3.20 or newer and a compiler with C++20 support.

To build the core diagnostic program and tests:

```sh
cmake -S . -B out/build
cmake --build out/build
ctest --test-dir out/build --output-on-failure
```

To build the X-Plane plugin:

```sh
cmake -S . -B out/plugin -DXPLLINK_BUILD_XPLANE_PLUGIN=ON
cmake --build out/plugin --config Release
```

The bundled SDK is detected automatically. Alternatively, set
`XPLANE_SDK_ROOT` or pass `-DXPLLINK_XPLANE_SDK_PATH=/path/to/sdk`.

To deploy after building, set either:

- `XPLLINK_XPLANE_ROOT` to the X-Plane installation root; or
- `XPLLINK_XPLANE_PLUGIN_DEPLOY_DIR` to the exact platform plugin directory.

Debug builds do not update `dist/` unless
`XPLLINK_DISTRIBUTE_DEBUG_BUILDS=ON`.

## Runtime data

The plugin writes settings, cached profiles, and optional serial traces under
`X-Plane/Output/XPLLink/`. Cached profiles allow a known device to reconnect
without repeating its full registration sequence.

Within X-Plane, use the **Plugins > XPLLink** menu to engage or disengage
devices, open the status window, toggle serial tracing, or clear the profile
cache.

## Arduino library

Copy `firmware/arduino/libraries/XPLLink` into the Arduino libraries directory,
or install that directory as a ZIP library. Start with the `BasicSerial` and
`CacheTest` examples.

## License

No open-source license has been declared. Unless the copyright holder adds one,
the repository is provided without a grant to copy, modify, or redistribute it.
