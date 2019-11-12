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
#include "objs.h"

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

bool getFirstDevice(struct args args, struct objs *objs)
{
	bool ret = false;
	*objs = objs_default_value();

	objs->ctx = rs2_create_context(RS2_API_VERSION, &objs->err);
	if (objs->err) {
		objs->ctx = NULL;
		goto FAIL;
	}

	objs->device_list = rs2_query_devices(objs->ctx, &objs->err);
	if (objs->err) {
		objs->device_list = NULL;
		goto FAIL;
	}

	int dev_count = rs2_get_device_count(objs->device_list, &objs->err);
	if (objs->err) {
		goto FAIL;
	}
	if (dev_count == 0) {
		puts("There are no attached devices");
		goto FAIL;
	}

	objs->dev = rs2_create_device(objs->device_list, 0, &objs->err);
	if (objs->err) {
		objs->dev = NULL;
		goto FAIL;
	}

	objs->pipeline = rs2_create_pipeline(objs->ctx, &objs->err);
	if (objs->err) {
		objs->pipeline = NULL;
		goto FAIL;
	}

	objs->config = rs2_create_config(&objs->err);
	if (objs->err) {
		objs->config = NULL;
		goto FAIL;
	}

	// Request a specific configuration
	rs2_config_enable_stream(objs->config, STREAM, STREAM_INDEX, WIDTH, HEIGHT, FORMAT, FPS, &objs->err);
	if (objs->err) {
		goto FAIL;
	}

	//TODO: handle all these errors
	if(args.write) {
		rs2_config_enable_record_to_file(objs->config, args.file, &objs->err);
		if (objs->err) {
			goto FAIL;
		}
		objs->pipeline_profile = rs2_pipeline_start_with_config(objs->pipeline, objs->config, &objs->err);
		if (objs->err) {
			goto FAIL;
		}
	} else {
		rs2_config_enable_device_from_file(objs->config, args.file, &objs->err);
		if (objs->err) {
			goto FAIL;
		}

		rs2_delete_device(objs->dev);

		objs->pipeline_profile = rs2_pipeline_start_with_config(objs->pipeline, objs->config, &objs->err);
		if (objs->err) {
			goto FAIL;
		}

		objs->dev = rs2_pipeline_profile_get_device(objs->pipeline_profile, &objs->err);
		if (objs->err) {
			goto FAIL;
		}
	}

	objs->stream_profile_list = rs2_pipeline_profile_get_streams(objs->pipeline_profile, &objs->err);
	if (objs->err) {
		goto FAIL;
	}

	objs->stream_profile = rs2_get_stream_profile(objs->stream_profile_list, 0, &objs->err);
	if (objs->err) {
		goto FAIL;
	}


	ret = true;
	goto SUCCESS;
FAIL:
	if (objs->err) {
		print_error(objs->err);
	}
	objs_delete(*objs);
SUCCESS:
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
	struct objs objs;
	rs2_error* e = NULL;


	success = parseArgs(argc, argv, &args);
	if (!success) {
		goto FAIL;
	}

	success = getFirstDevice(args, &objs);
	if (!success) {
		goto FAIL;
	}

	const char *name = rs2_get_device_info(objs.dev, RS2_CAMERA_INFO_NAME, &e);
	if (e) {
		goto FAIL;
	}
	printf("Using device \"%s\"\n", name);

	printStream(objs);
	ret = EXIT_SUCCESS;

FAIL:
	if (e) {
		print_error(e);
	}
	objs_delete(objs);
	return ret;
}
