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

    playmidi.c -- random stuff in need of rearrangement

*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#ifdef __W32__
#include "interface.h"
#endif
#include <stdio.h>
#include <stdlib.h>

#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <math.h>
#ifdef __W32__
#include <windows.h>
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
#include "mix.h"
#include "controls.h"
#include "miditrace.h"
#include "recache.h"
#include "arc.h"
#include "reverb.h"
#include "wrd.h"
#include "aq.h"
#include "freq.h"
#include "quantity.h"

extern void convert_mod_to_midi_file(MidiEvent * ev);

#define ABORT_AT_FATAL 1 /*#################*/
#define MYCHECK(s) do { if(s == 0) { printf("## L %d\n", __LINE__); abort(); } } while(0)

extern VOLATILE int intr;

/* #define SUPPRESS_CHANNEL_LAYER */

#ifdef SOLARIS
/* shut gcc warning up */
int usleep(unsigned int useconds);
#endif

#ifdef SUPPORT_SOUNDSPEC
#include "soundspec.h"
#endif /* SUPPORT_SOUNDSPEC */

#include "tables.h"

#define PLAY_INTERLEAVE_SEC		1.0
#define PORTAMENTO_TIME_TUNING		(1.0 / 5000.0)
#define PORTAMENTO_CONTROL_RATIO	256	/* controls per sec */
#define DEFAULT_CHORUS_DELAY1		0.02
#define DEFAULT_CHORUS_DELAY2		0.003
#define CHORUS_OPPOSITE_THRESHOLD	32
#define CHORUS_VELOCITY_TUNING1		0.7
#define CHORUS_VELOCITY_TUNING2		0.6
#define EOT_PRESEARCH_LEN		32
#define SPEED_CHANGE_RATE		1.0594630943592953  /* 2^(1/12) */

/* Undefine if you don't want to use auto voice reduce implementation */
#define REDUCE_VOICE_TIME_TUNING	(play_mode->rate/5) /* 0.2 sec */
#ifdef REDUCE_VOICE_TIME_TUNING
static int max_good_nv = 1;
static int min_bad_nv = 256;
static int32 ok_nv_total = 32;
static int32 ok_nv_counts = 1;
static int32 ok_nv_sample = 0;
static int ok_nv = 32;
static int old_rate = -1;
#endif

static int midi_streaming=0;
int volatile stream_max_compute=500; //compute time limit (in msec) when streaming
static int prescanning_flag;
static int32 midi_restart_time = 0;
Channel channel[MAX_CHANNELS];
Voice voice[MAX_VOICES];
int8 current_keysig = 0;
int8 current_temper_keysig = 0;
int8 opt_init_keysig = 0;
int8 opt_force_keysig = 8;
int32 current_play_tempo = 500000;
int opt_realtime_playing = 0;
int reduce_voice_threshold = -1;
static MBlockList playmidi_pool;
int check_eot_flag;
int special_tonebank = -1;
int default_tonebank = 0;
int playmidi_seek_flag = 0;
int play_pause_flag = 0;
static int file_from_stdin;
int key_adjust = 0;
int opt_pure_intonation = 0;
int current_freq_table = 0;

static void set_reverb_level(int ch, int level);
static int make_rvid_flag = 0; /* For reverb optimization */

/* Ring voice id for each notes.  This ID enables duplicated note. */
static uint8 vidq_head[128 * MAX_CHANNELS], vidq_tail[128 * MAX_CHANNELS];

#ifdef MODULATION_WHEEL_ALLOW
int opt_modulation_wheel = 1;
#else
int opt_modulation_wheel = 0;
#endif /* MODULATION_WHEEL_ALLOW */

#ifdef PORTAMENTO_ALLOW
int opt_portamento = 1;
#else
int opt_portamento = 0;
#endif /* PORTAMENTO_ALLOW */

#ifdef NRPN_VIBRATO_ALLOW
int opt_nrpn_vibrato = 1;
#else
int opt_nrpn_vibrato = 0;
#endif /* NRPN_VIBRATO_ALLOW */

#ifdef REVERB_CONTROL_ALLOW
int opt_reverb_control = 1;
#else
#ifdef FREEVERB_CONTROL_ALLOW
int opt_reverb_control = 3;
#else
int opt_reverb_control = 0;
#endif /* FREEVERB_CONTROL_ALLOW */
#endif /* REVERB_CONTROL_ALLOW */

#ifdef CHORUS_CONTROL_ALLOW
int opt_chorus_control = 1;
#else
int opt_chorus_control = 0;
#endif /* CHORUS_CONTROL_ALLOW */

#ifdef SURROUND_CHORUS_ALLOW
int opt_surround_chorus = 1;
#else
int opt_surround_chorus = 0;
#endif /* SURROUND_CHORUS_ALLOW */

#ifdef GM_CHANNEL_PRESSURE_ALLOW
int opt_channel_pressure = 1;
#else
int opt_channel_pressure = 0;
#endif /* GM_CHANNEL_PRESSURE_ALLOW */

#ifdef VOICE_BY_VOICE_LPF_ALLOW
int opt_lpf_def = 1;
#else
int opt_lpf_def = 0;
#endif /* VOICE_BY_VOICE_LPF_ALLOW */

#ifdef OVERLAP_VOICE_ALLOW
int opt_overlap_voice_allow = 1;
#else
int opt_overlap_voice_allow = 0;
#endif /* OVERLAP_VOICE_ALLOW */

#ifdef TEMPER_CONTROL_ALLOW
int opt_temper_control = 1;
#else
int opt_temper_control = 0;
#endif /* TEMPER_CONTROL_ALLOW */

int opt_tva_attack = 0;	/* attack envelope control */
int opt_tva_decay = 0;	/* decay envelope control */
int opt_tva_release = 0;	/* release envelope control */
int opt_delay_control = 0;	/* CC#94 delay(celeste) effect control */
int opt_eq_control = 0;		/* channel equalizer control */
int opt_insertion_effect = 0;	/* insertion effect control */
int opt_drum_effect = 0;	/* drumpart effect control */
int32 opt_drum_power = 100;		/* coef. of drum amplitude */
int opt_amp_compensation = 0;
int opt_modulation_envelope = 0;

int voices=DEFAULT_VOICES, upper_voices;

int32
    control_ratio=0,
    amplification=DEFAULT_AMPLIFICATION;

static FLOAT_T
    master_volume;
static int32 master_volume_ratio = 0xFFFF;
ChannelBitMask default_drumchannel_mask;
ChannelBitMask default_drumchannels;
ChannelBitMask drumchannel_mask;
ChannelBitMask drumchannels;
int adjust_panning_immediately=1;
int auto_reduce_polyphony=1;
double envelope_modify_rate = 1.0;
#if defined(CSPLINE_INTERPOLATION) || defined(LAGRANGE_INTERPOLATION) || defined(NEWTON_INTERPOLATION) || defined(GAUSS_INTERPOLATION)
int reduce_quality_flag=0;
int no_4point_interpolation=0;
#endif
char* pcm_alternate_file = NULL; /* NULL or "none": Nothing (default)
				  * "auto": Auto select
				  * filename: Use it
				  */

static int32 lost_notes, cut_notes;
static int32 common_buffer[AUDIO_BUFFER_SIZE*2], /* stereo samples */
             *buffer_pointer;
static int16 wav_buffer[AUDIO_BUFFER_SIZE*2];
static int32 buffered_count;
static char *reverb_buffer = NULL; /* MAX_CHANNELS*AUDIO_BUFFER_SIZE*8 */

#ifdef USE_DSP_EFFECT
static int32 insertion_effect_buffer[AUDIO_BUFFER_SIZE*2];
#endif /* USE_DSP_EFFECT */

static MidiEvent *event_list;
static MidiEvent *current_event;
static int32 sample_count;	/* Length of event_list */
int32 current_sample;		/* Number of calclated samples */

int note_key_offset = 0;	/* For key up/down */
FLOAT_T midi_time_ratio = 1.0;	/* For speed up/down */
ChannelBitMask channel_mute;	/* For channel mute */
int temper_type_mute;		/* For temperament type mute */

/* for auto amplitude compensation */
static int mainvolume_max; /* maximum value of mainvolume */
static double compensation_ratio = 1.0; /* compensation ratio */

static void update_portamento_controls(int ch);
static void update_rpn_map(int ch, int addr, int update_now);
static void ctl_prog_event(int ch);
static void ctl_timestamp(void);
static void ctl_updatetime(int32 samples);
static void ctl_pause_event(int pause, int32 samples);
static void update_legato_controls(int ch);
static void update_channel_freq(int ch);
static void set_single_note_tuning(int, int, int, int);

#define IS_SYSEX_EVENT_TYPE(type) ((type) == ME_NONE || (type) >= ME_RANDOM_PAN)

static char *event_name(int type)
{
#define EVENT_NAME(X) case X: return #X
    switch(type)
    {
	EVENT_NAME(ME_NONE);
	EVENT_NAME(ME_NOTEOFF);
	EVENT_NAME(ME_NOTEON);
	EVENT_NAME(ME_KEYPRESSURE);
	EVENT_NAME(ME_PROGRAM);
	EVENT_NAME(ME_CHANNEL_PRESSURE);
	EVENT_NAME(ME_PITCHWHEEL);
	EVENT_NAME(ME_TONE_BANK_MSB);
	EVENT_NAME(ME_TONE_BANK_LSB);
	EVENT_NAME(ME_MODULATION_WHEEL);
	EVENT_NAME(ME_BREATH);
	EVENT_NAME(ME_FOOT);
	EVENT_NAME(ME_MAINVOLUME);
	EVENT_NAME(ME_BALANCE);
	EVENT_NAME(ME_PAN);
	EVENT_NAME(ME_EXPRESSION);
	EVENT_NAME(ME_SUSTAIN);
	EVENT_NAME(ME_PORTAMENTO_TIME_MSB);
	EVENT_NAME(ME_PORTAMENTO_TIME_LSB);
	EVENT_NAME(ME_PORTAMENTO);
	EVENT_NAME(ME_PORTAMENTO_CONTROL);
	EVENT_NAME(ME_DATA_ENTRY_MSB);
	EVENT_NAME(ME_DATA_ENTRY_LSB);
	EVENT_NAME(ME_SOSTENUTO);
	EVENT_NAME(ME_SOFT_PEDAL);
	EVENT_NAME(ME_LEGATO_FOOTSWITCH);
	EVENT_NAME(ME_HOLD2);
	EVENT_NAME(ME_HARMONIC_CONTENT);
	EVENT_NAME(ME_RELEASE_TIME);
	EVENT_NAME(ME_ATTACK_TIME);
	EVENT_NAME(ME_BRIGHTNESS);
	EVENT_NAME(ME_REVERB_EFFECT);
	EVENT_NAME(ME_TREMOLO_EFFECT);
	EVENT_NAME(ME_CHORUS_EFFECT);
	EVENT_NAME(ME_CELESTE_EFFECT);
	EVENT_NAME(ME_PHASER_EFFECT);
	EVENT_NAME(ME_RPN_INC);
	EVENT_NAME(ME_RPN_DEC);
	EVENT_NAME(ME_NRPN_LSB);
	EVENT_NAME(ME_NRPN_MSB);
	EVENT_NAME(ME_RPN_LSB);
	EVENT_NAME(ME_RPN_MSB);
	EVENT_NAME(ME_ALL_SOUNDS_OFF);
	EVENT_NAME(ME_RESET_CONTROLLERS);
	EVENT_NAME(ME_ALL_NOTES_OFF);
	EVENT_NAME(ME_MONO);
	EVENT_NAME(ME_POLY);
#if 0
	EVENT_NAME(ME_VOLUME_ONOFF);		/* Not supported */
#endif
	EVENT_NAME(ME_RANDOM_PAN);
	EVENT_NAME(ME_SET_PATCH);
	EVENT_NAME(ME_DRUMPART);
	EVENT_NAME(ME_KEYSHIFT);
	EVENT_NAME(ME_PATCH_OFFS);

	EVENT_NAME(ME_TEMPO);
	EVENT_NAME(ME_CHORUS_TEXT);
	EVENT_NAME(ME_LYRIC);
	EVENT_NAME(ME_GSLCD);
	EVENT_NAME(ME_MARKER);
	EVENT_NAME(ME_INSERT_TEXT);
	EVENT_NAME(ME_TEXT);
	EVENT_NAME(ME_KARAOKE_LYRIC);
	EVENT_NAME(ME_MASTER_VOLUME);
	EVENT_NAME(ME_RESET);
	EVENT_NAME(ME_NOTE_STEP);
	EVENT_NAME(ME_TIMESIG);
	EVENT_NAME(ME_KEYSIG);
	EVENT_NAME(ME_SCALE_TUNING);
	EVENT_NAME(ME_BULK_TUNING_DUMP);
	EVENT_NAME(ME_SINGLE_NOTE_TUNING);
	EVENT_NAME(ME_TEMPER_KEYSIG);
	EVENT_NAME(ME_TEMPER_TYPE);
	EVENT_NAME(ME_MASTER_TEMPER_TYPE);
	EVENT_NAME(ME_WRD);
	EVENT_NAME(ME_SHERRY);
	EVENT_NAME(ME_BARMARKER);
	EVENT_NAME(ME_STEP);
	EVENT_NAME(ME_LAST);
	EVENT_NAME(ME_EOT);
    }
    return "Unknown";
#undef EVENT_NAME
}

static void adjust_amplification(void)
{
    /* compensate master volume */
    master_volume = (double)(amplification) / 100.0 *
	((double)master_volume_ratio * (compensation_ratio/0xFFFF));
}

static int new_vidq(int ch, int note)
{
    int i;

    if(opt_overlap_voice_allow)
    {
	i = ch * 128 + note;
	return vidq_head[i]++;
    }
    return 0;
}

static int last_vidq(int ch, int note)
{
    int i;

    if(opt_overlap_voice_allow)
    {
	i = ch * 128 + note;
	if(vidq_head[i] == vidq_tail[i])
	{
	    ctl->cmsg(CMSG_WARNING, VERB_DEBUG_SILLY,
		      "channel=%d, note=%d: Voice is already OFF", ch, note);
	    return -1;
	}
	return vidq_tail[i]++;
    }
    return 0;
}

static void reset_voices(void)
{
    int i;
    for(i = 0; i < MAX_VOICES; i++)
    {
	voice[i].status = VOICE_FREE;
	voice[i].temper_instant = 0;
	voice[i].chorus_link = i;
    }
    upper_voices = 0;
    memset(vidq_head, 0, sizeof(vidq_head));
    memset(vidq_tail, 0, sizeof(vidq_tail));
}

static void kill_note(int i)
{
    voice[i].status = VOICE_DIE;
    if(!prescanning_flag)
	ctl_note_event(i);
}

static void kill_all_voices(void)
{
    int i, uv = upper_voices;

    for(i = 0; i < uv; i++)
	if(voice[i].status & ~(VOICE_FREE | VOICE_DIE))
	    kill_note(i);
    memset(vidq_head, 0, sizeof(vidq_head));
    memset(vidq_tail, 0, sizeof(vidq_tail));
}

static void reset_drum_controllers(struct DrumParts *d[], int note)
{
    int i,j;

    if(note == -1)
    {
	for(i = 0; i < 128; i++)
	    if(d[i] != NULL)
	    {
		d[i]->drum_panning = NO_PANNING;
		for(j=0;j<6;j++) {d[i]->drum_envelope_rate[j] = -1;}
		d[i]->pan_random = 0;
		d[i]->drum_level = 1.0f;
		d[i]->coarse = 0;
		d[i]->fine = 0;
		d[i]->delay_level = 0;
		d[i]->chorus_level = 0;
		d[i]->reverb_level = 0;
		d[i]->play_note = -1;
		d[i]->rx_note_off = 0;
		d[i]->drum_cutoff_freq = 0;
		d[i]->drum_resonance = 0;
	    }
    }
    else
    {
	d[note]->drum_panning = NO_PANNING;
	for(j=0;j<6;j++) {d[note]->drum_envelope_rate[j] = -1;}
	d[note]->pan_random = 0;
	d[note]->drum_level = 1.0f;
	d[note]->coarse = 0;
	d[note]->fine = 0;
	d[note]->delay_level = 0;
	d[note]->chorus_level = 0;
	d[note]->reverb_level = 0;
	d[note]->play_note = -1;
	d[note]->rx_note_off = 0;
	d[note]->drum_cutoff_freq = 0;
	d[note]->drum_resonance = 0;
    }
}

static void reset_nrpn_controllers(int c)
{
  int i,j;

  /* NRPN */
  reset_drum_controllers(channel[c].drums, -1);
  channel[c].vibrato_ratio = 0;
  channel[c].vibrato_depth = 0;
  channel[c].vibrato_delay = 0;
  channel[c].param_cutoff_freq = 0;
  channel[c].param_resonance = 0;
  channel[c].cutoff_freq_coef = 1.0;
  channel[c].resonance_dB = 0;

  /* GS & XG System Exclusive */
  channel[c].eq_on = 1;
  channel[c].insertion_effect = 0;
  channel[c].velocity_sense_depth = 0x40;
  channel[c].velocity_sense_offset = 0x40;
  channel[c].pitch_offset_fine = 0;
  channel[c].legato = 0;
  channel[c].assign_mode = 1;
  if(play_system_mode == GS_SYSTEM_MODE) {
	  channel[c].bank_lsb = channel[c].tone_map0_number;
  }
	for (i = 0; i < 12; i++)
		channel[c].scale_tuning[i] = 0;
	channel[c].prev_scale_tuning = 0;
	channel[c].temper_type = 0;

  init_channel_layer(c);

  /* channel pressure & polyphonic key pressure control */
  channel[c].caf_lfo1_rate_ctl = 0;

  channel[c].sysex_gs_msb_addr = channel[c].sysex_gs_msb_val =
	channel[c].sysex_xg_msb_addr = channel[c].sysex_xg_msb_val =
	channel[c].sysex_msb_addr = channel[c].sysex_msb_val = 0;
}

/* Process the Reset All Controllers event */
static void reset_controllers(int c)
{
  int i,j;
    /* Some standard says, although the SCC docs say 0. */
    
  if(play_system_mode == XG_SYSTEM_MODE)
      channel[c].volume = 100;
  else
      channel[c].volume = 90;
  if (prescanning_flag) {
    if (channel[c].volume > mainvolume_max) {	/* pick maximum value of mainvolume */
      mainvolume_max = channel[c].volume;
      ctl->cmsg(CMSG_INFO,VERB_DEBUG,"ME_MAINVOLUME/max (CH:%d VAL:%#x)",c,mainvolume_max);
    }
  }

  channel[c].expression=127; /* SCC-1 does this. */
  channel[c].sustain=0;
  channel[c].pitchbend=0x2000;
  channel[c].pitchfactor=0; /* to be computed */
  channel[c].modulation_wheel = 0;
  channel[c].portamento_time_lsb = 0;
  channel[c].portamento_time_msb = 0;
  channel[c].porta_control_ratio = 0;
  channel[c].portamento = 0;
  channel[c].last_note_fine = -1;
  for(j=0;j<6;j++) {channel[c].envelope_rate[j] = -1;}
  update_portamento_controls(c);
  set_reverb_level(c, -1);
  if(opt_chorus_control == 1)
      channel[c].chorus_level = 0;
  else
      channel[c].chorus_level = -opt_chorus_control;
  channel[c].mono = 0;
  channel[c].delay_level = 0;
}

static void redraw_controllers(int c)
{
    ctl_mode_event(CTLE_VOLUME, 1, c, channel[c].volume);
    ctl_mode_event(CTLE_EXPRESSION, 1, c, channel[c].expression);
    ctl_mode_event(CTLE_SUSTAIN, 1, c, channel[c].sustain);
    ctl_mode_event(CTLE_MOD_WHEEL, 1, c, channel[c].modulation_wheel);
    ctl_mode_event(CTLE_PITCH_BEND, 1, c, channel[c].pitchbend);
    ctl_prog_event(c);
    ctl_mode_event(CTLE_TEMPER_TYPE, 1, c, channel[c].temper_type);
    ctl_mode_event(CTLE_MUTE, 1,
    		c, (IS_SET_CHANNELMASK(channel_mute, c)) ? 1 : 0);
    ctl_mode_event(CTLE_CHORUS_EFFECT, 1, c, get_chorus_level(c));
    ctl_mode_event(CTLE_REVERB_EFFECT, 1, c, get_reverb_level(c));
}

static void reset_midi(int playing)
{
	int i, cnt;
	
	for (i = 0; i < MAX_CHANNELS; i++) {
		reset_controllers(i);
		reset_nrpn_controllers(i);
		/* The rest of these are unaffected
		 * by the Reset All Controllers event
		 */
		channel[i].program = default_program[i];
		channel[i].panning = NO_PANNING;
		channel[i].pan_random = 0;
		/* tone bank or drum set */
		if (ISDRUMCHANNEL(i)) {
			channel[i].bank = 0;
			channel[i].altassign = drumset[0]->alt;
		} else {
			if (special_tonebank >= 0)
				channel[i].bank = special_tonebank;
			else
				channel[i].bank = default_tonebank;
		}
		channel[i].bank_lsb = channel[i].bank_msb =
				channel[i].tone_map0_number = 0;
		if (play_system_mode == XG_SYSTEM_MODE && i % 16 == 9)
			channel[i].bank_msb = 127;	/* Use MSB=127 for XG */
		update_rpn_map(i, RPN_ADDR_FFFF, 0);
		channel[i].special_sample = 0;
		channel[i].key_shift = 0;
		channel[i].mapID = get_default_mapID(i);
		channel[i].lasttime = 0;
	}
	if (playing) {
		kill_all_voices();
		if (temper_type_mute) {
			if (temper_type_mute & 1)
				FILL_CHANNELMASK(channel_mute);
			else
				CLEAR_CHANNELMASK(channel_mute);
		}
		for (i = 0; i < MAX_CHANNELS; i++)
			redraw_controllers(i);
		if (midi_streaming && free_instruments_afterwards) {
			free_instruments(0);
			/* free unused memory */
			cnt = free_global_mblock();
			if (cnt > 0)
				ctl->cmsg(CMSG_INFO, VERB_VERBOSE,
						"%d memory blocks are free", cnt);
		}
	} else
		reset_voices();
	master_volume_ratio = 0xffff;
	adjust_amplification();
	init_freq_table_tuning();
	if (current_file_info) {
		COPY_CHANNELMASK(drumchannels, current_file_info->drumchannels);
		COPY_CHANNELMASK(drumchannel_mask,
				current_file_info->drumchannel_mask);
	} else {
		COPY_CHANNELMASK(drumchannels, default_drumchannels);
		COPY_CHANNELMASK(drumchannel_mask, default_drumchannel_mask);
	}
	ctl_mode_event(CTLE_MASTER_VOLUME, 0, amplification, 0);
	ctl_mode_event(CTLE_KEY_OFFSET, 0, note_key_offset, 0);
	ctl_mode_event(CTLE_TIME_RATIO, 0, 100 / midi_time_ratio + 0.5, 0);
}

void recompute_freq(int v)
{
	int i;
	int ch = voice[v].channel;
	int note = voice[v].note;
	int32 tuning = 0;
	int8 st = channel[ch].scale_tuning[note % 12];
	int8 tt = channel[ch].temper_type;
	uint8 tp = channel[ch].rpnmap[RPN_ADDR_0003];
	int32 f;
	int pb = channel[ch].pitchbend;
	int32 tmp;
	FLOAT_T pf, root_freq;
	int32 a;
	
	if (! voice[v].sample->sample_rate)
		return;
	if (! opt_modulation_wheel)
		voice[v].modulation_wheel = 0;
	if (! opt_portamento)
		voice[v].porta_control_ratio = 0;
	voice[v].vibrato_control_ratio = voice[v].orig_vibrato_control_ratio;
	if (voice[v].vibrato_control_ratio || voice[v].modulation_wheel > 0) {
		/* This instrument has vibrato. Invalidate any precomputed
		 * sample_increments.
		 */
		if (voice[v].modulation_wheel > 0) {
			voice[v].vibrato_control_ratio = play_mode->rate / 2.0
					* MODULATION_WHEEL_RATE / VIBRATO_SAMPLE_INCREMENTS;
			voice[v].vibrato_delay = 0;
		}
		for (i = 0; i < VIBRATO_SAMPLE_INCREMENTS; i++)
			voice[v].vibrato_sample_increment[i] = 0;
		voice[v].cache = NULL;
	}
	/* fine: [0..128] => [-256..256]
	 * 1 coarse = 256 fine (= 1 note)
	 * 1 fine = 2^5 tuning
	 */
	tuning = (channel[ch].rpnmap[RPN_ADDR_0001] - 0x40
			+ (channel[ch].rpnmap[RPN_ADDR_0002] - 0x40) * 64) << 7;
	/* for NRPN Coarse Pitch of Drum (GS) & Fine Pitch of Drum (XG) */
	if (ISDRUMCHANNEL(ch) && channel[ch].drums[note] != NULL
			&& (channel[ch].drums[note]->fine
			|| channel[ch].drums[note]->coarse)) {
		tuning += (channel[ch].drums[note]->fine
				+ channel[ch].drums[note]->coarse * 64) << 7;
	}
	if (opt_modulation_envelope) {
		if (voice[v].sample->tremolo_to_pitch)
			tuning += lookup_triangular(voice[v].tremolo_phase >> RATE_SHIFT)
					* (voice[v].sample->tremolo_to_pitch << 13) / 100.0 + 0.5;
		if (voice[v].sample->modenv_to_pitch)
			tuning += voice[v].last_modenv_volume
					* (voice[v].sample->modenv_to_pitch << 13) / 100.0 + 0.5;
	}
	/* GS/XG - Scale Tuning */
	if (! ISDRUMCHANNEL(ch)) {
		tuning += (st << 13) / 100.0 + 0.5;
		if (st != channel[ch].prev_scale_tuning) {
			channel[ch].pitchfactor = 0;
			channel[ch].prev_scale_tuning = st;
		}
	}
	if (! opt_pure_intonation
			&& opt_temper_control && voice[v].temper_instant) {
		switch (tt) {
		case 0:
			f = freq_table_tuning[tp][note];
			break;
		case 1:
			f = freq_table_pytha[current_freq_table][note];
			break;
		case 2:
			if (current_temper_keysig < 8)
				f = freq_table_meantone[current_freq_table][note];
			else
				f = freq_table_meantone[current_freq_table + 12][note];
			break;
		case 3:
			if (current_temper_keysig < 8)
				f = freq_table_pureint[current_freq_table][note];
			else
				f = freq_table_pureint[current_freq_table + 12][note];
			break;
		default:	/* user-defined temperaments */
			if ((tt -= 0x40) >= 0 && tt < 4) {
				if (current_temper_keysig < 8)
					f = freq_table_user[tt][current_freq_table][note];
				else
					f = freq_table_user[tt][current_freq_table + 12][note];
			} else
				f = freq_table[note];
			break;
		}
		voice[v].orig_frequency = f;
	}
	if (! voice[v].porta_control_ratio) {
		if (tuning == 0 && pb == 0x2000)
			voice[v].frequency = voice[v].orig_frequency;
		else {
			pb -= 0x2000;
			if (! channel[ch].pitchfactor) {
				/* Damn.  Somebody bent the pitch. */
				tmp = pb * channel[ch].rpnmap[RPN_ADDR_0000] + tuning;
				if (tmp >= 0)
					channel[ch].pitchfactor = bend_fine[tmp >> 5 & 0xff]
							* bend_coarse[tmp >> 13 & 0x7f];
				else
					channel[ch].pitchfactor = 1.0 /
							(bend_fine[-tmp >> 5 & 0xff]
							* bend_coarse[-tmp >> 13 & 0x7f]);
			}
			voice[v].frequency =
					voice[v].orig_frequency * channel[ch].pitchfactor;
			if (voice[v].frequency != voice[v].orig_frequency)
				voice[v].cache = NULL;
		}
	} else {	/* Portamento */
		pb -= 0x2000;
		tmp = pb * channel[ch].rpnmap[RPN_ADDR_0000]
				+ (voice[v].porta_pb << 5) + tuning;
		if (tmp >= 0)
			pf = bend_fine[tmp >> 5 & 0xff]
					* bend_coarse[tmp >> 13 & 0x7f];
		else
			pf = 1.0 / (bend_fine[-tmp >> 5 & 0xff]
					* bend_coarse[-tmp >> 13 & 0x7f]);
		voice[v].frequency = voice[v].orig_frequency * pf;
		voice[v].cache = NULL;
	}
	if (ISDRUMCHANNEL(ch) && channel[ch].drums[note] != NULL
			&& channel[ch].drums[note]->play_note != -1)
		root_freq = voice[v].sample->root_freq
				* (double) freq_table[channel[ch].drums[note]->play_note]
				/ voice[v].orig_frequency;
	else
		root_freq = voice[v].sample->root_freq;
	a = TIM_FSCALE(((double) voice[v].sample->sample_rate
			* voice[v].frequency + channel[ch].pitch_offset_fine)
			/ (root_freq * play_mode->rate), FRACTION_BITS) + 0.5;
	/* need to preserve the loop direction */
	voice[v].sample_increment = (voice[v].sample_increment >= 0) ? a : -a;
#ifdef ABORT_AT_FATAL
	if (voice[v].sample_increment == 0) {
		fprintf(stderr, "Invalid sample increment a=%e %ld %ld %ld %ld%s\n",
				(double)a, (long) voice[v].sample->sample_rate,
				(long) voice[v].frequency, (long) voice[v].sample->root_freq,
				(long) play_mode->rate, (voice[v].cache) ? " (Cached)" : "");
		abort();
	}
#endif	/* ABORT_AT_FATAL */
}

