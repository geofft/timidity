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

   playmidi.h

   */

#ifndef ___PLAYMIDI_H_
#define ___PLAYMIDI_H_

typedef struct {
  int32 time;
  uint8 type, channel, a, b;
} MidiEvent;

#define REVERB_MAX_DELAY_OUT (4 * play_mode->rate)

#define MIDI_EVENT_NOTE(ep) (ISDRUMCHANNEL((ep)->channel) ? (ep)->a : \
			     (((int)(ep)->a + note_key_offset + \
			       channel[ep->channel].key_shift) & 0x7f))

#define MIDI_EVENT_TIME(ep) ((int32)((ep)->time * midi_time_ratio + 0.5))

/* Midi events */
enum midi_event_t
{
    ME_NONE,

    /* MIDI events */
    ME_NOTEOFF,
    ME_NOTEON,
    ME_KEYPRESSURE,
    ME_PROGRAM,
    ME_CHANNEL_PRESSURE,
    ME_PITCHWHEEL,

    /* Controls */
    ME_TONE_BANK_MSB,
    ME_TONE_BANK_LSB,
    ME_MODULATION_WHEEL,
    ME_BREATH,
    ME_FOOT,
    ME_MAINVOLUME,
    ME_BALANCE,
    ME_PAN,
    ME_EXPRESSION,
    ME_SUSTAIN,
    ME_PORTAMENTO_TIME_MSB,
    ME_PORTAMENTO_TIME_LSB,
    ME_PORTAMENTO,
    ME_PORTAMENTO_CONTROL,
    ME_DATA_ENTRY_MSB,
    ME_DATA_ENTRY_LSB,
    ME_SOSTENUTO,
    ME_SOFT_PEDAL,
	ME_LEGATO_FOOTSWITCH,
	ME_HOLD2,
    ME_HARMONIC_CONTENT,
    ME_RELEASE_TIME,
    ME_ATTACK_TIME,
    ME_BRIGHTNESS,
    ME_REVERB_EFFECT,
    ME_TREMOLO_EFFECT,
    ME_CHORUS_EFFECT,
    ME_CELESTE_EFFECT,
    ME_PHASER_EFFECT,
    ME_RPN_INC,
    ME_RPN_DEC,
    ME_NRPN_LSB,
    ME_NRPN_MSB,
    ME_RPN_LSB,
    ME_RPN_MSB,
    ME_ALL_SOUNDS_OFF,
    ME_RESET_CONTROLLERS,
    ME_ALL_NOTES_OFF,
    ME_MONO,
    ME_POLY,

    /* TiMidity Extensionals */
#if 0
    ME_VOLUME_ONOFF,		/* Not supported */
#endif
    ME_RANDOM_PAN,
    ME_SET_PATCH,		/* Install special instrument */
    ME_DRUMPART,
    ME_KEYSHIFT,
    ME_PATCH_OFFS,		/* Change special instrument sample position
				 * Channel, LSB, MSB
				 */

    /* Global channel events */

    ME_TEMPO,
    ME_CHORUS_TEXT,
    ME_LYRIC,
    ME_GSLCD,	/* GS L.C.D. Exclusive message event */
    ME_MARKER,
    ME_INSERT_TEXT, /* for SC */
    ME_TEXT,
    ME_KARAOKE_LYRIC, /* for KAR format */
    ME_MASTER_VOLUME,
    ME_RESET,			/* Reset and change system mode */
    ME_NOTE_STEP,

    ME_TIMESIG,			/* Time signature */
    ME_KEYSIG,			/* Key signature */
    ME_SCALE_TUNING,		/* Scale tuning */
    ME_BULK_TUNING_DUMP,	/* Bulk tuning dump */
    ME_SINGLE_NOTE_TUNING,	/* Single-note tuning */
    ME_TEMPER_KEYSIG,		/* Temperament key signature */
    ME_TEMPER_TYPE,		/* Temperament type */
    ME_MASTER_TEMPER_TYPE,	/* Master Temperament type */

	ME_SYSEX_LSB,	/* Universal system exclusive message (LSB) */
	ME_SYSEX_MSB,	/* Universal system exclusive message (MSB) */
	ME_SYSEX_GS_LSB,	/* GS system exclusive message (LSB) */
	ME_SYSEX_GS_MSB,	/* GS system exclusive message (MSB) */
	ME_SYSEX_XG_LSB,	/* XG system exclusive message (LSB) */
	ME_SYSEX_XG_MSB,	/* XG system exclusive message (MSB) */

