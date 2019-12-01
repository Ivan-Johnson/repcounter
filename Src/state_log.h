#ifndef STATE_LOG_H
#define STATE_LOG_H

#include <stdint.h>

// state defines runLog for us
#include "state.h"

struct argsLog {
	unsigned int cRep;

	// The time that the reps stopped at, measured as the number of seconds
	// since the epoch
	unsigned long long sStop;
};

#endif
