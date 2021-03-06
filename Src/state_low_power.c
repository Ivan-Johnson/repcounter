#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "video.h"
#include "camera.h"
#include "ccamera.h"
#include "helper.h"
#include "state.h"

struct state runLowPower (void *args, char **err_msg, int *ret)
{
	(void) args;
	(void) err_msg;
	(void) ret;

	size_t numPixels = ccameraGetNumPixels();
	struct state stateNext = STATE_STARTING;
	uint16_t *data_new = NULL;
	uint16_t *data_old = NULL;
	uint16_t *data_scratch = NULL;
	data_new = malloc(ccameraGetFrameSize());
	data_old = malloc(ccameraGetFrameSize());
	data_scratch = malloc(ccameraGetFrameSize());
	assert(data_new && data_old && data_scratch);

	unsigned long long tStart = getTimeInMs();

	float cActive = 0;
	float cThreshold = 0.1f * (float) numPixels; // randomly chosen, but it works
	while (cActive < cThreshold) {
		ccameraGetFrames(data_new, data_old);
		unsigned long long tLast = getTimeInMs();

		float nActivePixels = 0;
		for (size_t iPixel = 0; iPixel<numPixels; iPixel++) {
			float activeness = fabsf((float) data_new[iPixel] - (float) data_old[iPixel]);
			if (activeness > 40) {
				nActivePixels++;
			}
		}

#define CURWEIGHT 0.15f
		cActive = CURWEIGHT*nActivePixels + (1-CURWEIGHT)*cActive;

		assert(tLast < LLONG_MAX);
		unsigned long long tNext = tLast + 1000/CAMERA_FPS;
		assert(tNext < LLONG_MAX);
		long long tSleep = (long long) tNext - (long long) tLast;
		tSleep = tSleep > 0 ? tSleep : 0;
		assert(tSleep < UINT_MAX / 1000);
		usleep((unsigned int) tSleep * 1000);
	}

	unsigned long long delta = getTimeInMs() - tStart;
	printf("Activated after %f s\n", (double) delta / 1000.0);

	if (data_new) {
		free(data_new);
	}
	if (data_scratch) {
		free(data_scratch);
	}
	if (data_old) {
		free(data_old);
	}

	return stateNext;
}
