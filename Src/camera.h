#ifndef CAMERA_H
#define CAMERA_H

#include <stdbool.h>

int cameraInit(struct args args);
int cameraDestroy();

bool cameraHasNewFrame();
int cameraGetFrame();

#endif