static int32 calc_velocity(int32 ch,int32 vel)
{
	int32 velocity;
	velocity = channel[ch].velocity_sense_depth * vel / 64 + (channel[ch].velocity_sense_offset - 64) * 2;
	if(velocity > 127) {velocity = 127;}
	return velocity;
}

static void recompute_amp(int v)
{
	FLOAT_T tempamp;

	/* master_volume and sample->volume are percentages, used to scale
	 *  amplitude directly, NOT perceived volume
	 *
	 * all other MIDI volumes are linear in perceived volume, 0-127
	 * use a lookup table for the non-linear scalings
	 */
	if(play_system_mode == GS_SYSTEM_MODE) {	/* use measured curve */ 
	tempamp = master_volume *
		   voice[v].sample->volume *
		   sc_vel_table[calc_velocity(voice[v].channel,voice[v].velocity)] *
		   sc_vol_table[channel[voice[v].channel].volume] *
		   sc_vol_table[channel[voice[v].channel].expression]; /* 21 bits */
	} else {	/* use generic exponential curve */
	tempamp = master_volume *
		  voice[v].sample->volume *
		  perceived_vol_table[calc_velocity(voice[v].channel,voice[v].velocity)] *
		  perceived_vol_table[channel[voice[v].channel].volume] *
		  perceived_vol_table[channel[voice[v].channel].expression]; /* 21 bits */
	}

	/* every digital effect increases amplitude, so that it must be reduced in advance. */
	if((opt_reverb_control || opt_chorus_control
			|| opt_delay_control || (opt_eq_control && (eq_status.low_gain != 0x40 || eq_status.high_gain != 0x40)) || opt_insertion_effect)
			&& !(play_mode->encoding & PE_MONO)) {
		tempamp *= 0.55f * 1.35f;
	} else {
		tempamp *= 1.35f;
	}

	/* NRPN - drum instrument tva level */
	if(ISDRUMCHANNEL(voice[v].channel)) {
		if(channel[voice[v].channel].drums[voice[v].note] != NULL) {
			tempamp *= channel[voice[v].channel].drums[voice[v].note]->drum_level;
		}
		tempamp *= (double)opt_drum_power * 0.01f;
	}

	if(!(play_mode->encoding & PE_MONO))
    	{
		if(voice[v].panning == 64)
		{
			voice[v].panned = PANNED_CENTER;
			voice[v].left_amp = voice[v].right_amp = TIM_FSCALENEG(tempamp * sc_pan_table[64], 27);
		}
		else if (voice[v].panning < 2)
		{
			voice[v].panned = PANNED_LEFT;
			voice[v].left_amp = TIM_FSCALENEG(tempamp, 20);
			voice[v].right_amp = 0;
		}
		else if(voice[v].panning == 127)
		{
#ifdef SMOOTH_MIXING
			if(voice[v].panned == PANNED_MYSTERY) {
				voice[v].old_left_mix = voice[v].old_right_mix;
				voice[v].old_right_mix = 0;
			}
#endif
			voice[v].panned = PANNED_RIGHT;
			voice[v].left_amp =  TIM_FSCALENEG(tempamp, 20);
			voice[v].right_amp = 0;
		}
		else
		{
#ifdef SMOOTH_MIXING
			if(voice[v].panned == PANNED_RIGHT) {
				voice[v].old_right_mix = voice[v].old_left_mix;
				voice[v].old_left_mix = 0;
			}
#endif
			voice[v].panned = PANNED_MYSTERY;
			voice[v].left_amp = TIM_FSCALENEG(tempamp * sc_pan_table[127 - voice[v].panning], 27);
			voice[v].right_amp = TIM_FSCALENEG(tempamp * sc_pan_table[voice[v].panning], 27);
		}
    	}
    	else
    	{
		voice[v].panned = PANNED_CENTER;
		voice[v].left_amp = TIM_FSCALENEG(tempamp, 21);
    	}
}

void recompute_channel_filter(MidiEvent *e)
{
	int ch = e->channel, note, prog, bk;
	double coef = 1.0f, reso = 0;
	ToneBankElement *elm;

	if(channel[ch].special_sample > 0) {return;}

	note = MIDI_EVENT_NOTE(e);
	prog = channel[ch].program;
	bk = channel[ch].bank;
	elm = &tonebank[bk]->tone[prog];

	/* Soft Pedal */
	if(channel[ch].soft_pedal > 63) {
		if(note > 49) {	/* tre corde */
			coef *= 1.0 - 0.20 * ((double)channel[ch].soft_pedal - 64) / 63.0f;
		} else {	/* una corda (due corde) */
			coef *= 1.0 - 0.25 * ((double)channel[ch].soft_pedal - 64) / 63.0f;
		}
	}

	if(!ISDRUMCHANNEL(ch)) {
		/* NRPN Filter Cutoff */
		coef *= pow(1.26, (double)(channel[ch].param_cutoff_freq) / 8.0f);
		/* NRPN Resonance */
		reso = (double)channel[ch].param_resonance * 0.5f;
	}

	channel[ch].cutoff_freq_coef = coef;
	channel[ch].resonance_dB = reso;

/*	ctl->cmsg(CMSG_INFO,VERB_NOISY,"Cutoff Frequency (CH:%d VAL:%f)",ch,coef);
	ctl->cmsg(CMSG_INFO,VERB_NOISY,"Resonance (CH:%d VAL:%f)",ch,reso);*/
}

void recompute_voice_filter(int v)
{
	int ch = voice[v].channel, note = voice[v].note;
	double coef = 1.0, reso = 0, cent = 0;
	int32 freq;
	FilterCoefficients *fc = &(voice[v].fc);
	Sample *sp = (Sample *) &voice[v].sample;

	if(fc->freq == -1) {return;}

	coef = channel[ch].cutoff_freq_coef;

	if(ISDRUMCHANNEL(ch) && channel[ch].drums[note] != NULL) {
		/* NRPN Drum Instrument Filter Cutoff */
		coef *= pow(1.26, (double)(channel[ch].drums[note]->drum_cutoff_freq) / 8.0f);
		/* NRPN Drum Instrument Filter Resonance */
		reso += (double)channel[ch].drums[note]->drum_resonance * 0.5f;
	}

	if(sp->vel_to_fc) {	/* velocity to filter cutoff frequency */
		if(voice[v].velocity > sp->vel_to_fc_threshold)
			cent += sp->vel_to_fc * (double)(127 - voice[v].velocity) / 127.0f;
		else
			coef += -1200;
	}
	if(sp->vel_to_resonance) {	/* velocity to filter resonance */
		reso += (double)voice[v].velocity * sp->vel_to_resonance / 127.0f / 10.0f;
	}
	if(sp->key_to_fc) {	/* filter cutoff key-follow */
		cent += sp->vel_to_fc * (double)(voice[v].note - sp->key_to_fc_bpo);
	}

	if(opt_modulation_envelope) {
		if(voice[v].sample->tremolo_to_fc) {
			cent += (double)voice[v].sample->tremolo_to_fc * lookup_triangular(voice[v].tremolo_phase >> RATE_SHIFT);
		}
		if(voice[v].sample->modenv_to_fc) {
			cent += (double)voice[v].sample->modenv_to_fc * voice[v].last_modenv_volume;
		}
	}

	if(cent != 0) {coef *= pow(2.0, cent / 1200.0f);}

	freq = (double)fc->orig_freq * coef;

	if (fc->filter_calculated == 0 && freq > play_mode->rate / 2) {
		fc->freq = -1;
		return;
	}
	else if(freq < 5) {freq = 5;}
	else if(freq > 20000) {freq = 20000;}
	fc->freq = freq;

	fc->reso_dB = fc->orig_reso_dB + channel[ch].resonance_dB + reso;
	if(fc->reso_dB < 0.0f) {fc->reso_dB = 0.0f;}
	else if(fc->reso_dB > 96.0f) {fc->reso_dB = 96.0f;}
	fc->reso_dB -= 3.01f;
}

FLOAT_T calc_drum_tva_level(int ch,int note,int level)
{
	int def_level,nbank,nprog;
	ToneBank *bank;

	if(channel[ch].special_sample > 0) {return 1.0;}

	nbank = channel[ch].bank;
	nprog = note;
	instrument_map(channel[ch].mapID, &nbank, &nprog);

	if(ISDRUMCHANNEL(ch)) {
		bank = drumset[nbank];
		if(bank == NULL) {bank = drumset[0];}
	} else {
		return 1.0;
	}

	def_level = bank->tone[nprog].tva_level;

	if(def_level == -1 || def_level == 0) {def_level = 127;}
	else if(def_level > 127) {def_level = 127;}

	return (sc_drum_level_table[level] / sc_drum_level_table[def_level]);
}

void recompute_bank_parameter(int ch,int note)
{
	int prog;
	ToneBank *bank;

	if(channel[ch].special_sample > 0) {return;}

	prog = channel[ch].program;

	if(ISDRUMCHANNEL(ch)) {
		return;
	} else {
		if(prog == SPECIAL_PROGRAM) {return;}
		bank = tonebank[(int)channel[ch].bank];
		if(bank == NULL) {bank = tonebank[0];}
	}

	channel[ch].legato = bank->tone[prog].legato;
}

static void *memdup(void *s,size_t size)
{
	void *p;

	p = safe_malloc(size);
	if(p != NULL) {memcpy(p,s,size);}

	return p;
}

void dup_tone_bank_element(int dr, int bk, int prog)
{
	ToneBank **bank = ((dr) ? drumset : tonebank);
	ToneBankElement *elm = &bank[bk]->tone[prog];
	int i;
	
	if (elm->name)
		elm->name = safe_strdup(elm->name);
	if (elm->comment)
		elm->comment = safe_strdup(elm->comment);
	if (elm->tunenum)
		elm->tune = (float *) memdup(
				elm->tune, elm->tunenum * sizeof(float));
	if (elm->envratenum) {
		elm->envrate = (int **) memdup(
				elm->envrate, elm->envratenum * sizeof(int *));
		for (i = 0; i < elm->envratenum; i++)
			elm->envrate[i] = (int *) memdup(elm->envrate[i], 6 * sizeof(int));
	}
	if (elm->envofsnum) {
		elm->envofs = (int **) memdup(
				elm->envofs, elm->envofsnum * sizeof(int *));
		for (i = 0; i < elm->envofsnum; i++)
			elm->envofs[i] = (int *) memdup(elm->envofs[i], 6 * sizeof(int));
	}
	if (elm->tremnum) {
		elm->trem = (Quantity **) memdup(elm->trem, elm->tremnum * sizeof(Quantity *));
		for (i = 0; i < elm->tremnum; i++)
			elm->trem[i] = (Quantity *) memdup(elm->trem[i], 3 * sizeof(Quantity));
	}
	if (elm->vibnum) {
		elm->vib = (Quantity **) memdup(elm->vib, elm->vibnum * sizeof(Quantity *));
		for (i = 0; i < elm->vibnum; i++)
			elm->vib[i] = (Quantity *) memdup(elm->vib[i], 3 * sizeof(Quantity));
	}
}

void free_tone_bank_element(int dr, int bk, int prog)
{
	ToneBank **bank = (dr) ? drumset : tonebank;
	ToneBankElement *elm = &bank[bk]->tone[prog];
	int i;
	
	if (elm->name) {
		free(elm->name);
		elm->name = NULL;
	}
	if (elm->comment) {
		free(elm->comment);
		elm->comment = NULL;
	}
	if (elm->tune) {
		free(elm->tune);
		elm->tune = NULL;
	}
	if (elm->envratenum) {
		free_ptr_list(elm->envrate, elm->envratenum);
		elm->envrate = NULL;
		elm->envratenum = 0;
	}
	if (elm->envofsnum) {
		free_ptr_list(elm->envofs, elm->envofsnum);
		elm->envofs = NULL;
		elm->envofsnum = 0;
	}
	if (elm->tremnum) {
		free_ptr_list(elm->trem, elm->tremnum);
		elm->trem = NULL;
		elm->tremnum = 0;
	}
	if (elm->vibnum) {
		free_ptr_list(elm->vib, elm->vibnum);
		elm->vib = NULL;
		elm->vibnum = 0;
	}
	elm->instype = 0;
}

Instrument *play_midi_load_instrument(int dr, int bk, int prog)
{
    ToneBank **bank = ((dr) ? drumset : tonebank);
	ToneBankElement *elm;
    Instrument *ip;
    int load_success;

    if(bank[bk] == NULL)
	bk = 0;

    load_success = 0;
    if(! opt_realtime_playing)
    {
	ip = bank[bk]->tone[prog].instrument;
#ifndef SUPPRESS_CHANNEL_LAYER
	if(ip == MAGIC_LOAD_INSTRUMENT || ip == NULL)
#else
	if(ip == MAGIC_LOAD_INSTRUMENT)
#endif
	{
	    ip = bank[bk]->tone[prog].instrument =
		load_instrument(dr, bk, prog);
	    if(ip != NULL)
		load_success = 1;
	}
	if(ip == NULL && bk != 0)
	{
	    /* Instrument is not found.
	       Retry to load the instrument from bank 0 */
	    if((ip = bank[0]->tone[prog].instrument) == NULL)
		ip = bank[0]->tone[prog].instrument =
		    load_instrument(dr, 0, prog);
	    if(ip != NULL)
	    {
			/* duplicate tone bank parameter */
			elm = &bank[bk]->tone[prog];
			memcpy(elm, &bank[0]->tone[prog], sizeof(ToneBankElement));
			elm->instrument = ip;
			dup_tone_bank_element(dr, bk, prog);
			load_success = 1;
	    }
	}
    }
    else
    {
	if((ip = bank[bk]->tone[prog].instrument) == NULL)
	{
	    ip = bank[bk]->tone[prog].instrument =
		load_instrument(dr, bk, prog);
	    if(ip != NULL)
		load_success = 1;
	}
	if(ip == NULL && bk != 0)
	{
	    /* Instrument is not found.
	       Retry to load the instrument from bank 0 */
	    if((ip = bank[0]->tone[prog].instrument) == NULL)
		ip = bank[0]->tone[prog].instrument =
		    load_instrument(dr, 0, prog);
	    if(ip != NULL)
	    {
			/* duplicate tone bank parameter */
			elm = &bank[bk]->tone[prog];
			memcpy(elm, &bank[0]->tone[prog], sizeof(ToneBankElement));
			elm->instrument = ip;
			dup_tone_bank_element(dr, bk, prog);
			load_success = 1;
	    }
	}
    }

    if(load_success)
	aq_add(NULL, 0); /* Update software buffer */

    if(ip == MAGIC_ERROR_INSTRUMENT)
	return NULL;
    if(ip == NULL)
	bank[bk]->tone[prog].instrument = MAGIC_ERROR_INSTRUMENT;

    return ip;
}

#if 0
/* reduce_voice_CPU() may not have any speed advantage over reduce_voice().
 * So this function is not used, now.
 */

/* The goal of this routine is to free as much CPU as possible without
   loosing too much sound quality.  We would like to know how long a note
   has been playing, but since we usually can't calculate this, we guess at
   the value instead.  A bad guess is better than nothing.  Notes which
   have been playing a short amount of time are killed first.  This causes
   decays and notes to be cut earlier, saving more CPU time.  It also causes
   notes which are closer to ending not to be cut as often, so it cuts
   a different note instead and saves more CPU in the long run.  ON voices
   are treated a little differently, since sound quality is more important
   than saving CPU at this point.  Duration guesses for loop regions are very
   crude, but are still better than nothing, they DO help.  Non-looping ON
   notes are cut before looping ON notes.  Since a looping ON note is more
   likely to have been playing for a long time, we want to keep it because it
   sounds better to keep long notes.
*/
static int reduce_voice_CPU(void)
{
    int32 lv, v, vr;
    int i, j, lowest=-0x7FFFFFFF;
    int32 duration;

    i = upper_voices;
    lv = 0x7FFFFFFF;
    
    /* Look for the decaying note with the longest remaining decay time */
    /* Protect drum decays.  They do not take as much CPU (?) and truncating
       them early sounds bad, especially on snares and cymbals */
    for(j = 0; j < i; j++)
    {
	if(voice[j].status & VOICE_FREE || voice[j].cache != NULL)
	    continue;
	/* skip notes that don't need resampling (most drums) */
	if (voice[j].sample->note_to_use)
	    continue;
	if(voice[j].status & ~(VOICE_ON | VOICE_DIE | VOICE_SUSTAINED))
	{
	    /* Choose note with longest decay time remaining */
	    /* This frees more CPU than choosing lowest volume */
	    if (!voice[j].envelope_increment) duration = 0;
	    else duration =
	    	(voice[j].envelope_target - voice[j].envelope_volume) /
	    	voice[j].envelope_increment;
	    v = -duration;
	    if(v < lv)
	    {
		lv = v;
		lowest = j;
	    }
	}
    }
    if(lowest != -0x7FFFFFFF)
    {
	/* This can still cause a click, but if we had a free voice to
	   spare for ramping down this note, we wouldn't need to kill it
	   in the first place... Still, this needs to be fixed. Perhaps
	   we could use a reserve of voices to play dying notes only. */

	cut_notes++;
	return lowest;
    }

    /* try to remove VOICE_DIE before VOICE_ON */
    lv = 0x7FFFFFFF;
    lowest = -1;
    for(j = 0; j < i; j++)
    {
      if(voice[j].status & VOICE_FREE || voice[j].cache != NULL)
	    continue;
      if(voice[j].status & ~(VOICE_ON | VOICE_SUSTAINED))
      {
	/* continue protecting non-resample decays */
	if (voice[j].status & ~(VOICE_DIE) && voice[j].sample->note_to_use)
		continue;

	/* choose note which has been on the shortest amount of time */
	/* this is a VERY crude estimate... */
	if (voice[j].sample->modes & MODES_LOOPING)
	    duration = voice[j].sample_offset - voice[j].sample->loop_start;
	else
	    duration = voice[j].sample_offset;
	if (voice[j].sample_increment > 0)
	    duration /= voice[j].sample_increment;
	v = duration;
	if(v < lv)
	{
	    lv = v;
	    lowest = j;
	}
      }
    }
    if(lowest != -1)
    {
	cut_notes++;
	return lowest;
    }

    /* try to remove VOICE_SUSTAINED before VOICE_ON */
    lv = 0x7FFFFFFF;
    lowest = -0x7FFFFFFF;
    for(j = 0; j < i; j++)
    {
      if(voice[j].status & VOICE_FREE || voice[j].cache != NULL)
	    continue;
      if(voice[j].status & VOICE_SUSTAINED)
      {
	/* choose note which has been on the shortest amount of time */
	/* this is a VERY crude estimate... */
	if (voice[j].sample->modes & MODES_LOOPING)
	    duration = voice[j].sample_offset - voice[j].sample->loop_start;
	else
	    duration = voice[j].sample_offset;
	if (voice[j].sample_increment > 0)
	    duration /= voice[j].sample_increment;
	v = duration;
	if(v < lv)
	{
	    lv = v;
	    lowest = j;
	}
      }
    }
    if(lowest != -0x7FFFFFFF)
    {
	cut_notes++;
	return lowest;
    }

    /* try to remove chorus before VOICE_ON */
    lv = 0x7FFFFFFF;
    lowest = -0x7FFFFFFF;
    for(j = 0; j < i; j++)
    {
      if(voice[j].status & VOICE_FREE || voice[j].cache != NULL)
	    continue;
      if(voice[j].chorus_link < j)
      {
	/* score notes based on both volume AND duration */
	/* this scoring function needs some more tweaking... */
	if (voice[j].sample->modes & MODES_LOOPING)
	    duration = voice[j].sample_offset - voice[j].sample->loop_start;
	else
	    duration = voice[j].sample_offset;
	if (voice[j].sample_increment > 0)
	    duration /= voice[j].sample_increment;
	v = voice[j].left_mix * duration;
	vr = voice[j].right_mix * duration;
	if(voice[j].panned == PANNED_MYSTERY && vr > v)
	    v = vr;
	if(v < lv)
	{
	    lv = v;
	    lowest = j;
	}
      }
    }
    if(lowest != -0x7FFFFFFF)
    {
	cut_notes++;

	/* hack - double volume of chorus partner, fix pan */
	j = voice[lowest].chorus_link;
	voice[j].velocity <<= 1;
    	voice[j].panning = channel[voice[lowest].channel].panning;
    	recompute_amp(j);
    	apply_envelope_to_amp(j);

	return lowest;
    }

    lost_notes++;

    /* try to remove non-looping voices first */
    lv = 0x7FFFFFFF;
    lowest = -0x7FFFFFFF;
    for(j = 0; j < i; j++)
    {
      if(voice[j].status & VOICE_FREE || voice[j].cache != NULL)
	    continue;
      if(!(voice[j].sample->modes & MODES_LOOPING))
      {
	/* score notes based on both volume AND duration */
	/* this scoring function needs some more tweaking... */
	duration = voice[j].sample_offset;
	if (voice[j].sample_increment > 0)
	    duration /= voice[j].sample_increment;
	v = voice[j].left_mix * duration;
	vr = voice[j].right_mix * duration;
	if(voice[j].panned == PANNED_MYSTERY && vr > v)
	    v = vr;
	if(v < lv)
	{
	    lv = v;
	    lowest = j;
	}
      }
    }
    if(lowest != -0x7FFFFFFF)
    {
	return lowest;
    }

    lv = 0x7FFFFFFF;
    lowest = 0;
    for(j = 0; j < i; j++)
    {
	if(voice[j].status & VOICE_FREE || voice[j].cache != NULL)
	    continue;
	if (!(voice[j].sample->modes & MODES_LOOPING)) continue;

	/* score notes based on both volume AND duration */
	/* this scoring function needs some more tweaking... */
	duration = voice[j].sample_offset - voice[j].sample->loop_start;
	if (voice[j].sample_increment > 0)
	    duration /= voice[j].sample_increment;
	v = voice[j].left_mix * duration;
	vr = voice[j].right_mix * duration;
	if(voice[j].panned == PANNED_MYSTERY && vr > v)
	    v = vr;
	if(v < lv)
	{
	    lv = v;
	    lowest = j;
	}
    }

    return lowest;
}
#endif

/* this reduces voices while maintaining sound quality */
static int reduce_voice(void)
{
    int32 lv, v;
    int i, j, lowest=-0x7FFFFFFF;

    i = upper_voices;
    lv = 0x7FFFFFFF;
    
    /* Look for the decaying note with the smallest volume */
    /* Protect drum decays.  Truncating them early sounds bad, especially on
       snares and cymbals */
    for(j = 0; j < i; j++)
    {
	if(voice[j].status & VOICE_FREE ||
	   (voice[j].sample->note_to_use && ISDRUMCHANNEL(voice[j].channel)))
	    continue;
	
	if(voice[j].status & ~(VOICE_ON | VOICE_DIE | VOICE_SUSTAINED))
	{
	    /* find lowest volume */
	    v = voice[j].left_mix;
	    if(voice[j].panned == PANNED_MYSTERY && voice[j].right_mix > v)
	    	v = voice[j].right_mix;
	    if(v < lv)
	    {
		lv = v;
		lowest = j;
	    }
	}
    }
    if(lowest != -0x7FFFFFFF)
    {
	/* This can still cause a click, but if we had a free voice to
	   spare for ramping down this note, we wouldn't need to kill it
	   in the first place... Still, this needs to be fixed. Perhaps
	   we could use a reserve of voices to play dying notes only. */

	cut_notes++;
	free_voice(lowest);
	if(!prescanning_flag)
	    ctl_note_event(lowest);
	return lowest;
    }

    /* try to remove VOICE_DIE before VOICE_ON */
    lv = 0x7FFFFFFF;
    lowest = -1;
    for(j = 0; j < i; j++)
    {
      if(voice[j].status & VOICE_FREE)
	    continue;
      if(voice[j].status & ~(VOICE_ON | VOICE_SUSTAINED))
      {
	/* continue protecting drum decays */
	if (voice[j].status & ~(VOICE_DIE) &&
	    (voice[j].sample->note_to_use && ISDRUMCHANNEL(voice[j].channel)))
		continue;
	/* find lowest volume */
	v = voice[j].left_mix;
	if(voice[j].panned == PANNED_MYSTERY && voice[j].right_mix > v)
	    v = voice[j].right_mix;
	if(v < lv)
	{
	    lv = v;
	    lowest = j;
	}
      }
    }
    if(lowest != -1)
    {
	cut_notes++;
	free_voice(lowest);
	if(!prescanning_flag)
	    ctl_note_event(lowest);
	return lowest;
    }

    /* try to remove VOICE_SUSTAINED before VOICE_ON */
    lv = 0x7FFFFFFF;
    lowest = -0x7FFFFFFF;
    for(j = 0; j < i; j++)
    {
      if(voice[j].status & VOICE_FREE)
	    continue;
      if(voice[j].status & VOICE_SUSTAINED)
      {
	/* find lowest volume */
	v = voice[j].left_mix;
	if(voice[j].panned == PANNED_MYSTERY && voice[j].right_mix > v)
	    v = voice[j].right_mix;
	if(v < lv)
	{
	    lv = v;
	    lowest = j;
	}
      }
    }
    if(lowest != -0x7FFFFFFF)
    {
	cut_notes++;
	free_voice(lowest);
	if(!prescanning_flag)
	    ctl_note_event(lowest);
	return lowest;
    }

    /* try to remove chorus before VOICE_ON */
    lv = 0x7FFFFFFF;
    lowest = -0x7FFFFFFF;
    for(j = 0; j < i; j++)
    {
      if(voice[j].status & VOICE_FREE)
	    continue;
      if(voice[j].chorus_link < j)
      {
	/* find lowest volume */
	v = voice[j].left_mix;
	if(voice[j].panned == PANNED_MYSTERY && voice[j].right_mix > v)
	    v = voice[j].right_mix;
	if(v < lv)
	{
	    lv = v;
	    lowest = j;
	}
      }
    }
    if(lowest != -0x7FFFFFFF)
    {
	cut_notes++;

	/* hack - double volume of chorus partner, fix pan */
	j = voice[lowest].chorus_link;
	voice[j].velocity <<= 1;
    	voice[j].panning = channel[voice[lowest].channel].panning;
    	recompute_amp(j);
    	apply_envelope_to_amp(j);

	free_voice(lowest);
	if(!prescanning_flag)
	    ctl_note_event(lowest);
	return lowest;
    }

    lost_notes++;

    /* remove non-drum VOICE_ON */
    lv = 0x7FFFFFFF;
    lowest = -0x7FFFFFFF;
    for(j = 0; j < i; j++)
    {
        if(voice[j].status & VOICE_FREE ||
	   (voice[j].sample->note_to_use && ISDRUMCHANNEL(voice[j].channel)))
	   	continue;

	/* find lowest volume */
	v = voice[j].left_mix;
	if(voice[j].panned == PANNED_MYSTERY && voice[j].right_mix > v)
	    v = voice[j].right_mix;
	if(v < lv)
	{
	    lv = v;
	    lowest = j;
	}
    }
    if(lowest != -0x7FFFFFFF)
    {
	free_voice(lowest);
	if(!prescanning_flag)
	    ctl_note_event(lowest);
	return lowest;
    }

    /* remove all other types of notes */
    lv = 0x7FFFFFFF;
    lowest = 0;
    for(j = 0; j < i; j++)
    {
	if(voice[j].status & VOICE_FREE)
	    continue;
	/* find lowest volume */
	v = voice[j].left_mix;
	if(voice[j].panned == PANNED_MYSTERY && voice[j].right_mix > v)
	    v = voice[j].right_mix;
	if(v < lv)
	{
	    lv = v;
	    lowest = j;
	}
    }

    free_voice(lowest);
    if(!prescanning_flag)
	ctl_note_event(lowest);
    return lowest;
}



