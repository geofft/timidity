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
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include "timidity.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "readmidi.h"
#include "output.h"
#include "controls.h"
#include "strtab.h"
#include "memb.h"
#include "zip.h"
#include "arc.h"
#include "mod.h"
#include "wrd.h"
#include "tables.h"
#include "reverb.h"
#include <math.h>

/* rcp.c */
int read_rcp_file(struct timidity_file *tf, char *magic0, char *fn);

/* mld.c */
extern int read_mfi_file(struct timidity_file *tf);
extern char *get_mfi_file_title(struct timidity_file *tf);

#define MAX_MIDI_EVENT ((MAX_SAFE_MALLOC_SIZE / sizeof(MidiEvent)) - 1)
#define MARKER_START_CHAR	'('
#define MARKER_END_CHAR		')'
#define REDUCE_CHANNELS		16

static uint8 rhythm_part[2];

enum
{
    CHORUS_ST_NOT_OK = 0,
    CHORUS_ST_OK
};

#ifdef ALWAYS_TRACE_TEXT_META_EVENT
int opt_trace_text_meta_event = 1;
#else
int opt_trace_text_meta_event = 0;
#endif /* ALWAYS_TRACE_TEXT_META_EVENT */

FLOAT_T tempo_adjust = 1.0;
int opt_default_mid = 0;
int opt_system_mid = 0;
int ignore_midi_error = 1;
ChannelBitMask quietchannels;
struct midi_file_info *current_file_info = NULL;
int readmidi_error_flag = 0;
int readmidi_wrd_mode = 0;
int play_system_mode = DEFAULT_SYSTEM_MODE;

static MidiEventList *evlist, *current_midi_point;
static int32 event_count;
static MBlockList mempool;
static StringTable string_event_strtab;
static int current_read_track;
static int karaoke_format, karaoke_title_flag;
static struct midi_file_info *midi_file_info = NULL;
static char **string_event_table = NULL;
static int    string_event_table_size = 0;
int    default_channel_program[256];
static MidiEvent timesig[256];

void init_delay_status();
void recompute_delay_status();
void set_delay_macro(int macro);

void init_chorus_status();
void recompute_chorus_status();
void set_chorus_macro(int macro);

void init_reverb_status();
void recompute_reverb_status();
void set_reverb_macro(int macro);

void init_eq_status();
void recompute_eq_status();

void init_insertion_effect_status();
void set_insertion_effect_default_parameter();
void recompute_insertion_effect();

void init_channel_layer(int);
void remove_channel_layer(int);
void add_channel_layer(int, int);

/* MIDI ports will be merged in several channels in the future. */
int midi_port_number;

/* These would both fit into 32 bits, but they are often added in
   large multiples, so it's simpler to have two roomy ints */
static int32 sample_increment, sample_correction; /*samples per MIDI delta-t*/

#define SETMIDIEVENT(e, at, t, ch, pa, pb) \
    { (e).time = (at); (e).type = (t); \
      (e).channel = (uint8)(ch); (e).a = (uint8)(pa); (e).b = (uint8)(pb); }

#define MIDIEVENT(at, t, ch, pa, pb) \
    { MidiEvent event; SETMIDIEVENT(event, at, t, ch, pa, pb); \
      readmidi_add_event(&event); }

#define MIDIEVENT_LAYER(at, t, ch, pa, pb) \
    { MidiEvent event; int _layer_cnt; \
	for(_layer_cnt = 0; channel[ch].channel_layer[_layer_cnt] != -1; _layer_cnt++) {	\
	SETMIDIEVENT(event, at, t, channel[ch].channel_layer[_layer_cnt], pa, pb); \
	readmidi_add_event(&event);}}

#if MAX_CHANNELS <= 16
#define MERGE_CHANNEL_PORT(ch) ((int)(ch))
#define MERGE_CHANNEL_PORT2(ch, port) ((int)(ch))
#else
#define MERGE_CHANNEL_PORT(ch) ((int)(ch) | (midi_port_number << 4))
#define MERGE_CHANNEL_PORT2(ch, port) ((int)(ch) | ((int)port << 4))
#endif

#define alloc_midi_event() \
    (MidiEventList *)new_segment(&mempool, sizeof(MidiEventList))

typedef struct _UserDrumset {
	int8 bank;
	int8 prog;
	int8 play_note;
	int8 level;
	int8 assign_group;
	int8 pan;
	int8 reverb_send_level;
	int8 chorus_send_level;
	int8 rx_note_off;
	int8 rx_note_on;
	int8 delay_send_level;
	int8 source_map;
	int8 source_prog;
	int8 source_note;
	struct _UserDrumset *next;
} UserDrumset;

UserDrumset *userdrum_first = (UserDrumset *)NULL;
UserDrumset *userdrum_last = (UserDrumset *)NULL; 

void init_userdrum();
UserDrumset *get_userdrum(int bank, int prog);
void recompute_userdrum(int bank, int prog);
void recompute_userdrum_altassign(int bank,int group);

typedef struct _UserInstrument {
	int8 bank;
	int8 prog;
	int8 source_map;
	int8 source_bank;
	int8 source_prog;
	int8 vibrato_rate;
	int8 vibrato_depth;
	int8 cutoff_freq;
	int8 resonance;
	int8 env_attack;
	int8 env_decay;
	int8 env_release;
	int8 vibrato_delay;
	struct _UserInstrument *next;
} UserInstrument;

UserInstrument *userinst_first = (UserInstrument *)NULL;
UserInstrument *userinst_last = (UserInstrument *)NULL; 

void init_userinst();
UserInstrument *get_userinst(int bank, int prog);
void recompute_userinst(int bank, int prog);
void recompute_userinst_altassign(int bank,int group);

int32 readmidi_set_track(int trackno, int rewindp)
{
    current_read_track = trackno;
    memset(&chorus_status, 0, sizeof(chorus_status));
    if(karaoke_format == 1 && current_read_track == 2)
	karaoke_format = 2; /* Start karaoke lyric */
    else if(karaoke_format == 2 && current_read_track == 3)
	karaoke_format = 3; /* End karaoke lyric */
    midi_port_number = 0;

    if(evlist == NULL)
	return 0;
    if(rewindp)
	current_midi_point = evlist;
    else
    {
	/* find the last event in the list */
	while(current_midi_point->next != NULL)
	    current_midi_point = current_midi_point->next;
    }
    return current_midi_point->event.time;
}

void readmidi_add_event(MidiEvent *a_event)
{
    MidiEventList *newev;
    int32 at;

    if(event_count++ == MAX_MIDI_EVENT)
    {
	if(!readmidi_error_flag)
	{
	    readmidi_error_flag = 1;
	    ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		      "Maxmum number of events is exceeded");
	}
	return;
    }

    at = a_event->time;
    newev = alloc_midi_event();
    newev->event = *a_event;	/* assign by value!!! */
    if(at < 0)	/* for safety */
	at = newev->event.time = 0;

    if(at >= current_midi_point->event.time)
    {
	/* Forward scan */
	MidiEventList *next = current_midi_point->next;
	while (next && (next->event.time <= at))
	{
	    current_midi_point = next;
	    next = current_midi_point->next;
	}
	newev->prev = current_midi_point;
	newev->next = next;
	current_midi_point->next = newev;
	if (next)
	    next->prev = newev;
    }
    else
    {
	/* Backward scan -- symmetrical to the one above */
	MidiEventList *prev = current_midi_point->prev;
	while (prev && (prev->event.time > at)) {
	    current_midi_point = prev;
	    prev = current_midi_point->prev;
	}
	newev->prev = prev;
	newev->next = current_midi_point;
	current_midi_point->prev = newev;
	if (prev)
	    prev->next = newev;
    }
    current_midi_point = newev;
}

void readmidi_add_ctl_event(int32 at, int ch, int a, int b)
{
    MidiEvent ev;

    if(convert_midi_control_change(ch, a, b, &ev))
    {
	ev.time = at;
	readmidi_add_event(&ev);
    }
    else
	ctl->cmsg(CMSG_INFO, VERB_DEBUG, "(Control ch=%d %d: %d)", ch, a, b);
}

void readmidi_add_ctl_event_layer(int32 at, int ch, int a, int b)
{
	int i;
	for(i = 0; channel[ch].channel_layer[i] != -1; i++)
		readmidi_add_ctl_event(at, channel[ch].channel_layer[i], a, b);
}

char *readmidi_make_string_event(int type, char *string, MidiEvent *ev,
				 int cnv)
{
    char *text;
    int len;
    StringTableNode *st;
    int a, b;

    if(string_event_strtab.nstring == 0)
	put_string_table(&string_event_strtab, "", 0);
    else if(string_event_strtab.nstring == 0x7FFE)
    {
	SETMIDIEVENT(*ev, 0, type, 0, 0, 0);
	return NULL; /* Over flow */
    }
    a = (string_event_strtab.nstring & 0xff);
    b = ((string_event_strtab.nstring >> 8) & 0xff);

    len = strlen(string);
    if(cnv)
    {
	text = (char *)new_segment(&tmpbuffer, SAFE_CONVERT_LENGTH(len) + 1);
	code_convert(string, text + 1, SAFE_CONVERT_LENGTH(len), NULL, NULL);
    }
    else
    {
	text = (char *)new_segment(&tmpbuffer, len + 1);
	memcpy(text + 1, string, len);
	text[len + 1] = '\0';
    }

    st = put_string_table(&string_event_strtab, text, strlen(text + 1) + 1);
    reuse_mblock(&tmpbuffer);

    text = st->string;
    *text = type;
    SETMIDIEVENT(*ev, 0, type, 0, a, b);
    return text;
}

static char *readmidi_make_lcd_event(int type, const uint8 *data, MidiEvent *ev)
{
    char *text;
    int len;
    StringTableNode *st;
    int a, b, i;

    if(string_event_strtab.nstring == 0)
	put_string_table(&string_event_strtab, "", 0);
    else if(string_event_strtab.nstring == 0x7FFE)
    {
	SETMIDIEVENT(*ev, 0, type, 0, 0, 0);
	return NULL; /* Over flow */
    }
    a = (string_event_strtab.nstring & 0xff);
    b = ((string_event_strtab.nstring >> 8) & 0xff);

    len = 128;
    
	text = (char *)new_segment(&tmpbuffer, len + 2);

    for( i=0; i<64; i++){
	const char tbl[]= "0123456789ABCDEF";
	text[1+i*2  ]=tbl[data[i]>>4];
	text[1+i*2+1]=tbl[data[i]&0xF];
    }
    text[len + 1] = '\0';
    
    
    st = put_string_table(&string_event_strtab, text, strlen(text + 1) + 1);
    reuse_mblock(&tmpbuffer);

    text = st->string;
    *text = type;
    SETMIDIEVENT(*ev, 0, type, 0, a, b);
    return text;
}

/* Computes how many (fractional) samples one MIDI delta-time unit contains */
static void compute_sample_increment(int32 tempo, int32 divisions)
{
  double a;
  a = (double) (tempo) * (double) (play_mode->rate) * (65536.0/1000000.0) /
    (double)(divisions);

  sample_correction = (int32)(a) & 0xFFFF;
  sample_increment = (int32)(a) >> 16;

  ctl->cmsg(CMSG_INFO, VERB_DEBUG, "Samples per delta-t: %d (correction %d)",
       sample_increment, sample_correction);
}

/* Read variable-length number (7 bits per byte, MSB first) */
static int32 getvl(struct timidity_file *tf)
{
    int32 l;
    int c;

    errno = 0;
    l = 0;

    /* 1 */
    if((c = tf_getc(tf)) == EOF)
	goto eof;
    if(!(c & 0x80)) return l | c;
    l = (l | (c & 0x7f)) << 7;

    /* 2 */
    if((c = tf_getc(tf)) == EOF)
	goto eof;
    if(!(c & 0x80)) return l | c;
    l = (l | (c & 0x7f)) << 7;

    /* 3 */
    if((c = tf_getc(tf)) == EOF)
	goto eof;
    if(!(c & 0x80)) return l | c;
    l = (l | (c & 0x7f)) << 7;

    /* 4 */
    if((c = tf_getc(tf)) == EOF)
	goto eof;
    if(!(c & 0x80)) return l | c;

    /* Error */
    ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
	      "%s: Illigal Variable-length quantity format.",
	      current_filename);
    return -2;

  eof:
    if(errno)
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		  "%s: read_midi_event: %s",
		  current_filename, strerror(errno));
    else
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		  "Warning: %s: Too shorten midi file.",
		  current_filename);
    return -1;
}

static char *add_karaoke_title(char *s1, char *s2)
{
    char *ks;
    int k1, k2;

    if(s1 == NULL)
	return safe_strdup(s2);

    k1 = strlen(s1);
    k2 = strlen(s2);
    if(k2 == 0)
	return s1;
    ks = (char *)safe_malloc(k1 + k2 + 2);
    memcpy(ks, s1, k1);
    ks[k1++] = ' ';
    memcpy(ks + k1, s2, k2 + 1);
    free(s1);

    return ks;
}


/* Print a string from the file, followed by a newline. Any non-ASCII
   or unprintable characters will be converted to periods. */
static char *dumpstring(int type, int32 len, char *label, int allocp,
			struct timidity_file *tf)
{
    char *si, *so;
    int s_maxlen = SAFE_CONVERT_LENGTH(len);
    int llen, solen;

    if(len <= 0)
    {
	ctl->cmsg(CMSG_TEXT, VERB_VERBOSE, "%s", label);
	return NULL;
    }

    si = (char *)new_segment(&tmpbuffer, len + 1);
    so = (char *)new_segment(&tmpbuffer, s_maxlen);

    if(len != tf_read(si, 1, len, tf))
    {
	reuse_mblock(&tmpbuffer);
	return NULL;
    }
    si[len]='\0';

    if(type == 1 &&
       current_read_track == 1 &&
       current_file_info->format == 1 &&
       strncmp(si, "@KMIDI", 6) == 0)
	karaoke_format = 1;

    code_convert(si, so, s_maxlen, NULL, NULL);

    llen = strlen(label);
    solen = strlen(so);
    if(llen + solen >= MIN_MBLOCK_SIZE)
	so[MIN_MBLOCK_SIZE - llen - 1] = '\0';

    ctl->cmsg(CMSG_TEXT, VERB_VERBOSE, "%s%s", label, so);

    if(allocp)
    {
	so = safe_strdup(so);
	reuse_mblock(&tmpbuffer);
	return so;
    }
    reuse_mblock(&tmpbuffer);
    return NULL;
}

static uint16 gs_convert_master_vol(int vol)
{
    double v;

    if(vol >= 0x7f)
	return 0xffff;
    v = (double)vol * (0xffff/127.0);
    if(v >= 0xffff)
	return 0xffff;
    return (uint16)v;
}

static uint16 gm_convert_master_vol(uint16 v1, uint16 v2)
{
    return (((v1 & 0x7f) | ((v2 & 0x7f) << 7)) << 2) | 3;
}

static void check_chorus_text_start(void)
{
    if(chorus_status.status != CHORUS_ST_OK &&
       chorus_status.voice_reserve[17] &&
       chorus_status.macro[2] &&
       chorus_status.pre_lpf[2] &&
       chorus_status.level[2] &&
       chorus_status.feed_back[2] &&
       chorus_status.delay[2] &&
       chorus_status.rate[2] &&
       chorus_status.depth[2] &&
       chorus_status.send_level[2])
    {
	ctl->cmsg(CMSG_INFO, VERB_DEBUG, "Chorus text start");
	chorus_status.status = CHORUS_ST_OK;
    }
}

int convert_midi_control_change(int chn, int type, int val, MidiEvent *ev_ret)
{
    switch(type)
    {
      case   0: type = ME_TONE_BANK_MSB; break;
      case   1: type = ME_MODULATION_WHEEL; break;
      case   2: type = ME_BREATH; break;
      case   4: type = ME_FOOT; break;
      case   5: type = ME_PORTAMENTO_TIME_MSB; break;
      case   6: type = ME_DATA_ENTRY_MSB; break;
      case   7: type = ME_MAINVOLUME; break;
      case   8: type = ME_BALANCE; break;
      case  10: type = ME_PAN; break;
      case  11: type = ME_EXPRESSION; break;
      case  32: type = ME_TONE_BANK_LSB; break;
      case  37: type = ME_PORTAMENTO_TIME_LSB; break;
      case  38: type = ME_DATA_ENTRY_LSB; break;
      case  64: type = ME_SUSTAIN; break;
      case  65: type = ME_PORTAMENTO; break;
      case  66: type = ME_SOSTENUTO; break;
      case  67: type = ME_SOFT_PEDAL; break;
	  case  68: type = ME_LEGATO_FOOTSWITCH; break;
	  case  69: type = ME_HOLD2; break;
      case  71: type = ME_HARMONIC_CONTENT; break;
      case  72: type = ME_RELEASE_TIME; break;
      case  73: type = ME_ATTACK_TIME; break;
      case  74: type = ME_BRIGHTNESS; break;
      case  84: type = ME_PORTAMENTO_CONTROL; break;
      case  91: type = ME_REVERB_EFFECT; break;
      case  92: type = ME_TREMOLO_EFFECT; break;
      case  93: type = ME_CHORUS_EFFECT; break;
      case  94: type = ME_CELESTE_EFFECT; break;
      case  95: type = ME_PHASER_EFFECT; break;
      case  96: type = ME_RPN_INC; break;
      case  97: type = ME_RPN_DEC; break;
      case  98: type = ME_NRPN_LSB; break;
      case  99: type = ME_NRPN_MSB; break;
      case 100: type = ME_RPN_LSB; break;
      case 101: type = ME_RPN_MSB; break;
      case 120: type = ME_ALL_SOUNDS_OFF; break;
      case 121: type = ME_RESET_CONTROLLERS; break;
      case 123: type = ME_ALL_NOTES_OFF; break;
      case 126: type = ME_MONO; break;
      case 127: type = ME_POLY; break;
      default: type = -1; break;
    }

    if(type != -1)
    {
	if(val > 127)
	    val = 127;
	ev_ret->type    = type;
	ev_ret->channel = chn;
	ev_ret->a       = val;
	ev_ret->b       = 0;
	return 1;
    }
    return 0;
}

