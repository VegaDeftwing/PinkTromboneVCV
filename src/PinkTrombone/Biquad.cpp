//
//  Biquad.cpp
//  PinkTrombone - All
//
//  Created by Samuel Tarakajian on 8/30/19.
//

#include "Biquad.hpp"
#include <math.h>
#include "../plugin.hpp"

Biquad::Biquad(sample_t sampleRate) : frequency(200),
									  q(0.5),
									  gain(1.0),
									  xm1(0.0),
									  xm2(0.0),
									  ym1(0.0),
									  ym2(0.0)
{
	this->sampleRate = sampleRate;
	this->twopiOverSampleRate = M_PI * 2.0 / this->sampleRate;
	this->updateCoefficients();
}

void Biquad::updateCoefficients()
{
	sample_t omega = this->frequency * this->twopiOverSampleRate;
	sample_t sn = rack::simd::sin(omega);
	sample_t cs = rack::simd::cos(omega);
	sample_t one_over_Q = 1. / this->q;
	sample_t alpha = sn * 0.5 * one_over_Q;

	// Bandpass only, for now
	sample_t b0 = 1. / (1. + alpha);
	this->a0 = alpha * b0;
	this->a1 = 0.;
	this->a2 = -alpha * b0;
	this->b1 = -2. * cs * b0;
	this->b2 = (1. - alpha) * b0;
}

void Biquad::setFrequency(sample_t f)
{
	if (f == this->frequency) return;
	this->frequency = f;
	this->updateCoefficients();
}

void Biquad::setGain(sample_t g)
{
	if (g == this->gain) return;
	this->gain = g;
	this->updateCoefficients();
}

void Biquad::setQ(sample_t q)
{
	if (q == this->q) return;
	this->q = q;
	this->updateCoefficients();
}

sample_t Biquad::runStep(sample_t xn)
{
    if (frequency <= 0.f) { reset(); return 0.f; }
	sample_t yn = (this->a0 * xn) + (this->a1 * this->xm1) + (this->a2 * this->xm2) - (this->b1 * this->ym1) - (this->b2 * this->ym2);
	this->ym2 = this->ym1;
	this->ym1 = yn;
	this->xm2 = this->xm1;
	this->xm1 = xn;
	return yn;
}

void Biquad::setSampleRate(sample_t rate) {
    sampleRate = rate;
    twopiOverSampleRate = M_PI * 2.0 / sampleRate;
    updateCoefficients();
}
void Biquad::reset() { xm1 = xm2 = ym1 = ym2 = 0.f; }
