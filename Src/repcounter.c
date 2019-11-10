#include <assert.h>
#include <librealsense2/h/rs_frame.h>
#include <librealsense2/h/rs_option.h>
#include <librealsense2/h/rs_pipeline.h>
#include <librealsense2/rs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rs-depth.h"

void print_error(rs2_error* e)
{
	printf("rs_error was raised when calling %s(%s):\n", rs2_get_failed_function(e), rs2_get_failed_args(e));
	printf("    %s\n", rs2_get_error_message(e));
}

// Get the first connected device
// The returned object should be released with rs2_delete_device(...)
bool getFirstDevice(rs2_context **ctx, rs2_device **dev)
{
	bool ret = false;
	rs2_error* e = NULL;

	(*ctx) = rs2_create_context(RS2_API_VERSION, &e);
	if (e) {
		(*ctx) = NULL;
		goto FAIL;
	}

	rs2_device_list* device_list = rs2_query_devices(*ctx, &e);
	if (e) {
		goto FAIL;
	}

	int dev_count = rs2_get_device_count(device_list, &e);
	if (e) {
		goto FAIL;
	}
	if (dev_count == 0) {
		puts("There are no attached devices");
		goto FAIL;
	}

	(*dev) = rs2_create_device(device_list, 0, &e);
	if (e) {
		(*dev) = NULL;
		goto FAIL;
	}

	rs2_delete_device_list(device_list);
	return true;
FAIL:
	if (e) {
		print_error(e);
	}
	if (device_list) {
		rs2_delete_device_list(device_list);
	}
	if (ctx) {
		rs2_delete_context(*ctx);
	}
	return ret;
}

struct args {
	bool write;
	char *file;
};

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
	int ret = EXIT_FAILURE;
	bool success;
	struct args args;
	rs2_device *dev = NULL;
	rs2_context *ctx = NULL;
	rs2_error* e = 0;


	success = parseArgs(argc, argv, &args);
	if (!success) {
		goto FAIL;
	}

	success = getFirstDevice(&ctx, &dev);
	if (!success) {
		goto FAIL;
	}

	const char *name = rs2_get_device_info(dev, RS2_CAMERA_INFO_NAME, &e);
	if (e) {
		goto FAIL;
	}
	printf("Using device \"%s\"\n", name);

	printStream(ctx, dev); // no exit?
	ret = EXIT_SUCCESS;

FAIL:
	if (e) {
		print_error(e);
	}
	if (dev) {
		rs2_delete_device(dev);
	}
	if (ctx) {
		rs2_delete_context(ctx);
	}
	return ret;
}