/* Only one instance of a note can be playing on a single channel. */
static int find_voice(MidiEvent *e)
{
  int i, j, lowest=-1, note, ch, status_check, mono_check;
  AlternateAssign *altassign;

  note = MIDI_EVENT_NOTE(e);
  ch = e->channel;

  if(opt_overlap_voice_allow)
      status_check = (VOICE_OFF | VOICE_SUSTAINED);
  else
      status_check = 0xFF;
  mono_check = channel[ch].mono;
  altassign = find_altassign(channel[ch].altassign, note);

  i = upper_voices;
  for(j = 0; j < i; j++)
      if(voice[j].status == VOICE_FREE)
      {
	  lowest = j; /* lower volume */
	  break;
      }

	for (j = 0; j < i; j++)
		if (voice[j].status != VOICE_FREE && voice[j].channel == ch
				&& ((voice[j].status & status_check) && voice[j].note == note
				|| mono_check
				|| (altassign && find_altassign(altassign, voice[j].note))))
			kill_note(j);
		else if (voice[j].status != VOICE_FREE && voice[j].channel == ch
				&& voice[j].note == note && ((channel[ch].assign_mode == 1
				&& voice[j].proximate_flag == 0)
				|| channel[ch].assign_mode == 0))
			kill_note(j);
	for (j = 0; j < i; j++)
		if (voice[j].channel == ch && voice[j].note == note)
			voice[j].proximate_flag = 0;

  if(lowest != -1)
  {
      /* Found a free voice. */
      if(upper_voices <= lowest)
	  upper_voices = lowest + 1;
      return lowest;
  }

  if(i < voices)
      return upper_voices++;
  return reduce_voice();
}

void free_voice(int v1)
{
    int v2;

    v2 = voice[v1].chorus_link;
    if(v1 != v2)
    {
	/* Unlink chorus link */
	voice[v1].chorus_link = v1;
	voice[v2].chorus_link = v2;
    }
    voice[v1].status = VOICE_FREE;
    voice[v1].temper_instant = 0;
}

static int find_free_voice(void)
{
    int i, nv = voices, lowest;
    int32 lv, v;

    for(i = 0; i < nv; i++)
	if(voice[i].status == VOICE_FREE)
	{
	    if(upper_voices <= i)
		upper_voices = i + 1;
	    return i;
	}

    upper_voices = voices;

    /* Look for the decaying note with the lowest volume */
    lv = 0x7FFFFFFF;
    lowest = -1;
    for(i = 0; i < nv; i++)
    {
	if(voice[i].status & ~(VOICE_ON | VOICE_DIE) &&
	   !(voice[i].sample->note_to_use && ISDRUMCHANNEL(voice[i].channel)))
	{
	    v = voice[i].left_mix;
	    if((voice[i].panned==PANNED_MYSTERY) && (voice[i].right_mix>v))
		v = voice[i].right_mix;
	    if(v<lv)
	    {
		lv = v;
		lowest = i;
	    }
	}
    }
    if(lowest != -1 && !prescanning_flag)
    {
	free_voice(lowest);
	ctl_note_event(lowest);
    }
    return lowest;
}

static int select_play_sample(Sample *splist,
		int nsp, int note, int *vlist, MidiEvent *e)
{
	int32 f, fs, ft, fst, fc, cdiff, diff;
	int8 tt = channel[e->channel].temper_type;
	uint8 tp = channel[e->channel].rpnmap[RPN_ADDR_0003];
	Sample *sp, *spc;
	int16 st;
	double ratio;
	int i, j, nv, vel;
	
	if (opt_pure_intonation) {
		if (current_keysig < 8)
			f = freq_table_pureint[current_freq_table][note];
		else
			f = freq_table_pureint[current_freq_table + 12][note];
	} else if (opt_temper_control)
		switch (tt) {
		case 0:
			f = freq_table_tuning[tp][note];
			break;
		case 1:
			f = freq_table_pytha[current_freq_table][note];
			break;
		case 2:
			if (current_temper_keysig < 8)
				f = freq_table_meantone[current_freq_table][note];
			else
				f = freq_table_meantone[current_freq_table + 12][note];
			break;
		case 3:
			if (current_temper_keysig < 8)
				f = freq_table_pureint[current_freq_table][note];
			else
				f = freq_table_pureint[current_freq_table + 12][note];
			break;
		default:	/* user-defined temperaments */
			if ((tt -= 0x40) >= 0 && tt < 4) {
				if (current_temper_keysig < 8)
					f = freq_table_user[tt][current_freq_table][note];
				else
					f = freq_table_user[tt][current_freq_table + 12][note];
			} else
				f = freq_table[note];
			break;
		}
	else
		f = freq_table[note];
	if (opt_temper_control)
		fs = (tt) ? freq_table[note] : freq_table_tuning[tp][note];
	else
		fs = freq_table[note];
	vel = e->b;
	nv = 0;
	for (i = 0, sp = splist; i < nsp; i++, sp++) {
		/* SF2 - Scale Tuning */
		if ((st = sp->scale_tuning) != 100) {
			ratio = pow(2.0, (note - 60) * (st - 100) / 1200.0);
			ft = f * ratio + 0.5, fst = fs * ratio + 0.5;
		} else
			ft = f, fst = fs;
		if (sp->low_freq <= fst && sp->high_freq >= fst
				&& sp->low_vel <= vel && sp->high_vel >= vel) {
			j = vlist[nv] = find_voice(e);
			voice[j].orig_frequency = ft;
			MYCHECK(voice[j].orig_frequency);
			voice[j].sample = sp;
			voice[j].status = VOICE_ON;
			nv++;
		}
	}
	if (nv == 0) {
		cdiff = 0x7fffffff;
		for (i = 0, sp = splist; i < nsp; i++, sp++) {
			/* SF2 - Scale Tuning */
			if ((st = sp->scale_tuning) != 100) {
				ratio = pow(2.0, (note - 60) * (st - 100) / 1200.0);
				ft = f * ratio + 0.5, fst = fs * ratio + 0.5;
			} else
				ft = f, fst = fs;
			diff = abs(sp->root_freq - fst);
			if (diff < cdiff) {
				fc = ft;
				spc = sp;
				cdiff = diff;
			}
		}
		j = vlist[nv] = find_voice(e);
		voice[j].orig_frequency = fc;
		MYCHECK(voice[j].orig_frequency);
		voice[j].sample = spc;
		voice[j].status = VOICE_ON;
		nv++;
	}
	return nv;
}

static int find_samples(MidiEvent *e, int *vlist)
{
	Instrument *ip;
	int i, nv, note, ch, prog, bk;

	ch = e->channel;
	if(channel[ch].special_sample > 0)
	{
	    SpecialPatch *s;

	    s = special_patch[channel[ch].special_sample];
	    if(s == NULL)
	    {
		ctl->cmsg(CMSG_WARNING, VERB_VERBOSE,
			  "Strange: Special patch %d is not installed",
			  channel[ch].special_sample);
		return 0;
	    }
	    note = e->a + channel[ch].key_shift + note_key_offset + key_adjust;

	    if(note < 0)
		note = 0;
	    else if(note > 127)
		note = 127;
	    return select_play_sample(s->sample, s->samples, note, vlist, e);
	}

	bk = channel[ch].bank;
	if(ISDRUMCHANNEL(ch))
	{
	    note = e->a & 0x7F;
	    instrument_map(channel[ch].mapID, &bk, &note);
	    if(!(ip = play_midi_load_instrument(1, bk, note)))
		return 0;	/* No instrument? Then we can't play. */

		if(ip->type == INST_GUS && ip->samples != 1)
		ctl->cmsg(CMSG_WARNING, VERB_VERBOSE,
			  "Strange: percussion instrument with %d samples!",
			  ip->samples);
		if(ip->sample->note_to_use)
		note = ip->sample->note_to_use;
		if(ip->type == INST_SF2) {
			nv = select_play_sample(ip->sample, ip->samples, note, vlist, e);
			/* Replace the sample if the sample is cached. */
			if(!prescanning_flag)
			{
				if(ip->sample->note_to_use)
				note = MIDI_EVENT_NOTE(e);

				for(i = 0; i < nv; i++)
				{
				int j;

				j = vlist[i];
				if(!opt_realtime_playing && allocate_cache_size > 0 &&
				   !channel[ch].portamento)
				{
					voice[j].cache = resamp_cache_fetch(voice[j].sample, note);
					if(voice[j].cache) /* cache hit */
					voice[j].sample = voice[j].cache->resampled;
				}
				else
					voice[j].cache = NULL;
				}
			}
			return nv;
		} else {
			i = vlist[0] = find_voice(e);
			voice[i].orig_frequency = freq_table[note];
			voice[i].sample = ip->sample;
			voice[i].status = VOICE_ON;
			return 1;
		}
	}

	prog = channel[ch].program;
	if(prog == SPECIAL_PROGRAM)
	    ip = default_instrument;
	else
	{
	    instrument_map(channel[ch].mapID, &bk, &prog);
	    if(!(ip = play_midi_load_instrument(0, bk, prog)))
		return 0;	/* No instrument? Then we can't play. */
	}

	if(ip->sample->note_to_use)
	    note = ip->sample->note_to_use + channel[ch].key_shift + note_key_offset + key_adjust;
	else
	    note = e->a + channel[ch].key_shift + note_key_offset + key_adjust;
	if(note < 0)
	    note = 0;
	else if(note > 127)
	    note = 127;

	nv = select_play_sample(ip->sample, ip->samples, note, vlist, e);

	/* Replace the sample if the sample is cached. */
	if(!prescanning_flag)
	{
	    if(ip->sample->note_to_use)
		note = MIDI_EVENT_NOTE(e);

	    for(i = 0; i < nv; i++)
	    {
		int j;

		j = vlist[i];
		if(!opt_realtime_playing && allocate_cache_size > 0 &&
		   !channel[ch].portamento)
		{
		    voice[j].cache = resamp_cache_fetch(voice[j].sample, note);
		    if(voice[j].cache) /* cache hit */
			voice[j].sample = voice[j].cache->resampled;
		}
		else
		    voice[j].cache = NULL;
	    }
	}
	return nv;
}

static int get_panning(int ch, int note,int v)
{
    int i, pan;

	if(voice[v].sample_panning_average == -1) {	/* mono sample */
		if(channel[ch].panning != NO_PANNING) {pan = (int)channel[ch].panning - 64;}
		else {pan = 0;}
		if(ISDRUMCHANNEL(ch) &&
		 channel[ch].drums[note] != NULL &&
		 channel[ch].drums[note]->drum_panning != NO_PANNING) {
			pan += channel[ch].drums[note]->drum_panning;
		} else {
			pan += voice[v].sample->panning;
		}
	} else {	/* stereo sample */
		if(channel[ch].panning != NO_PANNING) {pan = (int)channel[ch].panning - 64;}
		else {pan = 0;}
		if(ISDRUMCHANNEL(ch) &&
		 channel[ch].drums[note] != NULL &&
		 channel[ch].drums[note]->drum_panning != NO_PANNING) {
			pan += channel[ch].drums[note]->drum_panning - 64;
		}
		pan += voice[v].sample->panning - voice[v].sample_panning_average + 64;
	}

	if (pan > 127) pan = 127;
	else if (pan < 0) pan = 0;

	return pan;
}

static void calc_sample_panning_average(int nv,int *vlist)
{
	int i, v, average = 0;

	if(!nv) {return;}	/* error! */
	else if(nv == 1) {	/* mono sample */
		v = vlist[0];
		voice[v].sample_panning_average = -1;
		return;
	}

	for(i=0;i<nv;i++) {
		v = vlist[i];
		average += voice[v].sample->panning;
	}
	average /= nv;

	for(i=0;i<nv;i++) {
		v = vlist[i];
		voice[v].sample_panning_average = average;
	}
}

static void start_note(MidiEvent *e, int i, int vid, int cnt)
{
  int j, ch, note, pan;

  ch = e->channel;

  note = MIDI_EVENT_NOTE(e);
  voice[i].status = VOICE_ON;
  voice[i].channel = ch;
  voice[i].note = note;
  voice[i].velocity = e->b;
  voice[i].chorus_link = i;	/* No link */
  voice[i].proximate_flag = 1;

  j = channel[ch].special_sample;
  if(j == 0 || special_patch[j] == NULL)
      voice[i].sample_offset = 0;
  else
  {
      voice[i].sample_offset =
	  special_patch[j]->sample_offset << FRACTION_BITS;
      if(voice[i].sample->modes & MODES_LOOPING)
      {
	  if(voice[i].sample_offset > voice[i].sample->loop_end)
	      voice[i].sample_offset = voice[i].sample->loop_start;
      }
      else if(voice[i].sample_offset > voice[i].sample->data_length)
      {
	  free_voice(i);
	  return;
      }
  }
  voice[i].sample_increment=0; /* make sure it isn't negative */
  voice[i].modulation_wheel=channel[ch].modulation_wheel;
  voice[i].delay = voice[i].sample->envelope_delay;
  voice[i].modenv_delay = voice[i].sample->modenv_delay;
  voice[i].tremolo_delay = voice[i].sample->tremolo_delay;
  voice[i].vid=vid;

  voice[i].tremolo_phase=0;
  voice[i].tremolo_phase_increment=voice[i].sample->tremolo_phase_increment;
  voice[i].tremolo_sweep=voice[i].sample->tremolo_sweep_increment;
  voice[i].tremolo_sweep_position=0;
  voice[i].tremolo_depth = voice[i].sample->tremolo_depth;

  voice[i].vibrato_sweep=voice[i].sample->vibrato_sweep_increment;
  voice[i].vibrato_sweep_position=0;

  voice[i].delay_counter = 0;

  memset(&(voice[i].fc), 0, sizeof(FilterCoefficients));
  if(opt_lpf_def && voice[i].sample->cutoff_freq) {
	  voice[i].fc.orig_freq = voice[i].sample->cutoff_freq;
	  voice[i].fc.orig_reso_dB = (double)voice[i].sample->resonance / 10.0f;
  } else {
	  voice[i].fc.freq = -1;
  }

  if(opt_nrpn_vibrato)
  {
	  if(channel[ch].vibrato_ratio) {
	      voice[i].vibrato_control_ratio = voice[i].sample->vibrato_control_ratio + channel[ch].vibrato_ratio;
		  if(voice[i].vibrato_control_ratio < 0) {voice[i].vibrato_control_ratio = 0;}
	  }
	  if(channel[ch].vibrato_depth) {
	      voice[i].vibrato_depth = voice[i].sample->vibrato_depth + channel[ch].vibrato_depth;
		  if(voice[i].vibrato_depth > 255) {voice[i].vibrato_depth = 255;}
		  else if(voice[i].vibrato_depth < -255) {voice[i].vibrato_depth = -255;}
	  }
      voice[i].vibrato_delay = voice[i].sample->vibrato_delay + channel[ch].vibrato_delay;
  }
  else
  {
      voice[i].vibrato_control_ratio = voice[i].sample->vibrato_control_ratio;
      voice[i].vibrato_depth = voice[i].sample->vibrato_depth;
      voice[i].vibrato_delay = voice[i].sample->vibrato_delay;
  }
  voice[i].orig_vibrato_control_ratio = voice[i].sample->vibrato_control_ratio;

  voice[i].vibrato_control_counter=voice[i].vibrato_phase=0;
  for (j=0; j<VIBRATO_SAMPLE_INCREMENTS; j++)
    voice[i].vibrato_sample_increment[j]=0;

  /* Pan */
  voice[i].panning = get_panning(ch, note, i);

  voice[i].porta_control_counter = 0;
  if(channel[ch].legato && channel[ch].legato_flag) {
	  update_legato_controls(ch);
  }
  else if(channel[ch].portamento && !channel[ch].porta_control_ratio)
      update_portamento_controls(ch);
  if(channel[ch].porta_control_ratio)
  {
      if(channel[ch].last_note_fine == -1)
      {
	  /* first on */
	  channel[ch].last_note_fine = voice[i].note * 256;
	  channel[ch].porta_control_ratio = 0;
      }
      else
      {
	  voice[i].porta_control_ratio = channel[ch].porta_control_ratio;
	  voice[i].porta_dpb = channel[ch].porta_dpb;
	  voice[i].porta_pb = channel[ch].last_note_fine -
	      voice[i].note * 256;
	  if(voice[i].porta_pb == 0)
	      voice[i].porta_control_ratio = 0;
      }
  }

  if(cnt == 0)
      channel[ch].last_note_fine = voice[i].note * 256;

  recompute_amp(i);
  if (voice[i].sample->modes & MODES_ENVELOPE)
    {
      /* Ramp up from 0 */
	  voice[i].envelope_stage=0;
      voice[i].envelope_volume=0;
      voice[i].control_counter=0;
      recompute_envelope(i);
	  apply_envelope_to_amp(i);
	  voice[i].modenv_stage = 0;
      voice[i].modenv_volume = 0;
      recompute_modulation_envelope(i);
	  apply_modulation_envelope(i);

    }
  else
    {
      voice[i].envelope_increment=0;
	  voice[i].modenv_increment=0;
      apply_envelope_to_amp(i);
	  apply_modulation_envelope(i);
    }
  recompute_freq(i);
  recompute_voice_filter(i);

  voice[i].timeout = -1;
  if(!prescanning_flag)
      ctl_note_event(i);
}

static void finish_note(int i)
{
    if (voice[i].sample->modes & MODES_ENVELOPE)
    {
	/* We need to get the envelope out of Sustain stage. */
	/* Note that voice[i].envelope_stage < 3 */
	voice[i].status=VOICE_OFF;
	voice[i].envelope_stage=3;
	recompute_envelope(i);
	voice[i].modenv_stage=3;
	recompute_modulation_envelope(i);
	apply_modulation_envelope(i);
	apply_envelope_to_amp(i);
	ctl_note_event(i);
	}
    else
    {
	if(current_file_info->pcm_mode != PCM_MODE_NON)
	{
	    free_voice(i);
	    ctl_note_event(i);
	}
	else
	{
	    /* Set status to OFF so resample_voice() will let this voice out
		of its loop, if any. In any case, this voice dies when it
		    hits the end of its data (ofs>=data_length). */
	    if(voice[i].status != VOICE_OFF)
	    {
		voice[i].status = VOICE_OFF;
		ctl_note_event(i);
	    }
	}
    }
}

static void set_envelope_time(int ch,int val,int stage)
{
	val = val & 0x7F;
	switch(stage) {
	case 0:	/* Attack */
		ctl->cmsg(CMSG_INFO,VERB_NOISY,"Attack Time (CH:%d VALUE:%d)",ch,val);
		break;
	case 2: /* Decay */
		ctl->cmsg(CMSG_INFO,VERB_NOISY,"Decay Time (CH:%d VALUE:%d)",ch,val);
		break;
	case 3:	/* Release */
		ctl->cmsg(CMSG_INFO,VERB_NOISY,"Release Time (CH:%d VALUE:%d)",ch,val);
		break;
	default:
		ctl->cmsg(CMSG_INFO,VERB_NOISY,"? Time (CH:%d VALUE:%d)",ch,val);
	}
	channel[ch].envelope_rate[stage] = val;
}

static void new_delay_voice(int v1, int level)
{
    int v2,ch=voice[v1].channel;
	FLOAT_T delay,vol;
	FLOAT_T threshold = 1.0;

	/* NRPN Delay Send Level of Drum */
	if(ISDRUMCHANNEL(ch) &&	channel[ch].drums[voice[v1].note] != NULL) {
		level *= (FLOAT_T)channel[ch].drums[voice[v1].note]->delay_level / 127.0;
	}

	vol = voice[v1].velocity * level / 127.0 * delay_status.level_ratio_c;

	if (vol > threshold) {
		delay = 0;
		if((v2 = find_free_voice()) == -1) {return;}
		voice[v2].cache = NULL;
		delay += delay_status.time_center;
		voice[v2] = voice[v1];	/* copy all parameters */
		voice[v2].velocity = (uint8)vol;
		voice[v2].delay += (int32)(play_mode->rate * delay / 1000);
		recompute_amp(v2);
		apply_envelope_to_amp(v2);
		recompute_freq(v2);
	}
}


static void new_chorus_voice(int v1, int level)
{
    int v2, ch;
    uint8 vol;

    if((v2 = find_free_voice()) == -1)
	return;
    ch = voice[v1].channel;
    vol = voice[v1].velocity;
    voice[v2] = voice[v1];	/* copy all parameters */

	/* NRPN Chorus Send Level of Drum */
	if(ISDRUMCHANNEL(ch) && channel[ch].drums[voice[v1].note] != NULL) {
		level *= (FLOAT_T)channel[ch].drums[voice[v1].note]->chorus_level / 127.0;
	}

    /* Choose lower voice index for base voice (v1) */
    if(v1 > v2)
    {
	int tmp;
	tmp = v1;
	v1 = v2;
	v2 = v1;
    }

    /* v1: Base churos voice
     * v2: Sub chorus voice (detuned)
     */

    voice[v1].velocity = (uint8)(vol * CHORUS_VELOCITY_TUNING1);
    voice[v2].velocity = (uint8)(vol * CHORUS_VELOCITY_TUNING2);

    /* Make doubled link v1 and v2 */
    voice[v1].chorus_link = v2;
    voice[v2].chorus_link = v1;

    level >>= 2;		     /* scale level to a "better" value */
    if(channel[ch].pitchbend + level < 0x2000)
        voice[v2].orig_frequency *= bend_fine[level];
    else
	voice[v2].orig_frequency /= bend_fine[level];

    MYCHECK(voice[v2].orig_frequency);

    voice[v2].cache = NULL;

    /* set panning & delay */
    if(!(play_mode->encoding & PE_MONO))
    {
	double delay;

	if(voice[v2].panned == PANNED_CENTER)
	{
	    voice[v2].panning = 64 + int_rand(40) - 20; /* 64 +- rand(20) */
	    delay = 0;
	}
	else
	{
	    int panning = voice[v2].panning;

	    if(panning < CHORUS_OPPOSITE_THRESHOLD)
	    {
		voice[v2].panning = 127;
		delay = DEFAULT_CHORUS_DELAY1;
	    }
	    else if(panning > 127 - CHORUS_OPPOSITE_THRESHOLD)
	    {
		voice[v2].panning = 0;
		delay = DEFAULT_CHORUS_DELAY1;
	    }
	    else
	    {
		voice[v2].panning = (panning < 64 ? 0 : 127);
		delay = DEFAULT_CHORUS_DELAY2;
	    }
	}
	voice[v2].delay += (int)(play_mode->rate * delay);
    }

    recompute_amp(v1);
    apply_envelope_to_amp(v1);
    recompute_amp(v2);
    apply_envelope_to_amp(v2);

    /* voice[v2].orig_frequency is changed.
     * Update the depened parameters.
     */
    recompute_freq(v2);
}

/* Yet another chorus implementation
 *	by Eric A. Welsh <ewelsh@gpc.wustl.edu>.
 */
static void new_chorus_voice_alternate(int v1, int level)
{
    int v2, ch, panlevel;
    uint8 vol, pan;
    double delay;

    if((v2 = find_free_voice()) == -1)
	return;
    ch = voice[v1].channel;
    voice[v2] = voice[v1];

	/* NRPN Chorus Send Level of Drum */
	if(ISDRUMCHANNEL(ch) && channel[ch].drums[voice[v1].note] != NULL) {
		level *= (FLOAT_T)channel[ch].drums[voice[v1].note]->chorus_level / 127.0;
	}

    /* for our purposes, hard left will be equal to 1 instead of 0 */
    pan = voice[v1].panning;
    if (!pan) pan = 1;

    /* Choose lower voice index for base voice (v1) */
    if(v1 > v2)
    {
	int tmp;
	tmp = v1;
	v1 = v2;
	v2 = v1;
    }

    /* lower the volumes so that the two notes add to roughly the orig. vol */
    vol = voice[v1].velocity;
    voice[v1].velocity  = (uint8)(vol * CHORUS_VELOCITY_TUNING1);
    voice[v2].velocity  = (uint8)(vol * CHORUS_VELOCITY_TUNING1);

    /* Make doubled link v1 and v2 */
    voice[v1].chorus_link = v2;
    voice[v2].chorus_link = v1;

    /* detune notes for chorus effect */
    level >>= 2;		/* scale to a "better" value */
    if (level)
    {
        if(channel[ch].pitchbend + level < 0x2000)
            voice[v2].orig_frequency *= bend_fine[level];
        else
	    voice[v2].orig_frequency /= bend_fine[level];
        voice[v2].cache = NULL;
    }

    MYCHECK(voice[v2].orig_frequency);

    delay = 0.0025;

    /* Try to keep the delayed voice from cancelling out the other voice */
    /* Don't bother with trying to figure out drum pitches... */
    /* Don't bother with mod files for the same reason... */
    /* Drums and mods could be fixed, but pitch detection is too expensive */
    if (!ISDRUMCHANNEL(voice[v1].channel) &&
    	current_file_info->file_type != IS_MOD_FILE &&
    	current_file_info->file_type != IS_S3M_FILE)
    {
    	double freq, frac;
    
    	freq = pitch_freq_table[voice[v1].note];
    	delay *= freq;
    	frac = delay - floor(delay);

	/* force the delay away from 0.5 period */
    	if (frac < 0.5 && frac > 0.40)
    	{
    	    delay = (floor(delay) + 0.40) / freq;
    	    if (play_mode->encoding & ~PE_MONO)
    	    	delay += (0.5 - frac) * (1.0 - labs(64 - pan) / 63.0) / freq;
    	}
    	else if (frac >= 0.5 && frac < 0.60)
    	{
    	    delay = (floor(delay) + 0.60) / freq;
    	    if (play_mode->encoding & ~PE_MONO)
    	    	delay += (0.5 - frac) * (1.0 - labs(64 - pan) / 63.0) / freq;
    	}
    	else
	    delay = 0.0025;
    }

    /* set panning & delay for pseudo-surround effect */
    if(play_mode->encoding & PE_MONO)    /* delay sounds good */
        voice[v2].delay += (int)(play_mode->rate * delay);
    else
    {
        panlevel = 63;
        if (pan - panlevel < 1) panlevel = pan - 1;
        if (pan + panlevel > 127) panlevel = 127 - pan;
        voice[v1].panning -= panlevel;
        voice[v2].panning += panlevel;

        /* choose which voice is delayed based on panning */
        if (voice[v1].panned == PANNED_CENTER) {
            /* randomly choose which voice is delayed */
            if (int_rand(2))
                voice[v1].delay += (int)(play_mode->rate * delay);
            else
                voice[v2].delay += (int)(play_mode->rate * delay);
        }
        else if (pan - 64 < 0) {
            voice[v2].delay += (int)(play_mode->rate * delay);
        }
        else {
            voice[v1].delay += (int)(play_mode->rate * delay);
        }
    }

    recompute_amp(v1);
    apply_envelope_to_amp(v1);
    recompute_amp(v2);
    apply_envelope_to_amp(v2);
    if (level) recompute_freq(v2);
}

