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

    mix.c
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "timidity.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "output.h"
#include "controls.h"
#include "tables.h"
#include "resample.h"
#include "mix.h"

#ifdef SMOOTH_MIXING
#ifdef LOOKUP_HACK
/* u2l: 0..255 -> -32256..32256
 * shift 3 bit: -> within MAX_AMP_VALUE
 */
#define FROM_FINAL_VOLUME(a) (_u2l[(uint8) ~(a)] >> 3)
#else
#define FROM_FINAL_VOLUME(a) (a)
#endif
#endif

#define OFFSET_MAX 0x3fc00000

#if OPT_MODE != 0
#define VOICE_LPF
#endif

#ifdef VOICE_LPF
static int32 a1, a2, b0, b1, b2, hist1, hist2, centernode;
static int8 filter_on;
#define MIXATION(a) \
	if (filter_on) { \
		centernode = (a) * s - imuldiv16(a1, hist1) - imuldiv16(a2, hist2); \
		*lp++ += imuldiv16(b0, centernode) \
				+ imuldiv16(b1, hist1) + imuldiv16(b2, hist2); \
		hist2 = hist1, hist1 = centernode; \
	} else \
		*lp++ += (a) * s
#else
#ifdef LOOKUP_HACK
#define MIXATION(a) *lp++ += mixup[(a << 8) | (uint8) s]
#else
#define MIXATION(a) *lp++ += (a) * s
#endif
#endif

void mix_voice(int32 *, int, int32);
#ifdef VOICE_LPF
static inline void set_voice_filter(int);
static inline void recalc_voice_resonance(int);
static inline void recalc_voice_fc(int);
#endif
static inline void ramp_out(sample_t *, int32 *, int, int32);
static inline void mix_mono_signal(sample_t *, int32 *, int, int);
static inline void mix_mono(sample_t *, int32 *, int, int);
static inline void mix_mystery_signal(sample_t *, int32 *, int, int);
static inline void mix_mystery(sample_t *, int32 *, int, int);
static inline void mix_center_signal(sample_t *, int32 *, int, int);
static inline void mix_center(sample_t *, int32 *, int, int);
static inline void mix_single_signal(sample_t *, int32 *, int, int);
static inline void mix_single(sample_t *, int32 *, int, int);
static inline int update_signal(int);
static inline int update_envelope(int);
int recompute_envelope(int);
static inline void voice_ran_out(int);
static inline int next_stage(int);
static inline void update_tremolo(int);
int apply_envelope_to_amp(int);
#ifdef SMOOTH_MIXING
static inline void compute_mix_smoothing(Voice *);
#endif
#ifdef VOICE_LPF
static inline void finish_voice_filter(int);
#endif

int min_sustain_time = 0;

/**************** interface function ****************/
void mix_voice(int32 *buf, int v, int32 c)
{
	Voice *vp = voice + v;
	sample_t *sp;
	
	if (vp->status == VOICE_DIE) {
		if (c >= MAX_DIE_TIME)
			c = MAX_DIE_TIME;
		sp = resample_voice(v, &c);
#ifdef VOICE_LPF
		set_voice_filter(v);
#endif
		if (c > 0)
			ramp_out(sp, buf, v, c);
		free_voice(v);
	} else {
		if (vp->delay) {
			if (c < vp->delay) {
				vp->delay -= c;
				return;
			}
			if (play_mode->encoding & PE_MONO)
				buf += vp->delay;
			else
				buf += vp->delay * 2;
			c -= vp->delay;
			vp->delay = 0;
		}
		sp = resample_voice(v, &c);
#ifdef VOICE_LPF
		set_voice_filter(v);
#endif
		if (play_mode->encoding & PE_MONO) {
			/* Mono output. */
			if (vp->envelope_increment || vp->tremolo_phase_increment)
				mix_mono_signal(sp, buf, v, c);
			else
				mix_mono(sp, buf, v, c);
		} else {
			if (vp->panned == PANNED_MYSTERY) {
				if (vp->envelope_increment || vp->tremolo_phase_increment)
					mix_mystery_signal(sp, buf, v, c);
				else
					mix_mystery(sp, buf, v, c);
			} else if (vp->panned == PANNED_CENTER) {
				if (vp->envelope_increment || vp->tremolo_phase_increment)
					mix_center_signal(sp, buf, v, c);
				else
					mix_center(sp, buf, v, c);
			} else {
				/* It's either full left or full right. In either case,
				 * every other sample is 0. Just get the offset right:
				 */
				if (vp->panned == PANNED_RIGHT)
					buf++;
				if (vp->envelope_increment || vp->tremolo_phase_increment)
					mix_single_signal(sp, buf, v, c);
				else
					mix_single(sp, buf, v, c);
			}
		}
	}
#ifdef VOICE_LPF
	finish_voice_filter(v);
#endif
}

