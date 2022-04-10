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

  - [x] Voicebox Level (Basically a VCA?)

    > This *might* not be necessary. This appears to be glottis->loudness (or maybe glottis->intensity, not sure what the difference is?) which is a simple VCA, which would imply it doesn't need to be included. *However*, this would happen between the Glottis and Tract, which means the tract may have filters/noise that can decay after the glotis has been shut. ~~I also might be able to use `isTouched` and change `alwaysVocied` to false.~~ That had almost no effect. Trying via modulating intensity directly... It works. Values need clamped between 0 and 1, so envelope input will be divided by 10.

  - [ ] Pallete Close / Nasal Only (Gate)

    > I **think** this is just ToungeY going negative

- [x] Limit Pitch range to prevent loss of output - limit seems to be ~ 135hz

- [x] Figure out how to disable internal vibrato

- [x] Figure out what the lambda values are for

  > They seem to have no effect?

- [x] Figure out why pitch wobble occassionaly stops?

  > ~~This might have to do with `glottis->totalTime` and the size of timeStep, but I'm not sure yet. This really does ruin the vocal effect to not have this, so this NEEDS fixed~~
  time was limited to 512. The solution is hacky and probably in a bad place, but at least it's fixed.

- [ ] Determine if any other parameters should have controls

  - [x] Internal Vibrato

    `glottis->finishBlock(params[VIBDEPTH_PARAM].getValue());`

    > This makes a HUGE difference for how much it actually sounds like a voice. Without this it's not all that far from a normal formant filter

  - [x] Tenseness

    `glottis->setTargetTenseness(tenseness);`

  - [x] Fricative Intensity

    This determines how much noise is added when the threat is nearly closed. It would definitely be nice to be able to adjust this on the panel. Valid values range between 0 and 1

  - [ ] Nose len

    > At least for now this doesn't appear to do anything, but I haven't yet figured out how to colse the pallete / use the nose, so that makes sense.
    >
    > ... Okay, obviously it does something. Set the nose length to 44 (the same as `n`, which I still don't know what is) and it seems to be closer to the web version now?

  - [ ] Tip & Lip start

    > Tip start makes a large difference. Need to determine how to modulate it. I think it should only be able to me subtracted from (values less than 32)
    >
    > Changing Lip Start works, I can't really tell what it's doing. I think it should only be able to be increased (values greater than 39)
    >
    > I suspect that Lip start must always be greater than tip start, so rather than give it direct control I will likely make it an offset, and abs() any input given to it's control port.
    >
    > I'm not yet sure if these can be changed while the code is running without dramatic issues, , or if they should be CV modulatable or just have knobs.

  - [ ] TRACT_DIAMETER_*

    > Changing these does seem to matter, though I'm not sure if it's doing much more than moving the range of the knobs, or at least what they snap to when at a bound. I'll need to actually map them to some knobs on the panel and see how it goes.
    >
    > I'm not yet sure if these can be changed while the code is running without dramatic issues, or if they should be CV modulatable or just have knobs.

    ```c++
    	for (int i = 0; i < this->tractProps->n; i++)
    	{
    		sample_t diameter = 0;
    		if (i < TRACT_BOUND_A * (sample_t)this->tractProps->n - 0.5)
    			diameter = TRACT_DIAMETER_A;
    		else if (i < TRACT_BOUND_B * (sample_t)this->tractProps->n)
    			diameter = TRACT_DIAMETER_B;
    		else
    			diameter = TRACT_DIAMETER_C;
    		this->diameter[i] = this->restDiameter[i] = this->targetDiameter[i] = this->newDiameter[i] = diameter;
    	}
    ```

  - [ ] Aspiration and Fricative filter cutoffs & Q's

    > Okay, so, yes changing the Fricative filter's cutoff and Q matters, but the default values are pretty good and I don't see other values being particularly useful HOWEVER, it might be nice to have a filter tracking switch. This would almost certainly not sound human, but I think it's a good option and would make the closed thoat's filter playable.
    >
    > The Aspiration filters values seem to have an even smaller effect. It does add some noise into the system, but it does make the vocie sound more believable and is subtle enough that I don't see a reason someone would want to turn it off or change it anyway.

    ```c++
    	sample_t aspiration = this->intensity * (1 - sqrt(this->targetTenseness)) * this->getNoiseModulator() * noiseSource;
    	aspiration *= 0.2 + 0.02 * simplex1(this->totalTime * 1.99);
    	out += aspiration;
    ```

    

- [ ] Limit controls to prevent inf and NaN

  > This seems to be caused by intense, fast modulation, and as I didn't write the DSP myself and don't fully understand it, fixing it will be rather difficult. I'll include a global slew control knob in the final version and try to put a tick mark indicating a risk of bad output on the panel. Rack 2's cables do catch the value and prevent it from progating though, so I won't clamp internally.
  >
  > The global slew should also help with clicks caused by square-y modulation.

- [ ] Redesign Panel

  - [ ] Attenu**verters** on everything.
  - [ ] Don't stack the knobs unless threre's a dramatic size increase
  - [ ] Make sure fonts are bigger
  - [ ] NanoVG From Iv for showing state
  - [ ] ~~RGB LEDs for indicators~~
    - [ ] ~~Tongue: Red: X, Blue: Y~~
    - [ ] ~~Cavity: Red: X, Blue: Y~~
    - [ ] ~~Voicebox: Red: - pitch, Green: + pitch, Blue, VCA~~
    - [ ] ~~Palette close - single, small LED indicator~~
  - [ ] Tune & Octave knobs
    - [ ] Slew control on main v/oct input
  - [ ] Master Slew
  - [ ] FM input (turn off internal vibrato)
  - [ ] Include attribution to original on panel
