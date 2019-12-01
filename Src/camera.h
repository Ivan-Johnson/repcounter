#ifndef CAMERA_H
#define CAMERA_H

#include <stdbool.h>
#include <stdint.h>

#include "args.h"

#define CAMERA_FPS 30

int cameraInit(struct args args);
int cameraDestroy();

size_t cameraGetFrameWidth();
size_t cameraGetFrameHeight();

// Waits for the next frame and writes it to frameOut
int cameraGetFrame(uint16_t *frameOut);

#endif
