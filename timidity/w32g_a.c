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

    w32g_a.c - by Daisuke Aoki <dai@y7.net>

    Functions to play sound on the Windows audio driver (Windows 95/98/NT).
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <stdio.h>
#include <stdlib.h>
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include "timidity.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "w32g.h"

// #define USE_WAVEOUT_EVENT

#if defined(__CYGWIN32__) || defined(__MINGW32__)
#ifdef HAVE_NEW_MMSYSTEM
#include <mmsystem.h>
#else
/* On cygnus, there is not mmsystem.h for Multimedia API's.
 * mmsystem.h can not distribute becase of Microsoft Lisence
 * Then declare some of them here.
 */

#define WOM_OPEN		0x3BB
#define WOM_CLOSE		0x3BC
#define WOM_DONE		0x3BD
#define WAVE_FORMAT_QUERY	0x0001
#define WAVE_ALLOWSYNC		0x0002
#define WAVE_FORMAT_PCM		1
#define CALLBACK_FUNCTION	0x00030000l
#define WAVERR_BASE		32
#define WAVE_MAPPER		(UINT)-1
#define WHDR_DONE       0x00000001

DECLARE_HANDLE(HWAVEOUT);
DECLARE_HANDLE(HWAVE);
typedef HWAVEOUT *LPHWAVEOUT;

/* Define WAVEHDR, WAVEFORMAT structure */
typedef struct wavehdr_tag {
    LPSTR       lpData;
    DWORD       dwBufferLength;
    DWORD       dwBytesRecorded;
    DWORD       dwUser;
    DWORD       dwFlags;
    DWORD       dwLoops;
    struct wavehdr_tag *lpNext;
    DWORD       reserved;
} WAVEHDR;

typedef struct {
    WORD    wFormatTag;
    WORD    nChannels;
    DWORD   nSamplesPerSec;
    DWORD   nAvgBytesPerSec;
    WORD    nBlockAlign;
    WORD    wBitsPerSample;
    WORD    cbSize;
} WAVEFORMATEX;

typedef struct waveoutcaps_tag {
    WORD    wMid;
    WORD    wPid;
    UINT    vDriverVersion;
#define MAXPNAMELEN      32
    char    szPname[MAXPNAMELEN];
    DWORD   dwFormats;
    WORD    wChannels;
    DWORD   dwSupport;
} WAVEOUTCAPS;

typedef WAVEHDR *LPWAVEHDR;
typedef WAVEFORMATEX *LPWAVEFORMATEX;
typedef WAVEOUTCAPS *LPWAVEOUTCAPS;
typedef UINT MMRESULT;

MMRESULT WINAPI waveOutOpen(LPHWAVEOUT, UINT,
			    LPWAVEFORMATEX, DWORD, DWORD, DWORD);
MMRESULT WINAPI waveOutClose(HWAVEOUT);
MMRESULT WINAPI waveOutPrepareHeader(HWAVEOUT, LPWAVEHDR, UINT);
MMRESULT WINAPI waveOutUnprepareHeader(HWAVEOUT, LPWAVEHDR, UINT);
MMRESULT WINAPI waveOutWrite(HWAVEOUT, LPWAVEHDR, UINT);
UINT WINAPI waveOutGetNumDevs(void);

MMRESULT WINAPI waveOutReset(HWAVEOUT);
MMRESULT WINAPI waveOutGetDevCaps(UINT, LPWAVEOUTCAPS, UINT);
MMRESULT WINAPI waveOutGetDevCapsA(UINT, LPWAVEOUTCAPS, UINT);
#define waveOutGetDevCaps waveOutGetDevCapsA
MMRESULT WINAPI waveOutGetID(HWAVEOUT, UINT*);
#endif
#else
#include <process.h>
#endif /* __CYGWIN32__ */

#include "timidity.h"
#include "output.h"
#include "controls.h"
#include "timer.h"
#include "instrum.h"
#include "playmidi.h"
#include "mblock.h"
#include "miditrace.h"

// #define DATA_BLOCK_NUM  (dpm.extra_param[0])
#define DATA_BLOCK_NUM 16
#define DATA_MIN_NBLOCKS (DATA_BLOCK_NUM-1)
static int data_block_size;
static int data_block_time;	// msec
volatile int data_block_bits = DEFAULT_AUDIO_BUFFER_BITS;
volatile int data_block_num = 64;
volatile struct data_block_t
{
	HGLOBAL data_hg;
    HGLOBAL head_hg;
    void *data;
    WAVEHDR *head;
    int blockno;
	struct data_block_t *next;
	int size;
};

