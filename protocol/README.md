# Protocol

XPLLink currently implements the legacy XPLPro-compatible serial protocol.
Frames are ASCII payloads enclosed in `[` and `]` and exchanged at 115200 baud.

The desktop parser and message model live in
`core/include/xpllink/protocol/legacy`. The corresponding Arduino constants and
packet handling live in `firmware/arduino/libraries/XPLLink/src`.

The profile protocol value remains `xplpro-legacy` intentionally. It identifies
the wire format and preserves compatibility with existing cached profiles; it
is not an obsolete product name.