static void note_on(MidiEvent *e)
{
    int i, nv, v, ch, note;
    int vlist[32];
    int vid;

    if((nv = find_samples(e, vlist)) == 0)
	return;

    note = MIDI_EVENT_NOTE(e);
    vid = new_vidq(e->channel, note);
    ch = e->channel;

	recompute_bank_parameter(ch,note);
	recompute_channel_filter(e);
	calc_sample_panning_average(nv, vlist);

    for(i = 0; i < nv; i++)
    {
	v = vlist[i];
	if(ISDRUMCHANNEL(ch) &&
	   channel[ch].drums[note] != NULL &&
	   channel[ch].drums[note]->pan_random)
	    channel[ch].drums[note]->drum_panning = int_rand(128);
	else if(channel[ch].pan_random)
	{
	    channel[ch].panning = int_rand(128);
	    ctl_mode_event(CTLE_PANNING, 1, ch, channel[ch].panning);
	}
	start_note(e, v, vid, nv - i - 1);
#ifdef SMOOTH_MIXING
	voice[v].old_left_mix = voice[v].old_right_mix =
	voice[v].left_mix_inc = voice[v].left_mix_offset =
	voice[v].right_mix_inc = voice[v].right_mix_offset = 0;
#endif
#ifdef USE_DSP_EFFECT
	if(opt_surround_chorus)
	    new_chorus_voice_alternate(v, 0);
#else
	if((channel[ch].chorus_level || opt_surround_chorus))
	{
	    if(opt_surround_chorus)
		new_chorus_voice_alternate(v, channel[ch].chorus_level);
	    else
		new_chorus_voice(v, channel[ch].chorus_level);
	}
	if(channel[ch].delay_level)
	{
	    new_delay_voice(v, channel[ch].delay_level);
	}
#endif
    }

    channel[ch].legato_flag = 1;
}

static void set_voice_timeout(Voice *vp, int ch, int note)
{
    int prog;
    ToneBank *bank;

    if(channel[ch].special_sample > 0)
	return;

    if(ISDRUMCHANNEL(ch))
    {
	prog = note;
	bank = drumset[(int)channel[ch].bank];
	if(bank == NULL)
	    bank = drumset[0];
    }
    else
    {
	if((prog = channel[ch].program) == SPECIAL_PROGRAM)
	    return;
	bank = tonebank[(int)channel[ch].bank];
	if(bank == NULL)
	    bank = tonebank[0];
    }

    if(bank->tone[prog].loop_timeout > 0)
	vp->timeout = (int32)(bank->tone[prog].loop_timeout
			      * play_mode->rate * midi_time_ratio
			      + current_sample);
}

static void note_off(MidiEvent *e)
{
  int uv = upper_voices, i;
  int ch, note, vid, sustain;

  ch = e->channel;
  note = MIDI_EVENT_NOTE(e);

  if(ISDRUMCHANNEL(ch) && channel[ch].drums[note] != NULL &&
     !channel[ch].drums[note]->rx_note_off)
  {
      ToneBank *bank;

      bank = drumset[channel[ch].bank];
      if(bank == NULL) bank = drumset[0];
      
      /* uh oh, this drum doesn't have an instrument loaded yet */
      if (bank->tone[note].instrument == NULL)
          return;

      /* only disallow Note Off if the drum sample is not looped */
      if (!(bank->tone[note].instrument->sample->modes & MODES_LOOPING))
	  return;	/* Note Off is not allowed. */
  }

  if((vid = last_vidq(ch, note)) == -1)
      return;
  sustain = channel[ch].sustain;
  for(i = 0; i < uv; i++)
  {
      if(voice[i].status==VOICE_ON &&
	 voice[i].channel==ch &&
	 voice[i].note==note &&
	 voice[i].vid==vid)
      {
	  if(sustain)
	  {
	      voice[i].status=VOICE_SUSTAINED;
	      set_voice_timeout(voice + i, ch, note);
	      ctl_note_event(i);
	  }
	  else
	      finish_note(i);
      }
  }

  channel[ch].legato_flag = 0;
}

/* Process the All Notes Off event */
static void all_notes_off(int c)
{
  int i, uv = upper_voices;
  ctl->cmsg(CMSG_INFO, VERB_DEBUG, "All notes off on channel %d", c);
  for(i = 0; i < uv; i++)
    if (voice[i].status==VOICE_ON &&
	voice[i].channel==c)
      {
	if (channel[c].sustain)
	  {
	    voice[i].status=VOICE_SUSTAINED;
	    set_voice_timeout(voice + i, c, voice[i].note);
	    ctl_note_event(i);
	  }
	else
	  finish_note(i);
      }
  for(i = 0; i < 128; i++)
      vidq_head[c * 128 + i] = vidq_tail[c * 128 + i] = 0;
}

/* Process the All Sounds Off event */
static void all_sounds_off(int c)
{
  int i, uv = upper_voices;
  for(i = 0; i < uv; i++)
    if (voice[i].channel==c &&
	(voice[i].status & ~(VOICE_FREE | VOICE_DIE)))
      {
	kill_note(i);
      }
  for(i = 0; i < 128; i++)
      vidq_head[c * 128 + i] = vidq_tail[c * 128 + i] = 0;
}

static void adjust_pressure(MidiEvent *e)
{
 /*   int i, uv = upper_voices;
    int note, ch;
	FLOAT_T pressure, amp_ctl, rate_ctl1, pitch_depth1, cutoff_ctl, coef;

	ch = e->channel;
	pressure = (FLOAT_T)e->b / 127.0f;
	amp_ctl = channel[ch].caf_amp_ctl * pressure;
	rate_ctl1 = channel[ch].caf_rate_ctl1 * pressure + 1.0f;
	pitch_depth1 = channel[ch].caf_pitch_depth1 * pressure + 1.0f;
	cutoff_ctl = channel[ch].caf_cutoff_ctl * pressure + 1.0f;
    note = MIDI_EVENT_NOTE(e);

    for(i = 0; i < uv; i++)
    if(voice[i].status == VOICE_ON &&
       voice[i].channel == ch &&
       voice[i].note == note)
    {
		recompute_amp(i);
		voice[i].tremolo_depth = amp_ctl * 255;
		apply_envelope_to_amp(i);
		voice[i].vibrato_control_ratio *= rate_ctl1;
		voice[i].vibrato_depth *= pitch_depth1;
		recompute_freq(i);
		if(opt_lpf_def && voice[i].sample->cutoff_freq) {
			coef = channel[ch].cutoff_freq_coef;
			channel[ch].cutoff_freq_coef *= cutoff_ctl;
			recompute_voice_filter(i);
			channel[ch].cutoff_freq_coef = coef;
		}
    }*/
}

static void adjust_channel_pressure(MidiEvent *e)
{
 /*   if(opt_channel_pressure)
    {
	int i, uv = upper_voices;
	int ch;
	FLOAT_T pressure, amp_ctl, rate_ctl1, pitch_depth1, cutoff_ctl, coef;

	ch = e->channel;
	pressure = (FLOAT_T)e->a / 127.0f;
	amp_ctl = channel[ch].caf_amp_ctl * pressure;
	rate_ctl1 = channel[ch].caf_rate_ctl1 * pressure + 1.0f;
	pitch_depth1 = channel[ch].caf_pitch_depth1 * pressure + 1.0f;
	cutoff_ctl = channel[ch].caf_cutoff_ctl * pressure + 1.0f;
	  
	for(i = 0; i < uv; i++)
	{
	    if(voice[i].status == VOICE_ON && voice[i].channel == ch)
	    {
		recompute_amp(i);
		voice[i].tremolo_depth = amp_ctl * 255;
		apply_envelope_to_amp(i);
		voice[i].vibrato_control_ratio *= rate_ctl1;
		voice[i].vibrato_depth *= pitch_depth1;
		recompute_freq(i);
		if(opt_lpf_def && voice[i].sample->cutoff_freq) {
			coef = channel[ch].cutoff_freq_coef;
			channel[ch].cutoff_freq_coef *= cutoff_ctl;
			recompute_voice_filter(i);
			channel[ch].cutoff_freq_coef = coef;
		}
	    }
	}
    }*/
}

static void adjust_panning(int c)
{
    int i, uv = upper_voices, pan = channel[c].panning;
    for(i = 0; i < uv; i++)
    {
	if ((voice[i].channel==c) &&
	    (voice[i].status & (VOICE_ON | VOICE_SUSTAINED)))
	{
            /* adjust pan to include drum/sample pan offsets */
			pan = get_panning(c,i,i);

	    /* Hack to handle -EFchorus=2 in a "reasonable" way */
#ifdef USE_DSP_EFFECT
	    if(opt_surround_chorus && voice[i].chorus_link != i)
#else
	    if((channel[c].chorus_level || opt_surround_chorus) &&
	       voice[i].chorus_link != i)
#endif
	    {
		int v1, v2;

		if(i >= voice[i].chorus_link)
		    /* `i' is not base chorus voice.
		     *  This sub voice is already updated.
		     */
		    continue;

		v1 = i;				/* base voice */
		v2 = voice[i].chorus_link;	/* sub voice (detuned) */

		if(opt_surround_chorus) /* Surround chorus mode by Eric. */
		{
		    int panlevel;

		    if (!pan) pan = 1;	/* make hard left be 1 instead of 0 */
		    panlevel = 63;
		    if (pan - panlevel < 1) panlevel = pan - 1;
		    if (pan + panlevel > 127) panlevel = 127 - pan;
		    voice[v1].panning = pan - panlevel;
		    voice[v2].panning = pan + panlevel;
		}
		else
		{
		    voice[v1].panning = pan;
		    if(pan > 60 && pan < 68) /* PANNED_CENTER */
			voice[v2].panning =
			    64 + int_rand(40) - 20; /* 64 +- rand(20) */
		    else if(pan < CHORUS_OPPOSITE_THRESHOLD)
			voice[v2].panning = 127;
		    else if(pan > 127 - CHORUS_OPPOSITE_THRESHOLD)
			voice[v2].panning = 0;
		    else
			voice[v2].panning = (pan < 64 ? 0 : 127);
		}
		recompute_amp(v2);
		apply_envelope_to_amp(v2);
		/* v1 == i, so v1 will be updated next */
	    }
	    else
		voice[i].panning = pan;

	    recompute_amp(i);
	    apply_envelope_to_amp(i);
	}
    }
}

void play_midi_setup_drums(int ch, int note)
{
    channel[ch].drums[note] = (struct DrumParts *)
	new_segment(&playmidi_pool, sizeof(struct DrumParts));
    reset_drum_controllers(channel[ch].drums, note);
}

static void adjust_drum_panning(int ch, int note)
{
    int i, uv = upper_voices, pan;

    for(i = 0; i < uv; i++) {
		if(voice[i].channel == ch &&
		   voice[i].note == note &&
		   (voice[i].status & (VOICE_ON | VOICE_SUSTAINED)))
		{
			voice[i].panning = get_panning(ch,note,i);
			recompute_amp(i);
			apply_envelope_to_amp(i);
		}
	}
}

static void drop_sustain(int c)
{
  int i, uv = upper_voices;
  for(i = 0; i < uv; i++)
    if (voice[i].status==VOICE_SUSTAINED && voice[i].channel==c)
      finish_note(i);
}

static void adjust_pitch(int c)
{
  int i, uv = upper_voices;
  for(i = 0; i < uv; i++)
    if (voice[i].status!=VOICE_FREE && voice[i].channel==c)
	recompute_freq(i);
}

static void adjust_volume(int c)
{
  int i, uv = upper_voices;
  for(i = 0; i < uv; i++)
    if (voice[i].channel==c &&
	(voice[i].status & (VOICE_ON | VOICE_SUSTAINED)))
      {
	recompute_amp(i);
	apply_envelope_to_amp(i);
      }
}

static void set_reverb_level(int ch, int level)
{
    if(opt_reverb_control <= 0)
    {
	channel[ch].reverb_level = channel[ch].reverb_id =
	    -opt_reverb_control;
	make_rvid_flag = 1;
	return;
    }
    channel[ch].reverb_level = level;
    make_rvid_flag = 0;	/* to update reverb_id */
}

int get_reverb_level(int ch)
{
    if(opt_reverb_control <= 0)
	return -opt_reverb_control;

    if(channel[ch].reverb_level == -1)
	return DEFAULT_REVERB_SEND_LEVEL;
    return channel[ch].reverb_level;
}

int get_chorus_level(int ch)
{
#ifdef DISALLOW_DRUM_BENDS
    if(ISDRUMCHANNEL(ch))
	return 0; /* Not supported drum channel chorus */
#endif
    if(opt_chorus_control == 1)
	return channel[ch].chorus_level;
    return -opt_chorus_control;
}

static void make_rvid(void)
{
    int i, j, lv, maxrv;

    for(maxrv = MAX_CHANNELS - 1; maxrv >= 0; maxrv--)
    {
	if(channel[maxrv].reverb_level == -1)
	    channel[maxrv].reverb_id = -1;
	else if(channel[maxrv].reverb_level >= 0)
	    break;
    }

    /* collect same reverb level. */
    for(i = 0; i <= maxrv; i++)
    {
	if((lv = channel[i].reverb_level) == -1)
	{
	    channel[i].reverb_id = -1;
	    continue;
	}
	channel[i].reverb_id = i;
	for(j = 0; j < i; j++)
	{
	    if(channel[j].reverb_level == lv)
	    {
		channel[i].reverb_id = j;
		break;
	    }
	}
    }
}

static void adjust_master_volume(void)
{
  int i, uv = upper_voices;
  adjust_amplification();
  for(i = 0; i < uv; i++)
      if(voice[i].status & (VOICE_ON | VOICE_SUSTAINED))
      {
	  recompute_amp(i);
	  apply_envelope_to_amp(i);
      }
}

int midi_drumpart_change(int ch, int isdrum)
{
    if(IS_SET_CHANNELMASK(drumchannel_mask, ch))
	return 0;
    if(isdrum)
	SET_CHANNELMASK(drumchannels, ch);
    else
	UNSET_CHANNELMASK(drumchannels, ch);
    return 1;
}

void midi_program_change(int ch, int prog)
{
    int newbank, dr;

    dr = (int)ISDRUMCHANNEL(ch);
    if(dr)
	newbank = channel[ch].program;
    else
	newbank = channel[ch].bank;

    switch(play_system_mode)
    {
      case GS_SYSTEM_MODE: /* GS */
	switch(channel[ch].bank_lsb)
	{
	  case 0:	/* No change */
	    break;
	  case 1:
	    channel[ch].mapID = (!ISDRUMCHANNEL(ch) ? SC_55_TONE_MAP
				 : SC_55_DRUM_MAP);
	    break;
	  case 2:
	    channel[ch].mapID = (!ISDRUMCHANNEL(ch) ? SC_88_TONE_MAP
				 : SC_88_DRUM_MAP);
	    break;
	  case 3:
	    channel[ch].mapID = (!ISDRUMCHANNEL(ch) ? SC_88PRO_TONE_MAP
				 : SC_88PRO_DRUM_MAP);
	    break;
	  default:
	    break;
	}
	newbank = channel[ch].bank_msb;
	break;

      case XG_SYSTEM_MODE: /* XG */
	switch(channel[ch].bank_msb)
	{
	  case 0: /* Normal */
	    if(ch == 9  && channel[ch].bank_lsb == 127 && channel[ch].mapID == XG_DRUM_MAP) {
	      /* FIXME: Why this part is drum?  Is this correct? */
	      ;
	    } else {
	      midi_drumpart_change(ch, 0);
	      channel[ch].mapID = XG_NORMAL_MAP;
	    }
	    break;
	  case 64: /* SFX voice */
	    midi_drumpart_change(ch, 0);
	    channel[ch].mapID = XG_SFX64_MAP;
	    break;
	  case 126: /* SFX kit */
	    midi_drumpart_change(ch, 1);
	    channel[ch].mapID = XG_SFX126_MAP;
	    break;
	  case 127: /* Drumset */
	    midi_drumpart_change(ch, 1);
	    channel[ch].mapID = XG_DRUM_MAP;
	    break;
	  default:
	    break;
	}
	dr = ISDRUMCHANNEL(ch);
	newbank = channel[ch].bank_lsb;
	break;

      default:
	newbank = channel[ch].bank_msb;
	break;
    }

    if(dr)
    {
	channel[ch].bank = prog; /* newbank is ignored */
	if(drumset[prog] == NULL || drumset[prog]->alt == NULL)
	    channel[ch].altassign = drumset[0]->alt;
	else
	    channel[ch].altassign = drumset[prog]->alt;
	ctl_mode_event(CTLE_DRUMPART, 1, ch, 1);
    }
    else
    {
	if(special_tonebank >= 0)
	    newbank = special_tonebank;
	channel[ch].bank = newbank;
	channel[ch].altassign = NULL;
	ctl_mode_event(CTLE_DRUMPART, 1, ch, 0);
    }

    if(!dr && default_program[ch] == SPECIAL_PROGRAM)
      channel[ch].program = SPECIAL_PROGRAM;
    else
      channel[ch].program = prog;

    if(opt_realtime_playing && !dr && (play_mode->flag & PF_PCM_STREAM))
    {
	int b, p;

	p = prog;
	b = channel[ch].bank;
	instrument_map(channel[ch].mapID, &b, &p);
	play_midi_load_instrument(0, b, p);
	}
}

static void process_sysex_event(int ev,int ch,int val,int b)
{
	if(ev == ME_SYSEX_GS_LSB)
	{
		switch(b)
		{
		case 0x00:	/* EQ ON/OFF */
			if(!opt_eq_control) {break;}
			if(channel[ch].eq_on != val) {
				if(val) {
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"EQ ON (CH:%d)",ch);
				} else {
					ctl->cmsg(CMSG_INFO,VERB_NOISY,"EQ OFF (CH:%d)",ch);
				}
			}
			channel[ch].eq_on = val;
			break;
		case 0x01:	/* EQ LOW FREQ */
			if(!opt_eq_control) {break;}
			eq_status.low_freq = val;
			recompute_eq_status();
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"EQ LOW FREQ (%d)",val);
			break;
		case 0x02:	/* EQ LOW GAIN */
			if(!opt_eq_control) {break;}
			eq_status.low_gain = val;
			recompute_eq_status();
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"EQ LOW GAIN (%d dB)",val - 0x40);
			break;
		case 0x03:	/* EQ HIGH FREQ */
			if(!opt_eq_control) {break;}
			eq_status.high_freq = val;
			recompute_eq_status();
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"EQ HIGH FREQ (%d)",val);
			break;
		case 0x04:	/* EQ HIGH GAIN */
			if(!opt_eq_control) {break;}
			eq_status.high_gain = val;
			recompute_eq_status();
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"EQ HIGH GAIN (%d dB)",val - 0x40);
			break;
		case 0x05:	/* Reverb Macro */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Reverb Macro (%d)",val);
			set_reverb_macro(val);
			recompute_reverb_status();
			init_reverb(play_mode->rate);
			break;
		case 0x06:	/* Reverb Character */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Reverb Character (%d)",val);
			if(reverb_status.character != val) {
				reverb_status.character = val;
				recompute_reverb_status();
				init_reverb(play_mode->rate);
			}
			break;
		case 0x07:	/* Reverb Pre-LPF */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Reverb Pre-LPF (%d)",val);
			if(reverb_status.pre_lpf != val) {
				reverb_status.pre_lpf = val;
				recompute_reverb_status();
			}
			break;
		case 0x08:	/* Reverb Level */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Reverb Level (%d)",val);
			if(reverb_status.level != val) {
				reverb_status.level = val;
				recompute_reverb_status();
				init_reverb(play_mode->rate);
			}
			break;
		case 0x09:	/* Reverb Time */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Reverb Time (%d)",val);
			if(reverb_status.time != val) {
				reverb_status.time = val;
				recompute_reverb_status();
				init_reverb(play_mode->rate);
			}
			break;
		case 0x0A:	/* Reverb Delay Feedback */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Reverb Delay Feedback (%d)",val);
			if(reverb_status.delay_feedback != val) {
				reverb_status.delay_feedback = val;
				recompute_reverb_status();
			}
			break;
		case 0x0C:	/* Reverb Predelay Time */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Reverb Predelay Time (%d)",val);
			if(reverb_status.pre_delay_time != val) {
				reverb_status.pre_delay_time = val;
				recompute_reverb_status();
			}
			break;
		case 0x0D:	/* Chorus Macro */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Chorus Macro (%d)",val);
			init_ch_chorus();
			set_chorus_macro(val);
			recompute_chorus_status();
			init_chorus_lfo();
			break;
		case 0x0E:	/* Chorus Pre-LPF */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Chorus Pre-LPF (%d)",val);
			chorus_param.chorus_pre_lpf = val;
			recompute_chorus_status();
			break;
		case 0x0F:	/* Chorus Level */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Chorus Level (%d)",val);
			chorus_param.chorus_level = val;
			recompute_chorus_status();
			break;
		case 0x10:	/* Chorus Feedback */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Chorus Feedback (%d)",val);
			chorus_param.chorus_feedback = val;
			recompute_chorus_status();
			break;
		case 0x11:	/* Chorus Delay */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Chorus Delay (%d)",val);
			chorus_param.chorus_delay = val;
			init_ch_chorus();
			recompute_chorus_status();
			init_chorus_lfo();
			break;
		case 0x12:	/* Chorus Rate */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Chorus Rate (%d)",val);
			chorus_param.chorus_rate = val;
			recompute_chorus_status();
			init_chorus_lfo();
			break;
		case 0x13:	/* Chorus Depth */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Chorus Depth (%d)",val);
			chorus_param.chorus_depth = val;
			recompute_chorus_status();
			init_chorus_lfo();
			break;
		case 0x14:	/* Chorus Send Level to Reverb */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Chorus Send Level to Reverb (%d)",val);
			chorus_param.chorus_send_level_to_reverb = val;
			recompute_chorus_status();
			break;
		case 0x15:	/* Chorus Send Level to Delay */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Chorus Send Level to Delay (%d)",val);
			chorus_param.chorus_send_level_to_delay = val;
			recompute_chorus_status();
			break;
		case 0x16:	/* Delay Macro */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Delay Macro (%d)",val);
			init_ch_delay();
			set_delay_macro(val);
			recompute_delay_status();
			break;
		case 0x17:	/* Delay Pre-LPF */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Delay Pre-LPF (%d)",val);
			delay_status.pre_lpf = val;
			recompute_delay_status();
			break;
		case 0x18:	/* Delay Time Center */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Delay Time Center (%d)",val);
			init_ch_delay();
			delay_status.time_center = delay_time_center_table[val > 0x73 ? 0x73 : val];
			recompute_delay_status();
			break;
		case 0x19:	/* Delay Time Ratio Left */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Delay Time Ratio Left (%d)",val);
			if(val == 0) {val = 1;}
			delay_status.time_ratio_left = (double)val / 24;
			init_ch_delay();
			recompute_delay_status();
			break;
		case 0x1A:	/* Delay Time Ratio Right */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Delay Time Ratio Right (%d)",val);
			if(val == 0) {val = 1;}
			delay_status.time_ratio_right = (double)val / 24;
			init_ch_delay();
			recompute_delay_status();
			break;
		case 0x1B:	/* Delay Level Center */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Delay Level Center (%d)",val);
			delay_status.level_center = val;
			recompute_delay_status();
			break;
		case 0x1C:	/* Delay Level Left */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Delay Level Left (%d)",val);
			delay_status.level_left = val;
			recompute_delay_status();
			break;
		case 0x1D:	/* Delay Level Right */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Delay Level Right (%d)",val);
			delay_status.level_right = val;
			recompute_delay_status();
			break;
		case 0x1E:	/* Delay Level */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Delay Level (%d)",val);
			delay_status.level = val;
			recompute_delay_status();
			break;
		case 0x1F:	/* Delay Feedback */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Delay Feedback (%d)",val);
			delay_status.feedback = val;
			recompute_delay_status();
			break;
		case 0x20:	/* Delay Send Level to Reverb */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Delay Send Level to Reverb (%d)",val);
			delay_status.send_reverb = val;
			recompute_delay_status();
			break;
		case 0x21:	/* Velocity Sense Depth */
			channel[ch].velocity_sense_depth = val;
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Velocity Sense Depth (CH:%d VAL:%d)",ch,val);
			break;
		case 0x22:	/* Velocity Sense Offset */
			channel[ch].velocity_sense_offset = val;
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Velocity Sense Offset (CH:%d VAL:%d)",ch,val);
			break;
		case 0x23:	/* Insertion Effect ON/OFF */
			if(!opt_insertion_effect) {break;}
			if(channel[ch].insertion_effect != val) {
				if(val) {ctl->cmsg(CMSG_INFO,VERB_NOISY,"EFX ON (CH:%d)",ch);}
				else {ctl->cmsg(CMSG_INFO,VERB_NOISY,"EFX OFF (CH:%d)",ch);}
			}
			channel[ch].insertion_effect = val;
			break;
		case 0x24:	/* Assign Mode */
			channel[ch].assign_mode = val;
			if(val == 0) {
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"Assign Mode: Single (CH:%d)",ch);
			} else if(val == 1) {
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"Assign Mode: Limited-Multi (CH:%d)",ch);
			} else if(val == 2) {
				ctl->cmsg(CMSG_INFO,VERB_NOISY,"Assign Mode: Full-Multi (CH:%d)",ch);
			}
			break;
		case 0x25:	/* TONE MAP-0 NUMBER */
			channel[ch].tone_map0_number = val;
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Tone Map-0 Number (CH:%d VAL:%d)",ch,val);
			break;
		case 0x26:	/* Pitch Offset Fine */
			channel[ch].pitch_offset_fine = (FLOAT_T)((((int32)val << 4) | (int32)val) - 0x80) / 10.0;
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Pitch Offset Fine (CH:%d %3fHz)",ch,channel[ch].pitch_offset_fine);
			break;
		case 0x27:	/* Insertion Effect Parameter */
			if(!opt_insertion_effect) {break;}
			gs_ieffect.type_msb = val;
			break;
		case 0x28:	/* Insertion Effect Parameter */
			if(!opt_insertion_effect) {break;}
			gs_ieffect.type_lsb = val;
			gs_ieffect.type = ((int32)gs_ieffect.type_msb << 8) | (int32)gs_ieffect.type_lsb;
			set_insertion_effect_default_parameter();
			recompute_insertion_effect();
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"EFX TYPE (%02X %02X)",gs_ieffect.type_msb,gs_ieffect.type_lsb);
			break;
		case 0x29:
			gs_ieffect.parameter[0] = val;
			recompute_insertion_effect();
			break;
		case 0x2A:
			gs_ieffect.parameter[1] = val;
			recompute_insertion_effect();
			break;
		case 0x2B:
			gs_ieffect.parameter[2] = val;
			recompute_insertion_effect();
			break;
		case 0x2C:
			gs_ieffect.parameter[3] = val;
			recompute_insertion_effect();
			break;
		case 0x2D:
			gs_ieffect.parameter[4] = val;
			recompute_insertion_effect();
			break;
		case 0x2E:
			gs_ieffect.parameter[5] = val;
			recompute_insertion_effect();
			break;
		case 0x2F:
			gs_ieffect.parameter[6] = val;
			recompute_insertion_effect();
			break;
		case 0x30:
			gs_ieffect.parameter[7] = val;
			recompute_insertion_effect();
			break;
		case 0x31:
			gs_ieffect.parameter[8] = val;
			recompute_insertion_effect();
			break;
		case 0x32:
			gs_ieffect.parameter[9] = val;
			recompute_insertion_effect();
			break;
		case 0x33:
			gs_ieffect.parameter[10] = val;
			recompute_insertion_effect();
			break;
		case 0x34:
			gs_ieffect.parameter[11] = val;
			recompute_insertion_effect();
			break;
		case 0x35:
			gs_ieffect.parameter[12] = val;
			recompute_insertion_effect();
			break;
		case 0x36:
			gs_ieffect.parameter[13] = val;
			recompute_insertion_effect();
			break;
		case 0x37:
			gs_ieffect.parameter[14] = val;
			recompute_insertion_effect();
			break;
		case 0x38:
			gs_ieffect.parameter[15] = val;
			recompute_insertion_effect();
			break;
		case 0x39:
			gs_ieffect.parameter[16] = val;
			recompute_insertion_effect();
			break;
		case 0x3A:
			gs_ieffect.parameter[17] = val;
			recompute_insertion_effect();
			break;
		case 0x3B:
			gs_ieffect.parameter[18] = val;
			recompute_insertion_effect();
			break;
		case 0x3C:
			gs_ieffect.parameter[19] = val;
			recompute_insertion_effect();
			break;
		case 0x3D:
			gs_ieffect.send_reverb = val;
			recompute_insertion_effect();
			break;
		case 0x3E:
			gs_ieffect.send_chorus = val;
			recompute_insertion_effect();
			break;
		case 0x3F:
			gs_ieffect.send_delay = val;
			recompute_insertion_effect();
			break;
		case 0x40:
			gs_ieffect.control_source1 = val;
			recompute_insertion_effect();
			break;
		case 0x41:
			gs_ieffect.control_depth1 = val;
			recompute_insertion_effect();
			break;
		case 0x42:
			gs_ieffect.control_source2 = val;
			recompute_insertion_effect();
			break;
		case 0x43:
			gs_ieffect.control_depth2 = val;
			recompute_insertion_effect();
			break;
		case 0x44:
			gs_ieffect.send_eq_switch = val;
			recompute_insertion_effect();
			break;
		case 0x45:	/* Rx. Channel */
			if(val == 0x10) {
				remove_channel_layer(ch);
				init_channel_layer(ch);
			} else {
				add_channel_layer(val, ch);
			}
			break;
		case 0x46:	/* Channel Msg Rx Port */
			add_channel_layer(ch, val);
			break;
			
			/* MOD PITCH CONTROL */
			/* 0x45~ MOD, CAF, PAF */
		default:
			break;
		}
	} else if(ev == ME_SYSEX_XG_LSB) {
		switch(b)
		{
		case 0x00:	/* Reverb Return */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Reverb Return (%d)",val);
			reverb_status.level = val;
			recompute_reverb_status();
			init_reverb(play_mode->rate);
			break;
		case 0x01:	/* Chorus Return */
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Chorus Return (%d)",val);
			chorus_param.chorus_level = val;
			recompute_chorus_status();
			break;
		case 0x65:	/* Rcv CHANNEL */
			if(val == 0x7F) {
			remove_channel_layer(ch);
			init_channel_layer(ch);
		    } else {
			add_channel_layer(val, ch);
		    }
			break;
		default:
			break;
		}
	}
}

