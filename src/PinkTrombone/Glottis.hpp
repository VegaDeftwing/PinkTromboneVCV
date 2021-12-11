//
//  Glottis.hpp
//  PinkTrombone - VST3
//
//  Created by Samuel Tarakajian on 8/28/19.
//

#ifndef Glottis_hpp
#define Glottis_hpp

#include <stdio.h>
#include "config.h"

class Glottis {
public:
	Glottis(double sampleRate);
	~Glottis();
	sample_t runStep(sample_t lambda, sample_t noiseSource);
	void finishBlock();
	sample_t getNoiseModulator();
	void setTargetFrequency(sample_t frequency); // 140
	void setTargetTenseness(sample_t tenseness); // 0.6
	
private:
	void setupWaveform(sample_t lambda);
	sample_t normalizedLFWaveform(sample_t t);
	
	sample_t sampleRate;
	sample_t timeInWaveform;
	sample_t frequency, oldFrequency, newFrequency, smoothFrequency,targetFrequency;
	sample_t oldTenseness, newTenseness, targetTenseness;
	sample_t waveformLength;
	sample_t Rd;
	sample_t alpha;
	sample_t E0;
	sample_t epsilon;
	sample_t shift;
	sample_t Delta;
	sample_t Te;
	sample_t omega;
	sample_t totalTime;
	sample_t intensity, loudness;
	sample_t vibratoAmount;
	sample_t vibratoFrequency;
	bool autoWobble;
	bool isTouched;
	bool alwaysVoice;
};

#endif /* Glottis_hpp */