    ME_WRD,			/* for MIMPI WRD tracer */
    ME_SHERRY,			/* for Sherry WRD tracer */
    ME_BARMARKER,
    ME_STEP,			/* for Metronome */

    ME_LAST = 254,		/* Last sequence of MIDI list.
				 * This event is reserved for realtime player.
				 */
    ME_EOT = 255		/* End of MIDI.  Finish to play */
};

#define GLOBAL_CHANNEL_EVENT_TYPE(type)	\
	((type) == ME_NONE || (type) >= ME_TEMPO)

enum rpn_data_address_t /* NRPN/RPN */
{
    NRPN_ADDR_0108,
    NRPN_ADDR_0109,
    NRPN_ADDR_010A,
    NRPN_ADDR_0120,
    NRPN_ADDR_0121,
    NRPN_ADDR_0163,
    NRPN_ADDR_0164,
    NRPN_ADDR_0166,
    NRPN_ADDR_1400,
    NRPN_ADDR_1500,
    NRPN_ADDR_1600,
    NRPN_ADDR_1700,
    NRPN_ADDR_1800,
    NRPN_ADDR_1900,
    NRPN_ADDR_1A00,
    NRPN_ADDR_1C00,
    NRPN_ADDR_1D00,
    NRPN_ADDR_1E00,
    NRPN_ADDR_1F00,
    RPN_ADDR_0000,
    RPN_ADDR_0001,
    RPN_ADDR_0002,
    RPN_ADDR_0003,
    RPN_ADDR_0004,
    RPN_ADDR_7F7F,
    RPN_ADDR_FFFF,
    RPN_MAX_DATA_ADDR
};

struct DrumParts
{
    int8 drum_panning;
    int32 drum_envelope_rate[6]; /* drum instrument envelope */
    int8 pan_random;    /* flag for drum random pan */
	FLOAT_T drum_level;

	int8 chorus_level, reverb_level, delay_level, coarse, fine,
		play_note, rx_note_off, drum_cutoff_freq, drum_resonance;
};

typedef struct {
  int8	bank_msb, bank_lsb, bank, program, volume,
	expression, sustain, panning, mono, portamento, modulation_wheel,
	key_shift;

  /* chorus, reverb... Coming soon to a 300-MHz, eight-way superscalar
     processor near you */
  int8	chorus_level,	/* Chorus level */
	reverb_level;	/* Reverb level. */
  int	reverb_id;	/* Reverb ID used for reverb optimize implementation
			   >=0 reverb_level
			   -1: DEFAULT_REVERB_SEND_LEVEL
			   */
  int8 delay_level;	/* Delay Send Level */
  int8 eq_on;	/* EQ ON/OFF */
  int8 insertion_effect;

  /* Special sample ID. (0 means Normal sample) */
  uint8 special_sample;

  int pitchbend;

  FLOAT_T
    pitchfactor; /* precomputed pitch bend factor to save some fdiv's */

  /* For portamento */
  uint8 portamento_time_msb, portamento_time_lsb;
  int porta_control_ratio, porta_dpb;
  int32 last_note_fine;

  /* For Drum part */
  struct DrumParts *drums[128];

  /* For vibrato */
  int32 vibrato_ratio, vibrato_depth, vibrato_delay;

  /* For RPN */
  uint8 rpnmap[RPN_MAX_DATA_ADDR]; /* pseudo RPN address map */
  uint8 lastlrpn, lastmrpn;
  int8  nrpn; /* 0:RPN, 1:NRPN, -1:Undefined */
  int rpn_7f7f_flag;		/* Boolean flag used for RPN 7F/7F */

  /* For channel envelope */
  int32 envelope_rate[6]; /* for Envelope Generator in mix.c
			   * 0: value for attack rate
			   * 1: value for decay rate
			   * 3: value for release rate
			   */

  int mapID;			/* Program map ID */
  AlternateAssign *altassign;	/* Alternate assign patch table */
  int32 lasttime;     /* Last sample time of computed voice on this channel */

  /* flag for random pan */
  int pan_random;

  /* for Voice LPF / Resonance */
  int8 param_resonance, param_cutoff_freq;	/* -64 ~ 63 */
  double cutoff_freq_coef;
  double resonance_dB;

  int8 velocity_sense_depth, velocity_sense_offset;
  
  int8 scale_tuning[12], prev_scale_tuning;
  int8 temper_type;

  int8 soft_pedal;

  int8 tone_map0_number;
  FLOAT_T pitch_offset_fine;	/* in Hz */
  int8 assign_mode;

  int8 legato;	/* legato footswitch */
  int8 legato_flag;	/* note-on flag for legato */

  int8 mod_pitch_ctl;	/* in semitones */
  int8 mod_amp_ctl, mod_lfo1_rate_ctl, mod_lfo2_rate_ctl, mod_lfo2_tva_depth;
  int16 mod_tvf_cutoff_ctl, mod_lfo1_pitch_depth,
	  mod_lfo2_pitch_depth, mod_lfo2_tvf_depth;	/* in cents */

  int8 caf_pitch_ctl;	/* in semitones */
  int8 caf_amp_ctl, caf_lfo1_rate_ctl, caf_lfo2_rate_ctl, caf_lfo2_tva_depth;
  int16 caf_tvf_cutoff_ctl, caf_lfo1_pitch_depth,
	  caf_lfo2_pitch_depth, caf_lfo2_tvf_depth;	/* in cents */

  int8 paf_pitch_ctl;	/* in semitones */
  int8 paf_amp_ctl, paf_lfo1_rate_ctl, paf_lfo2_rate_ctl, paf_lfo2_tva_depth;
  int16 paf_tvf_cutoff_ctl, paf_lfo1_pitch_depth,
	  paf_lfo2_pitch_depth, paf_lfo2_tvf_depth;	/* in cents */

  int8 *channel_layer;

  int8 sysex_gs_msb_addr, sysex_gs_msb_val,
		sysex_xg_msb_addr, sysex_xg_msb_val, sysex_msb_addr, sysex_msb_val;
} Channel;