static void play_midi_prescan(MidiEvent *ev)
{
    int i;
    
    if(opt_amp_compensation) {mainvolume_max = 0;}
    else {mainvolume_max = 0x7f;}
    compensation_ratio = 1.0;

    prescanning_flag = 1;
    change_system_mode(DEFAULT_SYSTEM_MODE);
    reset_midi(0);
    resamp_cache_reset();

    while(ev->type != ME_EOT)
    {
	int ch, j, orig_ch, layered;

#ifndef SUPPRESS_CHANNEL_LAYER
	orig_ch = ev->channel;
	layered = !IS_SYSEX_EVENT_TYPE(ev->type);
	for(j = 0; (!layered && j < 1) ||
		(layered && channel[orig_ch].channel_layer[j] != -1); j++)
	{
	if(layered) {
		ev->channel = channel[orig_ch].channel_layer[j];
	}
#endif
	ch = ev->channel;

	switch(ev->type)
	{
	  case ME_NOTEON:
	    if((channel[ch].portamento_time_msb |
		channel[ch].portamento_time_lsb) == 0 ||
	       channel[ch].portamento == 0)
	    {
		int nv;
		int vlist[32];
		Voice *vp;

		nv = find_samples(ev, vlist);
		for(i = 0; i < nv; i++)
		{
		    vp = voice + vlist[i];
		    start_note(ev, vlist[i], 0, nv - i - 1);
		    resamp_cache_refer_on(vp, ev->time);
		    vp->status = VOICE_FREE;
		    vp->temper_instant = 0;
		}
	    }
	    break;

	  case ME_NOTEOFF:
	    resamp_cache_refer_off(ch, MIDI_EVENT_NOTE(ev), ev->time);
	    break;

	  case ME_PORTAMENTO_TIME_MSB:
	    channel[ch].portamento_time_msb = ev->a;
	    break;

	  case ME_PORTAMENTO_TIME_LSB:
	    channel[ch].portamento_time_lsb = ev->a;
	    break;

	  case ME_PORTAMENTO:
	    channel[ch].portamento = (ev->a >= 64);

	  case ME_RESET_CONTROLLERS:
	    reset_controllers(ch);
	    resamp_cache_refer_alloff(ch, ev->time);
	    break;

	  case ME_PROGRAM:
	    midi_program_change(ch, ev->a);
	    break;

	  case ME_TONE_BANK_MSB:
	    channel[ch].bank_msb = ev->a;
	    break;

	  case ME_TONE_BANK_LSB:
	    channel[ch].bank_lsb = ev->a;
	    break;

	  case ME_RESET:
	    change_system_mode(ev->a);
	    reset_midi(0);
	    break;

	  case ME_PITCHWHEEL:
	  case ME_ALL_NOTES_OFF:
	  case ME_ALL_SOUNDS_OFF:
	  case ME_MONO:
	  case ME_POLY:
	    resamp_cache_refer_alloff(ch, ev->time);
	    break;

	  case ME_DRUMPART:
	    if(midi_drumpart_change(ch, ev->a))
		midi_program_change(ch, channel[ch].program);
	    break;

	  case ME_KEYSHIFT:
	    resamp_cache_refer_alloff(ch, ev->time);
	    channel[ch].key_shift = (int)ev->a - 0x40;
	    break;

	  case ME_SCALE_TUNING:
		resamp_cache_refer_alloff(ch, ev->time);
		channel[ch].scale_tuning[ev->a] = ev->b;
		break;

	  case ME_MAINVOLUME:
	    if (ev->a > mainvolume_max) {
	      mainvolume_max = ev->a;
	      ctl->cmsg(CMSG_INFO,VERB_DEBUG,"ME_MAINVOLUME/max (CH:%d VAL:%#x)",ev->channel,ev->a);
	    }
	    break;
	}
#ifndef SUPPRESS_CHANNEL_LAYER
	}
	ev->channel = orig_ch;
#endif
	ev++;
    }

    /* calculate compensation ratio */
    if (0 < mainvolume_max && mainvolume_max < 0x7f) {
      compensation_ratio = pow((double)0x7f/(double)mainvolume_max, 4);
      ctl->cmsg(CMSG_INFO,VERB_DEBUG,"Compensation ratio:%lf",compensation_ratio);
    }

    for(i = 0; i < MAX_CHANNELS; i++)
	resamp_cache_refer_alloff(i, ev->time);
    resamp_cache_create();
    prescanning_flag = 0;
}

static int32 midi_cnv_vib_rate(int rate)
{
    return (int32)((double)play_mode->rate * MODULATION_WHEEL_RATE
		   / (midi_time_table[rate] *
		      2.0 * VIBRATO_SAMPLE_INCREMENTS));
}

static int midi_cnv_vib_depth(int depth)
{
    return (int)(depth * VIBRATO_DEPTH_TUNING);
}

static int32 midi_cnv_vib_delay(int delay)
{
    return (int32)(midi_time_table[delay]);
}

static int last_rpn_addr(int ch)
{
	int lsb, msb, addr, i;
	struct rpn_tag_map_t *addrmap;
	struct rpn_tag_map_t {
		int addr, mask, tag;
	};
	static struct rpn_tag_map_t nrpn_addr_map[] = {
		{0x0108, 0xffff, NRPN_ADDR_0108},
		{0x0109, 0xffff, NRPN_ADDR_0109},
		{0x010a, 0xffff, NRPN_ADDR_010A},
		{0x0120, 0xffff, NRPN_ADDR_0120},
		{0x0121, 0xffff, NRPN_ADDR_0121},
		{0x0163, 0xffff, NRPN_ADDR_0163},
		{0x0164, 0xffff, NRPN_ADDR_0164},
		{0x0166, 0xffff, NRPN_ADDR_0166},
		{0x1400, 0xff00, NRPN_ADDR_1400},
		{0x1500, 0xff00, NRPN_ADDR_1500},
		{0x1600, 0xff00, NRPN_ADDR_1600},
		{0x1700, 0xff00, NRPN_ADDR_1700},
		{0x1800, 0xff00, NRPN_ADDR_1800},
		{0x1900, 0xff00, NRPN_ADDR_1900},
		{0x1a00, 0xff00, NRPN_ADDR_1A00},
		{0x1c00, 0xff00, NRPN_ADDR_1C00},
		{0x1d00, 0xff00, NRPN_ADDR_1D00},
		{0x1e00, 0xff00, NRPN_ADDR_1E00},
		{0x1f00, 0xff00, NRPN_ADDR_1F00},
		{-1, -1, 0}
	};
	static struct rpn_tag_map_t rpn_addr_map[] = {
		{0x0000, 0xffff, RPN_ADDR_0000},
		{0x0001, 0xffff, RPN_ADDR_0001},
		{0x0002, 0xffff, RPN_ADDR_0002},
		{0x0003, 0xffff, RPN_ADDR_0003},
		{0x0004, 0xffff, RPN_ADDR_0004},
		{0x7f7f, 0xffff, RPN_ADDR_7F7F},
		{0xffff, 0xffff, RPN_ADDR_FFFF},
		{-1, -1}
	};
	
	if (channel[ch].nrpn == -1)
		return -1;
	lsb = channel[ch].lastlrpn;
	msb = channel[ch].lastmrpn;
	if (lsb == 0xff || msb == 0xff)
		return -1;
	addr = (msb << 8 | lsb);
	if (channel[ch].nrpn)
		addrmap = nrpn_addr_map;
	else
		addrmap = rpn_addr_map;
	for (i = 0; addrmap[i].addr != -1; i++)
		if (addrmap[i].addr == (addr & addrmap[i].mask))
			return addrmap[i].tag;
	return -1;
}

static void update_channel_freq(int ch)
{
	int i, uv = upper_voices;
	for (i = 0; i < uv; i++)
		if (voice[i].status != VOICE_FREE && voice[i].channel == ch)
	recompute_freq(i);
}

static void update_rpn_map(int ch, int addr, int update_now)
{
	int val, drumflag, i, note;
	
	val = channel[ch].rpnmap[addr];
	drumflag = 0;
	switch (addr) {
	case NRPN_ADDR_0108:	/* Vibrato Rate */
		if (opt_nrpn_vibrato) {
			/* from -10.72 Hz to +10.72 Hz. */
			ctl->cmsg(CMSG_INFO, VERB_NOISY,
					"Vibrato Rate (CH:%d VAL:%d)", ch, val - 64);
			channel[ch].vibrato_ratio = 168 * (val - 64)
					* (VIBRATO_RATE_TUNING * play_mode->rate)
					/ (2 * VIBRATO_SAMPLE_INCREMENTS);
		}
		if (update_now)
			update_channel_freq(ch);
		break;
	case NRPN_ADDR_0109:	/* Vibrato Depth */
		if (opt_nrpn_vibrato) {
			/* from -10cents to +10cents. */
			ctl->cmsg(CMSG_INFO, VERB_NOISY,
					"Vibrato Depth (CH:%d VAL:%d)", ch, val - 64);
			channel[ch].vibrato_depth =
					(double) (val - 64) * 0.15625 * 256 / 400;
		}
		if (update_now)
			update_channel_freq(ch);
		break;
	case NRPN_ADDR_010A:	/* Vibrato Delay */
		if (opt_nrpn_vibrato) {
			ctl->cmsg(CMSG_INFO, VERB_NOISY,
					"Vibrato Delay (CH:%d VAL:%d)", ch, val);
			channel[ch].vibrato_delay =
					play_mode->rate * delay_time_center_table[val] * 0.001;
		}
		if (update_now)
			update_channel_freq(ch);
		break;
	case NRPN_ADDR_0120:	/* Filter Cutoff Frequency */
		if (opt_lpf_def) {
			ctl->cmsg(CMSG_INFO, VERB_NOISY,
					"Filter Cutoff (CH:%d VAL:%d)", ch, val - 64);
			channel[ch].param_cutoff_freq = val - 64;
		}
		break;
	case NRPN_ADDR_0121:	/* Filter Resonance */
		if (opt_lpf_def) {
			ctl->cmsg(CMSG_INFO, VERB_NOISY,
					"Filter Resonance (CH:%d VAL:%d)", ch, val - 64);
			channel[ch].param_resonance = val - 64;
		}
		break;
	case NRPN_ADDR_0163:	/* Attack Time */
		if (opt_tva_attack) {
			ctl->cmsg(CMSG_INFO, VERB_NOISY,
					"Attack Time (CH:%d VAL:%d)", ch, val);
			set_envelope_time(ch, val, 0);
		}
		break;
	case NRPN_ADDR_0164:	/* EG Decay Time */
		if (opt_tva_decay) {
			ctl->cmsg(CMSG_INFO, VERB_NOISY,
					"EG Decay Time (CH:%d VAL:%d)", ch, val);
			set_envelope_time(ch, val, 2);
		}
		break;
	case NRPN_ADDR_0166:	/* EG Release Time */
		if (opt_tva_release) {
			ctl->cmsg(CMSG_INFO, VERB_NOISY,
					"EG Release Time (CH:%d VAL:%d)", ch, val);
			set_envelope_time(ch, val, 3);
		}
		break;
	case NRPN_ADDR_1400:	/* Drum Filter Cutoff (XG) */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);
		ctl->cmsg(CMSG_INFO, VERB_NOISY,
				"Drum Instrument Filter Cutoff (CH:%d NOTE:%d VALUE:%d)",
				ch, note, val);
		channel[ch].drums[note]->drum_cutoff_freq = val - 64;
		break;
	case NRPN_ADDR_1500:	/* Drum Filter Resonance (XG) */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);
		ctl->cmsg(CMSG_INFO, VERB_NOISY,
				"Drum Instrument Filter Resonance (CH:%d NOTE:%d VALUE:%d)",
				ch, note, val);
		channel[ch].drums[note]->drum_resonance = val - 64;
		break;
	case NRPN_ADDR_1600:	/* Drum EG Attack Time (XG) */
		drumflag = 1;
		if (opt_tva_attack) {
			val = val & 0x7f;
			note = channel[ch].lastlrpn;
			if (channel[ch].drums[note] == NULL)
				play_midi_setup_drums(ch, note);
			val	-= 64;
			ctl->cmsg(CMSG_INFO, VERB_NOISY,
					"XG Drum Attack Time (CH:%d NOTE:%d VALUE:%d)",
					ch, note, val);
			channel[ch].drums[note]->drum_envelope_rate[0] = val;
		}
		break;
	case NRPN_ADDR_1700:	/* Drum EG Decay Time (XG) */
		drumflag = 1;
		if (opt_tva_decay) {
			val = val & 0x7f;
			note = channel[ch].lastlrpn;
			if (channel[ch].drums[note] == NULL)
				play_midi_setup_drums(ch, note);
			val	-= 64;
			ctl->cmsg(CMSG_INFO, VERB_NOISY,
					"XG Drum Decay Time (CH:%d NOTE:%d VALUE:%d)",
					ch, note, val);
			channel[ch].drums[note]->drum_envelope_rate[2] = val;
		}
		break;
	case NRPN_ADDR_1800:	/* Coarse Pitch of Drum (GS) */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);
		if (current_event->b == 0x01) {
			channel[ch].drums[note]->play_note = val;
			ctl->cmsg(CMSG_INFO, VERB_NOISY,
			"Drum Instrument Play Note (CH:%d NOTE:%d VALUE:%d)",
			ch, note, channel[ch].drums[note]->play_note);
		} else {
			channel[ch].drums[note]->coarse = val - 64;
			ctl->cmsg(CMSG_INFO, VERB_NOISY,
			"Drum Instrument Pitch Coarse (CH:%d NOTE:%d VALUE:%d)",
			ch, note, channel[ch].drums[note]->coarse);
		}
		channel[ch].pitchfactor = 0;
		break;
	case NRPN_ADDR_1900:	/* Fine Pitch of Drum (XG) */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);
		channel[ch].drums[note]->fine = val - 64;
		ctl->cmsg(CMSG_INFO, VERB_NOISY,
				"Drum Instrument Pitch Fine (CH:%d NOTE:%d VALUE:%d)",
				ch, note, channel[ch].drums[note]->fine);
		channel[ch].pitchfactor = 0;
		break;
	case NRPN_ADDR_1A00:	/* Level of Drum */	 
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);
		ctl->cmsg(CMSG_INFO, VERB_NOISY,
				"Drum Instrument TVA Level (CH:%d NOTE:%d VALUE:%d)",
				ch, note, val);
		channel[ch].drums[note]->drum_level =
				calc_drum_tva_level(ch, note, val);
		break;
	case NRPN_ADDR_1C00:	/* Panpot of Drum */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);
		if(val == 0) {
			val = int_rand(128);
			channel[ch].drums[note]->pan_random = 1;
		} else
			channel[ch].drums[note]->pan_random = 0;
		channel[ch].drums[note]->drum_panning = val;
		if (update_now && adjust_panning_immediately
				&& ! channel[ch].pan_random)
			adjust_drum_panning(ch, note);
		break;
	case NRPN_ADDR_1D00:	/* Reverb Send Level of Drum */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);
		ctl->cmsg(CMSG_INFO, VERB_NOISY,
				"Reverb Send Level of Drum (CH:%d NOTE:%d VALUE:%d)",
				ch, note, val);
		channel[ch].drums[note]->reverb_level = val;
		break;
	case NRPN_ADDR_1E00:	/* Chorus Send Level of Drum */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);
		ctl->cmsg(CMSG_INFO, VERB_NOISY,
				"Chorus Send Level of Drum (CH:%d NOTE:%d VALUE:%d)",
				ch, note, val);
		channel[ch].drums[note]->chorus_level = val;
		break;
	case NRPN_ADDR_1F00:	/* Variation Send Level of Drum */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);
		ctl->cmsg(CMSG_INFO, VERB_NOISY,
				"Delay Send Level of Drum (CH:%d NOTE:%d VALUE:%d)",
				ch, note, val);
		channel[ch].drums[note]->delay_level = val;
		break;
	case RPN_ADDR_0000:		/* Pitch bend sensitivity */
		ctl->cmsg(CMSG_INFO, VERB_DEBUG,
				"Pitch Bend Sensitivity (CH:%d VALUE:%d)", ch, val);
		/* for mod2mid.c, arpeggio */
		if (! IS_CURRENT_MOD_FILE && channel[ch].rpnmap[RPN_ADDR_0000] > 24)
			channel[ch].rpnmap[RPN_ADDR_0000] = 24;
		channel[ch].pitchfactor = 0;
		break;
	case RPN_ADDR_0001:		/* Master Fine Tuning */
		ctl->cmsg(CMSG_INFO, VERB_DEBUG,
				"Master Fine Tuning (CH:%d VALUE:%d)", ch, val);
		channel[ch].pitchfactor = 0;
		break;
	case RPN_ADDR_0002:		/* Master Coarse Tuning */
		ctl->cmsg(CMSG_INFO, VERB_DEBUG,
				"Master Coarse Tuning (CH:%d VALUE:%d)", ch, val);
		channel[ch].pitchfactor = 0;
		break;
	case RPN_ADDR_0003:		/* Tuning Program Select */
		ctl->cmsg(CMSG_INFO, VERB_DEBUG,
				"Tuning Program Select (CH:%d VALUE:%d)", ch, val);
		for (i = 0; i < upper_voices; i++)
			if (voice[i].status != VOICE_FREE) {
				voice[i].temper_instant = 1;
				recompute_freq(i);
			}
		break;
	case RPN_ADDR_0004:		/* Tuning Bank Select */
		ctl->cmsg(CMSG_INFO, VERB_DEBUG,
				"Tuning Bank Select (CH:%d VALUE:%d)", ch, val);
		for (i = 0; i < upper_voices; i++)
			if (voice[i].status != VOICE_FREE) {
				voice[i].temper_instant = 1;
				recompute_freq(i);
			}
		break;
	case RPN_ADDR_7F7F:		/* RPN reset */
		channel[ch].rpn_7f7f_flag = 1;
		break;
	case RPN_ADDR_FFFF:		/* RPN initialize */
		/* All reset to defaults */
		channel[ch].rpn_7f7f_flag = 0;
		memset(channel[ch].rpnmap, 0, sizeof(channel[ch].rpnmap));
		channel[ch].lastlrpn = channel[ch].lastmrpn = 0;
		channel[ch].nrpn = 0;
		channel[ch].rpnmap[RPN_ADDR_0000] = 2;
		channel[ch].rpnmap[RPN_ADDR_0001] = 0x40;
		channel[ch].rpnmap[RPN_ADDR_0002] = 0x40;
		channel[ch].pitchfactor = 0;
		break;
	}
	drumflag = 0;
	if (drumflag && midi_drumpart_change(ch, 1)) {
		midi_program_change(ch, channel[ch].program);
		if (update_now)
			ctl_prog_event(ch);
	}
}

static void seek_forward(int32 until_time)
{
    int32 i;

    playmidi_seek_flag = 1;
    wrd_midi_event(WRD_START_SKIP, WRD_NOARG);
    while(MIDI_EVENT_TIME(current_event) < until_time)
    {
	int ch, j = 0, orig_ch, layered;

#ifndef SUPPRESS_CHANNEL_LAYER
	orig_ch = current_event->channel;
	layered = !IS_SYSEX_EVENT_TYPE(current_event->type);
	for(j = 0; (!layered && j < 1) ||
		(layered && channel[orig_ch].channel_layer[j] != -1); j++)
	{
	if(layered) {
		current_event->channel = channel[orig_ch].channel_layer[j];
	}
#endif
	ch = current_event->channel;
	
	switch(current_event->type)
	{
	  case ME_PITCHWHEEL:
	    channel[ch].pitchbend = current_event->a + current_event->b * 128;
	    channel[ch].pitchfactor=0;
	    break;

	  case ME_MAINVOLUME:
	    channel[ch].volume = current_event->a;
	    break;

	  case ME_MASTER_VOLUME:
	    master_volume_ratio =
		(int32)current_event->a + 256 * (int32)current_event->b;
	    break;

	  case ME_PAN:
	    channel[ch].panning = current_event->a;
	    channel[ch].pan_random = 0;
	    break;

	  case ME_EXPRESSION:
	    channel[ch].expression=current_event->a;
	    break;

	  case ME_PROGRAM:
	    midi_program_change(ch, current_event->a);
	    break;

	  case ME_SUSTAIN:
	    channel[ch].sustain = (current_event->a >= 64);
	    break;

	  case ME_SOSTENUTO:
	    break;

	  case ME_LEGATO_FOOTSWITCH:
        channel[ch].legato = (current_event->a >= 64);
	    break;

      case ME_HOLD2:
        break;

	  case ME_FOOT:
	    break;

	  case ME_BREATH:
	    break;

	  case ME_BALANCE:
	    break;

	  case ME_RESET_CONTROLLERS:
	    reset_controllers(ch);
	    break;

	  case ME_TONE_BANK_MSB:
	    channel[ch].bank_msb = current_event->a;
	    break;

	  case ME_TONE_BANK_LSB:
	    channel[ch].bank_lsb = current_event->a;
	    break;

	  case ME_MODULATION_WHEEL:
	    channel[ch].modulation_wheel =
		midi_cnv_vib_depth(current_event->a);
	    break;

	  case ME_PORTAMENTO_TIME_MSB:
	    channel[ch].portamento_time_msb = current_event->a;
	    break;

	  case ME_PORTAMENTO_TIME_LSB:
	    channel[ch].portamento_time_lsb = current_event->a;
	    break;

	  case ME_PORTAMENTO:
	    channel[ch].portamento = (current_event->a >= 64);
	    break;

	  case ME_MONO:
	    channel[ch].mono = 1;
	    break;

	  case ME_POLY:
	    channel[ch].mono = 0;
	    break;

	  case ME_SOFT_PEDAL:
		  if(opt_lpf_def) {
			  channel[ch].soft_pedal = current_event->a;
			  ctl->cmsg(CMSG_INFO,VERB_NOISY,"Soft Pedal (CH:%d VAL:%d)",ch,channel[ch].soft_pedal);
		  }
		  break;

	  case ME_HARMONIC_CONTENT:
		  if(opt_lpf_def) {
			  channel[ch].param_resonance = current_event->a - 64;
			  ctl->cmsg(CMSG_INFO,VERB_NOISY,"Harmonic Content (CH:%d VAL:%d)",ch,channel[ch].param_resonance);
		  }
		  break;

	  case ME_BRIGHTNESS:
		  if(opt_lpf_def) {
			  channel[ch].param_cutoff_freq = current_event->a - 64;
			  ctl->cmsg(CMSG_INFO,VERB_NOISY,"Brightness (CH:%d VAL:%d)",ch,channel[ch].param_cutoff_freq);
		  }
		  break;

	    /* RPNs */
	  case ME_NRPN_LSB:
	    channel[ch].lastlrpn = current_event->a;
	    channel[ch].nrpn = 1;
	    break;
	  case ME_NRPN_MSB:
	    channel[ch].lastmrpn = current_event->a;
	    channel[ch].nrpn = 1;
	    break;
	  case ME_RPN_LSB:
	    channel[ch].lastlrpn = current_event->a;
	    channel[ch].nrpn = 0;
	    break;
	  case ME_RPN_MSB:
	    channel[ch].lastmrpn = current_event->a;
	    channel[ch].nrpn = 0;
	    break;
	  case ME_RPN_INC:
	    if(channel[ch].rpn_7f7f_flag) /* disable */
		break;
	    if((i = last_rpn_addr(ch)) >= 0)
	    {
		if(channel[ch].rpnmap[i] < 127)
		    channel[ch].rpnmap[i]++;
		update_rpn_map(ch, i, 0);
	    }
	    break;
	case ME_RPN_DEC:
	    if(channel[ch].rpn_7f7f_flag) /* disable */
		break;
	    if((i = last_rpn_addr(ch)) >= 0)
	    {
		if(channel[ch].rpnmap[i] > 0)
		    channel[ch].rpnmap[i]--;
		update_rpn_map(ch, i, 0);
	    }
	    break;
	  case ME_DATA_ENTRY_MSB:
	    if(channel[ch].rpn_7f7f_flag) /* disable */
		break;
	    if((i = last_rpn_addr(ch)) >= 0)
	    {
		channel[ch].rpnmap[i] = current_event->a;
		update_rpn_map(ch, i, 0);
	    }
	    break;
	  case ME_DATA_ENTRY_LSB:
	    if(channel[ch].rpn_7f7f_flag) /* disable */
		break;
	    /* Ignore */
	    channel[ch].nrpn = -1;
	    break;

	  case ME_REVERB_EFFECT:
	    set_reverb_level(ch, current_event->a);
	    break;

	  case ME_CHORUS_EFFECT:
	    if(opt_chorus_control == 1)
		channel[ch].chorus_level = current_event->a;
	    else
		channel[ch].chorus_level = -opt_chorus_control;

		if(current_event->a) {
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Chorus Send (CH:%d LEVEL:%d)",ch,current_event->a);
		}
		break;

	  case ME_TREMOLO_EFFECT:
		ctl->cmsg(CMSG_INFO,VERB_NOISY,"Tremolo Send (CH:%d LEVEL:%d)",ch,current_event->a);
		break;

	  case ME_CELESTE_EFFECT:
		if(opt_delay_control) {
			channel[ch].delay_level = current_event->a;
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Delay Send (CH:%d LEVEL:%d)",ch,current_event->a);
		}
	    break;

	  case ME_ATTACK_TIME:
	  	if(!opt_tva_attack) { break; }
		set_envelope_time(ch,current_event->a,0);
		break;

	  case ME_RELEASE_TIME:
	  	if(!opt_tva_release) { break; }
		set_envelope_time(ch,current_event->a,3);
		break;

	  case ME_PHASER_EFFECT:
		ctl->cmsg(CMSG_INFO,VERB_NOISY,"Phaser Send (CH:%d LEVEL:%d)",ch,current_event->a);
		break;

	  case ME_RANDOM_PAN:
	    channel[ch].panning = int_rand(128);
	    channel[ch].pan_random = 1;
	    break;

	  case ME_SET_PATCH:
	    i = channel[ch].special_sample = current_event->a;
	    if(special_patch[i] != NULL)
		special_patch[i]->sample_offset = 0;
	    break;

	  case ME_TEMPO:
	    current_play_tempo = ch +
		current_event->b * 256 + current_event->a * 65536;
	    break;

	  case ME_RESET:
	    change_system_mode(current_event->a);
	    reset_midi(0);
	    break;

	  case ME_PATCH_OFFS:
	    i = channel[ch].special_sample;
	    if(special_patch[i] != NULL)
		special_patch[i]->sample_offset =
		    (current_event->a | 256 * current_event->b);
	    break;

	  case ME_WRD:
	    wrd_midi_event(ch, current_event->a | 256 * current_event->b);
	    break;

	  case ME_SHERRY:
	    wrd_sherry_event(ch |
			     (current_event->a<<8) |
			     (current_event->b<<16));
	    break;

	  case ME_DRUMPART:
	    if(midi_drumpart_change(ch, current_event->a))
		midi_program_change(ch, channel[ch].program);
	    break;

	  case ME_KEYSHIFT:
	    channel[ch].key_shift = (int)current_event->a - 0x40;
	    break;

	case ME_KEYSIG:
		current_keysig = current_event->a + current_event->b * 16;
		current_temper_keysig = current_keysig;
		break;

	case ME_SCALE_TUNING:
		channel[ch].scale_tuning[current_event->a] = current_event->b;
		break;

	case ME_BULK_TUNING_DUMP:
		set_single_note_tuning(ch, current_event->a, current_event->b, 0);
		break;

	case ME_SINGLE_NOTE_TUNING:
		set_single_note_tuning(ch, current_event->a, current_event->b, 0);
		break;

	case ME_TEMPER_KEYSIG:
		current_temper_keysig = current_event->a;
		break;

	case ME_TEMPER_TYPE:
		channel[ch].temper_type = current_event->a;
		break;

	case ME_MASTER_TEMPER_TYPE:
		for (i = 0; i < MAX_CHANNELS; i++)
			channel[i].temper_type = current_event->a;
		break;

	  case ME_SYSEX_LSB:
	    process_sysex_event(ME_SYSEX_LSB,ch,current_event->a,current_event->b);
	    break;

	  case ME_SYSEX_MSB:
	    process_sysex_event(ME_SYSEX_MSB,ch,current_event->a,current_event->b);
	    break;

	  case ME_SYSEX_GS_LSB:
	    process_sysex_event(ME_SYSEX_GS_LSB,ch,current_event->a,current_event->b);
	    break;

	  case ME_SYSEX_GS_MSB:
	    process_sysex_event(ME_SYSEX_GS_MSB,ch,current_event->a,current_event->b);
	    break;

	  case ME_SYSEX_XG_LSB:
	    process_sysex_event(ME_SYSEX_XG_LSB,ch,current_event->a,current_event->b);
	    break;

	  case ME_SYSEX_XG_MSB:
	    process_sysex_event(ME_SYSEX_XG_MSB,ch,current_event->a,current_event->b);
	    break;

	  case ME_EOT:
	    current_sample = current_event->time;
	    playmidi_seek_flag = 0;
	    return;
	}
#ifndef SUPPRESS_CHANNEL_LAYER
	}
	current_event->channel = orig_ch;
#endif
	current_event++;
    }
    wrd_midi_event(WRD_END_SKIP, WRD_NOARG);

    playmidi_seek_flag = 0;
    if(current_event != event_list)
	current_event--;
    current_sample = until_time;
}