static int open_output(void); /* 0=success, 1=warning, -1=fatal error */
static void close_output(void);
static int waveOutWriteBlock(struct data_block_t *block);
static int output_data(char *buf, int32 nbytes);
static int acntl(int request, void *arg);

volatile static struct data_block_t *all_data_block;
volatile static struct data_block_t *free_data_block;
static void reuse_data_block(struct data_block_t *);
static void reset_data_block(void);
static struct data_block_t *new_data_block();
static struct data_block_t *cur_data_block();
static void used_cur_data_block(void);
static struct data_block_t *current_data_block = NULL;
static int *all_data_block_used_flag;

HANDLE hEventWaveOut = NULL;

volatile static HWAVEOUT dev;
volatile static int nBlocks;
static const char *mmerror_code_string(MMRESULT res);

/* export the playback mode */

#define dpm w32_play_mode

PlayMode dpm = {
    33075,
    PE_16BIT|PE_SIGNED,
    PF_PCM_STREAM|PF_CAN_TRACE|PF_BUFF_FRAGM_OPT,
    -1,
    {32},
    "Windows audio driver", 'd',
    NULL,
    open_output,
    close_output,
    output_data,
    acntl
};

extern CRITICAL_SECTION critSect;
/* Optional flag */
static int w32_wave_allowsync = 1; /* waveOutOpen() fdwOpen : WAVE_ALLOWSYNC */
// static int w32_wave_allowsync = 0;

// #define USE_WAVEFORMTHREAD
static volatile HANDLE hWaveformThread = 0;
static volatile DWORD dwWaveformThreadID = 0;
static void InitWaveformThread(void);
static void ExitWaveformThread(void);
#define WM_WFT_OUTPUT_DATA		(WM_USER + 1)
#define WM_WFT_OUTPUT_DONE		(WM_USER + 2)
#define WM_WFT_OPEN_OUTPUT		(WM_USER + 3)
#define WM_WFT_CLOSE_OUTPUT		(WM_USER + 4)
#define WM_WFT_ACNTL			(WM_USER + 5)

#include "w32g_res.h"

static void Wait(int discard_current_block)
{
	int i;
	int num = nBlocks + 1; /* Avoid infinite loop */

	if(current_data_block!=NULL){
		EnterCriticalSection (&critSect);
		if(discard_current_block)
			reuse_data_block(current_data_block);
		else if(current_data_block->size){
			waveOutWriteBlock(current_data_block);
		}
		LeaveCriticalSection (&critSect);
		current_data_block = NULL;
	}

	for(;;){
// PrintfDebugWnd("Wait(). nBlocks: %d\n",nBlocks);
   		if(!nBlocks)
			break;
#ifdef USE_WAVEOUT_EVENT
		if(hEventWaveOut!=NULL){
			SetEvent(hEventWaveOut);
		}
#endif
		Sleep(data_block_time);
		num--;
		if(num<0){
			/* Timeout.  Discard all blocks */
			EnterCriticalSection (&critSect);
			for(i = 0; i < data_block_num; i++)
				if(all_data_block_used_flag[i]==1)
					if(all_data_block[i].head->dwFlags==WHDR_DONE){
						waveOutUnprepareHeader(dev,all_data_block[i].head,sizeof(WAVEHDR));
						reuse_data_block(&all_data_block[i]);
						all_data_block_used_flag[i] = 0;
					}
			LeaveCriticalSection (&critSect);
			break;
		}
	}
}

static void CALLBACK wave_callback(HWAVE hWave, UINT uMsg,
				   DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
{
    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
	      "MMCallback: Msg=0x%x, nBlocks=%d", uMsg, nBlocks);

    if(uMsg == WOM_DONE)
    {
	WAVEHDR *wh;
// PrintfDebugWnd("MMCallback: Msg=0x%x, nBlocks=%d\n", uMsg, nBlocks);
	EnterCriticalSection (&critSect);
	wh = (WAVEHDR *)dwParam1;
	if(all_data_block_used_flag[wh->dwUser]==1){
		waveOutUnprepareHeader(dev, wh, sizeof(WAVEHDR));
		reuse_data_block(&all_data_block[wh->dwUser]);
		all_data_block_used_flag[wh->dwUser] = 0;
	}
	LeaveCriticalSection (&critSect);
#ifdef USE_WAVEOUT_EVENT
		if(hEventWaveOut!=NULL){
			if(nBlocks >= 4){
// PrintfDebugWnd("RESET\n");
				ResetEvent(hEventWaveOut);
			}
			if(nBlocks <= 3){
// PrintfDebugWnd("SET\n");
				SetEvent(hEventWaveOut);
			}
		}
#endif
	}
}

