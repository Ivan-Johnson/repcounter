#ifndef VIDEO_H
#define VIDEO_H

#include <stdint.h>
#include <stdbool.h>

bool videoEncodeFrame(uint16_t *data);

bool videoStart(char *file);
bool videoStop();

#endif