static void skip_to(int32 until_time)
{
  int ch;

  trace_flush();
  current_event = NULL;

  if (current_sample > until_time)
    current_sample=0;

  change_system_mode(DEFAULT_SYSTEM_MODE);
  reset_midi(0);

  buffered_count=0;
  buffer_pointer=common_buffer;
  current_event=event_list;
  current_play_tempo = 500000; /* 120 BPM */

  if (until_time)
    seek_forward(until_time);
  for(ch = 0; ch < MAX_CHANNELS; ch++)
      channel[ch].lasttime = current_sample;

  ctl_mode_event(CTLE_RESET, 0, 0, 0);
  trace_offset(until_time);

#ifdef SUPPORT_SOUNDSPEC
  soundspec_update_wave(NULL, 0);
#endif /* SUPPORT_SOUNDSPEC */
}

static int32 sync_restart(int only_trace_ok)
{
    int32 cur;

    cur = current_trace_samples();
    if(cur == -1)
    {
	if(only_trace_ok)
	    return -1;
	cur = current_sample;
    }
    aq_flush(1);
    skip_to(cur);
    return cur;
}

static int playmidi_change_rate(int32 rate, int restart)
{
    int arg;

    if(rate == play_mode->rate)
	return 1; /* Not need to change */

    if(rate < MIN_OUTPUT_RATE || rate > MAX_OUTPUT_RATE)
    {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		  "Out of sample rate: %d", rate);
	return -1;
    }

    if(restart)
    {
	if((midi_restart_time = current_trace_samples()) == -1)
	    midi_restart_time = current_sample;
    }
    else
	midi_restart_time = 0;

    arg = (int)rate;
    if(play_mode->acntl(PM_REQ_RATE, &arg) == -1)
    {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		  "Can't change sample rate to %d", rate);
	return -1;
    }

    aq_flush(1);
    aq_setup();
    aq_set_soft_queue(-1.0, -1.0);
    free_instruments(1);
#ifdef SUPPORT_SOUNDSPEC
    soundspec_reinit();
#endif /* SUPPORT_SOUNDSPEC */
    return 0;
}

void playmidi_output_changed(int play_state)
{
    if(target_play_mode == NULL)
	return;
    play_mode = target_play_mode;

    if(play_state == 0)
    {
	/* Playing */
	if((midi_restart_time = current_trace_samples()) == -1)
	    midi_restart_time = current_sample;
    }
    else /* Not playing */
	midi_restart_time = 0;

    if(play_state != 2)
    {
	aq_flush(1);
	aq_setup();
	aq_set_soft_queue(-1.0, -1.0);
	clear_magic_instruments();
    }
    free_instruments(1);
#ifdef SUPPORT_SOUNDSPEC
    soundspec_reinit();
#endif /* SUPPORT_SOUNDSPEC */
    target_play_mode = NULL;
}

int check_apply_control(void)
{
    int rc;
    int32 val;

    if(file_from_stdin)
	return RC_NONE;
    rc = ctl->read(&val);
    switch(rc)
    {
      case RC_CHANGE_VOLUME:
	if (val>0 || amplification > -val)
	    amplification += val;
	else
	    amplification=0;
	if (amplification > MAX_AMPLIFICATION)
	    amplification=MAX_AMPLIFICATION;
	adjust_amplification();
	ctl_mode_event(CTLE_MASTER_VOLUME, 0, amplification, 0);
	break;
      case RC_SYNC_RESTART:
	aq_flush(1);
	break;
      case RC_TOGGLE_PAUSE:
	play_pause_flag = !play_pause_flag;
	ctl_pause_event(play_pause_flag, 0);
	return RC_NONE;
      case RC_TOGGLE_SNDSPEC:
#ifdef SUPPORT_SOUNDSPEC
	if(view_soundspec_flag)
	    close_soundspec();
	else
	    open_soundspec();
	if(view_soundspec_flag || ctl_speana_flag)
	    soundspec_update_wave(NULL, -1);
	return RC_NONE;
      case RC_TOGGLE_CTL_SPEANA:
	ctl_speana_flag = !ctl_speana_flag;
	if(view_soundspec_flag || ctl_speana_flag)
	    soundspec_update_wave(NULL, -1);
	return RC_NONE;
#endif /* SUPPORT_SOUNDSPEC */
      case RC_CHANGE_RATE:
	if(playmidi_change_rate(val, 0))
	    return RC_NONE;
	return RC_RELOAD;
      case RC_OUTPUT_CHANGED:
	playmidi_output_changed(1);
	return RC_RELOAD;
    }
    return rc;
}

static void voice_increment(int n)
{
    int i;
    for(i = 0; i < n; i++)
    {
	if(voices == MAX_VOICES)
	    break;
	voice[voices].status = VOICE_FREE;
	voice[voices].temper_instant = 0;
	voice[voices].chorus_link = voices;
	voices++;
    }
    if(n > 0)
	ctl_mode_event(CTLE_MAXVOICES, 1, voices, 0);
}

static void voice_decrement(int n)
{
    int i, j, lowest;
    int32 lv, v;

    /* decrease voice */
    for(i = 0; i < n && voices > 0; i++)
    {
	voices--;
	if(voice[voices].status == VOICE_FREE)
	    continue;	/* found */

	for(j = 0; j < voices; j++)
	    if(voice[j].status == VOICE_FREE)
		break;
	if(j != voices)
	{
	    voice[j] = voice[voices];
	    continue;	/* found */
	}

	/* Look for the decaying note with the lowest volume */
	lv = 0x7FFFFFFF;
	lowest = -1;
	for(j = 0; j <= voices; j++)
	{
	    if(voice[j].status & ~(VOICE_ON | VOICE_DIE))
	    {
		v = voice[j].left_mix;
		if((voice[j].panned==PANNED_MYSTERY) &&
		   (voice[j].right_mix > v))
		    v = voice[j].right_mix;
		if(v < lv)
		{
		    lv = v;
		    lowest = j;
		}
	    }
	}

	if(lowest != -1)
	{
	    cut_notes++;
	    free_voice(lowest);
	    ctl_note_event(lowest);
	    voice[lowest] = voice[voices];
	}
	else
	    lost_notes++;
    }
    if(upper_voices > voices)
	upper_voices = voices;
    if(n > 0)
	ctl_mode_event(CTLE_MAXVOICES, 1, voices, 0);
}

/* EAW -- do not throw away good notes, stop decrementing */
static void voice_decrement_conservative(int n)
{
    int i, j, lowest, finalnv;
    int32 lv, v;

    /* decrease voice */
    finalnv = voices - n;
    for(i = 1; i <= n && voices > 0; i++)
    {
	if(voice[voices-1].status == VOICE_FREE) {
	    voices--;
	    continue;	/* found */
	}

	for(j = 0; j < finalnv; j++)
	    if(voice[j].status == VOICE_FREE)
		break;
	if(j != finalnv)
	{
	    voice[j] = voice[voices-1];
	    voices--;
	    continue;	/* found */
	}

	/* Look for the decaying note with the lowest volume */
	lv = 0x7FFFFFFF;
	lowest = -1;
	for(j = 0; j < voices; j++)
	{
	    if(voice[j].status & ~(VOICE_ON | VOICE_DIE) &&
	       !(voice[j].sample->note_to_use &&
	         ISDRUMCHANNEL(voice[j].channel)))
	    {
		v = voice[j].left_mix;
		if((voice[j].panned==PANNED_MYSTERY) &&
		   (voice[j].right_mix > v))
		    v = voice[j].right_mix;
		if(v < lv)
		{
		    lv = v;
		    lowest = j;
		}
	    }
	}

	if(lowest != -1)
	{
	    voices--;
	    cut_notes++;
	    free_voice(lowest);
	    ctl_note_event(lowest);
	    voice[lowest] = voice[voices];
	}
	else break;
    }
    if(upper_voices > voices)
	upper_voices = voices;
}

void restore_voices(int save_voices)
{
#ifdef REDUCE_VOICE_TIME_TUNING
    static int old_voices = -1;
    if(old_voices == -1 || save_voices)
	old_voices = voices;
    else if (voices < old_voices)
	voice_increment(old_voices - voices);
    else
	voice_decrement(voices - old_voices);
#endif /* REDUCE_VOICE_TIME_TUNING */
}
	

static int apply_controls(void)
{
    int rc, i, jump_flag = 0;
    int32 val, cur;
    FLOAT_T r;
    ChannelBitMask tmp_chbitmask;

    /* ASCII renditions of CD player pictograms indicate approximate effect */
    do
    {
	switch(rc=ctl->read(&val))
	{
	  case RC_STOP:
	  case RC_QUIT:		/* [] */
	  case RC_LOAD_FILE:
	  case RC_NEXT:		/* >>| */
	  case RC_REALLY_PREVIOUS: /* |<< */
	  case RC_TUNE_END:	/* skip */
	    aq_flush(1);
	    return rc;

	  case RC_CHANGE_VOLUME:
	    if (val>0 || amplification > -val)
		amplification += val;
	    else
		amplification=0;
	    if (amplification > MAX_AMPLIFICATION)
		amplification=MAX_AMPLIFICATION;
	    adjust_amplification();
	    for (i=0; i<upper_voices; i++)
		if (voice[i].status != VOICE_FREE)
		{
		    recompute_amp(i);
		    apply_envelope_to_amp(i);
		}
	    ctl_mode_event(CTLE_MASTER_VOLUME, 0, amplification, 0);
	    continue;

	  case RC_CHANGE_REV_EFFB:
	  case RC_CHANGE_REV_TIME:
	    reverb_rc_event(rc, val);
	    sync_restart(0);
	    continue;

	  case RC_PREVIOUS:	/* |<< */
	    aq_flush(1);
	    if (current_sample < 2*play_mode->rate)
		return RC_REALLY_PREVIOUS;
	    return RC_RESTART;

	  case RC_RESTART:	/* |<< */
	    if(play_pause_flag)
	    {
		midi_restart_time = 0;
		ctl_pause_event(1, 0);
		continue;
	    }
	    aq_flush(1);
	    skip_to(0);
	    ctl_updatetime(0);
	    jump_flag = 1;
		midi_restart_time = 0;
	    continue;

	  case RC_JUMP:
	    if(play_pause_flag)
	    {
		midi_restart_time = val;
		ctl_pause_event(1, val);
		continue;
	    }
	    aq_flush(1);
	    if (val >= sample_count)
		return RC_TUNE_END;
	    skip_to(val);
	    ctl_updatetime(val);
	    return rc;

	  case RC_FORWARD:	/* >> */
	    if(play_pause_flag)
	    {
		midi_restart_time += val;
		if(midi_restart_time > sample_count)
		    midi_restart_time = sample_count;
		ctl_pause_event(1, midi_restart_time);
		continue;
	    }
	    cur = current_trace_samples();
	    aq_flush(1);
	    if(cur == -1)
		cur = current_sample;
	    if(val + cur >= sample_count)
		return RC_TUNE_END;
	    skip_to(val + cur);
	    ctl_updatetime(val + cur);
	    return RC_JUMP;

	  case RC_BACK:		/* << */
	    if(play_pause_flag)
	    {
		midi_restart_time -= val;
		if(midi_restart_time < 0)
		    midi_restart_time = 0;
		ctl_pause_event(1, midi_restart_time);
		continue;
	    }
	    cur = current_trace_samples();
	    aq_flush(1);
	    if(cur == -1)
		cur = current_sample;
	    if(cur > val)
	    {
		skip_to(cur - val);
		ctl_updatetime(cur - val);
	    }
	    else
	    {
		skip_to(0);
		ctl_updatetime(0);
		midi_restart_time = 0;
	    }
	    return RC_JUMP;

	  case RC_TOGGLE_PAUSE:
	    if(play_pause_flag)
	    {
		play_pause_flag = 0;
		skip_to(midi_restart_time);
	    }
	    else
	    {
		midi_restart_time = current_trace_samples();
		if(midi_restart_time == -1)
		    midi_restart_time = current_sample;
		aq_flush(1);
		play_pause_flag = 1;
	    }
	    ctl_pause_event(play_pause_flag, midi_restart_time);
	    jump_flag = 1;
	    continue;

	  case RC_KEYUP:
	  case RC_KEYDOWN:
	    note_key_offset += val;
	    current_freq_table += val;
	    current_freq_table -= floor(current_freq_table / 12.0) * 12;
	    if(sync_restart(1) != -1)
		jump_flag = 1;
	    ctl_mode_event(CTLE_KEY_OFFSET, 0, note_key_offset, 0);
	    continue;

	  case RC_SPEEDUP:
	    r = 1.0;
	    for(i = 0; i < val; i++)
		r *= SPEED_CHANGE_RATE;
	    sync_restart(0);
	    midi_time_ratio /= r;
	    current_sample = (int32)(current_sample / r + 0.5);
	    trace_offset(current_sample);
	    jump_flag = 1;
	    ctl_mode_event(CTLE_TIME_RATIO, 0, 100 / midi_time_ratio + 0.5, 0);
	    continue;

	  case RC_SPEEDDOWN:
	    r = 1.0;
	    for(i = 0; i < val; i++)
		r *= SPEED_CHANGE_RATE;
	    sync_restart(0);
	    midi_time_ratio *= r;
	    current_sample = (int32)(current_sample * r + 0.5);
	    trace_offset(current_sample);
	    jump_flag = 1;
	    ctl_mode_event(CTLE_TIME_RATIO, 0, 100 / midi_time_ratio + 0.5, 0);
	    continue;

	  case RC_VOICEINCR:
	    restore_voices(0);
	    voice_increment(val);
	    if(sync_restart(1) != -1)
		jump_flag = 1;
	    restore_voices(1);
	    continue;

	  case RC_VOICEDECR:
	    restore_voices(0);
	    if(sync_restart(1) != -1)
	    {
		voices -= val;
		if(voices < 0)
		    voices = 0;
		jump_flag = 1;
	    }
	    else
		voice_decrement(val);
	    restore_voices(1);
	    continue;

	  case RC_TOGGLE_DRUMCHAN:
	    midi_restart_time = current_trace_samples();
	    if(midi_restart_time == -1)
		midi_restart_time = current_sample;
	    SET_CHANNELMASK(drumchannel_mask, val);
	    SET_CHANNELMASK(current_file_info->drumchannel_mask, val);
	    if(IS_SET_CHANNELMASK(drumchannels, val))
	    {
		UNSET_CHANNELMASK(drumchannels, val);
		UNSET_CHANNELMASK(current_file_info->drumchannels, val);
	    }
	    else
	    {
		SET_CHANNELMASK(drumchannels, val);
		SET_CHANNELMASK(current_file_info->drumchannels, val);
	    }
	    aq_flush(1);
	    return RC_RELOAD;

	  case RC_TOGGLE_SNDSPEC:
#ifdef SUPPORT_SOUNDSPEC
	    if(view_soundspec_flag)
		close_soundspec();
	    else
		open_soundspec();
	    if(view_soundspec_flag || ctl_speana_flag)
	    {
		sync_restart(0);
		soundspec_update_wave(NULL, -1);
	    }
#endif /* SUPPORT_SOUNDSPEC */
	    continue;

	  case RC_TOGGLE_CTL_SPEANA:
#ifdef SUPPORT_SOUNDSPEC
	    ctl_speana_flag = !ctl_speana_flag;
	    if(view_soundspec_flag || ctl_speana_flag)
	    {
		sync_restart(0);
		soundspec_update_wave(NULL, -1);
	    }
#endif /* SUPPORT_SOUNDSPEC */
	    continue;

	  case RC_SYNC_RESTART:
	    sync_restart(val);
	    jump_flag = 1;
	    continue;

	  case RC_RELOAD:
	    midi_restart_time = current_trace_samples();
	    if(midi_restart_time == -1)
		midi_restart_time = current_sample;
	    aq_flush(1);
	    return RC_RELOAD;

	  case RC_CHANGE_RATE:
	    if(playmidi_change_rate(val, 1))
		return RC_NONE;
	    return RC_RELOAD;

	  case RC_OUTPUT_CHANGED:
	    playmidi_output_changed(0);
	    return RC_RELOAD;

	case RC_TOGGLE_MUTE:
		TOGGLE_CHANNELMASK(channel_mute, val);
		sync_restart(0);
		jump_flag = 1;
		ctl_mode_event(CTLE_MUTE, 0,
				val, (IS_SET_CHANNELMASK(channel_mute, val)) ? 1 : 0);
		continue;

	case RC_SOLO_PLAY:
		COPY_CHANNELMASK(tmp_chbitmask, channel_mute);
		FILL_CHANNELMASK(channel_mute);
		UNSET_CHANNELMASK(channel_mute, val);
		if (! COMPARE_CHANNELMASK(tmp_chbitmask, channel_mute)) {
			sync_restart(0);
			jump_flag = 1;
			for (i = 0; i < MAX_CHANNELS; i++)
				ctl_mode_event(CTLE_MUTE, 0, i, 1);
			ctl_mode_event(CTLE_MUTE, 0, val, 0);
		}
		continue;

	case RC_MUTE_CLEAR:
		COPY_CHANNELMASK(tmp_chbitmask, channel_mute);
		CLEAR_CHANNELMASK(channel_mute);
		if (! COMPARE_CHANNELMASK(tmp_chbitmask, channel_mute)) {
			sync_restart(0);
			jump_flag = 1;
			for (i = 0; i < MAX_CHANNELS; i++)
				ctl_mode_event(CTLE_MUTE, 0, i, 0);
		}
		continue;
	}
	if(intr)
	    return RC_QUIT;
	if(play_pause_flag)
	    usleep(300000);
    } while (rc != RC_NONE || play_pause_flag);
    return jump_flag ? RC_JUMP : RC_NONE;
}

#ifdef USE_DSP_EFFECT
/* do_compute_data_midi() for new chorus */
static void do_compute_data_midi(int32 count)
{
	int i, j, uv, stereo, n, ch, note;
	int32 *vpblist[MAX_CHANNELS];
	int vc[MAX_CHANNELS];
	int channel_effect,channel_reverb,channel_chorus,channel_delay,channel_eq;
	int32 cnt = count * 2;
	
	stereo = ! (play_mode->encoding & PE_MONO);
	n = count * ((stereo) ? 8 : 4); /* in bytes */

	memset(buffer_pointer, 0, n);

	memset(insertion_effect_buffer, 0, n);

	/* are effects valid? / don't supported in mono */
	channel_reverb = ((opt_reverb_control == 1 || opt_reverb_control == 3)
			&& stereo);
	channel_chorus = (opt_chorus_control != 0 && stereo);
	channel_delay = (opt_delay_control > 0 && stereo);

	/* is EQ valid? */
	channel_eq = opt_eq_control && (eq_status.low_gain != 0x40 || eq_status.high_gain != 0x40);

	channel_effect = ((channel_reverb || channel_chorus
			|| channel_delay || channel_eq || opt_insertion_effect) && stereo);

	uv = upper_voices;
	for(i=0;i<uv;i++) {
		if(voice[i].status != VOICE_FREE) {
			channel[voice[i].channel].lasttime = current_sample + count;
		}
	}

	if(channel_effect) {
		int buf_index = 0;
		
		if(reverb_buffer == NULL) {	/* allocating buffer for channel effect */
			reverb_buffer = (char *)safe_malloc(MAX_CHANNELS * AUDIO_BUFFER_SIZE * 8);
		}

		for(i=0;i<MAX_CHANNELS;i++) {
			if(ISDRUMCHANNEL(i) && opt_drum_effect == 0) {
				vpblist[i] = buffer_pointer;
			} else if(opt_insertion_effect && channel[i].insertion_effect) {
				vpblist[i] = insertion_effect_buffer;
			} else if(channel[i].eq_on || (channel[i].reverb_level >= 0
					&& current_sample - channel[i].lasttime < REVERB_MAX_DELAY_OUT)
					|| channel[i].chorus_level > 0 || channel[i].delay_level > 0) {
				vpblist[i] = (int32*)(reverb_buffer + buf_index);
				buf_index += n;
			} else {
				vpblist[i] = buffer_pointer;
			}
		}

		if(buf_index) {memset(reverb_buffer, 0, buf_index);}
	}

	for(i=0;i<uv;i++) {
		if(voice[i].status != VOICE_FREE) {
			int32 *vpb;
			
			if(channel_effect) {
				ch = voice[i].channel;
				vpb = vpblist[ch];
			} else {
				vpb = buffer_pointer;
			}

			if(!IS_SET_CHANNELMASK(channel_mute, voice[i].channel)) {
				mix_voice(vpb, i, count);
			} else {
				free_voice(i);
				ctl_note_event(i);
			}

			if(voice[i].timeout > 0 && voice[i].timeout < current_sample) {
				/* timeout (See also "#extension timeout" line in *.cfg file */
				if(voice[i].timeout > 1) {
					finish_note(i);
				} else {
					free_voice(i);
					ctl_note_event(i);
				}
			}
		}
	}

	while(uv > 0 && voice[uv - 1].status == VOICE_FREE)	{uv--;}
	upper_voices = uv;

	if(channel_effect) {
		if(opt_insertion_effect) { 	/* insertion effect */
			/* applying insertion effect */
			do_insertion_effect(insertion_effect_buffer, cnt);
			/* sending insertion effect voice to channel effect */
			set_ch_chorus(insertion_effect_buffer, cnt, gs_ieffect.send_chorus);
			set_ch_delay(insertion_effect_buffer, cnt, gs_ieffect.send_delay);
			set_ch_reverb(insertion_effect_buffer, cnt,	gs_ieffect.send_reverb);
			if(gs_ieffect.send_eq_switch && channel_eq) {
				set_ch_eq(insertion_effect_buffer, cnt);
			} else {
				set_dry_signal(insertion_effect_buffer, cnt);
			}
		}

		for(i=0;i<MAX_CHANNELS;i++) {	/* system effects */
			int32 *p;	
			p = vpblist[i];
			if(p != buffer_pointer && p != insertion_effect_buffer) {
				if(channel_chorus && channel[i].chorus_level > 0) {
					set_ch_chorus(p, cnt, channel[i].chorus_level);
				}
				if(channel_delay && channel[i].delay_level > 0) {
					set_ch_delay(p, cnt, channel[i].delay_level);
				}
				if(channel_reverb && channel[i].reverb_level > 0
					&& current_sample - channel[i].lasttime < REVERB_MAX_DELAY_OUT) {
					set_ch_reverb(p, cnt, channel[i].reverb_level);
				}
				if(channel_eq && channel[i].eq_on) {
					set_ch_eq(p, cnt);
				} else {
					set_dry_signal(p, cnt);
				}
			}
		}
		
		if(channel_reverb) {
			set_ch_reverb(buffer_pointer, cnt, DEFAULT_REVERB_SEND_LEVEL);
		}
		set_dry_signal(buffer_pointer, cnt);

		/* mixing signal and applying system effects */ 
		mix_dry_signal(buffer_pointer, cnt);
		if(channel_eq) {do_ch_eq(buffer_pointer, cnt);}
		if(channel_chorus) {do_ch_chorus(buffer_pointer, cnt);}
		if(channel_delay) {do_ch_delay(buffer_pointer, cnt);}
		if(channel_reverb) {do_ch_reverb(buffer_pointer, cnt);}
	}

	current_sample += count;
}

#else
/* do_compute_data_midi() for traditionally chorus */
static void do_compute_data_midi(int32 count)
{
	int i, j, uv, stereo, n, ch, note;
	int32 *vpblist[MAX_CHANNELS];
	int vc[MAX_CHANNELS];
	int channel_reverb;
	int channel_effect;
	int32 cnt = count * 2;
	
	stereo = ! (play_mode->encoding & PE_MONO);
	n = count * ((stereo) ? 8 : 4); /* in bytes */
	channel_reverb = ((opt_reverb_control == 1 || opt_reverb_control == 3)
			&& stereo);
		/* don't supported in mono */
	memset(buffer_pointer, 0, n);

	channel_effect = ((opt_reverb_control || opt_chorus_control
			|| opt_delay_control || opt_eq_control || opt_insertion_effect) && stereo);
	uv = upper_voices;
	for (i = 0; i < uv; i++)
		if (voice[i].status != VOICE_FREE)
			channel[voice[i].channel].lasttime = current_sample + count;

	if (channel_reverb) {
		int chbufidx;
		
		if (! make_rvid_flag) {
			make_rvid();
			make_rvid_flag = 1;
		}
		chbufidx = 0;
		for (i = 0; i < MAX_CHANNELS; i++) {
			vc[i] = 0;
			if (channel[i].reverb_id != -1
					&& current_sample - channel[i].lasttime
					< REVERB_MAX_DELAY_OUT) {
				if (reverb_buffer == NULL)
					reverb_buffer = (char *) safe_malloc(MAX_CHANNELS
							* AUDIO_BUFFER_SIZE * 8);
				if (channel[i].reverb_id != i)
					vpblist[i] = vpblist[channel[i].reverb_id];
				else {
					vpblist[i] = (int32 *) (reverb_buffer + chbufidx);
					chbufidx += n;
				}
			} else
				vpblist[i] = buffer_pointer;
		}
		if (chbufidx)
			memset(reverb_buffer, 0, chbufidx);
	}
	for (i = 0; i < uv; i++)
		if (voice[i].status != VOICE_FREE) {
			int32 *vpb;
			
			if (channel_reverb) {
				int ch = voice[i].channel;
				
				vpb = vpblist[ch];
				vc[ch] = 1;
			} else
				vpb = buffer_pointer;
			if (! IS_SET_CHANNELMASK(channel_mute, voice[i].channel))
				mix_voice(vpb, i, count);
			else {
				free_voice(i);
				ctl_note_event(i);
			}
			if (voice[i].timeout > 0 && voice[i].timeout < current_sample) {
				if (voice[i].timeout > 1)
					finish_note(i);
						/* timeout (See also "#extension timeout"
						line in *.cfg file */
				else {
					free_voice(i);
					ctl_note_event(i);
				}
			}
		}

	while (uv > 0 && voice[uv - 1].status == VOICE_FREE)
		uv--;
	upper_voices = uv;

	if (channel_reverb) {
		int k;
		
		k = count * 2; /* calclated buffer length in int32 */
		for (i = 0; i < MAX_CHANNELS; i++) {
			int32 *p;
			
			p = vpblist[i];
			if (p != buffer_pointer && channel[i].reverb_id == i)
				set_ch_reverb(p, k, channel[i].reverb_level);
		}
		set_ch_reverb(buffer_pointer, k, DEFAULT_REVERB_SEND_LEVEL);
		do_ch_reverb(buffer_pointer, k);
	}
	current_sample += count;
}
#endif

