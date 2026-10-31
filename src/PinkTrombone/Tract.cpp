//
//  Tract
//  PinkTrombone - VST3
//
//  Created by Samuel Tarakajian on 9/5/19.
//

#include "Tract.hpp"
#include <math.h>
#include "util.h"
#include <cstring>
#include "../plugin.hpp"

typedef struct t_transient
{
	int position;
	sample_t timeAlive;
	sample_t lifeTime;
	sample_t strength;
	sample_t exponent;
	bool living;
} t_transient;

static sample_t restingDiameter(int index, int n) {
    if (index < TRACT_BOUND_A * (sample_t)n - 0.5) return TRACT_DIAMETER_A;
    if (index < TRACT_BOUND_B * (sample_t)n) return TRACT_DIAMETER_B;
    return TRACT_DIAMETER_C;
}

void initializeTractProps(t_tractProps *props, int n)
{
	props->n = n;
	props->bladeStart = BLADE_START;
	props->tipStart = TIP_START;
	props->lipStart = LIP_START;
	props->noseLength = NOSE_LENGTH;
	props->bladeStart = (int)floor(props->bladeStart * (sample_t)n / NUM_CONSTRICTIONS);
	props->tipStart = (int)floor(props->tipStart * (sample_t)n / NUM_CONSTRICTIONS);
	props->lipStart = (int)floor(props->lipStart * (sample_t)n / NUM_CONSTRICTIONS);
	props->tongueIndex = props->bladeStart;
	props->tongueDiameter = TONGUE_DIAMETER;
	props->tractDiameter = (sample_t *)calloc(n, sizeof(sample_t));
	props->noseLength = (int)floor(props->noseLength * (sample_t)props->n / NUM_CONSTRICTIONS);
	props->noseDiameter = (sample_t *)calloc(props->noseLength, sizeof(sample_t));
	props->noseStart = props->n - props->noseLength + 1;
	props->noseOffset = NOSE_OFFSET;
}

void destroyTractProps(t_tractProps *props)
{
	if (props->tractDiameter)
		free(props->tractDiameter);

	if (props->noseDiameter)
		free(props->noseDiameter);
}

Tract::Tract(sample_t sampleRate, sample_t blockTime, t_tractProps *props) : lipOutput(0),
																			 noseOutput(0),
																			 glottalReflection(GLOTTAL_REFLECTION),
																			 lipReflection(LIP_REFLECTION),
																			 lastObstruction(-1),
																			 fade(TRACT_FADE),				//0.9999,
																			 movementSpeed(MOVEMENT_SPEED), //cm per second
																			 velumTarget(0.01),
																			 constrictionIndex(3.0),   // TODO values ex recto
																			 constrictionDiameter(1.0) // TODO values ex recto
{
	this->sampleRate = sampleRate;
	this->blockTime = blockTime;
	this->transients = (t_transient *)calloc(MAX_TRANSIENTS, sizeof(t_transient));
	this->transientCount = 0;
	this->tractProps = props;
	this->init();
}

