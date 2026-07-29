# Tests

The native test suite covers:

- legacy frame parsing and message decoding;
- device-session handshakes and traffic;
- device-controller datarefs, commands, scaling, and profiles;
- scheduled update delivery;
- JSON profile persistence; and
- the X-Plane bridge abstraction.

Build and run all tests with:

```sh
cmake -S . -B out/build
cmake --build out/build
ctest --test-dir out/build --output-on-failure
```

The tests use lightweight fakes and do not require X-Plane or a serial device.