/* Causes the instrument's default panning to be used. */
#define NO_PANNING -1

typedef struct {
	int16 freq, last_freq, orig_freq;
	double reso_dB, last_reso_dB, orig_reso_dB, reso_lin, filter_gain; 
	int32 a1, a2, b02, b1, hist1, hist2;
	int32 a1_incr, a2_incr, b02_incr, b1_incr;
	int8 filter_calculated;
	int32 filter_coeff_incr_count;
} FilterCoefficients;

typedef struct {
  uint8
    status, channel, note, velocity;
  int vid, temper_instant;
  Sample *sample;
#if SAMPLE_LENGTH_BITS == 32 && TIMIDITY_HAVE_INT64
  int64 sample_offset;	/* sample_offset must be signed */
#else
  splen_t sample_offset;
#endif
  int32
    orig_frequency, frequency, sample_increment,
    envelope_volume, envelope_target, envelope_increment,
    tremolo_sweep, tremolo_sweep_position,
    tremolo_phase, tremolo_phase_increment,
    vibrato_sweep, vibrato_sweep_position;

  final_volume_t left_mix, right_mix;
#ifdef SMOOTH_MIXING
  int32 old_left_mix, old_right_mix,
     left_mix_offset, right_mix_offset,
     left_mix_inc, right_mix_inc;
#endif

  FLOAT_T
    left_amp, right_amp, tremolo_volume;
  int32
    vibrato_sample_increment[VIBRATO_SAMPLE_INCREMENTS], vibrato_delay;
  int
    vibrato_phase, orig_vibrato_control_ratio, vibrato_control_ratio,
    vibrato_depth, vibrato_control_counter,
    envelope_stage, control_counter, panning, panned, modulation_wheel;
  int16 tremolo_depth;

  /* for portamento */
  int porta_control_ratio, porta_control_counter, porta_dpb;
  int32 porta_pb;

  int delay; /* Note ON delay samples */
  int32 timeout;
  struct cache_hash *cache;

  uint8 chorus_link;	/* Chorus link */
  int8 proximate_flag;

  int sample_panning_average;

  FilterCoefficients fc;

  FLOAT_T envelope_scale, last_envelope_volume;
  int32 inv_envelope_scale;

  int modenv_stage;
  int32
    modenv_volume, modenv_target, modenv_increment;
  FLOAT_T last_modenv_volume;
  int32 tremolo_delay, modenv_delay;

  int32 delay_counter;

  int8 key_pressure;
} Voice;

