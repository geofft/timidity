/* -*- c-file-style: "gnu" -*-
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2002 Masanao Izumo <mo@goice.co.jp>
    Copyright (C) 1995 Tuukka Toivonen <tt@cgs.fi>
    ALSA 0.[56] support by Katsuhiro Ueno <katsu@blue.sky.or.jp>

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
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    darwin_a.c

    Functions to play sound on the Darwin audio driver
    by T.Nogami <t-nogami@happy.email.ne.jp>
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <CoreAudio/AudioHardware.h>

#include "timidity.h"
#include "common.h"
#include "output.h"
#include "controls.h"
#include "timer.h"
#include "instrum.h"
#include "playmidi.h"
#include "miditrace.h"

static int open_output(void); /* 0=success, 1=warning, -1=fatal error */
static void close_output(void);
static int output_data(char *buf, int32 nbytes);
static int acntl(int request, void *arg);
static int total_bytes;
float output_volume = 1.0;

int	    mac_buf_using_num;

/* export the playback mode */

/*********************************************************************/
#define dpm darwin_play_mode

PlayMode dpm = {
  //  DEFAULT_RATE, PE_16BIT|PE_SIGNED, PF_PCM_STREAM|PF_CAN_TRACE,
    44100, PE_16BIT|PE_SIGNED, PF_PCM_STREAM|PF_CAN_TRACE,

  -1,
  {0}, /* default: get all the buffer fragments you can */
  "Mac OS X pcm device", 'd',
  "",
  open_output,
  close_output,
  output_data,
  acntl
};

#define FailWithAction(cond, action, handler)				\
	if (cond) {							\
		{ action; }						\
		goto handler;						\
	}

//#define DEBUG_DARWIN_A(x)   /*nothing*/
#define DEBUG_DARWIN_A(x)   ctl->cmsg x;

/*********************************************************************/

typedef struct {
    Boolean		soundPlaying;
    AudioDeviceID	device;			// the default device
    UInt32	        deviceBufferSize;
                  // bufferSize returned by kAudioDevicePropertyBufferSize
    AudioStreamBasicDescription		deviceFormat;
                  // info about the default device
#define BUFNUM 256
#define BUFLEN 4096
    char             buffer[BUFNUM][BUFLEN];   //1 buffer size = 4096bytes
    unsigned short   buffer_len[BUFNUM];
    volatile int     currBuf, nextBuf;
    int		     samples, get_samples;
} appGlobals, *appGlobalsPtr, **appGlobalsHandle;	

static appGlobals	globals;


/*********************************************************************/
OSStatus appIOProc (AudioDeviceID  inDevice, const AudioTimeStamp*  inNow,
                    const AudioBufferList*  inInputData,
		    const AudioTimeStamp*  inInputTime, 
                    AudioBufferList*  outOutputData,
		    const AudioTimeStamp* inOutputTime, void* dummy )
{
    int       next_curr;
        
    if( globals.currBuf==globals.nextBuf ){  //end of play
        outOutputData->mNumberBuffers=0;
        memset(outOutputData->mBuffers[0].mData, 0, globals.deviceBufferSize);
        globals.soundPlaying=0;
        return 0;
    }
    
    if( output_volume==1.0 ){
        memcpy(  outOutputData->mBuffers[0].mData,
             &globals.buffer[globals.currBuf][0], 
             globals.deviceBufferSize);   // move data into output data buffer
    }else{
        float   *src = (float*)&globals.buffer[globals.currBuf][0],
                *dst = outOutputData->mBuffers[0].mData;
        int     i, quant = globals.deviceBufferSize/sizeof(float);
        for(i=0; i<quant; i++){
            *dst++ = (*src++)*output_volume;
        }
    }
    outOutputData->mBuffers[0].mDataByteSize =
      globals.buffer_len[globals.currBuf];
    
    outOutputData->mNumberBuffers=1;
    globals.samples += globals.buffer_len[globals.currBuf]
                              / globals.deviceFormat.mBytesPerPacket;
    
    next_curr = globals.currBuf+1;
    next_curr %= BUFNUM;
    mac_buf_using_num--;
    globals.currBuf = next_curr;
    
    return 0; //no err
}

