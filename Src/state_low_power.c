#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "video.h"
#include "ccamera.h"
#include "helper.h"
#include "state.h"

struct state runLowPower (char **err_msg, int *ret)
{
	int numPixels = ccameraGetNumPixels();
	struct state stateNext = STATE_EXIT;
	uint16_t *data_new = NULL;
	uint16_t *data_old = NULL;
	uint16_t *data_scratch = NULL;
	bool b = videoStart("/tmp/test_lp");
	if (!b) {
		*err_msg = "Failed to initialize video";
		*ret = 1;
		stateNext = STATE_ERROR;
		goto DONE;
	}
	data_new = malloc(ccameraGetFrameSize());
	data_old = malloc(ccameraGetFrameSize());
	data_scratch = malloc(ccameraGetFrameSize());
	assert(data_new && data_old && data_scratch);


	for (int frame = 0; frame < 25 * 10; frame++) {
		printf("PROCESSING FRAME %d\n", frame);

		ccameraGetFrames(data_new, data_old);
		unsigned long long tLast = getTimeInMs();

		for (int iPixel = 0; iPixel<numPixels; iPixel++) {
			data_scratch[iPixel] = abs((int) data_new[iPixel] - (int) data_old[iPixel]);
			data_scratch[iPixel] = data_scratch[iPixel] > 40 ? 4000 : 0;
		}

		b = videoEncodeFrame(data_scratch);
		//b = videoEncodeFrame(data_new);
		if (!b) {
			*err_msg = "Failed to encode frame";
			*ret = 1;
			stateNext = STATE_ERROR;
			goto DONE;
		}

		unsigned long long tNext = tLast + 1000/25;
		int tSleep = (long long) tNext - tLast;
		tSleep = tSleep > 0 ? tSleep : tSleep;
		usleep(tSleep * 1000);
	}

DONE:
	if (data_new) {
		free(data_new);
	}
	if (data_scratch) {
		free(data_scratch);
	}
	if (data_old) {
		free(data_old);
	}

	videoStop();
	return stateNext;
}