static int block_to_part(int block, int port)
{
	int p;
	p = block & 0x0F;
	if(p == 0) {p = 9;}
	else if(p <= 9) {p--;}
	return MERGE_CHANNEL_PORT2(p, port);
}

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
int parse_sysex_event_multi(uint8 *val, int32 len, MidiEvent *evm)
{
    int num_events = 0;				/* Number of events added */
	uint16 channel_st;
	int i, j;

    if(current_file_info->mid == 0 || current_file_info->mid >= 0x7e)
	current_file_info->mid = val[0];

    /* XG Multi Part Data parameter change */
    /* There are two ways to do this, neither of which match the XG spec... */
    /* All variables given in a single big block */
    if(len >= 10 &&
       val[0] == 0x43 && /* Yamaha ID */
       val[2] == 0x4C && /* XG Model ID */
       val[4] == 0x29) /* Total size of data body to be analyzed */  
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

		if(val[5] == 0x01) {	/* Effect 1 */
			for (ent=0; body <= body_end; body++, ent++) {
				switch(ent) {
				case 0x0C:	/* Reverb Return */
				  reverb_status.level = *body;
				  recompute_reverb_status();
				  ctl->cmsg(CMSG_INFO,VERB_NOISY,"Reverb Level (%d)",*body);
				  break;

				case 0x2C:	/* Chorus Return */
				  chorus_param.chorus_level = *body;
				  recompute_chorus_status();
				  ctl->cmsg(CMSG_INFO,VERB_NOISY,"Chorus Level (%d)",*body);
				  break;

				default:
				  ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported XG SysEx. (ADDR:%02X %02X %02X VAL:%02X)",val[3],val[4],val[5],val[6]);
				  continue;
				  break;
				}
			}
		} else if(val[5] == 0x08) {	/* Multi Part Data parameter change */
			for (ent=0; body <= body_end; body++, ent++) {
				switch(ent) {

				case 0x01:	/* bank select MSB */
				  SETMIDIEVENT(evm[num_events], 0, ME_TONE_BANK_MSB, p, *body, 0);
				  num_events++;
				  break;

				case 0x02:	/* bank select LSB */
				  SETMIDIEVENT(evm[num_events], 0, ME_TONE_BANK_LSB, p, *body, 0);
				  num_events++;
				  break;

				case 0x03:	/* program number */
				  SETMIDIEVENT(evm[num_events], 0, ME_PROGRAM, p, *body, 0);
				  num_events++;
				  break;

				case 0x04:	/* Rcv CHANNEL */
				  if(*body == 0x7F) {
					remove_channel_layer(p);
				   	init_channel_layer(p);
				  } else {
					add_channel_layer(*body, p);
				  }
				  break;

				case 0x05:	/* mono/poly mode */
				  if(*body == 0) {
					  channel[p].mono = 1;
				  } else {
					  channel[p].mono = 0;
				  }
				  break;

				case 0x08:	/* note shift ? */
				  SETMIDIEVENT(evm[num_events], 0, ME_KEYSHIFT, p, *body, 0);
				  num_events++;
				  break;

				case 0x0B:	/* volume */
				  SETMIDIEVENT(evm[num_events], 0, ME_MAINVOLUME, p, *body, 0);
				  num_events++;
				  break;

				case 0x0E:	/* pan */
				  if(*body == 0) {
					SETMIDIEVENT(evm[num_events], 0, ME_RANDOM_PAN, p, 0, 0);
				  }
				  else {
					SETMIDIEVENT(evm[num_events], 0, ME_PAN, p, *body, 0);
				  }
				  num_events++;
				  break;

				case 0x12:	/* chorus send */
				  SETMIDIEVENT(evm[num_events], 0, ME_CHORUS_EFFECT, p, *body, 0);
				  num_events++;
				  break;

				case 0x13:	/* reverb send */
				  SETMIDIEVENT(evm[num_events], 0, ME_REVERB_EFFECT, p, *body, 0);
				  num_events++;
				  break;

				case 0x23:	/* bend pitch control */
				  SETMIDIEVENT(evm[num_events], 0,ME_RPN_MSB,p,0,0);
				  SETMIDIEVENT(evm[num_events+1], 0,ME_RPN_LSB,p,0,0);
				  SETMIDIEVENT(evm[num_events+2], 0,ME_DATA_ENTRY_MSB,p,(*body - 0x40) & 0x7F,0);
				  num_events += 3;
				  break;

				case 0x41:	/* scale tuning */
				case 0x42:
				case 0x43:
				case 0x44:
				case 0x45:
				case 0x46:
				case 0x47:
				case 0x48:
				case 0x49:
				case 0x4a:
				case 0x4b:
				case 0x4c:
					SETMIDIEVENT(evm[num_events],
							0, ME_SCALE_TUNING, p, ent - 0x41, *body - 64);
					num_events++;
					ctl->cmsg(CMSG_INFO, VERB_NOISY,
							"Scale Tuning %s (CH:%d %d cent)",
							note_name[ent - 0x41], p, *body - 64);
					break;

				default:
				  ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported XG SysEx. (ADDR:%02X %02X %02X VAL:%02X)",val[3],val[4],val[5],val[6]);
				  continue;
				  break;
				}
			}
		}
    }
    /* Or you can specify them one SYSEX event at a time... */
    else if(len == 8 &&
       val[0] == 0x43 && /* Yamaha ID */
       val[2] == 0x4C) /* XG Model ID */ 
    {
		uint8 p;				/* Channel part number [0..15] */
		int ent;				/* Entry # of sub-event */

		p = val[4];
		ent = val[5];

		if(val[3] == 0x01) {	/* Effect 1 */
			switch(ent) {
				case 0x0C:	/* Reverb Return */
				  SETMIDIEVENT(evm[0], 0,ME_SYSEX_XG_LSB,p,val[6],0x00);
				  num_events++;
				  break;

				case 0x2C:	/* Chorus Return */
				  SETMIDIEVENT(evm[0], 0,ME_SYSEX_XG_LSB,p,val[6],0x01);
				  num_events++;
				  break;

				default:
				  break;
			}
		} else if(val[3] == 0x08) {	/* Multi Part Data parameter change */
			switch(ent) {
				case 0x01:	/* bank select MSB */
				  SETMIDIEVENT(evm[0], 0, ME_TONE_BANK_MSB, p, val[6], 0);
				  num_events++;
				  break;

				case 0x02:	/* bank select LSB */
				  SETMIDIEVENT(evm[0], 0, ME_TONE_BANK_LSB, p, val[6], 0);
				  num_events++;
				  break;

				case 0x03:	/* program number */
				  SETMIDIEVENT(evm[0], 0, ME_PROGRAM, p, val[6], 0);
				  num_events++;
				  break;

				case 0x04:	/* Rcv CHANNEL */
				  if(val[6] == 0x7F) {
					remove_channel_layer(p);
				   	init_channel_layer(p);
				  } else {
					add_channel_layer(val[6], p);
				  }
				  break;

				case 0x05:	/* mono/poly mode */
 				  if(val[6] == 0) {SETMIDIEVENT(evm[0], 0, ME_MONO, p, val[6], 0);}
				  else {SETMIDIEVENT(evm[0], 0, ME_POLY, p, val[6], 0);}
				  num_events++;
				  break;

				case 0x08:	/* note shift ? */
				  SETMIDIEVENT(evm[0], 0, ME_KEYSHIFT, p, val[6], 0);
				  num_events++;
				  break;

				case 0x0B:	/* volume */
				  SETMIDIEVENT(evm[0], 0, ME_MAINVOLUME, p, val[6], 0);
				  num_events++;
				  break;

				case 0x0E:	/* pan */
				  if(val[6] == 0) {
					SETMIDIEVENT(evm[0], 0, ME_RANDOM_PAN, p, 0, 0);
				  }
				  else {
					SETMIDIEVENT(evm[0], 0, ME_PAN, p, val[6], 0);
				  }
				  num_events++;
				  break;

				case 0x12:	/* chorus send */
				  SETMIDIEVENT(evm[0], 0, ME_CHORUS_EFFECT, p, val[6], 0);
				  num_events++;
				  break;

				case 0x13:	/* reverb send */
				  SETMIDIEVENT(evm[0], 0, ME_REVERB_EFFECT, p, val[6], 0);
				  num_events++;
				  break;

				case 0x23:	/* bend pitch control */
				  SETMIDIEVENT(evm[0], 0,ME_RPN_MSB,p,0,0);
				  SETMIDIEVENT(evm[1], 0,ME_RPN_LSB,p,0,0);
				  SETMIDIEVENT(evm[2], 0,ME_DATA_ENTRY_MSB,p,(val[6] - 0x40) & 0x7F,0);
				  num_events += 3;
				  break;

				case 0x41:	/* scale tuning */
				case 0x42:
				case 0x43:
				case 0x44:
				case 0x45:
				case 0x46:
				case 0x47:
				case 0x48:
				case 0x49:
				case 0x4a:
				case 0x4b:
				case 0x4c:
					SETMIDIEVENT(evm[0],
							0, ME_SCALE_TUNING, p, ent - 0x41, val[6] - 64);
					num_events++;
					ctl->cmsg(CMSG_INFO, VERB_NOISY,
							"Scale Tuning %s (CH:%d %d cent)",
							note_name[ent - 0x41], p, val[6] - 64);
					break;

				default:
				  ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported XG SysEx. (ADDR:%02X %02X %02X VAL:%02X)",val[3],val[4],val[5],val[6]);
				  break;
			}
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
		uint8 p, dp, udn, gslen, port = 0;
		int i, addr, addr_h, addr_m, addr_l, checksum;
		p = block_to_part(val[5], midi_port_number);

		/* calculate checksum */
		checksum = 0;
		for(gslen = 9; gslen < len; gslen++)
			if(val[gslen] == 0xF7)
				break;
		for(i=4;i<gslen-1;i++) {
			checksum += val[i];
		}
		if(((128 - (checksum & 0x7F)) & 0x7F) != val[gslen-1]) {
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"GS SysEx: Checksum Error.");
			return num_events;
		}

		/* drum channel */
		dp = rhythm_part[(val[5] & 0xF0) >> 4];

		/* calculate user drumset number */
		udn = (val[5] & 0xF0) >> 4;

		addr_h = val[4];
		addr_m = val[5];
		addr_l = val[6];
		if(addr_h == 0x50) {	/* for double module mode */
			port = 1;
			p = block_to_part(val[5], port);
			addr_h = 0x40;
		} else if(addr_h == 0x51) {
			port = 1;
			p = block_to_part(val[5], port);
			addr_h = 0x41;
		}
		addr = (((int32)addr_h)<<16 | ((int32)addr_m)<<8 | (int32)addr_l);

		switch(addr_h) {	
		case 0x40:
			if((addr & 0xFFF000) == 0x401000) {
				switch(addr & 0xFF) {
				case 0x00:
					SETMIDIEVENT(evm[0], 0, ME_TONE_BANK_MSB,p,val[7],0);
					SETMIDIEVENT(evm[1], 0, ME_PROGRAM,p,val[8],0);
					num_events += 2;
					break;
				case 0x02:	/* Rx. Channel */
					if(val[7] == 0x10) {
						remove_channel_layer(p);
						init_channel_layer(p);
					} else {
						add_channel_layer(val[7], p);
					}
					break;
				case 0x13:
					if(val[7] == 0) {SETMIDIEVENT(evm[0], 0, ME_MONO,p,val[7],0);}
					else {SETMIDIEVENT(evm[0], 0, ME_POLY,p,val[7],0);}
					num_events++;
					break;
				case 0x14:	/* Assign Mode */
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x24);
					num_events++;
					break;
				case 0x15:	/* Use for Rhythm Part */
					if(val[7]) {
						rhythm_part[val[7] - 1] = p;
					}
					break;
				case 0x16:	/* Pitch Key Shift */
					break;
				case 0x17:	/* Pitch Offset Fine */
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x26);
					num_events++;
					break;
				case 0x19:
					SETMIDIEVENT(evm[0], 0,ME_MAINVOLUME,p,val[7],0);
					num_events++;
					break;
				case 0x1A:	/* Velocity Sense Depth */
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x21);
					num_events++;
					break;
				case 0x1B:	/* Velocity Sense Offset */
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x22);
					num_events++;
					break;
				case 0x1C:
					if (val[7] == 0) {
						SETMIDIEVENT(evm[0], 0, ME_RANDOM_PAN, p, 0, 0);
					} else {
						SETMIDIEVENT(evm[0], 0,ME_PAN,p,val[7],0);
					}
					num_events++;
					break;
				case 0x21:
					SETMIDIEVENT(evm[0], 0,ME_CHORUS_EFFECT,p,val[7],0);
					num_events++;
					break;
				case 0x22:
					SETMIDIEVENT(evm[0], 0,ME_REVERB_EFFECT,p,val[7],0);
					num_events++;
					break;
				case 0x2C:
					SETMIDIEVENT(evm[0], 0,ME_CELESTE_EFFECT,p,val[7],0);
					num_events++;
					break;
				case 0x2A:
					SETMIDIEVENT(evm[0], 0,ME_NRPN_MSB,p,0x00,0);
					SETMIDIEVENT(evm[1], 0,ME_NRPN_LSB,p,0x01,0);
					SETMIDIEVENT(evm[2], 0,ME_DATA_ENTRY_MSB,p,val[7],0);
					SETMIDIEVENT(evm[3], 0,ME_DATA_ENTRY_LSB,p,val[8],0);
					num_events += 4;
					break;
				case 0x30:
					SETMIDIEVENT(evm[0], 0,ME_NRPN_MSB,p,0x01,0);
					SETMIDIEVENT(evm[1], 0,ME_NRPN_LSB,p,0x08,0);
					SETMIDIEVENT(evm[2], 0,ME_DATA_ENTRY_MSB,p,val[7],0);
					num_events += 3;
					break;
				case 0x31:
					SETMIDIEVENT(evm[0], 0,ME_NRPN_MSB,p,0x01,0);
					SETMIDIEVENT(evm[1], 0,ME_NRPN_LSB,p,0x09,0);
					SETMIDIEVENT(evm[2], 0,ME_DATA_ENTRY_MSB,p,val[7],0);
					num_events += 3;
					break;
				case 0x32:
					SETMIDIEVENT(evm[0], 0,ME_NRPN_MSB,p,0x01,0);
					SETMIDIEVENT(evm[1], 0,ME_NRPN_LSB,p,0x20,0);
					SETMIDIEVENT(evm[2], 0,ME_DATA_ENTRY_MSB,p,val[7],0);
					num_events += 3;
					break;
				case 0x33:
					SETMIDIEVENT(evm[0], 0,ME_NRPN_MSB,p,0x01,0);
					SETMIDIEVENT(evm[1], 0,ME_NRPN_LSB,p,0x21,0);
					SETMIDIEVENT(evm[2], 0,ME_DATA_ENTRY_MSB,p,val[7],0);
					num_events += 3;
					break;
				case 0x34:
					SETMIDIEVENT(evm[0], 0,ME_NRPN_MSB,p,0x01,0);
					SETMIDIEVENT(evm[1], 0,ME_NRPN_LSB,p,0x63,0);
					SETMIDIEVENT(evm[2], 0,ME_DATA_ENTRY_MSB,p,val[7],0);
					num_events += 3;
					break;
				case 0x35:
					SETMIDIEVENT(evm[0], 0,ME_NRPN_MSB,p,0x01,0);
					SETMIDIEVENT(evm[1], 0,ME_NRPN_LSB,p,0x64,0);
					SETMIDIEVENT(evm[2], 0,ME_DATA_ENTRY_MSB,p,val[7],0);
					num_events += 3;
					break;
				case 0x36:
					SETMIDIEVENT(evm[0], 0,ME_NRPN_MSB,p,0x01,0);
					SETMIDIEVENT(evm[1], 0,ME_NRPN_LSB,p,0x66,0);
					SETMIDIEVENT(evm[2], 0,ME_DATA_ENTRY_MSB,p,val[7],0);
					num_events += 3;
					break;
				case 0x37:
					SETMIDIEVENT(evm[0], 0,ME_NRPN_MSB,p,0x01,0);
					SETMIDIEVENT(evm[1], 0,ME_NRPN_LSB,p,0x0A,0);
					SETMIDIEVENT(evm[2], 0,ME_DATA_ENTRY_MSB,p,val[7],0);
					num_events += 3;
					break;
				case 0x40:	/* Scale Tuning */
					for (i = 0; i < 12; i++) {
						SETMIDIEVENT(evm[i],
								0, ME_SCALE_TUNING, p, i, val[i + 7] - 64);
						ctl->cmsg(CMSG_INFO, VERB_NOISY,
								"Scale Tuning %s (CH:%d %d cent)",
								note_name[i], p, val[i + 7] - 64);
					}
					num_events += 12;
					break;
				default:
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported GS SysEx. (ADDR:%02X %02X %02X VAL:%02X %02X)",addr_h,addr_m,addr_l,val[7],val[8]);
					break;
				}
			} else if((addr & 0xFFF000) == 0x402000) {
				switch(addr & 0xFF) {
				case 0x10:	/* Bend Pitch Control */
					SETMIDIEVENT(evm[0], 0,ME_RPN_MSB,p,0,0);
					SETMIDIEVENT(evm[1], 0,ME_RPN_LSB,p,0,0);
					SETMIDIEVENT(evm[2], 0,ME_DATA_ENTRY_MSB,p,(val[7] - 0x40) & 0x7F,0);
					num_events += 3;
					break;
				default:
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported GS SysEx. (ADDR:%02X %02X %02X VAL:%02X %02X)",addr_h,addr_m,addr_l,val[7],val[8]);
					break;
				}
			} else if((addr & 0xFFFF00) == 0x400100) {
				switch(addr & 0xFF) {
				case 0x30:	/* Reverb Macro */
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x05);
					num_events++;
					break;
				case 0x31:	/* Reverb Character */
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x06);
					num_events++;
					break;
				case 0x32:	/* Reverb Pre-LPF */
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x07);
					num_events++;
					break;
				case 0x33:	/* Reverb Level */
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x08);
					num_events++;
					break;
				case 0x34:	/* Reverb Time */
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x09);
					num_events++;
					break;
				case 0x35:	/* Reverb Delay Feedback */
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x0A);
					num_events++;
					break;
				case 0x36:	/* Unknown Reverb Parameter */
					break;
				case 0x37:	/* Reverb Predelay Time */
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x0C);
					num_events++;
					break;
				case 0x38:	/* Chorus Macro */
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x0D);
					num_events++;
					break;
				case 0x39:	/* Chorus Pre-LPF */
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x0E);
					num_events++;
					break;
				case 0x3A:	/* Chorus Level */
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x0F);
					num_events++;
					break;
				case 0x3B:	/* Chorus Feedback */
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x10);
					num_events++;
					break;
				case 0x3C:	/* Chorus Delay */
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x11);
					num_events++;
					break;
				case 0x3D:	/* Chorus Rate */
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x12);
					num_events++;
					break;
				case 0x3E:	/* Chorus Depth */
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x13);
					num_events++;
					break;
				case 0x3F:	/* Chorus Send Level to Reverb */
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x14);
					num_events++;
					break;
				case 0x40:	/* Chorus Send Level to Delay */
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x15);
					num_events++;
					break;
				case 0x50:	/* Delay Macro */
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x16);
					num_events++;
					break;
				case 0x51:	/* Delay Pre-LPF */
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x17);
					num_events++;
					break;
				case 0x52:	/* Delay Time Center */
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x18);
					num_events++;
					break;
				case 0x53:	/* Delay Time Ratio Left */
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x19);
					num_events++;
					break;
				case 0x54:	/* Delay Time Ratio Right */
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x1A);
					num_events++;
					break;
				case 0x55:	/* Delay Level Center */
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x1B);
					num_events++;
					break;
				case 0x56:	/* Delay Level Left */
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x1C);
					num_events++;
					break;
				case 0x57:	/* Delay Level Right */
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x1D);
					num_events++;
					break;
				case 0x58:	/* Delay Level */
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x1E);
					num_events++;
					break;
				case 0x59:	/* Delay Feedback */
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x1F);
					num_events++;
					break;
				case 0x5A:	/* Delay Send Level to Reverb */
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x20);
					num_events++;
					break;
				default:
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported GS SysEx. (ADDR:%02X %02X %02X VAL:%02X %02X)",addr_h,addr_m,addr_l,val[7],val[8]);
					break;
				}
			} else if((addr & 0xFFFF00) == 0x400200) {
				switch(addr & 0xFF) {	/* EQ Parameter */
				case 0x00:	/* EQ LOW FREQ */
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x01);
					num_events++;
					break;
				case 0x01:	/* EQ LOW GAIN */
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x02);
					num_events++;
					break;
				case 0x02:	/* EQ HIGH FREQ */
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x03);
					num_events++;
					break;
				case 0x03:	/* EQ HIGH GAIN */
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x04);
					num_events++;
					break;
				default:
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported GS SysEx. (ADDR:%02X %02X %02X VAL:%02X %02X)",addr_h,addr_m,addr_l,val[7],val[8]);
					break;
				}
			} else if((addr & 0xFFFF00) == 0x400300) {
				switch(addr & 0xFF) {	/* Insertion Effect Parameter */
				case 0x00:
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x27);
					SETMIDIEVENT(evm[1], 0,ME_SYSEX_GS_LSB,p,val[8],0x28);
					num_events += 2;
					break;
				case 0x03:
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x29);
					num_events++;
					break;
				case 0x04:
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x2A);
					num_events++;
					break;
				case 0x05:
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x2B);
					num_events++;
					break;
				case 0x06:
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x2C);
					num_events++;
					break;
				case 0x07:
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x2D);
					num_events++;
					break;
				case 0x08:
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x2E);
					num_events++;
					break;
				case 0x09:
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x2F);
					num_events++;
					break;
				case 0x0A:
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x30);
					num_events++;
					break;
				case 0x0B:
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x31);
					num_events++;
					break;
				case 0x0C:
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x32);
					num_events++;
					break;
				case 0x0D:
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x33);
					num_events++;
					break;
				case 0x0E:
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x34);
					num_events++;
					break;
				case 0x0F:
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x35);
					num_events++;
					break;
				case 0x10:
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x36);
					num_events++;
					break;
				case 0x11:
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x37);
					num_events++;
					break;
				case 0x12:
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x38);
					num_events++;
					break;
				case 0x13:
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x39);
					num_events++;
					break;
				case 0x14:
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x3A);
					num_events++;
					break;
				case 0x15:
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x3B);
					num_events++;
					break;
				case 0x16:
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x3C);
					num_events++;
					break;
				case 0x17:
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x3D);
					num_events++;
					break;
				case 0x18:
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x3E);
					num_events++;
					break;
				case 0x19:
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x3F);
					num_events++;
					break;
				case 0x1B:
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x40);
					num_events++;
					break;
				case 0x1C:
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x41);
					num_events++;
					break;
				case 0x1D:
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x42);
					num_events++;
					break;
				case 0x1E:
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x43);
					num_events++;
					break;
				case 0x1F:
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x44);
					num_events++;
					break;
				default:
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported GS SysEx. (ADDR:%02X %02X %02X VAL:%02X %02X)",addr_h,addr_m,addr_l,val[7],val[8]);
					break;
				}
			} else if((addr & 0xFFF000) == 0x404000) {
				switch(addr & 0xFF) {
				case 0x00:	/* TONE MAP NUMBER */
					if(val[7] == 0) {
						SETMIDIEVENT(evm[0], 0, ME_TONE_BANK_LSB,p,channel[p].tone_map0_number,0);
					} else {
						SETMIDIEVENT(evm[0], 0, ME_TONE_BANK_LSB,p,val[7],0);
					}
					num_events++;
					break;
				case 0x01:	/* TONE MAP-0 NUMBER */
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x25);
					num_events++;
					break;
				case 0x20:	/* EQ ON/OFF */
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x00);
					num_events++;
					break;
				case 0x22:	/* EFX ON/OFF */
					SETMIDIEVENT(evm[0], 0,ME_SYSEX_GS_LSB,p,val[7],0x23);
					num_events++;
					break;
				default:
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported GS SysEx. (ADDR:%02X %02X %02X VAL:%02X %02X)",addr_h,addr_m,addr_l,val[7],val[8]);
					break;
				}
			}
			break;
		case 0x41:
			switch(addr & 0xF00) {
			case 0x100:	/* Play Note */
				SETMIDIEVENT(evm[0], 0,ME_NRPN_MSB,dp,0x18,0);
				SETMIDIEVENT(evm[1], 0,ME_NRPN_LSB,dp,val[6],0);
				SETMIDIEVENT(evm[2], 0,ME_DATA_ENTRY_MSB,dp,val[7],0);
				num_events += 3;
				break;
			case 0x200:
				SETMIDIEVENT(evm[0], 0,ME_NRPN_MSB,dp,0x1A,0);
				SETMIDIEVENT(evm[1], 0,ME_NRPN_LSB,dp,val[6],0);
				SETMIDIEVENT(evm[2], 0,ME_DATA_ENTRY_MSB,dp,val[7],0);
				num_events += 3;
				break;
			case 0x400:
				SETMIDIEVENT(evm[0], 0,ME_NRPN_MSB,dp,0x1C,0);
				SETMIDIEVENT(evm[1], 0,ME_NRPN_LSB,dp,val[6],0);
				SETMIDIEVENT(evm[2], 0,ME_DATA_ENTRY_MSB,dp,val[7],0x1);
				num_events += 3;
				break;
			case 0x500:
				SETMIDIEVENT(evm[0], 0,ME_NRPN_MSB,dp,0x1D,0);
				SETMIDIEVENT(evm[1], 0,ME_NRPN_LSB,dp,val[6],0);
				SETMIDIEVENT(evm[2], 0,ME_DATA_ENTRY_MSB,dp,val[7],0);
				num_events += 3;
				break;
			case 0x600:
				SETMIDIEVENT(evm[0], 0,ME_NRPN_MSB,dp,0x1E,0);
				SETMIDIEVENT(evm[1], 0,ME_NRPN_LSB,dp,val[6],0);
				SETMIDIEVENT(evm[2], 0,ME_DATA_ENTRY_MSB,dp,val[7],0);
				num_events += 3;
				break;
			case 0x700:
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"Rx. Note Off (CH:%d NOTE:%d VAL:%d)",dp,val[6],val[7]);
				if(channel[dp].drums[val[6]] == NULL) {play_midi_setup_drums(dp, val[6]);}
				channel[dp].drums[val[6]]->rx_note_off = val[7];
				break;
			case 0x900:
				SETMIDIEVENT(evm[0], 0,ME_NRPN_MSB,dp,0x1F,0);
				SETMIDIEVENT(evm[1], 0,ME_NRPN_LSB,dp,val[6],0);
				SETMIDIEVENT(evm[2], 0,ME_DATA_ENTRY_MSB,dp,val[7],0);
				num_events += 3;
				break;
			default:
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported GS SysEx. (ADDR:%02X %02X %02X VAL:%02X %02X)",addr_h,addr_m,addr_l,val[7],val[8]);
				break;
			}
			break;
