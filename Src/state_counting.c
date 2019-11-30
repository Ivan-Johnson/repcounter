#include <assert.h>
#include <pthread.h>
#include <stdio.h>
//#include <string.h>
#include <unistd.h>
//#include <math.h>

#include "camera.h"
#include "ccamera.h"
#include "state.h"
#include "state_counting.h"
#include "video.h"

// Stores the new frames that have not yet been considered. It is initially
// empty, but a separate thread adds new frames to the end of it as they become
// available.
static uint16_t **buf;
// how many frames are in fNew right now
static unsigned int cBuf = 0;
// max size of buf
static const unsigned int cBufMax = CAMERA_FPS * 5;
// mutex for all buf related variables
static pthread_mutex_t mutBuf = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;

static pthread_t thdRead; // reads individual frames into `fNew`

static volatile bool done;

static void* readMain(void *none)
{
	while (!done) {
		usleep(1000000 / CAMERA_FPS);

		pthread_mutex_lock(&mutBuf);

		assert(cBuf < cBufMax);
		ccameraGetFrame(buf[cBuf]);
		cBuf++;

		pthread_mutex_unlock(&mutBuf);
	}

	return NULL;
}

static void initialize()
{
	int fail;

	buf = malloc(sizeof(*buf) * cBufMax);
	assert(buf);
	for (unsigned int ii = 0; ii < cBufMax; ii++) {
		buf[ii] = malloc(ccameraGetFrameSize());
		assert(buf[ii]);
	}

	fail = pthread_create(&thdRead, NULL, &readMain, NULL);
	assert(!fail);
}

static void destroy()
{
	assert(!done);
	done = true;

	struct timespec stop = {time(NULL) + 2, 0}; // TODO: tighten this bound
	assert(!pthread_timedjoin_np(thdRead, NULL, &stop));

	assert(!pthread_mutex_destroy(&mutBuf));

	free(buf);
}

static void destroyArgs(struct argsCounting *args)
{
	for (unsigned int ii = 0; ii < args->cFrames; ii++) {
		free(args->frames[ii]);
	}
	free(args->frames);
	free(args->frameAverages);
}

struct state runCounting(void *a, char **err_msg, int *ret)
{
	struct argsCounting *args = a;
	initialize();

	assert(!videoStart("/tmp/counting"));
	for (unsigned int ii = 0; ii < args->cFrames; ii++) {
		assert(!videoEncodeFrame(args->frames[ii]));
	}

	assert(!videoEncodeColor(0));
	assert(!videoEncodeColor(0));
	assert(!videoEncodeColor(0));
	assert(!videoEncodeColor(0));
	assert(!videoEncodeColor(0));

	unsigned int iF = 0;
	while (iF < CAMERA_FPS * 5) {
		pthread_mutex_lock(&mutBuf);

		unsigned int iTmp = 0;
		while (iTmp < cBuf) {
			assert(!videoEncodeFrame(buf[iTmp]));
			iTmp++;
		}
		iF += cBuf;
		cBuf = 0;
		pthread_mutex_unlock(&mutBuf);

		usleep(100000); // 100ms
	}

	assert(!videoStop());


	destroy();
	destroyArgs(args);
	return STATE_EXIT;
}