#ifdef VOICE_LPF
static inline void set_voice_filter(int v)
{
	FilterCoefficients *fc = &(voice[v].fc);
	
	if(fc->freq == -1)
		filter_on = 0;
	else {
		recalc_voice_resonance(v);
		recalc_voice_fc(v);
		hist1 = fc->hist1, hist2 = fc->hist2;
		a1 = fc->a1, a2 = fc->a2;
		b0 = fc->b0, b1 = fc->b1, b2 = fc->b2;
		filter_on = 1;
	}
}

static inline void recalc_voice_resonance(int v)
{
	double reso_dB;
	FilterCoefficients *fc = &(voice[v].fc);
	
	reso_dB = fc->reso_dB;
	if (reso_dB != fc->last_reso_dB || fc->reso_lin == 0) {
		fc->last_reso_dB = reso_dB;
		fc->reso_lin = pow(10.0, reso_dB / 20);
		fc->filter_gain = 1 / sqrt(fc->reso_lin);
	}
}

static inline void recalc_voice_fc(int v)
{
	double freq, omega, cos_coef, sin_coef, alpha_coef;
	double a1, a2, b0, b1, b2;
	FilterCoefficients *fc = &(voice[v].fc);
	
	freq = fc->freq;
	if (freq != fc->last_freq) {
		fc->last_freq = freq;
		omega = 2 * M_PI * freq / play_mode->rate;
		cos_coef = cos(omega), sin_coef = sin(omega);
		alpha_coef = sin_coef / (2 * fc->reso_lin);
		a1 = -2 * cos_coef / (1 + alpha_coef);
		a2 = (1 - alpha_coef) / (1 + alpha_coef);
		b1 = (1 - cos_coef) / (1 + alpha_coef) * fc->filter_gain;
		b0 = b2 = b1 * 0.5f;
		fc->a1 = a1 * 0x10000, fc->a2 = a2 * 0x10000;
		fc->b0 = b0 * 0x10000, fc->b1 = b1 * 0x10000, fc->b2 = b2 * 0x10000;
	}
}
#endif

/* Ramp a note out in c samples */
static inline void ramp_out(sample_t *sp, int32 *lp, int v, int32 c)
{
	/* should be final_volume_t, but uint8 gives trouble. */
	int32 left, right, li, ri, i;
	/* silly warning about uninitialized s */
	sample_t s = 0;
	
	left = voice[v].left_mix;
	li = -(left / c);
	if (! li)
		li = -1;
#if 0
	printf("Ramping out: left=%d, c=%d, li=%d\n", left, c, li);
#endif
	if (! (play_mode->encoding & PE_MONO)) {
		if (voice[v].panned == PANNED_MYSTERY) {
			right = voice[v].right_mix;
			ri = -(right / c);
			for (i = 0; i < c; i++) {
				left += li;
				if (left < 0)
					left = 0;
				right += ri;
				if (right < 0)
					right = 0;
				s = *sp++;
				MIXATION(left);
				MIXATION(right);
			}
		} else if (voice[v].panned == PANNED_CENTER)
			for (i = 0; i < c; i++) {
				left += li;
				if (left < 0)
					return;
				s = *sp++;
				MIXATION(left);
				MIXATION(left);
			}
		else if (voice[v].panned == PANNED_LEFT)
			for (i = 0; i < c; i++) {
				left += li;
				if (left < 0)
					return;
				s = *sp++;
				MIXATION(left);
				lp++;
			}
		else if (voice[v].panned == PANNED_RIGHT)
			for (i = 0; i < c; i++) {
				left += li;
				if (left < 0)
					return;
				s = *sp++;
				lp++;
				MIXATION(left);
			}
	} else
		/* Mono output. */
		for (i = 0; i < c; i++) {
			left += li;
			if (left < 0)
				return;
			s = *sp++;
			MIXATION(left);
		}
}

