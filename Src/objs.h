#ifndef OBJS
#define OBJS

#include <librealsense2/h/rs_types.h>

struct objs {
	rs2_config *config;
	rs2_context* ctx;
	rs2_device_list* device_list;
	rs2_error* err;
	rs2_pipeline *pipeline;
	const rs2_stream_profile* stream_profile;
	rs2_device *dev;
	rs2_stream_profile_list* stream_profile_list;
	rs2_pipeline_profile* pipeline_profile;
};

struct objs objs_default_value();
void objs_delete(struct objs objs);

#endif
