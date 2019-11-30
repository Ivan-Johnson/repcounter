#include <assert.h>
#include <pthread.h>
#include <stdio.h>
//#include <string.h>
#include <unistd.h>
#include <math.h>

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
struct box {
	unsigned int xMin, xMax, yMin, yMax;
};
static struct box box;

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

static unsigned int findNext(double *avgs, unsigned int cFrames, unsigned int start, bool max)
{
	static const double thresh = 8; // TODO: unduplicate with state_starting#minDeviation
	double sum = 0;
	for (unsigned int ii = 0; ii < cFrames; ii++) {
		sum += avgs[ii];
	}
	double avg = sum / cFrames;

	for (unsigned int ii = start; ii < cFrames; ii++) {
		double value = avgs[ii];
		if ((max  && value >= avg + thresh) ||
		    (!max && value <= avg - thresh)) {
			return ii;
		}
	}

	// should never happen because of the conditions of state_starting
	assert(false);
}

static unsigned int findNextHigh(double *avgs, unsigned int cFrames, unsigned int start)
{
	return findNext(avgs, cFrames, start, true);
}

static unsigned int findNextLow(double *avgs, unsigned int cFrames, unsigned int start)
{
	return findNext(avgs, cFrames, start, false);
}

static unsigned int findExtreme(double *avgs, unsigned int cFrames, unsigned int start, unsigned int end, bool max)
{
	unsigned int iExtreme = 0;
	double vExtreme = avgs[iExtreme];

	for (unsigned int ii = iExtreme+1; ii < cFrames; ii++) {
		double value = avgs[ii];
		if ((max  && value > vExtreme) ||
		    (!max && value < vExtreme)) {
			iExtreme = ii;
			vExtreme = value;
		}
	}

	return iExtreme;
}

static unsigned int findMax(double *avgs, unsigned int cFrames, unsigned int start, unsigned int end)
{
	return findExtreme(avgs, cFrames, start, end, true);
}

static unsigned int findMin(double *avgs, unsigned int cFrames, unsigned int start, unsigned int end)
{
	return findExtreme(avgs, cFrames, start, end, false);
}

static double computeUtil(int *frame, struct box box)
{
	unsigned int width = ccameraGetFrameWidth();
	unsigned int tmp = 0; // TODO: delete after passing test
	double total = 0;

	for (unsigned int iY = box.yMin; iY < box.yMax; iY++) {
		for (unsigned int iX = box.xMin; iX < box.xMax; iX++) {
			total += frame[iY*width + iX];
			tmp++;
		}
	}

	unsigned int numPixels = (box.yMax - box.yMin) * (box.xMax - box.xMin);
	if (numPixels != tmp) {
		printf("{%d, %d}, {%d, %d}, %d, %d\n", box.xMin, box.yMin, box.xMax, box.yMax, numPixels, tmp);
		assert(false);
	}
	return total / numPixels;
}

// Return a box whose contained pixels are a strict subset of the given box's
// pixels.
//
// Each subsequent call shrinks the box in a different direction, and possibly
// by a different amount. This isn't thread safe or anything though, so be
// careful if you rely on that behavior.
static struct box nextShrink(struct box box, bool reset)
{
	static unsigned int dir = 0;
	static double maxAmount = 0.3;
	static double amount;

	if (reset) {
		dir = 0;
		amount = maxAmount;
		return box;
	}

	bool vertical = dir % 2;
	bool increaseMin = (dir / 2) % 2;

	unsigned int range = vertical ? box.yMax - box.yMin : box.xMax - box.xMin;
	unsigned int delta = ceil(range*amount);
	if (!increaseMin) {
		delta *= -1;
	}

	unsigned int *value = vertical ?
		(increaseMin ? &box.yMin : &box.yMax) :
		(increaseMin ? &box.xMin : &box.xMax);
	*value += delta;

	dir++;
	if (dir % 4 != dir) {
		dir = dir % 4;
		amount *= 0.7;
	}

	return box;
}

