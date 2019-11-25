#include <stdbool.h>
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

#include "args.h"
#include "objs.h"

#define STREAM          RS2_STREAM_DEPTH  // rs2_stream is a types of data provided by RealSense device           //
#define FORMAT          RS2_FORMAT_Z16    // rs2_format is identifies how binary data is encoded within a frame   //
#define WIDTH           640               // Defines the number of columns for each frame or zero for auto resolve//
#define HEIGHT          0                 // Defines the number of lines for each frame or zero for auto resolve  //
#define FPS             30                // Defines the rate of frames per second                                //
#define STREAM_INDEX    0                 // Defines the stream index, used for multiple streams of the same type //

void print_error(rs2_error* e)
{
	printf("rs_error was raised when calling %s(%s):\n", rs2_get_failed_function(e), rs2_get_failed_args(e));
	printf("    %s\n", rs2_get_error_message(e));
}

struct objs objs;
int frame_width;
int frame_height;

bool initializeWithFirstDevice(struct args args)
{
	objs = objs_default_value();

	objs.ctx = rs2_create_context(RS2_API_VERSION, &objs.err);
	if (objs.err) {
		objs.ctx = NULL;
		goto FAIL;
	}

	objs.pipeline = rs2_create_pipeline(objs.ctx, &objs.err);
	if (objs.err) {
		objs.pipeline = NULL;
		goto FAIL;
	}

	objs.config = rs2_create_config(&objs.err);
	if (objs.err) {
		objs.config = NULL;
		goto FAIL;
	}

	// Request a specific configuration
	rs2_config_enable_stream(objs.config, STREAM, STREAM_INDEX, WIDTH, HEIGHT, FORMAT, FPS, &objs.err);
	if (objs.err) {
		goto FAIL;
	}

	if(args.write) {
		objs.device_list = rs2_query_devices(objs.ctx, &objs.err);
		if (objs.err) {
			objs.device_list = NULL;
			goto FAIL;
		}

		int dev_count = rs2_get_device_count(objs.device_list, &objs.err);
		if (objs.err) {
			goto FAIL;
		}
		if (dev_count == 0) {
			puts("There are no attached devices");
			goto FAIL;
		}

		objs.dev = rs2_create_device(objs.device_list, 0, &objs.err);
		if (objs.err) {
			objs.dev = NULL;
			goto FAIL;
		}

		rs2_config_enable_record_to_file(objs.config, args.file, &objs.err);
		if (objs.err) {
			goto FAIL;
		}
		objs.pipeline_profile = rs2_pipeline_start_with_config(objs.pipeline, objs.config, &objs.err);
		if (objs.err) {
			goto FAIL;
		}
	} else {
		rs2_config_enable_device_from_file(objs.config, args.file, &objs.err);
		if (objs.err) {
			goto FAIL;
		}

		objs.pipeline_profile = rs2_pipeline_start_with_config(objs.pipeline, objs.config, &objs.err);
		if (objs.err) {
			goto FAIL;
		}

		objs.dev = rs2_pipeline_profile_get_device(objs.pipeline_profile, &objs.err);
		if (objs.err) {
			goto FAIL;
		}
	}

	objs.stream_profile_list = rs2_pipeline_profile_get_streams(objs.pipeline_profile, &objs.err);
	if (objs.err) {
		goto FAIL;
	}

	objs.stream_profile = rs2_get_stream_profile(objs.stream_profile_list, 0, &objs.err);
	if (objs.err) {
		goto FAIL;
	}

	rs2_get_video_stream_resolution(objs.stream_profile, &frame_width, &frame_height, &objs.err);
	if (objs.err) {
		goto FAIL;
	}

	return true;
FAIL:
	if (objs.err) {
		print_error(objs.err);
	}
	objs_delete(objs);
	return false;
}


int cameraInit(struct args args)
{
	int ret = EXIT_FAILURE;
	bool success;
	rs2_error* e = NULL;


	success = initializeWithFirstDevice(args);
	if (!success) {
		goto FAIL;
	}

	return EXIT_SUCCESS;
FAIL:
	if (e) {
		print_error(e);
	}
	objs_delete(objs);
	return ret;
}

int cameraDestroy()
{
	objs_delete(objs);
}

bool cameraHasNewFrame()
{

}


uint16_t*  cameraGetFrame()
{
	rs2_error* e = NULL; //TODO: just use objs.err? We have to initialize it to NULL though, I think.
	bool fail = false;
	uint16_t *ret = NULL;
	rs2_frame* frames = rs2_pipeline_wait_for_frames(objs.pipeline, RS2_DEFAULT_TIMEOUT, &e);
	if (e) {
		frames = NULL;
		goto FAIL;
	}

        int cFrames = rs2_embedded_frames_count(frames, &e);
	if (e) {
		goto FAIL;
	}

        for (int iFrame = 0; iFrame < cFrames && ret == NULL; iFrame++) {
		rs2_frame* frame = rs2_extract_frame(frames, iFrame, &e);
		if (e) {
			goto FAIL;
		}

		bool isDepthFrame = rs2_is_frame_extendable_to(frame, RS2_EXTENSION_DEPTH_FRAME, &e);
		if (e) {
			goto FAIL;
		}

		if (isDepthFrame) {
			const uint16_t* data = (const uint16_t*)(rs2_get_frame_data(frame, &e));
			int numPixels = frame_width * frame_height;
			ret = malloc(sizeof(uint16_t) * numPixels);
			if (!ret) {
				fail = true;
				goto FAIL;
			}
			for (int i = 0; i < numPixels; i++) {
				ret[i] = data[i];
			}
		}
		rs2_release_frame(frame);
	}
	if (!ret) {
		fail = true;
	}
FAIL:
	if (frames) {
		rs2_release_frame(frames);
	}
	if (e) {
		print_error(e);
	}
	if (fail || e) {
		return NULL;
	}

	return ret;
}
