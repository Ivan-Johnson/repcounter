#include <assert.h>
#include <unistd.h>

#include "ccamera.h"
#include "helper.h"
#include "state.h"
#include "video.h"

// how long to record for in ms
#define DURATION 90000

// file to record video of frames to.
// NULL to not record
#define FVIDEO NULL

struct state runRecording(char **err_msg, int *retStatus)
{
	unsigned long long tStart = getTimeInMs();
	uint16_t *frame = NULL;
	if (FVIDEO) {
		assert(videoStart(FVIDEO));
		frame = malloc(ccameraGetFrameSize());
		assert(frame);
	}

	while (getTimeInMs() - tStart < DURATION) {
		if (FVIDEO) {
			ccameraGetFrame(frame);
			assert(videoEncodeFrame(frame));
		}

		usleep(100000); // 100ms
	}

	if (FVIDEO) {
		assert(videoStop());
	}

	return STATE_EXIT;
}
