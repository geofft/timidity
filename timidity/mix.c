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

#define OFFSET_MAX (0x3FFFFFFF)

#if OPT_MODE != 0
#define VOICE_LPF
#endif

#ifdef VOICE_LPF
typedef int32 mix_t;
#else
typedef sample_t mix_t;
#endif

#ifdef LOOKUP_HACK
#define MIXATION(a) *lp++ += mixup[(a << 8) | (uint8) s]
#else
#define MIXATION(a) *lp++ += (a) * s
#endif

void mix_voice(int32 *, int, int32);
#ifdef VOICE_LPF
static inline void do_voice_filter(int, sample_t*, mix_t*, int32);
static inline void recalc_voice_resonance(int);
static inline void recalc_voice_fc(int);
#endif
static inline void ramp_out(mix_t *, int32 *, int, int32);
static inline void mix_mono_signal(mix_t *, int32 *, int, int);
static inline void mix_mono(mix_t *, int32 *, int, int);
static inline void mix_mystery_signal(mix_t *, int32 *, int, int);
static inline void mix_mystery(mix_t *, int32 *, int, int);
static inline void mix_center_signal(mix_t *, int32 *, int, int);
static inline void mix_center(mix_t *, int32 *, int, int);
static inline void mix_single_signal(mix_t *, int32 *, int, int);
static inline void mix_single(mix_t *, int32 *, int, int);
static inline int update_signal(int);
static inline int update_envelope(int);
int recompute_envelope(int);
static inline int update_modulation_envelope(int);
int apply_modulation_envelope(int);
int recompute_modulation_envelope(int);
static inline void voice_ran_out(int);
static inline int next_stage(int);
static inline void update_tremolo(int);
int apply_envelope_to_amp(int);
#ifdef SMOOTH_MIXING
static inline void compute_mix_smoothing(Voice *);
#endif

int min_sustain_time = 5000;

/**************** interface function ****************/
void mix_voice(int32 *buf, int v, int32 c)
{
	Voice *vp = voice + v;
	sample_t *sp;
#ifdef VOICE_LPF
	static mix_t lp[AUDIO_BUFFER_SIZE];
#else
	mix_t *lp;
#endif

	vp->delay_counter = c;

	if (vp->status == VOICE_DIE) {
		if (c >= MAX_DIE_TIME)
			c = MAX_DIE_TIME;
		sp = resample_voice(v, &c);
#ifdef VOICE_LPF
		do_voice_filter(v, sp, lp, c);
#else
		lp = sp;
#endif
		if (c > 0)
			ramp_out(lp, buf, v, c);
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
		do_voice_filter(v, sp, lp, c);
#else
		lp = sp;
#endif

		if (play_mode->encoding & PE_MONO) {
			/* Mono output. */
			if (vp->envelope_increment || vp->tremolo_phase_increment)
				mix_mono_signal(lp, buf, v, c);
			else
				mix_mono(lp, buf, v, c);
		} else {
			if (vp->panned == PANNED_MYSTERY) {
				if (vp->envelope_increment || vp->tremolo_phase_increment)
					mix_mystery_signal(lp, buf, v, c);
				else
					mix_mystery(lp, buf, v, c);
			} else if (vp->panned == PANNED_CENTER) {
				if (vp->envelope_increment || vp->tremolo_phase_increment)
					mix_center_signal(lp, buf, v, c);
				else
					mix_center(lp, buf, v, c);
			} else {
				/* It's either full left or full right. In either case,
				 * every other sample is 0. Just get the offset right:
				 */
				if (vp->panned == PANNED_RIGHT)
					buf++;
				if (vp->envelope_increment || vp->tremolo_phase_increment)
					mix_single_signal(lp, buf, v, c);
				else
					mix_single(lp, buf, v, c);
			}
		}
	}
}

