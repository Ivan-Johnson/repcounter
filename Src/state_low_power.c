#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>

#include "video.h"
#include "camera.h"
#include "ccamera.h"
#include "helper.h"
#include "state.h"

struct state runLowPower (char **err_msg, int *ret)
{
	int numPixels = ccameraGetNumPixels();
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
	float cThreshold = 0.1 * numPixels; // randomly chosen, but it works
	while (cActive < cThreshold) {
		ccameraGetFrames(data_new, data_old);
		unsigned long long tLast = getTimeInMs();

		float nActivePixels = 0;
		for (int iPixel = 0; iPixel<numPixels; iPixel++) {
			float activeness = fabs((float) data_new[iPixel] - (float) data_old[iPixel]);
			if (activeness > 40) {
				nActivePixels++;
			}
		}

#define CURWEIGHT 0.15
		cActive = CURWEIGHT*nActivePixels + (1-CURWEIGHT)*cActive;

		unsigned long long tNext = tLast + 1000/CAMERA_FPS;
		int tSleep = (long long) tNext - tLast;
		tSleep = tSleep > 0 ? tSleep : tSleep;
		usleep(tSleep * 1000);
	}

	printf("Activated after %f s\n", (getTimeInMs() - tStart)/1000.0);

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
