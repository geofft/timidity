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

    alsaseq_c.c - ALSA sequencer server interface
        Copyright (c) 2000  Takashi Iwai <tiwai@suse.de>

    This interface provides an ALSA sequencer client which receives 
    events and plays it in real-time.  On this mode, TiMidity works
    as a software (quasi-)real-time MIDI synth engine.

    See doc/C/README.alsaseq for more details.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/time.h>
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <math.h>
#include <signal.h>

#if HAHE_ALSA_ASOUNDLIB_H
#include <alsa/asoundlib.h>
#else
#include <sys/asoundlib.h>
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


#define NUM_PORTS	4	/* number of ports;
				 * this should be configurable via command line..
				 */

#define TICKTIME_HZ	100

struct seq_context {
	snd_seq_t *handle;	/* The snd_seq handle to /dev/snd/seq */
	int client;		/* The client associated with this context */
	int port[NUM_PORTS];	/* created sequencer ports */
	int fd;			/* The file descriptor */
	int used;		/* number of current connection */
	int active;		/* */
};

static struct seq_context alsactx;

#if SND_LIB_MINOR >= 6
/* !! this is a dirty hack.  not sure to work in future !! */
static int snd_seq_file_descriptor(snd_seq_t *handle)
{
	int pfds = snd_seq_poll_descriptors_count(handle, POLLIN);
	if (pfds > 0) {
		struct pollfd pfd;
		if (snd_seq_poll_descriptors(handle, &pfd, 1, POLLIN) >= 0)
			return pfd.fd;
	}
	return -ENXIO;
}

static int alsa_seq_open(snd_seq_t **seqp)
{
	return snd_seq_open(seqp, "hw", SND_SEQ_OPEN_INPUT, 0);
}

static int alsa_create_port(snd_seq_t *seq, int index)
{
	snd_seq_port_info_t *pinfo;
	char name[32];
	int port;

	sprintf(name, "TiMidity port %d", index);
	port = snd_seq_create_simple_port(seq, name,
					  SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,
					  SND_SEQ_PORT_TYPE_MIDI_GENERIC);
	if (port < 0) {
		fprintf(stderr, "error in snd_seq_create_simple_port\n");
		return -1;
	}
	return port;
}

#else
static int alsa_seq_open(snd_seq_t **seqp)
{
	return snd_seq_open(seqp, SND_SEQ_OPEN_IN);
}

static int alsa_create_port(snd_seq_t *seq, int index)
{
	snd_seq_port_info_t pinfo;

	memset(&pinfo, 0, sizeof(pinfo));
	sprintf(pinfo.name, "TiMidity port %d", index);
	pinfo.capability = SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE;
	pinfo.type = SND_SEQ_PORT_TYPE_MIDI_GENERIC;
	strcpy(pinfo.group, SND_SEQ_GROUP_DEVICE);
	if (snd_seq_create_port(alsactx.handle, &pinfo) < 0) {
		fprintf(stderr, "error in snd_seq_create_simple_port\n");
		return -1;
	}
	return pinfo.port;
}

#endif

static int ctl_open(int using_stdin, int using_stdout);
static void ctl_close(void);
static int ctl_read(int32 *valp);
static int cmsg(int type, int verbosity_level, char *fmt, ...);
static void ctl_event(CtlEvent *e);
static void ctl_pass_playing_list(int n, char *args[]);

/**********************************/
/* export the interface functions */

#define ctl alsaseq_control_mode

ControlMode ctl=
{
    "ALSA sequencer interface", 'A',
    1,0,0,
    0,
    ctl_open,
    ctl_close,
    ctl_pass_playing_list,
    ctl_read,
    cmsg,
    ctl_event
};

static int time_advance;
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

static int cmsg(int type, int verbosity_level, char *fmt, ...)
{
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

    return 0;
}

static void ctl_event(CtlEvent *e)
{
}

static RETSIGTYPE sig_timeout(int sig)
{
    signal(SIGALRM, sig_timeout); /* For SysV base */
    /* Expect EINTR */
}

static void doit(struct seq_context *ctxp);
static int do_sequencer(struct seq_context *ctxp);
static int start_sequencer(struct seq_context *ctxp);
static void stop_sequencer(struct seq_context *ctxp);
static void server_reset(void);

