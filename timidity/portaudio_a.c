/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2002 Masanao Izumo <mo@goice.co.jp>
    Copyright (C) 1995 Tuukka Toivonen <tt@cgs.fi>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    portaudio_a.c by Avatar <avatar@deva.net>
    based on esd_a.c

    Functions to play sound through EsounD
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

//#include <windows.h>
#include <portaudio.h>

#include "timidity.h"
#include "common.h"
#include "output.h"
#include "controls.h"
#include "timer.h"
#include "instrum.h"
#include "playmidi.h"
#include "miditrace.h"

/* followings are only needed by w32g_util.c */
volatile int data_block_num = 64;
volatile int data_block_bits = DEFAULT_AUDIO_BUFFER_BITS;

#define DATA_BLOCK_SIZE     (27648*4) /* WinNT Latency is 600 msec read pa_dsound.c */
#define DATA_BLOCK_NUM      (dpm.extra_param[0])
#define SAMPLE_RATE         (48000)

static int open_output(void); /* 0=success, 1=warning, -1=fatal error */
static void close_output(void);
static int output_data(char *buf, int32 nbytes);
static int acntl(int request, void *arg);

static int framesPerBuffer=256;
static int stereo=2;
static int data_nbyte;
static int numBuffers;
static unsigned int framesPerInBuffer;
static unsigned int bytesPerInBuffer;
static int  firsttime;

PortAudioStream *stream;
PaError  err;
typedef struct {
	char buf[DATA_BLOCK_SIZE*2];
	int32 samplesToGo;
	char *bufpoint;
	char *bufepoint;
} padata_t;
padata_t pa_data;

/* export the playback mode */

#define dpm portaudio_play_mode

PlayMode dpm = {
    SAMPLE_RATE,
    PE_16BIT|PE_SIGNED,
    PF_PCM_STREAM|PF_BUFF_FRAGM_OPT/*|PF_CAN_TRACE*/,
    -1,
    {32}, /* PF_BUFF_FRAGM_OPT  is need for TWSYNTH */
	"Portaudio Dirver", 'p',
    NULL,
    open_output,
    close_output,
    output_data,
    acntl
};


int paCallback(  void *inputBuffer, void *outputBuffer,
                     unsigned long framesPerBuffer,
                     PaTimestamp outTime, void *userData )
{
    unsigned int i;
	int finished = 0;
/* Cast data passed through stream to our structure type. */
//    pa_data_t pa_data = (pa_data_t*)userData;
    char *out = (char*)outputBuffer;
	if(pa_data.samplesToGo < framesPerBuffer*data_nbyte*stereo  ){
		for(i=0;i<pa_data.samplesToGo;i++){
			*out++ = *(pa_data.bufpoint)++;
			if( pa_data.buf+bytesPerInBuffer*2 <= pa_data.bufpoint ){
				pa_data.bufpoint=pa_data.buf;
			}
		}
		pa_data.samplesToGo=0;
		for(;i<framesPerBuffer*data_nbyte*stereo;i++){
			*out++=0;
		}
		finished = 0;
	}else{
		for(i=0;i<framesPerBuffer*data_nbyte*stereo;i++){
			*out++=*(pa_data.bufpoint)++;
			if( pa_data.buf+bytesPerInBuffer*2 <= pa_data.bufpoint ){
				pa_data.bufpoint=pa_data.buf;
			}
		}
		pa_data.samplesToGo -= framesPerBuffer*data_nbyte*stereo;
	}
    return finished ;
}


