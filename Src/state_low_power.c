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
	bool b = videoStart("/tmp/test_lp");
	struct state stateNext = STATE_EXIT;
	uint16_t *frame = NULL;
	if (!b) {
		*err_msg = "Failed to initialize video";
		*ret = 1;
		stateNext = STATE_ERROR;
		goto DONE;
	}
	frame = malloc(ccameraGetFrameSize());
	assert(frame);

	for (int nFrame = 0; nFrame < 25 * 30; nFrame++) {
		printf("PROCESSING FRAME %d\n", nFrame);
		ccameraGetFrame(frame);
		unsigned long long tLast = getTimeInMs();

		b = videoEncodeFrame(frame);
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
	if (frame) {
		free(frame);
	}

	videoStop();
	return stateNext;
}
