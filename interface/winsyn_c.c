
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


    winsyn_c.c - Windows synthesizer interface
        Copyright (c) 2002  Keishi Suenaga <s_keishi@mutt.freemail.ne.jp>

    I referenced following sources.
        alsaseq_c.c - ALSA sequencer server interface
            Copyright (c) 2000  Takashi Iwai <tiwai@suse.de>
        readmidi.c


    DESCRIPTION
    ===========

    This interface provides a Windows MIDI device interface which receives
    events and plays it in real-time.  On this mode, TiMidity works
    purely as software (real-time) MIDI render.

    For invoking Windows synthesizer interface, run timidity as folows:
      % timidity -iW    (interactively select an Input MIDI device)
    or
      % timidity -iW 2  (connect to MIDI device No. 2)

    TiMidity loads instruments dynamically at each time a PRM_CHANGE
    event is received.  It sometimes causes a noise.
    If you are using a low power machine, invoke timidity as follows:
      % timidity -s 11025 -iW        (set sampling freq. to 11025Hz)
    or
      % timidity -EFreverb=0 -iW    (disable MIDI reverb effect control)

    TiMidity keeps all loaded instruments during executing.

    To use TiMidity as output device, you need a MIDI loopback device.
    I use MIDI Yoke. It can freely be obtained MIDI-OX site
    (http://www.midiox.com).
*/
//#define  USE_PORTMIDI 1
//#undef USE_CURSES 1

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include "interface.h"

#include <stdio.h>

#if defined(__MINGW32__) && defined(USE_PDCURSES)
#define _NO_OLDNAMES 1	/* avoid type mismatch of beep() */
#ifndef sleep
extern void sleep(unsigned long);
#endif /* sleep */
#include <stdlib.h>
#undef _NO_OLDNAMES
#else /* USE_PDCURSES */
#include <stdlib.h>
#endif

#include <stdarg.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#endif
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <math.h>
#include <signal.h>

#include "server_defs.h"

#ifdef __W32__
#include <windows.h>
#include <mmsystem.h>
#endif
 
#ifdef USE_CURSES

#ifdef __W32__
#undef MOUSE_MOVED
#endif
#ifdef HAVE_NCURSES_H
#	include <ncurses.h>
#elif defined(HAVE_NCURSES_CURSES_H)
#	include <ncurses/curses.h>
#else
#	include <curses.h>
#endif

#ifdef __W32__
#undef MOUSE_MOVED
#define MOUSE_MOVED  1                 //see windows.h
#endif


#endif

#ifdef USE_PORTMIDI
#include <portmidi.h>
#include <porttime.h>
#endif

#include "timidity.h"
#include "common.h"
#include "controls.h"
#include "instrum.h"
#include "playmidi.h"
#include "readmidi.h"
#include "recache.h"
#include "output.h"
#include "aq.h"
#include "timer.h"

#ifndef __W32__
#include <stdio.h>
#include <termios.h>
#include <term.h>
#include <unistd.h>
#endif

extern char *opt_aq_max_buff,*opt_aq_fill_buff;
extern  int midi_port_number;
extern int32 current_sample;
extern FLOAT_T midi_time_ratio;

#ifndef __W32__
#define UINT unsigned int
static struct termios initial_settings, new_settings;
static int peek_character = -1;
#endif

extern int volatile stream_max_compute;	// play_event() ÇÃ compute_data() Ç≈åvéZÇãñÇ∑ç≈ëÂéûä‘
int playdone;
int seq_playing;
int seq_quit;

int system_mode=DEFAULT_SYSTEM_MODE;

#define MAX_PORT 4
int portnumber=1;

#define MAX_EXBUF 20
#define BUFF_SIZE 512


#ifndef USE_PORTMIDI
UINT InNum,portID[MAX_PORT];
HMIDIIN  hMidiIn[MAX_PORT];
HMIDIOUT hMidiOut[MAX_PORT];
MIDIHDR *IMidiHdr[MAX_PORT][MAX_EXBUF];

char sIMidiHdr[MAX_PORT][MAX_EXBUF][sizeof(MIDIHDR)];
char sImidiHdr_data[MAX_PORT][MAX_EXBUF][BUFF_SIZE];

struct evbuf_t{
	int status;
	UINT wMsg;
	DWORD	dwInstance;
	DWORD	dwParam1;
	DWORD	dwParam2;
};
#define EVBUFF_SIZE 512
struct evbuf_t evbuf[EVBUFF_SIZE];
UINT  evbwpoint=0;
UINT  evbrpoint=0;
UINT evbsysexpoint;
UINT  mvbuse=0;
enum{B_OK, B_WORK, B_END};
#else
PmError pmerr;
static UINT InNum;
struct midistream_t{
	PortMidiStream* stream;
};
static struct midistream_t  midistream[MAX_PORT];
static PmDeviceID portID[MAX_PORT];
#define PMBUFF_SIZE 8192
#define EXBUFF_SIZE 512
static PmEvent pmbuffer[PMBUFF_SIZE];
static char    sysexbuffer[EXBUFF_SIZE];
#endif


double starttime;
static int time_advance;
extern double play_start_time;
double lastdintime;

//acitive sensing
static int active_sensing_flag=0;
static double active_sensing_time=0;


#define EX_RESET_NO 7
static char sysex_resets[EX_RESET_NO][11]={
		'\xf0','\x7e','\x7f','\x09','\x00','\xf7','\x00','\x00','\x00','\x00','\x00',
		'\xf0','\x7e','\x7f','\x09','\x01','\xf7','\x00','\x00','\x00','\x00','\x00',
		'\xf0','\x7e','\x7f','\x09','\x03','\xf7','\x00','\x00','\x00','\x00','\x00',
		'\xf0','\x41','\x10','\x42','\x12','\x40','\x00','\x7f','\x00','\x41','\xf7',
		'\xf0','\x41','\x10','\x42','\x12','\x00','\x00','\x7f','\x00','\x01','\xf7',
		'\xf0','\x41','\x10','\x42','\x12','\x00','\x00','\x7f','\x01','\x00','\xf7',
		'\xf0','\x43','\x10','\x4c','\x00','\x00','\x7E','\x00','\xf7','\x00','\x00' };
/*
#define EX_RESET_NO 9
static char sysex_resets[EX_RESET_NO][11]={
	'\xf0','\x7e','\x7f','\x09','\x00','\xf7','\x00','\x00','\x00','\x00','\x00', //gm off
	'\xf0','\x7e','\x7f','\x09','\x01','\xf7','\x00','\x00','\x00','\x00','\x00', //gm1
	'\xf0','\x7e','\x7f','\x09','\x02','\xf7','\x00','\x00','\x00','\x00','\x00', //gm off
	'\xf0','\x7e','\x7f','\x09','\x03','\xf7','\x00','\x00','\x00','\x00','\x00', //gm2
	'\xf0','\x41','\x10','\x42','\x12','\x40','\x00','\x7f','\x00','\x41','\xf7', //GS
	'\xf0','\x41','\x10','\x42','\x12','\x40','\x00','\x7f','\x7f','\x41','\xf7', //GS off
	'\xf0','\x41','\x10','\x42','\x12','\x00','\x00','\x7f','\x00','\x01','\xf7', //88
	'\xf0','\x41','\x10','\x42','\x12','\x00','\x00','\x7f','\x01','\x00','\xf7', //88
	'\xf0','\x43','\x10','\x4c','\x00','\x00','\x7E','\x00','\xf7','\x00','\x00'  //XG on
	};
*/
#define TICKTIME_HZ 50

static int ctl_open(int using_stdin, int using_stdout);
static void ctl_close(void);
static int ctl_read(int32 *valp);
static int cmsg(int type, int verbosity_level, char *fmt, ...);
static void ctl_event(CtlEvent *e);
static void ctl_pass_playing_list(int n, char *args[]);
#ifndef __W32__
static void init_keybord(void);
static void close_keybord(void);
static int kbhit(void);
static char readch(void);
#endif

/**********************************/
/* export the interface functions */

#define ctl winsyn_control_mode

ControlMode ctl=
{
    "Windows Synthesizer interface", 'W',
    1,0,0,
    0,
    ctl_open,
    ctl_close,
    ctl_pass_playing_list,
    ctl_read,
    cmsg,
    ctl_event
};

static int32 event_time_offset;
static FILE *outfp;

/*ARGSUSED*/

static int ctl_open(int using_stdin, int using_stdout)
{
	ctl.opened = 1;
	ctl.flags &= ~(CTLF_LIST_RANDOM|CTLF_LIST_SORT);
	if (using_stdout)
		outfp = stderr;
	else
		outfp = stdout;
	return 0;
}

static void ctl_close(void)
{
  fflush(outfp);
  ctl.opened=0;
}

static int ctl_read(int32 *valp)
{
    return RC_NONE;
}

#ifdef IA_W32G_SYN
extern void PutsConsoleWnd(char *str);
extern int ConsoleWndFlag;
#endif
static int cmsg(int type, int verbosity_level, char *fmt, ...)
{
#ifndef IA_W32G_SYN

	va_list ap;

  if ((type==CMSG_TEXT || type==CMSG_INFO || type==CMSG_WARNING) &&
      ctl.verbosity<verbosity_level)
    return 0;
  va_start(ap, fmt);
  if(type == CMSG_WARNING || type == CMSG_ERROR || type == CMSG_FATAL)
      dumb_error_count++;
  if (!ctl.opened)
    {
      vfprintf(stderr, fmt, ap);
      fputs(NLS, stderr);
    }
  else
    {
      vfprintf(outfp, fmt, ap);
      fputs(NLS, outfp);
      fflush(outfp);
    }
  va_end(ap);

#else
	if ( !ConsoleWndFlag ) return 0;
	{
    char buffer[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);

    if((type==CMSG_TEXT || type==CMSG_INFO || type==CMSG_WARNING) &&
       ctl.verbosity<verbosity_level) 
	return 0;
//    if(type == CMSG_FATAL)
//	w32g_msg_box(buffer, "TiMidity Error", MB_OK);
    PutsConsoleWnd(buffer);
    PutsConsoleWnd("\n");
    return 0;
	}
#endif

    return 0;
}

static void ctl_event(CtlEvent *e)
{
}

static void seq_reset(void);
void seq_play_event(MidiEvent *);
void seq_set_time(MidiEvent *);
static void stop_playing(void);
static void doit(void);
void server_reset(void);
void play_some_data (void);
void winplaymidi(void);
#ifndef USE_PORTMIDI
void CALLBACK MidiInProc(HMIDIIN,UINT,DWORD,DWORD,DWORD);
#endif

#ifdef IA_W32G_SYN
extern void w32g_syn_doit(void);
extern void w32g_syn_ctl_pass_playing_list(int n_, char *args_[]);
static void ctl_pass_playing_list(int n, char *args[])
{
	w32g_syn_ctl_pass_playing_list ( n, args );
}
#endif

#ifndef IA_W32G_SYN
static void ctl_pass_playing_list(int n, char *args[])
#else
// 0: OK, 2: Require to reset.
int ctl_pass_playing_list2(int n, char *args[])
#endif
{
	int i, j;
	MidiEvent ev;
	UINT port=0 ;
#ifdef USE_PORTMIDI
	PmDeviceInfo *deviceinfo;
#endif
#ifdef USE_CURSES
	int wlaw;
#endif

#ifdef USE_CURSES
	initscr();
	cbreak();
	noecho();
#endif


#ifdef USE_PORTMIDI
	pmerr=Pm_Initialize();
	if( pmerr != pmNoError ) goto pmerror;
#endif
#ifndef IA_W32G_SYN
	if(n > MAX_PORT ){
#ifndef USE_CURSES
		printf( "Usage: timidity -iW [Midi interface No s]\n");
#else 
		move(0,0);
		printw( "Usage: timidity -iW [Midi interface No s]");
		refresh();
#endif
		return;
	}
#endif

	if(n>0){
		port=0;
		while(port<n && n!=0){
			if( (portID[port] = atoi(args[port]))==0 ){
				n=0;
			}else{
#ifdef USE_PORTMIDI
				deviceinfo=(PmDeviceInfo *)Pm_GetDeviceInfo(portID[port]-1);
				if(TRUE==(deviceinfo)->input) {
#else
				if(MMSYSERR_NOERROR==midiInOpen(&hMidiIn[port],portID[port]-1,(DWORD)NULL,(DWORD)0L,CALLBACK_NULL)){
					midiInReset(hMidiIn[port]);
					midiInClose(hMidiIn[port]);
#endif
				}else{
					n=0;
#ifdef IA_W32G_SYN
					{
						char buff[1024];
						sprintf ( buff, "MIDI IN Device ID %d is not available. So set a proper ID for the MIDI port %d and restart.", portID[port], port );
						MessageBox ( NULL, buff, "Error", MB_OK );
						return 2;
					}
#endif
				}
			}
		port++;
		}
		portnumber=port;
	}
#ifndef IA_W32G_SYN
	if(n==0){
		char cbuf[80];
#ifndef USE_PORTMIDI
		MIDIINCAPS InCaps;
#endif
#ifndef USE_CURSES
		printf("Whow many ports do you use?(max %d)\n",MAX_PORT);
		do{
			if (0==scanf("%u",&portnumber)) scanf("%s",cbuf);
		}while(portnumber == 0 ||portnumber > MAX_PORT);
		printf("\n");
		printf("Opening Device drivers:");
		printf("Available Midi Input devices:\n");
#else 
		move(0,0);
		printw("Whow many ports do you use?(max %d)\n",MAX_PORT);
		refresh();
		do{
#ifdef USE_PDCURSES
			getstr(cbuf);
#else
			getnstr(cbuf,80);
#endif
			sscanf(cbuf,"%u",&portnumber);
		}while(portnumber == 0 ||portnumber > MAX_PORT);
		clear();
		refresh();
		wlaw=0;
		move(wlaw++,0);
		printw("Opening Device drivers:");
		printw("Available Midi Input devices:");
#endif
#if USE_PORTMIDI
		InNum = Pm_CountDevices();
		for (portID[port]=1;portID[port] <=InNum;portID[port]++){
			deviceinfo=(PmDeviceInfo *)Pm_GetDeviceInfo(portID[port]-1);
#ifndef USE_CURSES
			if(TRUE==deviceinfo->input){
				printf("%d:%s\n",portID[port],deviceinfo->name);
			}
#else
			if(TRUE==deviceinfo->input){
				move (wlaw++,0);
				printw("%d:%s",portID[port],deviceinfo->name);
			}
#endif			
		}

#else
		InNum = midiInGetNumDevs();
		for (portID[port]=1;portID[port] <=InNum;portID[port]++){
			midiInGetDevCaps(portID[port]-1,(LPMIDIINCAPSA) &InCaps,sizeof(InCaps));
#ifndef USE_CURSES
			printf("%d:%s\n",portID[port],(LPSTR)InCaps.szPname);
#else
			move (wlaw++,0);
			printw("%d:%s",portID[port],(LPSTR)InCaps.szPname);
#endif			
		}
#endif

#ifndef USE_CURSES
		for(port=0;port<portnumber;port++){
			printf("Keyin Input Device Number of port%d\n",port+1);
			do{
				if (0==scanf("%u",&portID[port])) scanf("%s",cbuf);
			}while(portID[port] == 0 ||(portID[port] > InNum));
			printf("\n");			
		}
#else
		for(port=0;port<portnumber;port++){
			move(wlaw++,0);
			printw("Keyin Input Device Number of port%d",port+1);
			refresh();
			do{
#ifdef USE_PDCURSES
				getstr(cbuf);
#else
				getnstr(cbuf,80);
#endif
				sscanf(cbuf,"%u",&portID[port]); 
			}while(portID[port] == 0 ||(portID[port] > InNum));
			printf("\n");			
		}
		clear();
		refresh();
#endif		
	}
#endif

	for(port=0;port<portnumber;port++){
		portID[port]=portID[port]-1;
	}

#ifndef IA_W32G_SYN
#ifndef USE_CURSES
	printf("TiMidity starting in Windows Synthesizer mode\n");
	printf("Usage: timidity -iW [Midi interface No]\n");
	printf("\n");
//	printf("Please wait for piano 'C' sound\n");
	printf("\n");
	printf("N (Normal mode) M(GM mode) S(GS mode) X(XG mode) \n");
	printf("(Only in Normal mode, Mode can be changed by MIDI data)\n");
	printf("m(GM reset) s(GS reset) x(XG reset)\n");
	printf("\n");
	printf("Press 'q' key to stop\n");
#else
	move(0,0);
	printw("TiMidity starting in Windows Synthesizer mode\n");
	printw("Usage: timidity -iW [Midi interface No]\n");
	printw("\n");
//	printw("Please wait for piano 'C' sound\n");
	printw("\n");
	printw("N (Normal mode) M(GM mode) S(GS mode) X(XG mode) \n");
	printw("(Only in Normal mode, Mode can be changed by MIDI data)\n");
	printw("m(GM reset) s(GS reset) x(XG reset)\n");
	printw("\n");
	printw("Press 'q' key to stop\n");
	refresh();
#endif
#endif
/*
	for(port=0;port<portnumber;port++){		//trick for MIDI Yoke
		midiInOpen(&hMidiIn[port],portID[port],(DWORD)NULL,(DWORD)0L,CALLBACK_NULL);
		midiInStart(hMidiIn[port]);
		midiOutOpen(&hMidiOut[port],portID[port],(DWORD)NULL,(DWORD)0L,CALLBACK_NULL);
		midiOutShortMsg(hMidiOut[port], 0x007f3c90);
		midiOutShortMsg(hMidiOut[port], 0x00003c80);
		midiOutReset(hMidiOut[port]);
		midiOutClose(hMidiOut[port]);
		midiInReset(hMidiIn[port]);
		midiInClose(hMidiIn[port]);
	}
*/
	opt_realtime_playing = 2; /* Enable loading patch while playing */
	allocate_cache_size = 0; /* Don't use pre-calclated samples */
	auto_reduce_polyphony = 0;
	current_keysig = current_temper_keysig = opt_init_keysig;
	note_key_offset = 0;
	time_advance=play_mode->rate/TICKTIME_HZ*2;
	if (!(play_mode->encoding & PE_MONO))
		time_advance >>= 1;
	if (play_mode->encoding & PE_16BIT)
		time_advance >>= 1;

	if (opt_force_keysig != 8) {
		i = current_keysig + ((current_keysig < 8) ? 7 : -6);
		j = opt_force_keysig + ((current_keysig < 8) ? 7 : 10);
		while (i != j && i != j + 12) {
			if (++note_key_offset > 6)
				note_key_offset -= 12;
			i += (i > 10) ? -5 : 7;
		}
	}
	i = current_keysig + ((current_keysig < 8) ? 7 : -9), j = 0;
	while (i != 7 && i != 19)
		i += (i < 7) ? 5 : -7, j++;
	j += note_key_offset, j -= floor(j / 12.0) * 12;
	current_freq_table = j;

	seq_reset();

/*
	for(port=0;port<portnumber;port++){
		for (i=0;i<MAX_EXBUF;i++){
			IMidiHdr[port][i] = (MIDIHDR*)safe_malloc(sizeof(MIDIHDR));
			memset(IMidiHdr[port][i],0,sizeof(MIDIHDR));
			IMidiHdr[port][i]->lpData = (char*)safe_malloc(BUFF_SIZE);
			memset((IMidiHdr[port][i]->lpData),0,BUFF_SIZE);
			IMidiHdr[port][i]->dwBufferLength = BUFF_SIZE;
		}
	}
*/
#ifndef USE_PORTMIDI
	for(port=0;port<portnumber;port++){
		for (i=0;i<MAX_EXBUF;i++){
			IMidiHdr[port][i] = sIMidiHdr[port][i];
			memset(IMidiHdr[port][i],0,sizeof(MIDIHDR));
			IMidiHdr[port][i]->lpData = sImidiHdr_data[port][i];
			memset((IMidiHdr[port][i]->lpData),0,BUFF_SIZE);
			IMidiHdr[port][i]->dwBufferLength = BUFF_SIZE;
		}
	}
	evbuf[0].status=B_END;
	evbwpoint=0;
	evbrpoint=0;
	mvbuse=0;
#endif

	seq_quit=0;
		
#ifdef USE_PORTMIDI
	for(port=0;port<portnumber;port++){
		PortMidiStream* stream;
		void* timeinfo;
		pmerr=Pm_OpenInput( &stream,
             	portID[port],
                NULL,
				(PMBUFF_SIZE),
                NULL,
                Pt_Time,
                NULL);
		midistream[port].stream=stream;
		if( pmerr != pmNoError ) goto pmerror;
			pmerr=Pm_SetFilter(midistream[port].stream,PM_FILT_CLOCK);
		if( pmerr != pmNoError ) goto pmerror;
	}
#else
	for(port=0;port<portnumber;port++){
		midiInOpen(&hMidiIn[port],portID[port],(DWORD)MidiInProc,(DWORD)port,CALLBACK_FUNCTION);
		for (i=0;i<MAX_EXBUF;i++){
			midiInPrepareHeader(hMidiIn[port],IMidiHdr[port][i],sizeof(MIDIHDR));
			midiInAddBuffer(hMidiIn[port],IMidiHdr[port][i],sizeof(MIDIHDR));
		}
	}
		for(port=0;port<portnumber;port++){
	if(MMSYSERR_NOERROR !=midiInStart(hMidiIn[port]))
		printf("midiInStarterror\n");
	}
#endif
//	seq_reset();

	system_mode=DEFAULT_SYSTEM_MODE;
	change_system_mode(system_mode);

	ev.type=ME_RESET;
	ev.a=GS_SYSTEM_MODE; //GM is mor better ???
	seq_play_event(&ev);

/*
	ev.channel=0x00;
	ev.a=0x3c;
	ev.b=0x7f;
	ev.type = ME_NOTEON;
	seq_play_event(&ev);
	ev.type = ME_NOTEOFF;
	seq_play_event(&ev);

	for(i=0;i<0.4*TICKTIME_HZ;i++){
			ev.type = ME_NONE;
			seq_set_time(&ev);
			play_event(&ev);
			aq_fill_nonblocking();

	}
*/
	while(seq_quit==0) {

#ifndef IA_W32G_SYN
		doit();
#else
		w32g_syn_doit();
#endif
	}
	for(port=0;port<portnumber;port++){
#ifdef USE_PORTMIDI
		pmerr=Pm_Abort(midistream[port].stream);
//		if( pmerr != pmNoError ) goto pmerror;
#else
		midiInStop(hMidiIn[port]);
#endif
	}
	stop_playing();
//	play_mode->close_output();
#ifdef USE_PORTMIDI
	Pm_Terminate();
#else
	for(port=0;port<portnumber;port++){
		midiInReset(hMidiIn[port]);
		for (i=0;i<MAX_EXBUF;i++){
			midiInUnprepareHeader(hMidiIn[port],IMidiHdr[port][i],sizeof(MIDIHDR));
		}
		midiInClose(hMidiIn[port]);
/*
		for (i=0;i<MAX_EXBUF;i++){
			free(IMidiHdr[port][i]->lpData);
			free(IMidiHdr[port][i]);
		}
*/
	}
#endif

#ifdef USE_CURSES
		clear();
		refresh();
		endwin();
#endif
#ifdef IA_W32G_SYN
	return 0;
#endif
	return;
#ifdef USE_PORTMIDI
pmerror:
		Pm_Terminate();
	ctl.cmsg(  CMSG_ERROR, VERB_NORMAL, "PortMIDI error: %s\n", Pm_GetErrorText( pmerr ) );
	return ;
#endif
}

#ifndef __W32__
static void init_keybord(void){
	tcgetattr(0,&initial_settings);
	tcgetattr(0,&new_settings);
	new_settings.c_lflag &= ~ICANON;
	new_settings.c_lflag &= ~ECHO;
	new_settings.c_lflag &= ~ISIG;
	new_settings.c_cc[VMIN] = 1;
	new_settings.c_cc[VTIME] = 0;
	tcsetattr(0, TCSANOW, &new_settings);
}

static void close_keybord(void){
	tcsetattr(0, TCSANOW, &initial_settings);
}

static int kbhit(void){
	char ch;
	int nread;
	
	if(peek_character != -1)
		return 1;
	new_settings.c_cc[VMIN]=0;
	tcsetattr(0,TCSANOW, &new_settings);
	nread = read(0, &ch, 1);
	new_settings.c_cc[VMIN]=1;
	tcsetattr(0,TCSANOW, &new_settings);
	
	if(nread == 1) {
		peek_character = ch;
		return 1;
	}
	return 0;
}


static char readch(void){
	char ch;
	if(peek_character != -1){
		ch = peek_character;
		peek_character = -1;
		return ch;
	}
	read(0,&ch,1);
	return ch;
}
#endif		

static void seq_reset(void){
		stop_playing();
		free_instruments(0);        //also in server_reset
		free_global_mblock();
		server_reset();
//		printf("system reseted\n");
}

void seq_play_event(MidiEvent *ev)
{
  int gch;
  int32 cet;
	gch = GLOBAL_CHANNEL_EVENT_TYPE(ev->type);
	if(gch || !IS_SET_CHANNELMASK(quietchannels, ev->channel) ){
    if ( !seq_quit ) {
			ev->time=0;
			play_event(ev);
		}
	}
}

void seq_set_time(MidiEvent *ev)
{
	double past_time,btime;
	event_time_offset = (int32)time_advance;
	past_time = get_current_calender_time() - starttime;
	ev->time = (int32)((past_time) * play_mode->rate);
	ev->time += (int32)event_time_offset;

#if 0
	btime = (double)((ev->time-current_sample/midi_time_ratio)/play_mode->rate);
	btime *= 1.01; /* to be sure */
	aq_set_soft_queue(btime, 0.0);
#endif
}

static void stop_playing(void)
{
	if(upper_voices) {
		MidiEvent ev;
		ev.type = ME_EOT;
		ev.a = 0;
		ev.b = 0;
		seq_play_event(&ev);
		aq_flush(1);
	}
}

#ifndef IA_W32G_SYN
static void doit(void)
{
	MidiEvent ev;
#ifndef __W32__
		init_keybord();
#endif

	playdone=0;
	while(seq_quit==0){
#ifdef __W32__
		if(kbhit()){
			switch(getch()){
#else		
		if(kbhit()){
			switch(readch()){
#endif
				case 'Q':
				case 'q':
					seq_quit=~0;
				break;

				case 'm':
					server_reset();
					ev.type=ME_RESET;
					ev.a=GM_SYSTEM_MODE;
					seq_play_event(&ev);
				break;

				case 's':
					server_reset();
					ev.type=ME_RESET;
					ev.a=GS_SYSTEM_MODE;
					seq_play_event(&ev);
				break;

				case 'x':
					server_reset();
					ev.type=ME_RESET;
					ev.a=XG_SYSTEM_MODE;
					ev.time=0;
					seq_play_event(&ev);
				break;

				case 'c':
					server_reset();
					ev.type=ME_RESET;
					ev.a=system_mode;
					seq_play_event(&ev);
				break;

				case 'M':
					server_reset();
					system_mode=GM_SYSTEM_MODE;
					ev.type=ME_RESET;
					ev.a=GM_SYSTEM_MODE;
					seq_play_event(&ev);
					change_system_mode(system_mode);
				break;

				case 'S':
					server_reset();
					system_mode=GS_SYSTEM_MODE;
					ev.type=ME_RESET;
					ev.a=GS_SYSTEM_MODE;
					seq_play_event(&ev);
					change_system_mode(system_mode);
				break;

				case 'X':
					server_reset();
					system_mode=XG_SYSTEM_MODE;
					ev.type=ME_RESET;
					ev.a=XG_SYSTEM_MODE;
					seq_play_event(&ev);
					change_system_mode(system_mode);
				break;

				case 'N':
					server_reset();
					system_mode=DEFAULT_SYSTEM_MODE;
					ev.type=ME_RESET;
					ev.a=GS_SYSTEM_MODE;
					seq_play_event(&ev);
					change_system_mode(system_mode);
				break;

			}
		}
		winplaymidi();
		sleep(0);
	}
#ifndef __W32__
	close_keybord();
#endif
}
#endif

void server_reset(void)
{
	play_mode->close_output();	// PM_REQ_PLAY_START wlll called in playmidi_stream_init()
	play_mode->open_output();	// but w32_a.c does not have it.
	readmidi_read_init();
	playmidi_stream_init();
	starttime=get_current_calender_time();
	reduce_voice_threshold = 0; // * Disable auto reduction voice *
	auto_reduce_polyphony = 0;
	event_time_offset = 0;
}

#ifdef IA_W32G_SYN
static int winplaymidi_sleep_level = 2;
static DWORD winplaymidi_active_start_time = 0;
#endif

void winplaymidi(void){
	MidiEvent ev;

#ifdef IA_W32G_SYN
	if ( winplaymidi_sleep_level < 1 ) {
		winplaymidi_sleep_level = 1;
	}
#endif
#if defined(IA_W32G_SYN) && !defined(USE_PORTMIDI)
	if( (evbuf[evbrpoint].status==B_OK)&&(evbrpoint!=evbwpoint) ){ //playdata exist
		winplaymidi_sleep_level = 0;
	}
#endif
	play_some_data();
#ifdef IA_W32G_SYN
	if ( winplaymidi_sleep_level == 1 ) {
		DWORD ct = GetCurrentTime ();
		if ( winplaymidi_active_start_time == 0 || ct < winplaymidi_active_start_time ) {
			winplaymidi_active_start_time = ct;
		} else if ( ct - winplaymidi_active_start_time > 2000 ) {
			winplaymidi_sleep_level = 2;
		}
	} else if ( winplaymidi_sleep_level == 0 ) {
		winplaymidi_active_start_time = 0;
	}
#endif
	ev.type = ME_NONE;
	seq_set_time(&ev);
	play_event(&ev);
	aq_fill_nonblocking();

	if(active_sensing_flag==~0 && (get_current_calender_time() > active_sensing_time+0.5)){
//normaly acitive sensing expiering time is 330ms(>300ms) but this loop is heavy
		play_mode->close_output();
		play_mode->open_output();
		printf ("Active Sensing Expired\n");
		active_sensing_flag=0;
	}
#ifdef IA_W32G_SYN
	if ( winplaymidi_sleep_level >= 2) {
		Sleep ( 100 );
	} else if ( winplaymidi_sleep_level > 0 ) {
		Sleep ( 1 );
	}
#endif

}

#ifdef USE_PORTMIDI
void play_some_data (void){
	PmMessage pmmsg;
	MidiEvent ev;
	MidiEvent evm[16];
	int i,j,ii,port,exlen,data,shift,chk,ne;
	long pmlength;
	for(port=0;port<portnumber;port++){
		pmerr=Pm_Read(midistream[port].stream, pmbuffer, PMBUFF_SIZE);
		if(pmerr==pmBufferOverflow){
			ctl.cmsg(  CMSG_ERROR, VERB_NORMAL, "PortMIDI buffer overflow: %s\n", Pm_GetErrorText( pmerr ) );
		}else{
			if(pmerr<0) goto pmerror;
		}
		pmlength=pmerr;
		i=0;
		while(i<pmlength){
			pmmsg=pmbuffer[i].message;
			i++;
			ev.type = ME_NONE;
			ev.channel = Pm_MessageStatus(pmmsg) & 0x0000000f;
			ev.channel = ev.channel+port*16;
			ev.a = Pm_MessageData1(pmmsg);
			ev.b = Pm_MessageData2(pmmsg);
			switch ((int) (Pm_MessageStatus(pmmsg) & 0x000000f0)) {
			case 0x80:
				ev.type = ME_NOTEOFF;
//				seq_play_event(&ev);
				break;
			case 0x90:
				ev.type = (ev.b) ? ME_NOTEON : ME_NOTEOFF;
//				seq_play_event(&ev);
				break;
			case 0xa0:
				ev.type = ME_KEYPRESSURE;
//				seq_play_event(&ev);
				break;
			case 0xb0:
				if (! convert_midi_control_change(ev.channel, ev.a, ev.b, &ev))
					ev.type = ME_NONE;
				break;
			case 0xc0:
				ev.type = ME_PROGRAM;
//				seq_play_event(&ev);
				break;
			case 0xd0:
				ev.type = ME_CHANNEL_PRESSURE;
//				seq_play_event(&ev);
				break;
			case 0xe0:
				ev.type = ME_PITCHWHEEL;
//				seq_play_event(&ev);
				break;
			case 0x0f0:
				if ( (Pm_MessageStatus(pmmsg) & 0x000000ff) == 0xf0) {
					// SysEx
					j=0;
					sysexbuffer[j++] = 0xf0;
					sysexbuffer[j++]=Pm_MessageData1(pmmsg);
					sysexbuffer[j++]=Pm_MessageData2(pmmsg);
					sysexbuffer[j++]=(pmmsg>> 24) & 0x0FF;
					if(i>=pmlength){
						{
							pmerr=Pm_Read(midistream[port].stream, pmbuffer, 1);
							if(pmerr<0){ printf("guha1\n");goto pmerror; }
							sleep(0);
						}while(pmerr==8);
						pmlength=pmerr;
						i=0;
					}
					data=0;
					while(j<EXBUFF_SIZE-4){
						for (shift = 0; shift < 32 && (data != 0x0f7); shift += 8) {
                			data= (pmbuffer[i].message >> shift) & 0x0FF;
							sysexbuffer[j++]=data;
						}
						if(data==0x0f7) break;
						i++;
						if( i>=pmlength ){
							{
								pmerr=Pm_Read(midistream[port].stream, pmbuffer, 1);
								if(pmerr<0){ printf("guha2\n"); goto pmerror;}
								sleep(0);
							}while(pmerr==0);
							pmlength=pmerr;
							i=0;
						}
					}

					exlen=j;
					for(ii=0;ii<EX_RESET_NO;ii++){
						chk=0;
						for(j=0;(j<exlen)&&(j<11);j++){
							if(chk==0 && sysex_resets[ii][j]!=sysexbuffer[j]){
								chk=~0;
							}
						}
						if(chk==0){
							 server_reset();
						}
					}
/*
					printf("SyeEx length=%x bytes \n", exlen);
					for(ii=0;ii<exlen;ii++){
						printf("%x ",sysexbuffer[ii]);
					}
					printf("\n");
*/
					if(parse_sysex_event(sysexbuffer+1,exlen-1,&ev)){
						if(ev.type==ME_RESET && system_mode!=DEFAULT_SYSTEM_MODE)
						ev.a=system_mode;
						change_system_mode(system_mode);
						seq_play_event(&ev);
					}
					if(ne=parse_sysex_event_multi(sysexbuffer+1,exlen-1, evm)){
						for (ii = 0; ii < ne; ii++){
							seq_play_event(&evm[ii]);
						}
					}
						
				}

				if ((Pm_MessageStatus(pmmsg) & 0x000000ff) == 0xf2) {
					ev.type = ME_PROGRAM;
//					seq_play_event(&ev);
				}
#if 0
				if ((Pm_MessageStatus(pmmsg)  & 0x000000ff) == 0xf1)
					//MIDI Time Code Qtr. Frame (not need)
					printf("MIDI Time Code Qtr\n");
				if ((Pm_MessageStatus(pmmsg)  & 0x000000ff) == 0xf3)
					//Song Select(Song #) (not need)
				if ((Pm_MessageStatus(pmmsg)  & 0x000000ff) == 0xf6)
					//Tune request (not need)
					printf("Tune request\n");
				if ((Pm_MessageStatus(pmmsg)  & 0x000000ff) == 0xf8)
					//Timing Clock (not need)
					printf("Timing Clock\n");
				if ((Pm_MessageStatus(pmmsg) &0x000000ff)==0xfa)
					//Start
				if ((Pm_MessageStatus(pmmsg)  & 0x000000ff) == 0xfb)
					//Continue
				if ((Pm_MessageStatus(pmmsg)  & 0x000000ff) == 0xfc) {
					//Stop
					printf("Stop\n");
					playdone = ~0;
				}
#endif
				if ((Pm_MessageStatus(pmmsg)  & 0x000000ff) == 0xfe) {
					//Active Sensing
//					printf("Active Sensing\n");
					active_sensing_flag = ~0;
					active_sensing_time = get_current_calender_time();
				}
				if ((Pm_MessageStatus(pmmsg)  & 0x000000ff) == 0xff) {
					//System Reset
					printf("System Reset\n");
					playdone = ~0;
				}
//				break;
			default:
//				printf("Unsup/ed event %d\n", aevp->type);
				break;
			}
			if (ev.type != ME_NONE) {
				seq_play_event(&ev);
				seq_playing=~0;
			}
		}
	}
	return;
pmerror:
	Pm_Terminate();
	ctl.cmsg(  CMSG_ERROR, VERB_NORMAL, "PortMIDI error: %s\n", Pm_GetErrorText( pmerr ) );
	return;
}

#else
void play_some_data (void){
	UINT wMsg;
	DWORD	dwInstance;
	DWORD	dwParam1;
	DWORD	dwParam2;
	MidiEvent ev;
	MidiEvent evm[16];
	int port;
	UINT evbpoint;
	MIDIHDR *IIMidiHdr;
	int exlen;
	char *sysexbuffer;
	int ne,i,j,chk;

	while( (evbuf[evbrpoint].status==B_OK)&&(evbrpoint!=evbwpoint) ){

		evbpoint=evbrpoint;
		if (++evbrpoint >= EVBUFF_SIZE)
				evbrpoint -= EVBUFF_SIZE;

		wMsg=evbuf[evbpoint].wMsg;
		dwInstance=evbuf[evbpoint].dwInstance;
		dwParam1=evbuf[evbpoint].dwParam1;
		dwParam2=evbuf[evbpoint].dwParam2;

		port=(UINT)dwInstance;
		switch (wMsg) {
		case MIM_DATA:
			ev.type = ME_NONE;
			ev.channel = dwParam1 & 0x0000000f;
			ev.channel = ev.channel+port*16;
			ev.a = (dwParam1 >> 8) & 0xff;
			ev.b = (dwParam1 >> 16) & 0xff;
			switch ((int) (dwParam1 & 0x000000f0)) {
			case 0x80:
				ev.type = ME_NOTEOFF;
//				seq_play_event(&ev);
				break;
			case 0x90:
			ev.type = (ev.b) ? ME_NOTEON : ME_NOTEOFF;
//				seq_play_event(&ev);
				break;
			case 0xa0:
				ev.type = ME_KEYPRESSURE;
//				seq_play_event(&ev);
				break;
			case 0xb0:
				if (! convert_midi_control_change(ev.channel, ev.a, ev.b, &ev))
				ev.type = ME_NONE;
				break;
			case 0xc0:
				ev.type = ME_PROGRAM;
//				seq_play_event(&ev);
				break;
			case 0xd0:
				ev.type = ME_CHANNEL_PRESSURE;
//				seq_play_event(&ev);
				break;
			case 0xe0:
				ev.type = ME_PITCHWHEEL;
//				seq_play_event(&ev);
				break;
			case 0xf0:
				if ((dwParam1 & 0x000000ff) == 0xf2) {
					ev.type = ME_PROGRAM;
//					seq_play_event(&ev);
				}
#if 0
				if ((dwParam1 & 0x000000ff) == 0xf1)
					//MIDI Time Code Qtr. Frame (not need)
					printf("MIDI Time Code Qtr\n");
				if ((dwParam1 & 0x000000ff) == 0xf3)
					//Song Select(Song #) (not need)
				if ((dwParam1 & 0x000000ff) == 0xf6)
					//Tune request (not need)
					printf("Tune request\n");
				if ((dwParam1 & 0x000000ff) == 0xf8)
					//Timing Clock (not need)
					printf("Timing Clock\n");
				if ((dwParam1&0x000000ff)==0xfa)
					//Start
				if ((dwParam1 & 0x000000ff) == 0xfb)
					//Continue
				if ((dwParam1 & 0x000000ff) == 0xfc) {
					//Stop
					printf("Stop\n");
					playdone = ~0;
				}
#endif
				if ((dwParam1 & 0x000000ff) == 0xfe) {
					//Active Sensing
//					printf("Active Sensing\n");
					active_sensing_flag = ~0;
					active_sensing_time = get_current_calender_time();
				}
				if ((dwParam1 & 0x000000ff) == 0xff) {
					//System Reset
					printf("System Reset\n");
					playdone = ~0;
				}
				break;
			default:
//				printf("Unsup/ed event %d\n", aevp->type);
				break;
			}
			if (ev.type != ME_NONE) {
				seq_play_event(&ev);
				seq_playing=~0;
			}
			break;
		case MIM_LONGDATA:
			IIMidiHdr = (MIDIHDR *) dwParam1;
			exlen=(int)IIMidiHdr->dwBytesRecorded;
			sysexbuffer=IIMidiHdr->lpData;
			if(sysexbuffer[exlen-1]=='\xf7'){            // I don't konw why this need
				for(i=0;i<EX_RESET_NO;i++){
					chk=0;
					for(j=0;(j<exlen)&&(j<11);j++){
						if(chk==0 && sysex_resets[i][j]!=sysexbuffer[j]){
							chk=~0;
						}
					}
					if(chk==0){
						 server_reset();
					}
				}

				printf("SyeEx length=%x bytes \n", exlen);
				for(i=0;i<exlen;i++){
					printf("%x ",sysexbuffer[i]);
				}
				printf("\n");

				if(parse_sysex_event(sysexbuffer+1,exlen-1,&ev)){
					if(ev.type==ME_RESET && system_mode!=DEFAULT_SYSTEM_MODE)
						ev.a=system_mode;
					change_system_mode(system_mode);
					seq_play_event(&ev);
				}
				if(ne=parse_sysex_event_multi(sysexbuffer+1,exlen-1, evm)){
					for (i = 0; i < ne; i++){
						seq_play_event(&evm[i]);
					}
				}
			}
			if (MMSYSERR_NOERROR != midiInUnprepareHeader(
					hMidiIn[port], IIMidiHdr, sizeof(MIDIHDR)))
				printf("error1\n");
			if (MMSYSERR_NOERROR != midiInPrepareHeader(
					hMidiIn[port], IIMidiHdr, sizeof(MIDIHDR)))
				printf("error5\n");
			if (MMSYSERR_NOERROR != midiInAddBuffer(
					hMidiIn[port], IIMidiHdr, sizeof(MIDIHDR)))
				printf("error6\n");
			break;
		}
	}
}

void CALLBACK MidiInProc(HMIDIIN hMidiInL, UINT wMsg, DWORD dwInstance,
		DWORD dwParam1, DWORD dwParam2)
{
	UINT evbpoint;
	UINT port;
	
	port=(UINT)dwInstance;
	lastdintime = get_current_calender_time();
	switch (wMsg) {
	case MIM_DATA:
	case MIM_LONGDATA:
		evbpoint = evbwpoint;
		if (++evbwpoint >= EVBUFF_SIZE)
			evbwpoint -= EVBUFF_SIZE;
		evbuf[evbwpoint].status = B_END;
		evbuf[evbpoint].status = B_WORK;
		evbuf[evbpoint].wMsg = wMsg;
		evbuf[evbpoint].dwInstance = dwInstance;
		evbuf[evbpoint].dwParam1 = dwParam1;
		evbuf[evbpoint].dwParam2 = dwParam2;
		evbuf[evbpoint].status = B_OK;
		break;
	case MIM_OPEN:
//		printf("MIM_OPEN\n");
		break;
	case MIM_CLOSE:
//		printf("MIM_CLOSE\n");
		break;
	case MIM_LONGERROR:
		printf("MIM_LONGERROR\n");
		break;
	case MIM_ERROR:
		printf("MIM_ERROR\n");
		break;
	case MIM_MOREDATA:
		printf("MIM_MOREDATA\n");
		break;
	}
}
#endif
/*
 * interface_<id>_loader();
 */
ControlMode *interface_W_loader(void)
{
    return &ctl;
}