static inline void mix_mono_signal(
		sample_t *sp, int32 *lp, int v, int count)
{
	Voice *vp = voice + v;
	final_volume_t left = vp->left_mix;
	int cc, i;
	sample_t s;
#ifdef SMOOTH_MIXING
	int32 linear_left;
#endif
	
	if (! (cc = vp->control_counter)) {
		cc = control_ratio;
		if (update_signal(v))
			/* Envelope ran out */
			return;
		left = vp->left_mix;
	}
#ifdef SMOOTH_MIXING
	compute_mix_smoothing(vp);
#endif
	while (count)
		if (cc < count) {
			count -= cc;
#ifdef SMOOTH_MIXING
			linear_left = FROM_FINAL_VOLUME(left);
			if (vp->left_mix_offset) {
				linear_left += vp->left_mix_offset;
				if (linear_left > MAX_AMP_VALUE) {
					linear_left = MAX_AMP_VALUE;
					vp->left_mix_offset = 0;
				}
				left = FINAL_VOLUME(linear_left);
			}
			for (i = 0; vp->left_mix_offset && i < cc; i++) {
				s = *sp++;
				MIXATION(left);
				vp->left_mix_offset += vp->left_mix_inc;
				linear_left += vp->left_mix_inc;
				if (linear_left > MAX_AMP_VALUE) {
					linear_left = MAX_AMP_VALUE;
					vp->left_mix_offset = 0;
				}
				left = FINAL_VOLUME(linear_left);
			}
			vp->old_left_mix = linear_left;
			cc -= i;
#endif
			for (i = 0; i < cc; i++) {
				s = *sp++;
				MIXATION(left);
			}
			cc = control_ratio;
			if (update_signal(v))
				/* Envelope ran out */
				return;
			left = vp->left_mix;
#ifdef SMOOTH_MIXING
			compute_mix_smoothing(vp);
#endif
		} else {
			vp->control_counter = cc - count;
#ifdef SMOOTH_MIXING
			linear_left = FROM_FINAL_VOLUME(left);
			if (vp->left_mix_offset) {
				linear_left += vp->left_mix_offset;
				if (linear_left > MAX_AMP_VALUE) {
					linear_left = MAX_AMP_VALUE;
					vp->left_mix_offset = 0;
				}
				left = FINAL_VOLUME(linear_left);
			}
			for (i = 0; vp->left_mix_offset && i < count; i++) {
				s = *sp++;
				MIXATION(left);
				vp->left_mix_offset += vp->left_mix_inc;
				linear_left += vp->left_mix_inc;
				if (linear_left > MAX_AMP_VALUE) {
					linear_left = MAX_AMP_VALUE;
					vp->left_mix_offset = 0;
				}
				left = FINAL_VOLUME(linear_left);
			}
			vp->old_left_mix = linear_left;
			count -= i;
#endif
			for (i = 0; i < count; i++) {
				s = *sp++;
				MIXATION(left);
			}
			return;
		}
}

static inline void mix_mono(sample_t *sp, int32 *lp, int v, int count)
{
	final_volume_t left = voice[v].left_mix;
	sample_t s;
	int i;
#ifdef SMOOTH_MIXING
	Voice *vp = voice + v;
	int32 linear_left;
#endif
	
#ifdef SMOOTH_MIXING
	compute_mix_smoothing(vp);
	linear_left = FROM_FINAL_VOLUME(left);
	if (vp->left_mix_offset) {
		linear_left += vp->left_mix_offset;
		if (linear_left > MAX_AMP_VALUE) {
			linear_left = MAX_AMP_VALUE;
			vp->left_mix_offset = 0;
		}
		left = FINAL_VOLUME(linear_left);
	}
	for (i = 0; vp->left_mix_offset && i < count; i++) {
		s = *sp++;
		MIXATION(left);
		MIXATION(left);
		vp->left_mix_offset += vp->left_mix_inc;
		linear_left += vp->left_mix_inc;
		if (linear_left > MAX_AMP_VALUE) {
			linear_left = MAX_AMP_VALUE;
			vp->left_mix_offset = 0;
		}
		left = FINAL_VOLUME(linear_left);
	}
	vp->old_left_mix = linear_left;
	count -= i;
#endif
	for (i = 0; i < count; i++) {
		s = *sp++;
		MIXATION(left);
	}
}

