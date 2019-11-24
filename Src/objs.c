#include <stdlib.h>

#include "objs.h"

#include <librealsense2/h/rs_frame.h>
#include <librealsense2/h/rs_option.h>
#include <librealsense2/h/rs_pipeline.h>
#include <librealsense2/rs.h>

struct objs objs_default_value()
{
	struct objs objs;
	objs.config = NULL;
	objs.ctx = NULL;
	objs.device_list = NULL;
	objs.err = NULL;
	objs.pipeline = NULL;
	objs.stream_profile = NULL;
	objs.dev = NULL;
	objs.device_list = NULL;
	objs.pipeline_profile = NULL;
	return objs;
}

void objs_delete(struct objs objs)
{
	if (objs.config) {
		rs2_delete_config(objs.config);
	}
	if (objs.ctx) {
		rs2_delete_context(objs.ctx);
	}
	if (objs.device_list) {
		rs2_delete_device_list(objs.device_list);
	}
	if (objs.err) {
		//TODO: is it correct to do nothing here?
		//rs2_delete_config(objs.config);
	}
	if (objs.pipeline) {
		rs2_delete_pipeline(objs.pipeline);
	}
	if (objs.stream_profile) {
		//TODO: is it correct to do nothing here? objs.stream_profile is const, so...
		//rs2_delete_stream_profile(objs.stream_profile);
	}
	if (objs.dev) {
		rs2_delete_device(objs.dev);
	}
	if (objs.stream_profile_list) {
		rs2_delete_stream_profiles_list(objs.stream_profile_list);
	}
	if (objs.pipeline_profile) {
		rs2_delete_pipeline_profile(objs.pipeline_profile);
	}
}