void Tract::init()
{
	this->diameter = (sample_t *)calloc(this->tractProps->n, sizeof(sample_t));
	this->restDiameter = (sample_t *)calloc(this->tractProps->n, sizeof(sample_t));
	this->targetDiameter = (sample_t *)calloc(this->tractProps->n, sizeof(sample_t));
	this->newDiameter = (sample_t *)calloc(this->tractProps->n, sizeof(sample_t));
	for (int i = 0; i < this->tractProps->n; i++)
	{
        sample_t diameter = restingDiameter(i, tractProps->n);
		this->diameter[i] = this->restDiameter[i] = this->targetDiameter[i] = this->newDiameter[i] = diameter;
	}
	this->R = (sample_t *)calloc(this->tractProps->n, sizeof(sample_t));
	this->L = (sample_t *)calloc(this->tractProps->n, sizeof(sample_t));
	this->reflection = (sample_t *)calloc(this->tractProps->n + 1, sizeof(sample_t));
	this->newReflection = (sample_t *)calloc(this->tractProps->n + 1, sizeof(sample_t));
	this->junctionOutputR = (sample_t *)calloc(this->tractProps->n + 1, sizeof(sample_t));
	this->junctionOutputL = (sample_t *)calloc(this->tractProps->n + 1, sizeof(sample_t));
	this->A = (sample_t *)calloc(this->tractProps->n, sizeof(sample_t));
	this->maxAmplitude = (sample_t *)calloc(this->tractProps->n, sizeof(sample_t));

	this->noseR = (sample_t *)calloc(this->tractProps->noseLength, sizeof(sample_t));
	this->noseL = (sample_t *)calloc(this->tractProps->noseLength, sizeof(sample_t));
	this->noseJunctionOutputR = (sample_t *)calloc(this->tractProps->noseLength + 1, sizeof(sample_t));
	this->noseJunctionOutputL = (sample_t *)calloc(this->tractProps->noseLength + 1, sizeof(sample_t));
	this->noseReflection = (sample_t *)calloc(this->tractProps->noseLength + 1, sizeof(sample_t));
	this->noseDiameter = (sample_t *)calloc(this->tractProps->noseLength, sizeof(sample_t));
	this->noseA = (sample_t *)calloc(this->tractProps->noseLength, sizeof(sample_t));
	this->noseMaxAmplitude = (sample_t *)calloc(this->tractProps->noseLength, sizeof(sample_t));
	for (int i = 0; i < this->tractProps->noseLength; i++)
	{
		sample_t diameter;
		sample_t d = 2.0 * ((sample_t)i / (sample_t)this->tractProps->noseLength);
		if (d < 1.0)
			diameter = 0.4 + 1.6 * d;
		else
			diameter = 0.5 + 1.5 * (2.0 - d);
		diameter = fmin(diameter, 1.9);
		this->noseDiameter[i] = diameter;
	}
	this->newReflectionLeft = this->newReflectionRight = this->newReflectionNose = 0.0;
    this->noseDiameter[0] = this->velumTarget;
    this->calculateNoseReflections();
    this->calculateReflections();
	memcpy(this->tractProps->tractDiameter, this->diameter, sizeof(sample_t) * this->tractProps->n);
	memcpy(this->tractProps->noseDiameter, this->noseDiameter, sizeof(sample_t) * this->tractProps->noseLength);
}

Tract::~Tract()
{
	if (this->diameter)
		free(this->diameter);

	if (this->restDiameter)
		free(this->restDiameter);

	if (this->targetDiameter)
		free(this->targetDiameter);

	if (this->newDiameter)
		free(this->newDiameter);

	if (this->R)
		free(this->R);

	if (this->L)
		free(this->L);

	if (this->reflection)
		free(this->reflection);

	if (this->newReflection)
		free(this->newReflection);

	if (this->junctionOutputR)
		free(this->junctionOutputR);

	if (this->junctionOutputL)
		free(this->junctionOutputL);

	if (this->A)
		free(this->A);

	if (this->maxAmplitude)
		free(this->maxAmplitude);

	if (this->noseR)
		free(this->noseR);

	if (this->noseL)
		free(this->noseL);

	if (this->noseJunctionOutputR)
		free(this->noseJunctionOutputR);

	if (this->noseJunctionOutputL)
		free(this->noseJunctionOutputL);

	if (this->noseReflection)
		free(this->noseReflection);

	if (this->noseDiameter)
		free(this->noseDiameter);

	if (this->noseA)
		free(this->noseA);

	if (this->noseMaxAmplitude)
		free(this->noseMaxAmplitude);

	if (this->transients)
		free(this->transients);
}

long Tract::getTractIndexCount()
{
	return this->tractProps->n;
}

long Tract::tongueIndexLowerBound()
{
	return this->tractProps->bladeStart + 2;
}

long Tract::tongueIndexUpperBound()
{
	return this->tractProps->tipStart - 3;
}

void Tract::addTransient(int position)
{
	if (this->transientCount < MAX_TRANSIENTS)
	{
        t_transient* trans = &transients[transientCount++];
        trans->position = position;
        trans->timeAlive = 0;
        trans->lifeTime = 0.2;
        trans->strength = 0.3;
        trans->exponent = 200;
        trans->living = true;
	}
}

void Tract::addTurbulenceNoise(sample_t turbulenceNoise, sample_t glottalNoiseModulator)
{
	if (this->constrictionIndex < 2.0 || this->constrictionIndex >= (sample_t)(this->tractProps->n - 2))
	{
		return;
	}
	if (this->constrictionDiameter <= 0.0)
		return;
	sample_t intensity = this->fricativeIntensity;
	this->addTurbulenceNoiseAtIndex(0.66 * turbulenceNoise * intensity, this->constrictionIndex, this->constrictionDiameter, glottalNoiseModulator);
}

