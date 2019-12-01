#include <stdio.h>

#include "state.h"
#include "state_log.h"

#define LOG_FNAME "/home/i/Code/RepCounter/Data/reps.txt"

struct state runLog(void *a, char **err, int *ret)
{
	struct argsLog *args = a;
	(void) err;
	(void) ret;
	(void) args;

	FILE *fOut = fopen(LOG_FNAME, "a");
	fprintf(fOut, "%llu\t%u\n", args->sStop, args->cRep);

	fclose(fOut);
	return STATE_STARTING;
}
