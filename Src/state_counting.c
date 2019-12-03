#include <assert.h>
#include <pthread.h>
#include <stdio.h>
//#include <string.h>
#include <unistd.h>
#include <math.h>

#include "camera.h"
#include "ccamera.h"
#include "helper.h"
#include "state.h"
#include "state_counting.h"
#include "state_log.h"
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



static bool growingDistant;

// This value is obtained by doing the computation:
//   take the last two extreme frames (either peak&valley of a rep, or the valley&peak).
//   subtract one from the other
//   take the average value of the pixels in box
//   THEN take the absolute value of that value

// todo: update this value as new data comes in. Actually not, because it seems
// likely that reps gradually get shallower as cReps increases and we don't want
// to cound the bad ones at the end.
static double range;
static double lastExtreme;

static volatile bool done;

// A region of a frame. `Min`s are included, `Max`s are excluded.
struct box {
	size_t xMin, xMax, yMin, yMax;
};
static struct box box;

// number of ms that the user has to be idle for before termination
static const unsigned long long msIdle = 10*1000;

static void* readMain(void *none)
{
	(void) none;

	while (!done) {
		usleep(1000000 / CAMERA_FPS);

		assert(!pthread_mutex_lock(&mutBuf));

		assert(cBuf < cBufMax);
		ccameraGetFrame(buf[cBuf]);
		cBuf++;

		assert(!pthread_mutex_unlock(&mutBuf));
	}

	return NULL;
}