void Tract::addTurbulenceNoiseAtIndex(sample_t turbulenceNoise, sample_t index, sample_t diameter, sample_t glottalNoiseModulator)
{
    if (!std::isfinite(index) || index < 0 || index >= tractProps->n - 2) return;
	long i = (long)floor(index);
	sample_t delta = index - (sample_t)i;
	turbulenceNoise *= glottalNoiseModulator;
	sample_t thinness0 = rack::clamp((float)8.0 * (0.7 - diameter),(float) 0.0,(float) 1.0);
	sample_t openness = rack::clamp((float)30.0 * (diameter - 0.3),(float) 0.0,(float) 1.0);
	sample_t noise0 = turbulenceNoise * (1.0 - delta) * thinness0 * openness;
	sample_t noise1 = turbulenceNoise * delta * thinness0 * openness;
	this->R[i + 1] += noise0 / 2.0;
	this->L[i + 1] += noise0 / 2.0;
	this->R[i + 2] += noise1 / 2.0;
	this->L[i + 2] += noise1 / 2.0;
}

void Tract::calculateReflections()
{
    int areaIndex = 0;
    if (useSimd) for (; areaIndex + 3 < tractProps->n; areaIndex += 4) {
        auto d = rack::simd::float_4::load(diameter + areaIndex);
        (d * d).store(A + areaIndex);
    }
    for (int i = areaIndex; i < tractProps->n; ++i) A[i] = diameter[i] * diameter[i];
    int reflectionIndex = 1;
    if (useSimd) for (; reflectionIndex + 3 < tractProps->n; reflectionIndex += 4) {
        using rack::simd::float_4;
        auto previous = float_4::load(A + reflectionIndex - 1);
        auto current = float_4::load(A + reflectionIndex);
        auto closed = current == 0.f;
        auto denominator = rack::simd::ifelse(closed, float_4(1.f), previous + current);
        auto next = rack::simd::ifelse(closed, float_4(.999f), (previous - current) / denominator);
        float_4::load(newReflection + reflectionIndex).store(reflection + reflectionIndex);
        next.store(newReflection + reflectionIndex);
    }
    for (int i = reflectionIndex; i < tractProps->n; ++i) {
        reflection[i] = newReflection[i];
        newReflection[i] = A[i] == 0.f ? .999f : (A[i - 1] - A[i]) / (A[i - 1] + A[i]);
    }

	//now at junction with nose
	this->reflectionLeft = this->newReflectionLeft;
	this->reflectionRight = this->newReflectionRight;
	this->reflectionNose = this->newReflectionNose;
	sample_t sum = this->A[this->tractProps->noseStart] + this->A[this->tractProps->noseStart + 1] + this->noseA[0];
	this->newReflectionLeft = (2.0 * this->A[this->tractProps->noseStart] - sum) / sum;
	this->newReflectionRight = (2 * this->A[this->tractProps->noseStart + 1] - sum) / sum;
	this->newReflectionNose = (2 * this->noseA[0] - sum) / sum;
}

void Tract::calculateNoseReflections()
{
	for (int i = 0; i < this->tractProps->noseLength; i++)
	{
		this->noseA[i] = this->noseDiameter[i] * this->noseDiameter[i];
	}
	for (int i = 1; i < this->tractProps->noseLength; i++)
	{
		this->noseReflection[i] = (this->noseA[i - 1] - this->noseA[i]) / (this->noseA[i - 1] + this->noseA[i]);
	}
}

void Tract::finishBlock()
{
    if (!shapeMoving && !reflectionDirty) return;
    bool changed = reshapeTract(blockTime);
    if (changed || reflectionDirty) {
        calculateReflections();
        memcpy(tractProps->tractDiameter, diameter, sizeof(sample_t) * tractProps->n);
        memcpy(tractProps->noseDiameter, noseDiameter, sizeof(sample_t) * tractProps->noseLength);
    }
    // One final update aligns the previous and new reflections at rest.
    reflectionDirty = changed;
}

void Tract::setRestDiameter(sample_t tongueIndex, sample_t tongueDiameter)
{
    shapeMoving = true;
	this->tractProps->tongueIndex = tongueIndex;
	this->tractProps->tongueDiameter = tongueDiameter;
    // Restore sections which leave the tongue's range when the blade moves.
    for (int i = 0; i < tractProps->bladeStart; ++i) {
        restDiameter[i] = restingDiameter(i, tractProps->n);
    }
	for (long i = this->tractProps->bladeStart; i < this->tractProps->lipStart; i++)
	{
        int span = this->tractProps->tipStart - this->tractProps->bladeStart;
        if (span == 0) span = 1;
		sample_t t = 1.1 * M_PI * (sample_t)(tongueIndex - i) / (sample_t)span;
		sample_t fixedTongueDiameter = 2 + (tongueDiameter - 2) / 1.5;
		sample_t curve = (1.5 - fixedTongueDiameter + 1.7) * rack::simd::cos(t);
		if (i == this->tractProps->bladeStart - 2 || i == this->tractProps->lipStart - 1)
			curve *= 0.8;
		if (i == this->tractProps->bladeStart || i == this->tractProps->lipStart - 2)
			curve *= 0.94;
		this->restDiameter[i] = 1.5 - curve;
	}
	for (long i = 0; i < this->tractProps->n; i++)
	{
		this->targetDiameter[i] = this->restDiameter[i];
	}
}

