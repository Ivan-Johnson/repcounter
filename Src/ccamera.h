#ifndef CCAMERA_H
#define CCAMERA_H

// This `Clean Camera` module is a wrapper around `camera` that denoises data,
// and adds a couple of other helpful features.

#include <stdint.h>

#include "args.h"

int ccameraInit(struct args args);
int ccameraDestroy();

int ccameraGetFrameWidth();
int ccameraGetFrameHeight();
int ccameraGetNumPixels();
size_t ccameraGetFrameSize();

uint16_t ccameraGetPixelFromFrame(uint16_t *frame, size_t x, size_t y);

// TODO/OPTIMIZE: instead of returning `malloc`ed memory, take an input
// argument. Duh.
//
// returns the latest frame of clean data. Must be `free`ed
uint16_t* ccameraGetNewFrame();

// Same as ccameraGetNewFrame, except instead of the latest frame, return the
// almost-latest, as specified by the initialization args.
uint16_t* ccameraGetOldFrame();

#endif