/*********************************************************************/
static void init_variable()
{
    globals.samples = globals.get_samples = 0;
    globals.currBuf = globals.nextBuf = 0;
    globals.soundPlaying = 0;
    mac_buf_using_num = 0;
}

/*********************************************************************/
/*return value == 0 sucess
               == -1 fails
 */
static int open_output(void)
{
    OSStatus				err = 0; //no err
    UInt32				count,
                                        bufferSize;
    AudioDeviceID			device = kAudioDeviceUnknown;
    AudioStreamBasicDescription		format;

    // get the default output device for the HAL
    count = sizeof(globals.device);
    // it is required to pass the size of the data to be returned
    err = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice,
                                   &count, (void *) &device);
    if (err != 0) goto Bail;
    
    // get the buffersize that the default device uses for IO
    count = sizeof(globals.deviceBufferSize);
              // it is required to pass the size of the data to be returned
    err = AudioDeviceGetProperty(device, 0, 0, kAudioDevicePropertyBufferSize,
                                 &count, &bufferSize);
    if (err != 0) goto Bail;
   
    if( globals.deviceBufferSize>BUFLEN ){
        fprintf(stderr, "globals.deviceBufferSize NG: %ld\n",
                globals.deviceBufferSize);
        exit(1);
    }
   
         // get a description of the data format used by the default device
    count = sizeof(globals.deviceFormat);
         // it is required to pass the size of the data to be returned
    err = AudioDeviceGetProperty(device, 0, 0,
                                 kAudioDevicePropertyStreamFormat,
                                 &count, &format);
    if (err != 0) goto Bail;
    FailWithAction(format.mFormatID != kAudioFormatLinearPCM, err = -1, Bail);
                    // bail if the format is not linear pcm
    
    // everything is ok so fill in these globals
    globals.device = device;
    globals.deviceBufferSize = bufferSize;
    globals.deviceFormat = format;
    init_variable();
    
    err = AudioDeviceAddIOProc(globals.device, appIOProc, 0 );
                    // setup our device with an IO proc
    if (err != 0) goto Bail;

    globals.deviceFormat.mSampleRate = dpm.rate;

#if 0
    globals.deviceFormat.mFormatFlags =  kLinearPCMFormatFlagIsBigEndian
                                       | kLinearPCMFormatFlagIsPacked
                                       | kLinearPCMFormatFlagIsSignedInteger;
    globals.deviceFormat.mBytesPerPacket = 4;
    globals.deviceFormat.mBytesPerFrame = 4;
    globals.deviceFormat.mBitsPerChannel = 0x10;
    
    err = AudioDeviceSetProperty(device, &inWhen, 0, 0,
                                 kAudioDevicePropertyStreamFormat,
                                 count, &globals.deviceFormat);
    if (err != 0) goto Bail;
#endif

#if 0
    fprintf(stderr, "deviceBufferSize = %d\n", globals.deviceBufferSize);
    fprintf(stderr, "mSampleRate = %g\n", globals.deviceFormat.mSampleRate);
    fprintf(stderr, "mFormatID = 0x%08x\n", globals.deviceFormat.mFormatID);
    fprintf(stderr, "mFormatFlags = 0x%08x\n",
            globals.deviceFormat.mFormatFlags);
    fprintf(stderr, "mBytesPerPacket = 0x%08x\n",
            globals.deviceFormat.mBytesPerPacket);
    fprintf(stderr, "mBytesPerFrame = 0x%08x\n",
            globals.deviceFormat.mBytesPerFrame);
    fprintf(stderr, "mBitsPerChannel = 0x%08x\n",
            globals.deviceFormat.mBitsPerChannel);
#endif

Bail:
    return (err);
}

