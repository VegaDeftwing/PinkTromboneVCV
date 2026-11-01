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
make test-sanitized -j4 RACK_DIR=/path/to/Rack-SDK
```

The standalone harness releases its Rack model factory, so leak checking can
remain enabled.

`patches/polyphony.vcv` is a portable four-voice keyboard → Pink Trombone → VCA
→ stereo audio patch. Select your output device in Audio 2. Use the QWERTY or
ZXCVB rows to play chords. The VCA gates the otherwise continuous synthesizer.


Speech tests cover dictionary pronunciations, number expansion, unknown-word
fallback, common Unicode text, text limits, finite articulatory targets, base
offsets, additive CV, unchanged stored knobs, native expander audio and channel
routing, idle silence, gate/end timing, independent phrase lifetimes, queued
starts, stop cancellation, repeat, pitch CV, bypass, reset and patch persistence.
Both the module and vendored English frontend are instrumented by the sanitizer
target. These checks verify control and audio behavior, not subjective speech
intelligibility.

To render a sentence through the physical model for listening:

```sh
LD_LIBRARY_PATH="$RACK_DIR" build/tests/regression --render-speech /tmp/speech.wav "Hello world."
```

`patches/speech.vcv` places the expander immediately left of a neutral host,
connected to stereo Audio 2. Choose the audio device, then click Speak. All text
processing is offline; there are no runtime dictionary/model downloads.

Word/smoothing regressions check repeated and expanded words, exact boundaries,
next-word wrap, button/trigger equivalence, held gates, latched selection,
polyphonic selection, queued compilation, phrase versus word END timing, zero
smoothing compatibility, independent pitch/envelopes and sample-rate-independent
smoothing time constants.
