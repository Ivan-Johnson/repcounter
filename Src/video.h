#ifndef VIDEO_H
#define VIDEO_H

#include <stdint.h>
#include <stdbool.h>

// TODO last: parameterize this, specifing what values are white/black, and how many copies to encode
int videoEncodeFrame(uint16_t *data);

// encode a new frame filled with a single color; any float between zero and one
// (inclusive)
int videoEncodeColor(float tmp);

// TODO: parameterize, FPS
int videoStart(char *file);
int videoStop();

#endif
