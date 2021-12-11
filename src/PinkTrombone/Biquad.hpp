//
//  Biquad.hpp
//  PinkTrombone - All
//
//  Created by Samuel Tarakajian on 8/30/19.
//

#ifndef Biquad_hpp
#define Biquad_hpp

#include <stdio.h>
#include "config.h"

class Biquad {
public:
	Biquad(sample_t sampleRate);
	~Biquad();
	void setFrequency(sample_t f);
	void setQ(sample_t f);
	void setGain(sample_t g);
	sample_t runStep(sample_t xn);
private:
	void updateCoefficients();
	
	sample_t frequency, q, gain;
	sample_t a0, a1, a2, b1, b2;
	sample_t xm1, xm2, ym1, ym2;
	sample_t sampleRate, twopiOverSampleRate;
};

#endif /* Biquad_hpp */