static void do_compute_data_wav(int32 count)
{
    int i, stereo, n, file_byte, samples;

    stereo = !(play_mode->encoding & PE_MONO);
    samples = (stereo ? (count * 2) : count );
    n = samples*4; /* in bytes */
    file_byte = samples*2; /*regard as 16bit*/

    memset(buffer_pointer, 0, n);

    tf_read(wav_buffer, 1, file_byte, current_file_info->pcm_tf);
    for( i=0; i<samples; i++ ){
    	buffer_pointer[i] = (LE_SHORT(wav_buffer[i])) << 16;
    	buffer_pointer[i] /=4; /*level down*/
    }

    current_sample += count;
}

static void do_compute_data_aiff(int32 count)
{
    int i, stereo, n, file_byte, samples;

    stereo = !(play_mode->encoding & PE_MONO);
    samples = (stereo ? (count * 2) : count );
    n = samples*4; /* in bytes */
    file_byte = samples*2; /*regard as 16bit*/

    memset(buffer_pointer, 0, n);

    tf_read(wav_buffer, 1, file_byte, current_file_info->pcm_tf);
    for( i=0; i<samples; i++ ){
    	buffer_pointer[i] = (BE_SHORT(wav_buffer[i])) << 16;
    	buffer_pointer[i] /=4; /*level down*/
    }

    current_sample += count;
}

static void do_compute_data(int32 count)
{
    switch(current_file_info->pcm_mode)
    {
      case PCM_MODE_NON:
    	do_compute_data_midi(count);
      	break;
      case PCM_MODE_WAV:
    	do_compute_data_wav(count);
        break;
      case PCM_MODE_AIFF:
    	do_compute_data_aiff(count);
        break;
      case PCM_MODE_AU:
        break;
      case PCM_MODE_MP3:
        break;
    }    
}

static int check_midi_play_end(MidiEvent *e, int len)
{
    int i, type;

    for(i = 0; i < len; i++)
    {
	type = e[i].type;
	if(type == ME_NOTEON || type == ME_LAST || type == ME_WRD || type == ME_SHERRY)
	    return 0;
	if(type == ME_EOT)
	    return i + 1;
    }
    return 0;
}

static int compute_data(int32 count);
static int midi_play_end(void)
{
    int i, rc = RC_TUNE_END;

    check_eot_flag = 0;

    if(opt_realtime_playing && current_sample == 0)
    {
	reset_voices();
	return RC_TUNE_END;
    }

    if(upper_voices > 0)
    {
	int fadeout_cnt;

	rc = compute_data(play_mode->rate);
	if(RC_IS_SKIP_FILE(rc))
	    goto midi_end;

	for(i = 0; i < upper_voices; i++)
	    if(voice[i].status & (VOICE_ON | VOICE_SUSTAINED))
		finish_note(i);
	if(opt_realtime_playing)
	    fadeout_cnt = 3;
	else
	    fadeout_cnt = 6;
	for(i = 0; i < fadeout_cnt && upper_voices > 0; i++)
	{
	    rc = compute_data(play_mode->rate / 2);
	    if(RC_IS_SKIP_FILE(rc))
		goto midi_end;
	}

	/* kill voices */
	kill_all_voices();
	rc = compute_data(MAX_DIE_TIME);
	if(RC_IS_SKIP_FILE(rc))
	    goto midi_end;
	upper_voices = 0;
    }

    /* clear reverb echo sound */
    init_reverb(play_mode->rate);
    for(i = 0; i < MAX_CHANNELS; i++)
    {
	channel[i].reverb_level = -1;
	channel[i].reverb_id = -1;
	make_rvid_flag = 1;
    }

    /* output null sound */
    if(opt_realtime_playing)
	rc = compute_data((int32)(play_mode->rate * PLAY_INTERLEAVE_SEC/2));
    else
	rc = compute_data((int32)(play_mode->rate * PLAY_INTERLEAVE_SEC));
    if(RC_IS_SKIP_FILE(rc))
	goto midi_end;

    compute_data(0); /* flush buffer to device */

    if(ctl->trace_playing)
    {
	rc = aq_flush(0); /* Wait until play out */
	if(RC_IS_SKIP_FILE(rc))
	    goto midi_end;
    }
    else
    {
	trace_flush();
	rc = aq_soft_flush();
	if(RC_IS_SKIP_FILE(rc))
	    goto midi_end;
    }

  midi_end:
    if(RC_IS_SKIP_FILE(rc))
	aq_flush(1);

    ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "Playing time: ~%d seconds",
	      current_sample/play_mode->rate+2);
    ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "Notes cut: %d",
	      cut_notes);
    ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "Notes lost totally: %d",
	      lost_notes);
    if(RC_IS_SKIP_FILE(rc))
	return rc;
    return RC_TUNE_END;
}

/* count=0 means flush remaining buffered data to output device, then
   flush the device itself */
static int compute_data(int32 count)
{
  int rc;

  if (!count)
    {
      if (buffered_count)
      {
	  ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY,
		    "output data (%d)", buffered_count);

#ifdef SUPPORT_SOUNDSPEC
	  soundspec_update_wave(common_buffer, buffered_count);
#endif /* SUPPORT_SOUNDSPEC */

	  if(aq_add(common_buffer, buffered_count) == -1)
	      return RC_ERROR;
      }
      buffer_pointer=common_buffer;
      buffered_count=0;
      return RC_NONE;
    }

  while ((count+buffered_count) >= audio_buffer_size)
    {
      int i;

      if((rc = apply_controls()) != RC_NONE)
	  return rc;

      do_compute_data(audio_buffer_size-buffered_count);
      count -= audio_buffer_size-buffered_count;
      ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY,
		"output data (%d)", audio_buffer_size);

#ifdef SUPPORT_SOUNDSPEC
      soundspec_update_wave(common_buffer, audio_buffer_size);
#endif /* SUPPORT_SOUNDSPEC */

#if defined(CSPLINE_INTERPOLATION) || defined(LAGRANGE_INTERPOLATION) || defined(NEWTON_INTERPOLATION) || defined(GAUSS_INTERPOLATION)
      /* fall back to linear interpolation when queue < 100% */
      if (! opt_realtime_playing && (play_mode->flag & PF_CAN_TRACE)) {
	  if (!aq_fill_buffer_flag &&
	      100 * ((double)(aq_filled() + aq_soft_filled()) /
		     aq_get_dev_queuesize()) < 99)
	      reduce_quality_flag = 1;
	  else
	      reduce_quality_flag = no_4point_interpolation;
      }
#endif

#ifdef REDUCE_VOICE_TIME_TUNING
      /* Auto voice reduce implementation by Masanao Izumo */
      if(reduce_voice_threshold &&
	 (play_mode->flag & PF_CAN_TRACE) &&
	 !aq_fill_buffer_flag &&
	 aq_get_dev_queuesize() > 0)
      {
	  /* Reduce voices if there is not enough audio device buffer */

          int nv, filled, filled_limit, rate, rate_limit;
          static int last_filled;

	  filled = aq_filled();

	  rate_limit = 75;
	  if(reduce_voice_threshold >= 0)
	  {
	      filled_limit = play_mode->rate * reduce_voice_threshold / 1000
		  + 1; /* +1 disable zero */
	  }
	  else /* Use default threshold */
	  {
	      int32 maxfill;
	      maxfill = aq_get_dev_queuesize();
	      filled_limit = REDUCE_VOICE_TIME_TUNING;
	      if(filled_limit > maxfill / 5) /* too small audio buffer */
	      {
		  rate_limit -= 100 * audio_buffer_size / maxfill / 5;
		  filled_limit = 1;
	      }
	  }

	  /* Calculate rate as it is displayed in ncurs_c.c */
	  /* The old method of calculating rate resulted in very low values
	     when using the new high order interplation methods on "slow"
	     CPUs when the queue was being drained WAY too quickly.  This
	     caused premature voice reduction under Linux, even if the queue
	     was over 2000%, leading to major voice lossage. */
	  rate = (int)(((double)(aq_filled() + aq_soft_filled()) /
                  	aq_get_dev_queuesize()) * 100 + 0.5);

          for(i = nv = 0; i < upper_voices; i++)
	      if(voice[i].status != VOICE_FREE)
	          nv++;

	  if(! opt_realtime_playing)
	  {
	      /* calculate ok_nv, the "optimum" max polyphony */
	      if (auto_reduce_polyphony && rate < 85) {
		/* average in current nv */
	        if ((rate == old_rate && nv > min_bad_nv) ||
	            (rate >= old_rate && rate < 20)) {
	        	ok_nv_total += nv;
	        	ok_nv_counts++;
	        }
	        /* increase polyphony when it is too low */
	        else if (nv == voices &&
	                 (rate > old_rate && filled > last_filled)) {
	          		ok_nv_total += nv + 1;
	          		ok_nv_counts++;
	        }
	        /* reduce polyphony when loosing buffer */
	        else if (rate < 75 &&
	        	 (rate < old_rate && filled < last_filled)) {
	        	ok_nv_total += min_bad_nv;
	    		ok_nv_counts++;
	        }
	        else goto NO_RESCALE_NV;

		/* rescale ok_nv stuff every 1 seconds */
		if (current_sample >= ok_nv_sample && ok_nv_counts > 1) {
			ok_nv_total >>= 1;
			ok_nv_counts >>= 1;
			ok_nv_sample = current_sample + (play_mode->rate);
		}

		NO_RESCALE_NV:;
	      }
	  }

	  /* EAW -- if buffer is < 75%, start reducing some voices to
	     try to let it recover.  This really helps a lot, preserves
	     decent sound, and decreases the frequency of lost ON notes */
	  if ((! opt_realtime_playing && rate < rate_limit)
	      || filled < filled_limit)
	  {
	      if(filled <= last_filled)
	      {
	          int v, kill_nv, temp_nv;

		  /* set bounds on "good" and "bad" nv */
		  if (! opt_realtime_playing && rate > 20 &&
		      nv < min_bad_nv) {
		  	min_bad_nv = nv;
	                if (max_good_nv < min_bad_nv)
	                	max_good_nv = min_bad_nv;
	          }

		  /* EAW -- count number of !ON voices */
		  /* treat chorus notes as !ON */
		  for(i = kill_nv = 0; i < upper_voices; i++) {
		      if(voice[i].status & VOICE_FREE ||
		         voice[i].cache != NULL)
		      		continue;
		      
		      if((voice[i].status & ~(VOICE_ON|VOICE_SUSTAINED) &&
			  !(voice[i].status & ~(VOICE_DIE) &&
			    voice[i].sample->note_to_use)))
				kill_nv++;
		  }

		  /* EAW -- buffer is dangerously low, drasticly reduce
		     voices to a hopefully "safe" amount */
		  if (filled < filled_limit &&
		      (opt_realtime_playing || rate < 10)) {
		      FLOAT_T n;

		      /* calculate the drastic voice reduction */
		      if(nv > kill_nv) /* Avoid division by zero */
		      {
			  n = (FLOAT_T) nv / (nv - kill_nv);
			  temp_nv = (int)(nv - nv / (n + 1));

			  /* reduce by the larger of the estimates */
			  if (kill_nv < temp_nv && temp_nv < nv)
			      kill_nv = temp_nv;
		      }
		      else kill_nv = nv - 1; /* do not kill all the voices */
		  }
		  else {
		      /* the buffer is still high enough that we can throw
		         fewer voices away; keep the ON voices, use the
		         minimum "bad" nv as a floor on voice reductions */
		      temp_nv = nv - min_bad_nv;
		      if (kill_nv > temp_nv)
		          kill_nv = temp_nv;
		  }

		  for(i = 0; i < kill_nv; i++)
		      v = reduce_voice();

		  /* lower max # of allowed voices to let the buffer recover */
		  if (auto_reduce_polyphony) {
		  	temp_nv = nv - kill_nv;
		  	ok_nv = ok_nv_total / ok_nv_counts;

		  	/* decrease it to current nv left */
		  	if (voices > temp_nv && temp_nv > ok_nv)
			    voice_decrement_conservative(voices - temp_nv);
			/* decrease it to ok_nv */
		  	else if (voices > ok_nv && temp_nv <= ok_nv)
			    voice_decrement_conservative(voices - ok_nv);
		  	/* increase the polyphony */
		  	else if (voices < ok_nv)
			    voice_increment(ok_nv - voices);
		  }

		  while(upper_voices > 0 &&
			voice[upper_voices - 1].status == VOICE_FREE)
		      upper_voices--;
	      }
	      last_filled = filled;
	  }
	  else {
	      if (! opt_realtime_playing && rate >= rate_limit &&
	          filled > last_filled) {

		    /* set bounds on "good" and "bad" nv */
		    if (rate > 85 && nv > max_good_nv) {
		  	max_good_nv = nv;
		  	if (min_bad_nv > max_good_nv)
		  	    min_bad_nv = max_good_nv;
		    }

		    if (auto_reduce_polyphony) {
		    	/* reset ok_nv stuff when out of danger */
		    	ok_nv_total = max_good_nv * ok_nv_counts;
			if (ok_nv_counts > 1) {
			    ok_nv_total >>= 1;
			    ok_nv_counts >>= 1;
			}

		    	/* restore max # of allowed voices to normal */
			restore_voices(0);
		    }
	      }

	      last_filled = filled_limit;
          }
          old_rate = rate;
      }
#endif

      if(aq_add(common_buffer, audio_buffer_size) == -1)
	  return RC_ERROR;

      buffer_pointer=common_buffer;
      buffered_count=0;
      if(current_event->type != ME_EOT)
	  ctl_timestamp();

      /* check break signals */
      VOLATILE_TOUCH(intr);
      if(intr)
	  return RC_QUIT;

      if(upper_voices == 0 && check_eot_flag &&
	 (i = check_midi_play_end(current_event, EOT_PRESEARCH_LEN)) > 0)
      {
	  if(i > 1)
	      ctl->cmsg(CMSG_INFO, VERB_VERBOSE,
			"Last %d MIDI events are ignored", i - 1);
	  return midi_play_end();
      }
    }
  if (count>0)
    {
      do_compute_data(count);
      buffered_count += count;
      buffer_pointer += (play_mode->encoding & PE_MONO) ? count : count*2;
    }
  return RC_NONE;
}

static void update_modulation_wheel(int ch, int val)
{
    int i, uv = upper_voices;
    for(i = 0; i < uv; i++)
	if(voice[i].status != VOICE_FREE && voice[i].channel == ch)
	{
	    /* Set/Reset mod-wheel */
	    voice[i].modulation_wheel = val;
	    voice[i].vibrato_delay = 0;
	    recompute_freq(i);
	}
}

static void drop_portamento(int ch)
{
    int i, uv = upper_voices;

    channel[ch].porta_control_ratio = 0;
    for(i = 0; i < uv; i++)
	if(voice[i].status != VOICE_FREE &&
	   voice[i].channel == ch &&
	   voice[i].porta_control_ratio)
	{
	    voice[i].porta_control_ratio = 0;
	    recompute_freq(i);
	}
    channel[ch].last_note_fine = -1;
}

static void update_portamento_controls(int ch)
{
    if(!channel[ch].portamento ||
       (channel[ch].portamento_time_msb | channel[ch].portamento_time_lsb)
       == 0)
	drop_portamento(ch);
    else
    {
	double mt, dc;
	int d;

	mt = midi_time_table[channel[ch].portamento_time_msb & 0x7F] *
	    midi_time_table2[channel[ch].portamento_time_lsb & 0x7F] *
		PORTAMENTO_TIME_TUNING;
	dc = play_mode->rate * mt;
	d = (int)(1.0 / (mt * PORTAMENTO_CONTROL_RATIO));
	d++;
	channel[ch].porta_control_ratio = (int)(d * dc + 0.5);
	channel[ch].porta_dpb = d;
    }
}

static void update_portamento_time(int ch)
{
    int i, uv = upper_voices;
    int dpb;
    int32 ratio;

    update_portamento_controls(ch);
    dpb = channel[ch].porta_dpb;
    ratio = channel[ch].porta_control_ratio;

    for(i = 0; i < uv; i++)
    {
	if(voice[i].status != VOICE_FREE &&
	   voice[i].channel == ch &&
	   voice[i].porta_control_ratio)
	{
	    voice[i].porta_control_ratio = ratio;
	    voice[i].porta_dpb = dpb;
	    recompute_freq(i);
	}
    }
}

static void update_legato_controls(int ch)
{
	double mt, dc;
	int d;

	mt = 0.06250 * PORTAMENTO_TIME_TUNING * 0.3;
	dc = play_mode->rate * mt;
	d = (int)(1.0 / (mt * PORTAMENTO_CONTROL_RATIO));
	d++;
	channel[ch].porta_control_ratio = (int)(d * dc + 0.5);
	channel[ch].porta_dpb = d;
}

int play_event(MidiEvent *ev)
{
    int ch, k, orig_ch, layered;
    int32 i, j, cet;

    if(play_mode->flag & PF_MIDI_EVENT)
	return play_mode->acntl(PM_REQ_MIDI, ev);
    if(!(play_mode->flag & PF_PCM_STREAM))
	return RC_NONE;

    current_event = ev;
    cet = MIDI_EVENT_TIME(ev);

    if(ctl->verbosity >= VERB_DEBUG_SILLY)
	ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY,
		  "Midi Event %d: %s %d %d %d", cet,
		  event_name(ev->type), ev->channel, ev->a, ev->b);
    if(cet > current_sample)
    {
	int rc;


    if(midi_streaming!=0){
    	if ( (cet - current_sample) * 1000 / play_mode->rate > stream_max_compute ) {
			kill_all_voices();
//			reset_voices();
//			ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY, "play_event: discard %d samples", cet - current_sample);
			current_sample = cet;
		}
    }

	rc = compute_data(cet - current_sample);
	ctl_mode_event(CTLE_REFRESH, 0, 0, 0);
    if(rc == RC_JUMP)
	{
		ctl_timestamp();
		return RC_NONE;
	}
	if(rc != RC_NONE)
	    return rc;
	}

#ifndef SUPPRESS_CHANNEL_LAYER
	orig_ch = ev->channel;
	layered = !IS_SYSEX_EVENT_TYPE(ev->type);
	for(k = 0; (!layered && k < 1) ||
		(layered && channel[orig_ch].channel_layer[k] != -1); k++)
	{
	if(layered) {
		ev->channel = channel[orig_ch].channel_layer[k];
	}
#endif
	ch = ev->channel;

    switch(ev->type)
    {
	/* MIDI Events */
      case ME_NOTEOFF:
	note_off(ev);
	break;

      case ME_NOTEON:
	note_on(ev);
	break;

      case ME_KEYPRESSURE:
	adjust_pressure(ev);
	break;

      case ME_PROGRAM:
	midi_program_change(ch, ev->a);
	ctl_prog_event(ch);
	break;

      case ME_CHANNEL_PRESSURE:
	adjust_channel_pressure(ev);
	break;

      case ME_PITCHWHEEL:
	channel[ch].pitchbend = ev->a + ev->b * 128;
	channel[ch].pitchfactor = 0;
	/* Adjust pitch for notes already playing */
	adjust_pitch(ch);
	ctl_mode_event(CTLE_PITCH_BEND, 1, ch, channel[ch].pitchbend);
	break;

	/* Controls */
      case ME_TONE_BANK_MSB:
	channel[ch].bank_msb = ev->a;
	break;

      case ME_TONE_BANK_LSB:
	channel[ch].bank_lsb = ev->a;
	break;

      case ME_MODULATION_WHEEL:
	channel[ch].modulation_wheel =
	    midi_cnv_vib_depth(ev->a);
	update_modulation_wheel(ch, channel[ch].modulation_wheel);
	ctl_mode_event(CTLE_MOD_WHEEL, 1, ch, channel[ch].modulation_wheel);
	break;

      case ME_MAINVOLUME:
	channel[ch].volume = ev->a;
	adjust_volume(ch);
	ctl_mode_event(CTLE_VOLUME, 1, ch, ev->a);
	break;

      case ME_PAN:
	channel[ch].panning = ev->a;
	channel[ch].pan_random = 0;
	if(adjust_panning_immediately && !channel[ch].pan_random)
	    adjust_panning(ch);
	ctl_mode_event(CTLE_PANNING, 1, ch, ev->a);
	break;

      case ME_EXPRESSION:
	channel[ch].expression = ev->a;
	adjust_volume(ch);
	ctl_mode_event(CTLE_EXPRESSION, 1, ch, ev->a);
	break;

      case ME_SUSTAIN:
	channel[ch].sustain = (ev->a >= 64);
	if(!ev->a)
	    drop_sustain(ch);
	ctl_mode_event(CTLE_SUSTAIN, 1, ch, ev->a >= 64);
	break;

      case ME_SOSTENUTO:
	ctl->cmsg(CMSG_INFO,VERB_NOISY,"Sostenuto - this function is not supported.");
	break;

      case ME_LEGATO_FOOTSWITCH:
    channel[ch].legato = (ev->a >= 64);
	ctl->cmsg(CMSG_INFO,VERB_NOISY,"Legato Footswitch (CH:%d VAL:%d)",ch,channel[ch].legato);
	break;

      case ME_HOLD2:
	ctl->cmsg(CMSG_INFO,VERB_NOISY,"Hold2 - this function is not supported.");
	break;

      case ME_BREATH:
	ctl->cmsg(CMSG_INFO,VERB_NOISY,"Breath - this function is not supported.");
	break;

      case ME_FOOT:
	ctl->cmsg(CMSG_INFO,VERB_NOISY,"Foot - this function is not supported.");
	break;

      case ME_BALANCE:
	ctl->cmsg(CMSG_INFO,VERB_NOISY,"Balance - this function is not supported.");
	break;

      case ME_PORTAMENTO_TIME_MSB:
	channel[ch].portamento_time_msb = ev->a;
	update_portamento_time(ch);
	break;

      case ME_PORTAMENTO_TIME_LSB:
	channel[ch].portamento_time_lsb = ev->a;
	update_portamento_time(ch);
	break;

      case ME_PORTAMENTO:
	channel[ch].portamento = (ev->a >= 64);
	if(!channel[ch].portamento)
	    drop_portamento(ch);
	break;

	  case ME_SOFT_PEDAL:
		  if(opt_lpf_def) {
			  channel[ch].soft_pedal = ev->a;
			  ctl->cmsg(CMSG_INFO,VERB_NOISY,"Soft Pedal (CH:%d VAL:%d)",ch,channel[ch].soft_pedal);
		  }
		  break;

	  case ME_HARMONIC_CONTENT:
		  if(opt_lpf_def) {
			  channel[ch].param_resonance = ev->a - 64;
			  ctl->cmsg(CMSG_INFO,VERB_NOISY,"Harmonic Content (CH:%d VAL:%d)",ch,channel[ch].param_resonance);
		  }
		  break;

	  case ME_BRIGHTNESS:
		  if(opt_lpf_def) {
			  channel[ch].param_cutoff_freq = ev->a - 64;
			  ctl->cmsg(CMSG_INFO,VERB_NOISY,"Brightness (CH:%d VAL:%d)",ch,channel[ch].param_cutoff_freq);
		  }
		  break;

      case ME_DATA_ENTRY_MSB:
	if(channel[ch].rpn_7f7f_flag) /* disable */
	    break;
	if((i = last_rpn_addr(ch)) >= 0)
	{
	    channel[ch].rpnmap[i] = ev->a;
	    update_rpn_map(ch, i, 1);
	}
	break;

      case ME_DATA_ENTRY_LSB:
	if(channel[ch].rpn_7f7f_flag) /* disable */
	    break;
	/* Ignore */
	channel[ch].nrpn = -1;
	break;

      case ME_REVERB_EFFECT:
	if(opt_reverb_control)
	{
	    set_reverb_level(ch, ev->a);
	    ctl_mode_event(CTLE_REVERB_EFFECT, 1, ch, get_reverb_level(ch));
	}
	break;

      case ME_CHORUS_EFFECT:
	if(opt_chorus_control)
	{
	    if(opt_chorus_control == 1)
		channel[ch].chorus_level = ev->a;
	    else
		channel[ch].chorus_level = -opt_chorus_control;
	    ctl_mode_event(CTLE_CHORUS_EFFECT, 1, ch, get_chorus_level(ch));
		if(ev->a) {
			ctl->cmsg(CMSG_INFO,VERB_NOISY,"Chorus Send (CH:%d LEVEL:%d)",ch,ev->a);
		}
	}
	break;

      case ME_TREMOLO_EFFECT:
	ctl->cmsg(CMSG_INFO,VERB_NOISY,"Tremolo Send (CH:%d LEVEL:%d)",ch,ev->a);
	break;

      case ME_CELESTE_EFFECT:
	if(opt_delay_control) {
		channel[ch].delay_level = ev->a;
		ctl->cmsg(CMSG_INFO,VERB_NOISY,"Delay Send (CH:%d LEVEL:%d)",ch,ev->a);
	}
	break;

	  case ME_ATTACK_TIME:
  	if(!opt_tva_attack) { break; }
	set_envelope_time(ch,ev->a,0);
	break;

	  case ME_RELEASE_TIME:
  	if(!opt_tva_release) { break; }
	set_envelope_time(ch,ev->a,3);
	break;

      case ME_PHASER_EFFECT:
	ctl->cmsg(CMSG_INFO,VERB_NOISY,"Phaser Send (CH:%d LEVEL:%d)",ch,ev->a);
	break;

      case ME_RPN_INC:
	if(channel[ch].rpn_7f7f_flag) /* disable */
	    break;
	if((i = last_rpn_addr(ch)) >= 0)
	{
	    if(channel[ch].rpnmap[i] < 127)
		channel[ch].rpnmap[i]++;
	    update_rpn_map(ch, i, 1);
	}
	break;

      case ME_RPN_DEC:
	if(channel[ch].rpn_7f7f_flag) /* disable */
	    break;
	if((i = last_rpn_addr(ch)) >= 0)
	{
	    if(channel[ch].rpnmap[i] > 0)
		channel[ch].rpnmap[i]--;
	    update_rpn_map(ch, i, 1);
	}
	break;

      case ME_NRPN_LSB:
	channel[ch].lastlrpn = ev->a;
	channel[ch].nrpn = 1;
	break;

      case ME_NRPN_MSB:
	channel[ch].lastmrpn = ev->a;
	channel[ch].nrpn = 1;
	break;

      case ME_RPN_LSB:
	channel[ch].lastlrpn = ev->a;
	channel[ch].nrpn = 0;
	break;

      case ME_RPN_MSB:
	channel[ch].lastmrpn = ev->a;
	channel[ch].nrpn = 0;
	break;

      case ME_ALL_SOUNDS_OFF:
	all_sounds_off(ch);
	break;

      case ME_RESET_CONTROLLERS:
	reset_controllers(ch);
	redraw_controllers(ch);
	break;

      case ME_ALL_NOTES_OFF:
	all_notes_off(ch);
	break;

      case ME_MONO:
	channel[ch].mono = 1;
	all_notes_off(ch);
	break;

      case ME_POLY:
	channel[ch].mono = 0;
	all_notes_off(ch);
	break;

	/* TiMidity Extensionals */
      case ME_RANDOM_PAN:
	channel[ch].panning = int_rand(128);
	channel[ch].pan_random = 1;
	if(adjust_panning_immediately && !channel[ch].pan_random)
	    adjust_panning(ch);
	break;

      case ME_SET_PATCH:
	i = channel[ch].special_sample = current_event->a;
	if(special_patch[i] != NULL)
	    special_patch[i]->sample_offset = 0;
	ctl_prog_event(ch);
	break;

      case ME_TEMPO:
	current_play_tempo = ch + ev->b * 256 + ev->a * 65536;
	ctl_mode_event(CTLE_TEMPO, 1, current_play_tempo, 0);
	break;

      case ME_CHORUS_TEXT:
      case ME_LYRIC:
      case ME_MARKER:
      case ME_INSERT_TEXT:
      case ME_TEXT:
      case ME_KARAOKE_LYRIC:
	i = ev->a | ((int)ev->b << 8);
	ctl_mode_event(CTLE_LYRIC, 1, i, 0);
	break;

      case ME_GSLCD:
	i = ev->a | ((int)ev->b << 8);
	ctl_mode_event(CTLE_GSLCD, 1, i, 0);
	break;

      case ME_MASTER_VOLUME:
	master_volume_ratio = (int32)ev->a + 256 * (int32)ev->b;
	adjust_master_volume();
	break;

      case ME_RESET:
	change_system_mode(ev->a);
	reset_midi(1);
	break;

      case ME_PATCH_OFFS:
	i = channel[ch].special_sample;
	if(special_patch[i] != NULL)
	    special_patch[i]->sample_offset =
		(current_event->a | 256 * current_event->b);
	break;

      case ME_WRD:
	push_midi_trace2(wrd_midi_event,
			 ch, current_event->a | (current_event->b << 8));
	break;

      case ME_SHERRY:
	push_midi_trace1(wrd_sherry_event,
			 ch | (current_event->a<<8) | (current_event->b<<16));
	break;

      case ME_DRUMPART:
	if(midi_drumpart_change(ch, current_event->a))
	{
	    /* Update bank information */
	    midi_program_change(ch, channel[ch].program);
	    ctl_mode_event(CTLE_DRUMPART, 1, ch, ISDRUMCHANNEL(ch));
	    ctl_prog_event(ch);
	}
	break;

      case ME_KEYSHIFT:
	i = (int)current_event->a - 0x40;
	if(i != channel[ch].key_shift)
	{
	    all_sounds_off(ch);
	    channel[ch].key_shift = (int8)i;
	}
	break;

	case ME_KEYSIG:
		current_keysig = current_event->a + current_event->b * 16;
		ctl_mode_event(CTLE_KEYSIG, 1, current_keysig, 0);
		current_temper_keysig = current_keysig;
		ctl_mode_event(CTLE_TEMPER_KEYSIG, 1, current_temper_keysig, 0);
		if (opt_force_keysig != 8) {
			i = current_keysig + ((current_keysig < 8) ? 7 : -6);
			note_key_offset -= floor(note_key_offset / 12.0) * 12;
			for (j = 0; j < note_key_offset; j++)
				i += (i > 10) ? -5 : 7;
			j = opt_force_keysig + ((current_keysig < 8) ? 7 : 10);
			while (i != j && i != j + 12) {
				if (++note_key_offset > 6)
					note_key_offset -= 12;
				i += (i > 10) ? -5 : 7;
			}
			kill_all_voices();
			ctl_mode_event(CTLE_KEY_OFFSET, 1, note_key_offset, 0);
		}
		i = current_keysig + ((current_keysig < 8) ? 7 : -9), j = 0;
		while (i != 7 && i != 19)
			i += (i < 7) ? 5 : -7, j++;
		j += note_key_offset, j -= floor(j / 12.0) * 12;
		current_freq_table = j;
		break;

	case ME_SCALE_TUNING:
		resamp_cache_refer_alloff(ch, current_event->time);
		channel[ch].scale_tuning[current_event->a] = current_event->b;
		adjust_pitch(ch);
		break;

	case ME_BULK_TUNING_DUMP:
		set_single_note_tuning(ch, current_event->a, current_event->b, 0);
		break;

	case ME_SINGLE_NOTE_TUNING:
		set_single_note_tuning(ch, current_event->a, current_event->b, 1);
		break;

	case ME_TEMPER_KEYSIG:
		current_temper_keysig = current_event->a;
		ctl_mode_event(CTLE_TEMPER_KEYSIG, 1, current_temper_keysig, 0);
		i = current_temper_keysig + ((current_temper_keysig < 8) ? 7 : -9);
		j = 0;
		while (i != 7 && i != 19)
			i += (i < 7) ? 5 : -7, j++;
		j += note_key_offset, j -= floor(j / 12.0) * 12;
		current_freq_table = j;
		if (current_event->b)
			for (i = 0; i < upper_voices; i++)
				if (voice[i].status != VOICE_FREE) {
					voice[i].temper_instant = 1;
					recompute_freq(i);
				}
		break;

	case ME_TEMPER_TYPE:
		channel[ch].temper_type = current_event->a;
		ctl_mode_event(CTLE_TEMPER_TYPE, 1, ch, channel[ch].temper_type);
		if (temper_type_mute) {
			if (temper_type_mute & 1 << current_event->a
					- ((current_event->a >= 0x40) ? 0x3c : 0)) {
				SET_CHANNELMASK(channel_mute, ch);
				ctl_mode_event(CTLE_MUTE, 1, ch, 1);
			} else {
				UNSET_CHANNELMASK(channel_mute, ch);
				ctl_mode_event(CTLE_MUTE, 1, ch, 0);
			}
		}
		if (current_event->b)
			for (i = 0; i < upper_voices; i++)
				if (voice[i].status != VOICE_FREE) {
					voice[i].temper_instant = 1;
					recompute_freq(i);
				}
		break;

	case ME_MASTER_TEMPER_TYPE:
		for (i = 0; i < MAX_CHANNELS; i++) {
			channel[i].temper_type = current_event->a;
			ctl_mode_event(CTLE_TEMPER_TYPE, 1, i, channel[i].temper_type);
		}
		if (temper_type_mute) {
			if (temper_type_mute & 1 << current_event->a
					- ((current_event->a >= 0x40) ? 0x3c : 0)) {
				FILL_CHANNELMASK(channel_mute);
				for (i = 0; i < MAX_CHANNELS; i++)
					ctl_mode_event(CTLE_MUTE, 1, i, 1);
			} else {
				CLEAR_CHANNELMASK(channel_mute);
				for (i = 0; i < MAX_CHANNELS; i++)
					ctl_mode_event(CTLE_MUTE, 1, i, 0);
			}
		}
		if (current_event->b)
			for (i = 0; i < upper_voices; i++)
				if (voice[i].status != VOICE_FREE) {
					voice[i].temper_instant = 1;
					recompute_freq(i);
				}
		break;

	case ME_SYSEX_LSB:
		process_sysex_event(ME_SYSEX_LSB,ch,current_event->a,current_event->b);
	    break;

	case ME_SYSEX_MSB:
		process_sysex_event(ME_SYSEX_MSB,ch,current_event->a,current_event->b);
	    break;

	case ME_SYSEX_GS_LSB:
		process_sysex_event(ME_SYSEX_GS_LSB,ch,current_event->a,current_event->b);
	    break;

	case ME_SYSEX_GS_MSB:
		process_sysex_event(ME_SYSEX_GS_MSB,ch,current_event->a,current_event->b);
	    break;

	case ME_SYSEX_XG_LSB:
		process_sysex_event(ME_SYSEX_XG_LSB,ch,current_event->a,current_event->b);
	    break;

	case ME_SYSEX_XG_MSB:
		process_sysex_event(ME_SYSEX_XG_MSB,ch,current_event->a,current_event->b);
	    break;

	case ME_NOTE_STEP:
		i = ev->a + ((ev->b & 0x0f) << 8);
		j = ev->b >> 4;
		ctl_mode_event(CTLE_METRONOME, 1, i, j);
		if (readmidi_wrd_mode)
			wrdt->update_events();
		break;

      case ME_EOT:
	return midi_play_end();
    }
#ifndef SUPPRESS_CHANNEL_LAYER
	}
	ev->channel = orig_ch;