#if 0
		case 0x20:	/* User Instrument */
			switch(addr & 0xF00) {
				case 0x000:	/* Source Map */
					get_userinst(64+udn, val[6])->source_map = val[7];
					break;
				case 0x100:	/* Source Bank */
					get_userinst(64+udn, val[6])->source_bank = val[7];
					break;
#if !defined(TIMIDITY_TOOLS)
				case 0x200:	/* Source Prog */
					get_userinst(64+udn, val[6])->source_prog = val[7];
					break;
#endif
				default:
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported GS SysEx. (ADDR:%02X %02X %02X VAL:%02X %02X)",addr_h,addr_m,addr_l,val[7],val[8]);
					break;
			}
			break;
#endif
		case 0x21:	/* User Drumset */
			switch(addr & 0xF00) {
				case 0x100:	/* Play Note */
					get_userdrum(64+udn, val[6])->play_note = val[7];
					SETMIDIEVENT(evm[0], 0,ME_NRPN_MSB,dp,0x18,0);
					SETMIDIEVENT(evm[1], 0,ME_NRPN_LSB,dp,val[6],0);
					SETMIDIEVENT(evm[2], 0,ME_DATA_ENTRY_MSB,dp,val[7],0x1);
					num_events += 3;
					break;
				case 0x200:	/* Level */
					get_userdrum(64+udn, val[6])->level = val[7];
					SETMIDIEVENT(evm[0], 0,ME_NRPN_MSB,dp,0x1A,0);
					SETMIDIEVENT(evm[1], 0,ME_NRPN_LSB,dp,val[6],0);
					SETMIDIEVENT(evm[2], 0,ME_DATA_ENTRY_MSB,dp,val[7],0);
					num_events += 3;
					break;
				case 0x300:	/* Assign Group */
					get_userdrum(64+udn, val[6])->assign_group = val[7];
					if(val[7] != 0) {recompute_userdrum_altassign(udn+64, val[7]);}
					break;
				case 0x400:	/* Panpot */
					get_userdrum(64+udn, val[6])->pan = val[7];
					SETMIDIEVENT(evm[0], 0,ME_NRPN_MSB,dp,0x1C,0);
					SETMIDIEVENT(evm[1], 0,ME_NRPN_LSB,dp,val[6],0);
					SETMIDIEVENT(evm[2], 0,ME_DATA_ENTRY_MSB,dp,val[7],0);
					num_events += 3;
					break;
				case 0x500:	/* Reverb Send Level */
					get_userdrum(64+udn, val[6])->reverb_send_level = val[7];
					SETMIDIEVENT(evm[0], 0,ME_NRPN_MSB,dp,0x1D,0);
					SETMIDIEVENT(evm[1], 0,ME_NRPN_LSB,dp,val[6],0);
					SETMIDIEVENT(evm[2], 0,ME_DATA_ENTRY_MSB,dp,val[7],0);
					num_events += 3;
					break;
				case 0x600:	/* Chorus Send Level */
					get_userdrum(64+udn, val[6])->chorus_send_level = val[7];
					SETMIDIEVENT(evm[0], 0,ME_NRPN_MSB,dp,0x1E,0);
					SETMIDIEVENT(evm[1], 0,ME_NRPN_LSB,dp,val[6],0);
					SETMIDIEVENT(evm[2], 0,ME_DATA_ENTRY_MSB,dp,val[7],0);
					num_events += 3;
					break;
				case 0x700:	/* Rx. Note Off */
					get_userdrum(64+udn, val[6])->rx_note_off = val[7];
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"Rx. Note Off (CH:%d NOTE:%d VAL:%d)",dp,val[6],val[7]);
					if(channel[dp].drums[val[6]] == NULL) {play_midi_setup_drums(dp, val[6]);}
					channel[dp].drums[val[6]]->rx_note_off = val[7];
					break;
				case 0x800:	/* Rx. Note On */
					get_userdrum(64+udn, val[6])->rx_note_on = val[7];
					break;
				case 0x900:	/* Delay Send Level */
					get_userdrum(64+udn, val[6])->delay_send_level = val[7];
					SETMIDIEVENT(evm[0], 0,ME_NRPN_MSB,dp,0x1F,0);
					SETMIDIEVENT(evm[1], 0,ME_NRPN_LSB,dp,val[6],0);
					SETMIDIEVENT(evm[2], 0,ME_DATA_ENTRY_MSB,dp,val[7],0);
					num_events += 3;
					break;
				case 0xA00:	/* Source Map */
					get_userdrum(64+udn, val[6])->source_map = val[7];
					break;
				case 0xB00:	/* Source Prog */
					get_userdrum(64+udn, val[6])->source_prog = val[7];
					break;
#if !defined(TIMIDITY_TOOLS)
				case 0xC00:	/* Source Note */
					get_userdrum(64+udn, val[6])->source_note = val[7];
					break;
#endif
				default:
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported GS SysEx. (ADDR:%02X %02X %02X VAL:%02X %02X)",addr_h,addr_m,addr_l,val[7],val[8]);
					break;
			}
			break;
		case 0x00:	/* System */
			switch(addr & 0xFFF0) {
				case 0x0100:	/* Channel Msg Rx Port (A) */
					add_channel_layer(block_to_part(addr & 0xF, val[7]), block_to_part(addr & 0xF, 0));
					break;
				case 0x0110:	/* Channel Msg Rx Port (B) */
					add_channel_layer(block_to_part(addr & 0xF, val[7]), block_to_part(addr & 0xF, 1));
					break;
				default:
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"Unsupported GS SysEx. (ADDR:%02X %02X %02X VAL:%02X %02X)",addr_h,addr_m,addr_l,val[7],val[8]);
					break;
			}
			break;
		}
    }

	/* Non-RealTime / RealTime Universal SysEx messages
	 * 0 0x7e(Non-RealTime) / 0x7f(RealTime)
	 * 1 SysEx device ID.  Could be from 0x00 to 0x7f.
	 *   0x7f means disregard device.
	 * 2 Sub ID
	 * ...
	 * E 0xf7
	 */
	if (len > 4 && val[0] >= 0x7e)
		switch (val[2]) {
		case 0x01:	/* Sample Dump header */
		case 0x02:	/* Sample Dump packet */
		case 0x03:	/* Dump Request */
		case 0x05:	/* Sample Dump extensions */
		case 0x06:	/* Inquiry Message */
		case 0x07:	/* File Dump */
			break;
		case 0x08:	/* MIDI Tuning Standard */
			switch (val[3]) {
			case 0x0b:
				channel_st = val[4] << 14 | val[5] << 7 | val[6];
				if (val[1] == 0x7f) {
					SETMIDIEVENT(evm[0], 0, ME_MASTER_TEMPER_TYPE,
							0, val[7], (val[0] == 0x7f) ? 1 : 0);
					num_events++;
				} else {
					for (i = 0, j = 0; i < 16; i++)
						if (channel_st & 1 << i) {
							SETMIDIEVENT(evm[j], 0, ME_TEMPER_TYPE,
									MERGE_CHANNEL_PORT(i),
									val[7], (val[0] == 0x7f) ? 1 : 0);
							j++;
						}
					num_events += j;
				}
				break;
			}
			break;
		case 0x09:	/* General MIDI Message */
		case 0x7b:	/* End of File */
		case 0x7c:	/* Handshaking Message: Wait */
		case 0x7d:	/* Handshaking Message: Cancel */
		case 0x7e:	/* Handshaking Message: NAK */
		case 0x7f:	/* Handshaking Message: ACK */
			break;
		}

    return(num_events);
}

