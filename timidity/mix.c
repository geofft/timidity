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

#define DELAYED_MIXATION(a) *lp++ += pan_delay_buf[pan_delay_wpt--];	\
	if(pan_delay_wpt < 0) {pan_delay_wpt = vp->pan_delay_rpt - 1;}	\
	pan_delay_buf[pan_delay_wpt] = (a) * s

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
static inline int modenv_next_stage(int);
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
		vp->delay_counter = c;
		if (vp->delay) {
			if (c < vp->delay) {
				vp->delay -= c;
				if (vp->tremolo_phase_increment)
					update_tremolo(v);
				if (opt_modulation_envelope && vp->sample->modes & MODES_ENVELOPE)
					update_modulation_envelope(v);
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
static inline void do_voice_filter(int v, sample_t *sp, mix_t *lp, int32 count)
{
	FilterCoefficients *fc = &(voice[v].fc);
	int32 i, f, q, p, b0, b1, b2, b3, b4, t1, t2, x;
	
	if(fc->type == 0) {	/* copy without change.	*/
		/* (FIXME: It's absolutely essential that converting int16 sp[] to int32 lp[],
			but it might not be only way.) */
		for(i = 0; i <count; i++) {
			lp[i] = sp[i];
		}
		return;
	} else if(fc->type == 1) {	/* copy with applying Chamberlin's lowpass filter. */
		recalc_voice_resonance(v);
		recalc_voice_fc(v);
		f = fc->f, q = fc->q, b0 = fc->b0, b1 = fc->b1, b2 = fc->b2;
		for(i = 0; i < count; i++) {
			b0 = b0 + imuldiv24(b2, f);
			b1 = sp[i] - b0 - imuldiv24(b2, q);
			b2 = imuldiv24(b1, f) + b2;
			lp[i] = b0;
		}
		fc->b0 = b0, fc->b1 = b1, fc->b2 = b2;
		return;
	} else if(fc->type == 2) {	/* copy with applying Moog lowpass VCF. */
		recalc_voice_resonance(v);
		recalc_voice_fc(v);
		f = fc->f, q = fc->q, p = fc->p, b0 = fc->b0, b1 = fc->b1,
			b2 = fc->b2, b3 = fc->b3, b4 = fc->b4;
		for(i = 0; i < count; i++) {
			x = sp[i] - imuldiv24(q, b4);	/* feedback */
			t1 = b1;  b1 = imuldiv24(x + b0, p) - imuldiv24(b1, f);
			t2 = b2;  b2 = imuldiv24(b1 + t1, p) - imuldiv24(b2, f);
			t1 = b3;  b3 = imuldiv24(b2 + t2, p) - imuldiv24(b3, f);
			lp[i] = b4 = imuldiv24(b3 + t1, p) - imuldiv24(b4, f);
			b0 = x;
		}
		fc->b0 = b0, fc->b1 = b1, fc->b2 = b2, fc->b3 = b3, fc->b4 = b4;
		return;
	}
}

#define MOOG_RESONANCE_MAX 0.897638f

static inline void recalc_voice_resonance(int v)
{
	double q;
	FilterCoefficients *fc = &(voice[v].fc);
	
	if (fc->reso_dB != fc->last_reso_dB || fc->q == 0) {
		fc->last_reso_dB = fc->reso_dB;
		if(fc->type == 1) {
			q = 1.0 / chamberlin_filter_db_to_q_table[(int)(fc->reso_dB * 4)];
			fc->q = TIM_FSCALE(q, 24);
			if(fc->q <= 0) {fc->q = 1;}	/* must never be 0. */
		} else if(fc->type == 2) {
			fc->reso_lin = fc->reso_dB * MOOG_RESONANCE_MAX / 20.0f;
			if (fc->reso_lin > MOOG_RESONANCE_MAX) {fc->reso_lin = MOOG_RESONANCE_MAX;}
			else if(fc->reso_lin < 0) {fc->reso_lin = 0;}
		}
		fc->last_freq = -1;
	}
}

static inline void recalc_voice_fc(int v)
{
	double f, p, q, fr;
	FilterCoefficients *fc = &(voice[v].fc);

	if (fc->freq != fc->last_freq) {
		if(fc->type == 1) {
			f = 2.0 * sin(M_PI * (double)fc->freq / (double)play_mode->rate);
			fc->f = TIM_FSCALE(f, 24);
		} else if(fc->type == 2) {
			fr = 2.0 * (double)fc->freq / (double)play_mode->rate;
			q = 1.0 - fr;
			p = fr + 0.8 * fr * q;
			f = p + p - 1.0;
			q = fc->reso_lin * (1.0 + 0.5 * q * (1.0 - q + 5.6 * q * q));
			fc->f = TIM_FSCALE(f, 24);
			fc->p = TIM_FSCALE(p, 24);
			fc->q = TIM_FSCALE(q, 24);
		}
		fc->last_freq = fc->freq;
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
#ifdef ENABLE_PAN_DELAY
	Voice *vp = &voice[v];
	int32 pan_delay_wpt = vp->pan_delay_wpt, *pan_delay_buf = vp->pan_delay_buf;
#endif

	left = voice[v].left_mix;
	li = -(left / c);
	if (! li)
		li = -1;
#if 0
	printf("Ramping out: left=%d, c=%d, li=%d\n", left, c, li);
#endif
	if (! (play_mode->encoding & PE_MONO)) {
		if (voice[v].panned == PANNED_MYSTERY) {
#ifdef ENABLE_PAN_DELAY
			right = voice[v].right_mix;
			ri = -(right / c);
			if(vp->pan_delay_rpt == 0) {
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
			} else if(vp->panning < 64) {
				for (i = 0; i < c; i++) {
					left += li;
					if (left < 0)
						left = 0;
					right += ri;
					if (right < 0)
						right = 0;
					s = *sp++;
					MIXATION(left);
					DELAYED_MIXATION(right);
				}
			} else {
				for (i = 0; i < c; i++) {
					left += li;
					if (left < 0)
						left = 0;
					right += ri;
					if (right < 0)
						right = 0;
					s = *sp++;
					DELAYED_MIXATION(left);
					MIXATION(right);
				}
			}
			vp->pan_delay_wpt = pan_delay_wpt;
#else
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
#endif /* ENABLE_PAN_DELAY */
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

#ifdef ENABLE_PAN_DELAY
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
	int32 pan_delay_wpt = vp->pan_delay_wpt, *pan_delay_buf = vp->pan_delay_buf;

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
			if(vp->pan_delay_rpt == 0) {
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
			} else if(vp->panning < 64) {
				for (i = 0; (vp->left_mix_offset | vp->right_mix_offset)
						&& i < cc; i++) {
					s = *sp++;
					MIXATION(left);
					DELAYED_MIXATION(right);
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
			} else {
				for (i = 0; (vp->left_mix_offset | vp->right_mix_offset)
						&& i < cc; i++) {
					s = *sp++;
					DELAYED_MIXATION(left);
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
			}
			vp->old_left_mix = linear_left;
			vp->old_right_mix = linear_right;
			cc -= i;
#endif
			if(vp->pan_delay_rpt == 0) {
				for (i = 0; i < cc; i++) {
					s = *sp++;
					MIXATION(left);
					MIXATION(right);
				}
			} else if(vp->panning < 64) {
				for (i = 0; i < cc; i++) {
					s = *sp++;
					MIXATION(left);
					DELAYED_MIXATION(right);
				}
			} else {
				for (i = 0; i < cc; i++) {
					s = *sp++;
					DELAYED_MIXATION(left);
					MIXATION(right);
				}
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
			if(vp->pan_delay_rpt == 0) {
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
			} else if(vp->panning < 64) {
				for (i = 0; (vp->left_mix_offset | vp->right_mix_offset)
						&& i < count; i++) {
					s = *sp++;
					MIXATION(left);
					DELAYED_MIXATION(right);
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
			} else {
				for (i = 0; (vp->left_mix_offset | vp->right_mix_offset)
						&& i < count; i++) {
					s = *sp++;
					DELAYED_MIXATION(left);
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
			}

			vp->old_left_mix = linear_left;
			vp->old_right_mix = linear_right;
			count -= i;
#endif
			if(vp->pan_delay_rpt == 0) {
				for (i = 0; i < count; i++) {
					s = *sp++;
					MIXATION(left);
					MIXATION(right);
				}
			} else if(vp->panning < 64) {
				for (i = 0; i < count; i++) {
					s = *sp++;
					MIXATION(left);
					DELAYED_MIXATION(right);
				}
			} else {
				for (i = 0; i < count; i++) {
					s = *sp++;
					DELAYED_MIXATION(left);
					MIXATION(right);
				}
			}
			vp->pan_delay_wpt = pan_delay_wpt;
			return;
		}
}
#else	/* ENABLE_PAN_DELAY */
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
#endif	/* ENABLE_PAN_DELAY */

#ifdef ENABLE_PAN_DELAY
static inline void mix_mystery(mix_t *sp, int32 *lp, int v, int count)
{
	final_volume_t left = voice[v].left_mix, right = voice[v].right_mix;
	mix_t s;
	int i;
#ifdef SMOOTH_MIXING
	Voice *vp = voice + v;
	int32 linear_left, linear_right;
#endif
	int32 pan_delay_wpt = vp->pan_delay_wpt, *pan_delay_buf = vp->pan_delay_buf;

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
	if(vp->pan_delay_rpt == 0) {
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
	} else if(vp->panning < 64) {
		for (i = 0; (vp->left_mix_offset | vp->right_mix_offset)
				&& i < count; i++) {
			s = *sp++;
			MIXATION(left);
			DELAYED_MIXATION(right);
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
	} else {
		for (i = 0; (vp->left_mix_offset | vp->right_mix_offset)
				&& i < count; i++) {
			s = *sp++;
			DELAYED_MIXATION(left);
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
	}

	vp->old_left_mix = linear_left;
	vp->old_right_mix = linear_right;
	count -= i;
#endif
	if(vp->pan_delay_rpt == 0) {
		for (i = 0; i < count; i++) {
			s = *sp++;
			MIXATION(left);
			MIXATION(right);
		}
	} else if(vp->panning < 64) {
		for (i = 0; i < count; i++) {
			s = *sp++;
			MIXATION(left);
			DELAYED_MIXATION(right);
		}
	} else {
		for (i = 0; i < count; i++) {
			s = *sp++;
			DELAYED_MIXATION(left);
			MIXATION(right);
		}
	}
	vp->pan_delay_wpt = pan_delay_wpt;
}
#else	/* ENABLE_PAN_DELAY */
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
#endif	/* ENABLE_PAN_DELAY */


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
	if (opt_modulation_envelope && vp->sample->modes & MODES_ENVELOPE)
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
	double sustain_time;
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
			ch = vp->channel;
			if (channel[ch].sostenuto) {
				sustain_time = min_sustain_time;
			} else {
				sustain_time = (double)min_sustain_time * (double)(channel[ch].sustain - 64) / 63.0;
			}
			/* Calculate the release phase speed. */
			rate = -0x3fffffff * (double) control_ratio * 1000
					/ (sustain_time * play_mode->rate);
			
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
		if (val != -1) {
			if(stage > 2) {
				rate *= sc_eg_release_table[val & 0x7f];
			} else {
				rate *= sc_eg_decay_table[val & 0x7f];
			}
		}
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
			return;
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
			lookup_sine(vp->tremolo_phase >> RATE_SHIFT)
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
	
	if (vp->panned == PANNED_MYSTERY) {
		ramp = vp->right_amp;
		if (vp->tremolo_phase_increment) {
			lamp *= vp->tremolo_volume;
			ramp *= vp->tremolo_volume;
		}
		if (vp->sample->modes & MODES_ENVELOPE) {
			if (vp->envelope_stage > 3)
				vp->last_envelope_volume = v_table[
						imuldiv16(vp->envelope_volume >> 20,
						vp->inv_envelope_scale)]
						* vp->envelope_scale;
			else if (vp->envelope_stage > 1)
				vp->last_envelope_volume = v_table[
						vp->envelope_volume >> 20];
			else
				vp->last_envelope_volume = attack_vol_table[
				vp->envelope_volume >> 20];
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
						imuldiv16(vp->envelope_volume >> 20,
						vp->inv_envelope_scale)]
						* vp->envelope_scale;
			else if (vp->envelope_stage > 1)
				vp->last_envelope_volume = v_table[
						vp->envelope_volume >> 20];
			else
				vp->last_envelope_volume = attack_vol_table[
				vp->envelope_volume >> 20];
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
		if(vp->modenv_delay > 0) {return 1;}
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

	if (vp->sample->modes & MODES_ENVELOPE) {
		if (vp->modenv_stage > 1)
			vp->last_modenv_volume = (double)vp->modenv_volume / (double)OFFSET_MAX;
		else
			vp->last_modenv_volume = convex_vol_table[vp->modenv_volume >> 20];
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
	else if(stage < 2 && rate > OFFSET_MAX) {	/* instantaneous attack */
			vp->modenv_volume = offset;
			return recompute_modulation_envelope(v);
	}
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
		if (val != -1) {
			if(stage > 2) {
				rate *= sc_eg_release_table[val & 0x7f];
			} else {
				rate *= sc_eg_decay_table[val & 0x7f];
			}
		}
		if(rate > vp->modenv_volume - offset) {	/* fastest decay */
			rate = -vp->modenv_volume + offset - 1;
		} else if (rate < 1) {	/* slowest decay */
			rate = -1;
		} else {	/* ordinary decay */
			rate = -rate;
		}
	} else {	/* attacking phase */
		if (val != -1)
			rate *= sc_eg_attack_table[val & 0x7f];
		if(rate > offset - vp->modenv_volume) {	/* fastest attack */
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
	double sustain_time;
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
			ch = vp->channel;
			if (channel[ch].sostenuto) {
				sustain_time = min_sustain_time;
			} else {
				sustain_time = (double)min_sustain_time * (double)(channel[ch].sustain - 64) / 63.0;
			}
			/* Calculate the release phase speed. */
			rate = -0x3fffffff * (double) control_ratio * 1000
					/ (sustain_time * play_mode->rate);
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
