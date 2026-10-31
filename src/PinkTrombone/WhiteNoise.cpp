//
//  WhiteNoise.cpp
//  PinkTrombone - All
//
//  Created by Samuel Tarakajian on 8/30/19.
//

#include "WhiteNoise.hpp"
#include <stdlib.h>
#include <math.h>


WhiteNoise::WhiteNoise(long sampleLength, uint32_t seed) {
	this->index = 0;
	this->size = sampleLength > 0 ? sampleLength : 1;
	this->buffer = (sample_t *) malloc(this->size * sizeof(sample_t));
	
	for (long i = 0; i < this->size; i++) {
		seed = seed ? seed : 0x6d2b79f5u;
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        this->buffer[i] = (seed >> 8) * (2.f / 16777216.f) - 1.f;
	}
}

WhiteNoise::~WhiteNoise() {
	free(this->buffer);
	this->buffer = nullptr;
}

sample_t WhiteNoise::runStep() {
	sample_t ov = this->buffer[this->index];
	this->index = this->index + 1;
	if (this->index >= this->size) this->index = 0;
	return ov;
}
