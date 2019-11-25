#ifndef CAMERA_H
#define CAMERA_H

#include <stdbool.h>
#include <stdint.h>

#include "args.h"

#define CAMERA_FPS 30

int cameraInit(struct args args);
int cameraDestroy();

int cameraGetFrameWidth();
int cameraGetFrameHeight();

//TODO: test this function
// Returns NULL on failure, otherwise waits until the next frame of data is
// available and returns it.
uint16_t* cameraGetFrame();

#endif