/* reset all when SIGHUP is received */
static RETSIGTYPE sig_reset(int sig)
{
	if (alsactx.active) {
		stop_sequencer(&alsactx);
		server_reset();
	}
	signal(SIGHUP, sig_reset);
}

/*
 * set the process to realtime privs
 */
static int set_realtime_priority(void)
{
	struct sched_param schp;

        memset(&schp, 0, sizeof(schp));
        schp.sched_priority = sched_get_priority_max(SCHED_FIFO);

        if (sched_setscheduler(0, SCHED_FIFO, &schp) != 0) {
		printf("can't set sched_setscheduler - using normal priority\n");
                return -1;
        }
	printf("set SCHED_FIFO\n");
        return 0;
}

static void ctl_pass_playing_list(int n, char *args[])
{
	double btime;
	int i, j;

#ifdef SIGPIPE
	signal(SIGPIPE, SIG_IGN);    /* Handle broken pipe */
#endif /* SIGPIPE */

	printf("TiMidity starting in ALSA server mode\n");

	set_realtime_priority();

	if (alsa_seq_open(&alsactx.handle) < 0) {
		fprintf(stderr, "error in snd_seq_open\n");
		return;
	}
	alsactx.client = snd_seq_client_id(alsactx.handle);
	alsactx.fd = snd_seq_file_descriptor(alsactx.handle);
	snd_seq_set_client_pool_input(alsactx.handle, 1000); /* enough? */

	printf("Opening sequencer port:");
	for (i = 0; i < NUM_PORTS; i++) {
		int port;
		port = alsa_create_port(alsactx.handle, i);
		if (port < 0)
			return;
		alsactx.port[i] = port;
		printf(" %d:%d", alsactx.client, alsactx.port[i]);
	}
	printf("\n");

	alsactx.used = 0;
	alsactx.active = 0;

	opt_realtime_playing = 2; /* Enable loading patch while playing */
	allocate_cache_size = 0; /* Don't use pre-calclated samples */
	current_keysig = current_temper_keysig = opt_init_keysig;
	note_key_offset = 0;

	/* set the audio queue size as minimum as possible, since
	 * we don't have to use audio queue..
	 */
	play_mode->acntl(PM_REQ_GETFRAGSIZ, &time_advance);
	if (!(play_mode->encoding & PE_MONO))
		time_advance >>= 1;
	if (play_mode->encoding & PE_16BIT)
		time_advance >>= 1;
	btime = (double)time_advance / play_mode->rate;
	btime *= 1.01; /* to be sure */
	aq_set_soft_queue(btime, 0.0);

	alarm(0);
	signal(SIGALRM, sig_timeout);
	signal(SIGINT, safe_exit);
	signal(SIGTERM, safe_exit);
	signal(SIGHUP, sig_reset);

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

	if (ctl.flags & CTLF_DAEMONIZE)
	{
		int pid = fork();
		FILE *pidf;
		switch (pid)
		{
			case 0:			// child is the daemon
				break;
			case -1:		// error status return
				exit(7);
			default:		// no error, doing well
				if ((pidf = fopen( "/var/run/timidity.pid", "w" )) != NULL )
					fprintf( pidf, "%d\n", pid );
				exit(0);
		}
	}

	for (;;) {
		server_reset();
		doit(&alsactx);
	}
}

static void seq_play_event(MidiEvent *ev)
{
  //JAVE  make channel -Q channels quiet, modified some code from readmidi.c
  int gch;
  gch = GLOBAL_CHANNEL_EVENT_TYPE(ev->type);

  if(gch || !IS_SET_CHANNELMASK(quietchannels, ev->channel)){
    //if its a global event or not a masked event
    ev->time = event_time_offset;
    play_event(ev);
  }
}

static void stop_playing(void)
{
	if(upper_voices) {
		MidiEvent ev;
		ev.type = ME_EOT;
		ev.a = 0;
		ev.b = 0;
		seq_play_event(&ev);
		aq_flush(0);
	}
}

