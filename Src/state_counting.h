#ifndef STATE_COUNTING_H
#define STATE_COUNTING_H

#include <stdint.h>

// state defines runCounting for us
#include "state.h"

struct argsCounting {
	// Frames to process before getting new ones from ccamera. Each frame in
	// `frames` as well as `frames` itself is assumed to be `malloc`ed and
	// will be `free`ed by state_counting.
	uint16_t **frames;
	unsigned int cFrames;
};

#endif