/* Voice status options: */
#define VOICE_FREE	(1<<0)
#define VOICE_ON	(1<<1)
#define VOICE_SUSTAINED	(1<<2)
#define VOICE_OFF	(1<<3)
#define VOICE_DIE	(1<<4)

/* Voice panned options: */
#define PANNED_MYSTERY 0
#define PANNED_LEFT 1
#define PANNED_RIGHT 2
#define PANNED_CENTER 3
/* Anything but PANNED_MYSTERY only uses the left volume */

#define ISDRUMCHANNEL(c)  IS_SET_CHANNELMASK(drumchannels, c)

extern Channel channel[];
extern Voice voice[];

extern int32 control_ratio, amp_with_poly, amplification;

extern ChannelBitMask default_drumchannel_mask;
extern ChannelBitMask drumchannel_mask;
extern ChannelBitMask default_drumchannels;
extern ChannelBitMask drumchannels;

extern int adjust_panning_immediately;
extern int voices, upper_voices;
extern int note_key_offset;
extern FLOAT_T midi_time_ratio;
extern int opt_modulation_wheel;
extern int opt_portamento;
extern int opt_nrpn_vibrato;
extern int opt_reverb_control;
extern int opt_chorus_control;
extern int opt_surround_chorus;
extern int opt_channel_pressure;
extern int opt_lpf_def;
extern int opt_overlap_voice_allow;
extern int opt_temper_control;
extern int opt_tva_attack;
extern int opt_tva_decay;
extern int opt_tva_release;
extern int opt_delay_control;
extern int opt_eq_control;
extern int opt_insertion_effect;
extern int opt_drum_effect;
extern int opt_env_attack;
extern int opt_modulation_envelope;
extern int noise_sharp_type;
extern int32 current_play_tempo;
extern int opt_realtime_playing;
extern int reduce_voice_threshold; /* msec */
extern int check_eot_flag;
extern int special_tonebank;
extern int default_tonebank;
extern int playmidi_seek_flag;
extern int effect_lr_mode;
extern int effect_lr_delay_msec;
extern int auto_reduce_polyphony;
extern int play_pause_flag;
#if defined(LAGRANGE_INTERPOLATION) || defined(CSPLINE_INTERPOLATION) || defined(NEWTON_INTERPOLATION) || defined(GAUSS_INTERPOLATION)
extern int reduce_quality_flag;
extern int no_4point_interpolation;
#endif
extern ChannelBitMask channel_mute;
extern int temper_type_mute;
extern int8 current_keysig;
extern int8 current_temper_keysig;
extern int8 opt_init_keysig;
extern int8 opt_force_keysig;
extern int key_adjust;
extern int opt_pure_intonation;
extern int current_freq_table;
extern int32 opt_drum_power;
extern int opt_amp_compensation;
extern int opt_realtime_priority;	/* interface/alsaseq_c.c */
extern int opt_sequencer_ports;		/* interface/alsaseq_c.c */

extern int play_midi_file(char *fn);
extern void dumb_pass_playing_list(int number_of_files, char *list_of_files[]);
extern void default_ctl_lyric(int lyricid);
extern int check_apply_control(void);
extern void recompute_freq(int v);
extern int midi_drumpart_change(int ch, int isdrum);
extern void ctl_note_event(int noteID);
extern void ctl_mode_event(int type, int trace, long arg1, long arg2);
extern char *channel_instrum_name(int ch);
extern int get_reverb_level(int ch);
extern int get_chorus_level(int ch);
extern void playmidi_output_changed(int play_state);
extern Instrument *play_midi_load_instrument(int dr, int bk, int prog);
extern void midi_program_change(int ch, int prog);
extern void free_voice(int v);
extern void play_midi_setup_drums(int ch,int note);

/* For stream player */
extern void playmidi_stream_init(void);
extern void playmidi_tmr_reset(void);
extern int play_event(MidiEvent *ev);

extern void dup_tone_bank_element(int,int,int);
extern void free_tone_bank_element(int,int,int);

extern void recompute_voice_filter(int);

#endif /* ___PLAYMIDI_H_ */
