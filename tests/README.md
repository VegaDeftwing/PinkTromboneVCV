# Pink Trombone verification

From the repository root:

```sh
make -j4 RACK_DIR=/path/to/Rack-SDK
make test RACK_DIR=/path/to/Rack-SDK
make benchmark RACK_DIR=/path/to/Rack-SDK
make install RACK_DIR=/path/to/Rack-SDK
```

Tests link the real Rack engine and run without a window or audio device.
They cover independent voices, mono CV broadcast, short/disconnected cables,
channel-count changes, five sample rates (22.05–192 kHz), malformed and extreme
CV, and all panel controls. Routing is compared sample for sample against
independent instances of the corrected mono voice. A separate scalar waveguide
checks the SIMD implementation with a 0.2 mV tolerance. These tests do not claim
bit-exact compatibility with the old implementation's documented DSP bugs.

Specific regressions cover fractional nasal scattering, turbulence bounds,
transient expiry/reuse, equal tip/blade positions, filter coefficient caching,
zero cutoff, deterministic independent noise, concurrent simplex initialization,
and inverse anatomical hit testing. Switch tests check independent states and
retained amounts. Spread tests check stable offsets, patch persistence, mono
bypass and conditional UI visibility. External audio tests check source
replacement, silence, impulse tails, polyphony, channel isolation and exact dry
bypass. Follow-up regressions check modulation clocks after minutes of operation
at high sample rates, geometry independence from earlier blade positions, moving
nasal coupling and external noise with a silent Voice input.

The benchmark compares the same corrected DSP with and without explicit Rack
SIMD kernels. Compiler auto-vectorization remains enabled in both cases.

For address and undefined-behavior checks on Linux:

```sh
mkdir -p build/tests
g++ -std=c++11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Isrc -I"$RACK_DIR/include" -I"$RACK_DIR/dep/include" \
  tests/regression.cpp src/PinkTrombone/*.cpp -L"$RACK_DIR" -lRack \
  -pthread -o build/tests/regression-sanitized
LD_LIBRARY_PATH="$RACK_DIR" build/tests/regression-sanitized
```

The standalone harness releases its Rack model factory, so leak checking can
remain enabled.

`patches/polyphony.vcv` is a portable four-voice keyboard → Pink Trombone → VCA
→ stereo audio patch. Select your output device in Audio 2. Use the QWERTY or
ZXCVB rows to play chords. The VCA gates the otherwise continuous synthesizer.
