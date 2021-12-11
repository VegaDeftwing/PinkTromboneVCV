# PinkTromboneVCV
Pink Trombone for VCV Rack

Original Pink Trombone Code is MIT License'd & Copyright (c) 2021 cutelabnyc
available at https://github.com/cutelabnyc/pink-trombone-cpp

# TODO

- [ ] Get all I/O working

  - [x] Output

  - [x] Tongue X - ahh  -> EEE

  - [x] Tongue Y - ahh -> ayy

  - [x] Cavity X - .46 to .999

  - [x] Cavity Y - .575 to .69125

  - [x] Voicebox Pitch (V/Oct) - 140Hz to 4186Hz on Tune

    > It is possible to go down a bit lower, but it does sometimes fail. 140 should be safe. It can go lower with the v/oct input without failing for some reason.

  - [ ] Voicebox Level (Basically a VCA)

    > This might not be necessary. This appears to be glottis->loudness, which is a simple VCA. No need to bake that in.

  - [ ] Pallete Close / Nasal Only (Gate)

- [ ] Limit Pitch range to prevent loss of output - limit seems to be ~ 135hz

- [x] Figure out how to disable internal vibrato

- [x] Figure out what the lambda values are for

  > They seem to have no effect?

- [ ] Determine if any other parameters should have controls

  - [x] Internal Vibrato

    `glottis->finishBlock(params[VIBDEPTH_PARAM].getValue());`

  - [x] Tenseness

    `glottis->setTargetTenseness(tenseness);`

  - [ ] Nose len

  - [ ] Tip & Lip start

  - [ ] TRACT_DIAMETER_*

  - [ ] asp and fri filter cutoffs

- [ ] Limit controls to prevent inf and NaN

- [ ] Redesign Panel

  - [ ] RGB LEDs for indicators
    - [ ] Tongue: Red: X, Blue: Y
    - [ ] Cavity: Red: X, Blue: Y
    - [ ] Voicebox: Red: - pitch, Green: + pitch, Blue, VCA
    - [ ] Palette close - single, small LED indicator
  - [ ] Tune & Octave knobs
  - [ ] Master Slew
  - [ ] FM input (turn off internal vibrato)
  - [ ] Include attribution to original on panel
