# PinkTromboneVCV
Pink Trombone for VCV Rack

Original Pink Trombone Code is MIT License'd & Copyright (c) 2021 cutelabnyc
available at https://github.com/cutelabnyc/pink-trombone-cpp

# TODO

- [ ] Get all I/O working

  - [x] Output

  - [x] Tongue X

  - [x] Tongue Y

  - [x] Cavity X

  - [x] Cavity Y

  - [x] Voicebox Pitch (V/Oct)

  - [ ] Voicebox Level (Basically a VCA)

    > This might not be necessary. This appears to be glottis->loudness, which is a simple VCA. No need to bake that in.

  - [ ] Pallete Close / Nasal Only (Gate)

- [x] Figure out how to disable internal vibrato

- [ ] Figure out what the lambda values are for

- [ ] Determine if any other parameters should have controls

  - [x] Internal Vibrato

    `glottis->finishBlock(params[VIBDEPTH_PARAM].getValue());`

  - [x] Tenseness

    `glottis->setTargetTenseness(tenseness);`

  - [ ] Nose len

  - [ ] Tip & Lip start

  - [ ] TRACT_DIAMETER_*

  - [ ] asp and fri filter cutoffs

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