int parse_sysex_event(uint8 *val, int32 len, MidiEvent *ev)
{
	uint16 vol;
	
    if(current_file_info->mid == 0 || current_file_info->mid >= 0x7e)
	current_file_info->mid = val[0];

    if(len >= 10 &&
       val[0] == 0x41 && /* Roland ID */
       val[1] == 0x10 && /* Device ID */
       val[2] == 0x42 && /* GS Model ID */
       val[3] == 0x12)   /* Data Set Command */
    {
	/* Roland GS-Based Synthesizers.
	 * val[4..6] is address, val[7..len-2] is body.
	 *
	 * GS     Channel part number
	 * 0      10
	 * 1-9    1-9
	 * 10-15  11-16
	 */

	int32 addr,checksum,i;		/* SysEx address */
	uint8 *body;		/* SysEx body */
	uint8 p,gslen;		/* Channel part number [0..15] */

	/* check Checksum */
	checksum = 0;
	for(gslen = 9; gslen < len; gslen++)
		if(val[gslen] == 0xF7)
			break;
	for(i=4;i<gslen-1;i++) {
		checksum += val[i];
	}
	if(((128 - (checksum & 0x7F)) & 0x7F) != val[gslen-1]) {
		return 0;
	}

	addr = (((int32)val[4])<<16 |
		((int32)val[5])<<8 |
		(int32)val[6]);
	body = val + 7;
	p = (uint8)((addr >> 8) & 0xF);
	if(p == 0)
	    p = 9;
	else if(p <= 9)
	    p--;
	p = MERGE_CHANNEL_PORT(p);

	if(val[4] == 0x50) {	/* for double module mode */
		p += 16;
		addr = (((int32)0x40)<<16 |
			((int32)val[5])<<8 |
			(int32)val[6]);
	} else {	/* single module mode */
		addr = (((int32)val[4])<<16 |
			((int32)val[5])<<8 |
			(int32)val[6]);
	}

	if((addr & 0xFFF0FF) == 0x401015) /* Rhythm Parts */
	{
#ifdef GS_DRUMPART
/* GS drum part check from Masaaki Koyanagi's patch (GS_Drum_Part_Check()) */
/* Modified by Masanao Izumo */
	    SETMIDIEVENT(*ev, 0, ME_DRUMPART, p, *body, 0);
	    return 1;
#else
	    return 0;
#endif /* GS_DRUMPART */
	}

	if((addr & 0xFFF0FF) == 0x401016) /* Key Shift */
	{
	    SETMIDIEVENT(*ev, 0, ME_KEYSHIFT, p, *body, 0);
	    return 1;
	}

	if(addr == 0x400004) /* Master Volume */
	{
	    vol = gs_convert_master_vol(*body);
	    SETMIDIEVENT(*ev, 0, ME_MASTER_VOLUME,
			 0, vol & 0xFF, (vol >> 8) & 0xFF);
	    return 1;
	}

	if((addr & 0xFFF0FF) == 0x401019) /* Volume on/off */
	{
#if 0
	    SETMIDIEVENT(*ev, 0, ME_VOLUME_ONOFF, p, *body >= 64, 0);
#endif
	    return 0;
	}

	if((addr & 0xFFF0FF) == 0x401002) /* Receive channel on/off */
	{
#if 0
	    SETMIDIEVENT(*ev, 0, ME_RECEIVE_CHANNEL, (uint8)p, *body >= 64, 0);
#endif
	    return 0;
	}

	if(0x402000 <= addr && addr <= 0x402F5A) /* Controller Routing */
	    return 0;

	if((addr & 0xFFF0FF) == 0x401040) /* Alternate Scale Tunings */
	    return 0;

	if((addr & 0xFFFFF0) == 0x400130) /* Changing Effects */
	{
	    switch(addr & 0xF)
	    {
	      case 0x8: /* macro */
		memcpy(chorus_status.macro, body, 3);
		break;
	      case 0x9: /* PRE-LPF */
		memcpy(chorus_status.pre_lpf, body, 3);
		break;
	      case 0xa: /* level */
		memcpy(chorus_status.level, body, 3);
		break;
	      case 0xb: /* feed back */
		memcpy(chorus_status.feed_back, body, 3);
		break;
	      case 0xc: /* delay */
		memcpy(chorus_status.delay, body, 3);
		break;
	      case 0xd: /* rate */
		memcpy(chorus_status.rate, body, 3);
		break;
	      case 0xe: /* depth */
		memcpy(chorus_status.depth, body, 3);
		break;
	      case 0xf: /* send level */
		memcpy(chorus_status.send_level, body, 3);
		break;
		  default: break;
	    }

	    check_chorus_text_start();
	    return 0;
	}

	if((addr & 0xFFF0FF) == 0x401003) /* Rx Pitch-Bend */
	    return 0;

	if(addr == 0x400110) /* Voice Reserve */
	{
	    if(len >= 25)
		memcpy(chorus_status.voice_reserve, body, 18);
	    check_chorus_text_start();
	    return 0;
	}

	if(addr == 0x40007F ||	/* GS Reset */
	   addr == 0x00007F)	/* SC-88 Single Module */
	{
	    SETMIDIEVENT(*ev, 0, ME_RESET, 0, GS_SYSTEM_MODE, 0);
	    return 1;
	}
	return 0;
    }

    if(len > 9 &&
       val[0] == 0x41 && /* Roland ID */
       val[1] == 0x10 && /* Device ID */
       val[2] == 0x45 && 
       val[3] == 0x12 && 
       val[4] == 0x10 && 
       val[5] == 0x00 && 
       val[6] == 0x00)
    {
	/* Text Insert for SC */
	uint8 save;

	len -= 2;
	save = val[len];
	val[len] = '\0';
	if(readmidi_make_string_event(ME_INSERT_TEXT, (char *)val + 7, ev, 1))
	{
	    val[len] = save;
	    return 1;
	}
	val[len] = save;
	return 0;
    }

    if(len > 9 &&                     /* GS lcd event. by T.Nogami*/
       val[0] == 0x41 && /* Roland ID */
       val[1] == 0x10 && /* Device ID */
       val[2] == 0x45 && 
       val[3] == 0x12 && 
       val[4] == 0x10 && 
       val[5] == 0x01 && 
       val[6] == 0x00)
    {
	/* Text Insert for SC */
	uint8 save;

	len -= 2;
	save = val[len];
	val[len] = '\0';
	if(readmidi_make_lcd_event(ME_GSLCD, (uint8 *)val + 7, ev))
	{
	    val[len] = save;
	    return 1;
	}
	val[len] = save;
	return 0;
    }
    
    if(len >= 8 &&
       val[0] == 0x43 &&
       val[1] == 0x10 &&
       val[2] == 0x4C &&
       val[3] == 0x00 &&
       val[4] == 0x00 &&
       val[5] == 0x7E)
    {
	/* XG SYSTEM ON */
	SETMIDIEVENT(*ev, 0, ME_RESET, 0, XG_SYSTEM_MODE, 0);
	return 1;
    }

	/* Non-RealTime / RealTime Universal SysEx messages
	 * 0 0x7e(Non-RealTime) / 0x7f(RealTime)
	 * 1 SysEx device ID.  Could be from 0x00 to 0x7f.
	 *   0x7f means disregard device.
	 * 2 Sub ID
	 * ...
	 * E 0xf7
	 */
	if (len > 4 && val[0] >= 0x7e)
		switch (val[2]) {
		case 0x01:	/* Sample Dump header */
		case 0x02:	/* Sample Dump packet */
		case 0x03:	/* Dump Request */
			break;
		case 0x04:	/* MIDI Time Code Setup/Device Control */
			switch (val[3]) {
			case 0x01:	/* Master Volume */
				vol = gm_convert_master_vol(val[4], val[5]);
				if (val[1] == 0x7f) {
					SETMIDIEVENT(*ev, 0, ME_MASTER_VOLUME, 0,
							vol & 0xff, vol >> 8 & 0xff);
				} else {
					SETMIDIEVENT(*ev, 0, ME_MAINVOLUME,
							MERGE_CHANNEL_PORT(val[1]),
							vol >> 8 & 0xff, 0);
				}
				return 1;
			}
			break;
		case 0x05:	/* Sample Dump extensions */
		case 0x06:	/* Inquiry Message */
		case 0x07:	/* File Dump */
			break;
		case 0x08:	/* MIDI Tuning Standard */
			switch (val[3]) {
			case 0x0a:
				SETMIDIEVENT(*ev, 0, ME_TEMPER_KEYSIG, 0,
						val[4] - 0x40 + val[5] * 16,
						(val[0] == 0x7f) ? 1 : 0);
				return 1;
			}
			break;
		case 0x09:	/* General MIDI Message */
			/* GM System Enable/Disable */
			if(val[3]) {
				ctl->cmsg(CMSG_INFO, VERB_DEBUG, "SysEx: GM System Enable");
				SETMIDIEVENT(*ev, 0, ME_RESET, 0, GM_SYSTEM_MODE, 0);
			} else {
				ctl->cmsg(CMSG_INFO, VERB_DEBUG, "SysEx: GM System Disable");
				SETMIDIEVENT(*ev, 0, ME_RESET, 0, DEFAULT_SYSTEM_MODE, 0);
			}
			return 1;
		case 0x7b:	/* End of File */
		case 0x7c:	/* Handshaking Message: Wait */
		case 0x7d:	/* Handshaking Message: Cancel */
		case 0x7e:	/* Handshaking Message: NAK */
		case 0x7f:	/* Handshaking Message: ACK */
			break;
		}

    return 0;
}

static int read_sysex_event(int32 at, int me, int32 len,
			    struct timidity_file *tf)
{
    uint8 *val;
    MidiEvent ev, evm[16];
    int ne, i;

    if(len == 0)
	return 0;
    if(me != 0xF0)
    {
	skip(tf, len);
	return 0;
    }

    val = (uint8 *)new_segment(&tmpbuffer, len);
    if(tf_read(val, 1, len, tf) != len)
    {
	reuse_mblock(&tmpbuffer);
	return -1;
    }
    if(parse_sysex_event(val, len, &ev))
    {
	ev.time = at;
	readmidi_add_event(&ev);
    }
	if ((ne = parse_sysex_event_multi(val, len, evm)))
		for (i = 0; i < ne; i++) {
			evm[i].time = at;
			readmidi_add_event(&evm[i]);
		}
    
    reuse_mblock(&tmpbuffer);

    return 0;
}

static char *fix_string(char *s)
{
    int i, j, w;
    char c;

    if(s == NULL)
	return NULL;
    while(*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
	s++;

    /* s =~ tr/ \t\r\n/ /s; */
    w = 0;
    for(i = j = 0; (c = s[i]) != '\0'; i++)
    {
	if(c == '\t' || c == '\r' || c == '\n')
	    c = ' ';
	if(w)
	    w = (c == ' ');
	if(!w)
	{
	    s[j++] = c;
	    w = (c == ' ');
	}
    }

    /* s =~ s/ $//; */
    if(j > 0 && s[j - 1] == ' ')
	j--;

    s[j] = '\0';
    return s;
}

static void smf_time_signature(int32 at, struct timidity_file *tf, int len)
{
    int n, d, c, b;

    /* Time Signature (nn dd cc bb)
     * [0]: numerator
     * [1]: denominator
     * [2]: number of MIDI clocks in a metronome click
     * [3]: number of notated 32nd-notes in a MIDI
     *      quarter-note (24 MIDI Clocks).
     */

    if(len != 4)
    {
	ctl->cmsg(CMSG_WARNING, VERB_VERBOSE, "Invalid time signature");
	skip(tf, len);
	return;
    }

    n = tf_getc(tf);
    d = (1<<tf_getc(tf));
    c = tf_getc(tf);
    b = tf_getc(tf);

    if(n == 0 || d == 0)
    {
	ctl->cmsg(CMSG_WARNING, VERB_VERBOSE, "Invalid time signature");
	return;
    }

    MIDIEVENT(at, ME_TIMESIG, 0, n, d);
    MIDIEVENT(at, ME_TIMESIG, 1, c, b);
    ctl->cmsg(CMSG_INFO, VERB_NOISY,
	      "Time signature: %d/%d %d clock %d q.n.", n, d, c, b);
    if(current_file_info->time_sig_n == -1)
    {
	current_file_info->time_sig_n = n;
	current_file_info->time_sig_d = d;
	current_file_info->time_sig_c = c;
	current_file_info->time_sig_b = b;
    }
}

static void smf_key_signature(int32 at, struct timidity_file *tf, int len)
{
	int8 sf, mi;
	/* Key Signature (sf mi)
	 * sf = -7:  7 flats
	 * sf = -1:  1 flat
	 * sf = 0:   key of C
	 * sf = 1:   1 sharp
	 * sf = 7:   7 sharps
	 * mi = 0:  major key
	 * mi = 1:  minor key
	 */
	
	if (len != 2) {
		ctl->cmsg(CMSG_WARNING, VERB_VERBOSE, "Invalid key signature");
		skip(tf, len);
		return;
	}
	sf = tf_getc(tf);
	mi = tf_getc(tf);
	if (sf < -7 || sf > 7) {
		ctl->cmsg(CMSG_WARNING, VERB_VERBOSE, "Invalid key signature");
		return;
	}
	if (mi != 0 && mi != 1) {
		ctl->cmsg(CMSG_WARNING, VERB_VERBOSE, "Invalid key signature");
		return;
	}
	MIDIEVENT(at, ME_KEYSIG, 0, sf, mi);
	ctl->cmsg(CMSG_INFO, VERB_NOISY,
			"Key signature: %d %s %s", abs(sf),
			(sf < 0) ? "flat(s)" : "sharp(s)", (mi) ? "minor" : "major");
}

/* Used for WRD reader */
int dump_current_timesig(MidiEvent *codes, int maxlen)
{
    int i, n;
    MidiEventList *e;

    if(maxlen <= 0 || evlist == NULL)
	return 0;
    n = 0;
    for(i = 0, e = evlist; i < event_count; i++, e = e->next)
	if(e->event.type == ME_TIMESIG && e->event.channel == 0)
	{
	    if(n == 0 && e->event.time > 0)
	    {
		/* 4/4 is default */
		SETMIDIEVENT(codes[0], 0, ME_TIMESIG, 0, 4, 4);
		n++;
		if(maxlen == 1)
		    return 1;
	    }

	    if(n > 0)
	    {
		if(e->event.a == codes[n - 1].a &&
		   e->event.b == codes[n - 1].b)
		    continue; /* Unchanged */
		if(e->event.time == codes[n - 1].time)
		    n--; /* overwrite previous code */
	    }
	    codes[n++] = e->event;
	    if(n == maxlen)
		return n;
	}
    return n;
}

/* Read a SMF track */
static int read_smf_track(struct timidity_file *tf, int trackno, int rewindp)
{
    int32 len, next_pos, pos;
    char tmp[4];
    int lastchan, laststatus;
    int me, type, a, b, c;
    int i;
    int32 smf_at_time;

    smf_at_time = readmidi_set_track(trackno, rewindp);

    /* Check the formalities */
    if((tf_read(tmp, 1, 4, tf) != 4) || (tf_read(&len, 4, 1, tf) != 1))
    {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		  "%s: Can't read track header.", current_filename);
	return -1;
    }
    len = BE_LONG(len);
    next_pos = tf_tell(tf) + len;
    if(strncmp(tmp, "MTrk", 4))
    {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		  "%s: Corrupt MIDI file.", current_filename);
	return -2;
    }

    lastchan = laststatus = 0;

    for(;;)
    {
	if(readmidi_error_flag)
	    return -1;
	if((len = getvl(tf)) < 0)
	    return -1;
	smf_at_time += len;
	errno = 0;
	if((i = tf_getc(tf)) == EOF)
	{
	    if(errno)
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "%s: read_midi_event: %s",
			  current_filename, strerror(errno));
	    else
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "Warning: %s: Too shorten midi file.",
			  current_filename);
	    return -1;
	}

	me = (uint8)i;
	if(me == 0xF0 || me == 0xF7) /* SysEx event */
	{
	    if((len = getvl(tf)) < 0)
		return -1;
	    if((i = read_sysex_event(smf_at_time, me, len, tf)) != 0)
		return i;
	}
	else if(me == 0xFF) /* Meta event */
	{
	    type = tf_getc(tf);
	    if((len = getvl(tf)) < 0)
		return -1;
	    if(type > 0 && type < 16)
	    {
		static char *label[] =
		{
		    "Text event: ", "Text: ", "Copyright: ", "Track name: ",
		    "Instrument: ", "Lyric: ", "Marker: ", "Cue point: "
		};

		if(type == 5 || /* Lyric */
		   (type == 1 && (opt_trace_text_meta_event ||
				  karaoke_format == 2 ||
				  chorus_status.status == CHORUS_ST_OK)) ||
		   (type == 6 &&  (current_file_info->format == 0 ||
				   (current_file_info->format == 1 &&
				    current_read_track == 0))))
		{
		    char *str, *text;
		    MidiEvent ev;

		    str = (char *)new_segment(&tmpbuffer, len + 3);
		    if(type != 6)
		    {
			i = tf_read(str, 1, len, tf);
			str[len] = '\0';
		    }
		    else
		    {
			i = tf_read(str + 1, 1, len, tf);
			str[0] = MARKER_START_CHAR;
			str[len + 1] = MARKER_END_CHAR;
			str[len + 2] = '\0';
		    }

		    if(i != len)
		    {
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				  "Warning: %s: Too shorten midi file.",
				  current_filename);
			reuse_mblock(&tmpbuffer);
			return -1;
		    }

		    if((text = readmidi_make_string_event(1, str, &ev, 1))
		       == NULL)
		    {
			reuse_mblock(&tmpbuffer);
			continue;
		    }
		    ev.time = smf_at_time;

		    if(type == 6)
		    {
			if(strlen(fix_string(text + 1)) == 2)
			{
			    reuse_mblock(&tmpbuffer);
			    continue; /* Empty Marker */
			}
		    }

		    switch(type)
		    {
		      case 1:
			if(karaoke_format == 2)
			{
			    *text = ME_KARAOKE_LYRIC;
			    if(karaoke_title_flag == 0 &&
			       strncmp(str, "@T", 2) == 0)
				current_file_info->karaoke_title =
				    add_karaoke_title(current_file_info->
						      karaoke_title, str + 2);
			    ev.type = ME_KARAOKE_LYRIC;
			    readmidi_add_event(&ev);
			    continue;
			}
			if(chorus_status.status == CHORUS_ST_OK)
			{
			    *text = ME_CHORUS_TEXT;
			    ev.type = ME_CHORUS_TEXT;
			    readmidi_add_event(&ev);
			    continue;
			}
			*text = ME_TEXT;
			ev.type = ME_TEXT;
			readmidi_add_event(&ev);
			continue;
		      case 5:
			*text = ME_LYRIC;
			ev.type = ME_LYRIC;
			readmidi_add_event(&ev);
			continue;
		      case 6:
			*text = ME_MARKER;
			ev.type = ME_MARKER;
			readmidi_add_event(&ev);
			continue;
		    }
		}

		if(type == 3 && /* Sequence or Track Name */
		   (current_file_info->format == 0 ||
		    (current_file_info->format == 1 &&
		     current_read_track == 0)))
		{
		  if(current_file_info->seq_name == NULL) {
		    char *name = dumpstring(3, len, "Sequence: ", 1, tf);
		    current_file_info->seq_name = safe_strdup(fix_string(name));
		    free(name);
		  }
		    else
			dumpstring(3, len, "Sequence: ", 0, tf);
		}
		else if(type == 1 &&
			current_file_info->first_text == NULL &&
			(current_file_info->format == 0 ||
			 (current_file_info->format == 1 &&
			  current_read_track == 0))) {
		  char *name = dumpstring(1, len, "Text: ", 1, tf);
		  current_file_info->first_text = safe_strdup(fix_string(name));
		  free(name);
		}
		else
		    dumpstring(type, len, label[(type>7) ? 0 : type], 0, tf);
	    }
	    else
	    {
		switch(type)
		{
		  case 0x00:
		    if(len == 2)
		    {
			a = tf_getc(tf);
			b = tf_getc(tf);
			ctl->cmsg(CMSG_INFO, VERB_DEBUG,
				  "(Sequence Number %02x %02x)", a, b);
		    }
		    else
			ctl->cmsg(CMSG_INFO, VERB_DEBUG,
				  "(Sequence Number len=%d)", len);
		    break;

		  case 0x2F: /* End of Track */
		    pos = tf_tell(tf);
		    if(pos < next_pos)
			tf_seek(tf, next_pos - pos, SEEK_CUR);
		    return 0;

		  case 0x51: /* Tempo */
		    a = tf_getc(tf);
		    b = tf_getc(tf);
		    c = tf_getc(tf);
		    MIDIEVENT(smf_at_time, ME_TEMPO, c, a, b);
		    break;

		  case 0x54:
		    /* SMPTE Offset (hr mn se fr ff)
		     * hr: hours&type
		     *     0     1     2     3    4    5    6    7   bits
		     *     0  |<--type -->|<---- hours [0..23]---->|
		     * type: 00: 24 frames/second
		     *       01: 25 frames/second
		     *       10: 30 frames/second (drop frame)
		     *       11: 30 frames/second (non-drop frame)
		     * mn: minis [0..59]
		     * se: seconds [0..59]
		     * fr: frames [0..29]
		     * ff: fractional frames [0..99]
		     */
		    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
			      "(SMPTE Offset meta event)");
		    skip(tf, len);
		    break;

		  case 0x58: /* Time Signature */
		    smf_time_signature(smf_at_time, tf, len);
		    break;

		  case 0x59: /* Key Signature */
		    smf_key_signature(smf_at_time, tf, len);
		    break;

		  case 0x7f: /* Sequencer-Specific Meta-Event */
		    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
			      "(Sequencer-Specific meta event, length %ld)",
			      len);
		    skip(tf, len);
		    break;

		  case 0x20: /* MIDI channel prefix (SMF v1.0) */
		    if(len == 1)
		    {
			int midi_channel_prefix = tf_getc(tf);
			ctl->cmsg(CMSG_INFO, VERB_DEBUG,
				  "(MIDI channel prefix %d)",
				  midi_channel_prefix);
		    }
		    else
			skip(tf, len);
		    break;

		  case 0x21: /* MIDI port number */
		    if(len == 1)
		    {
			if((midi_port_number = tf_getc(tf))
			   == EOF)
			{
			    ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
				      "Warning: %s: Too shorten midi file.",
				      current_filename);
			    return -1;
			}
			midi_port_number &= 0xF;
			ctl->cmsg(CMSG_INFO, VERB_DEBUG,
				  "(MIDI port number %d)", midi_port_number);
		    }
		    else
			skip(tf, len);
		    break;

		  default:
		    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
			      "(Meta event type 0x%02x, length %ld)",
			      type, len);
		    skip(tf, len);
		    break;
		}
	    }
	}
	else /* MIDI event */
	{
	    a = me;
	    if(a & 0x80) /* status byte */
	    {
		lastchan = MERGE_CHANNEL_PORT(a & 0x0F);
		laststatus = (a >> 4) & 0x07;
		if(laststatus != 7)
		    a = tf_getc(tf) & 0x7F;
	    }
	    switch(laststatus)
	    {
	      case 0: /* Note off */
		b = tf_getc(tf) & 0x7F;
		MIDIEVENT_LAYER(smf_at_time, ME_NOTEOFF, lastchan, a,b);
		break;

	      case 1: /* Note on */
		b = tf_getc(tf) & 0x7F;
		if(b)
		{
		    MIDIEVENT_LAYER(smf_at_time, ME_NOTEON, lastchan, a,b);
		}
		else /* b == 0 means Note Off */
		{
		    MIDIEVENT_LAYER(smf_at_time, ME_NOTEOFF, lastchan, a, 0);
		}
		break;

	      case 2: /* Key Pressure */
		b = tf_getc(tf) & 0x7F;
		MIDIEVENT_LAYER(smf_at_time, ME_KEYPRESSURE, lastchan, a, b);
		break;

	      case 3: /* Control change */
		b = tf_getc(tf);
		readmidi_add_ctl_event_layer(smf_at_time, lastchan, a, b);
		break;

	      case 4: /* Program change */
		MIDIEVENT_LAYER(smf_at_time, ME_PROGRAM, lastchan, a, 0);
		break;

	      case 5: /* Channel pressure */
		MIDIEVENT_LAYER(smf_at_time, ME_CHANNEL_PRESSURE, lastchan, a, 0);
		break;

	      case 6: /* Pitch wheel */
		b = tf_getc(tf) & 0x7F;
		MIDIEVENT_LAYER(smf_at_time, ME_PITCHWHEEL, lastchan, a, b);
		break;

	      default: /* case 7: */
		/* Ignore this event */
		switch(lastchan & 0xF)
		{
		  case 2: /* Sys Com Song Position Pntr */
		    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
			      "(Sys Com Song Position Pntr)");
		    tf_getc(tf);
		    tf_getc(tf);
		    break;

		  case 3: /* Sys Com Song Select(Song #) */
		    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
			      "(Sys Com Song Select(Song #))");
		    tf_getc(tf);
		    break;

		  case 6: /* Sys Com tune request */
		    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
			      "(Sys Com tune request)");
		    break;
		  case 8: /* Sys real time timing clock */
		    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
			      "(Sys real time timing clock)");
		    break;
		  case 10: /* Sys real time start */
		    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
			      "(Sys real time start)");
		    break;
		  case 11: /* Sys real time continue */
		    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
			      "(Sys real time continue)");
		    break;
		  case 12: /* Sys real time stop */
		    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
			      "(Sys real time stop)");
		    break;
		  case 14: /* Sys real time active sensing */
		    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
			      "(Sys real time active sensing)");
		    break;
