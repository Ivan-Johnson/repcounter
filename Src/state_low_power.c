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
	if (!b) {
		*err_msg = "Failed to initialize video";
		*ret = 1;
		stateNext = STATE_ERROR;
		goto DONE;
	}
	for (int frame = 0; frame < 25 * 240; frame++) {
		printf("PROCESSING FRAME %d\n", frame);
		uint16_t *data = ccameraGetNewFrame();
		unsigned long long tLast = getTimeInMs();

		if (data == NULL) {
			*err_msg = "Failed to get frame";
			*ret = 1;
			stateNext = STATE_ERROR;
			goto DONE;
		}

		b = videoEncodeFrame(data);
		if (!b) {
			*err_msg = "Failed to encode frame";
			*ret = 1;
			stateNext = STATE_ERROR;
			free(data); //TODO: is there a more elegant way of doing this, without duplicate code?
			goto DONE;
		}

		free(data);

		unsigned long long tNext = tLast + 1000/25;
		int tSleep = (long long) tNext - tLast;
		tSleep = tSleep > 0 ? tSleep : tSleep;
		usleep(tSleep * 1000);

	}

DONE:
	videoStop();
	return stateNext;
}