static int open_output(void)
{
    int i, j, mono, eight_bit, warnings = 0;
    WAVEFORMATEX wf;
    WAVEOUTCAPS caps;
    MMRESULT res;
    UINT devid;

    if(dpm.extra_param[0] < 8)
    {
	ctl->cmsg(CMSG_WARNING, VERB_NORMAL, "Too small -B option: %d",
		  dpm.extra_param[0]);
	dpm.extra_param[0] = 8;
    }

    /* Check if there is at least one audio device */
    if (!(i=waveOutGetNumDevs ()))
    {
	ctl->cmsg (CMSG_ERROR, VERB_NORMAL, "No audio devices present!");
	return -1;
    }

    /* They can't mean these */
    dpm.encoding &= ~(PE_ULAW|PE_ALAW|PE_BYTESWAP);

    if (dpm.encoding & PE_16BIT)
	dpm.encoding |= PE_SIGNED;
    else
	dpm.encoding &= ~PE_SIGNED;

    mono = (dpm.encoding & PE_MONO);
    eight_bit = !(dpm.encoding & PE_16BIT);

    memset(&wf, 0, sizeof(wf));
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = mono ? 1 : 2;
    wf.nSamplesPerSec = i = dpm.rate;
    j = 1;
    if (!mono)
    {
	i *= 2;
	j *= 2;
    }
    if (!eight_bit)
    {
	i *= 2;
	j *= 2;
    }
    wf.nAvgBytesPerSec = i;
    wf.nBlockAlign = j;
    wf.wBitsPerSample = eight_bit ? 8 : 16;
    wf.cbSize=sizeof(WAVEFORMATEX);

    dev = 0;
#ifdef USE_WAVEFORMTHREAD
	InitWaveformThread();
#endif
    if (w32_wave_allowsync)
	res = waveOutOpen (&dev, WAVE_MAPPER, &wf,
			   (DWORD)wave_callback, 0,
			   CALLBACK_FUNCTION | WAVE_ALLOWSYNC);
    else
	res = waveOutOpen (&dev, WAVE_MAPPER, &wf,
			   (DWORD)wave_callback, 0, CALLBACK_FUNCTION);
    if (res)
    {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		  "Can't open audio device: "
		  "encoding=<%s>, rate=<%d>, ch=<%d>: %s",
		  output_encoding_string(dpm.encoding),
		  dpm.rate,
		  wf.nChannels,
		  mmerror_code_string(res));
	return -1;
    }

    devid = 0;
    memset(&caps, 0, sizeof(WAVEOUTCAPS));
    waveOutGetID(dev, &devid);
    res = waveOutGetDevCaps(devid, &caps, sizeof(WAVEOUTCAPS));
    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
	      "Play Device ID: %d", devid);
    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
	      "Manufacture ID: %d");
    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
	      "Product ID: %d", caps.wPid);
    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
	      "Version of the driver: %d", caps.vDriverVersion);
    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
	      "Product name: %s", caps.szPname);
    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
	      "Formats supported: 0x%x", caps.dwFormats);
    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
	      "number of sources supported: %d", caps.wChannels);
    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
	      "functionality supported by driver: 0x%x", caps.dwSupport);

    /* Prepere audio queue buffer */
//    data_block_size = (int)((double)wf.nAvgBytesPerSec * data_block_time / 1000);
    data_block_size = audio_buffer_size;
    if(!(dpm.encoding & PE_MONO))
      data_block_size *= 2;
    if(dpm.encoding & PE_16BIT)
      data_block_size *= 2;
    data_block_time = data_block_size * 1000 / wf.nAvgBytesPerSec;

