#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "camera.h"
#include "ccamera.h"
#include "helper.h"
#include "state.h"
#include "state_counting.h"
#include "video.h"


// this whole chunk of variables belongs to `mutFrames`

// `frames` stores the frames that are actually being processed. High indicies
// are older. Low indicies are the most recent data.
static uint16_t **frames;
static const unsigned int cFrames = CAMERA_FPS * 5; // # of frames in `frames`.
static pthread_mutex_t mutFrames = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
// how far away from the average must you go before it counts as a up or a down.
static const double minDeviation = 8; // todo: unduplicate with state_counting#findNext#thresh
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

// number of ms that the user has to be idle for before termination
static const unsigned long long msIdle = 30*1000;


static void* readMain(void *none)
{
	(void) none;

	while (!done) {
		usleep(1000000 / CAMERA_FPS);

		// todo: instead of this convoluted mess of having a second done
		// check, simply aquire the lock before doing the `done` check
		// (and release it during sleeps)
		assert(!pthread_mutex_lock(&mutFNew));
		if (done) {
			goto CONTINUE;
		}

		assert(cFNew < cFNewMax);
		ccameraGetFrame(fNew[cFNew]);
		cFNew++;

	CONTINUE:
		assert(!pthread_mutex_unlock(&mutFNew));
	}

	return NULL;
}

static void* moveMain(void* none)
{
	(void) none;

	// todo: malloc/free fNewScratch here, not globally
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
		assert(!pthread_mutex_lock(&mutFrames));
		assert(!pthread_mutex_lock(&mutFNew));
		if (done) {
			goto CONTINUE;
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

	CONTINUE:
		assert(!pthread_mutex_unlock(&mutFNew));
		assert(!pthread_mutex_unlock(&mutFrames));
	}

	return NULL;
}

static void initialize()
{
	int fail;

	frames = malloc(sizeof(*frames) * cFrames);
	assert(frames);
	for (size_t i = 0; i < cFrames; i++) {
		frames[i] = malloc(ccameraGetFrameSize());
		assert(frames[i]);
		ccameraGetFrame(frames[i]);
		usleep(1000000 / CAMERA_FPS);
	}

	fNewScratch = malloc(sizeof(*fNew) * cFNewMax);
	assert(fNewScratch);
	fNew = malloc(sizeof(*fNew) * cFNewMax);
	assert(fNew);
	for (size_t i = 0; i < cFNewMax; i++) {
		fNew[i] = malloc(ccameraGetFrameSize());
		assert(fNew[i]);
	}
	cFNew = 0;

	done = false;

	fail = pthread_create(&thdRead, NULL, &readMain, NULL);
	assert(!fail);

	fail = pthread_create(&thdMove, NULL, &moveMain, NULL);
	assert(!fail);

}

static void destroy()
{
	// setting of `done` is now done in startingMain
	//done = true;

	struct timespec stop = {time(NULL) + 2, 0}; // todo: tighten this bound
	assert(!pthread_timedjoin_np(thdRead, NULL, &stop));
	assert(!pthread_timedjoin_np(thdMove, NULL, &stop));

	for (size_t i = 0; i < cFrames; i++) {
		if (frames[i] != NULL) {
			free(frames[i]);
		}
	}
	free(frames);

	for (size_t i = 0; i < cFNewMax; i++) {
		if (fNew[i] != NULL) {
			free(fNew[i]);
		}
	}
	free(fNew);

	free(fNewScratch);
}

static struct state startingMain()
{
	bool success = false;
	bool failure = false;

	double *dScratch = malloc(sizeof(double) * cFrames);
	assert(dScratch);

	unsigned long long tStart = getTimeInMs();

	assert(!pthread_mutex_lock(&mutFrames));
	while (!success && !failure) {
		// for each frame, compute the difference between its average
		// and the average of averages
		ccameraComputeFrameAverages(frames, cFrames, dScratch);
		double tmpTotal = 0;
		for (unsigned int ii = 0; ii < cFrames; ii++) {
			tmpTotal += dScratch[ii];
			printf("%f, ", dScratch[ii]);
		}
		printf("\n");
		double average = tmpTotal / cFrames;
		printf("%f, %f\n", average, minDeviation);

		for (unsigned int ii = 0; ii < cFrames; ii++) {
			dScratch[ii] -= average;
		}

		// Find the first frame that differs from the average by at
		// least minDeviation
		unsigned int ii = 0;
		while (ii < cFrames && fabs(dScratch[ii]) < minDeviation) {
			ii++;
		}
		if (ii == cFrames) {
			goto SLEEP;
		}
		tStart = getTimeInMs();

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
			success = true;
		}

	SLEEP:
		if (!success && getTimeInMs() - tStart > msIdle) {
			// `!success` check is implied by time check?
			failure = true;
		}

		// Wait for a new batch of frames
		//
		// todo: instead of a redundant check, why don't we just put
		// this at the top of the loop?
		if (!success && !failure) {
			uint16_t *tmp = frames[0];
			while(frames[0] == tmp) {
				assert(!pthread_mutex_unlock(&mutFrames));
				usleep(US_DELAY_MOVE / 10);
				assert(!pthread_mutex_lock(&mutFrames));
			}
		}
	}

	// request stop early to give other threads as much time as possible to
	// notice
	done = true;

	free(dScratch);

	struct state next;
	if (success) {
		assert(!pthread_mutex_lock(&mutFNew));

		struct argsCounting *args = malloc(sizeof(struct argsCounting));
		args->cFrames = cFrames + cFNew;
		args->frames = malloc(sizeof(*args->frames) * args->cFrames);

		unsigned int iBase = 0;
		for (unsigned int ii = 0; ii < cFrames; ii++) {
			args->frames[iBase + ii] = frames[ii];
			frames[ii] = NULL;
		}
		iBase += cFrames;
		for (unsigned int ii = 0; ii < cFNew; ii++) {
			args->frames[iBase + ii] = fNew[ii];
			fNew[ii] = NULL;
		}
		cFNew = 0;


		next = STATE_COUNTING;
		next.args = args;
		next.shouldFreeArgs = true;

		assert(!pthread_mutex_unlock(&mutFNew));
	} else {
		assert(failure);
		next = STATE_LOW_POWER;
	}

	assert(!pthread_mutex_unlock(&mutFrames));

	return next;
}

struct state runStarting(void *args, char **err_msg, int *retStatus)
{
	(void) args;
	(void) err_msg;
	(void) retStatus;

	initialize();

	struct state retState = startingMain();

	destroy();

	return retState;
}