static unsigned int findNext(double *avgs, unsigned int cFrames, unsigned int start, bool max)
{
	static const double thresh = 8; // todo: unduplicate with state_starting#minDeviation
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

static unsigned int findExtreme(double *avgs, unsigned int start, unsigned int end, bool max)
{
	unsigned int iExtreme = start;
	double vExtreme = avgs[iExtreme];

	for (unsigned int ii = iExtreme+1; ii < end; ii++) {
		double value = avgs[ii];
		if ((max  && value > vExtreme) ||
		    (!max && value < vExtreme)) {
			iExtreme = ii;
			vExtreme = value;
		}
	}

	return iExtreme;
}

static unsigned int findMax(double *avgs, unsigned int start, unsigned int end)
{
	return findExtreme(avgs, start, end, true);
}

static unsigned int findMin(double *avgs, unsigned int start, unsigned int end)
{
	return findExtreme(avgs, start, end, false);
}

// todo: duplicate code; avgInBoxInt/avgInBox
static double avgInBoxInt(int *frame, struct box box)
{
	size_t width = ccameraGetFrameWidth();
	double total = 0;

	for (size_t iY = box.yMin; iY < box.yMax; iY++) {
		for (size_t iX = box.xMin; iX < box.xMax; iX++) {
			total += frame[iY*width + iX];
		}
	}

	size_t numPixels = (box.yMax - box.yMin) * (box.xMax - box.xMin);
	return total / (double) numPixels;
}

// todo: duplicate code; avgInBoxInt/avgInBox
static double avgInBox(uint16_t *frame, struct box box)
{
	size_t width = ccameraGetFrameWidth();
	double total = 0;

	for (size_t iY = box.yMin; iY < box.yMax; iY++) {
		for (size_t iX = box.xMin; iX < box.xMax; iX++) {
			total += frame[iY*width + iX];
		}
	}

	size_t numPixels = (box.yMax - box.yMin) * (box.xMax - box.xMin);
	return total / (double) numPixels;
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

	size_t range = vertical ? box.yMax - box.yMin : box.xMax - box.xMin;
	double ddelta = ceil((double)range * amount);
	assert(ddelta < SIZE_MAX);
	size_t delta = (size_t) ddelta;

	size_t *value = vertical ?
		(increaseMin ? &box.yMin : &box.yMax) :
		(increaseMin ? &box.xMin : &box.xMax);
	if (!increaseMin) {
		assert(*value >= delta);
		*value -= delta;
	} else {
		assert(*value <= SIZE_MAX - delta);
		*value += delta;
	}


	dir++;
	if (dir % 4 != dir) {
		dir = dir % 4;
		amount *= 0.7;
	}

	return box;
}

static __attribute__((unused)) void drawBox(uint16_t *frame, uint16_t color, struct box box)
{
	size_t width = ccameraGetFrameWidth();

	// draw horizontal lines
	for (size_t iX = box.xMin; iX < box.xMax; iX++) {
		frame[width*box.yMin + iX] = color;
		frame[width*(box.yMax-1) + iX] = color;
	}

	// draw vertical lines
	for (size_t iY = box.yMin; iY < box.yMax; iY++) {
		frame[width*iY + box.xMin] = color;
		frame[width*iY + (box.xMax-1)] = color;
	}
}

// find a local min & local max of avgs
static void findExtremePair(double *avgs, unsigned int cFrames, unsigned int *iMin, unsigned int *iMax)
{
	unsigned int tmpLow  = findNextLow (avgs, cFrames, 0);
	unsigned int tmpHigh = findNextHigh(avgs, cFrames, 0);

	if (tmpLow < tmpHigh) { // todo: cleanup. If I used findExtreme instead, these two would probably be ~duplicate code.
		// skip the first rep; it's likely to be bad data
		tmpLow  = findNextLow (avgs, cFrames, tmpHigh);
		tmpHigh = findNextHigh(avgs, cFrames, tmpLow);

		*iMin   = findMin(avgs, tmpLow, tmpHigh);
		tmpLow  = findNextLow (avgs, cFrames, tmpHigh);
		*iMax   = findMax(avgs, tmpHigh, tmpLow);
	} else {
		// skip the first rep; it's likely to be bad data
		tmpHigh = findNextHigh(avgs, cFrames, tmpLow);
		tmpLow  = findNextLow (avgs, cFrames, tmpHigh);

		*iMax   = findMax(avgs, tmpHigh, tmpLow);
		tmpHigh = findNextHigh(avgs, cFrames, tmpLow);
		*iMin   = findMin(avgs, tmpLow, tmpHigh);
	}
}

// f1 - f2 -> fOut, but only for the pixels in box
static void boxSubtraction(uint16_t *f1, uint16_t *f2, int *fOut, struct box box)
{
	size_t fWidth = ccameraGetFrameWidth(); // width of the FRAME, not box.
	for (size_t iY = box.yMin; iY < box.yMax; iY++) {
		for (size_t iX = box.xMin; iX < box.xMax; iX++) {
			size_t ii = iY*fWidth + iX;
			fOut[ii] = f1[ii] - f2[ii];
		}
	}
}

static void initializeBox(uint16_t *fMin, uint16_t *fMax)
{
	struct box boxBest;
	boxBest.xMax = ccameraGetFrameWidth();
	boxBest.xMin = 0;
	boxBest.yMax = ccameraGetFrameHeight();
	boxBest.yMin = 0;

	size_t numPixels = ccameraGetNumPixels();
	int *delta = malloc(sizeof(int*) * numPixels);
	assert(delta);
	boxSubtraction(fMax, fMin, delta, boxBest);

	double utilBest = avgInBoxInt(delta, boxBest);

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
		double utilNew = avgInBoxInt(delta, boxNew);


		double fracChange = utilNew / utilBest;
		if (fracChange >= 1.20) {
			nextShrink(boxBest, true);
			boxBest = boxNew;
			utilBest = utilNew;
			lastShrink = 0;
		}
	}

	free(delta);

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

	// It's important that all this junk is done AFTER starting `thdRead`,
	// otherwise we'll miss frames

	unsigned int iMin, iMax;
	double *avgs = malloc(sizeof(*avgs) * args->cFrames);
	assert(avgs);
	ccameraComputeFrameAverages(args->frames, args->cFrames, avgs);
	findExtremePair(avgs, args->cFrames, &iMin, &iMax);
	free(avgs);

	initializeBox(args->frames[iMin], args->frames[iMax]);

	growingDistant = iMin < iMax;

	double tmpMin = avgInBox(args->frames[iMin], box);
	double tmpMax = avgInBox(args->frames[iMax], box);
	range = tmpMax - tmpMin;
	printf("%f, %f, %f\n", tmpMin, tmpMax, range);

	assert(!videoStart("/tmp/countingTmp"));

	drawBox(args->frames[iMin], 10000, box);
	drawBox(args->frames[iMax], 10000, box);

	for (int i = 0; i < 60; i++) {
		assert(!videoEncodeFrame(args->frames[iMin]));
	}
	for (int i = 0; i < 60; i++) {
		assert(!videoEncodeFrame(args->frames[iMax]));
	}
	assert(!videoStop());

}

