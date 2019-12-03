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
#include "ccamera.h"

AVCodecContext *ctx = NULL;
AVPacket *pkt;
FILE *outfile;
AVCodec *codec;
uint8_t endcode[] = { 0, 0, 1, 0xb7 };
AVFrame *frame;
int iFrame;

static int encode(AVFrame *frame)
{
	int ret;

	/* send the frame to the encoder */
	ret = avcodec_send_frame(ctx, frame);
	if (ret < 0) {
		return ret;
	}

	while (ret >= 0) {
		ret = avcodec_receive_packet(ctx, pkt);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
			ret = 0;
			break;
		} else if (ret < 0) {
			ret = 1;
			break;
		}

		int tmp = pkt->size;
		assert(tmp > 0);
		size_t size = (size_t) tmp;
		fwrite(pkt->data, 1, size, outfile);
		av_packet_unref(pkt);
	}

	return ret;
}

static int prepEncode()
{
	int ret;
	/* make sure the frame data is writable */
	ret = av_frame_make_writable(frame);
	if (ret < 0) {
		return 1;
	}
	return 0;
}

int videoEncodeColor(float tmp)
{
	assert(0 <= tmp && tmp <= 1);
	uint8_t color = (uint8_t) (tmp * UINT8_MAX);

	int fail;

	fail = prepEncode();
	if (fail) {
		goto DONE;
	}

	for (int y = 0; y < ctx->height; y++) {
		for (int x = 0; x < ctx->width; x++) {
			frame->data[0][y * frame->linesize[0] + x] = color; // Y
		}
	}

	frame->pts = iFrame;
	iFrame++;

	fail = encode(frame);
DONE:
	return fail;
}

int videoEncodeFrame(uint16_t *data)
{
	int fail;

	fail = prepEncode();
	if (fail) {
		goto DONE;
	}

	// Choosen arbitrarily for one particular situation.
	uint16_t inputMax = 4000;

	for (int y = 0; y < ctx->height; y++) {
		for (int x = 0; x < ctx->width; x++) {
			uint16_t value = data[y*ctx->width + x];

			// clip to max
			value = value <= inputMax ? value : inputMax;

			// scale to uint8_t so that it fits in ffmpeg frame
			uint8_t small = (uint8_t) (value * ((float) UINT8_MAX / inputMax));

			// invert colors
			small = (uint8_t) (UINT8_MAX - small);

			frame->data[0][y * frame->linesize[0] + x] = small; // Y
		}
	}

	frame->pts = iFrame;
	iFrame++;

	fail = encode(frame);

DONE:
	return fail;
}

int videoStart(char *filename)
{
	int fail;

	char *codec_name = "mpeg2video";
	iFrame = 0;

	codec = avcodec_find_encoder_by_name(codec_name);
	if (!codec) {
		fail = 1;
		goto DONE;
	}

	ctx = avcodec_alloc_context3(codec);
	if (!ctx) {
		fail = 1;
		goto DONE;
	}

	pkt = av_packet_alloc();
	if (!pkt) {
		fail = 1;
		goto DONE;
	}

	/* put sample parameters */
	ctx->bit_rate = 400000;
	/* resolution must be a multiple of two */
	size_t width = ccameraGetFrameWidth();
	size_t height = ccameraGetFrameHeight();
	assert(width < INT_MAX);
	assert(height < INT_MAX);
	ctx->width = (int) width;
	ctx->height = (int) height;
	/* frames per second */
	//todo: 25->CAMERA_FPS?
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
	int ret = avcodec_open2(ctx, codec, NULL);
	if (ret < 0) {
		fail = 1;
		goto DONE;
	}

	outfile = fopen(filename, "wb");
	if (!outfile) {
		fail = 1;
		goto DONE;
	}

	frame = av_frame_alloc();
	if (!frame) {
		fail = 1;
		goto DONE;
	}
	frame->format = ctx->pix_fmt;
	frame->width  = ctx->width;
	frame->height = ctx->height;

	ret = av_frame_get_buffer(frame, 32);
	if (ret < 0) {
		fail = 1;
		goto DONE;
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

	fail = 0;
DONE:
	if (fail) {
		videoStop();
	}
	return fail;
}

int videoStop()
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

	return 0;
}
