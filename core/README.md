# XPLLink core

The core is the reusable C++20 implementation shared by the diagnostic program,
tests, and X-Plane plugin.

Its public headers live under `include/xpllink` and are divided into:

- `serial` — platform-specific enumeration and byte transports;
- `protocol/legacy` — framing and messages for XPLPro-compatible devices;
- `discovery` — device identity probing;
- `profile` — JSON-backed registration caches;
- `runtime` — device sessions, controllers, and scheduled updates;
- `xplane` — simulator-facing interfaces and SDK adapters; and
- `logging` — application and byte-level serial tracing.

`src/main.cpp` builds the `XPLLink` diagnostic executable. It scans serial
ports, probes compatible devices, and exercises them briefly without loading
X-Plane.
