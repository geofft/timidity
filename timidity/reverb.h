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

/*
 * REVERB EFFECT FOR TIMIDITY++-1.X (Version 0.06e  1999/1/28)
 *
 * Copyright (C) 1997,1998,1999  Masaki Kiryu <mkiryu@usa.net>
 *                           (http://w3mb.kcom.ne.jp/~mkiryu/)
 *
 * reverb.h
 *
 */
#ifndef ___REVERB_H_
#define ___REVERB_H_

#define DEFAULT_REVERB_SEND_LEVEL 40

extern int opt_effect_quality;

extern void set_dry_signal(register int32 *, int32);
extern void mix_dry_signal(register int32 *, int32);

/* channel by channel reverberation effect */
extern void do_reverb(int32 *, int32);
extern void do_ch_reverb(int32 *, int32);
extern void set_ch_reverb(register int32 *, int32, int32);
extern void do_mono_reverb(int32 *, int32);
extern void init_reverb(int32);
extern void reverb_rc_event(int, int32);
extern void recompute_reverb_value(int32);

/* channel by channel delay effect */
extern void do_ch_delay(int32 *, int32);
extern void set_ch_delay(register int32 *, int32, int32);
extern void init_ch_delay();

/* channel by channel chorus effect */
extern void do_ch_chorus(int32 *, int32);
extern void set_ch_chorus(register int32 *, int32, int32);
extern void init_chorus_lfo();
extern void init_ch_chorus();

/* channel by channel equalizer */
extern void init_eq();
extern void set_ch_eq(register int32 *, int32);
extern void do_ch_eq(int32 *, int32);
extern void calc_lowshelf_coefs(int32*,int32,FLOAT_T,int32);
extern void calc_highshelf_coefs(int32*,int32,FLOAT_T,int32);

/* insertion effect */
extern void init_insertion_effect();
extern void do_insertion_effect(int32*, int32);

/* lowpass filter for system effects */
extern void do_lowpass_24db(register int32*,int32,int32*,int32*);
extern void calc_lowpass_coefs_24db(int32*,int32,int16,int32);

extern void free_effect_buffers(void);

/* GS parameters of delay effect */
struct delay_status_t
{
	/* GS parameters */
	uint8 type;
	uint8 level;
    uint8 level_center;
    uint8 level_left;
    uint8 level_right;
    int time_center;			/* in ms */
    double time_ratio_left;		/* in pct */
    double time_ratio_right;	/* in pct */
    uint8 feedback;
	uint8 pre_lpf;
	uint8 send_reverb;

	/* for pre-calculation */
	int32 sample_c;
	int32 sample_l;
	int32 sample_r;
	double level_ratio_c;
	double level_ratio_l;
	double level_ratio_r;
	double feedback_ratio;
	double send_reverb_ratio;
	int32 lpf_coef[5];
	int32 lpf_val[8];
};

/* GS parameters of reverb effect */
struct reverb_status_t
{
	/* GS parameters */
	uint8 character;
	uint8 pre_lpf;
	uint8 level;
	uint8 time;
	uint8 delay_feedback;
	uint8 pre_delay_time;	/* in ms */

	/* for pre-calculation */
	int32 lpf_coef[5];
	int32 lpf_val[8];
	double level_ratio;
	double time_ratio;
};

/* GS parameters of chorus effect */
struct chorus_param_t
{
	/* GS parameters */
	uint8 chorus_macro;
	uint8 chorus_pre_lpf;
	uint8 chorus_level;
	uint8 chorus_feedback;
	uint8 chorus_delay;
	uint8 chorus_rate;
	uint8 chorus_depth;
	uint8 chorus_send_level_to_reverb;
	uint8 chorus_send_level_to_delay;

	/* for pre-calculation */
	double level_ratio;
	double feedback_ratio;
	double send_reverb_ratio;
	double send_delay_ratio;
	int32 cycle_in_sample;
	int32 depth_in_sample;
	int32 delay_in_sample;
	int32 lpf_coef[5];
	int32 lpf_val[8];
};

/* GS parameters of channel EQ */
struct eq_status_t
{
	/* GS parameters */
    uint8 low_freq;
	uint8 high_freq;
	uint8 low_gain;
	uint8 high_gain;

	/* for pre-calculation */
	/* for highpass shelving filter */
	int32 high_coef[5];
	int32 high_val[8];
	/* for lowpass shelving filter */
	int32 low_coef[5];
	int32 low_val[8];
};

/* GS paramters of insertion effect */
struct insertion_effect_t
{
	/* GS parameters */
	int8 type_lsb;
	int8 type_msb;
	int32 type;
	int8 parameter[20];
	int8 send_reverb;
	int8 send_chorus;
	int8 send_delay;
	int8 control_source1;
	int8 control_depth1;
	int8 control_source2;
	int8 control_depth2;
	int8 send_eq_switch;

	/* EQ for insertion effect */
	int32 eq_low_freq;
	int32 eq_low_gain;
	int32 eq_low_coef[5];
	int32 eq_low_val[8];
	int32 eq_high_freq;
	int32 eq_high_gain;
	int32 eq_high_coef[5];
	int32 eq_high_val[8];
};

/* see also readmidi.c */
struct chorus_status_t
{
    int status;
    uint8 voice_reserve[18];
    uint8 macro[3];
    uint8 pre_lpf[3];
    uint8 level[3];
    uint8 feed_back[3];
    uint8 delay[3];
    uint8 rate[3];
    uint8 depth[3];
    uint8 send_level[3];
};

extern struct delay_status_t delay_status;
extern struct chorus_status_t chorus_status;
extern struct chorus_param_t chorus_param;
extern struct reverb_status_t reverb_status;
extern struct eq_status_t eq_status;
extern struct insertion_effect_t insertion_effect;

#endif /* ___REVERB_H_ */