// PrintfDebugWnd("DATA_BLOCK_SIZE: %ld\n",data_block_size);
    all_data_block = (struct data_block_t *)
      safe_malloc(data_block_num * sizeof(struct data_block_t));
    all_data_block_used_flag = (int *)safe_malloc(data_block_num * sizeof(int));
    for(i = 0; i < data_block_num; i++)
    {
	struct data_block_t *block;
	block = &all_data_block[i];
	block->data_hg = GlobalAlloc(GMEM_MOVEABLE|GMEM_ZEROINIT|GMEM_SHARE, data_block_size);
	block->data = GlobalLock(block->data_hg);
	block->head_hg = GlobalAlloc(GMEM_MOVEABLE|GMEM_ZEROINIT|GMEM_SHARE, sizeof(WAVEHDR));
	block->head = GlobalLock(block->head_hg);
	memset(block->head,0,sizeof(WAVEHDR));
	block->size = 0;
	all_data_block_used_flag[i] = 0;
    }
    reset_data_block();
    dpm.fd = 0;
#ifdef USE_WAVEOUT_EVENT
	if(hEventWaveOut==NULL)
		hEventWaveOut = CreateEvent(NULL,TRUE,TRUE,NULL);
#endif
    return warnings;
}

static int waveOutWriteBlock(struct data_block_t *block)
{
    MMRESULT res;
    LPWAVEHDR wh;
// PrintfDebugWnd("waveOutWriteBlock Size=%d nBlocks=%d\n",block->size,nBlocks);
	wh = block->head;
	wh->dwBufferLength = (DWORD)block->size;
	wh->lpData = (void *)block->data;
	wh->dwUser = block->blockno;
	res = waveOutPrepareHeader(dev, wh, sizeof(WAVEHDR));
	if(res)
	{
	    ctl->cmsg (CMSG_ERROR, VERB_NORMAL, "waveOutPrepareHeader(): %s",
		       mmerror_code_string(res));
	    return -1;
	}
	res = waveOutWrite(dev, wh, sizeof(WAVEHDR));
	if(res)
	{
	    ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "waveOutWrite(): %s",
		      mmerror_code_string(res));
	    return -1;
	}
	all_data_block_used_flag[wh->dwUser] = 1;
	return 0;
}

static int output_data(char *buf, int32 nbytes)
{
	int res, size;
	int32 len = nbytes;
	char *pbuf = buf;
    struct data_block_t *block;
//	PrintfDebugWnd("output_data:Begin.\n");
	for(;;){
		EnterCriticalSection (&critSect);
		if((block = cur_data_block())==NULL){
			LeaveCriticalSection (&critSect);
// PrintfDebugWnd("output_data() -> No Data Block.\n");
			Sleep(data_block_time);
			continue;
		}
		if(block->size + len > data_block_size){
			size = data_block_size - block->size;
			CopyMemory((char *)block->data+block->size,pbuf,size);
			block->size += size;
			len -= size;
			pbuf = buf + size;
			res = waveOutWriteBlock(block);
			used_cur_data_block();
			LeaveCriticalSection (&critSect);
			if(res)
				return res;
			continue;
		}
		size = len;
		CopyMemory((char *)block->data+block->size,pbuf,size);
		block->size += size;
		LeaveCriticalSection (&critSect);
		break;
	}
//	PrintfDebugWnd("output_data:End.\n");
	return 0;
}

static void close_output(void)
{
    int i;

    if(dpm.fd == -1)
	return;
    Wait(0);
    waveOutClose(dev);

    for(i = 0; i < data_block_num; i++)
    {
	struct data_block_t *block;
	block = &all_data_block[i];
	GlobalUnlock(block->head_hg);
	GlobalFree(block->head_hg);
	GlobalUnlock(block->data_hg);
	GlobalFree(block->data_hg);
    }
    free(all_data_block);
	free(all_data_block_used_flag);

    dpm.fd = -1;

#ifdef USE_WAVEOUT_EVENT
	if(hEventWaveOut!=NULL){
		SetEvent(hEventWaveOut);
	}
	Sleep(200);
	if(hEventWaveOut!=NULL)
		CloseHandle(hEventWaveOut);
	hEventWaveOut = NULL;
#endif
#ifdef USE_WAVEFORMTHREAD
	ExitWaveformThread();
#endif
}

