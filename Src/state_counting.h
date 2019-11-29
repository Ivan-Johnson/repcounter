#ifndef STATE_COUNTING_H
#define STATE_COUNTING_H

#include <stdint.h>

// state defines runCounting for us
#include "state.h"

struct argsCounting {
	// Frames to process before getting new ones from ccamera, as well as
	// the average values of those frames. Each frame in `frames`, `frames`
	// itself, and frameAverages are assumed to be `malloc`ed and will be
	// `free`ed by state_counting.
	uint16_t **frames;
	double *frameAverages;
	unsigned int cFrames;
};

#endif
