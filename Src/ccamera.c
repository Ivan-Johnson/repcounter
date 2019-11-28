#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "camera.h"
#include "ccamera.h"

static unsigned int sample_size;
static unsigned int sample_delta;
static unsigned int cFrames;
static uint16_t *frameNew, *frameOld;

// a buffer for intermediate calculations of size sufficient to store an entire
// frame.
static uint16_t *scratch;

// the cFrames most recent frames from the camera. Large indicies are newer.
static uint16_t **frames;

static pthread_mutex_t mutex = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
static pthread_t background;
static bool stopRequested;

uint16_t ccameraGetPixelFromFrame(uint16_t *frame, size_t x, size_t y)
{
	return frame[y*ccameraGetFrameWidth() + x];
}

static void *backgroundMain(void *);

int ccameraInit(struct args args)
{
	int fail;

	fail = cameraInit(args);
	if (fail) {
		return fail;
	}

	fail = pthread_mutex_lock(&mutex);
	assert(!fail);

	sample_size = args.ccamera_sample_size;
	sample_delta = args.ccamera_sample_delta;
	stopRequested = false;

	cFrames = sample_size + sample_delta;

	frames = malloc(sizeof(*frames) * cFrames);
	assert(frames);

	for (unsigned int i = 0; i < cFrames; i++) {
		frames[i] = cameraGetFrame();
	}

	frameNew = malloc(sizeof(*frameNew) * ccameraGetNumPixels());
	frameOld = malloc(sizeof(*frameOld) * ccameraGetNumPixels());
	scratch = malloc(sizeof(*frameOld) * ccameraGetNumPixels());
	assert(frameNew && frameOld && scratch);

	fail = pthread_create(&background, NULL, &backgroundMain, NULL);
	assert(!fail);

	pthread_mutex_unlock(&mutex);
	assert(!fail);

	return 0;
}

int ccameraDestroy()
{
	// the mutex does not need to be locked for this function because:
	// A: we don't need the mutex to wait for other threads to exit
	// B: locking may prevent other threads from exiting
	// C: after other threads have exited, there's no point in locking

	stopRequested = true;
	struct timespec stop = {time(NULL) + 2, 0};
	int fail = pthread_timedjoin_np(background, NULL, &stop);
	assert(!fail);

	assert(!pthread_mutex_destroy(&mutex));

	for (unsigned int i = 0; i < cFrames; i++) {
		free(frames[i]);
	}
	free(frameNew);
	free(frameOld);
	free(scratch);

	assert(!cameraDestroy());

	return 0;
}

static void pixelSort(uint16_t *buf, size_t cPix)
{
	// if this fails, the function needs to be optimized for large inputs
	assert(cPix < 100);

	// bubble sort, because:
	//
	// A: it's trivial to implement, with a low risk of bugs
	//
	// B: for small inputs, it's faster than recursive algorithms.
	// Especially ones that rely on callbacks, like qsort.
	bool isSorted = false;
	while(!isSorted) {
		isSorted = true;
		for (size_t iPix = 1; iPix < cPix; iPix++) {
			if (buf[iPix-1] < buf[iPix]) {
				isSorted = false;
				uint16_t tmp = buf[iPix-1];
				buf[iPix-1] = buf[iPix];
				buf[iPix] = tmp;
			}
		}
	}
}

// After calling this function, frameOut[i] is the median of frames[j][i] for j
// in [iStart, iStart+sample_size)
//
// scratch is a temporary storage buffer of sufficient size to store at least sample_size+iStart pixels.
static void computeMedian(uint16_t *frameOut, unsigned int iStart, uint16_t *scratch)
{
	unsigned int iEnd    = iStart + sample_size;
	unsigned int iMedian = iStart + sample_size / 2;
	int cPixels = ccameraGetNumPixels();

	for (int iPixel = 0; iPixel < cPixels; iPixel++) {
		for (unsigned int iFrame = iStart; iFrame < iEnd; iFrame++) {
			scratch[iFrame] = frames[iFrame][iPixel];
		}

		pixelSort(scratch, sample_size);
		frameOut[iPixel] = scratch[iMedian];
	}
}

void *backgroundMain(void *foo)
{
	pthread_mutex_lock(&mutex);
	while(!stopRequested) {
		pthread_mutex_unlock(&mutex);
		usleep(100000);
		pthread_mutex_lock(&mutex);

		// update frames
		free(frames[0]);
		for (unsigned int iFrame = 1; iFrame < cFrames; iFrame++) {
			frames[iFrame-1] = frames[iFrame];
		}
		frames[cFrames-1] = cameraGetFrame();

		// update frame{Old,New}
		computeMedian(frameOld, 0, scratch);
		computeMedian(frameNew, sample_delta, scratch);
	}
	pthread_mutex_unlock(&mutex);
	return NULL;
}


int ccameraGetFrameWidth()
{
	int width = cameraGetFrameWidth();
	return width;
}

int ccameraGetFrameHeight()
{
	int height = cameraGetFrameHeight();
	return height;
}

int ccameraGetNumPixels()
{
	int count = ccameraGetFrameWidth() * ccameraGetFrameHeight();
	return count;
}

size_t ccameraGetFrameSize()
{
	size_t size = sizeof(**frames) * ccameraGetNumPixels();
	return size;
}

static uint16_t* duplicateFrame(uint16_t *frame)
{
	uint16_t *ret = malloc(ccameraGetFrameSize());
	if (!ret) {
		return NULL;
	}

	memcpy(ret, frame, ccameraGetFrameSize());

	return ret;
}

uint16_t* ccameraGetNewFrame()
{
	assert(!pthread_mutex_lock(&mutex));

	uint16_t *ret = duplicateFrame(frameNew);

	assert(!pthread_mutex_unlock(&mutex));

	return ret;
}

uint16_t* ccameraGetOldFrame()
{
	assert(!pthread_mutex_lock(&mutex));

	uint16_t *ret = duplicateFrame(frameOld);

	assert(!pthread_mutex_unlock(&mutex));

	return ret;
}