#ifdef VOICE_LPF
#define FILTER_TRANSITION_SAMPLES (control_ratio)
static inline void do_voice_filter(int v, sample_t *sp, mix_t *lp, int32 count)
{
	FilterCoefficients *fc = &(voice[v].fc);
	int32 a1, a2, b02, b1, hist1, hist2, centernode, i;
	int32 filter_coeff_incr_count = fc->filter_coeff_incr_count;
	int32 a1_incr, a2_incr, b02_incr, b1_incr;
	
	if(fc->freq == -1) {
		for(i=0;i<count;i++) {
			lp[i] = sp[i];
		}
		return;
	} else if(filter_coeff_incr_count > 0) {
		recalc_voice_resonance(v);
		recalc_voice_fc(v);
		hist1 = fc->hist1, hist2 = fc->hist2, a1 = fc->a1,
			a2 = fc->a2, b02 = fc->b02, b1 = fc->b1,
			a1_incr = fc->a1_incr, a2_incr = fc->a2_incr,
			b02_incr = fc->b02_incr, b1_incr = fc->b1_incr;
		if(filter_coeff_incr_count > count) {
			filter_coeff_incr_count = count;
		}
		for(i=0;i<filter_coeff_incr_count;i++) {
			centernode = sp[i] - imuldiv24(a1, hist1) - imuldiv24(a2, hist2);
			lp[i] = imuldiv24(b02, centernode + hist2)
				+ imuldiv24(b1, hist1);
			hist2 = hist1, hist1 = centernode;

			a1 += a1_incr;
			a2 += a2_incr;
			b02 += b02_incr;
			b1 += b1_incr;
		}
		for(i=filter_coeff_incr_count;i<count;i++) {
			centernode = sp[i] - imuldiv24(a1, hist1) - imuldiv24(a2, hist2);
			lp[i] = imuldiv24(b02, centernode + hist2)
				+ imuldiv24(b1, hist1);
			hist2 = hist1, hist1 = centernode;
		}
		fc->hist1 = hist1, fc->hist2 = hist2;
		fc->a1 = a1, fc->a2 = a2, fc->b02 = b02, fc->b1 = b1,
		fc->filter_coeff_incr_count -= filter_coeff_incr_count;
		return;
	} else {
		recalc_voice_resonance(v);
		recalc_voice_fc(v);
		hist1 = fc->hist1, hist2 = fc->hist2, a1 = fc->a1,
			a2 = fc->a2, b02 = fc->b02, b1 = fc->b1;
		for(i=0;i<count;i++) {
			centernode = sp[i] - imuldiv24(a1, hist1) - imuldiv24(a2, hist2);
			lp[i] = imuldiv24(b02, centernode + hist2)
				+ imuldiv24(b1, hist1);
			hist2 = hist1, hist1 = centernode;
		}
		fc->hist1 = hist1, fc->hist2 = hist2;
		return;
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
		fc->last_freq = -1;
	}
}

static inline void recalc_voice_fc(int v)
{
	double freq, omega, cos_coef, sin_coef, alpha_coef;
	double a1, a2, b02, b1;
	FilterCoefficients *fc = &(voice[v].fc);
	
	freq = fc->freq;
	if (freq != fc->last_freq) {
		omega = 2 * M_PI * freq / play_mode->rate;
		cos_coef = cos(omega), sin_coef = sin(omega);
		alpha_coef = sin_coef / (2 * fc->reso_lin);
		a1 = -2 * cos_coef / (1 + alpha_coef);
		a2 = (1 - alpha_coef) / (1 + alpha_coef);
		b1 = (1 - cos_coef) / (1 + alpha_coef) * fc->filter_gain;
		b02 = b1 * 0.5f;
		if(fc->filter_calculated) {
			fc->a1_incr = (TIM_FSCALE(a1, 24) - fc->a1) / FILTER_TRANSITION_SAMPLES;
			fc->a2_incr = (TIM_FSCALE(a2, 24) - fc->a2) / FILTER_TRANSITION_SAMPLES;
			fc->b1_incr = (TIM_FSCALE(b1, 24) - fc->b1) / FILTER_TRANSITION_SAMPLES;
			fc->b02_incr = (TIM_FSCALE(b02, 24) - fc->b02) / FILTER_TRANSITION_SAMPLES;
			fc->filter_coeff_incr_count = FILTER_TRANSITION_SAMPLES;
		} else {
			fc->a1 = TIM_FSCALE(a1, 24), fc->a2 = TIM_FSCALE(a2, 24),
			fc->b02 = TIM_FSCALE(b02, 24), fc->b1 = TIM_FSCALE(b1, 24);
			fc->filter_calculated = 1;
			fc->filter_coeff_incr_count = 0;
		}
		fc->last_freq = freq;
	}
}
#endif

