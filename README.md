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

## Pink Speech expander

Place **Pink Speech directly to the left of Pink Trombone**. Type ordinary
English, wait for Ready, and press Speak or Ctrl+Enter. Stop cancels playback
and queued starts; Repeat restarts the phrase. Text and settings are saved with
the patch. Initialize restores the example sentence. The host is silent while
the expander is idle; detach it to return to manual synthesis.

The English frontend runs locally, with no download, account, service or Python
runtime. It uses CMU Flite's tokenizer, number expansion, 36,963-entry dictionary,
learned letter-to-sound fallback, duration model and intonation. Unfamiliar words
and invented names are pronounced by rules rather than requiring phonetic
markup. English spelling remains ambiguous, so names, acronyms and unusual
spellings may need respelling. Common accented Latin letters and typographic
quotes are normalized; this is a US English frontend, not a multilingual one.
Text is limited to 8,192 UTF-8 bytes and 4,096 speech sounds per phrase.

Phoneme targets, coarticulation and envelopes drive the actual Pink Trombone
vocal tract. Flite supplies no audio recordings or acoustic voice. The result
has Pink Trombone's robotic character; pronunciation coverage does not imply
human speech quality. The controller is informed by
[zakaton's programmable Pink Trombone](https://github.com/zakaton/Pink-Trombone).
[Samuel](https://github.com/vvolhejn/samuel) uses a learned controller and a
larger neural TTS stack; this implementation uses a native offline phonetic
controller instead. Upstream notices are included in the repository.

With the expander attached, the host's BASE header changes to OFFSET. Each base
knob adds its difference from its factory default to the speech target, then
the usual CV, attenuverter and per-voice spread are applied. Factory-default
base knobs therefore produce the intended articulation. Existing knob settings
can deliberately distort it; Initialize the host for neutral offsets. Knobs are
never overwritten by speech, and their usual values return when detached.
Always voice does not force sound during speech rests. Pitch wobble still
controls whether the vibrato amount is applied. Voice/Noise audio inputs can
replace the exciters while speech continues to move the tract.

- Speak and Stop accept polyphonic rising-edge triggers. Mono triggers broadcast;
  shorter polyphonic cables affect only their existing lanes. The buttons act on
  every active voice. A trigger restarts that voice at the beginning.
- Voices or the widest expander input sets 1–16 speech lanes. Each lane has its
  own playback position. Editing text does not interrupt existing phrases;
  newly triggered voices use the new text. A Speak received while compiling
  waits for the newest text, and Stop cancels that wait.
- Rate is 0.25–3x, with additive CV at +0.2x/V, limited to 0.15–4x. Pitch adds
  +/-24 semitones and standard 1 V/oct CV, without changing speech duration.
- Gate is 10 V while speaking. End sends a 1 ms, 10 V pulse on phrase completion,
  including each repetition. Both outputs are polyphonic. Stop, disconnect,
  bypass and channel removal cancel playback without an end pulse.

Compilation and allocation happen on a background thread. The audio callback
uses immutable phrase plans and a Rack double-buffered expander message;
retired plans are reclaimed by the worker. Audio processing takes no text locks.
See `tests/patches/speech.vcv` for a ready-to-connect example.

### Word stepping and smoothing

**Word** (button or trigger) plays a single spoken word, then waits. Each trigger
advances one word and wraps to the first at the end, regardless of Repeat.
The **SELECT** input overrides stepping: 0–10 V divides the available word list
into equal bins, from first to last. Its voltage is latched on the Word trigger,
so subsequent triggers repeat that word until selection changes. Selection does
not move the automatic sequence position. Expanded numbers count as their
individual spoken words; repeated words remain separate. Speak still plays the
whole text and resets the automatic word sequence to the beginning. Stop leaves
the next-word position intact; Initialize or newly compiled text resets it.

**END** follows the playback mode: one 1 ms pulse when a triggered word finishes,
or one when the entire phrase finishes after Speak. Word mode never runs on
into the next word automatically. Stop/retrigger interrupts without sending a
completion pulse. Speak takes priority over Word if both arrive together, and
Stop takes priority over both. Word, selection and smoothing CV are polyphonic.
Queued Word triggers retain their selection while new text is compiling.

**SMOOTH** softens tongue/constriction, timbre, voicing and noise transitions,
with a time constant from zero to 250 ms. CV adds 10% per volt. Zero preserves
the existing articulation exactly. Higher values can blur consonants into a
slurred or lisp-like sound. Pitch, playback speed and gate/end timing remain
independent of smoothing. Each voice has its own smoothing state.
