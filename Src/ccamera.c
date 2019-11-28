#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "camera.h"
#include "ccamera.h"
#include "helper.h"

static unsigned int sample_size;
static unsigned int sample_delta;
static unsigned int cFrames;

// a buffer for intermediate calculations of size sufficient to store an entire
// frame.
static uint16_t *scratch;

// the cFrames most recent frames from the camera. Large indicies are newer.
static uint16_t **frames;


static uint16_t *frameNew, *frameOld;

// mutRecent is a mutex specifically for accessing frameNew and frameOld
static pthread_mutex_t mutRecent = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;

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

	sample_size = args.ccamera_sample_size;
	sample_delta = args.ccamera_sample_delta;
	stopRequested = false;

	cFrames = sample_size + sample_delta;

	frames = malloc(sizeof(*frames) * cFrames);
	assert(frames);

	for (unsigned int i = 0; i < cFrames; i++) {
		frames[i] = malloc(ccameraGetFrameSize());
		assert(frames[i]);
		assert(!cameraGetFrame(frames[i]));
	}

	frameNew = malloc(sizeof(*frameNew) * ccameraGetNumPixels());
	frameOld = malloc(sizeof(*frameOld) * ccameraGetNumPixels());
	scratch = malloc(sizeof(*frameOld) * ccameraGetNumPixels());
	assert(frameNew && frameOld && scratch);

	fail = pthread_create(&background, NULL, &backgroundMain, NULL);
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

	assert(!pthread_mutex_destroy(&mutRecent));

	for (unsigned int i = 0; i < cFrames; i++) {
		free(frames[i]);
	}
	free(frames);
	free(frameNew);
	free(frameOld);
	free(scratch);

	assert(!cameraDestroy());

	return 0;
}

static void pixelSort(uint16_t *buf, size_t cPix)
{
	// if this fails, the function needs to be optimized for large inputs
	assert(cPix < 30);

	//  insertion sort
	for(size_t ii = 1; ii < cPix; ii++) {
		// buf is sorted for indicies strictly less than ii

		uint16_t tmp = buf[ii];

		// shift right-most elements right by one until we've found the
		// spot for `tmp`
		size_t ij = ii - 1;
		while(ij >= 0 && tmp < buf[ij]) {
			buf[ij + 1] = buf[ij];
			ij--;
		}

		buf[ij + 1] = tmp;
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
	unsigned long long before = 0;
	unsigned long long after = 0;
	unsigned int dropped = 0;
	while(!stopRequested) {
		// rotate `frames` array
		uint16_t *tmp = frames[0];
		for (unsigned int iFrame = 1; iFrame < cFrames; iFrame++) {
			frames[iFrame-1] = frames[iFrame];
		}
		frames[cFrames-1] = tmp;

		// get new frame
		before = getTimeInMs();
		// this runs ~instantaneously unless it has to wait for a new frame
		assert(!cameraGetFrame(frames[cFrames-1]));
		after = getTimeInMs();

		dropped -= 5;
		dropped = dropped > 0 ? dropped : 0;
		if (after - before <= 1) {
			// we didn't have to wait for the next frame.
			// (we should have to wait 10+ ms)
			dropped += 10;
			assert(dropped < 50); // fail if we've been dropping a lot of frames
		}

		// update frame{Old,New}
		pthread_mutex_lock(&mutRecent);
		computeMedian(frameOld, 0, scratch);
		computeMedian(frameNew, sample_delta, scratch);
		pthread_mutex_unlock(&mutRecent);

	}

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

void ccameraGetFrame(uint16_t* frameOut)
{
	assert(!pthread_mutex_lock(&mutRecent));

	memcpy(frameOut, frameNew, ccameraGetFrameSize());

	assert(!pthread_mutex_unlock(&mutRecent));
}

void ccameraGetFrames(uint16_t* outNew, uint16_t* outOld)
{
	assert(!pthread_mutex_lock(&mutRecent));

	memcpy(outNew, frameNew, ccameraGetFrameSize());
	memcpy(outOld, frameOld, ccameraGetFrameSize());

	assert(!pthread_mutex_unlock(&mutRecent));
}
