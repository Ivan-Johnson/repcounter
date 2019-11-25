#include <stdint.h>
#include <stdio.h>

#include "video.h"
#include "camera.h"
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
	for (int frame = 0; frame < 25 * 5; frame++) {
		printf("PROCESSING FRAME %d\n", frame);
		uint16_t *data = cameraGetFrame();
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
			goto DONE;
		}

		free(data);
	}

DONE:
	videoStop();
	return stateNext;
}