/*********************************************************************/
static int output_data(char *buf, int32 nbytes)
{
    OSStatus		err = 0;
    int         next_nextbuf, outbytes=nbytes;
    int         inBytesPerQuant, max_quant, max_inbytes, out_quant, i;
    

    if (dpm.encoding & PE_16BIT){
            inBytesPerQuant = 2;
    }else{
            ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
                "Sorry, not support 8bit sound.");
            exit(1);
    }
    
    max_quant = globals.deviceBufferSize 
                   / (globals.deviceFormat.mBytesPerPacket/2);
    max_inbytes = max_quant * inBytesPerQuant;
        
 redo:
    outbytes=nbytes;
    if( outbytes > max_inbytes ){
        outbytes = max_inbytes;
        out_quant = outbytes/inBytesPerQuant;
    }

    next_nextbuf = globals.nextBuf+1;
    next_nextbuf %= BUFNUM;
    
    while( globals.currBuf==next_nextbuf ){ //queue full
        usleep(10000); //0.01sec
    }
    
    for( i=0; i<out_quant; i++){
        ((float*)(globals.buffer[globals.nextBuf]))[i] =
                          ((short*)buf)[i]/32768.0;
    }
    
    
    globals.buffer_len[globals.nextBuf] = outbytes;
    
    err = AudioDeviceStart(globals.device, appIOProc);
                        // start playing sound through the device
    if (err != 0) goto Bail;

    globals.nextBuf = next_nextbuf;
    globals.soundPlaying = 1;	// set the playing status global to true
    
    nbytes -= outbytes;
    buf += outbytes;
    mac_buf_using_num++;
    globals.get_samples += outbytes/globals.deviceFormat.mBytesPerPacket;
    
    if( nbytes ){
        goto redo;
    }
    
 Bail:
    return (err);
} 

/*********************************************************************/
static void close_output(void)
{
    OSStatus 	err = 0;
    
    err = AudioDeviceStop(globals.device, appIOProc);
                            // stop playing sound through the device
    if (err != 0) goto Bail;

    err = AudioDeviceRemoveIOProc(globals.device, appIOProc);
                       // remove the IO proc from the device
    if (err != 0) goto Bail;
    
    globals.soundPlaying = 0;
                    // set the playing status global to false
Bail:;
}

/*********************************************************************/
static int acntl(int request, void *arg)
{
    switch (request){
      case PM_REQ_GETFRAGSIZ:
	*((int *)arg) = BUFLEN;
	return 0;
        
	//case PM_REQ_GETQSIZ:
	if (total_bytes == -1)
	  return -1;
	*((int *)arg) = sizeof(globals.buffer);
	return 0;

      /*case PM_REQ_GETFILLABLE:
	if (total_bytes == -1)
	  return -1;
	if(!(dpm.encoding & PE_MONO)) i >>= 1;
	if(dpm.encoding & PE_16BIT) i >>= 1;
	*((int *)arg) = i;
	return 0;*/

      /*case PM_REQ_GETFILLED:

	i = pstatus.queue;
	if(!(dpm.encoding & PE_MONO)) i >>= 1;
	if(dpm.encoding & PE_16BIT) i >>= 1;
	*((int *)arg) = i;
	return 0;*/

    //case PM_REQ_GETSAMPLES:
          *((int *)arg) = globals.samples*2;
        /*{
            static int c=0;
            if( c%0x100000==0 ){
                ctl->cmsg(CMSG_INFO, VERB_VERBOSE,
                        "getsamples = %d", *((int *)arg));
            
            }
        }*/

          return 0;

    case PM_REQ_DISCARD:
          AudioDeviceStop(globals.device, appIOProc);
	  init_variable();
          return 0;
	  
    case PM_REQ_PLAY_START:
	init_variable();
	return 0;
	
    case PM_REQ_FLUSH:
    case PM_REQ_OUTPUT_FINISH:
        while( globals.soundPlaying ){
	    trace_loop();
	    usleep(1000);
	}
	init_variable();
	return 0;
    
    default:
        break;
    }
    return -1;
}

