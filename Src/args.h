#ifndef ARGS_H
#define ARGS_H

#include <stdbool.h>

struct args {
	bool write;
	char *file;

	unsigned int ccamera_sample_size;
	unsigned int ccamera_sample_delta;
};

#endif
