//
//  Tract.hpp
//  PinkTrombone - VST3
//
//  Created by Samuel Tarakajian on 9/5/19.
//

#ifndef Tract_hpp
#define Tract_hpp

#include <stdio.h>
#include "../JuceLibraryCode/JuceHeader.h"
#include "Glottis.hpp"
#include "config.h"

struct t_transient;

typedef struct t_tractProps {
	int n;
	int lipStart;
	int bladeStart;
	int tipStart;
	int noseStart;
	int noseLength;
	sample_t noseOffset;
	sample_t tongueIndex;
	sample_t tongueDiameter;
	sample_t *noseDiameter;
	sample_t *tractDiameter;
} t_tractProps;

void initializeTractProps(t_tractProps *props, int n);

class Tract {
public:
	Tract(sample_t sampleRate, sample_t blockSize, t_tractProps *p);
	~Tract();
	void runStep(sample_t glottalOutput, sample_t turbulenceNoise, sample_t lambda, Glottis *glottis);
	void finishBlock();
	void setRestDiameter(sample_t tongueIndex, sample_t tongueDiameter);
	void setConstriction(sample_t cindex, sample_t cdiam, sample_t fricativeIntensity);
	sample_t lipOutput;
	sample_t noseOutput;
	
	long getTractIndexCount();
	long tongueIndexLowerBound();
	long tongueIndexUpperBound();
	
private:
	void init();
	void addTransient(int position);
	void addTurbulenceNoise(sample_t turbulenceNoise, Glottis *glottis);
	void addTurbulenceNoiseAtIndex(sample_t turbulenceNoise, sample_t index, sample_t diameter, Glottis *glottis);
	void calculateReflections();
	void calculateNoseReflections();
	void processTransients();
	void reshapeTract(sample_t deltaTime);
	
	sample_t sampleRate, blockTime;
	t_tractProps *tractProps;
	sample_t glottalReflection;
	sample_t lipReflection;
	int lastObstruction;
	sample_t fade;
	sample_t movementSpeed;
	sample_t velumTarget;
	t_transient *transients;
	int transientCount;
	
	sample_t *diameter;
	sample_t *restDiameter;
	sample_t *targetDiameter;
	sample_t *newDiameter;
	
	sample_t *R;
	sample_t *L;
	sample_t *reflection;
	sample_t *newReflection;
	sample_t *junctionOutputR;
	sample_t *junctionOutputL;
	sample_t *A;
	sample_t *maxAmplitude;
	
	sample_t *noseR;
	sample_t *noseL;
	sample_t *noseJunctionOutputR;
	sample_t *noseJunctionOutputL;
	sample_t *noseReflection;
	sample_t *noseDiameter;
	sample_t *noseA;
	sample_t *noseMaxAmplitude;
	
	sample_t reflectionLeft, reflectionRight, reflectionNose;
	sample_t newReflectionLeft, newReflectionRight, newReflectionNose;
	
	sample_t constrictionIndex;
	sample_t constrictionDiameter;
	sample_t fricativeIntensity = 0.0;
};

#endif /* Tract_hpp */