static void doit(struct seq_context *ctxp)
{
	for (;;) {
		while (snd_seq_event_input_pending(ctxp->handle, 1)) {
			if (do_sequencer(ctxp))
				goto __done;
		}
		if (ctxp->active) {
			double fill_time;
			MidiEvent ev;

			/*event_time_offset += play_mode->rate / TICKTIME_HZ;*/
			event_time_offset += time_advance;
			ev.time = event_time_offset;
			ev.type = ME_NONE;
			play_event(&ev);
			aq_fill_nonblocking();
		} else {
			fd_set rfds;
			FD_ZERO(&rfds);
			FD_SET(ctxp->fd, &rfds);
			if (select(ctxp->fd + 1, &rfds, NULL, NULL, NULL) < 0)
				goto __done;
		}
	}

__done:
	if (ctxp->active) {
		stop_sequencer(ctxp);
	}
}

static void server_reset(void)
{
	readmidi_read_init();
	playmidi_stream_init();
	if (free_instruments_afterwards)
		free_instruments(0);
	reduce_voice_threshold = 0; /* Disable auto reduction voice */
	event_time_offset = 0;
}

static int start_sequencer(struct seq_context *ctxp)
{
	if (play_mode->open_output() < 0) {
		ctl.cmsg(CMSG_FATAL, VERB_NORMAL,
			 "Couldn't open %s (`%c')",
			 play_mode->id_name, play_mode->id_character);
		return 0;
	}
	ctxp->active = 1;
	return 1;
}

static void stop_sequencer(struct seq_context *ctxp)
{
	stop_playing();
	play_mode->close_output();
	free_instruments(0);
	free_global_mblock();
	ctxp->used = 0;
	ctxp->active = 0;
}

#define NOTE_CHAN(ev)	((ev)->dest.port * 16 + (ev)->data.note.channel)
#define CTRL_CHAN(ev)	((ev)->dest.port * 16 + (ev)->data.control.channel)