#if 0
		  case 15: /* Meta */
		  case 0: /* SysEx */
		  case 7: /* SysEx */
#endif
		  default: /* 1, 4, 5, 9, 13 */
		    ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			      "*** Can't happen: status 0x%02X channel 0x%02X",
			      laststatus, lastchan & 0xF);
		    break;
		}
		}
	}
    }
    /*NOTREACHED*/
}

/* Free the linked event list from memory. */
static void free_midi_list(void)
{
    if(evlist != NULL)
    {
	reuse_mblock(&mempool);
	evlist = NULL;
    }
}

static void move_channels(int *chidx)
{
    int i, ch, maxch;
    MidiEventList *e;

    maxch = 0;
    for(i = 0; i < 256; i++)
	chidx[i] = -1;

    /* check channels */
    for(i = 0, e = evlist; i < event_count; i++, e = e->next)
    {
	if(!GLOBAL_CHANNEL_EVENT_TYPE(e->event.type))
	{
	    ch = e->event.channel;
	    if(maxch < ch)
		maxch = ch;
	    if(ch < REDUCE_CHANNELS)
		chidx[ch] = ch;
	}
    }

    if(maxch < REDUCE_CHANNELS)
    {
	current_file_info->max_channel = maxch;
	return;
    }

    /* Move channel if enable */
    maxch = 0;
    for(i = 0, e = evlist; i < event_count; i++, e = e->next)
    {
	if(!GLOBAL_CHANNEL_EVENT_TYPE(e->event.type))
	{
	    ch = e->event.channel;
	    if(chidx[ch] != -1)
		ch = e->event.channel = chidx[ch];
	    else /* -1 */
	    {
		int newch;

		newch = ch % REDUCE_CHANNELS;
		while(newch < ch && newch < MAX_CHANNELS)
		{
		    if(chidx[newch] == -1)
		    {
			ctl->cmsg(CMSG_INFO, VERB_VERBOSE,
				  "channel %d => %d", ch, newch);
			ch = e->event.channel = chidx[ch] = newch;
			break;
		    }
		    newch += REDUCE_CHANNELS;
		}
		if(chidx[ch] == -1)
		{
		    if(ch < MAX_CHANNELS)
			chidx[ch] = ch;
		    else
		    {
			newch = ch % MAX_CHANNELS;
			ctl->cmsg(CMSG_WARNING, VERB_VERBOSE,
				  "channel %d => %d (mixed)", ch, newch);
			ch = e->event.channel = chidx[ch] = newch;
		    }
		}
	    }
	    if(maxch < ch)
		maxch = ch;
	}
    }
    current_file_info->max_channel = maxch;
}

void change_system_mode(int mode)
{
    int mid;

    if(opt_system_mid)
    {
	mid = opt_system_mid;
	mode = -1; /* Always use opt_system_mid */
    }
    else
	mid = current_file_info->mid;
    switch(mode)
    {
      case GM_SYSTEM_MODE:
	if(play_system_mode == DEFAULT_SYSTEM_MODE)
	{
	    play_system_mode = GM_SYSTEM_MODE;
	    vol_table = def_vol_table;
	}
	break;
      case GS_SYSTEM_MODE:
	play_system_mode = GS_SYSTEM_MODE;
	vol_table = gs_vol_table;
	break;
      case XG_SYSTEM_MODE:
	play_system_mode = XG_SYSTEM_MODE;
	vol_table = xg_vol_table;
	break;
      default:
	switch(mid)
	{
	  case 0x41:
	    play_system_mode = GS_SYSTEM_MODE;
	    vol_table = gs_vol_table;
	    break;
	  case 0x43:
	    play_system_mode = XG_SYSTEM_MODE;
	    vol_table = xg_vol_table;
	    break;
	  case 0x7e:
	    play_system_mode = GM_SYSTEM_MODE;
	    vol_table = def_vol_table;
	    break;
	  default:
	    play_system_mode = DEFAULT_SYSTEM_MODE;
		vol_table = def_vol_table;
	    break;
	}
	break;
    }
}

int get_default_mapID(int ch)
{
    if(play_system_mode == XG_SYSTEM_MODE)
	return ISDRUMCHANNEL(ch) ? XG_DRUM_MAP : XG_NORMAL_MAP;
    return INST_NO_MAP;
}

/* Allocate an array of MidiEvents and fill it from the linked list of
   events, marking used instruments for loading. Convert event times to
   samples: handle tempo changes. Strip unnecessary events from the list.
   Free the linked list. */