/* Ramp a note out in c samples */
static inline void ramp_out(mix_t *sp, int32 *lp, int v, int32 c)
{
	/* should be final_volume_t, but uint8 gives trouble. */
	int32 left, right, li, ri, i;
	/* silly warning about uninitialized s */
	mix_t s = 0;
	
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
		mix_t *sp, int32 *lp, int v, int count)
{
	Voice *vp = voice + v;
	final_volume_t left = vp->left_mix;
	int cc, i;
	mix_t s;
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

static inline void mix_mono(mix_t *sp, int32 *lp, int v, int count)
{
	final_volume_t left = voice[v].left_mix;
	mix_t s;
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
		mix_t *sp, int32 *lp, int v, int count)
{
	Voice *vp = voice + v;
	final_volume_t left = vp->left_mix, right = vp->right_mix;
	int cc, i;
	mix_t s;
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

static inline void mix_mystery(mix_t *sp, int32 *lp, int v, int count)
{
	final_volume_t left = voice[v].left_mix, right = voice[v].right_mix;
	mix_t s;
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
		mix_t *sp, int32 *lp, int v, int count)
{
	Voice *vp = voice + v;
	final_volume_t left=vp->left_mix;
	int cc, i;
	mix_t s;
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
			vp->old_left_mix = vp->old_right_mix = linear_left;
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
			vp->old_left_mix = vp->old_right_mix = linear_left;
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

static inline void mix_center(mix_t *sp, int32 *lp, int v, int count)
{
	final_volume_t left = voice[v].left_mix;
	mix_t s;
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
	vp->old_left_mix = vp->old_right_mix = linear_left;
	count -= i;
#endif
	for (i = 0; i < count; i++) {
		s = *sp++;
		MIXATION(left);
		MIXATION(left);
	}
}

static inline void mix_single_signal(
		mix_t *sp, int32 *lp, int v, int count)
{
	Voice *vp = voice + v;
	final_volume_t left = vp->left_mix;
	int cc, i;
	mix_t s;
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

static inline void mix_single(mix_t *sp, int32 *lp, int v, int count)
{
	final_volume_t left = voice[v].left_mix;
	mix_t s;
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
	Voice *vp = &voice[v];

	if (vp->envelope_increment && update_envelope(v))
		return 1;
	if (vp->tremolo_phase_increment)
		update_tremolo(v);
	if (opt_modulation_envelope)
		update_modulation_envelope(v);
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
	} else if (stage > 2 && vp->envelope_volume <= 0) {
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
	int32 offset, val;
	FLOAT_T rate;
	Voice *vp = &voice[v];
	
	stage = vp->envelope_stage++;
	offset = vp->sample->envelope_offset[stage];
	rate = vp->sample->envelope_rate[stage];
	if (vp->envelope_volume == offset
			|| stage > 2 && vp->envelope_volume < offset)
		return recompute_envelope(v);
	ch = vp->channel;

	/* envelope generator (see also playmidi.[ch]) */
	if (ISDRUMCHANNEL(ch))
		val = (channel[ch].drums[vp->note] != NULL)
				? channel[ch].drums[vp->note]->drum_envelope_rate[stage]
				: -1;
	else {
		if (vp->sample->envelope_keyf[stage])	/* envelope key-follow */
			rate *= pow(2.0, (double) (voice[v].note - 60)
					* (double)vp->sample->envelope_keyf[stage] / 1200.0f);
		val = channel[ch].envelope_rate[stage];
	}
	if (vp->sample->envelope_velf[stage])	/* envelope velocity-follow */
		rate *= pow(2.0, (double) (voice[v].velocity - vp->sample->envelope_velf_bpo)
				* (double)vp->sample->envelope_velf[stage] / 1200.0f);

	/* just before release-phase, some modifications are necessary */
	if (stage > 2) {
		/* adjusting release-rate for consistent release-time */
		rate *= (double) vp->envelope_volume
				/ vp->sample->envelope_offset[0];
		/* calculating current envelope scale and a inverted value for optimization */
		vp->envelope_scale = vp->last_envelope_volume;
		vp->inv_envelope_scale
			= TIM_FSCALE(OFFSET_MAX	/ (double)vp->envelope_volume, 16);
	}

	/* regularizing envelope */
	if (offset < vp->envelope_volume) {	/* decaying phase */
		if (val != -1)
			rate *= sc_eg_decay_table[val & 0x7f];
		if(stage < 2 && rate > OFFSET_MAX) {	/* instantaneous decay */
			vp->envelope_volume = offset;
			return recompute_envelope(v);
		} else if(rate > vp->envelope_volume - offset) {	/* fastest decay */
			rate = -vp->envelope_volume + offset - 1;
		} else if (rate < 1) {	/* slowest decay */
			rate = -1;
		}
		else {	/* ordinary decay */
			rate = -rate;
		}
	} else {	/* attacking phase */
		if (val != -1)
			rate *= sc_eg_attack_table[val & 0x7f];
		if(stage < 2 && rate > OFFSET_MAX) {	/* instantaneous attack */
			vp->envelope_volume = offset;
			return recompute_envelope(v);
		} else if(rate > offset - vp->envelope_volume) {	/* fastest attack */
			rate = offset - vp->envelope_volume + 1;
		} else if (rate < 1) {rate =  1;}	/* slowest attack */
	}

	vp->envelope_increment = (int32)rate;
	vp->envelope_target = offset;

	return 0;
}

static inline void update_tremolo(int v)
{
	Voice *vp = &voice[v];
	int32 depth = vp->tremolo_depth << 7;
	
	if(vp->tremolo_delay > 0)
	{
		vp->tremolo_delay -= vp->delay_counter;
		if(vp->tremolo_delay > 0) {
			vp->tremolo_volume = 1.0;
			return 0;
		}
		vp->tremolo_delay = 0;
	}
	if (vp->tremolo_sweep) {
		/* Update sweep position */
		vp->tremolo_sweep_position += vp->tremolo_sweep;
		if (vp->tremolo_sweep_position >= 1 << SWEEP_SHIFT)
			/* Swept to max amplitude */
			vp->tremolo_sweep = 0;
		else {
			/* Need to adjust depth */
			depth *= vp->tremolo_sweep_position;
			depth >>= SWEEP_SHIFT;
		}
	}
	vp->tremolo_phase += vp->tremolo_phase_increment;
#if 0
	if (vp->tremolo_phase >= SINE_CYCLE_LENGTH << RATE_SHIFT)
		vp->tremolo_phase -= SINE_CYCLE_LENGTH << RATE_SHIFT;
#endif

	if(vp->sample->inst_type == INST_SF2) {
	vp->tremolo_volume = 1.0 + TIM_FSCALENEG(
			lookup_log(vp->tremolo_phase >> RATE_SHIFT)
			* depth * TREMOLO_AMPLITUDE_TUNING, 17);
	} else {
	vp->tremolo_volume = 1.0 + TIM_FSCALENEG(
			lookup_sine(vp->tremolo_phase >> RATE_SHIFT)
			* depth * TREMOLO_AMPLITUDE_TUNING, 17);
	}
	/* I'm not sure about the +1.0 there -- it makes tremoloed voices'
	 *  volumes on average the lower the higher the tremolo amplitude.
	 */
}

int apply_envelope_to_amp(int v)
{
	Voice *vp = &voice[v];
	FLOAT_T lamp = vp->left_amp, ramp,
		*v_table = vp->sample->inst_type == INST_SF2 ? sb_vol_table : vol_table;
	int32 la, ra;
	
	if (vp->delay > 0) {vp->left_mix = vp->right_mix = 0;}
	else if (vp->panned == PANNED_MYSTERY) {
		ramp = vp->right_amp;
		if (vp->tremolo_phase_increment) {
			lamp *= vp->tremolo_volume;
			ramp *= vp->tremolo_volume;
		}
		if (vp->sample->modes & MODES_ENVELOPE) {
			if (vp->envelope_stage > 3)
				vp->last_envelope_volume = v_table[
						imuldiv16((vp->envelope_volume - 1) >> 20,
						vp->inv_envelope_scale)]
						* vp->envelope_scale;
			else if (vp->envelope_stage > 1)
				vp->last_envelope_volume = v_table[
						(vp->envelope_volume - 1) >> 20];
			else
				vp->last_envelope_volume = attack_vol_table[
				(vp->envelope_volume - 1) >> 20];
			lamp *= vp->last_envelope_volume;
			ramp *= vp->last_envelope_volume;
		}
		la = TIM_FSCALE(lamp, AMP_BITS);
		if (la > MAX_AMP_VALUE)
			la = MAX_AMP_VALUE;
		ra = TIM_FSCALE(ramp, AMP_BITS);
		if (ra > MAX_AMP_VALUE)
			ra = MAX_AMP_VALUE;
		if ((vp->status & (VOICE_OFF | VOICE_SUSTAINED))
				&& (la | ra) <= 0) {
			free_voice(v);
			ctl_note_event(v);
			return 1;
		}
		vp->left_mix = FINAL_VOLUME(la);
		vp->right_mix = FINAL_VOLUME(ra);
	} else {
		if (vp->tremolo_phase_increment)
			lamp *= vp->tremolo_volume;
		if (vp->sample->modes & MODES_ENVELOPE) {
			if (vp->envelope_stage > 3)
				vp->last_envelope_volume = v_table[
						imuldiv16((vp->envelope_volume - 1) >> 20,
						vp->inv_envelope_scale)]
						* vp->envelope_scale;
			else if (vp->envelope_stage > 1)
				vp->last_envelope_volume = v_table[
						(vp->envelope_volume - 1) >> 20];
			else
				vp->last_envelope_volume = attack_vol_table[
				(vp->envelope_volume - 1) >> 20];
			lamp *= vp->last_envelope_volume;
		}
		la = TIM_FSCALE(lamp, AMP_BITS);
		if (la > MAX_AMP_VALUE)
		la = MAX_AMP_VALUE;
		if ((vp->status & (VOICE_OFF | VOICE_SUSTAINED))
				&& la <= 0) {
			free_voice(v);
			ctl_note_event(v);
			return 1;
		}
		vp->left_mix = FINAL_VOLUME(la);
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

static inline int update_modulation_envelope(int v)
{
	Voice *vp = &voice[v];

	if(vp->modenv_delay > 0) {
		vp->modenv_delay -= vp->delay_counter;
		if(vp->modenv_delay > 0) {
			return 1;
		}
		vp->modenv_delay = 0;
	}
	vp->modenv_volume += vp->modenv_increment;
	if ((vp->modenv_increment < 0)
			^ (vp->modenv_volume > vp->modenv_target)) {
		vp->modenv_volume = vp->modenv_target;
		if (recompute_modulation_envelope(v)) {
			apply_modulation_envelope(v);
			return 1;
		}
	}

	apply_modulation_envelope(v);
	
	return 0;
}

int apply_modulation_envelope(int v)
{
	Voice *vp = &voice[v];

	if(!opt_modulation_envelope) {return 0;}

	if (vp->modenv_delay > 0) {vp->last_modenv_volume = 0;}
	else if (vp->sample->modes & MODES_ENVELOPE) {
		if (vp->modenv_stage > 1)
			vp->last_modenv_volume = (double)vp->modenv_volume / (double)OFFSET_MAX;
		else
			vp->last_modenv_volume = convex_vol_table[(vp->modenv_volume - 1) >> 20];
	}

	recompute_voice_filter(v);
	if(!(vp->porta_control_ratio && vp->porta_control_counter == 0)) {
		recompute_freq(v);
	}
	return 0;
}

static inline int modenv_next_stage(int v)
{
	int stage, ch;
	int32 offset, val;
	FLOAT_T rate;
	Voice *vp = &voice[v];
	
	stage = vp->modenv_stage++;
	offset = vp->sample->modenv_offset[stage];
	rate = vp->sample->modenv_rate[stage];
	if (vp->modenv_volume == offset
			|| (stage > 2 && vp->modenv_volume < offset))
		return recompute_modulation_envelope(v);
	ch = vp->channel;

	/* envelope generator (see also playmidi.[ch]) */
	if (ISDRUMCHANNEL(ch))
		val = (channel[ch].drums[vp->note] != NULL)
				? channel[ch].drums[vp->note]->drum_envelope_rate[stage]
				: -1;
	else {
		if (vp->sample->modenv_keyf[stage])	/* envelope key-follow */
			rate *= pow(2.0, (double) (voice[v].note - 60)
					* (double)vp->sample->modenv_keyf[stage] / 1200.0f);
		val = channel[ch].envelope_rate[stage];
	}
	if (vp->sample->modenv_velf[stage])
		rate *= pow(2.0, (double) (voice[v].velocity - vp->sample->modenv_velf_bpo)
				* (double)vp->sample->modenv_velf[stage] / 1200.0f);

	/* just before release-phase, some modifications are necessary */
	if (stage > 2) {
		/* adjusting release-rate for consistent release-time */
		rate *= (double) vp->modenv_volume
				/ vp->sample->modenv_offset[0];
	}

	/* regularizing envelope */
	if (offset < vp->modenv_volume) {	/* decaying phase */
		if (val != -1)
			rate *= sc_eg_decay_table[val & 0x7f];
		if(stage < 2 && rate > OFFSET_MAX) {	/* instantaneous decay */
			vp->modenv_volume = offset;
			return recompute_modulation_envelope(v);
		} else if(rate > vp->modenv_volume - offset) {	/* fastest decay */
			rate = -vp->modenv_volume + offset - 1;
		} else if (rate < 1) {	/* slowest decay */
			rate = -1;
		} else {	/* ordinary decay */
			rate = -rate;
		}
	} else {	/* attacking phase */
		if (val != -1)
			rate *= sc_eg_attack_table[val & 0x7f];
		if(stage < 2 && rate > OFFSET_MAX) {	/* instantaneous attack */
				vp->modenv_volume = offset;
				return recompute_modulation_envelope(v);
		} else if(rate > offset - vp->modenv_volume) {	/* fastest attack */
			rate = offset - vp->modenv_volume + 1;
		} else if (rate < 1) {rate = 1;}	/* slowest attack */
	}
	
	vp->modenv_increment = (int32)rate;
	vp->modenv_target = offset;

	return 0;
}

int recompute_modulation_envelope(int v)
{
	int stage, ch;
	int32 rate;
	Voice *vp = &voice[v];

	if(!opt_modulation_envelope) {return 0;}

	stage = vp->modenv_stage;
	if (stage > 5) {return 1;}
	else if (stage > 2 && vp->modenv_volume <= 0) {
		return 1;
	}

	if (stage == 3 && vp->sample->modes & MODES_ENVELOPE
			&& vp->status & (VOICE_ON | VOICE_SUSTAINED)) {
		/* Default behavior */
		if (min_sustain_time <= 0)
			/* Freeze envelope until note turns off */
			vp->modenv_increment = 0;
		else if (vp->status & VOICE_SUSTAINED
				&& vp->sample->modes & MODES_LOOPING
				&& vp->sample_offset - vp->sample->loop_start >= 0) {
			if (min_sustain_time == 1)
				/* The sustain stage is ignored. */
				return modenv_next_stage(v);
			/* Calculate the release phase speed. */
			rate = -0x3fffffff * (double) control_ratio * 1000
					/ (min_sustain_time * play_mode->rate);
			ch = vp->channel;
			vp->modenv_increment = -vp->sample->modenv_rate[2];
			/* use the slower of the two rates */
			if (vp->modenv_increment < rate)
				vp->modenv_increment = rate;
			if (! vp->modenv_increment)
				/* Avoid freezing */
				vp->modenv_increment = -1;
			vp->modenv_target = 0;
		/* it's not decaying, so freeze it */
		} else {
			/* tiny value to make other functions happy, freeze note */
			vp->modenv_increment = 1;
			/* this will cause update_envelope(v) to undo the +1 inc. */
			vp->modenv_target = vp->modenv_volume;
		}
		return 0;
	}
	return modenv_next_stage(v);
}
