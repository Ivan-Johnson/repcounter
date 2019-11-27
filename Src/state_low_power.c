#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "video.h"
#include "ccamera.h"
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
	for (int frame = 0; frame < 25 * 10; frame++) {
		printf("PROCESSING FRAME %d\n", frame);
		uint16_t *data = ccameraGetNewFrame();
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

		usleep(1e6 / 25);

		free(data);
	}

DONE:
	videoStop();
	return stateNext;
}
