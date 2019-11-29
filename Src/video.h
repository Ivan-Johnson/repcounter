#ifndef VIDEO_H
#define VIDEO_H

#include <stdint.h>
#include <stdbool.h>

bool videoEncodeFrame(uint16_t *data);

// encode a new frame filled with a single color; any float between zero and one
// (inclusive)
int videoEncodeColor(float tmp);

bool videoStart(char *file);
bool videoStop();

#endif
