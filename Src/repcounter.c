#include <stdio.h>
#include <string.h>

#include "args.h"
#include "state.h"
#include "camera.h"

bool parseArgs(int argc, char **argv, struct args *out)
{
	if (argc != 3) {
		goto FAIL;
	}

	if (!strcmp("--read", argv[1])) {
		out->write=false;
	} else if (!strcmp("--write", argv[1])) {
		out->write=true;
	} else {
		goto FAIL;
	}

	out->file = argv[2];

	return true;
FAIL:
	puts("USAGE:");
	printf("A: %s --write /file/\n", argv[0]);
	printf("B: %s --read /file/\n", argv[0]);
	return false;

	//TODO: https://github.com/IntelRealSense/librealsense/blob/master/examples/record-playback/rs-record-playback.cpp
}

int main(int argc, char **argv)
{
	bool success;
	struct args args;
	success = parseArgs(argc, argv, &args);
	if (!success) {
		return EXIT_FAILURE;
	}


	int fail;
	fail = cameraInit(args);
	if (fail) {
		puts("CAMERA INIT FAILED");
		return EXIT_FAILURE;
	}

	int ret = stateRun();

	fail = cameraDestroy();
	if (fail) {
		puts("CAMERA DESTROY FAILED");
		return EXIT_FAILURE;
	}

	return ret;
}