static void drawBox(uint16_t *frame, uint16_t color, struct box box)
{
	unsigned int width = ccameraGetFrameWidth();

	// draw horizontal lines
	for (unsigned int iX = box.xMin; iX < box.xMax; iX++) {
		frame[width*box.yMin + iX] = color;
		frame[width*box.yMax + iX] = color;
	}

	// draw vertical lines
	for (unsigned int iY = box.yMin; iY < box.yMax; iY++) {
		frame[width*iY + box.xMin] = color;
		frame[width*iY + box.xMax] = color;
	}
}

// find the first local min & local max of avgs
static void findFirstExtremePair(double *avgs, unsigned int cFrames, unsigned int *iMin, unsigned int *iMax)
{
	unsigned int tmpLow  = findNextLow (avgs, cFrames, 0);
	unsigned int tmpHigh = findNextHigh(avgs, cFrames, 0);

	if (tmpLow < tmpHigh) { // todo: cleanup. If I used findExtreme instead, these two would probably be ~duplicate code.
		*iMin   = findMin(avgs, cFrames, tmpLow, tmpHigh);
		tmpLow  = findNextLow (avgs, cFrames, tmpHigh);
		*iMax   = findMax(avgs, cFrames, tmpHigh, tmpLow);
	} else {
		*iMax   = findMax(avgs, cFrames, tmpHigh, tmpLow);
		tmpHigh = findNextHigh(avgs, cFrames, tmpLow);
		*iMin   = findMin(avgs, cFrames, tmpLow, tmpHigh);
	}
}

static void initializeBox(struct argsCounting *args, uint16_t *fMin, uint16_t *fMax)
{
	assert(!videoStart("/tmp/box"));
	uint16_t *fScratch = malloc(ccameraGetFrameSize());
	assert(fScratch);
	for (unsigned int ii = 0; ii < 50; ii++) {
		assert(!videoEncodeFrame(fMax));
	}

	int numPixels = ccameraGetNumPixels();
	int *delta = malloc(sizeof(int*) * numPixels);
	for (unsigned int ii = 0; ii < numPixels; ii++) {
		delta[ii] = fMax[ii] - fMin[ii];
	}

	struct box boxBest;
	boxBest.xMax = ccameraGetFrameWidth() - 1;
	boxBest.xMin = 0;
	boxBest.yMax = ccameraGetFrameHeight() - 1;
	boxBest.yMin = 0;

	double utilBest = computeUtil(delta, boxBest);

	nextShrink(boxBest, true);
	unsigned int lastShrink = 0;
	while(lastShrink < 10) { // arbitrary
		lastShrink++;
		// todo: use a slightly less greedy algorithm. Examine all four
		// shrink directions, and choose the one that gets the best result.

		// todo: instead of using static variables & special reset
		// parameter, just pass `lastShrink`. %4 to get direction, /4 to
		// get magnitude.
		struct box boxNew = nextShrink(boxBest, false);
		double utilNew = computeUtil(delta, boxNew);


		// TODO: print delta, not a normal frame.
		ccameraCopyFrame(fMax, fScratch);
		drawBox(fScratch, UINT16_MAX, boxBest);
		drawBox(fScratch, 0, boxNew);
		assert(!videoEncodeFrame(fScratch));

		double fracChange = utilNew / utilBest;
		if (fracChange >= 1.20) {
			nextShrink(boxBest, true);
			boxBest = boxNew;
			utilBest = utilNew;
			lastShrink = 0;
		}
	}

	ccameraCopyFrame(fMax, fScratch);
	drawBox(fScratch, UINT16_MAX, boxBest);
	for (unsigned int ii = 0; ii < 50; ii++) {
		assert(!videoEncodeFrame(fScratch));
	}

	free(delta);
	free(fScratch);

	assert(!videoStop());

	box = boxBest;
}

static void initialize(struct argsCounting *args)
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

	// It's important that these are done AFTER starting `thdRead`,
	// otherwise we'll miss frames
	unsigned int iMin, iMax;
	findFirstExtremePair(args->frameAverages, args->cFrames, &iMin, &iMax);
	initializeBox(args, args->frames[iMin], args->frames[iMax]);
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
	initialize(args);

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
