#include <stdio.h>
#include <string.h>

#include "args.h"
#include "state.h"
#include "ccamera.h"

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

	out->ccamera_sample_size = 5;
	out->ccamera_sample_delta = 2;

	return true;
FAIL:
	puts("USAGE:");
	printf("A: %s --write /file/\n", argv[0]);
	printf("B: %s --read /file/\n", argv[0]);
	return false;
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
	fail = ccameraInit(args);
	if (fail) {
		puts("CCAMERA INIT FAILED");
		return EXIT_FAILURE;
	}

	int ret = stateRun();

	fail = ccameraDestroy();
	if (fail) {
		puts("CCAMERA DESTROY FAILED");
		return EXIT_FAILURE;
	}

	return ret;
}