static MidiEvent *groom_list(int32 divisions, int32 *eventsp, int32 *samplesp)
{
    MidiEvent *groomed_list, *lp;
    MidiEventList *meep;
    int32 i, j, our_event_count, tempo, skip_this_event;
    int32 sample_cum, samples_to_do, at, st, dt, counting_time;
    int ch, gch;
    uint8 current_set[MAX_CHANNELS],
	warn_tonebank[128], warn_drumset[128];
    int8 bank_lsb[MAX_CHANNELS], bank_msb[MAX_CHANNELS], mapID[MAX_CHANNELS];
    int current_program[MAX_CHANNELS];
    int wrd_args[WRD_MAXPARAM];
    int wrd_argc;
    int chidx[256];
    int newbank, newprog;

    move_channels(chidx);

    COPY_CHANNELMASK(drumchannels, current_file_info->drumchannels);
    COPY_CHANNELMASK(drumchannel_mask, current_file_info->drumchannel_mask);

    /* Move drumchannels */
    for(ch = REDUCE_CHANNELS; ch < MAX_CHANNELS; ch++)
    {
	i = chidx[ch];
	if(i != -1 && i != ch && !IS_SET_CHANNELMASK(drumchannel_mask, i))
	{
	    if(IS_SET_CHANNELMASK(drumchannels, ch))
		SET_CHANNELMASK(drumchannels, i);
	    else
		UNSET_CHANNELMASK(drumchannels, i);
	}
    }

    for(j = 0; j < MAX_CHANNELS; j++)
    {
	if(ISDRUMCHANNEL(j))
	    current_set[j] = 0;
	else
	{
	    if(special_tonebank >= 0)
		current_set[j] = special_tonebank;
	    else
		current_set[j] = default_tonebank;
	    if(tonebank[current_set[j]] == NULL)
	    {
		if(warn_tonebank[current_set[j]] == 0)
		{
		    ctl->cmsg(CMSG_WARNING, VERB_VERBOSE,
			      "Tone bank %d is undefined", newbank);
		    warn_tonebank[current_set[j]] = 1;
		}
		current_set[j] = 0;
	    }

	}
	bank_lsb[j] = bank_msb[j] = 0;
	if(play_system_mode == XG_SYSTEM_MODE && j % 16 == 9)
	    bank_msb[j] = 127; /* Use MSB=127 for XG */
	current_program[j] = default_program[j];
    }

    memset(warn_tonebank, 0, sizeof(warn_tonebank));
    memset(warn_drumset, 0, sizeof(warn_drumset));
    tempo = 500000;
    compute_sample_increment(tempo, divisions);

    /* This may allocate a bit more than we need */
    groomed_list = lp =
	(MidiEvent *)safe_malloc(sizeof(MidiEvent) * (event_count + 1));
    meep = evlist;

    our_event_count = 0;
    st = at = sample_cum = 0;
    counting_time = 2; /* We strip any silence before the first NOTE ON. */
    wrd_argc = 0;
    change_system_mode(DEFAULT_SYSTEM_MODE);

    for(j = 0; j < MAX_CHANNELS; j++)
	mapID[j] = get_default_mapID(j);

    for(i = 0; i < event_count; i++)
    {
	skip_this_event = 0;
	ch = meep->event.channel;
	gch = GLOBAL_CHANNEL_EVENT_TYPE(meep->event.type);
	if(!gch && ch >= MAX_CHANNELS) /* For safety */
	    meep->event.channel = ch = ch % MAX_CHANNELS;

	if(!gch && IS_SET_CHANNELMASK(quietchannels, ch))
	    skip_this_event = 1;
	else switch(meep->event.type)
	{
	  case ME_NONE:
	    skip_this_event = 1;
	    break;
	  case ME_RESET:
	    change_system_mode(meep->event.a);
	    ctl->cmsg(CMSG_INFO, VERB_NOISY, "MIDI reset at %d sec",
		      (int)((double)st / play_mode->rate + 0.5));
	    for(j = 0; j < MAX_CHANNELS; j++)
	    {
		if(play_system_mode == XG_SYSTEM_MODE && j % 16 == 9)
		    mapID[j] = XG_DRUM_MAP;
		else
		    mapID[j] = get_default_mapID(j);
		if(ISDRUMCHANNEL(j))
		    current_set[j] = 0;
		else
		{
		    if(special_tonebank >= 0)
			current_set[j] = special_tonebank;
		    else
			current_set[j] = default_tonebank;
		    if(tonebank[current_set[j]] == NULL)
			current_set[j] = 0;
		}
		bank_lsb[j] = bank_msb[j] = 0;
		if(play_system_mode == XG_SYSTEM_MODE && j % 16 == 9)
		{
		    bank_msb[j] = 127; /* Use MSB=127 for XG */
		}
		current_program[j] = default_program[j];
	    }
	    break;

	  case ME_PROGRAM:
	    if(ISDRUMCHANNEL(ch))
		newbank = current_program[ch];
	    else
		newbank = current_set[ch];
	    newprog = meep->event.a;
	    switch(play_system_mode)
	    {
	      case GS_SYSTEM_MODE:
		switch(bank_lsb[ch])
		{
		  case 0:	/* No change */
		    break;
		  case 1:
		    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
			      "(GS ch=%d SC-55 MAP)", ch);
		    mapID[ch] = (!ISDRUMCHANNEL(ch) ? SC_55_TONE_MAP
				 : SC_55_DRUM_MAP);
		    break;
		  case 2:
		    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
			      "(GS ch=%d SC-88 MAP)", ch);
		    mapID[ch] = (!ISDRUMCHANNEL(ch) ? SC_88_TONE_MAP
				 : SC_88_DRUM_MAP);
		    break;
		  case 3:
		    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
			      "(GS ch=%d SC-88Pro MAP)", ch);
		    mapID[ch] = (!ISDRUMCHANNEL(ch) ? SC_88PRO_TONE_MAP
				 : SC_88PRO_DRUM_MAP);
		    break;
		  default:
		    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
			      "(GS: ch=%d Strange bank LSB %d)",
			      ch, bank_lsb[ch]);
		    break;
		}
		newbank = bank_msb[ch];
		break;

	      case XG_SYSTEM_MODE: /* XG */
		switch(bank_msb[ch])
		{
		  case 0: /* Normal */
		    if(ch == 9  && bank_lsb[ch] == 127 && mapID[ch] == XG_DRUM_MAP) {
		      /* FIXME: Why this part is drum?  Is this correct? */
		      ctl->cmsg(CMSG_WARNING, VERB_NORMAL,
				"Warning: XG bank 0/127 is found. It may be not correctly played.");
		      ;
		    } else {
		      ctl->cmsg(CMSG_INFO, VERB_DEBUG, "(XG ch=%d Normal voice)",
				ch);
		      midi_drumpart_change(ch, 0);
		      mapID[ch] = XG_NORMAL_MAP;
		    }
		    break;
		  case 64: /* SFX voice */
		    ctl->cmsg(CMSG_INFO, VERB_DEBUG, "(XG ch=%d SFX voice)",
			      ch);
		    midi_drumpart_change(ch, 0);
		    mapID[ch] = XG_SFX64_MAP;
		    break;
		  case 126: /* SFX kit */
		    ctl->cmsg(CMSG_INFO, VERB_DEBUG, "(XG ch=%d SFX kit)", ch);
		    midi_drumpart_change(ch, 1);
		    mapID[ch] = XG_SFX126_MAP;
		    break;
		  case 127: /* Drum kit */
		    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
			      "(XG ch=%d Drum kit)", ch);
		    midi_drumpart_change(ch, 1);
		    mapID[ch] = XG_DRUM_MAP;
		    break;
		  default:
		    ctl->cmsg(CMSG_INFO, VERB_DEBUG,
			      "(XG: ch=%d Strange bank MSB %d)",
			      ch, bank_msb[ch]);
		    break;
		}
		newbank = bank_lsb[ch];
		break;

	      default:
		newbank = bank_msb[ch];
		break;
	    }

	    if(ISDRUMCHANNEL(ch))
		current_set[ch] = newprog;
	    else
	    {
		if(special_tonebank >= 0)
		    newbank = special_tonebank;
		if(current_program[ch] == SPECIAL_PROGRAM)
		    skip_this_event = 1;
		current_set[ch] = newbank;
	    }
	    current_program[ch] = newprog;
	    break;

	  case ME_NOTEON:
	    if(counting_time)
		counting_time = 1;
	    if(ISDRUMCHANNEL(ch))
	    {
		newbank = current_set[ch];
		newprog = meep->event.a;
		instrument_map(mapID[ch], &newbank, &newprog);

		if(!drumset[newbank]) /* Is this a defined drumset? */
		{
		    if(warn_drumset[newbank] == 0)
		    {
			ctl->cmsg(CMSG_WARNING, VERB_VERBOSE,
				  "Drum set %d is undefined", newbank);
			warn_drumset[newbank] = 1;
		    }
		    newbank = 0;
		}

		/* Mark this instrument to be loaded */
		if(!(drumset[newbank]->tone[newprog].instrument))
		    drumset[newbank]->tone[newprog].instrument =
			MAGIC_LOAD_INSTRUMENT;
	    }
	    else
	    {
		if(current_program[ch] == SPECIAL_PROGRAM)
		    break;
		newbank = current_set[ch];
		newprog = current_program[ch];
		instrument_map(mapID[ch], &newbank, &newprog);
		if(tonebank[newbank] == NULL)
		{
		    if(warn_tonebank[newbank] == 0)
		    {
			ctl->cmsg(CMSG_WARNING, VERB_VERBOSE,
				  "Tone bank %d is undefined", newbank);
			warn_tonebank[newbank] = 1;
		    }
		    newbank = 0;
		}

		/* Mark this instrument to be loaded */
		if(!(tonebank[newbank]->tone[newprog].instrument))
		    tonebank[newbank]->tone[newprog].instrument =
			MAGIC_LOAD_INSTRUMENT;
	    }
	    break;

	  case ME_TONE_BANK_MSB:
	    bank_msb[ch] = meep->event.a;
	    break;

	  case ME_TONE_BANK_LSB:
	    bank_lsb[ch] = meep->event.a;
	    break;

	  case ME_CHORUS_TEXT:		  		  
	  case ME_LYRIC:
	  case ME_MARKER:
	  case ME_INSERT_TEXT:
	  case ME_TEXT:
	  case ME_KARAOKE_LYRIC:
	    if((meep->event.a | meep->event.b) == 0)
		skip_this_event = 1;
	    else if(counting_time && ctl->trace_playing)
		counting_time = 1;
	    break;

	  case ME_DRUMPART:
	    midi_drumpart_change(ch, meep->event.a);
	    break;

	  case ME_WRD:
	    if(readmidi_wrd_mode == WRD_TRACE_MIMPI)
	    {
		wrd_args[wrd_argc++] = meep->event.a | 256 * meep->event.b;
		if(ch != WRD_ARG)
		{
		    if(ch == WRD_MAG) {
			wrdt->apply(WRD_MAGPRELOAD, wrd_argc, wrd_args);
		    }
		    else if(ch == WRD_PLOAD)
			wrdt->apply(WRD_PHOPRELOAD, wrd_argc, wrd_args);
		    else if(ch == WRD_PATH)
			wrdt->apply(WRD_PATH, wrd_argc, wrd_args);
		    wrd_argc = 0;
		}
	    }
	    if(counting_time == 2 && readmidi_wrd_mode != WRD_TRACE_NOTHING)
		counting_time = 1;
	    break;

	  case ME_SHERRY:
	    if(counting_time == 2)
		counting_time = 1;
	    break;

	  case ME_NOTE_STEP:
	    if(counting_time == 2)
		skip_this_event = 1;
	    break;
        }

	/* Recompute time in samples*/
	if((dt = meep->event.time - at) && !counting_time)
	{
	    samples_to_do = sample_increment * dt;
	    sample_cum += sample_correction * dt;
	    if(sample_cum & 0xFFFF0000)
	    {
		samples_to_do += ((sample_cum >> 16) & 0xFFFF);
		sample_cum &= 0x0000FFFF;
	    }
	    st += samples_to_do;
	    if(st < 0)
	    {
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "Overflow the sample counter");
		free(groomed_list);
		return NULL;
	    }
	}
	else if(counting_time == 1)
	    counting_time = 0;

	if(meep->event.type == ME_TEMPO)
	{
	    tempo = ch + meep->event.b * 256 + meep->event.a * 65536;
	    tempo *= tempo_adjust;
	    compute_sample_increment(tempo, divisions);
	}

	if(!skip_this_event)
	{
	    /* Add the event to the list */
	    *lp = meep->event;
	    lp->time = st;
	    lp++;
	    our_event_count++;
	}
	at = meep->event.time;
	meep = meep->next;
    }
    /* Add an End-of-Track event */
    lp->time = st;
    lp->type = ME_EOT;
    our_event_count++;
    free_midi_list();
    
    *eventsp = our_event_count;
    *samplesp = st;
    return groomed_list;
}

static int read_smf_file(struct timidity_file *tf)
{
    int32 len, divisions;
    int16 format, tracks, divisions_tmp;
    int i;

    if(current_file_info->file_type == IS_OTHER_FILE)
	current_file_info->file_type = IS_SMF_FILE;

    if(current_file_info->karaoke_title == NULL)
	karaoke_title_flag = 0;
    else
	karaoke_title_flag = 1;

    errno = 0;
    if(tf_read(&len, 4, 1, tf) != 1)
    {
	if(errno)
	    ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s", current_filename,
		      strerror(errno));
	else
	    ctl->cmsg(CMSG_WARNING, VERB_NORMAL,
		      "%s: Not a MIDI file!", current_filename);
	return 1;
    }
    len = BE_LONG(len);

    tf_read(&format, 2, 1, tf);
    tf_read(&tracks, 2, 1, tf);
    tf_read(&divisions_tmp, 2, 1, tf);
    format = BE_SHORT(format);
    tracks = BE_SHORT(tracks);
    divisions_tmp = BE_SHORT(divisions_tmp);

    if(divisions_tmp < 0)
    {
	/* SMPTE time -- totally untested. Got a MIDI file that uses this? */
	divisions=
	    (int32)(-(divisions_tmp / 256)) * (int32)(divisions_tmp & 0xFF);
    }
    else
	divisions = (int32)divisions_tmp;

    if(len > 6)
    {
	ctl->cmsg(CMSG_WARNING, VERB_NORMAL,
		  "%s: MIDI file header size %ld bytes",
		  current_filename, len);
	skip(tf, len - 6); /* skip the excess */
    }
    if(format < 0 || format > 2)
    {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		  "%s: Unknown MIDI file format %d", current_filename, format);
	return 1;
    }
    ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "Format: %d  Tracks: %d  Divisions: %d",
	      format, tracks, divisions);

    current_file_info->format = format;
    current_file_info->tracks = tracks;
    current_file_info->divisions = divisions;
    if(tf->url->url_tell != NULL)
	current_file_info->hdrsiz = (int16)tf_tell(tf);
    else
	current_file_info->hdrsiz = -1;

    switch(format)
    {
      case 0:
	if(read_smf_track(tf, 0, 1))
	{
	    if(ignore_midi_error)
		break;
	    return 1;
	}
	break;

      case 1:
	for(i = 0; i < tracks; i++)
	{
	    if(read_smf_track(tf, i, 1))
	    {
		if(ignore_midi_error)
		    break;
		return 1;
	    }
	}
	break;

      case 2: /* We simply play the tracks sequentially */
	for(i = 0; i < tracks; i++)
	{
	    if(read_smf_track(tf, i, 0))
	    {
		if(ignore_midi_error)
		    break;
		return 1;
	    }
	}
	break;
    }
    return 0;
}

void readmidi_read_init(void)
{
    int i;

	/* initialize effect status */
	for(i = 0; i < MAX_CHANNELS; i++) {init_channel_layer(i);}
	init_reverb_status();
	init_delay_status();
	init_chorus_status();
	init_eq_status();
	init_insertion_effect_status();
	init_userdrum();
	init_userinst();
	rhythm_part[0] = 9;
	rhythm_part[1] = 9;

    /* Put a do-nothing event first in the list for easier processing */
    evlist = current_midi_point = alloc_midi_event();
    evlist->event.time = 0;
    evlist->event.type = ME_NONE;
    evlist->event.channel = 0;
    evlist->event.a = 0;
    evlist->event.b = 0;
    evlist->prev = NULL;
    evlist->next = NULL;
    readmidi_error_flag = 0;
    event_count = 1;

    if(string_event_table != NULL)
    {
	free(string_event_table[0]);
	free(string_event_table);
	string_event_table = NULL;
	string_event_table_size = 0;
    }
    init_string_table(&string_event_strtab);
    karaoke_format = 0;

    for(i = 0; i < 256; i++)
	default_channel_program[i] = -1;
    readmidi_wrd_mode = WRD_TRACE_NOTHING;
}

static void insert_note_steps(void)
{
	MidiEventList *e;
	int32 i, n, at, lasttime, meas, beat;
	uint8 num, denom, a, b;
	
	e = evlist;
	for (i = n = 0; i < event_count - 1 && n < 256 - 1; i++, e = e->next)
		if (e->event.type == ME_TIMESIG && e->event.channel == 0) {
			if (n == 0 && e->event.time > 0) {	/* 4/4 is default */
				SETMIDIEVENT(timesig[n], 0, ME_TIMESIG, 0, 4, 4);
				n++;
			}
			if (n > 0 && e->event.a == timesig[n - 1].a
					&& e->event.b == timesig[n - 1].b)
				continue;	/* unchanged */
			if (n > 0 && e->event.time == timesig[n - 1].time)
				n--;	/* overwrite previous timesig */
			timesig[n++] = e->event;
		}
	if (n == 0) {
		SETMIDIEVENT(timesig[n], 0, ME_TIMESIG, 0, 4, 4);
		n++;
	}
	timesig[n] = timesig[n - 1];
	timesig[n].time = 0x7fffffff;	/* stopper */
	lasttime = e->event.time;
	readmidi_set_track(0, 1);
	at = n = meas = beat = 0;
	while (at < lasttime && ! readmidi_error_flag) {
		if (at >= timesig[n].time) {
			if (beat != 0)
				meas++, beat = 0;
			num = timesig[n].a, denom = timesig[n].b, n++;
		}
		a = meas & 0xff;
		b = (meas >> 8 & 0x0f) + (beat + 1 << 4);
		MIDIEVENT(at, ME_NOTE_STEP, 0, a, b);
		if (++beat == num)
			meas++, beat = 0;
		at += current_file_info->divisions * 4 / denom;
	}
}

MidiEvent *read_midi_file(struct timidity_file *tf, int32 *count, int32 *sp,
			  char *fn)
{
    char magic[4];
    MidiEvent *ev;
    int err, macbin_check, mtype, i;

    macbin_check = 1;
    current_file_info = get_midi_file_info(current_filename, 1);
    COPY_CHANNELMASK(drumchannels, current_file_info->drumchannels);
    COPY_CHANNELMASK(drumchannel_mask, current_file_info->drumchannel_mask);

    errno = 0;

    if((mtype = get_module_type(fn)) > 0)
    {
	readmidi_read_init();
    	if(!IS_URL_SEEK_SAFE(tf->url))
    	    tf->url = url_cache_open(tf->url, 1);
  	err = load_module_file(tf, mtype);
	if(!err)
	{
	    current_file_info->format = 0;
	    memset(&drumchannels, 0, sizeof(drumchannels));
	    goto grooming;
	}
	free_midi_list();

	if(err == 2)
	    return NULL;
	url_rewind(tf->url);
	url_cache_disable(tf->url);
    }

#if MAX_CHANNELS > 16
    for(i = 16; i < MAX_CHANNELS; i++)
    {
	if(!IS_SET_CHANNELMASK(drumchannel_mask, i))
	{
	    if(IS_SET_CHANNELMASK(drumchannels, i & 0xF))
		SET_CHANNELMASK(drumchannels, i);
	    else
		UNSET_CHANNELMASK(drumchannels, i);
	}
    }
#endif

    if(opt_default_mid &&
       (current_file_info->mid == 0 || current_file_info->mid >= 0x7e))
	current_file_info->mid = opt_default_mid;

  retry_read:
    if(tf_read(magic, 1, 4, tf) != 4)
    {
	if(errno)
	    ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s", current_filename,
		      strerror(errno));
	else
	    ctl->cmsg(CMSG_WARNING, VERB_NORMAL,
		      "%s: Not a MIDI file!", current_filename);
	return NULL;
    }

    if(memcmp(magic, "MThd", 4) == 0)
    {
	readmidi_read_init();
	err = read_smf_file(tf);
    }
    else if(memcmp(magic, "RCM-", 4) == 0 || memcmp(magic, "COME", 4) == 0)
    {
	readmidi_read_init();
	err = read_rcp_file(tf, magic, fn);
    }
    else if(memcmp(magic, "melo", 4) == 0)
    {
	readmidi_read_init();
	err = read_mfi_file(tf);
    }
    else
    {
	if(macbin_check && magic[0] == 0)
	{
	    /* Mac Binary */
	    macbin_check = 0;
	    skip(tf, 128 - 4);
	    goto retry_read;
	}
	else if(memcmp(magic, "RIFF", 4) == 0)
	{
	    /* RIFF MIDI file */
	    skip(tf, 20 - 4);
	    goto retry_read;
	}
	err = 1;
	ctl->cmsg(CMSG_WARNING, VERB_NORMAL,
		  "%s: Not a MIDI file!", current_filename);
    }

    if(err)
    {
	free_midi_list();
	if(string_event_strtab.nstring > 0)
	    delete_string_table(&string_event_strtab);
	return NULL;
    }

    /* Read WRD file */
    if(!(play_mode->flag&PF_CAN_TRACE))
    {
	if(wrdt->start != NULL)
	    wrdt->start(WRD_TRACE_NOTHING);
	readmidi_wrd_mode = WRD_TRACE_NOTHING;
    }
    else if(wrdt->id != '-' && wrdt->opened)
    {
	readmidi_wrd_mode = import_wrd_file(fn);
	if(wrdt->start != NULL)
	    if(wrdt->start(readmidi_wrd_mode) == -1)
	    {
		/* strip all WRD events */
		MidiEventList *e;
		int32 i;
		for(i = 0, e = evlist; i < event_count; i++, e = e->next)
		    if(e->event.type == ME_WRD)
			e->event.type = ME_NONE;
	    }
    }
    else
	readmidi_wrd_mode = WRD_TRACE_NOTHING;

    /* make lyric table */
    if(string_event_strtab.nstring > 0)
    {
	string_event_table_size = string_event_strtab.nstring;
	string_event_table = make_string_array(&string_event_strtab);
	if(string_event_table == NULL)
	{
	    delete_string_table(&string_event_strtab);
	    string_event_table_size = 0;
	}
    }

  grooming:
    insert_note_steps();
    ev = groom_list(current_file_info->divisions, count, sp);
    if(ev == NULL)
    {
	free_midi_list();
	if(string_event_strtab.nstring > 0)
	    delete_string_table(&string_event_strtab);
	return NULL;
    }
    current_file_info->samples = *sp;
    if(current_file_info->first_text == NULL)
	current_file_info->first_text = safe_strdup("");
    current_file_info->readflag = 1;
    return ev;
}


struct midi_file_info *new_midi_file_info(const char *filename)
{
    struct midi_file_info *p;
    p = (struct midi_file_info *)safe_malloc(sizeof(struct midi_file_info));