static inline void mix_mystery_signal(
		sample_t *sp, int32 *lp, int v, int count)
{
	Voice *vp = voice + v;
	final_volume_t left = vp->left_mix, right = vp->right_mix;
	int cc, i;
	sample_t s;
#ifdef SMOOTH_MIXING
	int32 linear_left, linear_right;
#endif
	
	if (! (cc = vp->control_counter)) {
		cc = control_ratio;
		if (update_signal(v))
			/* Envelope ran out */
			return;
		left = vp->left_mix;
		right = vp->right_mix;
	}
#ifdef SMOOTH_MIXING
	compute_mix_smoothing(vp);
#endif
	while (count)
		if (cc < count) {
			count -= cc;
#ifdef SMOOTH_MIXING
			linear_left = FROM_FINAL_VOLUME(left);
			if (vp->left_mix_offset) {
				linear_left += vp->left_mix_offset;
				if (linear_left > MAX_AMP_VALUE) {
					linear_left = MAX_AMP_VALUE;
					vp->left_mix_offset = 0;
				}
				left = FINAL_VOLUME(linear_left);
			}
			linear_right = FROM_FINAL_VOLUME(right);
			if (vp->right_mix_offset) {
				linear_right += vp->right_mix_offset;
				if (linear_right > MAX_AMP_VALUE) {
					linear_right = MAX_AMP_VALUE;
					vp->right_mix_offset = 0;
				}
				right = FINAL_VOLUME(linear_right);
			}
			for (i = 0; (vp->left_mix_offset | vp->right_mix_offset)
					&& i < cc; i++) {
				s = *sp++;
				MIXATION(left);
				MIXATION(right);
				if (vp->left_mix_offset) {
					vp->left_mix_offset += vp->left_mix_inc;
					linear_left += vp->left_mix_inc;
					if (linear_left > MAX_AMP_VALUE) {
						linear_left = MAX_AMP_VALUE;
						vp->left_mix_offset = 0;
					}
					left = FINAL_VOLUME(linear_left);
				}
				if (vp->right_mix_offset) {
					vp->right_mix_offset += vp->right_mix_inc;
					linear_right += vp->right_mix_inc;
					if (linear_right > MAX_AMP_VALUE) {
						linear_right = MAX_AMP_VALUE;
						vp->right_mix_offset = 0;
					}
					right = FINAL_VOLUME(linear_right);
				}
			}
			vp->old_left_mix = linear_left;
			vp->old_right_mix = linear_right;
			cc -= i;
#endif
			for (i = 0; i < cc; i++) {
				s = *sp++;
				MIXATION(left);
				MIXATION(right);
			}
			cc = control_ratio;
			if (update_signal(v))
				/* Envelope ran out */
				return;
			left = vp->left_mix;
			right = vp->right_mix;
#ifdef SMOOTH_MIXING
			compute_mix_smoothing(vp);
#endif
		} else {
			vp->control_counter = cc - count;
#ifdef SMOOTH_MIXING
			linear_left = FROM_FINAL_VOLUME(left);
			if (vp->left_mix_offset) {
				linear_left += vp->left_mix_offset;
				if (linear_left > MAX_AMP_VALUE) {
					linear_left = MAX_AMP_VALUE;
					vp->left_mix_offset = 0;
				}
				left = FINAL_VOLUME(linear_left);
			}
			linear_right = FROM_FINAL_VOLUME(right);
			if (vp->right_mix_offset) {
				linear_right += vp->right_mix_offset;
				if (linear_right > MAX_AMP_VALUE) {
					linear_right = MAX_AMP_VALUE;
					vp->right_mix_offset = 0;
				}
				right = FINAL_VOLUME(linear_right);
			}
			for (i = 0; (vp->left_mix_offset | vp->right_mix_offset)
					&& i < count; i++) {
				s = *sp++;
				MIXATION(left);
				MIXATION(right);
				if (vp->left_mix_offset) {
					vp->left_mix_offset += vp->left_mix_inc;
					linear_left += vp->left_mix_inc;
					if (linear_left > MAX_AMP_VALUE) {
						linear_left = MAX_AMP_VALUE;
						vp->left_mix_offset = 0;
					}
					left = FINAL_VOLUME(linear_left);
				}
				if (vp->right_mix_offset) {
					vp->right_mix_offset += vp->right_mix_inc;
					linear_right += vp->right_mix_inc;
					if (linear_right > MAX_AMP_VALUE) {
						linear_right = MAX_AMP_VALUE;
						vp->right_mix_offset = 0;
					}
					right = FINAL_VOLUME(linear_right);
				}
			}
			vp->old_left_mix = linear_left;
			vp->old_right_mix = linear_right;
			count -= i;
#endif
			for (i = 0; i < count; i++) {
				s = *sp++;
				MIXATION(left);
				MIXATION(right);
			}
			return;
		}
}

static inline void mix_mystery(sample_t *sp, int32 *lp, int v, int count)
{
	final_volume_t left = voice[v].left_mix, right = voice[v].right_mix;
	sample_t s;
	int i;
#ifdef SMOOTH_MIXING
	Voice *vp = voice + v;
	int32 linear_left, linear_right;
#endif
	
#ifdef SMOOTH_MIXING
	compute_mix_smoothing(vp);
	linear_left = FROM_FINAL_VOLUME(left);
	if (vp->left_mix_offset) {
		linear_left += vp->left_mix_offset;
		if (linear_left > MAX_AMP_VALUE) {
			linear_left = MAX_AMP_VALUE;
			vp->left_mix_offset = 0;
		}
		left = FINAL_VOLUME(linear_left);
	}
	linear_right = FROM_FINAL_VOLUME(right);
	if (vp->right_mix_offset) {
		linear_right += vp->right_mix_offset;
		if (linear_right > MAX_AMP_VALUE) {
			linear_right = MAX_AMP_VALUE;
			vp->right_mix_offset = 0;
		}
		right = FINAL_VOLUME(linear_right);
	}
	for (i = 0; (vp->left_mix_offset | vp->right_mix_offset)
			&& i < count; i++) {
		s = *sp++;
		MIXATION(left);
		MIXATION(right);
		if (vp->left_mix_offset) {
			vp->left_mix_offset += vp->left_mix_inc;
			linear_left += vp->left_mix_inc;
			if (linear_left > MAX_AMP_VALUE) {
				linear_left = MAX_AMP_VALUE;
				vp->left_mix_offset = 0;
			}
			left = FINAL_VOLUME(linear_left);
		}
		if (vp->right_mix_offset) {
			vp->right_mix_offset += vp->right_mix_inc;
			linear_right += vp->right_mix_inc;
			if (linear_right > MAX_AMP_VALUE) {
				linear_right = MAX_AMP_VALUE;
				vp->right_mix_offset = 0;
			}
			right = FINAL_VOLUME(linear_right);
		}
	}
	vp->old_left_mix = linear_left;
	vp->old_right_mix = linear_right;
	count -= i;
#endif
	for (i = 0; i < count; i++) {
		s = *sp++;
		MIXATION(left);
		MIXATION(right);
	}
}

