#include <assert.h>
#include <pthread.h>
#include <unistd.h>

#include "camera.h"
#include "ccamera.h"
#include "state.h"
#include "video.h"


// `frames` stores the frames that are actually being processed. High indicies
// are older. Low indicies are the most recent data.
static uint16_t **frames;
static const unsigned int cFrames = CAMERA_FPS * 5; // # of frames in `frames`.
static pthread_mutex_t mutFrames = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;


#define US_DELAY (1000000 / CAMERA_FPS)

// Stores the new frames that have not yet been considered. It is initially
// empty, but a separate thread adds new frames to it as they become
// available. Approximately once ever `US_DELAY` uSeconds a different thread
// flushes them to `frames`.
static uint16_t **fNew;
static unsigned int cFNew;
static const unsigned int cFNewMax = CAMERA_FPS * 2;
static pthread_mutex_t mutFNew = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;

static pthread_t thdRead; // reads individual frames into `fNew`
static pthread_t thdMove; // Moves data from `fNew` to `frames`

static bool done;

static void* readMain(void *none)
{
	while (!done) {
		usleep(1000000 / CAMERA_FPS);

		// TODO: read
	}

	return NULL;
}

static void* moveMain(void* none)
{
	while (!done) {
		usleep(US_DELAY);

		//TODO: move

		// (Remember to lock `mutFrames` /before/ locking `mutFNew`. If
		// you use the reverse order, you'll likely end up blocking
		// `thdRead` for far too long)
	}

	return NULL;
}

static void initialize()
{
	int fail;

	frames = malloc(sizeof(*frames) * cFrames);
	assert(frames);
	for (int i = 0; i < cFrames; i++) {
		frames[i] = malloc(ccameraGetFrameSize());
		assert(frames[i]);
		ccameraGetFrame(frames[i]);
	}

	fNew = malloc(sizeof(*fNew) * cFNewMax);
	assert(fNew);
	for (int i = 0; i < cFNewMax; i++) {
		fNew[i] = malloc(ccameraGetFrameSize());
		assert(fNew[i]);
	}
	cFNew = 0;

	fail = pthread_create(&thdRead, NULL, &readMain, NULL);
	assert(!fail);

	fail = pthread_create(&thdMove, NULL, &moveMain, NULL);
	assert(!fail);

	done = false;
}

static void destroy()
{
	done = true;

	struct timespec stop = {time(NULL) + 2, 0}; // TODO: tighten this bound
	assert(!pthread_timedjoin_np(thdRead, NULL, &stop));
	assert(!pthread_timedjoin_np(thdMove, NULL, &stop));

	assert(!pthread_mutex_destroy(&mutFNew));
	assert(!pthread_mutex_destroy(&mutFrames));

	for (int i = 0; i < cFrames; i++) {
		free(frames[i]);
	}
	free(frames);

	for (int i = 0; i < cFNewMax; i++) {
		free(fNew[i]);
	}
	free(fNew);
}

static struct state startingMain()
{
	videoStart("/tmp/5s");

	pthread_mutex_lock(&mutFrames);
	for (int i = 0; i < cFrames; i++) {
		assert(videoEncodeFrame(frames[i]));
	}
	pthread_mutex_unlock(&mutFrames);

	videoStop();

	return STATE_EXIT;
}

struct state runStarting(char **err_msg, int *retStatus)
{
	initialize();

	struct state retState = startingMain();

	destroy();

	return retState;
}