    /* Initialize default members */
    memset(p, 0, sizeof(struct midi_file_info));
    p->hdrsiz = -1;
    p->format = -1;
    p->tracks = -1;
    p->divisions = -1;
    p->time_sig_n = p->time_sig_d = -1;
    p->samples = -1;
    p->max_channel = -1;
    p->file_type = IS_OTHER_FILE;
    if(filename != NULL)
	p->filename = safe_strdup(filename);
    COPY_CHANNELMASK(p->drumchannels, default_drumchannels);
    COPY_CHANNELMASK(p->drumchannel_mask, default_drumchannel_mask);

    /* Append to midi_file_info */
    p->next = midi_file_info;
    midi_file_info = p;

    return p;
}

void free_all_midi_file_info(void)
{
  struct midi_file_info *info, *next;

  info = midi_file_info;
  while (info) {
    next = info->next;
    free(info->filename);
    if (info->seq_name)
      free(info->seq_name);
    if (info->karaoke_title != NULL && info->karaoke_title == info->first_text)
      free(info->karaoke_title);
    else {
      if (info->karaoke_title)
	free(info->karaoke_title);
      if (info->first_text)
	free(info->first_text);
      if (info->midi_data)
	free(info->midi_data);
      if (info->pcm_filename)
	free(info->pcm_filename); /* Note: this memory is freed in playmidi.c*/
    }
    free(info);
    info = next;
  }
  midi_file_info = NULL;
  current_file_info = NULL;
}

struct midi_file_info *get_midi_file_info(char *filename, int newp)
{
    struct midi_file_info *p;

    filename = url_expand_home_dir(filename);
    /* Linear search */
    for(p = midi_file_info; p; p = p->next)
	if(!strcmp(filename, p->filename))
	    return p;
    if(newp)
	return new_midi_file_info(filename);
    return NULL;
}

struct timidity_file *open_midi_file(char *fn,
				     int decompress, int noise_mode)
{
    struct midi_file_info *infop;
    struct timidity_file *tf;
#if defined(SMFCONV) && defined(__W32__)
    extern int opt_rcpcv_dll;
#endif

    infop = get_midi_file_info(fn, 0);
    if(infop == NULL || infop->midi_data == NULL)
	tf = open_file(fn, decompress, noise_mode);
    else
    {
	tf = open_with_mem(infop->midi_data, infop->midi_data_size,
			   noise_mode);
	if(infop->compressed)
	{
	    if((tf->url = url_inflate_open(tf->url, infop->midi_data_size, 1))
	       == NULL)
	    {
		close_file(tf);
		return NULL;
	    }
	}
    }

#if defined(SMFCONV) && defined(__W32__)
    /* smf convert */
    if(tf != NULL && opt_rcpcv_dll)
    {
    	if(smfconv_w32(tf, fn))
	{
	    close_file(tf);
	    return NULL;
	}
    }
#endif

    return tf;
}

#ifndef NO_MIDI_CACHE
static long deflate_url_reader(char *buf, long size, void *user_val)
{
    return url_nread((URL)user_val, buf, size);
}

/*
 * URL data into deflated buffer.
 */
static void url_make_file_data(URL url, struct midi_file_info *infop)
{
    char buff[BUFSIZ];
    MemBuffer b;
    long n;
    DeflateHandler compressor;

    init_memb(&b);

    /* url => b */
    if((compressor = open_deflate_handler(deflate_url_reader, url,
					  ARC_DEFLATE_LEVEL)) == NULL)
	return;
    while((n = zip_deflate(compressor, buff, sizeof(buff))) > 0)
	push_memb(&b, buff, n);
    close_deflate_handler(compressor);
    infop->compressed = 1;

    /* b => mem */
    infop->midi_data_size = b.total_size;
    rewind_memb(&b);
    infop->midi_data = (void *)safe_malloc(infop->midi_data_size);
    read_memb(&b, infop->midi_data, infop->midi_data_size);
    delete_memb(&b);
}

static int check_need_cache(URL url, char *filename)
{
    int t1, t2;
    t1 = url_check_type(filename);
    t2 = url->type;
    return (t1 == URL_http_t || t1 == URL_ftp_t || t1 == URL_news_t)
	 && t2 != URL_arc_t;
}
#else
/*ARGSUSED*/
static void url_make_file_data(URL url, struct midi_file_info *infop)
{
}
/*ARGSUSED*/
static int check_need_cache(URL url, char *filename)
{
    return 0;
}
#endif /* NO_MIDI_CACHE */

int check_midi_file(char *filename)
{
    struct midi_file_info *p;
    struct timidity_file *tf;
    char tmp[4];
    int32 len;
    int16 format;
    int check_cache;

    if(filename == NULL)
    {
	if(current_file_info == NULL)
	    return -1;
	filename = current_file_info->filename;
    }

    p = get_midi_file_info(filename, 0);
    if(p != NULL)
	return p->format;
    p = get_midi_file_info(filename, 1);

    if(get_module_type(filename) > 0)
    {
	p->format = 0;
	return 0;
    }

    tf = open_file(filename, 1, OF_SILENT);
    if(tf == NULL)
	return -1;

    check_cache = check_need_cache(tf->url, filename);
    if(check_cache)
    {
	if(!IS_URL_SEEK_SAFE(tf->url))
	{
	    if((tf->url = url_cache_open(tf->url, 1)) == NULL)
	    {
		close_file(tf);
		return -1;
	    }
	}
    }

    /* Parse MIDI header */
    if(tf_read(tmp, 1, 4, tf) != 4)
    {
	close_file(tf);
	return -1;
    }

    if(tmp[0] == 0)
    {
	skip(tf, 128 - 4);
	if(tf_read(tmp, 1, 4, tf) != 4)
	{
	    close_file(tf);
	    return -1;
	}
    }

    if(strncmp(tmp, "RCM-", 4) == 0 ||
       strncmp(tmp, "COME", 4) == 0 ||
       strncmp(tmp, "M1", 2) == 0)
    {
	p->format = 1;
	goto end_of_header;
    }

    if(strncmp(tmp, "MThd", 4) != 0)
    {
	close_file(tf);
	return -1;
    }

    if(tf_read(&len, 4, 1, tf) != 1)
    {
	close_file(tf);
	return -1;
    }
    len = BE_LONG(len);

    tf_read(&format, 2, 1, tf);
    format = BE_SHORT(format);
    if(format < 0 || format > 2)
    {
	close_file(tf);
	return -1;
    }
    skip(tf, len - 2);

    p->format = format;
    p->hdrsiz = (int16)tf_tell(tf);

  end_of_header:
    if(check_cache)
    {
	url_rewind(tf->url);
	url_cache_disable(tf->url);
	url_make_file_data(tf->url, p);
    }
    close_file(tf);
    return format;
}

static char *get_midi_title1(struct midi_file_info *p)
{
    char *s;

    if(p->format != 0 && p->format != 1)
	return NULL;

    if((s = p->seq_name) == NULL)
	if((s = p->karaoke_title) == NULL)
	    s = p->first_text;
    if(s != NULL)
    {
	int all_space, i;

	all_space = 1;
	for(i = 0; s[i]; i++)
	    if(s[i] != ' ')
	    {
		all_space = 0;
		break;
	    }
	if(all_space)
	    s = NULL;
    }
    return s;
}

char *get_midi_title(char *filename)
{
    struct midi_file_info *p;
    struct timidity_file *tf;
    char tmp[4];
    int32 len;
    int16 format, tracks, trk;
    int laststatus, check_cache;
    int mtype;

    if(filename == NULL)
    {
	if(current_file_info == NULL)
	    return NULL;
	filename = current_file_info->filename;
    }

    p = get_midi_file_info(filename, 0);
    if(p == NULL)
	p = get_midi_file_info(filename, 1);
    else 
    {
	if(p->seq_name != NULL || p->first_text != NULL || p->format < 0)
	    return get_midi_title1(p);
    }

    tf = open_file(filename, 1, OF_SILENT);
    if(tf == NULL)
	return NULL;

    mtype = get_module_type(filename);
    check_cache = check_need_cache(tf->url, filename);
    if(check_cache || mtype > 0)
    {
	if(!IS_URL_SEEK_SAFE(tf->url))
	{
	    if((tf->url = url_cache_open(tf->url, 1)) == NULL)
	    {
		close_file(tf);
		return NULL;
	    }
	}
    }

    if(mtype > 0)
    {
	char *title, *str;

	title = get_module_title(tf, mtype);
	if(title == NULL)
	{
	    /* No title */
	    p->seq_name = NULL;
	    p->format = 0;
	    goto end_of_parse;
	}

	len = (int32)strlen(title);
	len = SAFE_CONVERT_LENGTH(len);
	str = (char *)new_segment(&tmpbuffer, len);
	code_convert(title, str, len, NULL, NULL);
	p->seq_name = (char *)safe_strdup(str);
	reuse_mblock(&tmpbuffer);
	p->format = 0;
	free (title);
	goto end_of_parse;
    }

    /* Parse MIDI header */
    if(tf_read(tmp, 1, 4, tf) != 4)
    {
	close_file(tf);
	return NULL;
    }

    if(tmp[0] == 0)
    {
	skip(tf, 128 - 4);
	if(tf_read(tmp, 1, 4, tf) != 4)
	{
	    close_file(tf);
	    return NULL;
	}
    }

    if(memcmp(tmp, "RCM-", 4) == 0 || memcmp(tmp, "COME", 4) == 0)
    {
    	int i;
	char local[0x40 + 1];
	char *str;

	p->format = 1;
	skip(tf, 0x20 - 4);
	tf_read(local, 1, 0x40, tf);
	local[0x40]='\0';

	for(i = 0x40 - 1; i >= 0; i--)
	{
	    if(local[i] == 0x20)
		local[i] = '\0';
	    else if(local[i] != '\0')
		break;
	}

	i = SAFE_CONVERT_LENGTH(i + 1);
	str = (char *)new_segment(&tmpbuffer, i);
	code_convert(local, str, i, NULL, NULL);
	p->seq_name = (char *)safe_strdup(str);
	reuse_mblock(&tmpbuffer);
	p->format = 1;
	goto end_of_parse;
    }
    if(memcmp(tmp, "melo", 4) == 0)
    {
	int i;
	char *master, *converted;
	
	master = get_mfi_file_title(tf);
	if (master != NULL)
	{
	    i = SAFE_CONVERT_LENGTH(strlen(master) + 1);
	    converted = (char *)new_segment(&tmpbuffer, i);
	    code_convert(master, converted, i, NULL, NULL);
	    p->seq_name = (char *)safe_strdup(converted);
	    reuse_mblock(&tmpbuffer);
	}
	else
	{
	    p->seq_name = (char *)safe_malloc(1);
	    p->seq_name[0] = '\0';
	}
	p->format = 0;
	goto end_of_parse;
    }

    if(strncmp(tmp, "M1", 2) == 0)
    {
	/* I don't know MPC file format */
	p->format = 1;
	goto end_of_parse;
    }

	  if(strncmp(tmp, "RIFF", 4) == 0)
	  {
	/* RIFF MIDI file */
	skip(tf, 20 - 4);
  if(tf_read(tmp, 1, 4, tf) != 4)
    {
	close_file(tf);
	return NULL;
    }
	  }

    if(strncmp(tmp, "MThd", 4) != 0)
    {
	close_file(tf);
	return NULL;
    }

    if(tf_read(&len, 4, 1, tf) != 1)
    {
	close_file(tf);
	return NULL;
    }

    len = BE_LONG(len);

    tf_read(&format, 2, 1, tf);
    tf_read(&tracks, 2, 1, tf);
    format = BE_SHORT(format);
    tracks = BE_SHORT(tracks);
    p->format = format;
    p->tracks = tracks;
    if(format < 0 || format > 2)
    {
	p->format = -1;
	close_file(tf);
	return NULL;
    }

    skip(tf, len - 4);
    p->hdrsiz = (int16)tf_tell(tf);

    if(format == 2)
	goto end_of_parse;

    if(tracks >= 3)
    {
	tracks = 3;
	karaoke_format = 0;
    }
    else
    {
	tracks = 1;
	karaoke_format = -1;
    }

    for(trk = 0; trk < tracks; trk++)
    {
	int32 next_pos, pos;

	if(trk >= 1 && karaoke_format == -1)
	    break;

	if((tf_read(tmp,1,4,tf) != 4) || (tf_read(&len,4,1,tf) != 1))
	    break;

	if(memcmp(tmp, "MTrk", 4))
	    break;

	next_pos = tf_tell(tf) + len;
	laststatus = -1;
	for(;;)
	{
	    int i, me, type;

	    /* skip Variable-length quantity */
	    do
	    {
		if((i = tf_getc(tf)) == EOF)
		    goto end_of_parse;
	    } while (i & 0x80);

	    if((me = tf_getc(tf)) == EOF)
		goto end_of_parse;

	    if(me == 0xF0 || me == 0xF7) /* SysEx */
	    {
		if((len = getvl(tf)) < 0)
		    goto end_of_parse;
		if((p->mid == 0 || p->mid >= 0x7e) && len > 0 && me == 0xF0)
		{
		    p->mid = tf_getc(tf);
		    len--;
		}
		skip(tf, len);
	    }
	    else if(me == 0xFF) /* Meta */
	    {
		type = tf_getc(tf);
		if((len = getvl(tf)) < 0)
		    goto end_of_parse;
		if((type == 1 || type == 3) && len > 0 &&
		   (trk == 0 || karaoke_format != -1))
		{
		    char *si, *so;
		    int s_maxlen = SAFE_CONVERT_LENGTH(len);

		    si = (char *)new_segment(&tmpbuffer, len + 1);
		    so = (char *)new_segment(&tmpbuffer, s_maxlen);

		    if(len != tf_read(si, 1, len, tf))
		    {
			reuse_mblock(&tmpbuffer);
			goto end_of_parse;
		    }

		    si[len]='\0';
		    code_convert(si, so, s_maxlen, NULL, NULL);
		    if(trk == 0 && type == 3)
		    {
		      if(p->seq_name == NULL) {
			char *name = safe_strdup(so);
			p->seq_name = safe_strdup(fix_string(name));
			free(name);
		      }
		      reuse_mblock(&tmpbuffer);
		      if(karaoke_format == -1)
			goto end_of_parse;
		    }
		    if(p->first_text == NULL) {
		      char *name;
		      name = safe_strdup(so);
		      p->first_text = safe_strdup(fix_string(name));
		      free(name);
		    }
		    if(karaoke_format != -1)
		    {
			if(trk == 1 && strncmp(si, "@KMIDI", 6) == 0)
			    karaoke_format = 1;
			else if(karaoke_format == 1 && trk == 2)
			    karaoke_format = 2;
		    }
		    if(type == 1 && karaoke_format == 2)
		    {
			if(strncmp(si, "@T", 2) == 0)
			    p->karaoke_title =
				add_karaoke_title(p->karaoke_title, si + 2);
			else if(si[0] == '\\')
			    goto end_of_parse;
		    }
		    reuse_mblock(&tmpbuffer);
		}
		else if(type == 0x2F)
		{
		    pos = tf_tell(tf);
		    if(pos < next_pos)
			tf_seek(tf, next_pos - pos, SEEK_CUR);
		    break; /* End of track */
		}
		else
		    skip(tf, len);
	    }
	    else /* MIDI event */
	    {
		/* skip MIDI event */
		karaoke_format = -1;
		if(trk != 0)
		    goto end_of_parse;

		if(me & 0x80) /* status byte */
		{
		    laststatus = (me >> 4) & 0x07;
		    if(laststatus != 7)
			tf_getc(tf);
		}

		switch(laststatus)
		{
		  case 0: case 1: case 2: case 3: case 6:
		    tf_getc(tf);
		    break;
		  case 7:
		    if(!(me & 0x80))
			break;
		    switch(me & 0x0F)
		    {
		      case 2:
			tf_getc(tf);
			tf_getc(tf);
			break;
		      case 3:
			tf_getc(tf);
			break;
		    }
		    break;
		}
	    }
	}
    }

  end_of_parse:
    if(check_cache)
    {
	url_rewind(tf->url);
	url_cache_disable(tf->url);
	url_make_file_data(tf->url, p);
    }
    close_file(tf);
    if(p->first_text == NULL)
	p->first_text = safe_strdup("");
    return get_midi_title1(p);
}

int midi_file_save_as(char *in_name, char *out_name)
{
    struct timidity_file *tf;
    FILE* ofp;
    char buff[BUFSIZ];
    long n;

    if(in_name == NULL)
    {
	if(current_file_info == NULL)
	    return 0;
	in_name = current_file_info->filename;
    }
    out_name = (char *)url_expand_home_dir(out_name);

    ctl->cmsg(CMSG_INFO, VERB_NORMAL, "Save as %s...", out_name);

    errno = 0;
    if((tf = open_midi_file(in_name, 1, 0)) == NULL)
    {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		  "%s: %s", out_name,
		  errno ? strerror(errno) : "Can't save file");
	return -1;
    }

    errno = 0;
    if((ofp = fopen(out_name, "wb")) == NULL)
    {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		  "%s: %s", out_name,
		  errno ? strerror(errno) : "Can't save file");
	close_file(tf);
	return -1;
    }

    while((n = tf_read(buff, 1, sizeof(buff), tf)) > 0)
	fwrite(buff, 1, n, ofp);
    ctl->cmsg(CMSG_INFO, VERB_NORMAL, "Save as %s...Done", out_name);

    fclose(ofp);
    close_file(tf);
    return 0;
}

char *event2string(int id)
{
    if(id == 0)
	return "";
#ifdef ABORT_AT_FATAL
    if(id >= string_event_table_size)
	abort();
#endif /* ABORT_AT_FATAL */
    if(string_event_table == NULL || id < 0 || id >= string_event_table_size)
	return NULL;
    return string_event_table[id];
}