static inline void mix_center_signal(
		sample_t *sp, int32 *lp, int v, int count)
{
	Voice *vp = voice + v;
	final_volume_t left=vp->left_mix;
	int cc, i;
	sample_t s;
#ifdef SMOOTH_MIXING
	int32 linear_left;
#endif
	
	if (! (cc = vp->control_counter)) {
		cc = control_ratio;
		if (update_signal(v))
			/* Envelope ran out */
			return;
		left = vp->left_mix;
	}
#ifdef SMOOTH_MIXING
	compute_mix_smoothing(vp);
#endif
	while (count)
		if (cc < count) {
			count -= cc;
#ifdef SMOOTH_MIXING
			linear_left = FROM_FINAL_VOLUME(left);
			if (vp->left_mix_offset) {
				linear_left += vp->left_mix_offset;
				if (linear_left > MAX_AMP_VALUE) {
					linear_left = MAX_AMP_VALUE;
					vp->left_mix_offset = 0;
				}
				left = FINAL_VOLUME(linear_left);
			}
			for (i = 0; vp->left_mix_offset && i < cc; i++) {
				s = *sp++;
				MIXATION(left);
				MIXATION(left);
				vp->left_mix_offset += vp->left_mix_inc;
				linear_left += vp->left_mix_inc;
				if (linear_left > MAX_AMP_VALUE) {
					linear_left = MAX_AMP_VALUE;
					vp->left_mix_offset = 0;
				}
				left = FINAL_VOLUME(linear_left);
			}
			vp->old_left_mix = linear_left;
			cc -= i;
#endif
			for (i = 0; i < cc; i++) {
				s = *sp++;
				MIXATION(left);
				MIXATION(left);
			}
			cc = control_ratio;
			if (update_signal(v))
				/* Envelope ran out */
				return;
			left = vp->left_mix;
#ifdef SMOOTH_MIXING
			compute_mix_smoothing(vp);
#endif
		} else {
			vp->control_counter = cc - count;
#ifdef SMOOTH_MIXING
			linear_left = FROM_FINAL_VOLUME(left);
			if (vp->left_mix_offset) {
				linear_left += vp->left_mix_offset;
				if (linear_left > MAX_AMP_VALUE) {
					linear_left = MAX_AMP_VALUE;
					vp->left_mix_offset = 0;
				}
				left = FINAL_VOLUME(linear_left);
			}
			for (i = 0; vp->left_mix_offset && i < count; i++) {
				s = *sp++;
				MIXATION(left);
				MIXATION(left);
				vp->left_mix_offset += vp->left_mix_inc;
				linear_left += vp->left_mix_inc;
				if (linear_left > MAX_AMP_VALUE) {
					linear_left = MAX_AMP_VALUE;
					vp->left_mix_offset = 0;
				}
				left = FINAL_VOLUME(linear_left);
			}
			vp->old_left_mix = linear_left;
			count -= i;
#endif
			for (i = 0; i < count; i++) {
				s = *sp++;
				MIXATION(left);
				MIXATION(left);
			}
			return;
		}
}

static inline void mix_center(sample_t *sp, int32 *lp, int v, int count)
{
	final_volume_t left = voice[v].left_mix;
	sample_t s;
	int i;
#ifdef SMOOTH_MIXING
	Voice *vp = voice + v;
	int32 linear_left;
#endif
	
#ifdef SMOOTH_MIXING
	compute_mix_smoothing(vp);
	linear_left = FROM_FINAL_VOLUME(left);
	if (vp->left_mix_offset) {
		linear_left += vp->left_mix_offset;
		if (linear_left > MAX_AMP_VALUE) {
			linear_left = MAX_AMP_VALUE;
			vp->left_mix_offset = 0;
		}
		left = FINAL_VOLUME(linear_left);
	}
	for (i = 0; vp->left_mix_offset && i < count; i++) {
		s = *sp++;
		MIXATION(left);
		MIXATION(left);
		vp->left_mix_offset += vp->left_mix_inc;
		linear_left += vp->left_mix_inc;
		if (linear_left > MAX_AMP_VALUE) {
			linear_left = MAX_AMP_VALUE;
			vp->left_mix_offset = 0;
		}
		left = FINAL_VOLUME(linear_left);
	}
	vp->old_left_mix = linear_left;
	count -= i;
#endif
	for (i = 0; i < count; i++) {
		s = *sp++;
		MIXATION(left);
		MIXATION(left);
	}
}

