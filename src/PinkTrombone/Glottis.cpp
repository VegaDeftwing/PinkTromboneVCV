//
//  Glottis.cpp
//  PinkTrombone - VST3
//
//  Created by Samuel Tarakajian on 8/28/19.
//

#include "Glottis.hpp"
#include <math.h>
#include "noise.hpp"
#include "util.h"

Glottis::Glottis(double sampleRate) :
	timeInWaveform(0),
	oldFrequency(140),
	newFrequency(140),
	smoothFrequency(140),
	targetFrequency(140),
	oldTenseness(0.6),
	newTenseness(0.6),
	targetTenseness(0.6),
	totalTime(0.0),
	intensity(0),
	loudness(1),
	vibratoAmount(VIBRATO_AMOUNT),
	vibratoFrequency(VIBRATO_FREQUENCY),
	autoWobble(false),
	isTouched(false),
	alwaysVoice(true)
{
	this->sampleRate = sampleRate;
	this->setupWaveform(0);
}

void Glottis::setupWaveform(sample_t lambda)
{
	this->frequency = this->oldFrequency * (1 - lambda) + this->newFrequency * lambda;
	sample_t tenseness = this->oldTenseness * (1 - lambda) + this->newTenseness * lambda;
	this->Rd = 3 * (1 - tenseness);
	this->waveformLength = 1.0 / this->frequency;
	
	sample_t Rd = this->Rd;
	if (Rd < 0.5) Rd = 0.5;
	if (Rd > 2.7) Rd = 2.7;
	// normalized to time = 1, Ee = 1
	sample_t Ra = -0.01 + 0.048 * Rd;
	sample_t Rk = 0.224 + 0.118 * Rd;
	sample_t Rg = (Rk / 4) * (0.5 + 1.2 * Rk) / (0.11 * Rd - Ra * (0.5 + 1.2 * Rk));
	
	sample_t Ta = Ra;
	sample_t Tp = 1 / (2.0 * Rg);
	sample_t Te = Tp + Tp * Rk;
	
	sample_t epsilon = 1 / Ta;
	sample_t shift = exp(-epsilon * (1 - Te));
	sample_t Delta = 1 - shift; //divide by this to scale RHS
	
	sample_t RHSIntegral = (1 / epsilon) * (shift - 1) + (1 - Te) * shift;
	RHSIntegral = RHSIntegral / Delta;
	
	sample_t totalLowerIntegral = - (Te-Tp) / 2.0 + RHSIntegral;
	sample_t totalUpperIntegral = -totalLowerIntegral;
	
	sample_t omega = M_PI / Tp;
	sample_t s = sin(omega * Te);
	// need E0*e^(alpha*Te)*s = -1 (to meet the return at -1)
	// and E0*e^(alpha*Tp/2) * Tp*2/pi = totalUpperIntegral
	//             (our approximation of the integral up to Tp)
	// writing x for e^alpha,
	// have E0*x^Te*s = -1 and E0 * x^(Tp/2) * Tp*2/pi = totalUpperIntegral
	// dividing the second by the first,
	// letting y = x^(Tp/2 - Te),
	// y * Tp*2 / (pi*s) = -totalUpperIntegral;
	sample_t y = -M_PI * s * totalUpperIntegral / (Tp * 2.0);
	sample_t z = log(y);
	sample_t alpha = z / (Tp / 2.0 - Te);
	sample_t E0 = -1.0 / (s * exp(alpha*Te));
	this->alpha = alpha;
	this->E0 = E0;
	this->epsilon = epsilon;
	this->shift = shift;
	this->Delta = Delta;
	this->Te = Te;
	this->omega = omega;
}

sample_t Glottis::getNoiseModulator()
{
	sample_t voiced = 0.1 + 0.2 * fmax(0.0, sin(M_PI * 2 * this->timeInWaveform / this->waveformLength));
	//return 0.3;
	return this->targetTenseness * this->intensity * voiced + (1 - this->targetTenseness * this->intensity) * 0.3;
}

void Glottis::setTargetFrequency(sample_t frequency)
{
	this->targetFrequency = frequency;
}

void Glottis::setTargetTenseness(sample_t tenseness)
{
	this->targetTenseness = tenseness;
}

void Glottis::finishBlock()
{
	sample_t vibrato = 0;
	vibrato += this->vibratoAmount * sin(2 * M_PI * this->totalTime * this->vibratoFrequency);
	vibrato += 0.02 * simplex1(this->totalTime * 4.07);
	vibrato += 0.04 * simplex1(this->totalTime * 2.15);
	if (this->autoWobble)
	{
		vibrato += 0.2 * simplex1(this->totalTime * 0.98);
		vibrato += 0.4 * simplex1(this->totalTime * 0.5);
	}
	if (this->targetFrequency > this->smoothFrequency)
		this->smoothFrequency = fmin(this->smoothFrequency * 1.1, this->targetFrequency);
	if (this->targetFrequency < this->smoothFrequency)
		this->smoothFrequency = fmax(this->smoothFrequency / 1.1, this->targetFrequency);
	this->oldFrequency = this->newFrequency;
	this->newFrequency = this->smoothFrequency * (1 + vibrato);
	this->oldTenseness = this->newTenseness;
	this->newTenseness = this->targetTenseness +
		0.1 * simplex1(this->totalTime * 0.46) + 0.05 * simplex1(this->totalTime * 0.36);
	if (!this->isTouched && alwaysVoice) this->newTenseness += (3-this->targetTenseness)*(1-this->intensity);
	
	if (this->isTouched || alwaysVoice) this->intensity += 0.13;
	else this->intensity -= 0.05;
	this->intensity = clamp(this->intensity, 0.0, 1.0);
}

sample_t Glottis::normalizedLFWaveform(sample_t t)
{
	sample_t output;
	if (t > this->Te) output = (-exp(-this->epsilon * (t - this->Te)) + this->shift) / this->Delta;
	else output = this->E0 * exp(this->alpha * t) * sin(this->omega * t);
	
	return output * this->intensity * this->loudness;
}

sample_t Glottis::runStep(sample_t lambda, sample_t noiseSource)
{
	
	sample_t timeStep = 1.0 / this->sampleRate;
	this->timeInWaveform += timeStep;
	this->totalTime += timeStep;
	if (this->timeInWaveform > this->waveformLength)
	{
		this->timeInWaveform -= this->waveformLength;
		this->setupWaveform(lambda);
	}
	sample_t out = this->normalizedLFWaveform(this->timeInWaveform / this->waveformLength);
	sample_t aspiration = this->intensity * (1 - sqrt(this->targetTenseness)) * this->getNoiseModulator() * noiseSource;
	aspiration *= 0.2 + 0.02 * simplex1(this->totalTime * 1.99);
	out += aspiration;
	return out;
}
