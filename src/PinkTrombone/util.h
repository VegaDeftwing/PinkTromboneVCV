//
//  util.h
//  PinkTrombone - All
//
//  Created by Samuel Tarakajian on 8/28/19.
//

#ifndef util_h
#define util_h

#include <random>
#include "config.h"

static sample_t maxf(sample_t a, sample_t b) {
	if (a > b) return a;
	return b;
}

static sample_t minf(sample_t a, sample_t b) {
	if (a < b) return a;
	return b;
}

static inline sample_t clamp(sample_t number, sample_t min, sample_t max) {
	if (number < min) return min;
	else if (number > max) return max;
	else return number;
}

static inline sample_t moveTowards(sample_t current, sample_t target, sample_t amount)
{
	if (current < target) return minf(current + amount, target);
	else return maxf(current - amount, target);
}

static inline sample_t moveTowards(sample_t current, sample_t target, sample_t amountUp, sample_t amountDown)
{
	if (current < target) return minf(current + amountUp, target);
	else return maxf(current - amountDown, target);
}

static inline sample_t gaussian()
{
	sample_t s = 0;
	for (int c = 0; c < 16; c++) s += (sample_t) rand() / (sample_t) RAND_MAX;
	return (s - 8.0) / 4.0;
}

#endif /* util_h */