static inline void mix_single_signal(
		sample_t *sp, int32 *lp, int v, int count)
{
	Voice *vp = voice + v;
	final_volume_t left = vp->left_mix;
	int cc, i;
	sample_t s;
#ifdef SMOOTH_MIXING
	int32 linear_left;
#endif
	
	if (!(cc = vp->control_counter)) {
		cc = control_ratio;
		if (update_signal(v))
			/* Envelope ran out */
			return;
		left = vp->left_mix;
	}
#ifdef SMOOTH_MIXING
	compute_mix_smoothing(vp);
#endif
	while (count)
		if (cc < count) {
			count -= cc;
#ifdef SMOOTH_MIXING
			linear_left = FROM_FINAL_VOLUME(left);
			if (vp->left_mix_offset) {
				linear_left += vp->left_mix_offset;
				if (linear_left > MAX_AMP_VALUE) {
					linear_left = MAX_AMP_VALUE;
					vp->left_mix_offset = 0;
				}
				left = FINAL_VOLUME(linear_left);
			}
			for (i = 0; vp->left_mix_offset && i < cc; i++) {
				s = *sp++;
				MIXATION(left);
				lp++;
				vp->left_mix_offset += vp->left_mix_inc;
				linear_left += vp->left_mix_inc;
				if (linear_left > MAX_AMP_VALUE) {
					linear_left = MAX_AMP_VALUE;
					vp->left_mix_offset = 0;
				}
				left = FINAL_VOLUME(linear_left);
			}
			vp->old_left_mix = linear_left;
			cc -= i;
#endif
			for (i = 0; i < cc; i++) {
				s = *sp++;
				MIXATION(left);
				lp++;
			}
			cc = control_ratio;
			if (update_signal(v))
				/* Envelope ran out */
				return;
			left = vp->left_mix;
#ifdef SMOOTH_MIXING
			compute_mix_smoothing(vp);
#endif
		} else {
			vp->control_counter = cc - count;
#ifdef SMOOTH_MIXING
			linear_left = FROM_FINAL_VOLUME(left);
			if (vp->left_mix_offset) {
				linear_left += vp->left_mix_offset;
				if (linear_left > MAX_AMP_VALUE) {
					linear_left = MAX_AMP_VALUE;
					vp->left_mix_offset = 0;
				}
				left = FINAL_VOLUME(linear_left);
			}
			for (i = 0; vp->left_mix_offset && i < count; i++) {
				s = *sp++;
				MIXATION(left);
				lp++;
				vp->left_mix_offset += vp->left_mix_inc;
				linear_left += vp->left_mix_inc;
				if (linear_left > MAX_AMP_VALUE) {
					linear_left = MAX_AMP_VALUE;
					vp->left_mix_offset = 0;
				}
				left = FINAL_VOLUME(linear_left);
			}
			vp->old_left_mix = linear_left;
			count -= i;
#endif
			for (i = 0; i < count; i++) {
				s = *sp++;
				MIXATION(left);
				lp++;
			}
			return;
		}
}

static inline void mix_single(sample_t *sp, int32 *lp, int v, int count)
{
	final_volume_t left = voice[v].left_mix;
	sample_t s;
	int i;
#ifdef SMOOTH_MIXING
	Voice *vp = voice + v;
	int32 linear_left;
#endif
	
#ifdef SMOOTH_MIXING
	compute_mix_smoothing(vp);
	linear_left = FROM_FINAL_VOLUME(left);
	if (vp->left_mix_offset) {
		linear_left += vp->left_mix_offset;
		if (linear_left > MAX_AMP_VALUE) {
			linear_left = MAX_AMP_VALUE;
			vp->left_mix_offset = 0;
		}
		left = FINAL_VOLUME(linear_left);
	}
	for (i = 0; vp->left_mix_offset && i < count; i++) {
		s = *sp++;
		MIXATION(left);
		lp++;
		vp->left_mix_offset += vp->left_mix_inc;
		linear_left += vp->left_mix_inc;
		if (linear_left > MAX_AMP_VALUE) {
			linear_left = MAX_AMP_VALUE;
			vp->left_mix_offset = 0;
		}
		left = FINAL_VOLUME(linear_left);
	}
	vp->old_left_mix = linear_left;
	count -= i;
#endif
	for (i = 0; i < count; i++) {
		s = *sp++;
		MIXATION(left);
		lp++;
	}
}

/* Returns 1 if the note died */
static inline int update_signal(int v)
{
	if (voice[v].envelope_increment && update_envelope(v))
		return 1;
	if (voice[v].tremolo_phase_increment)
		update_tremolo(v);
	return apply_envelope_to_amp(v);
}

static inline int update_envelope(int v)
{
	Voice *vp = &voice[v];
	
	vp->envelope_volume += vp->envelope_increment;
	if ((vp->envelope_increment < 0)
			^ (vp->envelope_volume > vp->envelope_target)) {
		vp->envelope_volume = vp->envelope_target;
		if (recompute_envelope(v))
			return 1;
	}
	return 0;
}

