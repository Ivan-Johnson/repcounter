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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                     These parameters are reconfigurable                                        //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define STREAM          RS2_STREAM_DEPTH  // rs2_stream is a types of data provided by RealSense device           //
#define FORMAT          RS2_FORMAT_Z16    // rs2_format is identifies how binary data is encoded within a frame   //
#define WIDTH           640               // Defines the number of columns for each frame or zero for auto resolve//
#define HEIGHT          0                 // Defines the number of lines for each frame or zero for auto resolve  //
#define FPS             30                // Defines the rate of frames per second                                //
#define STREAM_INDEX    0                 // Defines the stream index, used for multiple streams of the same type //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void print_error(rs2_error* e)
{
	printf("rs_error was raised when calling %s(%s):\n", rs2_get_failed_function(e), rs2_get_failed_args(e));
	printf("    %s\n", rs2_get_error_message(e));
}

struct args {
	bool write;
	char *file;
};

// Get the first connected device
// The returned object should be released with rs2_delete_device(...)
bool getFirstDevice(struct args args, rs2_pipeline **pipeline, const rs2_stream_profile** stream_profile, rs2_device **dev)
{
	bool ret = false;
	rs2_error* e = NULL;

	rs2_device_list* device_list = NULL;
	rs2_context* ctx = NULL;
	rs2_config* config = NULL;

	ctx = rs2_create_context(RS2_API_VERSION, &e);
	if (e) {
		ctx = NULL;
		goto FAIL;
	}

	device_list = rs2_query_devices(ctx, &e);
	if (e) {
		device_list = NULL;
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

	(*pipeline) = rs2_create_pipeline(ctx, &e);
	if (e) {
		(*pipeline) = NULL;
		goto FAIL;
	}

	config = rs2_create_config(&e);
	if (e) {
		config = NULL;
		goto FAIL;
	}

	//TODO: handle all these errors
	rs2_pipeline_profile* pipeline_profile;
	if(args.write) {
		rs2_config_enable_record_to_file(config, args.file, &e);
		if (e) {
			goto FAIL;
		}
		pipeline_profile = rs2_pipeline_start_with_config(*pipeline, config, &e);
		if (e) {
			printf("The connected device doesn't support depth streaming!\n");
			goto FAIL;
		}
	} else {
		rs2_config_enable_device_from_file(config, args.file, &e);
		if (e) {
			goto FAIL;
		}

		rs2_delete_device(*dev);

		pipeline_profile = rs2_pipeline_start_with_config(*pipeline, config, &e);
		if (e) {
			goto FAIL;
		}

		(*dev) = rs2_pipeline_profile_get_device(pipeline_profile, &e);
		if (e) {
			goto FAIL;
		}
	}
	if (e) {
		goto FAIL;
	}

	rs2_stream_profile_list* stream_profile_list = rs2_pipeline_profile_get_streams(pipeline_profile, &e);
	if (e) {
		printf("Failed to create stream profile list!\n");
		exit(EXIT_FAILURE);
	}

	*stream_profile = rs2_get_stream_profile(stream_profile_list, 0, &e);
	if (e) {
		printf("Failed to create stream profile!\n");
		exit(EXIT_FAILURE);
	}


	ret = true;
	goto SUCCESS;
FAIL:
	if (e) {
		print_error(e);
	}
	if (*dev) {
		rs2_delete_device(*dev);
	}
	if (*pipeline) {
		rs2_delete_pipeline(*pipeline);
	}
SUCCESS:
	if (device_list) {
		rs2_delete_device_list(device_list);
	}
	if (ctx) {
		rs2_delete_context(ctx);
	}
	return ret;
}

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
	const rs2_stream_profile* stream_profile = NULL;
	rs2_pipeline *pipeline = NULL;
	rs2_error* e = NULL;


	success = parseArgs(argc, argv, &args);
	if (!success) {
		goto FAIL;
	}

	success = getFirstDevice(args, &pipeline, &stream_profile, &dev);
	if (!success) {
		goto FAIL;
	}

	const char *name = rs2_get_device_info(dev, RS2_CAMERA_INFO_NAME, &e);
	if (e) {
		goto FAIL;
	}
	printf("Using device \"%s\"\n", name);

	printStream(pipeline, stream_profile, dev); // no exit?
	ret = EXIT_SUCCESS;

FAIL:
	if (e) {
		print_error(e);
	}
	if (dev) {
		rs2_delete_device(dev);
	}
	if (pipeline) {
		rs2_delete_pipeline(pipeline);
	}
	return ret;
}