void init_delay_status()
{
	delay_status.type = 0;
	delay_status.level = 0x40;
	delay_status.level_center = 0x7F;
	delay_status.level_left = 0;
	delay_status.level_right = 0;
	delay_status.time_center = 340.0;
	delay_status.time_ratio_left = 1.0 / 24.0;
	delay_status.time_ratio_right = 1.0 / 24.0;
	delay_status.feedback = 0x50;
	delay_status.pre_lpf = 0;
	recompute_delay_status();
}

void recompute_delay_status()
{
	int cutoff_freq;

	delay_status.sample_c = delay_status.time_center * play_mode->rate / 1000;
	delay_status.sample_l = delay_status.sample_c * delay_status.time_ratio_left;
	delay_status.sample_r = delay_status.sample_c * delay_status.time_ratio_right;
	if(delay_status.sample_c > play_mode->rate) {delay_status.sample_c = play_mode->rate;}
	if(delay_status.sample_l > play_mode->rate) {delay_status.sample_l = play_mode->rate;}
	if(delay_status.sample_r > play_mode->rate) {delay_status.sample_r = play_mode->rate;}
	delay_status.level_ratio_c = (double)delay_status.level * (double)delay_status.level_center / 16129.0;
	delay_status.level_ratio_l = (double)delay_status.level * (double)delay_status.level_left / 16129.0;
	delay_status.level_ratio_r = (double)delay_status.level * (double)delay_status.level_right / 16129.0;
	delay_status.feedback_ratio = (double)(delay_status.feedback - 64) * 0.0153125;
	delay_status.send_reverb_ratio = (double)delay_status.send_reverb / 127.0;

	if(delay_status.level_left || delay_status.level_right && delay_status.type == 0) {
		delay_status.type = 1;
	}

	switch(delay_status.pre_lpf) {
	case 0:	cutoff_freq = 0;	break;
	case 1:	cutoff_freq = 8000;	break;
	case 2:	cutoff_freq = 5000;	break;
	case 3:	cutoff_freq = 3150;	break;
	case 4:	cutoff_freq = 2000;	break;
	case 5:	cutoff_freq = 1250;	break;
	case 6:	cutoff_freq = 800;	break;
	case 7:	cutoff_freq = 500;	break;
	}

	/* pre-calculate LPF coefficients */
	if(cutoff_freq < play_mode->rate / 2) {
		calc_lowpass_coefs_24db(delay_status.lpf_coef,cutoff_freq,0,play_mode->rate);
	}
}

void set_delay_macro(int macro)
{
	if(macro > 3) {delay_status.type = 2;}
	macro *= 10;
	delay_status.time_center = delay_time_center_table[delay_macro_presets[macro+1]];
	delay_status.time_ratio_left = (double)delay_macro_presets[macro+2] / 24;
	delay_status.time_ratio_right = (double)delay_macro_presets[macro+3] / 24;
	delay_status.level_center = delay_macro_presets[macro+4];
	delay_status.level_left = delay_macro_presets[macro+5];
	delay_status.level_right = delay_macro_presets[macro+6];
	delay_status.level = delay_macro_presets[macro+7];
	delay_status.feedback = delay_macro_presets[macro+8];
}

void init_reverb_status()
{
	reverb_status.character = 0x04;
	reverb_status.pre_lpf = 0;
	reverb_status.level = 0x40;
	reverb_status.time = 0x40;
	reverb_status.delay_feedback = 0;
	reverb_status.pre_delay_time = 0;
	recompute_reverb_status();
	init_reverb(play_mode->rate);
}

void recompute_reverb_status()
{
	reverb_status.level_ratio = (double)reverb_status.level / 127.0f;
	reverb_status.time_ratio = (double)reverb_status.time / 128.0f + 0.5f;
}

void set_reverb_macro(int macro)
{
	macro *= 6;
	reverb_status.character = reverb_macro_presets[macro];
	reverb_status.pre_lpf = reverb_macro_presets[macro+1];
	reverb_status.level = reverb_macro_presets[macro+2];
	reverb_status.time = reverb_macro_presets[macro+3];
	reverb_status.delay_feedback = reverb_macro_presets[macro+4];
	reverb_status.pre_delay_time = reverb_macro_presets[macro+5];
}

void init_chorus_status()
{
	chorus_param.chorus_macro = 0;
	chorus_param.chorus_pre_lpf = 0;
	chorus_param.chorus_level = 0x40;
	chorus_param.chorus_feedback = 0x08;
	chorus_param.chorus_delay = 0x50;
	chorus_param.chorus_rate = 0x03;
	chorus_param.chorus_depth = 0x13;
	chorus_param.chorus_send_level_to_reverb = 0;
	chorus_param.chorus_send_level_to_delay = 0;
	recompute_chorus_status();
	init_chorus_lfo();
}

void recompute_chorus_status()
{
	int cutoff_freq;

	chorus_param.delay_in_sample = pre_delay_time_table[chorus_param.chorus_delay] * (double)play_mode->rate / 1000.0;
	chorus_param.depth_in_sample = chorus_param.delay_in_sample * chorus_param.chorus_depth / 127;
	chorus_param.cycle_in_sample = play_mode->rate / rate1_table[chorus_param.chorus_rate];
	chorus_param.feedback_ratio = (double)chorus_param.chorus_feedback * 0.0077165;
	chorus_param.level_ratio = (double)chorus_param.chorus_level / 127.0;
	chorus_param.send_reverb_ratio = (double)chorus_param.chorus_send_level_to_reverb / 127.0;
	chorus_param.send_delay_ratio = (double)chorus_param.chorus_send_level_to_delay / 127.0;

	switch(chorus_param.chorus_pre_lpf) {
	case 0:	cutoff_freq = 0;	break;
	case 1:	cutoff_freq = 8000;	break;
	case 2:	cutoff_freq = 5000;	break;
	case 3:	cutoff_freq = 3150;	break;
	case 4:	cutoff_freq = 2000;	break;
	case 5:	cutoff_freq = 1250;	break;
	case 6:	cutoff_freq = 800;	break;
	case 7:	cutoff_freq = 500;	break;
	}

	if(cutoff_freq < play_mode->rate / 2) {	/* pre-calculate LPF coefficients */
		calc_lowpass_coefs_24db(chorus_param.lpf_coef,cutoff_freq,0,play_mode->rate);
	}
}

void set_chorus_macro(int macro)
{
	macro *= 8;
	chorus_param.chorus_pre_lpf = chorus_macro_presets[macro];
	chorus_param.chorus_level = chorus_macro_presets[macro+1];
	chorus_param.chorus_feedback = chorus_macro_presets[macro+2];
	chorus_param.chorus_delay = chorus_macro_presets[macro+3];
	chorus_param.chorus_rate = chorus_macro_presets[macro+4];
	chorus_param.chorus_depth = chorus_macro_presets[macro+5];
	chorus_param.chorus_send_level_to_reverb = chorus_macro_presets[macro+6];
	chorus_param.chorus_send_level_to_delay = chorus_macro_presets[macro+7];
}

void init_eq_status()
{
	eq_status.low_freq = 0;
	eq_status.low_gain = 0x40;
	eq_status.high_freq = 0;
	eq_status.high_gain = 0x40;
	recompute_eq_status();
}

void recompute_eq_status()
{
	int32 freq;
	FLOAT_T dbGain;

	/* Lowpass Shelving Filter */
	if(eq_status.low_freq == 0) {freq = 200;}
	else {freq = 400;}
	dbGain = eq_status.low_gain - 0x40;
	if(freq < play_mode->rate / 2) {
		calc_lowshelf_coefs(eq_status.low_coef,freq,dbGain,play_mode->rate);
	}

	/* Highpass Shelving Filter */
	if(eq_status.high_freq == 0) {freq = 3000;}
	else {freq = 6000;}
	dbGain = eq_status.high_gain - 0x40;
	if(freq < play_mode->rate / 2) {
		calc_highshelf_coefs(eq_status.high_coef,freq,dbGain,play_mode->rate);
	}
}

void recompute_userdrum_altassign(int bank, int group)
{
	int number = 0, i;
	char *params[131], param[10];
	ToneBank *bk;
	UserDrumset *p;
	
	for(p = userdrum_first; p != NULL; p = p->next) {
		if(p->assign_group == group) {
			sprintf(param,"%d",i);
			params[number] = safe_strdup(param);
			number++;
		}
	}
	params[number] = NULL;

	alloc_instrument_bank(1, bank);
	bk = drumset[bank];
	bk->alt = add_altassign_string(bk->alt, params, number);
}

void init_userdrum()
{
	int i;
	AlternateAssign *alt;

	free_userdrum();

	for(i=0;i<2;i++) {	/* allocate alternative assign */
		alt = (AlternateAssign *)safe_malloc(sizeof(AlternateAssign));
		memset(alt, 0, sizeof(AlternateAssign));
		alloc_instrument_bank(1, 64 + i);
		drumset[64 + i]->alt = alt;
	}
}

void recompute_userdrum(int bank, int prog)
{
	UserDrumset *p;

	p = get_userdrum(bank, prog);

	if(drumset[bank]->tone[prog].name) {free_tone_bank_element(1, bank, prog);}
	if(drumset[p->source_prog]) {
		if(drumset[p->source_prog]->tone[p->source_note].name) {
			memcpy(&drumset[bank]->tone[prog], &drumset[p->source_prog]->tone[p->source_note], sizeof(ToneBankElement));
			dup_tone_bank_element(1, bank, prog);
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"User Drumset (%d %d -> %d %d)", p->source_prog, p->source_note, bank, prog);
		} else if(drumset[0]->tone[p->source_note].name) {
			memcpy(&drumset[bank]->tone[prog], &drumset[0]->tone[p->source_note], sizeof(ToneBankElement));
			dup_tone_bank_element(1, bank, prog);
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"User Drumset (%d %d -> %d %d)", 0, p->source_note, bank, prog);
		}
	}
}

UserDrumset *get_userdrum(int bank, int prog)
{
	UserDrumset *p;

	for(p = userdrum_first; p != NULL; p = p->next) {
		if(p->bank == bank && p->prog == prog) {return p;}
	}

	p = (UserDrumset *)safe_malloc(sizeof(UserDrumset));
	memset(p, 0, sizeof(UserDrumset));
	p->next = NULL;
	if(userdrum_first == NULL) {
		userdrum_first = p;
		userdrum_last = p;
	} else {
		userdrum_last->next = p;
		userdrum_last = p;
	}
	p->bank = bank;
	p->prog = prog;

	return p;
}

void free_userdrum()
{
	UserDrumset *p, *next;

	for(p = userdrum_first; p != NULL; p = next){
		next = p->next;
		free(p);
    }
	userdrum_first = userdrum_last = NULL;
}

void init_userinst()
{
	free_userinst();
}

void recompute_userinst(int bank, int prog)
{
	UserInstrument *p;

	p = get_userinst(bank, prog);

	if(tonebank[bank]->tone[prog].name) {free_tone_bank_element(0, bank, prog);}
	if(tonebank[p->source_bank]) {
		if(tonebank[p->source_bank]->tone[p->source_prog].name) {
			memcpy(&tonebank[bank]->tone[prog], &tonebank[p->source_bank]->tone[p->source_prog], sizeof(ToneBankElement));
			dup_tone_bank_element(0, bank, prog);
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"User Instrument (%d %d -> %d %d)", p->source_bank, p->source_prog, bank, prog);
		} else if(tonebank[0]->tone[p->source_prog].name) {
			memcpy(&tonebank[bank]->tone[prog], &tonebank[0]->tone[p->source_prog], sizeof(ToneBankElement));
			dup_tone_bank_element(0, bank, prog);
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"User Instrument (%d %d -> %d %d)", 0, p->source_prog, bank, prog);
		}
	}
}

UserInstrument *get_userinst(int bank, int prog)
{
	UserInstrument *p;

	for(p = userinst_first; p != NULL; p = p->next) {
		if(p->bank == bank && p->prog == prog) {return p;}
	}

	p = (UserInstrument *)safe_malloc(sizeof(UserInstrument));
	memset(p, 0, sizeof(UserInstrument));
	p->next = NULL;
	if(userinst_first == NULL) {
		userinst_first = p;
		userinst_last = p;
	} else {
		userinst_last->next = p;
		userinst_last = p;
	}
	p->bank = bank;
	p->prog = prog;

	return p;
}

void free_userinst()
{
	UserInstrument *p, *next;

	for(p = userinst_first; p != NULL; p = next){
		next = p->next;
		free(p);
    }
	userinst_first = userinst_last = NULL;
}

void init_insertion_effect_status()
{
	int i;
	struct insertion_effect_t *st = &insertion_effect;

	for(i=0;i<20;i++) {st->parameter[i] = 0;}

	st->type = 0;
	st->type_lsb = 0;
	st->type_msb = 0;
	st->send_reverb = 0x28;
	st->send_chorus = 0;
	st->send_delay = 0;
	st->control_source1 = 0;
	st->control_depth1 = 0x40;
	st->control_source2 = 0;
	st->control_depth2 = 0x40;
	st->send_eq_switch = 0x01;
	st->eq_low_gain = 0;
	st->eq_high_gain = 0;
}

void set_insertion_effect_default_parameter()
{
	struct insertion_effect_t *st = &insertion_effect;

	switch(st->type) {
	case 0x0110: /* Overdrive */
		st->parameter[0] = 48;
		st->parameter[1] = 1;
		st->parameter[2] = 1;
		st->parameter[16] = 0x40;
		st->parameter[17] = 0x40;
		st->parameter[18] = 0x40;
		st->parameter[19] = 96;
		break;
	case 0x0111: /* Distortion */
		st->parameter[0] = 76;
		st->parameter[1] = 3;
		st->parameter[2] = 1;
		st->parameter[16] = 0x40;
		st->parameter[17] = 0x38;
		st->parameter[18] = 0x40;
		st->parameter[19] = 84;
		break;
	case 0x1103: /* OD1 / OD2 */
		st->parameter[0] = 0;
		st->parameter[1] = 48;
		st->parameter[2] = 1;
		st->parameter[3] = 1;
		st->parameter[15] = 0x40;
		st->parameter[16] = 96;
		st->parameter[5] = 1;
		st->parameter[6] = 76;
		st->parameter[7] = 3;
		st->parameter[8] = 1;
		st->parameter[17] = 0x40;
		st->parameter[18] = 84;
		st->parameter[19] = 127;
		break;
	default: break;
	}
}

void recompute_insertion_effect()
{
	int32 freq;
	FLOAT_T dbGain;
	struct insertion_effect_t *st = &insertion_effect;

	switch(st->type) {
	case 0x0110: /* Overdrive */
		st->eq_low_gain = st->parameter[16] - 0x40;
		st->eq_high_gain = st->parameter[17] - 0x40;
		break;
	case 0x0111: /* Distortion */
		st->eq_low_gain = st->parameter[16] - 0x40;
		st->eq_high_gain = st->parameter[17] - 0x40;
		break;
	default: break;
	}

	/* calculate EQ coefficients */
	if(st->eq_low_gain != 0 || st->eq_high_gain != 0) {
		freq = st->eq_low_freq = 400;
		dbGain = st->eq_low_gain;
		calc_lowshelf_coefs(st->eq_low_coef,freq,dbGain,play_mode->rate);
		freq = st->eq_high_freq = 4000;
		dbGain = st->eq_high_gain;
		calc_highshelf_coefs(st->eq_high_coef,freq,dbGain,play_mode->rate);
	}
}

void init_channel_layer(int ch)
{
	if(channel[ch].channel_layer != NULL) {
		free(channel[ch].channel_layer);
		channel[ch].channel_layer = NULL;
	}
	add_channel_layer(ch, ch);
}

void add_channel_layer(int ch, int fromch)
{
	int i, j = 0;
	int8 layer[MAX_CHANNELS];

	/* delete overlapping channel layer */
	if(channel[fromch].channel_layer != NULL) {
		memcpy(layer, channel[fromch].channel_layer, sizeof(channel[fromch].channel_layer));
		for(i = 0; i < MAX_CHANNELS; i++)
		{
			if(layer[i] == -1) {
				channel[fromch].channel_layer[j] = -1;
				break;
			} else if(layer[i] != fromch) {
				channel[fromch].channel_layer[j] = layer[i];
				j++;
			}
		}
	}
	/* add channel layer */
	for(i = 0; i < MAX_CHANNELS; i++)
	{
		if(channel[ch].channel_layer == NULL || channel[ch].channel_layer[i] == -1) {
			channel[ch].channel_layer = (int8 *)safe_realloc(channel[ch].channel_layer, sizeof(int8) * (i + 2));
			channel[ch].channel_layer[i] = fromch;
			channel[ch].channel_layer[i + 1] = -1;
			if(i > 0) {
				ctl->cmsg(CMSG_INFO, VERB_NOISY, "Channel Layer (CH:%d -> CH:%d)", fromch, ch);
			}
			break;
		} else if(channel[ch].channel_layer[i] == fromch) {
			break;
		}
	}
}

void remove_channel_layer(int ch)
{
	int i, j, k;
	int8 layer[MAX_CHANNELS];

	for(k = 0; k < MAX_CHANNELS; k++)
	{
		j = 0;
		/* remove channel layer */
		if(channel[k].channel_layer != NULL) {
			memcpy(layer, channel[k].channel_layer, sizeof(channel[k].channel_layer));
			for(i = 0; i < MAX_CHANNELS; i++)
			{
				if(layer[i] == -1) {
					channel[k].channel_layer[j] = -1;
					break;
				} else if(layer[i] != ch) {
					channel[k].channel_layer[j] = layer[i];
					j++;
				}
			}
		}
	}
}