/* Returns 1 if envelope runs out */
int recompute_envelope(int v)
{
	int stage, ch;
	int32 rate;
	Voice *vp = &voice[v];
	
	stage = vp->envelope_stage;
	if (stage > 5) {
		voice_ran_out(v);
		return 1;
	}
	if (stage > 2 && vp->envelope_volume <= 0) {
		/* Remove silent voice in the release stage */
		voice_ran_out(v);
		return 1;
	}
	/* EAW -- Routine to decay the sustain envelope
	 *
	 * Disabled if !min_sustain_time or if there is no loop.
	 * If calculated decay rate is larger than the regular
	 *  stage 3 rate, use the stage 3 rate instead.
	 * min_sustain_time is given in msec, and is the time
	 *  it will take to decay a note at maximum volume.
	 * 2000-3000 msec seem to be decent values to use.
	 *
	 * 08/24/00 changed behavior to not begin the decay until
	 *  after the sample plays past it's loop start
	 *
	 */
	if (stage == 3 && vp->sample->modes & MODES_ENVELOPE
			&& vp->status & (VOICE_ON | VOICE_SUSTAINED)) {
		/* Default behavior */
		if (min_sustain_time <= 0)
			/* Freeze envelope until note turns off */
			vp->envelope_increment = 0;
		else if (vp->status & VOICE_SUSTAINED
				&& vp->sample->modes & MODES_LOOPING
				&& vp->sample_offset - vp->sample->loop_start >= 0) {
			if (min_sustain_time == 1)
				/* The sustain stage is ignored. */
				return next_stage(v);
			/* Calculate the release phase speed. */
			rate = -0x3fffffff * (double) control_ratio * 1000
					/ (min_sustain_time * play_mode->rate);
			ch = vp->channel;
			vp->envelope_increment = -vp->sample->envelope_rate[2];
			/* use the slower of the two rates */
			if (vp->envelope_increment < rate)
				vp->envelope_increment = rate;
			if (! vp->envelope_increment)
				/* Avoid freezing */
				vp->envelope_increment = -1;
			vp->envelope_target = 0;
		/* it's not decaying, so freeze it */
		} else {
			/* tiny value to make other functions happy, freeze note */
			vp->envelope_increment = 1;
			/* this will cause update_envelope(v) to undo the +1 inc. */
			vp->envelope_target = vp->envelope_volume;
		}
		return 0;
	}
	return next_stage(v);
}

/* Envelope ran out. */
static inline void voice_ran_out(int v)
{
	/* Already displayed as dead */
	int died = (voice[v].status == VOICE_DIE);
	
	free_voice(v);
	if (! died)
		ctl_note_event(v);
}

static inline int next_stage(int v)
{
	int stage, ch;
	int32 offset, val, rate;
	FLOAT_T tmp;
	Voice *vp = &voice[v];
	
	stage = vp->envelope_stage++;
	offset = vp->sample->envelope_offset[stage];
	if (vp->envelope_volume == offset
			|| stage > 2 && vp->envelope_volume < offset)
		return recompute_envelope(v);
	ch = vp->channel;
	tmp = vp->sample->envelope_rate[stage];
	if (vp->sample->modes & MODES_ENVELOPE) {
		if (vp->sample->inst_type == INST_SF2 && stage == 3)
			tmp *= (double) vp->envelope_volume
					/ vp->sample->envelope_offset[0];
		if (ISDRUMCHANNEL(ch))
			val = (channel[ch].drums[vp->note] != NULL)
					? channel[ch].drums[vp->note]->drum_envelope_rate[stage]
					: -1;
		else {
			/* envelope key-follow */
			if (vp->sample->envelope_keyf[stage])
				tmp *= pow(2.0, (double) (voice[v].note - 60)
						* vp->sample->envelope_keyf[stage]);
			val = channel[ch].envelope_rate[stage];
		}
		/* envelope velocity-follow */
		if (vp->sample->envelope_velf[stage])
			tmp *= pow(2.0, (double) (voice[v].velocity - 64)
					* vp->sample->envelope_velf[stage]);
		if (val != -1)
			tmp *= envelope_coef[val & 0x7f];
		if (tmp > ((stage < 2) ? 4 : 2) * 0x10000000)
			tmp = ((stage < 2) ? 4 : 2) * 0x10000000;
	}
	rate = tmp;
	vp->envelope_increment = rate;
	vp->envelope_target = offset;
	if (vp->envelope_target < vp->envelope_volume)
		vp->envelope_increment = -vp->envelope_increment;
	if(stage > 2) {
		vp->envelope_scale = vp->last_envelope_volume;
		vp->inv_envelope_scale = OFFSET_MAX
				/ (double) vp->envelope_volume * 0x10000;
	}
	return 0;
}

static inline void update_tremolo(int v)
{
	uint32 depth = voice[v].sample->tremolo_depth << 7;
	
	if (voice[v].tremolo_sweep) {
		/* Update sweep position */
		voice[v].tremolo_sweep_position += voice[v].tremolo_sweep;
		if (voice[v].tremolo_sweep_position >= 1 << SWEEP_SHIFT)
			/* Swept to max amplitude */
			voice[v].tremolo_sweep = 0;
		else {
			/* Need to adjust depth */
			depth *= voice[v].tremolo_sweep_position;
			depth >>= SWEEP_SHIFT;
		}
	}
	voice[v].tremolo_phase += voice[v].tremolo_phase_increment;
#if 0
	if (voice[v].tremolo_phase >= SINE_CYCLE_LENGTH << RATE_SHIFT)
		voice[v].tremolo_phase -= SINE_CYCLE_LENGTH << RATE_SHIFT;
#endif
	voice[v].tremolo_volume = 1.0 - TIM_FSCALENEG(
			(lookup_sine(voice[v].tremolo_phase >> RATE_SHIFT) + 1.0)
			* depth * TREMOLO_AMPLITUDE_TUNING, 17);
	/* I'm not sure about the +1.0 there -- it makes tremoloed voices'
	 *  volumes on average the lower the higher the tremolo amplitude.
	 */
}

