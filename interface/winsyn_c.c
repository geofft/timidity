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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include "interface.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
//#include <sched.h>
#include <sys/types.h>
#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#endif
//#include <netinet/in.h>
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <math.h>
#include <signal.h>

//#ifdef HAVE_SYS_SOUNDCARD_H
//#include <sys/asoundlib.h>
//#else
#include "server_defs.h"
//#endif /* HAVE_SYS_SOUNDCARD_H */

#include "windows.h" 
#include "mmsystem.h"

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

extern char *opt_aq_max_buff,*opt_aq_fill_buff;
extern  int midi_port_number;

int playdone;
int seq_playing;
int seq_quit;

int system_mode=DEFAULT_SYSTEM_MODE;

#define MAX_PORT 4
int portnumber=1;

HMIDIIN  hMidiIn[MAX_PORT];
HMIDIOUT hMidiOut[MAX_PORT];
UINT InNum,wInID[MAX_PORT];
MIDIHDR *IMidiHdr[MAX_PORT];

#define BUFF_SIZE 128

#define EVBUFF_SIZE 512
MidiEvent evbuf[EVBUFF_SIZE];
UINT  evbwpoint=0;
UINT  evbrpoint=0;
UINT evbsysexpoint;
UINT  mvbuse=0;

struct sysexent{
	char buf[BUFF_SIZE];
	UINT port;
	UINT evbpoint;
};
#define EXBUFF_SIZE 128
struct sysexent exbuf[EXBUFF_SIZE];
UINT  exbwpoint=0;
UINT  exbrpoint=0;
UINT  exbuse=0;
int sysexing=0;

double starttime;
static int time_advance;
extern double play_start_time;
double high_time_at=0.0;
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
	if (!ctl.opened)
		return;
}

static int ctl_read(int32 *valp)
{
    return RC_NONE;
}

#ifdef IA_W32G_SYN
extern void PutsConsoleWnd(char *str);
#endif
static int cmsg(int type, int verbosity_level, char *fmt, ...)
{
#ifndef IA_W32G_SYN
    va_list ap;

    if((type==CMSG_TEXT || type==CMSG_INFO || type==CMSG_WARNING) &&
       ctl.verbosity < verbosity_level)
	return 0;

    if(outfp == NULL)
	outfp = stderr;

    va_start(ap, fmt);
    vfprintf(outfp, fmt, ap);
    fputs(NLS, outfp);
    fflush(outfp);
    va_end(ap);
#else
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
#endif

    return 0;
}

static void ctl_event(CtlEvent *e)
{
}

static void seq_reset(void);
void seq_play_event(MidiEvent *);
void seq_set_time(MidiEvent *);
static void seq_set_time2(MidiEvent *);
static void stop_playing(void);
static void doit(void);
static int do_sequencer(void);
static void server_reset(void);
void winplaymidi(void);
void CALLBACK MidiInProc(HMIDIIN,UINT,DWORD,DWORD,DWORD);
int tw_parse_sysex_event_multi(uint8 *, int32, MidiEvent *);
  
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
	double btime;
	int i, j;
	UINT ID[MAX_PORT];
	MidiEvent ev;
	UINT port=0 ;

#ifndef IA_W32G_SYN
	if(n > MAX_PORT ){
		printf( "Usage: timidity -iW [Midi interface No s]\n");
		return;
    }	
