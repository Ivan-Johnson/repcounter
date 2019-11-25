// Heavily based on:
// https://github.com/FFmpeg/FFmpeg/blob/1ca978d6366f3c7d7df6b3d50566e892f8da605a/doc/examples/encode_video.c

/*
 * Copyright (c) 2001 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * @file
 * video encoding with libavcodec API example
 *
 * @example encode_video.c
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>

#include <libavutil/opt.h>
#include <libavutil/imgutils.h>

#include "video.h"
#include "camera.h"

AVCodecContext *ctx = NULL;
AVPacket *pkt;
FILE *outfile;
AVCodec *codec;
uint8_t endcode[] = { 0, 0, 1, 0xb7 };
AVFrame *frame;
int iFrame;

static void encode(AVFrame *frame)
{
	int ret;

	/* send the frame to the encoder */
	if (frame)
		printf("Send frame %3"PRId64"\n", frame->pts);

	ret = avcodec_send_frame(ctx, frame);
	if (ret < 0) {
		fprintf(stderr, "Error sending a frame for encoding\n");
		exit(1);
	}

	while (ret >= 0) {
		ret = avcodec_receive_packet(ctx, pkt);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			return;
		else if (ret < 0) {
			fprintf(stderr, "Error during encoding\n");
			exit(1);
		}

		printf("Write packet %3"PRId64" (size=%5d)\n", pkt->pts, pkt->size);
		fwrite(pkt->data, 1, pkt->size, outfile);
		av_packet_unref(pkt);
	}
}

bool videoEncodeFrame(uint16_t *data)
{
	int ret;
	/* make sure the frame data is writable */
	ret = av_frame_make_writable(frame);
	if (ret < 0) {
		goto FAIL;
	}

	// Choosen arbitrarily for one particular situation.
	// TODO: does this value yield good performance in general?
	uint16_t inputMax = 4000;

	//UINT8_MAX

	for (int y = 0; y < ctx->height; y++) {
		for (int x = 0; x < ctx->width; x++) {
			uint16_t value = data[y*ctx->width + x];

			// clip to max
			value = value <= inputMax ? value : inputMax;

			// scale to uint8_t
			value = (uint16_t) (value * ((float) UINT8_MAX / inputMax));

			// invert colors
			value = UINT8_MAX - value;

			frame->data[0][y * frame->linesize[0] + x] = value; // Y
		}
	}

	frame->pts = iFrame;
	iFrame++;

	/* encode the image */
	encode(frame);

	return true;
FAIL:
	return false;
}

bool videoStart(char *filename)
{
	int ret;

	char *codec_name = "mpeg2video";
	iFrame = 0;

	codec = avcodec_find_encoder_by_name(codec_name);
	if (!codec) {
		fprintf(stderr, "Codec '%s' not found\n", codec_name);
		goto FAIL;
	}

	ctx = avcodec_alloc_context3(codec);
	if (!ctx) {
		fprintf(stderr, "Could not allocate video codec context\n");
		goto FAIL;
	}

	pkt = av_packet_alloc();
	if (!pkt) {
		goto FAIL;
	}

	/* put sample parameters */
	ctx->bit_rate = 400000;
	/* resolution must be a multiple of two */
	ctx->width = cameraGetFrameWidth();
	ctx->height = cameraGetFrameHeight();
	/* frames per second */
	//TODO: 25->CAMERA_FPS?
	ctx->time_base = (AVRational){1, 25};
	ctx->framerate = (AVRational){25, 1};

	/* emit one intra frame every ten frames
	 * check frame pict_type before passing frame
	 * to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
	 * then gop_size is ignored and the output of encoder
	 * will always be I frame irrespective to gop_size
	 */
	ctx->gop_size = 10;
	ctx->max_b_frames = 1;
	ctx->pix_fmt = AV_PIX_FMT_YUV420P;

	/* open it */
	ret = avcodec_open2(ctx, codec, NULL);
	if (ret < 0) {
		fprintf(stderr, "Could not open codec: %s\n", av_err2str(ret));
		goto FAIL;
	}

	outfile = fopen(filename, "wb");
	if (!outfile) {
		fprintf(stderr, "Could not open %s\n", filename);
		goto FAIL;
	}

	frame = av_frame_alloc();
	if (!frame) {
		fprintf(stderr, "Could not allocate video frame\n");
		goto FAIL;
	}
	frame->format = ctx->pix_fmt;
	frame->width  = ctx->width;
	frame->height = ctx->height;

	ret = av_frame_get_buffer(frame, 32);
	if (ret < 0) {
		fprintf(stderr, "Could not allocate the video frame data\n");
		goto FAIL;
	}

	// note that Cb & Cr have half the dimensions of Y.
	for (int y = 0; y < ctx->height/2; y++) {
		for (int x = 0; x < ctx->width/2; x++) {
			// this particular data formate use uint8_t to store data.
			// using half the maximum value for Cb & Cr makes the image black and white.
			frame->data[1][y * frame->linesize[1] + x] = UINT8_MAX / 2; //Cb
			frame->data[2][y * frame->linesize[2] + x] = UINT8_MAX / 2; //Cr
		}
	}

	return true;

FAIL:
	videoStop();
	return false;
}

bool videoStop()
{
	/* flush the encoder */
	encode(NULL);

	/* add sequence end code to have a real MPEG file */
	assert(codec->id == AV_CODEC_ID_MPEG2VIDEO);
	fwrite(endcode, 1, sizeof(endcode), outfile);
	fclose(outfile);

	avcodec_free_context(&ctx);
	av_frame_free(&frame);
	av_packet_free(&pkt);

	return true;
}
