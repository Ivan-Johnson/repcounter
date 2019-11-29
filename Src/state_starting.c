#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include "camera.h"
#include "ccamera.h"
#include "state.h"
#include "video.h"


// this whole chunk of variables belongs to `mutFrames`

// `frames` stores the frames that are actually being processed. High indicies
// are older. Low indicies are the most recent data.
static uint16_t **frames;
static const unsigned int cFrames = CAMERA_FPS * 5; // # of frames in `frames`.
static pthread_mutex_t mutFrames = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
static uint16_t **framesScratch;
static const unsigned int cFramesScratch = 1;
// how far away from the average must you go before it counts as a up or a down.
static const double minDeviation = 8;
// how many reps have to be detected to be considered activated
static const unsigned int repThreshold = 2;

#define US_DELAY_MOVE 1000000

// Stores the new frames that have not yet been considered. It is initially
// empty, but a separate thread adds new frames to it as they become
// available. Approximately once ever `US_DELAY_MOVE` uSeconds a different
// thread flushes them to `frames`.
static uint16_t **fNew;
// how many frames are in fNew right now
static unsigned int cFNew = 0;
static const unsigned int cFNewMax = CAMERA_FPS * 2;
// a single scratch buffer (of size `cFNewMax`) for storing pointers to
// frames. i.e. it is not an array of many scratch frames. May only be used by
// whoever has the `mutFNew` mutex locked.
static uint16_t **fNewScratch;
static pthread_mutex_t mutFNew = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;



static pthread_t thdRead; // reads individual frames into `fNew`
static pthread_t thdMove; // Moves data from `fNew` to `frames`

static volatile bool done;

static void* readMain(void *none)
{
	while (!done) {
		usleep(1000000 / CAMERA_FPS);

		pthread_mutex_lock(&mutFNew);

		assert(cFNew < cFNewMax);
		ccameraGetFrame(fNew[cFNew]);
		cFNew++;

		pthread_mutex_unlock(&mutFNew);
	}

	return NULL;
}

static void* moveMain(void* none)
{
	while (!done) {
		usleep(US_DELAY_MOVE);

		// we're deliberately locking mutFrame before mutFNew because of
		// the combined reasons:
		//
		// A: mutFNew is much more sensitive to being locked for
		//    extended periods of time than mutFrame is
		//
		// B: locking mutFrame might take a while
		//
		// Also note that we can't delay the locking of mutFNew because
		// we're using cFNew.
		pthread_mutex_lock(&mutFrames);
		pthread_mutex_lock(&mutFNew);
		if (done) {
			pthread_mutex_unlock(&mutFrames);
			pthread_mutex_unlock(&mutFNew);
			break;
		}

		// "delete" the `cFNew` oldest frames from `frames`
		for (unsigned int ii = 0; ii < cFNew; ii++) {
			fNewScratch[ii] = frames[ii];
		}

		// shift frames to account for the "deletetion" of `cFNew` frames
		for (unsigned int iWrite = 0; iWrite+cFNew < cFrames; iWrite++) {
			frames[iWrite] = frames[iWrite+cFNew];
		}
		// frames[cFrames - cFNew] is the first frame of garbage

		// copy `cFNew` frames from `fNew` to `frames`
		// `offset` is the first free spot in `frames`
		unsigned int offset = cFrames - cFNew;
		for (unsigned int iRead = 0; iRead < cFNew; iRead++) {
			// append a valid frame pointer to `frames`
			frames[offset + iRead] = fNewScratch[iRead];

			// copy the data
			memcpy(frames[offset + iRead], fNew[iRead], ccameraGetFrameSize());
		}

		// clear the contents of fNew
		cFNew = 0;

		pthread_mutex_unlock(&mutFNew);
		pthread_mutex_unlock(&mutFrames);
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
		usleep(1000000 / CAMERA_FPS);
	}

	framesScratch = malloc(sizeof(*frames) * cFramesScratch);
	for (int ii = 0; ii < cFramesScratch; ii++) {
		framesScratch[ii] = malloc(ccameraGetFrameSize());
		assert(framesScratch[ii]);
	}

	fNewScratch = malloc(sizeof(*fNew) * cFNewMax);
	assert(fNewScratch);
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
	// setting of `done` is now done in startingMain
	//done = true;

	struct timespec stop = {time(NULL) + 2, 0}; // TODO: tighten this bound
	assert(!pthread_timedjoin_np(thdRead, NULL, &stop));
	assert(!pthread_timedjoin_np(thdMove, NULL, &stop));

	assert(!pthread_mutex_destroy(&mutFNew));
	assert(!pthread_mutex_destroy(&mutFrames));

	for (int ii = 0; ii < cFramesScratch; ii++) {
		free(framesScratch[ii]);
	}
	free(framesScratch);

	for (int i = 0; i < cFrames; i++) {
		free(frames[i]);
	}
	free(frames);

	for (int i = 0; i < cFNewMax; i++) {
		free(fNew[i]);
	}
	free(fNew);

	free(fNewScratch);
}

static struct state startingMain()
{
	bool breakLoop = false;

	double *dScratch = malloc(sizeof(double) * cFrames);
	assert(dScratch);

	pthread_mutex_lock(&mutFrames);
	while (!breakLoop) {
		// for each frame, compute the difference between its average
		// and the average of averages
		ccameraComputeFrameAverages(frames, cFrames, dScratch);
		double tmpTotal = 0;
		for (unsigned int ii = 0; ii < cFrames; ii++) {
			tmpTotal += dScratch[ii];
		}
		double average = tmpTotal / cFrames;

		for (unsigned int ii = 0; ii < cFrames; ii++) {
			dScratch[ii] -= average;
		}

		// Find the first frame that differs from the average by at
		// least minDeviation
		unsigned int ii = 0;
		while (fabs(dScratch[ii]) < minDeviation && ii < cFrames) {
			ii++;
		}
		if (ii == cFrames) {
			continue;
		}

		// TODO: this is a terrible system. It doesn't account for the
		// target taking up different fractions of the FOV. Normal
		// pushups when the camera is on my bedroom shelf are typically
		// 20 to 30.

		// Within this time sequence, find out how many times we went
		// from being more than minDeviation above to being more than
		// minDeviation below.
		bool far = dScratch[ii] > 0;
		unsigned int flipCount = 0;
		while (ii < cFrames) {
			if ((far && dScratch[ii] < -minDeviation) ||
				(!far && dScratch[ii] > minDeviation)) {
				flipCount++;
				far = !far;
			}
			ii++;
		}

		if (flipCount / 2 > repThreshold) {
			breakLoop = true;
		} else if (false) {
			// TODO: if more than n seconds from start of `starting` and we
			// haven't detected harmonic motion yet, return to low power
			// state.
		}

		// Wait for a new batch of frames
		if (!breakLoop) {
			uint16_t *tmp = frames[0];
			while(frames[0] == tmp) {
				pthread_mutex_unlock(&mutFrames);
				usleep(US_DELAY_MOVE / 10);
				pthread_mutex_lock(&mutFrames);
			}
		}
	}

	// TODO: compute bounding box

	free(dScratch);

	done = true;
	pthread_mutex_unlock(&mutFrames);

	// TODO return correct state, with correct args

	return STATE_EXIT;
}

struct state runStarting(char **err_msg, int *retStatus)
{
	initialize();

	struct state retState = startingMain();

	destroy();

	return retState;
}