#endif
	
	if(n>0){
		port=0;
		while(port<n && n!=0){
			if( (ID[port] = atoi(args[port]))==0 ){
				n=0;
			}else{			
				if(MMSYSERR_NOERROR==midiInOpen(&hMidiIn[port],ID[port]-1,(DWORD)NULL,(DWORD)0L,CALLBACK_NULL)){
					midiInReset(hMidiIn[port]);
					midiInClose(hMidiIn[port]);
				}else{
					n=0;
#ifdef IA_W32G_SYN
					{
						char buff[1024];
						sprintf ( buff, "MIDI IN Device ID %d is not available. So set a proper ID for the MIDI port %d and restart.", ID[port], port );
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
		MIDIINCAPS InCaps;
		char cbuf[80];

		printf("Opening Devicedriver:");
		InNum = midiInGetNumDevs();
		printf("Available Midi Input devices:\n");
		for (ID[port]=1;ID[port] <=InNum;ID[port]++){
   			midiInGetDevCaps(ID[port]-1,(LPMIDIINCAPSA) &InCaps,sizeof(InCaps));
			printf("%d:%s\n",ID[port],(LPSTR)InCaps.szPname);
		}

		printf("Whow many ports do you use?(max %d)\n",MAX_PORT);
		do{
			if (0==scanf("%u",&portnumber)) scanf("%s",cbuf);
		}while(portnumber == 0 ||portnumber > MAX_PORT);
		printf("\n");
	
		for(port=0;port<portnumber;port++){
			printf("Keyin Input Device Number of port%d\n",port);
			do{
				if (0==scanf("%u",&ID[port])) scanf("%s",cbuf);
			}while(ID[port] == 0 ||(ID[port] > InNum));
			printf("\n");		
		}
	}
#endif

	for(port=0;port<portnumber;port++){
		wInID[port]=ID[port]-1;
	}
	
#ifndef IA_W32G_SYN
	printf("TiMidity starting in Windows Synthesizer mode\n");
	printf("Usage: timidity -iW [Midi interface No]\n");
	printf("\n");
	printf("Please wait for piano 'C' sound\n");
	printf("\n");
	printf("N (Normal mode) M(GM mode) S(GS mode) X(XG mode) \n");
	printf("(Only in Normal mode, Mode can be changed by MIDI data)\n");
	printf("m(GM reset) s(GS reset) x(XG reset)\n");
	printf("\n");
	printf("Press 'q' key to stop\n");
#endif

	for(port=0;port<portnumber;port++){		//trick for MIDI Yoke
		midiInOpen(&hMidiIn[port],wInID[port],(DWORD)NULL,(DWORD)0L,CALLBACK_NULL);
		midiInStart(hMidiIn[port]);
		midiOutOpen(&hMidiOut[port],wInID[port],(DWORD)NULL,(DWORD)0L,CALLBACK_NULL);
		midiOutShortMsg(hMidiOut[port], 0x007f3c90); 
		midiOutShortMsg(hMidiOut[port], 0x00003c80);
		midiOutReset(hMidiOut[port]);
    	midiOutClose(hMidiOut[port]);
		midiInReset(hMidiIn[port]);
		midiInClose(hMidiIn[port]);
	}
	
	opt_realtime_playing = 2; /* Enable loading patch while playing */
	allocate_cache_size = 0; /* Don't use pre-calclated samples */
	auto_reduce_polyphony = 0;
	current_keysig = opt_init_keysig;
	note_key_offset = 0;
//	play_mode->acntl(PM_REQ_GETFRAGSIZ, &time_advance);
	time_advance=play_mode->rate/TICKTIME_HZ*2;
	if (!(play_mode->encoding & PE_MONO))
		time_advance >>= 1;
	if (play_mode->encoding & PE_16BIT)
		time_advance >>= 1;
	btime = (double)time_advance / play_mode->rate;
	btime *= 1.01; /* to be sure */	
//	aq_set_soft_queue(btime, 0.0); //removed cause chopping of sounds
//	aq_set_soft_queue(-1.0, 0.0);  
//	alarm(0);

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

	play_mode->close_output();
	play_mode->open_output();	
	seq_reset();
//	opt_aq_max_buff=safe_strdup("5.0");
//	opt_aq_fill_buff=safe_strdup("100%");
//	timidity_init_aq_buff();

	for(port=0;port<portnumber;port++){
		IMidiHdr[port] = (MIDIHDR*)safe_malloc(sizeof(MIDIHDR));
		memset(IMidiHdr[port],0,sizeof(MIDIHDR));
    	IMidiHdr[port]->lpData = (char*)safe_malloc(BUFF_SIZE);
		memset((IMidiHdr[port]->lpData),0,BUFF_SIZE);
    	IMidiHdr[port]->dwBufferLength = BUFF_SIZE;
	}

	evbuf[0].type=ME_NONE;
	evbwpoint=0;
	evbrpoint=0;
	mvbuse=0;
	
	exbuf[0].buf[0]=0;
	exbwpoint=0;
	exbrpoint=0;
	exbuse=0;
	
	seq_quit=0;
	
	ev.type=ME_RESET;
	ev.a=GS_SYSTEM_MODE; //GM is mor better ???
	seq_set_time(&ev);
	seq_play_event(&ev);
	system_mode=DEFAULT_SYSTEM_MODE;
	change_system_mode(system_mode);

	for(port=0;port<portnumber;port++){
		midiInOpen(&hMidiIn[port],wInID[port],(DWORD)MidiInProc,(DWORD)port,CALLBACK_FUNCTION);
		midiInPrepareHeader(hMidiIn[port],IMidiHdr[port],sizeof(MIDIHDR)); 
		midiInAddBuffer(hMidiIn[port],IMidiHdr[port],sizeof(MIDIHDR));
	}	
	for(port=0;port<portnumber;port++){
	if(MMSYSERR_NOERROR !=midiInStart(hMidiIn[port]))
		printf("midiInStarterror\n");
	}

	ev.channel=0x00;
	ev.a=0x3c;
	ev.b=0x7f;
	ev.type = ME_NOTEON;
	seq_set_time(&ev);
	seq_play_event(&ev);
    ev.type = ME_NOTEOFF;
    seq_set_time(&ev);
    seq_play_event(&ev);

	while(seq_quit==0) {

#ifndef IA_W32G_SYN
		doit();
#else
		w32g_syn_doit();
#endif
	}
	midiInStop(hMidiIn[port]);
	
	stop_playing();
	play_mode->close_output();
	
	for(port=0;port<portnumber;port++){
		midiInReset(hMidiIn[port]);
   		midiInUnprepareHeader(hMidiIn[port],IMidiHdr[port],sizeof(MIDIHDR));
    	midiInClose(hMidiIn[port]);
		free(IMidiHdr[port]->lpData);
		free(IMidiHdr[port]);
	}

#ifdef IA_W32G_SYN
	return 0;
#endif
}
static void seq_reset(void){								
		stop_playing();
		play_mode->close_output();  //also in server_reset
		free_instruments(0);        //also in server_reset
		free_global_mblock();       		
		server_reset();
//		printf("system reseted\n");
}

void seq_play_event(MidiEvent *ev)
{
  int gch;
	
	gch = GLOBAL_CHANNEL_EVENT_TYPE(ev->type);
	if(gch || !IS_SET_CHANNELMASK(quietchannels, ev->channel) ){
			seq_set_time2(ev);
			play_event(ev);
			aq_fill_nonblocking();
	}
}

void seq_set_time(MidiEvent *ev)
{
	double past_time;
//	event_time_offset = play_mode->rate/TICKTIME_HZ;
//	event_time_offset = time_advance;
	past_time = get_current_calender_time() - starttime;
	if(play_mode->flag & PF_PCM_STREAM)
		past_time += high_time_at;
	ev->time = (int32)((past_time) * play_mode->rate);
//	ev->time += (int32)event_time_offset;
}

static void seq_set_time2(MidiEvent *ev)
{
	double past_time;
//	event_time_offset = play_mode->rate/TICKTIME_HZ;
	event_time_offset = time_advance;
	past_time = get_current_calender_time() - starttime;
	if(play_mode->flag & PF_PCM_STREAM)
		past_time += high_time_at;
	ev->time = (int32)((past_time) * play_mode->rate);
	ev->time += (int32)event_time_offset;
}


static void stop_playing(void)
{
	if(upper_voices) {
		MidiEvent ev;
		ev.type = ME_EOT;
		ev.a = 0;
		ev.b = 0;
		seq_set_time(&ev);		
		seq_play_event(&ev);
		aq_flush(0);
	}
}

#ifndef IA_W32G_SYN
static void doit(void)
{
	MSG msg;
	double fill_time;
	MidiEvent ev;
	int i;
	char linebuf[128];

	
	
	playdone=0;

	while(seq_quit==0){
		if(kbhit()){
			switch(getch()){
				case 'Q':
				case 'q':
					seq_quit=~0;
				break;
				
				case 'm':
					ev.type=ME_RESET;
					ev.a=GM_SYSTEM_MODE;
				    seq_set_time(&ev);
					seq_play_event(&ev);
				break;

				case 's':
					ev.type=ME_RESET;
					ev.a=GS_SYSTEM_MODE;
				    seq_set_time(&ev);
					seq_play_event(&ev);
				break;

				case 'x':
					ev.type=ME_RESET;
					ev.a=XG_SYSTEM_MODE;
				    seq_set_time(&ev);
					seq_play_event(&ev);
				break;
				
				case 'c':
					ev.type=ME_RESET;
					ev.a=system_mode;
					seq_set_time(&ev);
					seq_play_event(&ev);
				break;

				case 'M':
					system_mode=GM_SYSTEM_MODE;
					ev.type=ME_RESET;
					ev.a=GM_SYSTEM_MODE;
					seq_set_time(&ev);
					seq_play_event(&ev);
					change_system_mode(system_mode);
					
				break;

				case 'S':
					system_mode=GS_SYSTEM_MODE;
					ev.type=ME_RESET;
					ev.a=GS_SYSTEM_MODE;
					seq_set_time(&ev);
					seq_play_event(&ev);
					change_system_mode(system_mode);
				break;

				case 'X':
					system_mode=XG_SYSTEM_MODE;
					ev.type=ME_RESET;
					ev.a=XG_SYSTEM_MODE;
					seq_set_time(&ev);
					seq_play_event(&ev);
					change_system_mode(system_mode);
					
				break;
				
				case 'N':
					system_mode=DEFAULT_SYSTEM_MODE;
					ev.type=ME_RESET;
					ev.a=GS_SYSTEM_MODE;
					seq_set_time(&ev);
					seq_play_event(&ev);
					change_system_mode(system_mode);
				break;

			}
		}
		winplaymidi();
		sleep(0);
/*		if(playdone!=0){
			seq_reset();
			playdone=0;
		}
*/
	}
}
#endif

static void server_reset(void)
{
	MidiEvent ev;

	play_mode->close_output();	// PM_REQ_PLAY_START wlll called in playmidi_stream_init()
	play_mode->open_output();	// but w32_a.c does not have it.
	readmidi_read_init();
	playmidi_stream_init();
	starttime=get_current_calender_time();
//	if (free_instruments_afterwards)   //also in seq_reset
//		free_instruments(0);
	reduce_voice_threshold = 0; // * Disable auto reduction voice *
	auto_reduce_polyphony = 0;
	event_time_offset = 0;
//	readmidi_read_init();								
}

static void winplayevents(void){
	MidiEvent ev;

	while(mvbuse!=0) sleep(0);
	mvbuse=~0;
	while(  (evbuf[evbrpoint].type!=ME_NONE) && (evbwpoint!=evbrpoint) && sysexing==0 &&
			( (exbuf[exbrpoint].buf[0]==0)||
			((exbuf[exbrpoint].buf[0]!=0)&&(evbrpoint!=exbuf[exbrpoint].evbpoint)) ) 
			){
			ev.time=evbuf[evbrpoint].time;
			ev.type=evbuf[evbrpoint].type;
			ev.channel=evbuf[evbrpoint].channel;
			ev.a=evbuf[evbrpoint].a;
			ev.b=evbuf[evbrpoint].b;
			evbrpoint++;if(evbrpoint>=EVBUFF_SIZE) evbrpoint -= EVBUFF_SIZE;
//			mvbuse=0;
/*			if(ev.type==ME_RESET_CONTROLLERS){
			playdone=~0;
			}
*/
			seq_set_time(&ev);
			seq_play_event(&ev);
			seq_playing=~0;	
				
			ev.type = ME_NONE;
			seq_set_time2(&ev);
			seq_play_event(&ev);
			

	}
	mvbuse=0;
}
static int evbp_ge(UINT evbpointa,UINT evbpointb){
	if( evbpointb<=evbpointa && (evbwpoint<evbpointb || evbpointa<=evbwpoint) ) return ~0;
	if( evbpointa< evbpointb && (evbpointa<=evbwpoint && evbwpoint<evbpointb) ) return ~0;
	return 0;
}

void winplaymidi(void){
	MidiEvent ev;
	int exlen;
	char exlbuf[EXBUFF_SIZE];
	int i,j,chk;
	UINT port;

	
	while(exbuse!=0) sleep(0);
	exbuse=~0;
	if(( exbuf[exbrpoint].buf[0]!=0)  && (exbwpoint!=exbrpoint) ){
		if(evbrpoint != exbuf[exbrpoint].evbpoint){
			winplayevents();		
		}
		exlen=0;
		while( (exbuf[exbrpoint].buf[exlen]!='\xf7') && (exlen<EXBUFF_SIZE-1) ){
			exlbuf[exlen]=exbuf[exbrpoint].buf[exlen];
			exlen++;
		}
		port=exbuf[exbrpoint].port;
		exlbuf[exlen]=exbuf[exbrpoint].buf[exlen];
		exlen++;
//		printf("sysex %x byte\n",exlen);
		exbrpoint++;if(exbrpoint>=EXBUFF_SIZE) exbrpoint -= EXBUFF_SIZE;
		exbuse=0;
		
		
		if(exlen!=0){
				for(i=0;i<EX_RESET_NO;i++){
					chk=0;
					for(j=0;(j<exlen)&&(j<11);j++){
						if(chk==0 && sysex_resets[i][j]!=exlbuf[j]){
							chk=~0;
						}
					}
					if(chk==0){
						 server_reset();
					}
				}
		}
		
	}else{
		exbuse=0;
	}
	if(exlen!=0){
		midi_port_number=port;
		if(parse_sysex_event(exlbuf,exlen,&ev)){
			if(ev.type==ME_RESET && system_mode!=DEFAULT_SYSTEM_MODE)
				ev.a=system_mode;
			change_system_mode(system_mode);
			seq_set_time(&ev);
			seq_play_event(&ev);
		
			ev.type = ME_NONE;
			seq_set_time2(&ev);
			seq_play_event(&ev);
		}
		tw_parse_sysex_event_multi(exlbuf,exlen, &ev);
	}	
	exlen=0;
		
	winplayevents();
	
	ev.type = ME_NONE;
	seq_set_time2(&ev);
	seq_play_event(&ev);
	if(active_sensing_flag==~0 && (get_current_calender_time() > active_sensing_time+0.5)){
//normaly acitive sensing expiering time is 330ms(>300ms) but this loop is heavy
		play_mode->close_output();
		play_mode->open_output();
		printf ("Active Sensing Expired\n");
		active_sensing_flag=0;
	}
		
//	if((seq_playing == ~0) && (get_current_calender_time()-lastdintime>30)){
//	playdone=~0;
//	seq_playing =0;
	
}

void CALLBACK MidiInProc(HMIDIIN hMidiInL, UINT wMsg, DWORD dwInstance,
		DWORD dwParam1, DWORD dwParam2)
{
	MidiEvent ev;
	uint8 a, b;
	UINT evbpoint;
	int port;
	
	port=(UINT)dwInstance;
	lastdintime = get_current_calender_time();
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
//			seq_play_event(&ev);
			break;
		case 0x90:
//			printf("%x\n", ev.b);
			ev.type = (ev.b) ? ME_NOTEON : ME_NOTEOFF;
//			seq_play_event(&ev);
			break;
		case 0xa0:
			ev.type = ME_KEYPRESSURE;
//			seq_play_event(&ev);
			break;
		case 0xb0:
			if (! convert_midi_control_change(ev.channel, ev.a, ev.b, &ev))
				ev.type = ME_NONE;
			break;
		case 0xc0:
			ev.type = ME_PROGRAM;
//			seq_play_event(&ev);
			break;
		case 0xd0:
			ev.type = ME_CHANNEL_PRESSURE;
//			seq_play_event(&ev);
			break;
		case 0xe0:
			ev.type = ME_PITCHWHEEL;
//			seq_play_event(&ev);
			break;
		case 0xf0:
			if ((dwParam1 & 0x000000ff) == 0xf2) {
				ev.a = a;
				ev.type = ME_PROGRAM;
//				seq_play_event(&ev);
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
//				printf("Active Sensing\n");
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
//			printf("Unsup/ed event %d\n", aevp->type);
			break;
		}
		if (ev.type != ME_NONE) {
			while (mvbuse)
				sleep(0);
			mvbuse = ~0;
//			seq_set_time(&ev);
			evbuf[evbwpoint].time = ev.time;
			evbuf[evbwpoint].type = ev.type;
			evbuf[evbwpoint].channel = ev.channel;
			evbuf[evbwpoint].a = ev.a;
			evbuf[evbwpoint].b = ev.b;
			if (++evbwpoint >= EVBUFF_SIZE)
				evbwpoint -= EVBUFF_SIZE;
			evbuf[evbwpoint].type = ME_NONE;
			mvbuse = 0;
		}
		break;
	case MIM_LONGDATA:
		sysexing = ~0;
		evbpoint = evbwpoint;
		lastdintime = get_current_calender_time();
//		printf("longdata recived\n");
		if (MMSYSERR_NOERROR != midiInUnprepareHeader(
				hMidiIn[port], IMidiHdr[port], sizeof(MIDIHDR)))
			printf("error1\n");
//		printf("length=%x bytes \n", IMidiHdr[port]->dwBytesRecorded);
		if (IMidiHdr[port]->dwBytesRecorded > EXBUFF_SIZE)
			printf("sysex is too long!! ignored\n");
		else {
			while (exbuse)
				sleep(0);
			exbuse = ~0;
			exbuf[exbwpoint].evbpoint = evbpoint;
			memcpy(exbuf[exbwpoint].buf, IMidiHdr[port]->lpData,
					IMidiHdr[port]->dwBytesRecorded);
			if (++exbwpoint >= EXBUFF_SIZE)
				exbwpoint -= EXBUFF_SIZE;
			exbuf[exbwpoint].buf[0] = 0;
			exbuf[exbwpoint].port = port;
			exbuse = 0;
		}
		sysexing = 0;
		if (MMSYSERR_NOERROR != midiInUnprepareHeader(
				hMidiIn[port], IMidiHdr[port], sizeof(MIDIHDR)))
			printf("error1\n");
		if (MMSYSERR_NOERROR != midiInPrepareHeader(
				hMidiIn[port], IMidiHdr[port], sizeof(MIDIHDR)))
			printf("error5\n");
		if (MMSYSERR_NOERROR != midiInAddBuffer(
				hMidiIn[port], IMidiHdr[port], sizeof(MIDIHDR)))
			printf("error6\n");
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

/*
 * interface_<id>_loader();
 */
ControlMode *interface_W_loader(void)
{
    return &ctl;
}



//
//	
//***********  The below codes are  came from readmidi.c     ***************
//
//

#include "tables.h"
#include "reverb.h"


#define MAX_MIDI_EVENT ((MAX_SAFE_MALLOC_SIZE / sizeof(MidiEvent)) - 1)

extern uint8 userdrum_assign_group[128];

/* MIDI ports will be merged in several channels in the future. */
extern  int midi_port_number;

#define SETMIDIEVENT(e, t, ch, pa, pb) \
    { (e).type = (t); \
      (e).channel = (uint8)(ch) ; (e).a = (uint8)(pa); (e).b = (uint8)(pb); }

#define TWMIDIEVENT(t, ch, pa, pb) \
    { MidiEvent event; SETMIDIEVENT(event, t, ch, pa, pb); \
      seq_set_time(&event); seq_play_event(&event); \
      event.type = ME_NONE; \
	  seq_set_time2(&event); \
	  seq_play_event(&event); \
    }

#if MAX_CHANNELS <= 16
#define MERGE_CHANNEL_PORT(ch) ((int)(ch))
#else
#define MERGE_CHANNEL_PORT(ch) ((int)(ch) | (midi_port_number << 4))
#endif



/* XG SysEx parsing function by Eric A. Welsh
 * Also handles GS patch+bank changes
 *
 * This function provides basic support for XG Multi Part Data
 * parameter change SysEx events
 *
 * NOTE - val[1] is documented as only being 0x10, but this rule is not
 * followed in real life, since I have midi that set it to 0x00 and are
 * interpreted correctly on my SW60XG ...
 */
int tw_parse_sysex_event_multi(uint8 *val, int32 len, MidiEvent *ev)
{
    int num_events = 0;				/* Number of events added */

    if(current_file_info->mid == 0 || current_file_info->mid >= 0x7e)
	current_file_info->mid = val[0];

    /* XG Multi Part Data parameter change */
    /* There are two ways to do this, neither of which match the XG spec... */
    /* All variables given in a single big block */
    if(len >= 10 &&
       val[0] == 0x43 && /* Yamaha ID */
       val[2] == 0x4C && /* XG Model ID */
       val[4] == 0x29 && /* Total size of data body to be analyzed */
       val[5] == 0x08)   /* Multi Part Data parameter change */
    {
	uint8 addhigh, addmid, addlow;		/* Addresses */
	uint8 *body;				/* SysEx body */
	uint8 p;				/* Channel part number [0..15] */
	int ent;				/* Entry # of sub-event */
	uint8 *body_end;			/* End of SysEx body */

	addhigh = val[3];
	addmid = val[4];
	addlow = val[5];
	body = val + 8;
	p = val[6];
	body_end = val + len-3;

	for (ent=0; body <= body_end; body++, ent++) {
	    switch(ent) {

		case 0x01:	/* bank select MSB */
		  TWMIDIEVENT( ME_TONE_BANK_MSB, p, *body, 0);
		  num_events++;
		  break;

		case 0x02:	/* bank select LSB */
		  TWMIDIEVENT( ME_TONE_BANK_LSB, p, *body, 0);
		  num_events++;
		  break;

		case 0x03:	/* program number */
		  TWMIDIEVENT( ME_PROGRAM, p, *body, 0);
		  num_events++;
		  break;

		case 0x05:	/* mono/poly mode */
 		  if(*body == 0) {
			  channel[p].mono = 1;
		  } else {
			  channel[p].mono = 0;
		  }
		  break;

		case 0x08:	/* note shift ? */
		  TWMIDIEVENT( ME_KEYSHIFT, p, *body, 0);
		  num_events++;
		  break;

		case 0x0B:	/* volume */
		  TWMIDIEVENT( ME_MAINVOLUME, p, *body, 0);
		  num_events++;
		  break;

		case 0x0E:	/* pan */
		  if(*body == 0) {
			TWMIDIEVENT( ME_RANDOM_PAN, p, 0, 0);
		  }
		  else {
			TWMIDIEVENT( ME_PAN, p, *body, 0);
		  }
		  num_events++;
		  break;

		case 0x12:	/* chorus send */
		  TWMIDIEVENT( ME_CHORUS_EFFECT, p, *body, 0);
		  num_events++;
		  break;

		case 0x13:	/* reverb send */
		  TWMIDIEVENT( ME_REVERB_EFFECT, p, *body, 0);
		  num_events++;
		  break;

		case 0x23:	/* bend pitch control */
		  TWMIDIEVENT(ME_RPN_MSB,p,0,0);
		  TWMIDIEVENT(ME_RPN_LSB,p,0,0);
		  TWMIDIEVENT(ME_DATA_ENTRY_MSB,p,(*body - 0x40) & 0x7F,0);
		  num_events += 3;
		  break;

		case 0x41:	/* scale tuning */
		  channel[p].scale_tuning[0] = *body - 64;
		  cmsg(CMSG_INFO,VERB_NOISY,"Scale Tuning C (CH:%d %dcent)",p,*body - 64);
		  break;

		case 0x42:	/* scale tuning */
		  channel[p].scale_tuning[1] = *body - 64;
		  cmsg(CMSG_INFO,VERB_NOISY,"Scale Tuning C# (CH:%d %dcent)",p,*body - 64);
		  break;

		case 0x43:	/* scale tuning */
		  channel[p].scale_tuning[2] = *body - 64;
		  cmsg(CMSG_INFO,VERB_NOISY,"Scale Tuning D (CH:%d %dcent)",p,*body - 64);
		  break;

		case 0x44:	/* scale tuning */
		  channel[p].scale_tuning[3] = *body - 64;
		  cmsg(CMSG_INFO,VERB_NOISY,"Scale Tuning D# (CH:%d %dcent)",p,*body - 64);
		  break;

		case 0x45:	/* scale tuning */
		  channel[p].scale_tuning[4] = *body - 64;
		  cmsg(CMSG_INFO,VERB_NOISY,"Scale Tuning E (CH:%d %dcent)",p,*body - 64);
		  break;

		case 0x46:	/* scale tuning */
		  channel[p].scale_tuning[5] = *body - 64;
		  cmsg(CMSG_INFO,VERB_NOISY,"Scale Tuning F (CH:%d %dcent)",p,*body - 64);
		  break;

		case 0x47:	/* scale tuning */
		  channel[p].scale_tuning[6] = *body - 64;
		  cmsg(CMSG_INFO,VERB_NOISY,"Scale Tuning F# (CH:%d %dcent)",p,*body - 64);
		  break;

		case 0x48:	/* scale tuning */
		  channel[p].scale_tuning[7] = *body - 64;
		  cmsg(CMSG_INFO,VERB_NOISY,"Scale Tuning G (CH:%d %dcent)",p,*body - 64);
		  break;

		case 0x49:	/* scale tuning */
		  channel[p].scale_tuning[8] = *body - 64;
		  cmsg(CMSG_INFO,VERB_NOISY,"Scale Tuning G# (CH:%d %dcent)",p,*body - 64);
		  break;

		case 0x4A:	/* scale tuning */
		  channel[p].scale_tuning[9] = *body - 64;
		  cmsg(CMSG_INFO,VERB_NOISY,"Scale Tuning A (CH:%d %dcent)",p,*body - 64);
		  break;

		case 0x4B:	/* scale tuning */
		  channel[p].scale_tuning[10] = *body - 64;
		  cmsg(CMSG_INFO,VERB_NOISY,"Scale Tuning A# (CH:%d %dcent)",p,*body - 64);
		  break;

		case 0x4C:	/* scale tuning */
		  channel[p].scale_tuning[11] = *body - 64;
		  cmsg(CMSG_INFO,VERB_NOISY,"Scale Tuning B (CH:%d %dcent)",p,*body - 64);
		  break;


		default:
		  cmsg(CMSG_INFO,VERB_NOISY,"Unsupported XG SysEx. (ADDR:%02X %02X %02X VAL:%02X)",val[3],val[4],val[5],val[6]);
		  continue;
		  break;
	    }
	}
    }
    /* Or you can specify them one SYSEX event at a time... */
    else if(len == 8 &&
       val[0] == 0x43 && /* Yamaha ID */
       val[2] == 0x4C && /* XG Model ID */
       val[3] == 0x08)   /* Multi Part Data parameter change */
    {
	uint8 p;				/* Channel part number [0..15] */
	int ent;				/* Entry # of sub-event */

	p = val[4];
	ent = val[5];

	switch(ent) {
		case 0x01:	/* bank select MSB */
		  TWMIDIEVENT( ME_TONE_BANK_MSB, p, val[6], 0);
		  num_events++;
		  break;

		case 0x02:	/* bank select LSB */
		  TWMIDIEVENT( ME_TONE_BANK_LSB, p, val[6], 0);
		  num_events++;
		  break;

		case 0x03:	/* program number */
		  TWMIDIEVENT( ME_PROGRAM, p, val[6], 0);
		  num_events++;
		  break;

		case 0x05:	/* mono/poly mode */
 		  if(val[6] == 0) {
			  channel[p].mono = 1;
		  } else {
			  channel[p].mono = 0;
		  }
		  break;

		case 0x08:	/* note shift ? */
		  TWMIDIEVENT( ME_KEYSHIFT, p, val[6], 0);
		  num_events++;
		  break;

		case 0x0B:	/* volume */
		  TWMIDIEVENT( ME_MAINVOLUME, p, val[6], 0);
		  num_events++;
		  break;

		case 0x0E:	/* pan */
		  if(val[6] == 0) {
			TWMIDIEVENT( ME_RANDOM_PAN, p, 0, 0);
		  }
		  else {
			TWMIDIEVENT( ME_PAN, p, val[6], 0);
		  }
		  num_events++;
		  break;

		case 0x12:	/* chorus send */
		  TWMIDIEVENT( ME_CHORUS_EFFECT, p, val[6], 0);
		  num_events++;
		  break;

		case 0x13:	/* reverb send */
		  TWMIDIEVENT( ME_REVERB_EFFECT, p, val[6], 0);
		  num_events++;
		  break;

		case 0x23:	/* bend pitch control */
		  TWMIDIEVENT(ME_RPN_MSB,p,0,0);
		  TWMIDIEVENT(ME_RPN_LSB,p,0,0);
		  TWMIDIEVENT(ME_DATA_ENTRY_MSB,p,(val[6] - 0x40) & 0x7F,0);
		  num_events += 3;
		  break;

		case 0x41:	/* scale tuning */
		  channel[p].scale_tuning[0] = val[6] - 64;
		  cmsg(CMSG_INFO,VERB_NOISY,"Scale Tuning C (CH:%d %dcent)",p,val[6] - 64);
		  break;

		case 0x42:	/* scale tuning */
		  channel[p].scale_tuning[1] = val[6] - 64;
		  cmsg(CMSG_INFO,VERB_NOISY,"Scale Tuning C# (CH:%d %dcent)",p,val[6] - 64);
		  break;

		case 0x43:	/* scale tuning */
		  channel[p].scale_tuning[2] = val[6] - 64;
		  cmsg(CMSG_INFO,VERB_NOISY,"Scale Tuning D (CH:%d %dcent)",p,val[6] - 64);
		  break;

		case 0x44:	/* scale tuning */
		  channel[p].scale_tuning[3] = val[6] - 64;
		  cmsg(CMSG_INFO,VERB_NOISY,"Scale Tuning D# (CH:%d %dcent)",p,val[6] - 64);
		  break;

		case 0x45:	/* scale tuning */
		  channel[p].scale_tuning[4] = val[6] - 64;
		  cmsg(CMSG_INFO,VERB_NOISY,"Scale Tuning E (CH:%d %dcent)",p,val[6] - 64);
		  break;

		case 0x46:	/* scale tuning */
		  channel[p].scale_tuning[5] = val[6] - 64;
		  cmsg(CMSG_INFO,VERB_NOISY,"Scale Tuning F (CH:%d %dcent)",p,val[6] - 64);
		  break;

		case 0x47:	/* scale tuning */
		  channel[p].scale_tuning[6] = val[6] - 64;
		  cmsg(CMSG_INFO,VERB_NOISY,"Scale Tuning F# (CH:%d %dcent)",p,val[6] - 64);
		  break;

		case 0x48:	/* scale tuning */
		  channel[p].scale_tuning[7] = val[6] - 64;
		  cmsg(CMSG_INFO,VERB_NOISY,"Scale Tuning G (CH:%d %dcent)",p,val[6] - 64);
		  break;

		case 0x49:	/* scale tuning */
		  channel[p].scale_tuning[8] = val[6] - 64;
		  cmsg(CMSG_INFO,VERB_NOISY,"Scale Tuning G# (CH:%d %dcent)",p,val[6] - 64);
		  break;

		case 0x4A:	/* scale tuning */
		  channel[p].scale_tuning[9] = val[6] - 64;
		  cmsg(CMSG_INFO,VERB_NOISY,"Scale Tuning A (CH:%d %dcent)",p,val[6] - 64);
		  break;

		case 0x4B:	/* scale tuning */
		  channel[p].scale_tuning[10] = val[6] - 64;
		  cmsg(CMSG_INFO,VERB_NOISY,"Scale Tuning A# (CH:%d %dcent)",p,val[6] - 64);
		  break;

		case 0x4C:	/* scale tuning */
		  channel[p].scale_tuning[11] = val[6] - 64;
		  cmsg(CMSG_INFO,VERB_NOISY,"Scale Tuning B (CH:%d %dcent)",p,val[6] - 64);
		  break;

		default:
		  cmsg(CMSG_INFO,VERB_NOISY,"Unsupported XG SysEx. (ADDR:%02X %02X %02X VAL:%02X)",val[3],val[4],val[5],val[6]);
		  break;
	}
    }

    /* parsing GS System Exclusive Message... */
	/* val[4] == Parameter Address(High)
	   val[5] == Parameter Address(Middle)
	   val[6] == Parameter Address(Low)
	   val[7]... == Data...
	   val[last] == Checksum(== 128 - (sum of addresses&data bytes % 128)) 
	*/
    else if(len >= 9 &&
	   val[0] == 0x41 && /* Roland ID */
       val[1] == 0x10 && /* Device ID */
       val[2] == 0x42 && /* GS Model ID */
       val[3] == 0x12) /* Data Set Command */
    {
		static uint8 userdrum_prog,userdrum_map,userinst_map,userinst_bank;
		uint8 p,dp,udn;
		int i,addr,addr_h,addr_m,addr_l;
		p = val[5] & 0x0F;
		if(p == 0) {p = 9;}
		else if(p <= 9) {p--;}
		p = MERGE_CHANNEL_PORT(p);

		/* search drum channel for user drumset */ 
		dp = (val[5] & 0xF0) >> 4;
		for(i=0;i<32;i++) {
			if(ISDRUMCHANNEL(i)) {
				if(dp == 0) {
					dp = i;
					break;
				}
				dp = 0;
			}
		}
		if(dp == 0) {dp = 9;}

		/* for double module mode SysEx. */
		if(val[4] == 0x50) {
			p += 16;
			val[4] = 0x40;
		}
		if(val[4] == 0x51) {val[4] = 0x41;}

		/* calculate user drumset number */
		udn = (val[5] & 0xF0) >> 4;

		addr = (((int32)val[4])<<16 | ((int32)val[5])<<8 | (int32)val[6]);
		addr_h = val[4];
		addr_m = val[5];
		addr_l = val[6];

		switch(addr_h) {	
		case 0x40:
			if((addr & 0xFFF000) == 0x401000) {
				switch(addr & 0xFF) {
				case 0x00:
					TWMIDIEVENT( ME_TONE_BANK_MSB,p,val[7],0);
					TWMIDIEVENT( ME_PROGRAM,p,val[8],0);
					num_events += 2;
					break;
				case 0x13:
					if(val[7] == 0) {
						channel[p].mono = 1;
					} else {
						channel[p].mono = 0;
					}
					break;
				case 0x14:	/* Assign Mode */
					channel[p].assign_mode = val[7];
					cmsg(CMSG_INFO,VERB_NOISY,"Unsupported GS SysEx: Assign Mode (CH:%d VAL:%d)",p,channel[p].assign_mode);
					break;
				case 0x15:	/* Use for Rhythm Part */
					break;
				case 0x16:	/* Pitch Key Shift */
					break;
				case 0x17:	/* Pitch Offset Fine */
					channel[p].pitch_offset_fine = (FLOAT_T)((((int32)val[7] << 4) | (int32)val[8]) - 0x80) / 10.0;
					cmsg(CMSG_INFO,VERB_NOISY,"Pitch Offset Fine (CH:%d %3fHz)",p,channel[p].pitch_offset_fine);
					break;
				case 0x19:
					TWMIDIEVENT(ME_MAINVOLUME,p,val[7],0);
					num_events++;
					break;
				case 0x1A:
					channel[p].velocity_sense_depth = val[7];
					cmsg(CMSG_INFO,VERB_NOISY,"Velocity Sense Depth (CH:%d VAL:%d)",p,val[7]);
					break;
				case 0x1B:
					channel[p].velocity_sense_offset = val[7];
					cmsg(CMSG_INFO,VERB_NOISY,"Velocity Sense Offset (CH:%d VAL:%d)",p,val[7]);
					break;
				case 0x1C:
					if (val[7] == 0) {
						TWMIDIEVENT( ME_RANDOM_PAN, p, 0, 0);
					} else {
						TWMIDIEVENT(ME_PAN,p,val[7],0);
					}
					num_events++;
					break;
				case 0x21:
					TWMIDIEVENT(ME_CHORUS_EFFECT,p,val[7],0);
					num_events++;
					break;
				case 0x22:
					TWMIDIEVENT(ME_REVERB_EFFECT,p,val[7],0);
					num_events++;
					break;
				case 0x2C:
					TWMIDIEVENT(ME_CELESTE_EFFECT,p,val[7],0);
					num_events++;
					break;
				case 0x2A:
					TWMIDIEVENT(ME_NRPN_MSB,p,0x00,0);
					TWMIDIEVENT(ME_NRPN_LSB,p,0x01,0);
					TWMIDIEVENT(ME_DATA_ENTRY_MSB,p,val[7],0);
					TWMIDIEVENT(ME_DATA_ENTRY_LSB,p,val[8],0);
					num_events += 4;
					break;
				case 0x30:
					TWMIDIEVENT(ME_NRPN_MSB,p,0x01,0);
					TWMIDIEVENT(ME_NRPN_LSB,p,0x08,0);
					TWMIDIEVENT(ME_DATA_ENTRY_MSB,p,val[7],0);
					num_events += 3;
					break;
				case 0x31:
					TWMIDIEVENT(ME_NRPN_MSB,p,0x01,0);
					TWMIDIEVENT(ME_NRPN_LSB,p,0x09,0);
					TWMIDIEVENT(ME_DATA_ENTRY_MSB,p,val[7],0);
					num_events += 3;
					break;
				case 0x32:
					TWMIDIEVENT(ME_NRPN_MSB,p,0x01,0);
					TWMIDIEVENT(ME_NRPN_LSB,p,0x20,0);
					TWMIDIEVENT(ME_DATA_ENTRY_MSB,p,val[7],0);
					num_events += 3;
					break;
				case 0x33:
					TWMIDIEVENT(ME_NRPN_MSB,p,0x01,0);
					TWMIDIEVENT(ME_NRPN_LSB,p,0x21,0);
					TWMIDIEVENT(ME_DATA_ENTRY_MSB,p,val[7],0);
					num_events += 3;
					break;
				case 0x34:
					TWMIDIEVENT(ME_NRPN_MSB,p,0x01,0);
					TWMIDIEVENT(ME_NRPN_LSB,p,0x63,0);
					TWMIDIEVENT(ME_DATA_ENTRY_MSB,p,val[7],0);
					num_events += 3;
					break;
				case 0x35:
					TWMIDIEVENT(ME_NRPN_MSB,p,0x01,0);
					TWMIDIEVENT(ME_NRPN_LSB,p,0x64,0);
					TWMIDIEVENT(ME_DATA_ENTRY_MSB,p,val[7],0);
					num_events += 3;
					break;
				case 0x36:
					TWMIDIEVENT(ME_NRPN_MSB,p,0x01,0);
					TWMIDIEVENT(ME_NRPN_LSB,p,0x66,0);
					TWMIDIEVENT(ME_DATA_ENTRY_MSB,p,val[7],0);
					num_events += 3;
					break;
				case 0x37:
					TWMIDIEVENT(ME_NRPN_MSB,p,0x01,0);
					TWMIDIEVENT(ME_NRPN_LSB,p,0x0A,0);
					TWMIDIEVENT(ME_DATA_ENTRY_MSB,p,val[7],0);
					num_events += 3;
					break;
				case 0x40:	/* Scale Tuning */
					channel[p].scale_tuning[0] = val[7] - 64;
					channel[p].scale_tuning[1] = val[8] - 64;
					channel[p].scale_tuning[2] = val[9] - 64;
					channel[p].scale_tuning[3] = val[10] - 64;
					channel[p].scale_tuning[4] = val[11] - 64;
					channel[p].scale_tuning[5] = val[12] - 64;
					channel[p].scale_tuning[6] = val[13] - 64;
					channel[p].scale_tuning[7] = val[14] - 64;
					channel[p].scale_tuning[8] = val[15] - 64;
					channel[p].scale_tuning[9] = val[16] - 64;
					channel[p].scale_tuning[10] = val[17] - 64;
					channel[p].scale_tuning[11] = val[18] - 64;
					cmsg(CMSG_INFO,VERB_NOISY,"Scale Tuning C (CH:%d %dcent)",p,val[7] - 64);
					cmsg(CMSG_INFO,VERB_NOISY,"Scale Tuning C# (CH:%d %dcent)",p,val[8] - 64);
					cmsg(CMSG_INFO,VERB_NOISY,"Scale Tuning D (CH:%d %dcent)",p,val[9] - 64);
					cmsg(CMSG_INFO,VERB_NOISY,"Scale Tuning D# (CH:%d %dcent)",p,val[10] - 64);
					cmsg(CMSG_INFO,VERB_NOISY,"Scale Tuning E (CH:%d %dcent)",p,val[11] - 64);
					cmsg(CMSG_INFO,VERB_NOISY,"Scale Tuning F (CH:%d %dcent)",p,val[12] - 64);
					cmsg(CMSG_INFO,VERB_NOISY,"Scale Tuning F# (CH:%d %dcent)",p,val[13] - 64);
					cmsg(CMSG_INFO,VERB_NOISY,"Scale Tuning G (CH:%d %dcent)",p,val[14] - 64);
					cmsg(CMSG_INFO,VERB_NOISY,"Scale Tuning G# (CH:%d %dcent)",p,val[15] - 64);
					cmsg(CMSG_INFO,VERB_NOISY,"Scale Tuning A (CH:%d %dcent)",p,val[16] - 64);
					cmsg(CMSG_INFO,VERB_NOISY,"Scale Tuning A# (CH:%d %dcent)",p,val[17] - 64);
					cmsg(CMSG_INFO,VERB_NOISY,"Scale Tuning B (CH:%d %dcent)",p,val[18] - 64);
					break;
				default:
					cmsg(CMSG_INFO,VERB_NOISY,"Unsupported GS SysEx. (ADDR:%02X %02X %02X VAL:%02X %02X)",val[4],val[5],val[6],val[7],val[8]);
					break;
				}
			} else if((addr & 0xFFF000) == 0x402000) {
				switch(addr & 0xFF) {
				case 0x10:	/* Bend Pitch Control */
					TWMIDIEVENT(ME_RPN_MSB,p,0,0);
					TWMIDIEVENT(ME_RPN_LSB,p,0,0);
					TWMIDIEVENT(ME_DATA_ENTRY_MSB,p,(val[7] - 0x40) & 0x7F,0);
					num_events += 3;
					break;
				default:
					cmsg(CMSG_INFO,VERB_NOISY,"Unsupported GS SysEx. (ADDR:%02X %02X %02X VAL:%02X %02X)",val[4],val[5],val[6],val[7],val[8]);
					break;
				}
			} else if((addr & 0xFFFF00) == 0x400100) {
				switch(addr & 0xFF) {
				case 0x30:	/* Reverb Macro */
					set_reverb_macro(val[7]);
					recompute_reverb_status();
					break;
				case 0x31:	/* Reverb Character */
					reverb_status.character = val[7];
					recompute_reverb_status();
					break;
				case 0x32:	/* Reverb Pre-LPF */
					reverb_status.pre_lpf = val[7];
					recompute_reverb_status();
					break;
				case 0x33:	/* Reverb Level */
					reverb_status.level = val[7];
					recompute_reverb_status();
					break;
				case 0x34:	/* Reverb Time */
					reverb_status.time = val[7];
					recompute_reverb_status();
					break;
				case 0x35:	/* Reverb Delay Feedback */
					reverb_status.delay_feedback = val[7];
					recompute_reverb_status();
					break;
				case 0x36:	/* Unknown Reverb Parameter */
					break;
				case 0x37:	/* Reverb Predelay Time */
					reverb_status.pre_delay_time = val[7];
					recompute_reverb_status();
					break;
				case 0x38:	/* Chorus Macro */
					set_chorus_macro(val[7]);
					recompute_chorus_status();
					init_chorus_lfo();
					break;
				case 0x39:	/* Chorus Pre-LPF */
					chorus_param.chorus_pre_lpf = val[7];
					recompute_chorus_status();
					break;
				case 0x3A:	/* Chorus Level */
					chorus_param.chorus_level = val[7];
					recompute_chorus_status();
					break;
				case 0x3B:	/* Chorus Feedback */
					chorus_param.chorus_feedback = val[7];
					recompute_chorus_status();
					break;
				case 0x3C:	/* Chorus Delay */
					chorus_param.chorus_delay = val[7];
					recompute_chorus_status();
					init_chorus_lfo();
					break;
				case 0x3D:	/* Chorus Rate */
					chorus_param.chorus_rate = val[7];
					recompute_chorus_status();
					init_chorus_lfo();
					break;
				case 0x3E:	/* Chorus Depth */
					chorus_param.chorus_depth = val[7];
					recompute_chorus_status();
					init_chorus_lfo();
					break;
				case 0x3F:	/* Chorus Send Level to Reverb */
					chorus_param.chorus_send_level_to_reverb = val[7];
					recompute_chorus_status();
					break;
				case 0x40:	/* Chorus Send Level to Delay */
					chorus_param.chorus_send_level_to_delay = val[7];
					recompute_chorus_status();
					break;
				case 0x50:	/* Delay Macro */
					set_delay_macro(val[7]);
					recompute_delay_status();
					break;
				case 0x51:	/* Delay Pre-LPF */
					delay_status.pre_lpf = val[7];
					recompute_delay_status();
					break;
				case 0x52:	/* Delay Time Center */
					delay_status.time_center = delay_time_center_table[val[7]];
					recompute_delay_status();
					break;
				case 0x53:	/* Delay Time Ratio Left */
					if(val[7] == 0) {val[7] = 1;}
					delay_status.time_ratio_left = (double)val[7] / 24;
					recompute_delay_status();
					break;
				case 0x54:	/* Delay Time Ratio Right */
					if(val[7] == 0) {val[7] = 1;}
					delay_status.time_ratio_right = (double)val[7] / 24;
					recompute_delay_status();
					break;
				case 0x55:	/* Delay Level Center */
					delay_status.level_center = val[7];
					recompute_delay_status();
					break;
				case 0x56:	/* Delay Level Left */
					delay_status.level_left = val[7];
					recompute_delay_status();
					break;
				case 0x57:	/* Delay Level Right */
					delay_status.level_right = val[7];
					recompute_delay_status();
					break;
				case 0x58:	/* Delay Level */
					delay_status.level = val[7];
					recompute_delay_status();
					break;
				case 0x59:	/* Delay Feedback */
					delay_status.feedback = val[7];
					recompute_delay_status();
					break;
				case 0x5A:	/* Delay Send Level to Reverb */
					delay_status.send_reverb = val[7];
					recompute_delay_status();
					break;
				default:
					cmsg(CMSG_INFO,VERB_NOISY,"Unsupported GS SysEx. (ADDR:%02X %02X %02X VAL:%02X %02X)",val[4],val[5],val[6],val[7],val[8]);
					break;
				}
			} else if((addr & 0xFFFF00) == 0x400200) {
				switch(addr & 0xFF) {	/* EQ Parameter */
				case 0x00:
					if(!opt_eq_control) {break;}
					eq_status.low_freq = val[7];
					recompute_eq_status();
					cmsg(CMSG_INFO,VERB_NOISY,"EQ LOW FREQ (%d)",val[7]);
					break;
				case 0x01:
					if(!opt_eq_control) {break;}
					eq_status.low_gain = val[7];
					recompute_eq_status();
					cmsg(CMSG_INFO,VERB_NOISY,"EQ LOW GAIN (%d dB)",val[7] - 0x40);
					break;
				case 0x02:
					if(!opt_eq_control) {break;}
					eq_status.high_freq = val[7];
					recompute_eq_status();
					cmsg(CMSG_INFO,VERB_NOISY,"EQ HIGH FREQ (%d)",val[7]);
					break;
				case 0x03:
					if(!opt_eq_control) {break;}
					eq_status.high_gain = val[7];
					recompute_eq_status();
					cmsg(CMSG_INFO,VERB_NOISY,"EQ HIGH GAIN (%d dB)",val[7] - 0x40);
					break;
				default:
					cmsg(CMSG_INFO,VERB_NOISY,"Unsupported GS SysEx. (ADDR:%02X %02X %02X VAL:%02X %02X)",val[4],val[5],val[6],val[7],val[8]);
					break;
				}
			} else if((addr & 0xFFFF00) == 0x400300) {
				switch(addr & 0xFF) {	/* Insertion Effect Parameter */
				case 0x00:
					if(!opt_insertion_effect) {break;}
					insertion_effect.type_msb = val[7];
					insertion_effect.type_lsb = val[8];
					insertion_effect.type = ((int32)val[7] << 8) | (int32)val[8];
					set_insertion_effect_default_parameter();
					recompute_insertion_effect();
					cmsg(CMSG_INFO,VERB_NOISY,"EFX TYPE (%02X %02X)",val[7],val[8]);
					break;
				case 0x03:
					insertion_effect.parameter[0] = val[7];
					recompute_insertion_effect();
					break;
				case 0x04:
					insertion_effect.parameter[1] = val[7];
					recompute_insertion_effect();
					break;
				case 0x05:
					insertion_effect.parameter[2] = val[7];
					recompute_insertion_effect();
					break;
				case 0x06:
					insertion_effect.parameter[3] = val[7];
					recompute_insertion_effect();
					break;
				case 0x07:
					insertion_effect.parameter[4] = val[7];
					recompute_insertion_effect();
					break;
				case 0x08:
					insertion_effect.parameter[5] = val[7];
					recompute_insertion_effect();
					break;
				case 0x09:
					insertion_effect.parameter[6] = val[7];
					recompute_insertion_effect();
					break;
				case 0x0A:
					insertion_effect.parameter[7] = val[7];
					recompute_insertion_effect();
					break;
				case 0x0B:
					insertion_effect.parameter[8] = val[7];
					recompute_insertion_effect();
					break;
				case 0x0C:
					insertion_effect.parameter[9] = val[7];
					recompute_insertion_effect();
					break;
				case 0x0D:
					insertion_effect.parameter[10] = val[7];
					recompute_insertion_effect();
					break;
				case 0x0E:
					insertion_effect.parameter[11] = val[7];
					recompute_insertion_effect();
					break;
				case 0x0F:
					insertion_effect.parameter[12] = val[7];
					recompute_insertion_effect();
					break;
				case 0x10:
					insertion_effect.parameter[13] = val[7];
					recompute_insertion_effect();
					break;
				case 0x11:
					insertion_effect.parameter[14] = val[7];
					recompute_insertion_effect();
					break;
				case 0x12:
					insertion_effect.parameter[15] = val[7];
					recompute_insertion_effect();
					break;
				case 0x13:
					insertion_effect.parameter[16] = val[7];
					recompute_insertion_effect();
					break;
				case 0x14:
					insertion_effect.parameter[17] = val[7];
					recompute_insertion_effect();
					break;
				case 0x15:
					insertion_effect.parameter[18] = val[7];
					recompute_insertion_effect();
					break;
				case 0x16:
					insertion_effect.parameter[19] = val[7];
					recompute_insertion_effect();
					break;
				case 0x17:
					insertion_effect.send_reverb = val[7];
					recompute_insertion_effect();
					break;
				case 0x18:
					insertion_effect.send_chorus = val[7];
					recompute_insertion_effect();
					break;
				case 0x19:
					insertion_effect.send_delay = val[7];
					recompute_insertion_effect();
					break;
				case 0x1B:
					insertion_effect.control_source1 = val[7];
					recompute_insertion_effect();
					break;
				case 0x1C:
					insertion_effect.control_depth1 = val[7];
					recompute_insertion_effect();
					break;
				case 0x1D:
					insertion_effect.control_source2 = val[7];
					recompute_insertion_effect();
					break;
				case 0x1E:
					insertion_effect.control_depth2 = val[7];
					recompute_insertion_effect();
					break;
				case 0x1F:
					insertion_effect.send_eq_switch = val[7];
					recompute_insertion_effect();
					break;
				default:
					cmsg(CMSG_INFO,VERB_NOISY,"Unsupported GS SysEx. (ADDR:%02X %02X %02X VAL:%02X %02X)",val[4],val[5],val[6],val[7],val[8]);
					break;
				}
			} else if((addr & 0xFFF000) == 0x404000) {
				switch(addr & 0xFF) {
				case 0x00:	/* TONE MAP NUMBER */
					if(val[7] == 0) {
						TWMIDIEVENT( ME_TONE_BANK_LSB,p,channel[p].tone_map0_number,0);
					} else {
						TWMIDIEVENT( ME_TONE_BANK_LSB,p,val[7],0);
					}
					num_events ++;
					break;
				case 0x01:	/* TONE MAP-0 NUMBER */
					channel[p].tone_map0_number = val[7];
					break;
				case 0x20:	/* EQ ON/OFF */
					TWMIDIEVENT(ME_NRPN_MSB,p,0x30,0);
					TWMIDIEVENT(ME_NRPN_LSB,p,0x00,0);
					TWMIDIEVENT(ME_DATA_ENTRY_MSB,p,val[7],0);
					num_events += 3;
					break;
				case 0x22:	/* EFX ON/OFF */
					TWMIDIEVENT(ME_NRPN_MSB,p,0x30,0);
					TWMIDIEVENT(ME_NRPN_LSB,p,0x01,0);
					TWMIDIEVENT(ME_DATA_ENTRY_MSB,p,val[7],0);
					num_events += 3;
					break;
				default:
					cmsg(CMSG_INFO,VERB_NOISY,"Unsupported GS SysEx. (ADDR:%02X %02X %02X VAL:%02X %02X)",val[4],val[5],val[6],val[7],val[8]);
					break;
				}
			}
			break;
		case 0x41:
			switch(addr & 0xF00) {
			case 0x100:	/* Play Note */
				TWMIDIEVENT(ME_NRPN_MSB,dp,0x18,0);
				TWMIDIEVENT(ME_NRPN_LSB,dp,val[6],0);
				TWMIDIEVENT(ME_DATA_ENTRY_MSB,dp,val[7],0);
				num_events += 3;
				break;
			case 0x200:
				TWMIDIEVENT(ME_NRPN_MSB,dp,0x1A,0);
				TWMIDIEVENT(ME_NRPN_LSB,dp,val[6],0);
				TWMIDIEVENT(ME_DATA_ENTRY_MSB,dp,val[7],0);
				num_events += 3;
				break;
			case 0x400:
				TWMIDIEVENT(ME_NRPN_MSB,dp,0x1C,0);
				TWMIDIEVENT(ME_NRPN_LSB,dp,val[6],0);
				TWMIDIEVENT(ME_DATA_ENTRY_MSB,dp,val[7],0x1);
				num_events += 3;
				break;
			case 0x500:
				TWMIDIEVENT(ME_NRPN_MSB,dp,0x1D,0);
				TWMIDIEVENT(ME_NRPN_LSB,dp,val[6],0);
				TWMIDIEVENT(ME_DATA_ENTRY_MSB,dp,val[7],0);
				num_events += 3;
				break;
			case 0x600:
				TWMIDIEVENT(ME_NRPN_MSB,dp,0x1E,0);
				TWMIDIEVENT(ME_NRPN_LSB,dp,val[6],0);
				TWMIDIEVENT(ME_DATA_ENTRY_MSB,dp,val[7],0);
				num_events += 3;
				break;
			case 0x900:
				TWMIDIEVENT(ME_NRPN_MSB,dp,0x1F,0);
				TWMIDIEVENT(ME_NRPN_LSB,dp,val[6],0);
				TWMIDIEVENT(ME_DATA_ENTRY_MSB,dp,val[7],0);
				num_events += 3;
				break;
			default:
				cmsg(CMSG_INFO,VERB_NOISY,"Unsupported GS SysEx. (ADDR:%02X %02X %02X VAL:%02X %02X)",val[4],val[5],val[6],val[7],val[8]);
				break;
			}
			break;
#if 0
		case 0x20:	/* User Instrument */
			switch(addr & 0xF00) {
				case 0x000:	/* Source Map */
					userinst_map = val[7];
					break;
				case 0x100:	/* Source Bank */
					userinst_bank = val[7];
					break;
#if !defined(TIMIDITY_TOOLS)
				case 0x200:	/* Source Prog */
					if(tonebank[64+udn]->tone[val[6]].name) {free_tone_bank_element(1,64+udn,val[6]);}
					if(tonebank[userinst_bank]) {
						if(tonebank[userinst_bank]->tone[val[7]].name) {
							memcpy(&tonebank[64+udn]->tone[val[6]],&tonebank[userinst_bank]->tone[val[7]],sizeof(ToneBankElement));
							dup_tone_bank_element(1,64+udn,val[6]);
							cmsg(CMSG_INFO,VERB_NOISY,"User Instrument (%d %d -> %d %d)",userinst_bank,val[7],udn+64,val[6]);
						} else if(tonebank[0]->tone[val[7]].name) {
							memcpy(&tonebank[64+udn]->tone[val[6]],&tonebank[0]->tone[val[7]],sizeof(ToneBankElement));
							dup_tone_bank_element(1,64+udn,val[6]);
							cmsg(CMSG_INFO,VERB_NOISY,"User Instrument (%d %d -> %d %d)",0,val[7],udn+64,val[6]);
						}
					}
					userinst_bank = 0;
					userinst_map = 0;
					break;
#endif
				default:
					cmsg(CMSG_INFO,VERB_NOISY,"Unsupported GS SysEx. (ADDR:%02X %02X %02X VAL:%02X %02X)",val[4],val[5],val[6],val[7],val[8]);
					break;
			}
			break;
#endif
		case 0x21:	/* User Drumset */
			switch(addr & 0xF00) {
				case 0x100:	/* Play Note */
					TWMIDIEVENT(ME_NRPN_MSB,dp,0x18,0);
					TWMIDIEVENT(ME_NRPN_LSB,dp,val[6],0);
					TWMIDIEVENT(ME_DATA_ENTRY_MSB,dp,val[7],0x1);
					num_events += 3;
					break;
				case 0x200:	/* Level */
					TWMIDIEVENT(ME_NRPN_MSB,dp,0x1A,0);
					TWMIDIEVENT(ME_NRPN_LSB,dp,val[6],0);
					TWMIDIEVENT(ME_DATA_ENTRY_MSB,dp,val[7],0);
					num_events += 3;
					break;
				case 0x300:	/* Assign Group */
					userdrum_assign_group[val[6]] = val[7];
					if(val[7] != 0) {
						scan_userdrum_assign_group(64+udn,val[7]);
						cmsg(CMSG_INFO,VERB_NOISY,"User Drumset - Assign Group(%d %d GROUP:%d)",udn+64,val[6],val[7]);
					}
					break;
				case 0x400:	/* Panpot */
					TWMIDIEVENT(ME_NRPN_MSB,dp,0x1C,0);
					TWMIDIEVENT(ME_NRPN_LSB,dp,val[6],0);
					TWMIDIEVENT(ME_DATA_ENTRY_MSB,dp,val[7],0);
					num_events += 3;
					break;
				case 0x500:	/* Reverb Send Level */
					TWMIDIEVENT(ME_NRPN_MSB,dp,0x1D,0);
					TWMIDIEVENT(ME_NRPN_LSB,dp,val[6],0);
					TWMIDIEVENT(ME_DATA_ENTRY_MSB,dp,val[7],0);
					num_events += 3;
					break;
				case 0x600:	/* Chorus Send Level */
					TWMIDIEVENT(ME_NRPN_MSB,dp,0x1E,0);
					TWMIDIEVENT(ME_NRPN_LSB,dp,val[6],0);
					TWMIDIEVENT(ME_DATA_ENTRY_MSB,dp,val[7],0);
					num_events += 3;
					break;
				case 0x900:	/* Delay Send Level */
					TWMIDIEVENT(ME_NRPN_MSB,dp,0x1F,0);
					TWMIDIEVENT(ME_NRPN_LSB,dp,val[6],0);
					TWMIDIEVENT(ME_DATA_ENTRY_MSB,dp,val[7],0);
					num_events += 3;
					break;
				case 0xA00:	/* Source Map */
					userdrum_map = val[7];
					break;
				case 0xB00:	/* Source Prog */
					userdrum_prog = val[7];
					break;
#if !defined(TIMIDITY_TOOLS)
				case 0xC00:	/* Source Note */
					if(drumset[64+udn]->tone[val[6]].name) {free_tone_bank_element(1,64+udn,val[6]);}
					if(drumset[userdrum_prog]) {
						if(drumset[userdrum_prog]->tone[val[7]].name) {
							memcpy(&drumset[64+udn]->tone[val[6]],&drumset[userdrum_prog]->tone[val[7]],sizeof(ToneBankElement));
							dup_tone_bank_element(1,64+udn,val[6]);
							cmsg(CMSG_INFO,VERB_NOISY,"User Drumset (%d %d -> %d %d)",userdrum_prog,val[7],udn+64,val[6]);
						} else if(drumset[0]->tone[val[7]].name) {
							memcpy(&drumset[64+udn]->tone[val[6]],&drumset[0]->tone[val[7]],sizeof(ToneBankElement));
							dup_tone_bank_element(1,64+udn,val[6]);
							cmsg(CMSG_INFO,VERB_NOISY,"User Drumset (%d %d -> %d %d)",0,val[7],udn+64,val[6]);
						}
					}
					userdrum_prog = 0;
					userdrum_map = 0;
					break;
#endif
				default:
					cmsg(CMSG_INFO,VERB_NOISY,"Unsupported GS SysEx. (ADDR:%02X %02X %02X VAL:%02X %02X)",val[4],val[5],val[6],val[7],val[8]);
					break;
			}
			break;
		}
    }

    return(num_events);
}