static int open_output(void)
{
	dpm.encoding = dpm.encoding  & !((int32)PE_ULAW) & !((int32)PE_ALAW) & !((int32)PE_BYTESWAP);
	dpm.encoding = dpm.encoding|PE_SIGNED;
	stereo=(dpm.encoding & PE_MONO)?1:2;
	data_nbyte=(dpm.encoding & PE_16BIT)?sizeof(int16):sizeof(int8);

	pa_data.samplesToGo=0;
	pa_data.bufpoint=pa_data.buf;
	pa_data.bufepoint=pa_data.buf;
	firsttime=1;
	numBuffers=Pa_GetMinNumBuffers( framesPerBuffer, dpm.rate );
	framesPerInBuffer=numBuffers*framesPerBuffer;
	if(framesPerInBuffer<4096) framesPerInBuffer=4096;
	bytesPerInBuffer=framesPerInBuffer*data_nbyte*stereo;
	printf("%d\n",framesPerInBuffer);
	
	err = Pa_Initialize();
	if( err != paNoError ) goto error;
	err = Pa_OpenDefaultStream(
    	&stream,        /* passes back stream pointer */
    	0,              /* no input channels */
    	stereo,              /* 2:stereo 1:mono output */
    	(dpm.encoding & PE_16BIT)?paInt16:paInt8,      /* 16 bit 8bit output */
		dpm.rate,          /* sample rate */
    	framesPerBuffer,            /* frames per buffer */
    	numBuffers,              /* number of buffers, if zero then use default minimum */
    	paCallback, /* specify our custom callback */
    	&pa_data);   /* pass our data through to callback */
	if( err != paNoError ) goto error;
	return 0;
error:
	Pa_Terminate();
	ctl->cmsg(  CMSG_ERROR, VERB_NORMAL, "PortAudio error: %s\n", Pa_GetErrorText( err ) );
	return -1;
}
static int output_data(char *buf, int32 nbytes)
{
	unsigned int i;
	

//	if(pa_data.samplesToGo > DATA_BLOCK_SIZE){ 
//		Sleep(  (pa_data.samplesToGo - DATA_BLOCK_SIZE)/dpm.rate/4  );
//	}
	for(i=0;i<nbytes;i++){
		*(pa_data.bufepoint)++ = *buf++ ;
		if( pa_data.buf+bytesPerInBuffer*2 <= pa_data.bufepoint ){
			pa_data.bufepoint=pa_data.buf;
		}
	}
	pa_data.samplesToGo += nbytes;
/*
	if(firsttime==1){
		err = Pa_StartStream( stream );
		if( err != paNoError ) goto error;
		firsttime=0;
	}
*/
	if( 0==Pa_StreamActive(stream)){
		err = Pa_StartStream( stream );
		if( err != paNoError ) goto error;
	}
	while(pa_data.samplesToGo > bytesPerInBuffer){ Pa_Sleep(1);};
//	Pa_Sleep( (pa_data.samplesToGo - bytesPerInBuffer)/dpm.rate * 1000);
	return 0;
error:
	Pa_Terminate();
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "PortAudio error: %s\n", Pa_GetErrorText( err ) );
	return -1;
}

static void close_output(void)
{	
	if( 1==Pa_StreamActive(stream)){
		Pa_Sleep(  bytesPerInBuffer/dpm.rate*1000  );
	}
	err = Pa_StopStream( stream );
	if( err != paNoError ) goto error;
	err = Pa_CloseStream( stream );
	if( err != paNoError ) goto error;
	Pa_Terminate();
	return;
error:
	Pa_Terminate();
	ctl->cmsg(  CMSG_ERROR, VERB_NORMAL, "PortAudio error: %s\n", Pa_GetErrorText( err ) );
	return;
}

static int acntl(int request, void *arg)
{
    switch(request)
    {
      case PM_REQ_GETQSIZ:
		 *(int *)arg = bytesPerInBuffer*2;
    	return 0;
		break;
      case PM_REQ_GETFILLABLE:
		 *(int *)arg = bytesPerInBuffer*2-pa_data.samplesToGo;
    	return 0;
		break;
      case PM_REQ_GETFILLED:
		 *(int *)arg = pa_data.samplesToGo;
    	return 0;
		break;
      case PM_REQ_DISCARD:
    	Pa_StopStream( stream );
    	close_output();
	    open_output();
		return 0;
		break;
      case PM_REQ_FLUSH:
    	close_output();
	    open_output();
		return 0;
		break;
      case PM_REQ_RATE:  /* NOT WORK */
    	{
    		int i;
    		double sampleRateBack;
    		i = *(int *)arg; /* sample rate in and out */
    		close_output();
    		sampleRateBack=dpm.rate;
    		dpm.rate=i;
    		if(0==open_output()){
    			return 0;
    		}else{    		
    			dpm.rate=sampleRateBack;
    			open_output();
    			return -1;
    		}
    	}
    	break;
    }
    return -1;
}
