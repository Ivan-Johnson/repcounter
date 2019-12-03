#ifndef CCAMERA_H
#define CCAMERA_H

// This `Clean Camera` module is a wrapper around `camera` that denoises data,
// and adds a couple of other helpful features.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "args.h"

int ccameraInit(struct args args);
int ccameraDestroy();

size_t ccameraGetFrameWidth();
size_t ccameraGetFrameHeight();
size_t ccameraGetNumPixels();
size_t ccameraGetFrameSize();

static uint16_t ccameraGetPixelFromFrame(uint16_t *frame, size_t x, size_t y) __attribute__((unused));
static uint16_t ccameraGetPixelFromFrame(uint16_t *frame, size_t x, size_t y)
{
	return frame[y*ccameraGetFrameWidth() + x];
}

static void ccameraSetPixelFromFrame(uint16_t *frame, size_t x, size_t y, uint16_t val) __attribute__((unused));
static void ccameraSetPixelFromFrame(uint16_t *frame, size_t x, size_t y, uint16_t val)
{
	frame[y*ccameraGetFrameWidth() + x] = val;
}

static void ccameraCopyFrame(uint16_t* fIn, uint16_t* fOut) __attribute__((unused));
static void ccameraCopyFrame(uint16_t* fIn, uint16_t* fOut)
{
	memcpy(fOut, fIn, ccameraGetFrameSize());
}

void ccameraGetFrame(uint16_t* frameOut);
void ccameraGetFrames(uint16_t* frameNew, uint16_t* frameOld);

// todo: figure out how to declare frame data as const
void ccameraComputeFrameAverages(uint16_t** frames, unsigned int cFrames, double *averages);

#endif