#endif

    return RC_NONE;
}

static void set_single_note_tuning(int part, int a, int b, int rt)
{
	static int tp;	/* tuning program number */
	static int kn;	/* MIDI key number */
	static int st;	/* the nearest equal-tempered semitone */
	double f, fst;	/* fraction of semitone */
	int i;
	
	switch (part) {
	case 0:
		tp = a;
		break;
	case 1:
		kn = a;
		st = b;
		break;
	case 2:
		if (st == 0x7f && a == 0x7f && b == 0x7f)	/* no change */
			break;
		f = 440 * pow(2.0, (st - 69) / 12.0);
		fst = pow(2.0, (a << 7 | b) / 196608.0);
		freq_table_tuning[tp][kn] = f * fst * 1000 + 0.5;
		if (rt)
			for (i = 0; i < upper_voices; i++)
				if (voice[i].status != VOICE_FREE) {
					voice[i].temper_instant = 1;
					recompute_freq(i);
				}
		break;
	}
}

static int play_midi(MidiEvent *eventlist, int32 samples)
{
    int rc;
    static int play_count = 0;

    if (play_mode->id_character == 'M') {
	int cnt;

	convert_mod_to_midi_file(eventlist);

	play_count = 0;
	cnt = free_global_mblock();	/* free unused memory */
	if(cnt > 0)
	    ctl->cmsg(CMSG_INFO, VERB_VERBOSE,
		      "%d memory blocks are free", cnt);
	return rc;
    }

    sample_count = samples;
    event_list = eventlist;
    lost_notes = cut_notes = 0;
    check_eot_flag = 1;

    wrd_midi_event(-1, -1); /* For initialize */

    reset_midi(0);
    if(!opt_realtime_playing &&
       allocate_cache_size > 0 &&
       !IS_CURRENT_MOD_FILE &&
       (play_mode->flag&PF_PCM_STREAM))
    {
	play_midi_prescan(eventlist);
	reset_midi(0);
    }

    rc = aq_flush(0);
    if(RC_IS_SKIP_FILE(rc))
	return rc;

    skip_to(midi_restart_time);

    if(midi_restart_time > 0) { /* Need to update interface display */
      int i;
      for(i = 0; i < MAX_CHANNELS; i++)
	redraw_controllers(i);
    }
    rc = RC_NONE;
    for(;;)
    {
	midi_restart_time = 1;
	rc = play_event(current_event);
	if(rc != RC_NONE)
	    break;
	if (midi_restart_time)    /* don't skip the first event if == 0 */
	    current_event++;
    }

    if(play_count++ > 3)
    {
	int cnt;
	play_count = 0;
	cnt = free_global_mblock();	/* free unused memory */
	if(cnt > 0)
	    ctl->cmsg(CMSG_INFO, VERB_VERBOSE,
		      "%d memory blocks are free", cnt);
    }
    return rc;
}

static void read_header_wav(struct timidity_file* tf)
{
    char buff[44];
    tf_read( buff, 1, 44, tf);
}

static int read_header_aiff(struct timidity_file* tf)
{
    char buff[5]="    ";
    int i;
    
    for( i=0; i<100; i++ ){
    	buff[0]=buff[1]; buff[1]=buff[2]; buff[2]=buff[3];
    	tf_read( &buff[3], 1, 1, tf);
    	if( strcmp(buff,"SSND")==0 ){
            /*SSND chunk found */
    	    tf_read( &buff[0], 1, 4, tf);
    	    tf_read( &buff[0], 1, 4, tf);
	    ctl->cmsg(CMSG_INFO, VERB_NOISY,
		      "aiff header read OK.");
	    return 0;
    	}
    }
    /*SSND chunk not found */
    return -1;
}

static int load_pcm_file_wav()
{
    char *filename;

    if(strcmp(pcm_alternate_file, "auto") == 0)
    {
	filename = safe_malloc(strlen(current_file_info->filename)+5);
	strcpy(filename, current_file_info->filename);
	strcat(filename, ".wav");
    }
    else if(strlen(pcm_alternate_file) >= 5 &&
	    strncasecmp(pcm_alternate_file + strlen(pcm_alternate_file) - 4,
			".wav", 4) == 0)
	filename = safe_strdup(pcm_alternate_file);
    else
	return -1;

    ctl->cmsg(CMSG_INFO, VERB_NOISY,
		      "wav filename: %s", filename);
    current_file_info->pcm_tf = open_file(filename, 0, OF_SILENT);
    if( current_file_info->pcm_tf ){
	ctl->cmsg(CMSG_INFO, VERB_NOISY,
		      "open successed.");
	read_header_wav(current_file_info->pcm_tf);
	current_file_info->pcm_filename = filename;
	current_file_info->pcm_mode = PCM_MODE_WAV;
	return 0;
    }else{
	ctl->cmsg(CMSG_INFO, VERB_NOISY,
		      "open failed.");
	free(filename);
	current_file_info->pcm_filename = NULL;
	return -1;
    }
}

static int load_pcm_file_aiff()
{
    char *filename;

    if(strcmp(pcm_alternate_file, "auto") == 0)
    {
	filename = safe_malloc(strlen(current_file_info->filename)+6);
	strcpy(filename, current_file_info->filename);
	strcat( filename, ".aiff");
    }
    else if(strlen(pcm_alternate_file) >= 6 &&
	    strncasecmp(pcm_alternate_file + strlen(pcm_alternate_file) - 5,
			".aiff", 5) == 0)
	filename = safe_strdup(pcm_alternate_file);
    else
	return -1;

    ctl->cmsg(CMSG_INFO, VERB_NOISY,
		      "aiff filename: %s", filename);
    current_file_info->pcm_tf = open_file(filename, 0, OF_SILENT);
    if( current_file_info->pcm_tf ){
	ctl->cmsg(CMSG_INFO, VERB_NOISY,
		      "open successed.");
	read_header_aiff(current_file_info->pcm_tf);
	current_file_info->pcm_filename = filename;
	current_file_info->pcm_mode = PCM_MODE_AIFF;
	return 0;
    }else{
	ctl->cmsg(CMSG_INFO, VERB_NOISY,
		      "open failed.");
	free(filename);
	current_file_info->pcm_filename = NULL;
	return -1;
    }
}

static void load_pcm_file()
{
    if( load_pcm_file_wav()==0 ) return; /*load OK*/
    if( load_pcm_file_aiff()==0 ) return; /*load OK*/
}

static int play_midi_load_file(char *fn,
			       MidiEvent **event,
			       int32 *nsamples)
{
    int rc;
    struct timidity_file *tf;
    int32 nevents;

    *event = NULL;

    if(!strcmp(fn, "-"))
	file_from_stdin = 1;
    else
	file_from_stdin = 0;

    ctl_mode_event(CTLE_NOW_LOADING, 0, (long)fn, 0);
    ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "MIDI file: %s", fn);
    if((tf = open_midi_file(fn, 1, OF_VERBOSE)) == NULL)
    {
	ctl_mode_event(CTLE_LOADING_DONE, 0, -1, 0);
	return RC_ERROR;
    }

    *event = NULL;
    rc = check_apply_control();
    if(RC_IS_SKIP_FILE(rc))
    {
	close_file(tf);
	ctl_mode_event(CTLE_LOADING_DONE, 0, 1, 0);
	return rc;
    }

    *event = read_midi_file(tf, &nevents, nsamples, fn);
    close_file(tf);

    if(*event == NULL)
    {
	ctl_mode_event(CTLE_LOADING_DONE, 0, -1, 0);
	return RC_ERROR;
    }

    ctl->cmsg(CMSG_INFO, VERB_NOISY,
	      "%d supported events, %d samples, time %d:%02d",
	      nevents, *nsamples,
	      *nsamples / play_mode->rate / 60,
	      (*nsamples / play_mode->rate) % 60);

    current_file_info->pcm_mode = PCM_MODE_NON; /*initialize*/
    if(pcm_alternate_file != NULL &&
       strcmp(pcm_alternate_file, "none") != 0 &&
       (play_mode->flag&PF_PCM_STREAM))
	load_pcm_file();

    if(!IS_CURRENT_MOD_FILE &&
       (play_mode->flag&PF_PCM_STREAM))
    {
	/* FIXME: Instruments is not need for pcm_alternate_file. */

	/* Load instruments
	 * If opt_realtime_playing, the instruments will be loaded later.
	 */
	if(!opt_realtime_playing)
	{
	    rc = RC_NONE;
	    load_missing_instruments(&rc);
	    if(RC_IS_SKIP_FILE(rc))
	    {
		/* Instrument loading is terminated */
		ctl_mode_event(CTLE_LOADING_DONE, 0, 1, 0);
		clear_magic_instruments();
		return rc;
	    }
	}
    }
    else
	clear_magic_instruments();	/* Clear load markers */

    ctl_mode_event(CTLE_LOADING_DONE, 0, 0, 0);

    return RC_NONE;
}

int play_midi_file(char *fn)
{
    int i, j, rc;
    static int last_rc = RC_NONE;
    MidiEvent *event;
    int32 nsamples;

    /* Set current file information */
    current_file_info = get_midi_file_info(fn, 1);

    rc = check_apply_control();
    if(RC_IS_SKIP_FILE(rc) && rc != RC_RELOAD)
	return rc;

    /* Reset key & speed each files */
    current_keysig = current_temper_keysig = opt_init_keysig;
    note_key_offset = 0;
    midi_time_ratio = 1.0;
	for (i = 0; i < MAX_CHANNELS; i++) {
		for (j = 0; j < 12; j++)
			channel[i].scale_tuning[j] = 0;
		channel[i].prev_scale_tuning = 0;
		channel[i].temper_type = 0;
	}
    CLEAR_CHANNELMASK(channel_mute);
	if (temper_type_mute & 1)
		FILL_CHANNELMASK(channel_mute);

    /* Reset restart offset */
    midi_restart_time = 0;

#ifdef REDUCE_VOICE_TIME_TUNING
    /* Reset voice reduction stuff */
    min_bad_nv = 256;
    max_good_nv = 1;
    ok_nv_total = 32;
    ok_nv_counts = 1;
    ok_nv = 32;
    ok_nv_sample = 0;
    old_rate = -1;
#if defined(CSPLINE_INTERPOLATION) || defined(LAGRANGE_INTERPOLATION) || defined(NEWTON_INTERPOLATION) || defined(GAUSS_INTERPOLATION)
    reduce_quality_flag = no_4point_interpolation;
#endif
    restore_voices(0);
#endif

	ctl_mode_event(CTLE_METRONOME, 0, 0, 0);
	ctl_mode_event(CTLE_KEYSIG, 0, current_keysig, 0);
	ctl_mode_event(CTLE_TEMPER_KEYSIG, 0, current_temper_keysig, 0);
	if (opt_force_keysig != 8) {
		i = current_keysig + ((current_keysig < 8) ? 7 : -6);
		j = opt_force_keysig + ((current_keysig < 8) ? 7 : 10);
		while (i != j && i != j + 12) {
			if (++note_key_offset > 6)
				note_key_offset -= 12;
			i += (i > 10) ? -5 : 7;
		}
	}
	ctl_mode_event(CTLE_KEY_OFFSET, 0, note_key_offset, 0);
	i = current_keysig + ((current_keysig < 8) ? 7 : -9), j = 0;
	while (i != 7 && i != 19)
		i += (i < 7) ? 5 : -7, j++;
	j += note_key_offset, j -= floor(j / 12.0) * 12;
	current_freq_table = j;
	ctl_mode_event(CTLE_TEMPO, 0, current_play_tempo, 0);
	ctl_mode_event(CTLE_TIME_RATIO, 0, 100 / midi_time_ratio + 0.5, 0);
	for (i = 0; i < MAX_CHANNELS; i++) {
		ctl_mode_event(CTLE_TEMPER_TYPE, 0, i, channel[i].temper_type);
		ctl_mode_event(CTLE_MUTE, 0, i, temper_type_mute & 1);
	}
  play_reload: /* Come here to reload MIDI file */
    rc = play_midi_load_file(fn, &event, &nsamples);
    if(RC_IS_SKIP_FILE(rc))
	goto play_end; /* skip playing */

    init_mblock(&playmidi_pool);
    ctl_mode_event(CTLE_PLAY_START, 0, nsamples, 0);
    play_mode->acntl(PM_REQ_PLAY_START, NULL);
    rc = play_midi(event, nsamples);
    play_mode->acntl(PM_REQ_PLAY_END, NULL);
    ctl_mode_event(CTLE_PLAY_END, 0, 0, 0);
    reuse_mblock(&playmidi_pool);

    for(i = 0; i < MAX_CHANNELS; i++)
	memset(channel[i].drums, 0, sizeof(channel[i].drums));

  play_end:
    if(current_file_info->pcm_tf){
    	close_file(current_file_info->pcm_tf);
    	current_file_info->pcm_tf = NULL;
    	free( current_file_info->pcm_filename );
    	current_file_info->pcm_filename = NULL;
    }
    
    if(wrdt->opened)
	wrdt->end();

    if(free_instruments_afterwards)
    {
	int cnt;
	free_instruments(0);
	cnt = free_global_mblock(); /* free unused memory */
	if(cnt > 0)
	    ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "%d memory blocks are free",
		      cnt);
    }

    free_special_patch(-1);

    if(event != NULL)
	free(event);
    if(rc == RC_RELOAD)
	goto play_reload;

    if(rc == RC_ERROR)
    {
	if(current_file_info->file_type == IS_OTHER_FILE)
	    current_file_info->file_type = IS_ERROR_FILE;
	if(last_rc == RC_REALLY_PREVIOUS)
	    return RC_REALLY_PREVIOUS;
    }
    last_rc = rc;
    return rc;
}

void dumb_pass_playing_list(int number_of_files, char *list_of_files[])
{
    int i = 0;

    for(;;)
    {
	switch(play_midi_file(list_of_files[i]))
	{
	  case RC_REALLY_PREVIOUS:
	    if(i > 0)
		i--;
	    break;

	  default: /* An error or something */
	  case RC_NEXT:
	    if(i < number_of_files-1)
	    {
		i++;
		break;
	    }
	    aq_flush(0);

	    if(!(ctl->flags & CTLF_LIST_LOOP))
		return;
	    i = 0;
	    break;

	    case RC_QUIT:
		return;
	}
    }
}

void default_ctl_lyric(int lyricid)
{
    char *lyric;

    lyric = event2string(lyricid);
    if(lyric != NULL)
	ctl->cmsg(CMSG_TEXT, VERB_VERBOSE, "%s", lyric + 1);
}

void ctl_mode_event(int type, int trace, long arg1, long arg2)
{
    CtlEvent ce;
    ce.type = type;
    ce.v1 = arg1;
    ce.v2 = arg2;
    if(trace && ctl->trace_playing)
	push_midi_trace_ce(ctl->event, &ce);
    else
	ctl->event(&ce);
}

void ctl_note_event(int noteID)
{
    CtlEvent ce;
    ce.type = CTLE_NOTE;
    ce.v1 = voice[noteID].status;
    ce.v2 = voice[noteID].channel;
    ce.v3 = voice[noteID].note;
    ce.v4 = voice[noteID].velocity;
    if(ctl->trace_playing)
	push_midi_trace_ce(ctl->event, &ce);
    else
	ctl->event(&ce);
}

static void ctl_timestamp(void)
{
    long i, secs, voices;
    CtlEvent ce;
    static int last_secs = -1, last_voices = -1;

    secs = (long)(current_sample / (midi_time_ratio * play_mode->rate));
    for(i = voices = 0; i < upper_voices; i++)
	if(voice[i].status != VOICE_FREE)
	    voices++;
    if(secs == last_secs && voices == last_voices)
	return;
    ce.type = CTLE_CURRENT_TIME;
    ce.v1 = last_secs = secs;
    ce.v2 = last_voices = voices;
    if(ctl->trace_playing)
	push_midi_trace_ce(ctl->event, &ce);
    else
	ctl->event(&ce);
}

static void ctl_updatetime(int32 samples)
{
    long secs;
    secs = (long)(samples / (midi_time_ratio * play_mode->rate));
    ctl_mode_event(CTLE_CURRENT_TIME, 0, secs, 0);
    ctl_mode_event(CTLE_REFRESH, 0, 0, 0);
}

static void ctl_prog_event(int ch)
{
    CtlEvent ce;
    int bank, prog;

    if(IS_CURRENT_MOD_FILE)
    {
	bank = 0;
	prog = channel[ch].special_sample;
    }
    else
    {
	bank = channel[ch].bank;
	prog = channel[ch].program;
    }

    ce.type = CTLE_PROGRAM;
    ce.v1 = ch;
    ce.v2 = prog;
    ce.v3 = (long)channel_instrum_name(ch);
    ce.v4 = (bank |
	     (channel[ch].bank_lsb << 8) |
	     (channel[ch].bank_msb << 16));
    if(ctl->trace_playing)
	push_midi_trace_ce(ctl->event, &ce);
    else
	ctl->event(&ce);
}

static void ctl_pause_event(int pause, int32 s)
{
    long secs;
    secs = (long)(s / (midi_time_ratio * play_mode->rate));
    ctl_mode_event(CTLE_PAUSE, 0, pause, secs);
}

char *channel_instrum_name(int ch)
{
    char *comm;
    int bank, prog;

    if(ISDRUMCHANNEL(ch)) {
	bank = channel[ch].bank;
	if (drumset[bank] == NULL) return "";
	prog = 0;
	comm = drumset[bank]->tone[prog].comment;
	if (comm == NULL) return "";
	return comm;
    }

    if(channel[ch].program == SPECIAL_PROGRAM)
	return "Special Program";

    if(IS_CURRENT_MOD_FILE)
    {
	int pr;
	pr = channel[ch].special_sample;
	if(pr > 0 &&
	   special_patch[pr] != NULL &&
	   special_patch[pr]->name != NULL)
	    return special_patch[pr]->name;
	return "MOD";
    }

    bank = channel[ch].bank;
    prog = channel[ch].program;
    instrument_map(channel[ch].mapID, &bank, &prog);
    if(tonebank[bank] == NULL)
	bank = 0;
    comm = tonebank[bank]->tone[prog].comment;
    if(comm == NULL)
	comm = tonebank[0]->tone[prog].comment;
    return comm;
}


/*
 * For MIDI stream player.
 */
void playmidi_stream_init(void)
{
    int i;
    static int first = 1;

    note_key_offset = 0;
    midi_time_ratio = 1.0;
    CLEAR_CHANNELMASK(channel_mute);
	if (temper_type_mute & 1)
		FILL_CHANNELMASK(channel_mute);
    midi_restart_time = 0;
    if(first)
    {
	first = 0;
        init_mblock(&playmidi_pool);
	current_file_info = get_midi_file_info("TiMidity", 1);
    midi_streaming=1;
    }
    else
        reuse_mblock(&playmidi_pool);

    /* Fill in current_file_info */
    current_file_info->readflag = 1;
    current_file_info->seq_name = safe_strdup("TiMidity server");
    current_file_info->karaoke_title = current_file_info->first_text = NULL;
    current_file_info->mid = 0x7f;
    current_file_info->hdrsiz = 0;
    current_file_info->format = 0;
    current_file_info->tracks = 0;
    current_file_info->divisions = 192; /* ?? */
    current_file_info->time_sig_n = 4; /* 4/ */
    current_file_info->time_sig_d = 4; /* /4 */
    current_file_info->time_sig_c = 24; /* clock */
    current_file_info->time_sig_b = 8;  /* q.n. */
    current_file_info->samples = 0;
    current_file_info->max_channel = MAX_CHANNELS;
    current_file_info->compressed = 0;
    current_file_info->midi_data = NULL;
    current_file_info->midi_data_size = 0;
    current_file_info->file_type = IS_OTHER_FILE;

    current_play_tempo = 500000;
    check_eot_flag = 0;

    /* Setup default drums */
    COPY_CHANNELMASK(drumchannels, current_file_info->drumchannels);
    COPY_CHANNELMASK(drumchannel_mask, current_file_info->drumchannel_mask);
    for(i = 0; i < MAX_CHANNELS; i++)
	memset(channel[i].drums, 0, sizeof(channel[i].drums));
    reset_midi(0);
    change_system_mode(DEFAULT_SYSTEM_MODE);

    playmidi_tmr_reset();
}

void playmidi_tmr_reset(void)
{
    int i;

    aq_flush(0);
    current_sample = 0;
    buffered_count = 0;
    buffer_pointer = common_buffer;
    for(i = 0; i < MAX_CHANNELS; i++)
	channel[i].lasttime = 0;
    play_mode->acntl(PM_REQ_PLAY_START, NULL);
}