static void destroy()
{
	assert(done);

	struct timespec stop = {time(NULL) + 2, 0}; // todo: tighten this bound
	assert(!pthread_timedjoin_np(thdRead, NULL, &stop));

	free(buf);
}

static void destroyArgs(struct argsCounting *args)
{
	for (unsigned int ii = 0; ii < args->cFrames; ii++) {
		free(args->frames[ii]);
	}
	free(args->frames);
}

static bool isRepFromFrame(uint16_t *frame)
{
	// Each pushup must have a range of at least thresh*range
	static const double thresh = 0.6;
	double avg = avgInBox(frame, box);

	if ((growingDistant && avg < lastExtreme) ||
		(!growingDistant && avg > lastExtreme)) {
		// a previous frame was extreme enough to flip growingDistant,
		// but the rep hasn't actually changed directions yet.
		lastExtreme = avg;
		return false;
	}

	double delta = lastExtreme - avg;

	bool goodMagnitude = fabs(delta) > range*thresh;
	bool goodSign = (growingDistant && avg > lastExtreme) ||
		(!growingDistant && avg < lastExtreme);

	if (!goodMagnitude || !goodSign) {
		// nothing interesting is happening
		return false;
	}

	// goodMagnitude && goodSign, therefore we've completed a half-rep and need to flipGrowingDistant
	lastExtreme = avg;
	growingDistant = !growingDistant;

	// only count every other half rep
	return growingDistant;
}

struct state runCounting(void *a, char **err_msg, int *ret)
{
	(void) err_msg;
	(void) ret;
	struct argsCounting *args = a;
	initialize(args);

	unsigned int cRep = 0;

	for (unsigned int ii = 0; ii < args->cFrames; ii++) {
		uint16_t *frame = args->frames[ii];
		if (isRepFromFrame(frame)) {
			cRep++;
			printf("Backlog rep: %d\n", cRep);
		}
	}

	unsigned long long tPrior = getTimeInMs();

	while (!done) {
		assert(!pthread_mutex_lock(&mutBuf));

		unsigned int iF = 0;
		while (iF < cBuf) {
			uint16_t *frame = buf[iF];
			if (isRepFromFrame(frame)) {
				cRep++;
				printf("New rep: %d\n", cRep);
				tPrior = getTimeInMs();
			}

			iF++;
		}
		cBuf = 0;

		if (getTimeInMs() - tPrior > msIdle) {
			done = true;
		}

		assert(!pthread_mutex_unlock(&mutBuf));

		usleep(100000); // 100ms
	}

	destroy();
	destroyArgs(args);

	if (!cRep) {
		return STATE_STARTING;
	}

	struct argsLog *logArgs = malloc(sizeof(struct argsLog));
	assert(logArgs);
	logArgs->cRep = cRep;
	logArgs->sStop = getTimeInMs() / 1000;

	struct state next = STATE_LOG;
	next.args = logArgs;
	next.shouldFreeArgs = true;
	return next;
}
