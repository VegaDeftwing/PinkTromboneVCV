//
//  WhiteNoise.cpp
//  PinkTrombone - All
//
//  Created by Samuel Tarakajian on 8/30/19.
//

#include "WhiteNoise.hpp"
#include <stdlib.h>
#include <math.h>


WhiteNoise::WhiteNoise(long sampleLength) {
	this->index = 0;
	this->size = sampleLength;
	this->buffer = (sample_t *) malloc(sampleLength * sizeof(sample_t));
	
	for (long i = 0; i < this->size; i++) {
		this->buffer[i] = ((sample_t) rand() / (sample_t) RAND_MAX) * 2.0 - 1.0;
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
