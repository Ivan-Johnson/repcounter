// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.
//
// 2019-11-09, Ivan Johnson: merged with example.h and converted to a helper
// function instead of a stand alone app.


/* Include the librealsense C header files */
#include <librealsense2/rs.h>
#include <librealsense2/h/rs_pipeline.h>
#include <librealsense2/h/rs_option.h>
#include <librealsense2/h/rs_frame.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <signal.h>

// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#include <librealsense2/rs.h>
#include <stdio.h>
#include <stdlib.h>

#include "rs-depth.h"

/* Function calls to librealsense may raise errors of type rs_error*/
void check_error(rs2_error* e)
{
	if (e) {
		printf("rs_error was raised when calling %s(%s):\n", rs2_get_failed_function(e), rs2_get_failed_args(e));
		printf("    %s\n", rs2_get_error_message(e));
		exit(EXIT_FAILURE);
	}
}

void print_device_info(rs2_device* dev)
{
	rs2_error* e = 0;
	printf("\nUsing device 0, an %s\n", rs2_get_device_info(dev, RS2_CAMERA_INFO_NAME, &e));
	check_error(e);
	printf("    Serial number: %s\n", rs2_get_device_info(dev, RS2_CAMERA_INFO_SERIAL_NUMBER, &e));
	check_error(e);
	printf("    Firmware version: %s\n\n", rs2_get_device_info(dev, RS2_CAMERA_INFO_FIRMWARE_VERSION, &e));
	check_error(e);
}

#define HEIGHT_RATIO    20                // Defines the height ratio between the original frame to the new frame //
#define WIDTH_RATIO     10                // Defines the width ratio between the original frame to the new frame  //

// The number of meters represented by a single depth unit
float get_depth_unit_value(const rs2_device* const dev)
{
	rs2_error* e = 0;
	rs2_sensor_list* sensor_list = rs2_query_sensors(dev, &e);
	check_error(e);

	int num_of_sensors = rs2_get_sensors_count(sensor_list, &e);
	check_error(e);

	float depth_scale = 0;
	int is_depth_sensor_found = 0;
	int i;
	for (i = 0; i < num_of_sensors; ++i) {
		rs2_sensor* sensor = rs2_create_sensor(sensor_list, i, &e);
		check_error(e);

		// Check if the given sensor can be extended to depth sensor interface
		is_depth_sensor_found = rs2_is_sensor_extendable_to(sensor, RS2_EXTENSION_DEPTH_SENSOR, &e);
		check_error(e);

		if (1 == is_depth_sensor_found) {
			depth_scale = rs2_get_option((const rs2_options*)sensor, RS2_OPTION_DEPTH_UNITS, &e);
			check_error(e);
			rs2_delete_sensor(sensor);
			break;
		}
		rs2_delete_sensor(sensor);
	}
	rs2_delete_sensor_list(sensor_list);

	if (0 == is_depth_sensor_found) {
		printf("Depth sensor not found!\n");
		exit(EXIT_FAILURE);
	}

	return depth_scale;
}

static volatile bool plsStop;
void intHandler(int dummy) {
	plsStop = true;
	signal(SIGINT, SIG_DFL);
}

void printStream(struct objs objs)
{
	plsStop = false;
	signal(SIGINT, intHandler);

	rs2_error* e = 0;

	/* Determine depth value corresponding to one meter */
	uint16_t one_meter = (uint16_t)(1.0f / get_depth_unit_value(objs.dev));

	rs2_stream stream;
	rs2_format format;
	int index;
	int unique_id;
	int framerate;
	rs2_get_stream_profile_data(objs.stream_profile, &stream, &format, &index, &unique_id, &framerate, &e);
	if (e) {
		printf("Failed to get stream profile data!\n");
		exit(EXIT_FAILURE);
	}

	int width;
	int height;
	rs2_get_video_stream_resolution(objs.stream_profile, &width, &height, &e);
	if (e) {
		printf("Failed to get video stream resolution data!\n");
		exit(EXIT_FAILURE);
	}
	int rows = height / HEIGHT_RATIO;
	int row_length = width / WIDTH_RATIO;
	int display_size = (rows + 1) * (row_length + 1);
	int buffer_size = display_size * sizeof(char);

	char* buffer = calloc(display_size, sizeof(char));
	char* out = NULL;

	while (!plsStop) {
		// This call waits until a new composite_frame is available
		// composite_frame holds a set of frames. It is used to prevent frame drops
		// The returned object should be released with rs2_release_frame(...)
		rs2_frame* frames = rs2_pipeline_wait_for_frames(objs.pipeline, RS2_DEFAULT_TIMEOUT, &e);
		check_error(e);

		// Returns the number of frames embedded within the composite frame
		int num_of_frames = rs2_embedded_frames_count(frames, &e);
		check_error(e);

		int i;
		for (i = 0; i < num_of_frames; ++i) {
			// The retunred object should be released with rs2_release_frame(...)
			rs2_frame* frame = rs2_extract_frame(frames, i, &e);
			check_error(e);

			// Check if the given frame can be extended to depth frame interface
			// Accept only depth frames and skip other frames
			if (0 == rs2_is_frame_extendable_to(frame, RS2_EXTENSION_DEPTH_FRAME, &e)) {
				rs2_release_frame(frame);
				continue;
			}

			/* Retrieve depth data, configured as 16-bit depth values */
			const uint16_t* depth_frame_data = (const uint16_t*)(rs2_get_frame_data(frame, &e));
			check_error(e);

			/* Print a simple text-based representation of the image, by breaking it into 10x5 pixel regions and approximating the coverage of pixels within one meter */
			out = buffer;
			int x, y, i;
			int* coverage = calloc(row_length, sizeof(int));

			for (y = 0; y < height; ++y) {
				for (x = 0; x < width; ++x) {
					// Create a depth histogram to each row
					int coverage_index = x / WIDTH_RATIO;
					int depth = *depth_frame_data++;
					if (depth > 0 && depth < one_meter) {
						++coverage[coverage_index];
					}
				}

				if ((y % HEIGHT_RATIO) == (HEIGHT_RATIO-1)) {
					for (i = 0; i < (row_length); ++i) {
						static const char* pixels = " .:nhBXWW";
						int pixel_index = (coverage[i] / (HEIGHT_RATIO * WIDTH_RATIO / sizeof(pixels)));
						*out++ = pixels[pixel_index];
						coverage[i] = 0;
					}
					*out++ = '\n';
				}
			}
			*out++ = 0;
			printf("\n%s", buffer);

			free(coverage);
			rs2_release_frame(frame);
		}

		rs2_release_frame(frames);
	}

	free(buffer);
}
