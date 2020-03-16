/*
 * NHVE Network Hardware Video Encoder C library implementation
 *
 * Copyright 2019-2020 (C) Bartosz Meglicki <meglickib@gmail.com>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#include "nhve.h"

// Minimal Latency Streaming Protocol library
#include "mlsp.h"
// Hardware Video Encoder library
#include "hve.h"

#include <stdio.h>

struct nhve
{
	struct mlsp *network_streamer;
	struct hve **hardware_encoder;
	int hardware_encoders_size;
};

static struct nhve *nhve_close_and_return_null(struct nhve *n);

struct nhve *nhve_init(const struct nhve_net_config *net_config,const struct nhve_hw_config *hw_config)
{
	return nhve_multi_init(net_config, hw_config, 1);
}

struct nhve *nhve_multi_init(const struct nhve_net_config *net_config,const struct nhve_hw_config *hw_config, int hw_size)
{
	struct nhve *n, zero_nhve = {0};
	struct mlsp_config mlsp_cfg = {net_config->ip, net_config->port, 0};

	if( ( n = (struct nhve*)malloc(sizeof(struct nhve))) == NULL )
	{
		fprintf(stderr, "nhve: not enough memory for nhve\n");
		return NULL;
	}

	*n = zero_nhve;
	//we need array of hardware encoder pointers
	if( ( n->hardware_encoder = (struct hve**)malloc(sizeof(struct hve*) * hw_size)) == NULL )
	{
		fprintf(stderr, "nhve: not enough memory for nhve hardware encoders\n");
		return nhve_close_and_return_null(n);
	}
	
	if( (n->network_streamer = mlsp_init_client(&mlsp_cfg)) == NULL )
	{
		fprintf(stderr, "nhve: failed to initialize network client\n");
		return nhve_close_and_return_null(n);
	}
	
	n->hardware_encoders_size = hw_size;

	for(int i=0;i<hw_size;++i)
		n->hardware_encoder[i] = NULL;

	for(int i=0;i<hw_size;++i)
	{		
		struct hve_config hve_cfg = {hw_config[i].width, hw_config[i].height, hw_config[i].framerate, hw_config[i].device,
		hw_config[i].encoder, hw_config[i].pixel_format, hw_config[i].profile, hw_config[i].max_b_frames, hw_config[i].bit_rate};

		if( (n->hardware_encoder[i] = hve_init(&hve_cfg)) == NULL )
		{
			fprintf(stderr, "nhve: failed to initalize hardware encoder %d\n", i);
			return nhve_close_and_return_null(n);
		}		
	}

	return n;
}

static struct nhve *nhve_close_and_return_null(struct nhve *n)
{
	nhve_close(n);
	return NULL;
}

void nhve_close(struct nhve *n)
{
	mlsp_close(n->network_streamer);
	for(int i=0;i<n->hardware_encoders_size;++i)
		hve_close(n->hardware_encoder[i]);
	free(n->hardware_encoder);
	free(n);
}

// NULL frame or NULL frame.data[0] to flush
int nhve_send_frame(struct nhve *n,struct nhve_frame *frame)
{
	struct hve_frame *final_video_frame = NULL, video_frame;

	// NULL is also valid input meaning - flush the encoder
	if(frame != NULL)
	{	//copy pointers to data and linesizes (just a few bytes)
		memcpy(video_frame.data, frame->data, sizeof(frame->data));
		memcpy(video_frame.linesize, frame->linesize, sizeof(frame->linesize));
		final_video_frame = &video_frame;
	}

	// send video frame data to hardware for encoding
	// in case frame argument was NULL pass NULL here for flushing encoder
	if( hve_send_frame(n->hardware_encoder[0], final_video_frame) != HVE_OK)
	{
		fprintf(stderr, "nhve: failed to send frame to hardware\n");
		return NHVE_ERROR;
	}

	AVPacket *encoded_frame;
	int failed;

	//get the encoded frame data from hardware
	while( (encoded_frame=hve_receive_packet(n->hardware_encoder[0], &failed)) )
	{
		struct mlsp_frame network_frame = {frame->framenumber, encoded_frame->data, encoded_frame->size};
		//send encoded frame data over network
		if ( mlsp_send(n->network_streamer, &network_frame) != MLSP_OK )
		{
			fprintf(stderr, "nhve: failed to send frame\n");
			return NHVE_ERROR;
		}
	}

	//NULL packet and non-zero failed indicates failure during encoding
	if(failed  != HVE_OK)
	{
		fprintf(stderr, "nhve: failed to encode frame\n");
		return NHVE_ERROR;
	}

	return NHVE_OK;
}