void Tract::setConstriction(sample_t cindex, sample_t cdiam, sample_t fricativeIntensity)
{
    shapeMoving = true;
	this->constrictionIndex = cindex;
	this->constrictionDiameter = cdiam;
	this->fricativeIntensity = fricativeIntensity;

	// This is basically the Tract touch handling code
	this->velumTarget = 0.01;
	if (this->constrictionIndex > this->tractProps->noseStart && this->constrictionDiameter < -this->tractProps->noseOffset)
	{
		this->velumTarget = 0.4;
	}
	if (this->constrictionDiameter < -0.85 - this->tractProps->noseOffset)
	{
		return;
	}

	sample_t diameter = this->constrictionDiameter - 0.3;
	if (diameter < 0)
		diameter = 0;
	long width = 2;
	if (this->constrictionIndex < 25)
		width = 10;
	else if (this->constrictionIndex >= this->tractProps->tipStart)
		width = 5;
	else
		width = 10.0 - 5 * (this->constrictionIndex - 25) / ((sample_t)this->tractProps->tipStart - 25.0);
	if (this->constrictionIndex >= 2 && this->constrictionIndex < this->tractProps->n && diameter < 3)
	{
		long intIndex = round(this->constrictionIndex);
		for (long i = -ceil(width) - 1; i < width + 1; i++)
		{
			if (intIndex + i < 0 || intIndex + i >= this->tractProps->n)
				continue;
			sample_t relpos = (intIndex + i) - this->constrictionIndex;
			relpos = std::fabs(relpos) - 0.5;
			sample_t shrink;
			if (relpos <= 0)
				shrink = 0;
			else if (relpos > width)
				shrink = 1;
			else
				shrink = 0.5 * (1 - rack::simd::cos(M_PI * relpos / (sample_t)width));
			if (diameter < this->targetDiameter[intIndex + i])
			{
				this->targetDiameter[intIndex + i] = diameter + (this->targetDiameter[intIndex + i] - diameter) * shrink;
			}
		}
	}
}

void Tract::processTransients()
{
    for (int i = 0; i < transientCount;) {
        t_transient* trans = &transients[i];
        if (trans->timeAlive > trans->lifeTime) {
            transients[i] = transients[--transientCount];
            continue;
        }
        sample_t amplitude = trans->strength * rack::simd::pow(2.0, -trans->exponent * trans->timeAlive);
        R[trans->position] += amplitude / 2.0;
        L[trans->position] += amplitude / 2.0;
        trans->timeAlive += 1.0 / (sampleRate * 2.0);
        ++i;
    }
}

bool Tract::reshapeTract(sample_t deltaTime)
{
    bool changed = false;
    bool moving = false;
	sample_t amount = deltaTime * this->movementSpeed;
	;
	int newLastObstruction = -1;

	for (int i = 0; i < this->tractProps->n; i++)
	{
		sample_t diameter = this->diameter[i];
		sample_t targetDiameter = this->targetDiameter[i];
		if (diameter <= 0)
			newLastObstruction = i;
		sample_t slowReturn;
		if (i < this->tractProps->noseStart)
			slowReturn = 0.6;
		else if (i >= this->tractProps->tipStart)
			slowReturn = 1.0;
		else
			slowReturn = 0.6 + 0.4 * (i - this->tractProps->noseStart) / (this->tractProps->tipStart - this->tractProps->noseStart);
		this->diameter[i] = moveTowards(diameter, targetDiameter, slowReturn * amount, 2 * amount);
        changed |= this->diameter[i] != diameter;
        moving |= this->diameter[i] != targetDiameter;
	}
	if (this->lastObstruction > -1 && newLastObstruction == -1 && this->noseA[0] < 0.05)
	{
		this->addTransient(this->lastObstruction);
	}
	this->lastObstruction = newLastObstruction;

	amount = deltaTime * this->movementSpeed;
    sample_t oldNose = noseDiameter[0];
	this->noseDiameter[0] = moveTowards(this->noseDiameter[0], this->velumTarget, amount * 0.25, amount * 0.1);
    changed |= noseDiameter[0] != oldNose;
    shapeMoving = moving || noseDiameter[0] != velumTarget;
	this->tractProps->noseDiameter[0] = this->noseDiameter[0];
	this->noseA[0] = this->noseDiameter[0] * this->noseDiameter[0];
    if (noseDiameter[0] != oldNose && tractProps->noseLength > 1)
        noseReflection[1] = (noseA[0] - noseA[1]) / (noseA[0] + noseA[1]);
    return changed;
}