int apply_envelope_to_amp(int v)
{
	FLOAT_T lamp = voice[v].left_amp, ramp;
	FLOAT_T *v_table = (voice[v].sample->inst_type == INST_SF2)
			? sb_vol_table : vol_table;
	int32 la, ra;
	
	if (voice[v].panned == PANNED_MYSTERY) {
		ramp = voice[v].right_amp;
		if (voice[v].tremolo_phase_increment) {
			lamp *= voice[v].tremolo_volume;
			ramp *= voice[v].tremolo_volume;
		}
		if (voice[v].sample->modes & MODES_ENVELOPE) {
			if (voice[v].envelope_stage > 3)
				voice[v].last_envelope_volume = v_table[
						imuldiv16(voice[v].envelope_volume >> 23,
						voice[v].inv_envelope_scale)]
						* voice[v].envelope_scale;
			else if (voice[v].envelope_stage > 1)
				voice[v].last_envelope_volume = v_table[
						voice[v].envelope_volume >> 23];
			else
				voice[v].last_envelope_volume = attack_vol_table[
						voice[v].envelope_volume >> 23];
			lamp *= voice[v].last_envelope_volume;
			ramp *= voice[v].last_envelope_volume;
		}
		la = TIM_FSCALE(lamp, AMP_BITS);
		if (la > MAX_AMP_VALUE)
			la = MAX_AMP_VALUE;
		ra = TIM_FSCALE(ramp, AMP_BITS);
		if (ra > MAX_AMP_VALUE)
			ra = MAX_AMP_VALUE;
		if ((voice[v].status & (VOICE_OFF | VOICE_SUSTAINED))
				&& (la | ra) <= 0) { /* <= MIN_AMP_VALUE */
			free_voice(v);
			ctl_note_event(v);
			return 1;
		}
		voice[v].left_mix = FINAL_VOLUME(la);
		voice[v].right_mix = FINAL_VOLUME(ra);
	} else {
		if (voice[v].tremolo_phase_increment)
			lamp *= voice[v].tremolo_volume;
		if (voice[v].sample->modes & MODES_ENVELOPE) {
			if (voice[v].envelope_stage > 3)
				voice[v].last_envelope_volume = v_table[
						imuldiv16(voice[v].envelope_volume >> 23,
						voice[v].inv_envelope_scale)]
						* voice[v].envelope_scale;
			else if (voice[v].envelope_stage > 1)
				voice[v].last_envelope_volume = v_table[
						voice[v].envelope_volume >> 23];
			else
				voice[v].last_envelope_volume = attack_vol_table[
						voice[v].envelope_volume >> 23];
			lamp *= voice[v].last_envelope_volume;
		}
		la = TIM_FSCALE(lamp, AMP_BITS);
		if (la > MAX_AMP_VALUE)
		la = MAX_AMP_VALUE;
		if ((voice[v].status & (VOICE_OFF | VOICE_SUSTAINED))
				&& la <= 0) { /* <= MIN_AMP_VALUE */
			free_voice(v);
			ctl_note_event(v);
			return 1;
		}
		voice[v].left_mix = FINAL_VOLUME(la);
	}
	return 0;
}

#ifdef SMOOTH_MIXING
static inline void compute_mix_smoothing(Voice *vp)
{
	int32 max_win, delta;
	
	/* reduce popping -- ramp the amp over a <= 0.5 msec window */
	max_win = play_mode->rate * 0.0005;
	delta = FROM_FINAL_VOLUME(vp->left_mix) - vp->old_left_mix;
	if (labs(delta) > max_win) {
		vp->left_mix_inc = delta / max_win;
		vp->left_mix_offset = vp->left_mix_inc * (1 - max_win);
	} else if (delta) {
		vp->left_mix_inc = -1;
		if (delta > 0)
			vp->left_mix_inc = 1;
		vp->left_mix_offset = vp->left_mix_inc - delta;
	}
	delta = FROM_FINAL_VOLUME(vp->right_mix) - vp->old_right_mix;
	if (labs(delta) > max_win) {
		vp->right_mix_inc = delta / max_win;
		vp->right_mix_offset = vp->right_mix_inc * (1 - max_win);
	} else if (delta) {
		vp->right_mix_inc = -1;
		if (delta > 0)
			vp->right_mix_inc = 1;
		vp->right_mix_offset = vp->right_mix_inc - delta;
	}
}
#endif

#ifdef VOICE_LPF
static inline void finish_voice_filter(int v)
{
	FilterCoefficients *fc = &(voice[v].fc);
	
	fc->hist1 = hist1, fc->hist2 = hist2;
}
#endif