static int acntl(int request, void *arg)
{
    switch(request)
    {
          case PM_REQ_GETQSIZ:
                *(int *)arg = data_block_num * audio_buffer_size;
		return 0;
	  case PM_REQ_DISCARD:
	  case PM_REQ_FLUSH:
		if(!nBlocks && current_data_block == NULL)
			return 0;
		if(request == PM_REQ_DISCARD){
			waveOutReset(dev);
			Wait(1);
		} else
			Wait(0);
		EnterCriticalSection (&critSect);
		reset_data_block();
		LeaveCriticalSection (&critSect);
		return 0;
	  case PM_REQ_RATE:
		break;
	  case PM_REQ_GETFILLABLE:
		break;
	  case PM_REQ_GETFILLED:
		break;
	  case PM_REQ_GETSAMPLES:
		break;
	  case PM_REQ_OUTPUT_FINISH:
		if(current_data_block != NULL && current_data_block->size > 0)
		{
			EnterCriticalSection (&critSect);
			waveOutWriteBlock(current_data_block);
			LeaveCriticalSection (&critSect);
		}
		current_data_block = NULL;
		return 0;
    }
    return -1;
}


#define ARRAY_SIZE(a) sizeof(a) / sizeof(a[0])
static const char *mmsyserr_code_string[] =
{
    "no error",
    "unspecified error",
    "device ID out of range",
    "driver failed enable",
    "device already allocated",
    "device handle is invalid",
    "no device driver present",
    "memory allocation error",
    "function isn't supported",
    "error value out of range",
    "invalid flag passed",
    "invalid parameter passed",
    "handle being used",
};

static const char *waverr_code_sring[] =
{
    "unsupported wave format",
    "still something playing",
    "header not prepared",
    "device is synchronous",
};

static const char *mmerror_code_string(MMRESULT err_code)
{
    static char s[32];

    if(err_code >= WAVERR_BASE)
    {
	err_code -= WAVERR_BASE;
	if(err_code > ARRAY_SIZE(waverr_code_sring))
	{
	    sprintf(s, "WAVERR %d", err_code);
	    return s;
	}
	return waverr_code_sring[err_code];
    }
    if(err_code > ARRAY_SIZE(mmsyserr_code_string))
    {
	sprintf(s, "MMSYSERR %d", err_code);
	return s;
    }
    return mmsyserr_code_string[err_code];
}

static struct data_block_t *new_data_block()
{
    struct data_block_t *p;

	 p = NULL;
    EnterCriticalSection (&critSect);
    if(free_data_block != NULL)
    {
	p = free_data_block;
	free_data_block = free_data_block->next;
	nBlocks++;
	p->next = NULL;
	p->size = 0;
    }
	current_data_block = p;
    LeaveCriticalSection (&critSect);

    return p;
}

static struct data_block_t *cur_data_block()
{
	if(current_data_block!=NULL)
		return current_data_block;
	else
		return new_data_block();
}

static void used_cur_data_block(void)
{
	current_data_block = NULL;
}

static void reuse_data_block(struct data_block_t *block)
{
    block->next = free_data_block;
    free_data_block = block;
    nBlocks--;
}

static void reset_data_block(void)
{
    int i;

    all_data_block[0].blockno = 0;
    all_data_block[0].next = &all_data_block[1];
    for(i = 1; i < data_block_num - 1; i++)
    {
	all_data_block[i].blockno = i;
	all_data_block[i].next = &all_data_block[i + 1];
    all_data_block[i].size = 0;
    }
    all_data_block[i].blockno = i;
    all_data_block[i].next = NULL;
    free_data_block = &all_data_block[0];
    nBlocks = 0;
	current_data_block = 0;
}













// ==================================================================
// Waveform Thread 
// ==================================================================

static volatile int ExitWaveformThreadFlag = 0;
static void WaveformThread(void *arg)
{
	MSG msg;
	while(GetMessage(&msg,NULL,0,0) ){
		if(ExitWaveformThreadFlag)
      		break;
		switch(msg.message){
		default:
      		break;
      }
//		Sleep(0);
	}
	hWaveformThread = 0;
	crt_endthread();
}

static void InitWaveformThread(void)
{
	if(hWaveformThread)
		return;
	hWaveformThread = crt_beginthreadex(NULL,0,WaveformThread,0,0,&dwWaveformThreadID);
	Sleep(100);
}

static void ExitWaveformThread(void)
{
	ExitWaveformThreadFlag = 1;
	WaitForSingleObject(hWaveformThread,INFINITE);
	CloseHandle(hWaveformThread);
}