static int do_sequencer(struct seq_context *ctxp)
{
	int n, ne, i;
	MidiEvent ev, evm[16];
	snd_seq_event_t *aevp;

	n = snd_seq_event_input(ctxp->handle, &aevp);
	if (n < 0 || aevp == NULL)
		return 0;

	switch(aevp->type) {
	case SND_SEQ_EVENT_NOTEON:
		ev.channel = NOTE_CHAN(aevp);
		ev.a       = aevp->data.note.note;
		ev.b       = aevp->data.note.velocity;
		if (ev.b == 0)
			ev.type = ME_NOTEOFF;
		else
			ev.type = ME_NOTEON;
		seq_play_event(&ev);
		break;

	case SND_SEQ_EVENT_NOTEOFF:
		ev.channel = NOTE_CHAN(aevp);
		ev.a       = aevp->data.note.note;
		ev.b       = aevp->data.note.velocity;
		ev.type = ME_NOTEOFF;
		seq_play_event(&ev);
		break;

	case SND_SEQ_EVENT_KEYPRESS:
		ev.channel = NOTE_CHAN(aevp);
		ev.a       = aevp->data.note.note;
		ev.b       = aevp->data.note.velocity;
		ev.type = ME_KEYPRESSURE;
		seq_play_event(&ev);
		break;

	case SND_SEQ_EVENT_PGMCHANGE:
		ev.channel = CTRL_CHAN(aevp);
		ev.a = aevp->data.control.value;
		ev.type = ME_PROGRAM;
		seq_play_event(&ev);
		break;

	case SND_SEQ_EVENT_CONTROLLER:
		if(convert_midi_control_change(CTRL_CHAN(aevp),
					       aevp->data.control.param,
					       aevp->data.control.value,
					       &ev))
			seq_play_event(&ev);
		break;

	case SND_SEQ_EVENT_CONTROL14:
		if (aevp->data.control.param < 0 || aevp->data.control.param >= 32)
			break;
		if (! convert_midi_control_change(CTRL_CHAN(aevp),
						  aevp->data.control.param,
						  (aevp->data.control.value >> 7) & 0x7f,
						  &ev))
			break;
		seq_play_event(&ev);
		if (! convert_midi_control_change(CTRL_CHAN(aevp),
						  aevp->data.control.param + 32,
						  aevp->data.control.value & 0x7f,
						  &ev))
			break;
		seq_play_event(&ev);
		break;
		    
	case SND_SEQ_EVENT_PITCHBEND:
		ev.type    = ME_PITCHWHEEL;
		ev.channel = CTRL_CHAN(aevp);
		aevp->data.control.value += 0x2000;
		ev.a       = (aevp->data.control.value) & 0x7f;
		ev.b       = (aevp->data.control.value>>7) & 0x7f;
		seq_play_event(&ev);
		break;

	case SND_SEQ_EVENT_CHANPRESS:
		ev.type    = ME_CHANNEL_PRESSURE;
		ev.channel = CTRL_CHAN(aevp);
		ev.a       = aevp->data.control.value;
		seq_play_event(&ev);
		break;
		
	case SND_SEQ_EVENT_NONREGPARAM:
		/* Break it back into its controler values */
		ev.type = ME_NRPN_MSB;
		ev.channel = CTRL_CHAN(aevp);
		ev.a = (aevp->data.control.param >> 7) & 0x7f;
		seq_play_event(&ev);
		ev.type = ME_NRPN_LSB;
		ev.channel = CTRL_CHAN(aevp);
		ev.a = aevp->data.control.param & 0x7f;
		seq_play_event(&ev);
		ev.type = ME_DATA_ENTRY_MSB;
		ev.channel = CTRL_CHAN(aevp);
		ev.a = (aevp->data.control.value >> 7) & 0x7f;
		seq_play_event(&ev);
		ev.type = ME_DATA_ENTRY_LSB;
		ev.channel = CTRL_CHAN(aevp);
		ev.a = aevp->data.control.value & 0x7f;
		seq_play_event(&ev);
		break;

	case SND_SEQ_EVENT_REGPARAM:
		/* Break it back into its controler values */
		ev.type = ME_RPN_MSB;
		ev.channel = CTRL_CHAN(aevp);
		ev.a = (aevp->data.control.param >> 7) & 0x7f;
		seq_play_event(&ev);
		ev.type = ME_RPN_LSB;
		ev.channel = CTRL_CHAN(aevp);
		ev.a = aevp->data.control.param & 0x7f;
		seq_play_event(&ev);
		ev.type = ME_DATA_ENTRY_MSB;
		ev.channel = CTRL_CHAN(aevp);
		ev.a = (aevp->data.control.value >> 7) & 0x7f;
		seq_play_event(&ev);
		ev.type = ME_DATA_ENTRY_LSB;
		ev.channel = CTRL_CHAN(aevp);
		ev.a = aevp->data.control.value & 0x7f;
		seq_play_event(&ev);
		break;

	case SND_SEQ_EVENT_SYSEX:
		if (parse_sysex_event(aevp->data.ext.ptr + 1,
				 aevp->data.ext.len - 1, &ev))
			seq_play_event(&ev);
		if (ne = parse_sysex_event_multi(aevp->data.ext.ptr + 1,
				aevp->data.ext.len - 1, evm))
			for (i = 0; i < ne; i++)
				seq_play_event(&evm[i]);
		break;

#if SND_LIB_MINOR >= 6
#define snd_seq_addr_equal(a,b)	((a)->client == (b)->client && (a)->port == (b)->port)
	case SND_SEQ_EVENT_PORT_SUBSCRIBED:
		if (snd_seq_addr_equal(&aevp->data.connect.dest, &aevp->dest)) {
			if (! ctxp->active) {
				if (! start_sequencer(ctxp)) {
					snd_seq_free_event(aevp);
					return 0;
				}
			}
			ctxp->used++;
		}
		break;

	case SND_SEQ_EVENT_PORT_UNSUBSCRIBED:
		if (snd_seq_addr_equal(&aevp->data.connect.dest, &aevp->dest)) {
			if (ctxp->active) {
				ctxp->used--;
				if (ctxp->used <= 0) {
					snd_seq_free_event(aevp);
					return 1; /* quit now */
				}
			}
		}
		break;
#else
	case SND_SEQ_EVENT_PORT_USED:
		if (! ctxp->active) {
			if (! start_sequencer(ctxp)) {
				snd_seq_free_event(aevp);
				return 0;
			}
		}
		ctxp->used++;
		break;

	case SND_SEQ_EVENT_PORT_UNUSED:
		if (ctxp->active) {
			ctxp->used--;
			if (ctxp->used <= 0) {
				snd_seq_free_event(aevp);
				return 1; /* quit now */
			}
		}
		break;
#endif
		
	default:
		/*printf("Unsupported event %d\n", aevp->type);*/
		break;
	}
	snd_seq_free_event(aevp);
	return 0;
}

/*
 * interface_<id>_loader();
 */
ControlMode *interface_A_loader(void)
{
    return &ctl;
}
