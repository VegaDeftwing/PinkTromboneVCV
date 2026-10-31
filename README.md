# PinkTromboneVCV
Pink Trombone for VCV Rack

Original Pink Trombone C++ Code is MIT License'd & Copyright (c) 2021 cutelabnyc
available at https://github.com/cutelabnyc/pink-trombone-cpp

The original Pink Trombone (Javascript) is Copyright 2017 Neil Thapen and also MIT licensed. It is available at https://dood.al/pinktrombone/.

However, this project is **GPL V3.**

## Polyphony and the interactive panel

The module supports up to 16 voices. The widest connected CV or audio input
sets the output width; mono inputs broadcast, and missing lanes in shorter
polyphonic cables supply zero. Inactive voices retain their state. The diagram
shows the live shape and CV-modulated positions of the first voice through a
thread-safe display snapshot.

The NanoVG tract drawing, polar touch mapping and voicebox keyboard are ported
from [Neil Thapen's original browser interface](https://github.com/evykassirer/pink-trombone).
Its MIT notice is included in `LICENSE-PinkTrombone-UI.txt`.

- Drag the tongue region to shape vowels. Drag the tract to make temporary
  constrictions or open the nasal passage; Shift-drag holds the new shape.
- Drag the voicebox horizontally for pitch and vertically for tenseness.
- Always voice and Pitch wobble are independent saved switches, with Rack's
  normal undo behavior. They retain the Level and Vibrato settings. With Always
  voice off, touching the voicebox sounds the internal source.
- All original controls are in aligned rows with base value, CV input and CV
  amount, with pitch, linear FM and vibrato at the top. Nose, tip, blade and filter-follow also have CV and attenuverters.
- In polyphony a third knob column, Spread, adds fixed independent offsets to
  each voice. Zero disables variation, and mono operation ignores spread.
  The voice personalities are stored with the patch. Full pitch spread is
  +/- one semitone; other controls use a quarter of their normal range.

The two left-hand audio jacks turn the waveguide into an effect:

- Voice replaces the glottal exciter. Rack's nominal +/-5 V is scaled to the
  physical model's +/-1 range. Unpatched internal noise is muted in this mode,
  so a silent input does not add an autonomous synthesizer signal.
- Noise replaces the white-noise source feeding aspiration and frication.
  Use it with the Voice input to supply both excitation paths independently.

Both audio inputs support polyphony, mono broadcasting and the existing CV
controls. External Voice processing works independently of Always voice.

## DSP review and changes

The implementation now handles sample-rate changes without recreating voices,
caches unchanged filter coefficients and settled tract geometry, and uses
Rack SIMD for area/reflection calculations and wave damping. All voices and
noise buffers are allocated before audio processing.

The review also corrected fractional nasal waves being truncated to integers,
out-of-bounds turbulence injection at the end of the tract, expired transients
never leaving the pool, reversed Q-knob bounds, singular/reversed tongue bounds,
shared mutable noise initialization, and the vibrato timer's equality-only wrap.
Geometry movement now uses seconds per processed sample instead of treating
128 samples as 128 seconds. Master pitch now matches the exponential Hz display;
its lower range includes the original graphical keyboard. These fixes and the
voice-local noise streams can change the sound of existing patches.

See [tests/README.md](tests/README.md) for builds, regression checks, benchmarks
and a keyboard test patch.

The follow-up review also fixed long-running modulation clock precision at high
sample rates, restored untouched tract sections when the blade moves, and made
the nasal reflection track the moving velum. In effect mode the external noise
path can excite constrictions independently of a silent Voice input. Rack's
module bypass passes the external Voice input through unchanged, preserving its
channel count.