void Tract::runStep(sample_t glottalOutput, sample_t turbulenceNoise, sample_t lambda, sample_t glottalNoiseModulator)
{
    // Peak histories were private, never read, and consumed global rand().

	//mouth
	this->processTransients();
	this->addTurbulenceNoise(turbulenceNoise, glottalNoiseModulator);

	//this->glottalReflection = -0.8 + 1.6 * Glottis.newTenseness;
	this->junctionOutputR[0] = this->L[0] * this->glottalReflection + glottalOutput;
	this->junctionOutputL[this->tractProps->n] = this->R[this->tractProps->n - 1] * this->lipReflection;


	for (int i = 1; i < this->tractProps->n; i++)
	{
		sample_t r = this->reflection[i] * (1 - lambda) + this->newReflection[i] * lambda;
		sample_t w = r * (this->R[i - 1] + this->L[i]);
		this->junctionOutputR[i] = this->R[i - 1] - w;
		this->junctionOutputL[i] = this->L[i] + w;
	}

	//now at junction with nose
	int i = this->tractProps->noseStart;
	sample_t r = this->newReflectionLeft * (1 - lambda) + this->reflectionLeft * lambda;
	this->junctionOutputL[i] = r * this->R[i - 1] + (1 + r) * (this->noseL[0] + this->L[i]);
	r = this->newReflectionRight * (1 - lambda) + this->reflectionRight * lambda;
	this->junctionOutputR[i] = r * this->L[i] + (1 + r) * (this->R[i - 1] + this->noseL[0]);
	r = this->newReflectionNose * (1 - lambda) + this->reflectionNose * lambda;
	this->noseJunctionOutputR[0] = r * this->noseL[0] + (1 + r) * (this->L[i] + this->R[i - 1]);

    int mouthUpdate = 0;
    if (useSimd) for (; mouthUpdate + 3 < tractProps->n; mouthUpdate += 4) {
        using rack::simd::float_4;
        (float_4::load(junctionOutputR + mouthUpdate) * .999f).store(R + mouthUpdate);
        (float_4::load(junctionOutputL + mouthUpdate + 1) * .999f).store(L + mouthUpdate);
    }
	for (int i = mouthUpdate; i < this->tractProps->n; i++)
	{
		this->R[i] = this->junctionOutputR[i] * 0.999;
		this->L[i] = this->junctionOutputL[i + 1] * 0.999;

		//this->R[i] = Math.clamp(this->junctionOutputR[i] * this->fade, -1, 1);
		//this->L[i] = Math.clamp(this->junctionOutputL[i+1] * this->fade, -1, 1);

	}

	this->lipOutput = this->R[this->tractProps->n - 1];

	//nose
	this->noseJunctionOutputL[this->tractProps->noseLength] = this->noseR[this->tractProps->noseLength - 1] * this->lipReflection;


	for (int i = 1; i < this->tractProps->noseLength; i++)
	{
		sample_t w = this->noseReflection[i] * (this->noseR[i - 1] + this->noseL[i]);
		this->noseJunctionOutputR[i] = this->noseR[i - 1] - w;
		this->noseJunctionOutputL[i] = this->noseL[i] + w;
	}


	for (int i = 0; i < this->tractProps->noseLength; i++)
	{
		this->noseR[i] = this->noseJunctionOutputR[i] * this->fade;
		this->noseL[i] = this->noseJunctionOutputL[i + 1] * this->fade;

		//this->noseR[i] = Math.clamp(this->noseJunctionOutputR[i] * this->fade, -1, 1);
		//this->noseL[i] = Math.clamp(this->noseJunctionOutputL[i+1] * this->fade, -1, 1);

	}

	this->noseOutput = this->noseR[this->tractProps->noseLength - 1];
}

void Tract::resetWaves() {
    std::fill_n(R, tractProps->n, 0.f);
    std::fill_n(L, tractProps->n, 0.f);
    std::fill_n(noseR, tractProps->noseLength, 0.f);
    std::fill_n(noseL, tractProps->noseLength, 0.f);
    transientCount = 0;
    lipOutput = noseOutput = 0.f;
}
