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
 * reverb.c  -- main reverb engine.
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include "timidity.h"
#include "controls.h"
#include "tables.h"
#include "common.h"
#include "output.h"
#include "reverb.h"
#include <math.h>
#include <stdlib.h>

int opt_effect_quality = 0;
static int32 sample_rate = 44100;

enum play_system_modes
{
    DEFAULT_SYSTEM_MODE,
    GM_SYSTEM_MODE,
    GS_SYSTEM_MODE,
    XG_SYSTEM_MODE
};
extern int play_system_mode;

static double REV_INP_LEV = 1.0;

/* delay buffers @65kHz */
#define REV_BUF0       344 * 2
#define REV_BUF1       684 * 2
#define REV_BUF2      2868 * 2
#define REV_BUF3      1368 * 2

#define REV_VAL0         5.3
#define REV_VAL1        10.5
#define REV_VAL2        44.12
#define REV_VAL3        21.0

#if OPT_MODE != 0
#define REV_FBK_LEV      TIM_FSCALE(0.12, 24)

#define REV_NMIX_LEV     0.7
#define REV_CMIX_LEV     TIM_FSCALE(0.9, 24)
#define REV_MONO_LEV     0.7

#define REV_HPF_LEV      TIM_FSCALE(0.5, 24)
#define REV_LPF_LEV      TIM_FSCALE(0.45, 24)
#define REV_LPF_INP      TIM_FSCALE(0.55, 24)
#define REV_EPF_LEV      TIM_FSCALE(0.4, 24)
#define REV_EPF_INP      TIM_FSCALE(0.48, 24)

#define REV_WIDTH        TIM_FSCALE(0.125, 24)
#else
#define REV_FBK_LEV      0.12

#define REV_NMIX_LEV     0.7
#define REV_CMIX_LEV     0.9
#define REV_MONO_LEV     0.7

#define REV_HPF_LEV      0.5
#define REV_LPF_LEV      0.45
#define REV_LPF_INP      0.55
#define REV_EPF_LEV      0.4
#define REV_EPF_INP      0.48

#define REV_WIDTH        0.125
#endif

static int  spt0, rpt0, def_rpt0;
static int  spt1, rpt1, def_rpt1;
static int  spt2, rpt2, def_rpt2;
static int  spt3, rpt3, def_rpt3;
static int32  buf0_L[REV_BUF0], buf0_R[REV_BUF0];
static int32  buf1_L[REV_BUF1], buf1_R[REV_BUF1];
static int32  buf2_L[REV_BUF2], buf2_R[REV_BUF2];
static int32  buf3_L[REV_BUF3], buf3_R[REV_BUF3];

static int32  effect_buffer[AUDIO_BUFFER_SIZE*2];
static int32  direct_buffer[AUDIO_BUFFER_SIZE*2];
static int32  effect_bufsize = sizeof(effect_buffer);
static int32  direct_bufsize = sizeof(direct_buffer);

static int32  ta, tb;
static int32  HPFL, HPFR;
static int32  LPFL, LPFR;
static int32  EPFL, EPFR;

#define rev_memset(xx)     memset(xx,0,sizeof(xx));

#define rev_ptinc() \
spt0++; if(spt0 == rpt0) spt0 = 0;\
spt1++; if(spt1 == rpt1) spt1 = 0;\
spt2++; if(spt2 == rpt2) spt2 = 0;\
spt3++; if(spt3 == rpt3) spt3 = 0;

#define MASTER_CHORUS_LEVEL 1.7
#define MASTER_DELAY_LEVEL 3.25

static void do_shelving_filter(register int32 *, int32, int32 *, int32 *);
static void do_freeverb(int32 *buf, int32 count);
static void alloc_revmodel(void);

#if OPT_MODE != 0 && _MSC_VER
void set_dry_signal(int32 *buf, int32 count)
{
	int32 *dbuf = direct_buffer;
	_asm {
		mov		ecx, [count]
		mov		esi, [buf]
		test	ecx, ecx
		jz		short L2
		mov		edi, [dbuf]
L1:		mov		eax, [esi]
		mov		ebx, [edi]
		add		esi, 4
		add		ebx, eax
		mov		[edi], ebx
		add		edi, 4
		dec		ecx
		jnz		L1
L2:
	}
}
#else
void set_dry_signal(register int32 *buf, int32 n)
{
#if USE_ALTIVEC
  if(is_altivec_available()) {
    v_set_dry_signal(direct_buffer,buf,n);
  } else {
#endif
    register int32 i;
	register int32 *dbuf = direct_buffer;

    for(i=n-1;i>=0;i--)
    {
        dbuf[i] += buf[i];
    }
#if USE_ALTIVEC
  }
#endif
}
#endif

void mix_dry_signal(register int32 *buf, int32 n)
{
 	memcpy(buf,direct_buffer,sizeof(int32) * n);
	memset(direct_buffer,0,sizeof(int32) * n);
}

/*                    */
/*  Effect Utitities  */
/*                    */

/* temporary variables */
static int32 _output, _bufout;

static void init_delay(delay *delay)
{
	memset(delay->buf, 0, sizeof(int32) * delay->size);
}

static void free_delay(delay *delay)
{
	if(delay->buf != NULL) {free(delay->buf);}
}

static void set_delay(delay *delay, int32 size)
{
	free_delay(delay);
	delay->buf = (int32 *)safe_malloc(sizeof(int32) * size);
	if(delay->buf == NULL) {return;}
	delay->index = 0;
	delay->size = size;
	init_delay(delay);
}

/*! simple delay */
#define do_delay(_stream, _buf, _size, _index) \
{ \
	_output = _buf[_index];	\
	_buf[_index] = _stream;	\
	if(++_index >= _size) {	\
		_index = 0;	\
	}	\
	_stream = _output;	\
}

/*! feedback delay */
#if OPT_MODE != 0	/* fixed-point implementation */
#define do_feedback_delay(_stream, _buf, _size, _index, _feedback) \
{ \
	_output = _buf[_index];	\
	_buf[_index] = _stream + imuldiv24(_output, _feedback);	\
	if(++_index >= _size) {	\
		_index = 0;	\
	}	\
	_stream = _output;	\
}
#else	/* floating-point implementation */
#define do_feedback_delay(_stream, _buf, _size, _index, _feedback) \
{ \
	_output = _buf[_index];	\
	_buf[_index] = _stream + _output * _feedback;	\
	if(++_index >= _size) {	\
		_index = 0;	\
	}	\
	_stream = _output;	\
}
#endif	/* OPT_MODE != 0 */

static void init_lfo(lfo *lfo, int32 cycle, int type)
{
	int32 i;

	lfo->count = 0;
	if(cycle < MIN_LFO_CYCLE_LENGTH) {cycle = MIN_LFO_CYCLE_LENGTH;}
	lfo->cycle = cycle;
	lfo->icycle = TIM_FSCALE((SINE_CYCLE_LENGTH - 1) / (double)cycle, 24) - 0.5;

	if(lfo->type != type) {	/* generate waveform of LFO */
		switch(type) {
		case LFO_SINE:
			for(i = 0; i < SINE_CYCLE_LENGTH; i++)
				lfo->buf[i] = TIM_FSCALE((lookup_sine(i) + 1.0) / 2.0, 16);
			break;
		case LFO_TRIANGULAR:
			for(i = 0; i < SINE_CYCLE_LENGTH; i++)
				lfo->buf[i] = TIM_FSCALE((lookup_triangular(i) + 1.0) / 2.0, 16);
			break;
		default:
			for(i = 0; i < SINE_CYCLE_LENGTH; i++) {lfo->buf[i] = TIM_FSCALE(0.5, 16);}
			break;
		}
	}
	lfo->type = type;
}

/*! LFO */
/* returned value is between 0 and (1 << 16) */
inline int32 do_lfo(lfo *lfo)
{
	int32 val;
	val = lfo->buf[imuldiv24(lfo->count, lfo->icycle)];
	if(++lfo->count == lfo->cycle) {lfo->count = 0;}
	return val;
}

/*! calculate Moog VCF coefficients */
void calc_filter_moog(filter_moog *svf)
{
	double res, fr, p, q, f;

	if(svf->freq != svf->last_freq || svf->res_dB != svf->last_res_dB) {
		if(svf->last_freq == 0) {	/* clear delay-line */
			svf->b0 = svf->b1 = svf->b2 = svf->b3 = svf->b4 = 0;
		}
		svf->last_freq = svf->freq, svf->last_res_dB = svf->res_dB;

		res = pow(10, (svf->res_dB - 96) / 20);
		fr = 2.0 * (double)svf->freq / (double)play_mode->rate;
		q = 1.0 - fr;
		p = fr + 0.8 * fr * q;
		f = p + p - 1.0;
		q = res * (1.0 + 0.5 * q * (1.0 - q + 5.6 * q * q));
		svf->f = TIM_FSCALE(f, 24);
		svf->p = TIM_FSCALE(p, 24);
		svf->q = TIM_FSCALE(q, 24);
	}
}

/*! calculate LPF18 coefficients */
void calc_filter_lpf18(filter_lpf18 *p)
{
	double kfcn, kp, kp1, kp1h, kres, value;

	if(p->freq != p->last_freq || p->dist != p->last_dist
		|| p->res != p->last_res) {
		if(p->last_freq == 0) {	/* clear delay-line */
			p->ay1 = p->ay2 = p->aout = p->lastin = 0;
		}
		p->last_freq = p->freq, p->last_dist = p->dist, p->last_res = p->res;

		kfcn = 2.0 * (double)p->freq / (double)play_mode->rate;
		kp = ((-2.7528 * kfcn + 3.0429) * kfcn + 1.718) * kfcn - 0.9984;
		kp1 = kp + 1.0;
		kp1h = 0.5 * kp1;
		kres = p->res * (((-2.7079 * kp1 + 10.963) * kp1 - 14.934) * kp1 + 8.4974);
		value = 1.0 + (p->dist * (1.5 + 2.0 * kres * (1.0 - kfcn)));

		p->kp = kp, p->kp1h = kp1h, p->kres = kres, p->value = value;
	}
}

/*! calculate 1st order IIR lowpass filter coefficients */
void calc_filter_iir1_lowpass(filter_iir1 *p)
{
	double freq, norm, w = 2.0 * (double)play_mode->rate, b1, a0, a1;

	if(p->freq != p->last_freq) {
		if(p->last_freq == 0) {	/* clear delay-line */
			p->x1l = p->y1l = p->x1r = p->y1r = 0;
		}
		p->last_freq = p->freq;

		freq = p->freq;
		freq *= 2.0 * M_PI;
		norm = 1.0 / (freq + w);
		b1 = (w - freq) * norm;
		a0 = a1 = freq * norm;

		p->a0i = TIM_FSCALE(a0, 24);
		p->a1i = TIM_FSCALE(a1, 24);
		p->b1i = TIM_FSCALE(b1, 24);
	}
}

/*! 1st order IIR lowpass filter */
void do_filter_iir1_lowpass_stereo(int32 *buf, int32 count, filter_iir1 *p)
{
	int32 i, a0, b1, x1l, y1l, x1r, y1r, input;

	a0 = p->a0i, b1 = p->b1i, x1l = p->x1l, y1l = p->y1l, x1r = p->x1r, y1r = p->y1r;
	for(i = 0; i < count; i++) {
		/* left */
		input = buf[i];
		y1l = buf[i] = imuldiv24(input + x1l, a0) + imuldiv24(y1l, b1);
		x1l = input;

		/* right */
		input = buf[++i];
		y1r = buf[i] = imuldiv24(input + x1r, a0) + imuldiv24(y1r, b1);
		x1r = input;
	}
	p->x1l = x1l, p->y1l = y1l, p->x1r = x1r, p->y1r = y1r;
}


#if OPT_MODE != 0
#if _MSC_VER
void set_ch_reverb(int32 *buf, int32 count, int32 level)
{
	int32 *dbuf = effect_buffer;
	level = TIM_FSCALE(level / 127.0 * REV_INP_LEV, 24);

	_asm {
		mov		ecx, [count]
		mov		esi, [buf]
		mov		ebx, [level]
		test	ecx, ecx
		jz		short L2
		mov		edi, [dbuf]
L1:		mov		eax, [esi]
		imul	ebx
		shr		eax, 24
		shl		edx, 8
		or		eax, edx	/* u */
		mov		edx, [edi]	/* v */
		add		esi, 4		/* u */	
		add		edx, eax	/* v */
		mov		[edi], edx	/* u */
		add		edi, 4		/* v */
		dec		ecx			/* u */
		jnz		L1			/* v */
L2:
	}
}
#else
void set_ch_reverb(int32 *buf, int32 count, int32 level)
{
    int32 i, *dbuf = effect_buffer;
    level = TIM_FSCALE(level / 127.0 * REV_INP_LEV, 24);

	for(i=count-1;i>=0;i--) {dbuf[i] += imuldiv24(buf[i], level);}
}
#endif	/* _MSC_VER */
#else
void set_ch_reverb(register int32 *sbuffer, int32 n, int32 level)
{
    register int32  i;
    FLOAT_T send_level = (FLOAT_T)level / 127.0 * REV_INP_LEV;
	
	for(i = 0; i < n; i++)
    {
        effect_buffer[i] += sbuffer[i] * send_level;
    }
}
#endif /* OPT_MODE != 0 */


#if OPT_MODE != 0
void do_standard_reverb(register int32 *comp, int32 n)
{
	int32 i, fixp, s, t;

    for(i = 0; i < n; i++)
    {
        /* L */
        fixp = effect_buffer[i];
        effect_buffer[i] = 0;

        LPFL = imuldiv24(LPFL,REV_LPF_LEV) + imuldiv24(buf2_L[spt2] + tb,REV_LPF_INP) + imuldiv24(ta,REV_WIDTH);
        ta = buf3_L[spt3];
        s  = buf3_L[spt3] = buf0_L[spt0];
        buf0_L[spt0] = -LPFL;

        t = imuldiv24(HPFL + fixp,REV_HPF_LEV);
        HPFL = t - fixp;

        buf2_L[spt2] = imuldiv24(s - imuldiv24(fixp,REV_FBK_LEV),REV_CMIX_LEV);
        tb = buf1_L[spt1];
        buf1_L[spt1] = t;

        EPFL = imuldiv24(EPFL,REV_EPF_LEV) + imuldiv24(ta,REV_EPF_INP);
        comp[i] += ta + EPFL;

        /* R */
        fixp = effect_buffer[++i];
        effect_buffer[i] = 0;

        LPFR = imuldiv24(LPFR,REV_LPF_LEV) + imuldiv24(buf2_R[spt2] + tb,REV_LPF_INP) + imuldiv24(ta,REV_WIDTH);
        ta = buf3_R[spt3];
        s  = buf3_R[spt3] = buf0_R[spt0];
        buf0_R[spt0] = LPFR;

        t = imuldiv24(HPFR + fixp,REV_HPF_LEV);
        HPFR = t - fixp;

        buf2_R[spt2] = imuldiv24(s - imuldiv24(fixp,REV_FBK_LEV),REV_CMIX_LEV);
        tb = buf1_R[spt1];
        buf1_R[spt1] = t;

        EPFR = imuldiv24(EPFR,REV_EPF_LEV) + imuldiv24(ta,REV_EPF_INP);
        comp[i] += ta + EPFR;

        rev_ptinc();
    }
}
#else
void do_standard_reverb(int32 *comp, int32 n)
{
    int32  fixp, s, t, i;

    for(i = 0; i < n; i++)
    {
        /* L */
        fixp = effect_buffer[i];
        effect_buffer[i] = 0;

        LPFL = LPFL*REV_LPF_LEV + (buf2_L[spt2]+tb)*REV_LPF_INP + ta*REV_WIDTH;
        ta = buf3_L[spt3];
        s  = buf3_L[spt3] = buf0_L[spt0];
        buf0_L[spt0] = -LPFL;

        t = (HPFL + fixp) * REV_HPF_LEV;
        HPFL = t - fixp;

        buf2_L[spt2] = (s - fixp * REV_FBK_LEV) * REV_CMIX_LEV;
        tb = buf1_L[spt1];
        buf1_L[spt1] = t;

        EPFL = EPFL * REV_EPF_LEV + ta * REV_EPF_INP;
        comp[i] += ta + EPFL;
        direct_buffer[i] = 0;

        /* R */
        fixp = effect_buffer[++i];
        effect_buffer[i] = 0;

        LPFR = LPFR*REV_LPF_LEV + (buf2_R[spt2]+tb)*REV_LPF_INP + ta*REV_WIDTH;
        ta = buf3_R[spt3];
        s  = buf3_R[spt3] = buf0_R[spt0];
        buf0_R[spt0] = LPFR;

        t = (HPFR + fixp) * REV_HPF_LEV;
        HPFR = t - fixp;

        buf2_R[spt2] = (s - fixp * REV_FBK_LEV) * REV_CMIX_LEV;
        tb = buf1_R[spt1];
        buf1_R[spt1] = t;

        EPFR = EPFR * REV_EPF_LEV + ta * REV_EPF_INP;
        comp[i] += ta + EPFR;
        direct_buffer[i] = 0;

        rev_ptinc();
    }
}
#endif /* OPT_MODE != 0 */

void do_ch_reverb(int32 *buf, int32 count)
{
	if ((opt_reverb_control == 3 || opt_reverb_control == 4
			|| opt_reverb_control < 0 && ! (opt_reverb_control & 0x100)
			|| opt_effect_quality >= 1) && reverb_status.pre_lpf)
		do_filter_iir1_lowpass_stereo(effect_buffer, count, &(reverb_status.lpf));
	if (opt_reverb_control == 3 || opt_reverb_control == 4
			|| opt_reverb_control < 0 && ! (opt_reverb_control & 0x100)
			|| opt_effect_quality >= 2)
		do_freeverb(buf, count);
	else
		do_standard_reverb(buf, count);
}

void do_reverb(int32 *comp, int32 n)
{
    int32  fixp, s, t, i;

    for(i = 0; i < n; i++)
    {
        /* L */
        fixp = comp[i];

        LPFL = LPFL*REV_LPF_LEV + (buf2_L[spt2]+tb)*REV_LPF_INP + ta*REV_WIDTH;
        ta = buf3_L[spt3];
        s  = buf3_L[spt3] = buf0_L[spt0];
        buf0_L[spt0] = -LPFL;

        t = (HPFL + fixp) * REV_HPF_LEV;
        HPFL = t - fixp;

        buf2_L[spt2] = (s - fixp * REV_FBK_LEV) * REV_NMIX_LEV;
        tb = buf1_L[spt1];
        buf1_L[spt1] = t;

        EPFL = EPFL * REV_EPF_LEV + ta * REV_EPF_INP;
        comp[i] = ta + EPFL + fixp;

        /* R */
        fixp = comp[++i];

        LPFR = LPFR*REV_LPF_LEV + (buf2_R[spt2]+tb)*REV_LPF_INP + ta*REV_WIDTH;
        ta = buf3_R[spt3];
        s  = buf3_R[spt3] = buf0_R[spt0];
        buf0_R[spt0] = LPFR;

        t = (HPFR + fixp) * REV_HPF_LEV;
        HPFR = t - fixp;

        buf2_R[spt2] = (s - fixp * REV_FBK_LEV) * REV_NMIX_LEV;
        tb = buf1_R[spt1];
        buf1_R[spt1] = t;

        EPFR = EPFR * REV_EPF_LEV + ta * REV_EPF_INP;
        comp[i] = ta + EPFR + fixp;

        rev_ptinc();
    }
}

void do_mono_reverb(int32 *comp, int32 n)
{
    int32  fixp, s, t, i;

    for(i = 0; i < n; i++)
    {
        /* L */
        fixp = comp[i] * REV_MONO_LEV;

        LPFL = LPFL*REV_LPF_LEV + (buf2_L[spt2]+tb)*REV_LPF_INP + ta*REV_WIDTH;
        ta = buf3_L[spt3];
        s  = buf3_L[spt3] = buf0_L[spt0];
        buf0_L[spt0] = -LPFL;

        t = (HPFL + fixp) * REV_HPF_LEV;
        HPFL = t - fixp;

        buf2_L[spt2] = (s - fixp * REV_FBK_LEV) * REV_NMIX_LEV;
        tb = buf1_L[spt1];
        buf1_L[spt1] = t;

        /* R */
        LPFR = LPFR*REV_LPF_LEV + (buf2_R[spt2]+tb)*REV_LPF_INP + ta*REV_WIDTH;
        ta = buf3_R[spt3];
        s  = buf3_R[spt3] = buf0_R[spt0];
        buf0_R[spt0] = LPFR;

        t = (HPFR + fixp) * REV_HPF_LEV;
        HPFR = t - fixp;

        buf2_R[spt2] = (s - fixp * REV_FBK_LEV) * REV_NMIX_LEV;
        tb = buf1_R[spt1];
        buf1_R[spt1] = t;

        EPFR = EPFR * REV_EPF_LEV + ta * REV_EPF_INP;
        comp[i] = ta + EPFR + fixp;

        rev_ptinc();
    }
}

/* dummy */
void reverb_rc_event(int rc, int32 val)
{
    switch(rc)
    {
      case RC_CHANGE_REV_EFFB:
        break;
      case RC_CHANGE_REV_TIME:
        break;
    }
}


/*                             */
/*   Delay (Celeste) Effect    */
/*                             */
#ifdef USE_DSP_EFFECT
static int32 delay_effect_buffer[AUDIO_BUFFER_SIZE * 2];
/* circular buffers and pointers */
#define DELAY_BUFFER_SIZE 48000 + 1
static int32 delay_buf0_L[DELAY_BUFFER_SIZE + 1];
static int32 delay_buf0_R[DELAY_BUFFER_SIZE + 1];
static int32 delay_rpt0 = DELAY_BUFFER_SIZE;
static int32 delay_wpt0;
static int32 delay_spt0;
static int32 delay_spt1;
static int32 delay_spt2;
#endif /* USE_DSP_EFFECT */

void do_basic_delay(int32* buf, int32 count);
void do_cross_delay(int32* buf, int32 count);
void do_3tap_delay(int32* buf, int32 count);

void init_ch_delay()
{
#ifdef USE_DSP_EFFECT
	memset(delay_buf0_L, 0, sizeof(delay_buf0_L));
	memset(delay_buf0_R, 0, sizeof(delay_buf0_R));
	memset(delay_effect_buffer, 0, sizeof(delay_effect_buffer));
	/* clear delay-line of LPF */
	delay_status.lpf.last_freq = 0;
	calc_filter_iir1_lowpass(&(delay_status.lpf));

	delay_wpt0 = 0;
	delay_spt0 = 0;
	delay_spt1 = 0;
	delay_spt2 = 0;
#endif /* USE_DSP_EFFECT */
}

#ifdef USE_DSP_EFFECT
void do_ch_delay(int32 *buf, int32 count)
{
	if ((opt_reverb_control == 3 || opt_reverb_control == 4
			|| opt_reverb_control < 0 && ! (opt_reverb_control & 0x100)
			|| opt_effect_quality >= 1) && delay_status.pre_lpf)
		do_filter_iir1_lowpass_stereo(delay_effect_buffer, count, &(delay_status.lpf));
	switch (delay_status.type) {
	case 1:
		do_3tap_delay(buf, count);
		break;
	case 2:
		do_cross_delay(buf, count);
		break;
	default:
		do_basic_delay(buf, count);
		break;
	}
}

#if OPT_MODE != 0
#if _MSC_VER
void set_ch_delay(int32 *buf, int32 count, int32 level)
{
	int32 *dbuf = delay_effect_buffer;
	level = level * 65536 / 127;

	_asm {
		mov		ecx, [count]
		mov		esi, [buf]
		mov		ebx, [level]
		test	ecx, ecx
		jz		short L2
		mov		edi, [dbuf]
L1:		mov		eax, [esi]
		imul	ebx
		shr		eax, 16
		shl		edx, 16
		or		eax, edx	/* u */
		mov		edx, [edi]	/* v */
		add		esi, 4		/* u */	
		add		edx, eax	/* v */
		mov		[edi], edx	/* u */
		add		edi, 4		/* v */
		dec		ecx			/* u */
		jnz		L1			/* v */
L2:
	}
}
#else
void set_ch_delay(register int32 *sbuffer, int32 n, int32 level)
{
    register int32 i;
	int32 *buf = delay_effect_buffer;
	level = level * 65536 / 127;

	for(i=n-1;i>=0;i--) {buf[i] += imuldiv16(sbuffer[i], level);}
}
#endif	/* _MSC_VER */
#else
void set_ch_delay(register int32 *sbuffer, int32 n, int32 level)
{
    register int32 i;
	register int32 count = n;
    FLOAT_T send_level = (FLOAT_T)level / 127.0;

    for(i=0;i<count;i++)
    {
        delay_effect_buffer[i] += sbuffer[i] * send_level;
    }
}
#endif /* OPT_MODE != 0 */

#if OPT_MODE != 0
void do_basic_delay(int32* buf, int32 count)
{
	register int32 i;
	register int32 n = count;
	int32 level,feedback,output,send_reverb;

	level = TIM_FSCALE(delay_status.level_ratio_c * MASTER_DELAY_LEVEL, 24);
	feedback = TIM_FSCALE(delay_status.feedback_ratio, 24);
	send_reverb = TIM_FSCALE(delay_status.send_reverb_ratio * REV_INP_LEV, 24);

	delay_spt0 = delay_wpt0 - delay_status.sample_c;
	if(delay_spt0 < 0) {delay_spt0 += delay_rpt0;}

	for(i=0;i<n;i++) {
		delay_buf0_L[delay_wpt0] = delay_effect_buffer[i] + imuldiv24(delay_buf0_L[delay_spt0], feedback);
		output = imuldiv24(delay_buf0_L[delay_spt0], level);
		buf[i] += output;
		effect_buffer[i] += imuldiv24(output, send_reverb);
		delay_effect_buffer[i] = 0;

		delay_buf0_R[delay_wpt0] = delay_effect_buffer[++i] + imuldiv24(delay_buf0_R[delay_spt0], feedback);
		output = imuldiv24(delay_buf0_R[delay_spt0], level);
		buf[i] += output;
		effect_buffer[i] += imuldiv24(output, send_reverb);
		delay_effect_buffer[i] = 0;

		if(++delay_spt0 == delay_rpt0) {delay_spt0 = 0;}
		if(++delay_wpt0 == delay_rpt0) {delay_wpt0 = 0;}
	}
}
#else
void do_basic_delay(int32* buf, int32 count)
{
	register int32 i;
	register int32 n = count;
	FLOAT_T level,feedback;

	level = delay_status.level_ratio_c * MASTER_DELAY_LEVEL;
	feedback = delay_status.feedback_ratio;

	delay_spt0 = delay_wpt0 - delay_status.sample_c;
	if(delay_spt0 < 0) {delay_spt0 += delay_rpt0;}

	for(i=0;i<n;i++) {
		delay_buf0_L[delay_wpt0] = delay_effect_buffer[i] + delay_buf0_L[delay_spt0] * feedback;
		buf[i] += delay_buf0_L[delay_spt0] * level;
		delay_effect_buffer[i] = 0;

		delay_buf0_R[delay_wpt0] = delay_effect_buffer[++i] + delay_buf0_R[delay_spt0] * feedback;
		buf[i] += delay_buf0_R[delay_spt0] * level;
		delay_effect_buffer[i] = 0;

		if(++delay_spt0 == delay_rpt0) {delay_spt0 = 0;}
		if(++delay_wpt0 == delay_rpt0) {delay_wpt0 = 0;}
	}
}
#endif /* OPT_MODE != 0 */

#if OPT_MODE != 0
void do_cross_delay(int32* buf, int32 count)
{
	register int32 i;
	register int32 n = count;
	int32 feedback,level_c,level_l,level_r,send_reverb,output;

	feedback = TIM_FSCALE(delay_status.feedback_ratio, 24);
	level_c = TIM_FSCALE(delay_status.level_ratio_c * MASTER_DELAY_LEVEL, 24);
	level_l = TIM_FSCALE(delay_status.level_ratio_l * MASTER_DELAY_LEVEL, 24);
	level_r = TIM_FSCALE(delay_status.level_ratio_r * MASTER_DELAY_LEVEL, 24);
	send_reverb = TIM_FSCALE(delay_status.send_reverb_ratio * REV_INP_LEV, 24);

	delay_spt0 = delay_wpt0 - delay_status.sample_c;
	if(delay_spt0 < 0) {delay_spt0 += delay_rpt0;}
	delay_spt1 = delay_wpt0 - delay_status.sample_l;
	if(delay_spt1 < 0) {delay_spt1 += delay_rpt0;}
	delay_spt2 = delay_wpt0 - delay_status.sample_r;
	if(delay_spt2 < 0) {delay_spt2 += delay_rpt0;}

	for(i=0;i<n;i++) {
		delay_buf0_L[delay_wpt0] = delay_effect_buffer[i] + imuldiv24(delay_buf0_R[delay_spt0],feedback);
		output = imuldiv24(delay_buf0_L[delay_spt0],level_c) + imuldiv24(delay_buf0_L[delay_spt1] + delay_buf0_R[delay_spt1],level_l);
		buf[i] += output;
		effect_buffer[i] += imuldiv24(output,send_reverb);
		delay_effect_buffer[i] = 0;

		delay_buf0_R[delay_wpt0] = delay_effect_buffer[++i] + imuldiv24(delay_buf0_L[delay_spt0],feedback);
		output = imuldiv24(delay_buf0_R[delay_spt0],level_c) + imuldiv24(delay_buf0_L[delay_spt2] + delay_buf0_R[delay_spt2],level_r);
		buf[i] += output;
		effect_buffer[i] += imuldiv24(output,send_reverb);
		delay_effect_buffer[i] = 0;

		if(++delay_spt0 == delay_rpt0) {delay_spt0 = 0;}
		if(++delay_spt1 == delay_rpt0) {delay_spt1 = 0;}
		if(++delay_spt2 == delay_rpt0) {delay_spt2 = 0;}
		if(++delay_wpt0 == delay_rpt0) {delay_wpt0 = 0;}
	}
}
#else
void do_cross_delay(int32* buf, int32 count)
{
	register int32 i;
	register int32 n = count;
	FLOAT_T feedback,level_c,level_l,level_r;

	feedback = delay_status.feedback_ratio;
	level_c = delay_status.level_ratio_c * MASTER_DELAY_LEVEL;
	level_l = delay_status.level_ratio_l * MASTER_DELAY_LEVEL;
	level_r = delay_status.level_ratio_r * MASTER_DELAY_LEVEL;

	delay_spt0 = delay_wpt0 - delay_status.sample_c;
	if(delay_spt0 < 0) {delay_spt0 += delay_rpt0;}
	delay_spt1 = delay_wpt0 - delay_status.sample_l;
	if(delay_spt1 < 0) {delay_spt1 += delay_rpt0;}
	delay_spt2 = delay_wpt0 - delay_status.sample_r;
	if(delay_spt2 < 0) {delay_spt2 += delay_rpt0;}

	for(i=0;i<n;i++) {
		delay_buf0_L[delay_wpt0] = delay_effect_buffer[i] + delay_buf0_R[delay_spt0] * feedback;
		buf[i] += delay_buf0_L[delay_spt0] * level_c + (delay_buf0_L[delay_spt1] + delay_buf0_R[delay_spt1]) * level_l;
		delay_effect_buffer[i] = 0;

		delay_buf0_R[delay_wpt0] = delay_effect_buffer[++i] + delay_buf0_L[delay_spt0] * feedback;
		buf[i] += delay_buf0_R[delay_spt0] * level_c + (delay_buf0_L[delay_spt2] + delay_buf0_R[delay_spt2]) * level_r;
		delay_effect_buffer[i] = 0;

		if(++delay_spt0 == delay_rpt0) {delay_spt0 = 0;}
		if(++delay_spt1 == delay_rpt0) {delay_spt1 = 0;}
		if(++delay_spt2 == delay_rpt0) {delay_spt2 = 0;}
		if(++delay_wpt0 == delay_rpt0) {delay_wpt0 = 0;}
	}
}
#endif /* OPT_MODE != 0 */

#if OPT_MODE != 0
void do_3tap_delay(int32* buf, int32 count)
{
	register int32 i;
	register int32 n = count;
	int32 feedback,level_c,level_l,level_r,output,send_reverb;

	feedback = TIM_FSCALE(delay_status.feedback_ratio, 24);
	level_c = TIM_FSCALE(delay_status.level_ratio_c * MASTER_DELAY_LEVEL, 24);
	level_l = TIM_FSCALE(delay_status.level_ratio_l * MASTER_DELAY_LEVEL, 24);
	level_r = TIM_FSCALE(delay_status.level_ratio_r * MASTER_DELAY_LEVEL, 24);
	send_reverb = TIM_FSCALE(delay_status.send_reverb_ratio * REV_INP_LEV, 24);

	delay_spt0 = delay_wpt0 - delay_status.sample_c;
	if(delay_spt0 < 0) {delay_spt0 += delay_rpt0;}
	delay_spt1 = delay_wpt0 - delay_status.sample_l;
	if(delay_spt1 < 0) {delay_spt1 += delay_rpt0;}
	delay_spt2 = delay_wpt0 - delay_status.sample_r;
	if(delay_spt2 < 0) {delay_spt2 += delay_rpt0;}

	for(i=0;i<n;i++) {
		delay_buf0_L[delay_wpt0] = delay_effect_buffer[i] + imuldiv24(delay_buf0_L[delay_spt0],feedback);
		output = imuldiv24(delay_buf0_L[delay_spt0],level_c) + imuldiv24(delay_buf0_L[delay_spt1] + delay_buf0_R[delay_spt1],level_l);
		buf[i] += output;
		effect_buffer[i] += imuldiv24(output,send_reverb);
		delay_effect_buffer[i] = 0;

		delay_buf0_R[delay_wpt0] = delay_effect_buffer[++i] + imuldiv24(delay_buf0_R[delay_spt0],feedback);
		output = imuldiv24(delay_buf0_R[delay_spt0],level_c) + imuldiv24(delay_buf0_L[delay_spt2] + delay_buf0_R[delay_spt2],level_r);
		buf[i] += output;
		effect_buffer[i] += imuldiv24(output,send_reverb);
		delay_effect_buffer[i] = 0;

		if(++delay_spt0 == delay_rpt0) {delay_spt0 = 0;}
		if(++delay_spt1 == delay_rpt0) {delay_spt1 = 0;}
		if(++delay_spt2 == delay_rpt0) {delay_spt2 = 0;}
		if(++delay_wpt0 == delay_rpt0) {delay_wpt0 = 0;}
	}
}
#else
void do_3tap_delay(int32* buf, int32 count)
{
	register int32 i;
	register int32 n = count;
	FLOAT_T feedback,level_c,level_l,level_r;

	feedback = delay_status.feedback_ratio;
	level_c = delay_status.level_ratio_c * MASTER_DELAY_LEVEL;
	level_l = delay_status.level_ratio_l * MASTER_DELAY_LEVEL;
	level_r = delay_status.level_ratio_r * MASTER_DELAY_LEVEL;

	delay_spt0 = delay_wpt0 - delay_status.sample_c;
	if(delay_spt0 < 0) {delay_spt0 += delay_rpt0;}
	delay_spt1 = delay_wpt0 - delay_status.sample_l;
	if(delay_spt1 < 0) {delay_spt1 += delay_rpt0;}
	delay_spt2 = delay_wpt0 - delay_status.sample_r;
	if(delay_spt2 < 0) {delay_spt2 += delay_rpt0;}

	for(i=0;i<n;i++) {
		delay_buf0_L[delay_wpt0] = delay_effect_buffer[i] + delay_buf0_L[delay_spt0] * feedback;
		buf[i] += delay_buf0_L[delay_spt0] * level_c + (delay_buf0_L[delay_spt1] + delay_buf0_R[delay_spt1]) * level_l;
		delay_effect_buffer[i] = 0;

		delay_buf0_R[delay_wpt0] = delay_effect_buffer[++i] + delay_buf0_R[delay_spt0] * feedback;
		buf[i] += delay_buf0_R[delay_spt0] * level_c + (delay_buf0_L[delay_spt2] + delay_buf0_R[delay_spt2]) * level_r;
		delay_effect_buffer[i] = 0;

		if(++delay_spt0 == delay_rpt0) {delay_spt0 = 0;}
		if(++delay_spt1 == delay_rpt0) {delay_spt1 = 0;}
		if(++delay_spt2 == delay_rpt0) {delay_spt2 = 0;}
		if(++delay_wpt0 == delay_rpt0) {delay_wpt0 = 0;}
	}
}
#endif /* OPT_MODE != 0 */
#endif /* USE_DSP_EFFECT */


/*                             */
/*        Chorus Effect        */
/*                             */
#ifdef USE_DSP_EFFECT
static int32 chorus_effect_buffer[AUDIO_BUFFER_SIZE * 2];
/* circular buffers and pointers */
#define CHORUS_BUFFER_SIZE 9600
static int32 chorus_buf0_L[CHORUS_BUFFER_SIZE + 1];
static int32 chorus_buf0_R[CHORUS_BUFFER_SIZE + 1];
static int32 chorus_rpt0 = CHORUS_BUFFER_SIZE;
static int32 chorus_wpt0, chorus_wpt1, chorus_spt0, chorus_spt1;
static int32 chorus_lfo0[SINE_CYCLE_LENGTH], chorus_lfo1[SINE_CYCLE_LENGTH];
static int32 chorus_cyc0, chorus_cnt0, chorus_hist0, chorus_hist1;
#endif /* USE_DSP_EFFECT */

void init_chorus_lfo(void)
{
#ifdef USE_DSP_EFFECT
	int32 i;

	chorus_cyc0 = chorus_param.cycle_in_sample;
	if(chorus_cyc0 == 0) {chorus_cyc0 = 1;}

	for(i=0;i<SINE_CYCLE_LENGTH;i++) {
		chorus_lfo0[i] = TIM_FSCALE((lookup_sine(i) + 1.0) / 2, 16);
	}

	for(i=0;i<SINE_CYCLE_LENGTH;i++) {
		chorus_lfo1[i] = TIM_FSCALE((lookup_sine(i + SINE_CYCLE_LENGTH / 4) + 1.0) / 2, 16);
	}
#endif /* USE_DSP_EFFECT */
}

void init_ch_chorus()
{
#ifdef USE_DSP_EFFECT
	memset(chorus_buf0_L, 0, sizeof(chorus_buf0_L));
	memset(chorus_buf0_R, 0, sizeof(chorus_buf0_R));
	memset(chorus_effect_buffer, 0, sizeof(chorus_effect_buffer));
	/* clear delay-line of LPF */
	chorus_param.lpf.last_freq = 0;
	calc_filter_iir1_lowpass(&(chorus_param.lpf));

	chorus_cnt0 = chorus_wpt0 = chorus_wpt1 = 0;
	chorus_spt0 = chorus_spt1 = chorus_hist0 = chorus_hist1 = 0;
#endif /* USE_DSP_EFFECT */
}

#ifdef USE_DSP_EFFECT
/*! chorus effect for GS mode; but, in reality, it's used for GM and XG mode at present. */ 
void do_stereo_chorus(int32* buf, register int32 count)
{
#if OPT_MODE != 0	/* fixed-point implementation */
	int32 i, level, feedback, send_reverb, send_delay, pdelay,
		depth, output, icycle, hist0, hist1, f0, f1, v0, v1;

	level = TIM_FSCALE(chorus_param.level_ratio * MASTER_CHORUS_LEVEL, 24);
	feedback = TIM_FSCALE(chorus_param.feedback_ratio, 24);
	send_reverb = TIM_FSCALE(chorus_param.send_reverb_ratio * REV_INP_LEV, 24);
	send_delay = TIM_FSCALE(chorus_param.send_delay_ratio, 24);
	depth = chorus_param.depth_in_sample;
	pdelay = chorus_param.delay_in_sample;
	icycle = TIM_FSCALE((SINE_CYCLE_LENGTH - 1) / (double)chorus_cyc0, 24) - 0.5;
	hist0 = chorus_hist0, hist1 = chorus_hist1;

	/* LFO */
	f0 = imuldiv24(chorus_lfo0[imuldiv24(chorus_cnt0, icycle)], depth);
	chorus_spt0 = chorus_wpt0 - pdelay - (f0 >> 8);	/* integral part of delay */
	f0 = 0xFF - (f0 & 0xFF);	/* (1 - frac) * 256 */
	if(chorus_spt0 < 0) {chorus_spt0 += chorus_rpt0;}
	f1 = imuldiv24(chorus_lfo1[imuldiv24(chorus_cnt0, icycle)], depth);
	chorus_spt1 = chorus_wpt1 - pdelay - (f1 >> 8);	/* integral part of delay */
	f1 = 0xFF - (f1 & 0xFF);	/* (1 - frac) * 256 */
	if(chorus_spt1 < 0) {chorus_spt1 += chorus_rpt0;}
	
	for(i = 0; i < count; i++) {
		v0 = chorus_buf0_L[chorus_spt0];
		v1 = chorus_buf0_R[chorus_spt1];

		/* LFO */
		if(++chorus_wpt0 == chorus_rpt0) {chorus_wpt0 = 0;}
		chorus_wpt1 = chorus_wpt0;
		f0 = imuldiv24(chorus_lfo0[imuldiv24(chorus_cnt0, icycle)], depth);
		chorus_spt0 = chorus_wpt0 - pdelay - (f0 >> 8);	/* integral part of delay */
		f0 = 0xFF - (f0 & 0xFF);	/* (1 - frac) * 256 */
		if(chorus_spt0 < 0) {chorus_spt0 += chorus_rpt0;}
		f1 = imuldiv24(chorus_lfo1[imuldiv24(chorus_cnt0, icycle)], depth);
		chorus_spt1 = chorus_wpt1 - pdelay - (f1 >> 8);	/* integral part of delay */
		f1 = 0xFF - (f1 & 0xFF);	/* (1 - frac) * 256 */
		if(chorus_spt1 < 0) {chorus_spt1 += chorus_rpt0;}
		if(++chorus_cnt0 == chorus_cyc0) {chorus_cnt0 = 0;}

		/* left */
		/* delay with all-pass interpolation */
		output = hist0 = hist0 + imuldiv8(chorus_buf0_L[chorus_spt0] - v0, f0);
		chorus_buf0_L[chorus_wpt0] = chorus_effect_buffer[i] + imuldiv24(output, feedback);
		output = imuldiv24(output, level);
		buf[i] += output;
		/* send to other system effects (it's peculiar to GS) */
		effect_buffer[i] += imuldiv24(output, send_reverb);
		delay_effect_buffer[i] += imuldiv24(output, send_delay);
		chorus_effect_buffer[i] = 0;

		/* right */
		/* delay with all-pass interpolation */
		output = hist1 = hist1 + imuldiv8(chorus_buf0_R[chorus_spt1] - v1, f1);
		chorus_buf0_R[chorus_wpt1] = chorus_effect_buffer[++i] + imuldiv24(output, feedback);
		output = imuldiv24(output, level);
		buf[i] += output;
		/* send to other system effects (it's peculiar to GS) */
		effect_buffer[i] += imuldiv24(output, send_reverb);
		delay_effect_buffer[i] += imuldiv24(output, send_delay);
		chorus_effect_buffer[i] = 0;
	}
	chorus_hist0 = hist0, chorus_hist1 = hist1;
#endif /* OPT_MODE != 0 */
}

#if OPT_MODE != 0	/* fixed-point implementation */
#if _MSC_VER
void set_ch_chorus(int32 *buf, int32 count, int32 level)
{
	int32 *dbuf = chorus_effect_buffer;
	level = level * 65536 / 127;

	_asm {
		mov		ecx, [count]
		mov		esi, [buf]
		mov		ebx, [level]
		test	ecx, ecx
		jz		short L2
		mov		edi, [dbuf]
L1:		mov		eax, [esi]
		imul	ebx
		shr		eax, 16
		shl		edx, 16
		or		eax, edx	/* u */
		mov		edx, [edi]	/* v */
		add		esi, 4		/* u */	
		add		edx, eax	/* v */
		mov		[edi], edx	/* u */
		add		edi, 4		/* v */
		dec		ecx			/* u */
		jnz		L1			/* v */
L2:
	}
}
#else
void set_ch_chorus(register int32 *sbuffer,int32 n, int32 level)
{
    register int32 i;
	int32 *buf = chorus_effect_buffer;
	level = level * 65536 / 127;

	for(i=n-1;i>=0;i--) {buf[i] += imuldiv16(sbuffer[i], level);}
}
#endif	/* _MSC_VER */
#else	/* floating-point implementation */
void set_ch_chorus(register int32 *sbuffer,int32 n, int32 level)
{
    register int32 i;
    register int32 count = n;
    FLOAT_T send_level = (FLOAT_T)level / 127.0;

    for(i=0;i<count;i++)
    {
		chorus_effect_buffer[i] += sbuffer[i] * send_level;
    }
}
#endif /* OPT_MODE != 0 */

void do_ch_chorus(int32 *buf, int32 count)
{
	if ((opt_reverb_control == 3 || opt_reverb_control == 4
			|| opt_reverb_control < 0 && ! (opt_reverb_control & 0x100)
			|| opt_effect_quality >= 1) && chorus_param.chorus_pre_lpf)
		do_filter_iir1_lowpass_stereo(chorus_effect_buffer, count, &(chorus_param.lpf));

	do_stereo_chorus(buf, count);
}
#endif /* USE_DSP_EFFECT */


/*                             */
/*       EQ (Equalizer)        */
/*                             */
static int32 eq_buffer[AUDIO_BUFFER_SIZE * 2];

void init_eq()
{
	memset(eq_buffer, 0, sizeof(eq_buffer));
	memset(eq_status.low_val, 0, sizeof(eq_status.low_val));
	memset(eq_status.high_val, 0, sizeof(eq_status.high_val));
}

void calc_lowshelf_coefs(int32* coef, int32 cutoff_freq, FLOAT_T dbGain, int32 rate)
{
	FLOAT_T a0, a1, a2, b0, b1, b2, omega, sn, cs, A, beta;

	A = pow(10, dbGain / 40);
	omega = 2.0 * M_PI * (FLOAT_T)cutoff_freq / (FLOAT_T)rate;
	sn = sin(omega);
	cs = cos(omega);
	beta = sqrt(A + A);

	a0 = 1.0 / ((A + 1) + (A - 1) * cs + beta * sn);
	a1 = 2.0 * ((A - 1) + (A + 1) * cs);
	a2 = -((A + 1) + (A - 1) * cs - beta * sn);
	b0 = A * ((A + 1) - (A - 1) * cs + beta * sn);
	b1 = 2.0 * A * ((A - 1) - (A + 1) * cs);
	b2 = A * ((A + 1) - (A - 1) * cs - beta * sn);

	a1 *= a0;
	a2 *= a0;
	b1 *= a0;
	b2 *= a0;
	b0 *= a0;

	coef[0] = TIM_FSCALE(a1, 24);
	coef[1] = TIM_FSCALE(a2, 24);
	coef[2] = TIM_FSCALE(b0, 24);
	coef[3] = TIM_FSCALE(b1, 24);
	coef[4] = TIM_FSCALE(b2, 24);
}

void calc_highshelf_coefs(int32* coef, int32 cutoff_freq, FLOAT_T dbGain, int32 rate)
{
	FLOAT_T a0, a1, a2, b0, b1, b2, omega, sn, cs, A, beta;

	A = pow(10, dbGain / 40);
	omega = 2.0 * M_PI * (FLOAT_T)cutoff_freq / (FLOAT_T)rate;
	sn = sin(omega);
	cs = cos(omega);
	beta = sqrt(A + A);

	a0 = 1.0 / ((A + 1) - (A - 1) * cs + beta * sn);
	a1 = (-2 * ((A - 1) - (A + 1) * cs));
	a2 = -((A + 1) - (A - 1) * cs - beta * sn);
	b0 = A * ((A + 1) + (A - 1) * cs + beta * sn);
	b1 = -2 * A * ((A - 1) + (A + 1) * cs);
	b2 = A * ((A + 1) + (A - 1) * cs - beta * sn);

	a1 *= a0;
	a2 *= a0;
	b0 *= a0;
	b1 *= a0;
	b2 *= a0;

	coef[0] = TIM_FSCALE(a1, 24);
	coef[1] = TIM_FSCALE(a2, 24);
	coef[2] = TIM_FSCALE(b0, 24);
	coef[3] = TIM_FSCALE(b1, 24);
	coef[4] = TIM_FSCALE(b2, 24);
}


static void do_shelving_filter(register int32* buf, int32 count, int32* eq_coef, int32* eq_val)
{
#if OPT_MODE != 0
	register int32 i;
	int32 x1l, x2l, y1l, y2l, x1r, x2r, y1r, y2r, yout;
	int32 a1, a2, b0, b1, b2;

	a1 = eq_coef[0];
	a2 = eq_coef[1];
	b0 = eq_coef[2];
	b1 = eq_coef[3];
	b2 = eq_coef[4];

	x1l = eq_val[0];
	x2l = eq_val[1];
	y1l = eq_val[2];
	y2l = eq_val[3];
	x1r = eq_val[4];
	x2r = eq_val[5];
	y1r = eq_val[6];
	y2r = eq_val[7];

	for(i=0;i<count;i++) {
		yout = imuldiv24(buf[i], b0) + imuldiv24(x1l, b1) + imuldiv24(x2l, b2) + imuldiv24(y1l, a1) + imuldiv24(y2l, a2);
		x2l = x1l;
		x1l = buf[i];
		y2l = y1l;
		y1l = yout;
		buf[i] = yout;

		yout = imuldiv24(buf[++i], b0) + imuldiv24(x1r, b1) + imuldiv24(x2r, b2) + imuldiv24(y1r, a1) + imuldiv24(y2r, a2);
		x2r = x1r;
		x1r = buf[i];
		y2r = y1r;
		y1r = yout;
		buf[i] = yout;
	}

	eq_val[0] = x1l;
	eq_val[1] = x2l;
	eq_val[2] = y1l;
	eq_val[3] = y2l;
	eq_val[4] = x1r;
	eq_val[5] = x2r;
	eq_val[6] = y1r;
	eq_val[7] = y2r;
#endif /* OPT_MODE != 0 */
}

void do_ch_eq(int32* buf,int32 n)
{
	register int32 i;
	register int32 count = n;

	do_shelving_filter(eq_buffer,count,eq_status.low_coef,eq_status.low_val);
	do_shelving_filter(eq_buffer,count,eq_status.high_coef,eq_status.high_val);

	for(i=0;i<count;i++) {
		buf[i] += eq_buffer[i];
		eq_buffer[i] = 0;
	}
}

#if OPT_MODE != 0
#if _MSC_VER
void set_ch_eq(int32 *buf, int32 count)
{
	int32 *dbuf = eq_buffer;
	_asm {
		mov		ecx, [count]
		mov		esi, [buf]
		test	ecx, ecx
		jz		short L2
		mov		edi, [dbuf]
L1:		mov		eax, [esi]
		mov		ebx, [edi]
		add		esi, 4
		add		ebx, eax
		mov		[edi], ebx
		add		edi, 4
		dec		ecx
		jnz		L1
L2:
	}
}
#else
void set_ch_eq(register int32 *buf, int32 n)
{
    register int32 i;

    for(i=n-1;i>=0;i--)
    {
        eq_buffer[i] += buf[i];
    }
}
#endif	/* _MSC_VER */
#else
void set_ch_eq(register int32 *sbuffer, int32 n)
{
    register int32  i;
    
	for(i = 0; i < n; i++)
    {
        eq_buffer[i] += sbuffer[i];
    }
}
#endif /* OPT_MODE != 0 */


/*                             */
/*   LPF for System Effects    */
/*                             */

void calc_lowpass_coefs_24db(int32* lpf_coef,int32 cutoff_freq,int16 resonance,int32 rate)
{
	FLOAT_T c,a1,a2,a3,b1,b2,q;
	const FLOAT_T sqrt2 = 1.4142134;

	/*memset(lpf_coef, 0, sizeof(lpf_coef));*/

	q = sqrt2 - (sqrt2 - 0.1) * (FLOAT_T)resonance / 127;

	c = (cutoff_freq == 0) ? 0 : (1.0 / tan(M_PI * (FLOAT_T)cutoff_freq / (FLOAT_T)rate));

	a1 = 1.0 / (1.0 + q * c + c * c); 
	a2 = 2* a1; 
	a3 = a1; 
	b1 = -(2.0 * (1.0 - c * c) * a1); 
	b2 = -(1.0 - q * c + c * c) * a1; 

	lpf_coef[0] = TIM_FSCALE(a1, 24);
	lpf_coef[1] = TIM_FSCALE(a2, 24);
	lpf_coef[2] = TIM_FSCALE(a3, 24);
	lpf_coef[3] = TIM_FSCALE(b1, 24);
	lpf_coef[4] = TIM_FSCALE(b2, 24);
}

void do_lowpass_24db(register int32* buf,int32 count,int32* lpf_coef,int32* lpf_val)
{
#if OPT_MODE != 0
	register int32 i,length;
	int32 x1l,x2l,y1l,y2l,x1r,x2r,y1r,y2r,yout;
	int32 a1,a2,a3,b1,b2;

	a1 = lpf_coef[0];
	a2 = lpf_coef[1];
	a3 = lpf_coef[2];
	b1 = lpf_coef[3];
	b2 = lpf_coef[4];

	length = count;

	x1l = lpf_val[0];
	x2l = lpf_val[1];
	y1l = lpf_val[2];
	y2l = lpf_val[3];
	x1r = lpf_val[4];
	x2r = lpf_val[5];
	y1r = lpf_val[6];
	y2r = lpf_val[7];

	for(i=0;i<length;i++) {
		yout = imuldiv24(buf[i] + x2l,a1) + imuldiv24(x1l,a2) + imuldiv24(y1l,b1) + imuldiv24(y2l,b2);
		x2l = x1l;
		x1l = buf[i];
		buf[i] = yout;
		y2l = y1l;
		y1l = yout;

		yout = imuldiv24(buf[++i] + x2r,a1) + imuldiv24(x1r,a2) + imuldiv24(y1r,b1) + imuldiv24(y2r,b2);
		x2r = x1r;
		x1r = buf[i];
		buf[i] = yout;
		y2r = y1r;
		y1r = yout;
	}

	lpf_val[0] = x1l;
	lpf_val[1] = x2l;
	lpf_val[2] = y1l;
	lpf_val[3] = y2l;
	lpf_val[4] = x1r;
	lpf_val[5] = x2r;
	lpf_val[6] = y1r;
	lpf_val[7] = y2r;
#endif /* OPT_MODE != 0 */
}


/*                             */
/* Insertion Effect (SC-88Pro) */
/*                             */
/* !!! rename this function to do_insertion_effect_gs() !!! */
void do_insertion_effect(int32 *buf, int32 count)
{
	do_effect_list(buf, count, gs_ieffect.ef);
}


/*                             */
/* High Quality Reverb Effect  */
/*                             */

/* allpass filter */
typedef struct _allpass {
	int32 *buf;
	int32 size;
	int32 index;
	double feedback;
	int32 feedbacki;
} allpass;

static void setbuf_allpass(allpass *allpass, int32 size)
{
	if(allpass->buf != NULL) {free(allpass->buf);}
	allpass->buf = (int32 *)safe_malloc(sizeof(int32) * size);
	if(allpass->buf == NULL) {return;}
	allpass->index = 0;
	allpass->size = size;
}

static void init_allpass(allpass *allpass)
{
	memset(allpass->buf, 0, sizeof(int32) * allpass->size);
}

/* comb filter */
typedef struct _comb {
	int32 *buf;
	int32 filterstore;
	int32 size;
	int32 index;
	double feedback;
	double damp1;
	double damp2;
	int32 feedbacki;
	int32 damp1i;
	int32 damp2i;
} comb;

static void setbuf_comb(comb *comb, int32 size)
{
	if(comb->buf != NULL) {free(comb->buf);}
	comb->buf = (int32 *)safe_malloc(sizeof(int32) * size);
	if(comb->buf == NULL) {return;}
	comb->index = 0;
	comb->size = size;
	comb->filterstore = 0;
}

static void init_comb(comb *comb)
{
	memset(comb->buf, 0, sizeof(int32) * comb->size);
}

#define numcombs 8
#define numallpasses 4
#define scalewet 0.06f
#define scaledamp 0.4f
#define scaleroom 0.28f
#define offsetroom 0.7f
#define initialroom 0.5f
#define initialdamp 0.5f
#define initialwet 1 / scalewet
#define initialdry 0
#define initialwidth 0.5f
#define initialallpassfbk 0.65f
#define stereospread 23
static int combtunings[numcombs] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
static int allpasstunings[numallpasses] = {225, 341, 441, 556};
#define fixedgain 0.025f

typedef struct _revmodel_t {
	double roomsize, roomsize1;
	double damp, damp1;
	double wet, wet1, wet2;
	double width;
	comb combL[numcombs];
	comb combR[numcombs];
	allpass allpassL[numallpasses];
	allpass allpassR[numallpasses];
	int32 wet1i, wet2i;
} revmodel_t;

revmodel_t *revmodel = NULL;

static inline int isprime(int val)
{
  int i;

  if (val == 2) return 1;
  if (val & 1)
	{
	  for (i=3; i<(int)sqrt((double)val)+1; i+=2)
		{
		  if ((val%i) == 0) return 0;
		}
	  return 1; /* prime */
	}
  else return 0; /* even */
}

static double gs_revchar_to_roomsize(int character)
{
	double rs;
	switch(character) {
	case 0: rs = 1.0;	break;	/* Room 1 */
	case 1: rs = 0.94;	break;	/* Room 2 */
	case 2: rs = 0.97;	break;	/* Room 3 */
	case 3: rs = 0.90;	break;	/* Hall 1 */
	case 4: rs = 0.85;	break;	/* Hall 2 */
	default: rs = 1.0;	break;	/* Plate, Delay, Panning Delay */
	}
	return rs;
}

static double gs_revchar_to_level(int character)
{
	double level;
	switch(character) {
	case 0: level = 0.744025605;	break;	/* Room 1 */
	case 1: level = 1.224309745;	break;	/* Room 2 */
	case 2: level = 0.858592403;	break;	/* Room 3 */
	case 3: level = 1.0471802;	break;	/* Hall 1 */
	case 4: level = 1.0;	break;	/* Hall 2 */
	case 5: level = 0.865335496;	break;	/* Plate */
	default: level = 1.0;	break;	/* Delay, Panning Delay */
	}
	return level;
}

static double gs_revchar_to_rt(int character)
{
	double rt;
	switch(character) {
	case 0: rt = 0.516850262;	break;	/* Room 1 */
	case 1: rt = 1.004226004;	break;	/* Room 2 */
	case 2: rt = 0.691046825;	break;	/* Room 3 */
	case 3: rt = 0.893006004;	break;	/* Hall 1 */
	case 4: rt = 1.0;	break;	/* Hall 2 */
	case 5: rt = 0.538476488;	break;	/* Plate */
	default: rt = 1.0;	break;	/* Delay, Panning Delay */
	}
	return rt;
}

static double gs_revchar_to_width(int character)
{
	double width;
	switch(character) {
	case 0: width = 0.5;	break;	/* Room 1 */
	case 1: width = 0.5;	break;	/* Room 2 */
	case 2: width = 0.5;	break;	/* Room 3 */
	case 3: width = 0.5;	break;	/* Hall 1 */
	case 4: width = 0.5;	break;	/* Hall 2 */
	default: width = 0.5;	break;	/* Plate, Delay, Panning Delay */
	}
	return width;
}

static double gs_revchar_to_apfbk(int character)
{
	double apf;
	switch(character) {
	case 0: apf = 0.7;	break;	/* Room 1 */
	case 1: apf = 0.7;	break;	/* Room 2 */
	case 2: apf = 0.7;	break;	/* Room 3 */
	case 3: apf = 0.6;	break;	/* Hall 1 */
	case 4: apf = 0.55;	break;	/* Hall 2 */
	default: apf = 0.55;	break;	/* Plate, Delay, Panning Delay */
	}
	return apf;
}

static void recalc_reverb_buffer(revmodel_t *rev)
{
	int i;
	int32 tmpL, tmpR;
	double time;

	time = reverb_time_table[reverb_status.time] * gs_revchar_to_rt(reverb_status.character)
		/ (60 * combtunings[numcombs - 1] / (-20 * log10(rev->roomsize1) * 44100.0));

	for(i = 0; i < numcombs; i++)
	{
		tmpL = combtunings[i] * sample_rate * time / 44100.0;
		tmpR = (combtunings[i] + stereospread) * sample_rate * time / 44100.0;
		if(tmpL < 10) tmpL = 10;
		if(tmpR < 10) tmpR = 10;
		while(!isprime(tmpL)) tmpL++;
		while(!isprime(tmpR)) tmpR++;
		rev->combL[i].size = tmpL;
		rev->combR[i].size = tmpR;
		setbuf_comb(&rev->combL[i], rev->combL[i].size);
		setbuf_comb(&rev->combR[i], rev->combR[i].size);
	}

	for(i = 0; i < numallpasses; i++)
	{
		tmpL = allpasstunings[i] * sample_rate * time / 44100.0;
		tmpR = (allpasstunings[i] + stereospread) * sample_rate * time / 44100.0;
		tmpL *= sample_rate / 44100.0;
		tmpR *= sample_rate / 44100.0;
		if(tmpL < 10) tmpL = 10;
		if(tmpR < 10) tmpR = 10;
		while(!isprime(tmpL)) tmpL++;
		while(!isprime(tmpR)) tmpR++;
		rev->allpassL[i].size = tmpL;
		rev->allpassR[i].size = tmpR;
		setbuf_allpass(&rev->allpassL[i], rev->allpassL[i].size);
		setbuf_allpass(&rev->allpassR[i], rev->allpassR[i].size);
	}
}

static void update_revmodel(revmodel_t *rev)
{
	int i;
	double allpassfbk = gs_revchar_to_apfbk(reverb_status.character), rtbase;

	rev->wet = (double)reverb_status.level / 127.0 * gs_revchar_to_level(reverb_status.character);
	rev->roomsize = gs_revchar_to_roomsize(reverb_status.character) * scaleroom + offsetroom;
	rev->width = gs_revchar_to_width(reverb_status.character);

	rev->wet1 = rev->width / 2 + 0.5f;
	rev->wet2 = (1 - rev->width) / 2;
	rev->roomsize1 = rev->roomsize;
	rev->damp1 = rev->damp;

	recalc_reverb_buffer(rev);
	rtbase = 1.0 / (44100.0 * reverb_time_table[reverb_status.time] * gs_revchar_to_rt(reverb_status.character));

	for(i = 0; i < numcombs; i++)
	{
		rev->combL[i].feedback = pow(10, -3 * (double)combtunings[i] * rtbase);
		rev->combR[i].feedback = pow(10, -3 * (double)(combtunings[i] /*+ stereospread*/) * rtbase);
		rev->combL[i].damp1 = rev->damp1;
		rev->combR[i].damp1 = rev->damp1;
		rev->combL[i].damp2 = 1 - rev->damp1;
		rev->combR[i].damp2 = 1 - rev->damp1;
		rev->combL[i].damp1i = TIM_FSCALE(rev->combL[i].damp1, 24);
		rev->combR[i].damp1i = TIM_FSCALE(rev->combR[i].damp1, 24);
		rev->combL[i].damp2i = TIM_FSCALE(rev->combL[i].damp2, 24);
		rev->combR[i].damp2i = TIM_FSCALE(rev->combR[i].damp2, 24);
		rev->combL[i].feedbacki = TIM_FSCALE(rev->combL[i].feedback, 24);
		rev->combR[i].feedbacki = TIM_FSCALE(rev->combR[i].feedback, 24);
	}

	for(i = 0; i < numallpasses; i++)
	{
		rev->allpassL[i].feedback = allpassfbk;
		rev->allpassR[i].feedback = allpassfbk;
		rev->allpassL[i].feedbacki = TIM_FSCALE(rev->allpassL[i].feedback, 24);
		rev->allpassR[i].feedbacki = TIM_FSCALE(rev->allpassR[i].feedback, 24);
	}

	rev->wet1i = TIM_FSCALE(rev->wet1, 24);
	rev->wet2i = TIM_FSCALE(rev->wet2, 24);
}

static void init_revmodel(revmodel_t *rev)
{
	int i;
	for(i = 0; i < numcombs; i++) {
		init_comb(&rev->combL[i]);
		init_comb(&rev->combR[i]);
	}
	for(i = 0; i < numallpasses; i++) {
		init_allpass(&rev->allpassL[i]);
		init_allpass(&rev->allpassR[i]);
	}
}

static void alloc_revmodel(void)
{
	static int revmodel_alloc_flag = 0;
	revmodel_t *rev;
	if(revmodel_alloc_flag) {return;}
	if(revmodel == NULL) {
		revmodel = (revmodel_t *)safe_malloc(sizeof(revmodel_t));
		if(revmodel == NULL) {return;}
		memset(revmodel, 0, sizeof(revmodel_t));
	}
	rev = revmodel;

	setbuf_comb(&rev->combL[0], combtunings[0]);
	setbuf_comb(&rev->combR[0], combtunings[0] + stereospread);
	setbuf_comb(&rev->combL[1], combtunings[1]);
	setbuf_comb(&rev->combR[1], combtunings[1] + stereospread);
	setbuf_comb(&rev->combL[2], combtunings[2]);
	setbuf_comb(&rev->combR[2], combtunings[2] + stereospread);
	setbuf_comb(&rev->combL[3], combtunings[3]);
	setbuf_comb(&rev->combR[3], combtunings[3] + stereospread);
	setbuf_comb(&rev->combL[4], combtunings[4]);
	setbuf_comb(&rev->combR[4], combtunings[4] + stereospread);
	setbuf_comb(&rev->combL[5], combtunings[5]);
	setbuf_comb(&rev->combR[5], combtunings[5] + stereospread);
	setbuf_comb(&rev->combL[6], combtunings[6]);
	setbuf_comb(&rev->combR[6], combtunings[6] + stereospread);
	setbuf_comb(&rev->combL[7], combtunings[7]);
	setbuf_comb(&rev->combR[7], combtunings[7] + stereospread);

	setbuf_allpass(&rev->allpassL[0], allpasstunings[0]);
	setbuf_allpass(&rev->allpassR[0], allpasstunings[0] + stereospread);
	setbuf_allpass(&rev->allpassL[1], allpasstunings[1]);
	setbuf_allpass(&rev->allpassR[1], allpasstunings[1] + stereospread);
	setbuf_allpass(&rev->allpassL[2], allpasstunings[2]);
	setbuf_allpass(&rev->allpassR[2], allpasstunings[2] + stereospread);
	setbuf_allpass(&rev->allpassL[3], allpasstunings[3]);
	setbuf_allpass(&rev->allpassR[3], allpasstunings[3] + stereospread);

	rev->allpassL[0].feedback = initialallpassfbk;
	rev->allpassR[0].feedback = initialallpassfbk;
	rev->allpassL[1].feedback = initialallpassfbk;
	rev->allpassR[1].feedback = initialallpassfbk;
	rev->allpassL[2].feedback = initialallpassfbk;
	rev->allpassR[2].feedback = initialallpassfbk;
	rev->allpassL[3].feedback = initialallpassfbk;
	rev->allpassR[3].feedback = initialallpassfbk;

	rev->wet = initialwet * scalewet;
	rev->damp = initialdamp * scaledamp;
	rev->width = initialwidth;
	rev->roomsize = initialroom * scaleroom + offsetroom;

	revmodel_alloc_flag = 1;
}

static void free_revmodel(void)
{
	int i;
	if(revmodel != NULL) {
		for(i = 0; i < numcombs; i++)
		{
			if(revmodel->combL[i].buf != NULL)
				free(revmodel->combL[i].buf);
			if(revmodel->combR[i].buf != NULL)
				free(revmodel->combR[i].buf);
		}
		for(i = 0; i < numallpasses; i++)
		{
			if(revmodel->allpassL[i].buf != NULL)
				free(revmodel->allpassL[i].buf);
			if(revmodel->allpassR[i].buf != NULL)
				free(revmodel->allpassR[i].buf);
		}
		free(revmodel);
	}
}

#if OPT_MODE != 0	/* fixed-point implementation */
#define do_allpass(_stream, _apbuf, _apsize, _apindex, _apfeedback) \
{ \
	_rev_bufout = _apbuf[_apindex];	\
	_rev_output = -_stream + _rev_bufout;	\
	_apbuf[_apindex] = _stream + imuldiv24(_rev_bufout, _apfeedback);	\
	if(++_apindex >= _apsize) {	\
		_apindex = 0;	\
	}	\
	_stream = _rev_output;	\
}

#define do_comb(_input, _output, _cbuf, _csize, _cindex, _cdamp1, _cdamp2, _cfs, _cfeedback)	\
{	\
	_rev_output = _cbuf[_cindex];	\
	_cfs = imuldiv24(_rev_output, _cdamp2) + imuldiv24(_cfs, _cdamp1);	\
	_cbuf[_cindex] = _input + imuldiv24(_cfs, _cfeedback);	\
	if(++_cindex >= _csize) {	\
		_cindex = 0;	\
	}	\
	_output += _rev_output;	\
}

static void do_freeverb(int32 *buf, int32 count)
{
	int32 _rev_bufout, _rev_output;
	int32 i, k = 0;
	int32 outl, outr, input;
	revmodel_t *rev = revmodel;
	comb *combL = rev->combL, *combR = rev->combR;
	allpass *allpassL = rev->allpassL, *allpassR = rev->allpassR;

	if(rev == NULL) {
		for (k = 0; k < count; k++)
		{
			effect_buffer[k] = 0;
		}
		return;
	}

	for (k = 0; k < count; k++)
	{
		outl = outr = 0;
		input = effect_buffer[k] + effect_buffer[k + 1];
		effect_buffer[k] = effect_buffer[k + 1] = 0;

		for (i = 0; i < numcombs; i++) {
			do_comb(input, outl, combL[i].buf, combL[i].size, combL[i].index,
				combL[i].damp1i, combL[i].damp2i, combL[i].filterstore, combL[i].feedbacki);
			do_comb(input, outr, combR[i].buf, combR[i].size, combR[i].index,
				combR[i].damp1i, combR[i].damp2i, combR[i].filterstore, combR[i].feedbacki);
		}
		for (i = 0; i < numallpasses; i++) {
			do_allpass(outl, allpassL[i].buf, allpassL[i].size, allpassL[i].index, allpassL[i].feedbacki);
			do_allpass(outr, allpassR[i].buf, allpassR[i].size, allpassR[i].index, allpassR[i].feedbacki);
		}
		buf[k] += imuldiv24(outl, rev->wet1i) + imuldiv24(outr, rev->wet2i);
		buf[k + 1] += imuldiv24(outr, rev->wet1i) + imuldiv24(outl, rev->wet2i);
		++k;
	}
}
#else	/* floating-point implementation */
#define do_allpass(_stream, _apbuf, _apsize, _apindex, _apfeedback) \
{ \
	_rev_bufout = _apbuf[_apindex];	\
	_rev_output = -_stream + _rev_bufout;	\
	_apbuf[_apindex] = _stream + (_rev_bufout * _apfeedback);	\
	if(++_apindex >= _apsize) {	\
		_apindex = 0;	\
	}	\
	_stream = _rev_output;	\
}

#define do_comb(_input, _output, _cbuf, _csize, _cindex, _cdamp1, _cdamp2, _cfs, _cfeedback)	\
{	\
	_rev_output = _cbuf[_cindex];	\
	_cfs = (_rev_output * _cdamp2) + (_cfs * _cdamp1);	\
	_cbuf[_cindex] = _input + (_cfs * _cfeedback);	\
	if(++_cindex >= _csize) {	\
		_cindex = 0;	\
	}	\
	_output += _rev_output;	\
}

static void do_freeverb(int32 *buf, int32 count)
{
	int32 _rev_bufout, _rev_output;
	int32 i, k = 0;
	int32 outl, outr, input;
	revmodel_t *rev = revmodel;

	if(rev == NULL) {
		for (k = 0; k < count; k++)
		{
			effect_buffer[k] = 0;
		}
		return;
	}

	for (k = 0; k < count; k++)
	{
		outl = outr = 0;
		input = effect_buffer[k] + effect_buffer[k + 1];
		effect_buffer[k] = effect_buffer[k + 1] = 0;

		for (i = 0; i < numcombs; i++) {
			do_comb(input, outl, rev->combL[i].buf, rev->combL[i].size, rev->combL[i].index,
				rev->combL[i].damp1, rev->combL[i].damp2, rev->combL[i].filterstore, rev->combL[i].feedback);
			do_comb(input, outr, rev->combR[i].buf, rev->combR[i].size, rev->combR[i].index,
				rev->combR[i].damp1, rev->combR[i].damp2, rev->combR[i].filterstore, rev->combR[i].feedback);
		}
		for (i = 0; i < numallpasses; i++) {
			do_allpass(outl, rev->allpassL[i].buf, rev->allpassL[i].size, rev->allpassL[i].index, rev->allpassL[i].feedback);
			do_allpass(outr, rev->allpassR[i].buf, rev->allpassR[i].size, rev->allpassR[i].index, rev->allpassR[i].feedback);
		}
		buf[k] += outl * rev->wet1 + outr * rev->wet2;
		buf[k + 1] += outr * rev->wet1 + outl * rev->wet2;
		++k;
	}
}
#endif	/* OPT_MODE != 0 */

void init_reverb(int32 output_rate)
{
	sample_rate = output_rate;
	/* clear delay-line of LPF */
	reverb_status.lpf.last_freq = 0;
	calc_filter_iir1_lowpass(&(reverb_status.lpf));
	/* Only initialize freeverb if stereo output */
	/* Old non-freeverb must be initialized for mono reverb not to crash */
	if (! (play_mode->encoding & PE_MONO)
			&& (opt_reverb_control == 3 || opt_reverb_control == 4
			|| opt_reverb_control < 0 && ! (opt_reverb_control & 0x100))
			|| opt_effect_quality >= 2) {
		alloc_revmodel();
		update_revmodel(revmodel);
		init_revmodel(revmodel);
		memset(effect_buffer, 0, effect_bufsize);
		memset(direct_buffer, 0, direct_bufsize);
		REV_INP_LEV = fixedgain * revmodel->wet;
	} else {
		ta = tb = 0;
		HPFL = HPFR = LPFL = LPFR = EPFL = EPFR = 0;
		spt0 = spt1 = spt2 = spt3 = 0;
		rev_memset(buf0_L);
		rev_memset(buf0_R);
		rev_memset(buf1_L);
		rev_memset(buf1_R);
		rev_memset(buf2_L);
		rev_memset(buf2_R);
		rev_memset(buf3_L);
		rev_memset(buf3_R);
		memset(effect_buffer, 0, effect_bufsize);
		memset(direct_buffer, 0, direct_bufsize);
		if (output_rate > 65000)
			output_rate = 65000;
		else if (output_rate < 4000)
			output_rate = 4000;
		def_rpt0 = rpt0 = REV_VAL0 * output_rate / 1000;
		def_rpt1 = rpt1 = REV_VAL1 * output_rate / 1000;
		def_rpt2 = rpt2 = REV_VAL2 * output_rate / 1000;
		def_rpt3 = rpt3 = REV_VAL3 * output_rate / 1000;
		REV_INP_LEV = 1.0;
		rpt0 = def_rpt0 * reverb_status.time_ratio;
		rpt1 = def_rpt1 * reverb_status.time_ratio;
		rpt2 = def_rpt2 * reverb_status.time_ratio;
		rpt3 = def_rpt3 * reverb_status.time_ratio;
		while (! isprime(rpt0))
			rpt0++;
		while (! isprime(rpt1))
			rpt1++;
		while (! isprime(rpt2))
			rpt2++;
		while (! isprime(rpt3))
			rpt3++;
	}
}

void free_effect_buffers(void)
{
	free_revmodel();
}

/*                                  */
/*  Insertion and Variation Effect  */
/*                                  */

/*! allocate new effect item and add it into the tail of effect list.
    EffectList *efc: pointer to the top of effect list.
    int8 type: type of new effect item.
    void *info: pointer to infomation of new effect item. */
EffectList *push_effect(EffectList *efc, int8 type, void *info)
{
	EffectList *eft, *efn;
	if(type == EFFECT_NONE) {return NULL;}
	efn = (EffectList *)safe_malloc(sizeof(EffectList));
	efn->type = type;
	efn->next_ef = NULL;
	efn->info = info;
	convert_effect(efn);

	if(efc == NULL) {
		efc = efn;
	} else {
		eft = efc;
		while(eft->next_ef != NULL) {
			eft = eft->next_ef;
		}
		eft->next_ef = efn;
	}
	return efc;
}

/*! process all items of effect list. */
void do_effect_list(int32 *buf, int32 count, EffectList *ef)
{
	EffectList *efc = ef;
	if(ef == NULL) {return;}
	while(efc != NULL && efc->do_effect != NULL)
	{
		(*efc->do_effect)(buf, count, efc);
		efc = efc->next_ef;
	}
}

/*! free all items of effect list. */
void free_effect_list(EffectList *ef)
{
	EffectList *efc, *efn;
	efc = ef;
	if(efc == NULL) {return;}
	do {
		efn = efc->next_ef;
		if(efc->info != NULL) {
			(*efc->do_effect)(NULL, MAGIC_FREE_EFFECT_INFO, efc);
			free(efc->info);
		}
		efc->do_effect = NULL;
		free(efc);
	} while ((efc = efn) != NULL);
}

/*! 2-Band EQ */
void do_eq2(int32 *buf, int32 count, EffectList *ef)
{
	InfoEQ2 *eq = (InfoEQ2 *)ef->info;
	if(count == MAGIC_INIT_EFFECT_INFO) {
		calc_lowshelf_coefs(eq->low_coef, eq->low_freq, eq->low_gain, play_mode->rate);
		calc_highshelf_coefs(eq->high_coef, eq->high_freq, eq->high_gain, play_mode->rate);
		memset(eq->low_val, 0, sizeof(eq->low_val));
		memset(eq->high_val, 0, sizeof(eq->high_val));
		return;
	} else if(count == MAGIC_FREE_EFFECT_INFO) {
		return;
	}
	if(eq->low_gain != 0) {
		do_shelving_filter(buf, count, eq->low_coef, eq->low_val);
	}
	if(eq->high_gain != 0) {
		do_shelving_filter(buf, count, eq->high_coef, eq->high_val);
	}
}

/*! panning (pan = [0, 127]) */
static inline int32 do_left_panning(int32 sample, int32 pan)
{
	return imuldiv8(sample, pan + pan);
}

static inline int32 do_right_panning(int32 sample, int32 pan)
{
	return imuldiv8(sample, 256 - pan - pan);
}

#define INT32_MAX_NEG (1.0 / (double)(1 << 31))
#define OVERDRIVE_DIST 4.0
#define OVERDRIVE_RES 0.1
#define OVERDRIVE_LEVEL 0.8
#define OVERDRIVE_OFFSET 0
#define DISTORTION_DIST 16.0
#define DISTORTION_RES 0.2
#define DISTORTION_LEVEL 0.2
#define DISTORTION_OFFSET 0

/*! GS 0x0110: Overdrive 1 */
void do_overdrive1(int32 *buf, int32 count, EffectList *ef)
{
	InfoOverdrive1 *info = (InfoOverdrive1 *)ef->info;
	filter_moog *svf = &(info->svf);
	int32 t1, t2, f = svf->f, q = svf->q, p = svf->p, b0 = svf->b0,
		b1 = svf->b1, b2 = svf->b2, b3 = svf->b3, b4 = svf->b4;
	filter_lpf18 *lpf = &(info->lpf18);
	double ay1 = lpf->ay1, ay2 = lpf->ay2, aout = lpf->aout, lastin = lpf->lastin,
		kres = lpf->kres, value = lpf->value, kp = lpf->kp, kp1h = lpf->kp1h, ax1, ay11, ay31;
	int32 i, input, high, low, leveli = info->leveli, leveldi = info->leveldi,
		pan = info->pan;
	double sig;

	if(count == MAGIC_INIT_EFFECT_INFO) {
		/* set parameters of decompositor */
		svf->freq = 500;
		svf->res_dB = 0;
		calc_filter_moog(svf);
		/* set parameters of waveshaper */
		/* ideally speaking:
		   lpf->res = table_res[drive];
		   lpf->dist = table_dist[drive]; */
		lpf->freq = 6000;
		lpf->res = OVERDRIVE_RES;
		lpf->dist = OVERDRIVE_DIST * sqrt((double)info->drive / 127.0) + OVERDRIVE_OFFSET;
		calc_filter_lpf18(lpf);
		info->leveli = TIM_FSCALE(info->level * OVERDRIVE_LEVEL, 24);
		info->leveldi = TIM_FSCALE(info->level, 24);
		return;
	} else if(count == MAGIC_FREE_EFFECT_INFO) {
		return;
	}
	for(i = 0; i < count; i++) {
		input = buf[i] + buf[i + 1];
		/* decomposition */
		input -= imuldiv24(q, b4);
		t1 = b1;  b1 = imuldiv24(input + b0, p) - imuldiv24(b1, f);
		t2 = b2;  b2 = imuldiv24(b1 + t1, p) - imuldiv24(b2, f);
		t1 = b3;  b3 = imuldiv24(b2 + t2, p) - imuldiv24(b3, f);
		low = b4 = imuldiv24(b3 + t1, p) - imuldiv24(b4, f);
		b0 = input;
		high = input - b4;
		/* waveshaping */
		sig = (double)high * INT32_MAX_NEG;
		ax1 = lastin;
		ay11 = ay1;
		ay31 = ay2;
		lastin = sig - tanh(kres * aout);
		ay1 = kp1h * (lastin + ax1) - kp * ay1;
		ay2 = kp1h * (ay1 + ay11) - kp * ay2;
		aout = kp1h * (ay2 + ay31) - kp * aout;
		sig = tanh(aout * value);
		high = TIM_FSCALE(sig, 31);
		/* mixing */
		input = imuldiv24(high, leveli) + imuldiv24(low, leveldi);
		buf[i] = do_left_panning(input, pan);
		buf[i + 1] = do_right_panning(input, pan);
		++i;
	}
	svf->b0 = b0, svf->b1 = b1, svf->b2 = b2, svf->b3 = b3, svf->b4 = b4;
    lpf->ay1 = ay1, lpf->ay2 = ay2, lpf->aout = aout, lpf->lastin = lastin;
}

/*! GS 0x0111: Distortion 1 */
void do_distortion1(int32 *buf, int32 count, EffectList *ef)
{
	InfoOverdrive1 *info = (InfoOverdrive1 *)ef->info;
	filter_moog *svf = &(info->svf);
	int32 t1, t2, f = svf->f, q = svf->q, p = svf->p, b0 = svf->b0,
		b1 = svf->b1, b2 = svf->b2, b3 = svf->b3, b4 = svf->b4;
	filter_lpf18 *lpf = &(info->lpf18);
	double ay1 = lpf->ay1, ay2 = lpf->ay2, aout = lpf->aout, lastin = lpf->lastin,
		kres = lpf->kres, value = lpf->value, kp = lpf->kp, kp1h = lpf->kp1h, ax1, ay11, ay31;
	int32 i, input, high, low, leveli = info->leveli, leveldi = info->leveldi,
		pan = info->pan;
	double sig;

	if(count == MAGIC_INIT_EFFECT_INFO) {
		/* set parameters of decompositor */
		svf->freq = 500;
		svf->res_dB = 0;
		calc_filter_moog(svf);
		/* set parameters of waveshaper */
		/* ideally speaking:
		   lpf->res = table_res[drive];
		   lpf->dist = table_dist[drive]; */
		lpf->freq = 6000;
		lpf->res = DISTORTION_RES;
		lpf->dist = DISTORTION_DIST * sqrt((double)info->drive / 127.0) + DISTORTION_OFFSET;
		calc_filter_lpf18(lpf);
		info->leveli = TIM_FSCALE(info->level * DISTORTION_LEVEL, 24);
		info->leveldi = TIM_FSCALE(info->level, 24);
		return;
	} else if(count == MAGIC_FREE_EFFECT_INFO) {
		return;
	}
	for(i = 0; i < count; i++) {
		input = buf[i] + buf[i + 1];
		/* decomposition */
		input -= imuldiv24(q, b4);
		t1 = b1;  b1 = imuldiv24(input + b0, p) - imuldiv24(b1, f);
		t2 = b2;  b2 = imuldiv24(b1 + t1, p) - imuldiv24(b2, f);
		t1 = b3;  b3 = imuldiv24(b2 + t2, p) - imuldiv24(b3, f);
		low = b4 = imuldiv24(b3 + t1, p) - imuldiv24(b4, f);
		b0 = input;
		high = input - b4;
		/* waveshaping */
		sig = (double)high * INT32_MAX_NEG;
		ax1 = lastin;
		ay11 = ay1;
		ay31 = ay2;
		lastin = sig - tanh(kres * aout);
		ay1 = kp1h * (lastin + ax1) - kp * ay1;
		ay2 = kp1h * (ay1 + ay11) - kp * ay2;
		aout = kp1h * (ay2 + ay31) - kp * aout;
		sig = tanh(aout * value);
		high = TIM_FSCALE(sig, 31);
		/* mixing */
		input = imuldiv24(high, leveli) + imuldiv24(low, leveldi);
		buf[i] = do_left_panning(input, pan);
		buf[i + 1] = do_right_panning(input, pan);
		++i;
	}
	svf->b0 = b0, svf->b1 = b1, svf->b2 = b2, svf->b3 = b3, svf->b4 = b4;
    lpf->ay1 = ay1, lpf->ay2 = ay2, lpf->aout = aout, lpf->lastin = lastin;
}

/*! GS 0x1103: OD1 / OD2 */
void do_dual_od(int32 *buf, int32 count, EffectList *ef)
{
	InfoOD1OD2 *info = (InfoOD1OD2 *)ef->info;
	filter_moog *svfl = &(info->svfl);
	int32 t1l, t2l, f = svfl->f, q = svfl->q, p = svfl->p, b0l = svfl->b0,
		b1l = svfl->b1, b2l = svfl->b2, b3l = svfl->b3, b4l = svfl->b4;
	filter_moog *svfr = &(info->svfr);
	int32 t1r, t2r, b0r = svfr->b0,
		b1r = svfr->b1, b2r = svfr->b2, b3r = svfr->b3, b4r = svfr->b4;
	filter_lpf18 *lpfl = &(info->lpf18l);
	double ay1l = lpfl->ay1, ay2l = lpfl->ay2, aoutl = lpfl->aout, lastinl = lpfl->lastin,
		kresl = lpfl->kres, valuel = lpfl->value, kp = lpfl->kp, kp1h = lpfl->kp1h, ax1l, ay11l, ay31l;
	filter_lpf18 *lpfr = &(info->lpf18r);
	double ay1r = lpfr->ay1, ay2r = lpfr->ay2, aoutr = lpfr->aout, lastinr = lpfr->lastin,
		kresr = lpfr->kres, valuer = lpfr->value, ax1r, ay11r, ay31r;
	int32 i, inputl, inputr, high, low, levelli = info->levelli, levelri = info->levelri,
		leveldli = info->leveldli, leveldri = info->leveldri, panl = info->panl, panr = info->panr;
	double sig;

	if(count == MAGIC_INIT_EFFECT_INFO) {
		/* left */
		/* set parameters of decompositor */
		svfl->freq = 500;
		svfl->res_dB = 0;
		calc_filter_moog(svfl);
		/* set parameters of waveshaper */
		lpfl->freq = 6000;
		if(info->typel == 0) {	/* Overdrive */
			lpfl->res = OVERDRIVE_RES;
			lpfl->dist = OVERDRIVE_DIST * sqrt((double)info->drivel / 127.0) + OVERDRIVE_OFFSET;
			info->levelli = TIM_FSCALE(info->levell * info->level * OVERDRIVE_LEVEL, 24);
		} else {	/* Distortion */
			lpfl->res = DISTORTION_RES;
			lpfl->dist = DISTORTION_DIST * sqrt((double)info->drivel / 127.0) + DISTORTION_OFFSET;
			info->levelli = TIM_FSCALE(info->levell * info->level * DISTORTION_LEVEL, 24);
		}
		info->leveldli = TIM_FSCALE(info->levell * info->level, 24);
		calc_filter_lpf18(lpfl);
		/* right */
		/* set parameters of decompositor */
		svfr->freq = 500;
		svfr->res_dB = 0;
		calc_filter_moog(svfr);
		/* set parameters of waveshaper */
		lpfr->freq = 6000;
		if(info->typer == 0) {	/* Overdrive */
			lpfr->res = OVERDRIVE_RES;
			lpfr->dist = OVERDRIVE_DIST * sqrt((double)info->driver / 127.0) + OVERDRIVE_OFFSET;
			info->levelri = TIM_FSCALE(info->levelr * info->level * OVERDRIVE_LEVEL, 24);
		} else {	/* Distortion */
			lpfr->res = DISTORTION_RES;
			lpfr->dist = DISTORTION_DIST * sqrt((double)info->driver / 127.0) + DISTORTION_OFFSET;
			info->levelri = TIM_FSCALE(info->levelr * info->level * DISTORTION_LEVEL, 24);
		}
		info->leveldri = TIM_FSCALE(info->levelr * info->level, 24);
		calc_filter_lpf18(lpfr);
		return;
	} else if(count == MAGIC_FREE_EFFECT_INFO) {
		return;
	}
	for(i = 0; i < count; i++) {
		/* left */
		inputl = buf[i];
		/* decomposition */
		inputl -= imuldiv24(q, b4l);
		t1l = b1l;  b1l = imuldiv24(inputl + b0l, p) - imuldiv24(b1l, f);
		t2l = b2l;  b2l = imuldiv24(b1l + t1l, p) - imuldiv24(b2l, f);
		t1l = b3l;  b3l = imuldiv24(b2l + t2l, p) - imuldiv24(b3l, f);
		low = b4l = imuldiv24(b3l + t1l, p) - imuldiv24(b4l, f);
		b0l = inputl;
		high = inputl - b4l;
		/* waveshaping */
		sig = (double)high * INT32_MAX_NEG;
		ax1l = lastinl;
		ay11l = ay1l;
		ay31l = ay2l;
		lastinl = sig - tanh(kresl * aoutl);
		ay1l = kp1h * (lastinl + ax1l) - kp * ay1l;
		ay2l = kp1h * (ay1l + ay11l) - kp * ay2l;
		aoutl = kp1h * (ay2l + ay31l) - kp * aoutl;
		sig = tanh(aoutl * valuel);
		high = TIM_FSCALE(sig, 31);
		inputl = imuldiv24(high, levelli) + imuldiv24(low, leveldli);

		/* right */
		inputr = buf[++i];
		/* decomposition */
		inputr -= imuldiv24(q, b4r);
		t1r = b1r;  b1r = imuldiv24(inputr + b0r, p) - imuldiv24(b1r, f);
		t2r = b2r;  b2r = imuldiv24(b1r + t1r, p) - imuldiv24(b2r, f);
		t1r = b3r;  b3r = imuldiv24(b2r + t2r, p) - imuldiv24(b3r, f);
		low = b4r = imuldiv24(b3r + t1r, p) - imuldiv24(b4r, f);
		b0r = inputr;
		high = inputr - b4r;
		/* waveshaping */
		sig = (double)high * INT32_MAX_NEG;
		ax1r = lastinr;
		ay11r = ay1r;
		ay31r = ay2r;
		lastinr = sig - tanh(kresr * aoutr);
		ay1r = kp1h * (lastinr + ax1r) - kp * ay1r;
		ay2r = kp1h * (ay1r + ay11r) - kp * ay2r;
		aoutr = kp1h * (ay2r + ay31r) - kp * aoutr;
		sig = tanh(aoutr * valuer);
		high = TIM_FSCALE(sig, 31);
		inputr = imuldiv24(high, levelri) + imuldiv24(low, leveldri);

		/* mixing */
		buf[i - 1] = do_left_panning(inputl, panl) + do_left_panning(inputr, panr);
		buf[i] = do_right_panning(inputl, panl) + do_right_panning(inputr, panr);
	}
	svfl->b0 = b0l, svfl->b1 = b1l, svfl->b2 = b2l, svfl->b3 = b3l, svfl->b4 = b4l;
    lpfl->ay1 = ay1l, lpfl->ay2 = ay2l, lpfl->aout = aoutl, lpfl->lastin = lastinl;
	svfr->b0 = b0r, svfr->b1 = b1r, svfr->b2 = b2r, svfr->b3 = b3r, svfr->b4 = b4r;
    lpfr->ay1 = ay1r, lpfr->ay2 = ay2r, lpfr->aout = aoutr, lpfr->lastin = lastinr;
}

#define HEXA_CHORUS_DEPTH_DEV 1
#define HEXA_CHORUS_DELAY_DEV 1

/*! GS 0x0140: HEXA-CHORUS */
void do_hexa_chorus(int32 *buf, int32 count, EffectList *ef)
{
	InfoHexaChorus *info = (InfoHexaChorus *)ef->info;
	lfo *lfo = &(info->lfo0);
	delay *buf0 = &(info->buf0);
	int32 *ebuf = buf0->buf, size = buf0->size, index = buf0->index;
	int32 spt0 = info->spt0, spt1 = info->spt1, spt2 = info->spt2,
		spt3 = info->spt3, spt4 = info->spt4, spt5 = info->spt5,
		hist0 = info->hist0, hist1 = info->hist1, hist2 = info->hist2,
		hist3 = info->hist3, hist4 = info->hist4, hist5 = info->hist5;
	int32 dryi = info->dryi, weti = info->weti;
	int32 pan0 = info->pan0, pan1 = info->pan1, pan2 = info->pan2,
		pan3 = info->pan3, pan4 = info->pan4, pan5 = info->pan5;
	int32 depth0 = info->depth0, depth1 = info->depth1, depth2 = info->depth2,
		depth3 = info->depth3, depth4 = info->depth4, depth5 = info->depth5,
		pdelay0 = info->pdelay0, pdelay1 = info->pdelay1, pdelay2 = info->pdelay2,
		pdelay3 = info->pdelay3, pdelay4 = info->pdelay4, pdelay5 = info->pdelay5;
	int32 i, inputw, inputd, lfo_val, wetl, wetr,
		v0, v1, v2, v3, v4, v5, f0, f1, f2, f3, f4, f5;

	if(count == MAGIC_INIT_EFFECT_INFO) {
		set_delay(buf0, 9600);
		init_lfo(lfo, lfo->cycle, LFO_SINE);
		info->dryi = TIM_FSCALE(info->level * info->dry, 24);
		info->weti = TIM_FSCALE(info->level * info->wet, 24);
		info->depth0 = info->depth + info->depth_dev * 3 * HEXA_CHORUS_DEPTH_DEV;
		info->depth1 = info->depth + info->depth_dev * 2 * HEXA_CHORUS_DEPTH_DEV;
		info->depth2 = info->depth + info->depth_dev * HEXA_CHORUS_DEPTH_DEV;
		info->depth3 = info->depth + info->depth_dev * HEXA_CHORUS_DEPTH_DEV;
		info->depth4 = info->depth + info->depth_dev * 2 * HEXA_CHORUS_DEPTH_DEV;
		info->depth5 = info->depth + info->depth_dev * 3 * HEXA_CHORUS_DEPTH_DEV;
		info->pdelay0 = info->pdelay + info->pdelay_dev * 3 * HEXA_CHORUS_DELAY_DEV;
		info->pdelay1 = info->pdelay + info->pdelay_dev * 2 * HEXA_CHORUS_DELAY_DEV;
		info->pdelay2 = info->pdelay + info->pdelay_dev * HEXA_CHORUS_DELAY_DEV;
		info->pdelay3 = info->pdelay + info->pdelay_dev * HEXA_CHORUS_DELAY_DEV;
		info->pdelay4 = info->pdelay + info->pdelay_dev * 2 * HEXA_CHORUS_DELAY_DEV;
		info->pdelay5 = info->pdelay + info->pdelay_dev * 3 * HEXA_CHORUS_DELAY_DEV;
		/* in this part, validation check may be necessary. */
		info->pan0 = 64 - info->pan_dev * 3;
		info->pan1 = 64 - info->pan_dev * 2;
		info->pan2 = 64 - info->pan_dev;
		info->pan3 = 64 + info->pan_dev;
		info->pan4 = 64 + info->pan_dev * 2;
		info->pan5 = 64 + info->pan_dev * 3;
		info->hist0 = info->hist1 = info->hist2
			= info->hist3 = info->hist4 = info->hist5 = 0;
		info->spt0 = info->spt1 = info->spt2
			= info->spt3 = info->spt4 = info->spt5 = 0;
		return;
	} else if(count == MAGIC_FREE_EFFECT_INFO) {
		free_delay(buf0);
		return;
	}

	/* LFO */
	lfo_val = lfo->buf[imuldiv24(lfo->count, lfo->icycle)];
	f0 = imuldiv24(lfo_val, depth0);
	spt0 = index - pdelay0 - (f0 >> 8);	/* integral part of delay */
	if(spt0 < 0) {spt0 += size;}
	f1 = imuldiv24(lfo_val, depth1);
	spt1 = index - pdelay1 - (f1 >> 8);	/* integral part of delay */
	if(spt1 < 0) {spt1 += size;}
	f2 = imuldiv24(lfo_val, depth2);
	spt2 = index - pdelay2 - (f2 >> 8);	/* integral part of delay */
	if(spt2 < 0) {spt2 += size;}
	f3 = imuldiv24(lfo_val, depth3);
	spt3 = index - pdelay3 - (f3 >> 8);	/* integral part of delay */
	if(spt3 < 0) {spt3 += size;}
	f4 = imuldiv24(lfo_val, depth4);
	spt4 = index - pdelay4 - (f4 >> 8);	/* integral part of delay */
	if(spt4 < 0) {spt4 += size;}
	f5 = imuldiv24(lfo_val, depth5);
	spt5 = index - pdelay5 - (f5 >> 8);	/* integral part of delay */
	if(spt5 < 0) {spt5 += size;}

	for(i = 0; i < count; i++) {
		v0 = ebuf[spt0], v1 = ebuf[spt1], v2 = ebuf[spt2],
		v3 = ebuf[spt3], v4 = ebuf[spt4], v5 = ebuf[spt5];

		/* LFO */
		if(++index == size) {index = 0;}
		lfo_val = do_lfo(lfo);
		f0 = imuldiv24(lfo_val, depth0);
		spt0 = index - pdelay0 - (f0 >> 8);	/* integral part of delay */
		f0 = 0xFF - (f0 & 0xFF);	/* (1 - frac) * 256 */
		if(spt0 < 0) {spt0 += size;}
		f1 = imuldiv24(lfo_val, depth1);
		spt1 = index - pdelay1 - (f1 >> 8);	/* integral part of delay */
		f1 = 0xFF - (f1 & 0xFF);	/* (1 - frac) * 256 */
		if(spt1 < 0) {spt1 += size;}
		f2 = imuldiv24(lfo_val, depth2);
		spt2 = index - pdelay2 - (f2 >> 8);	/* integral part of delay */
		f2 = 0xFF - (f2 & 0xFF);	/* (1 - frac) * 256 */
		if(spt2 < 0) {spt2 += size;}
		f3 = imuldiv24(lfo_val, depth3);
		spt3 = index - pdelay3 - (f3 >> 8);	/* integral part of delay */
		f3 = 0xFF - (f3 & 0xFF);	/* (1 - frac) * 256 */
		if(spt3 < 0) {spt3 += size;}
		f4 = imuldiv24(lfo_val, depth4);
		spt4 = index - pdelay4 - (f4 >> 8);	/* integral part of delay */
		f4 = 0xFF - (f4 & 0xFF);	/* (1 - frac) * 256 */
		if(spt4 < 0) {spt4 += size;}
		f5 = imuldiv24(lfo_val, depth5);
		spt5 = index - pdelay5 - (f5 >> 8);	/* integral part of delay */
		f5 = 0xFF - (f5 & 0xFF);	/* (1 - frac) * 256 */
		if(spt5 < 0) {spt5 += size;}

		/* chorus effect */
		/* all-pass interpolation */
		hist0 = hist0 + imuldiv8(ebuf[spt0] - v0, f0);
		hist1 = hist1 + imuldiv8(ebuf[spt1] - v1, f1);
		hist2 = hist2 + imuldiv8(ebuf[spt2] - v2, f2);
		hist3 = hist3 + imuldiv8(ebuf[spt3] - v3, f3);
		hist4 = hist4 + imuldiv8(ebuf[spt4] - v4, f4);
		hist5 = hist5 + imuldiv8(ebuf[spt5] - v5, f5);
		ebuf[index] = imuldiv24(buf[i] + buf[i + 1], weti);

		/* mixing */
		buf[i] = do_left_panning(hist0, pan0) + do_left_panning(hist1, pan1)
			+ do_left_panning(hist2, pan2) + do_left_panning(hist3, pan3)
			+ do_left_panning(hist4, pan4) + do_left_panning(hist5, pan5)
			+ imuldiv24(buf[i], dryi);
		buf[i + 1] = do_right_panning(hist0, pan0) + do_right_panning(hist1, pan1)
			+ do_right_panning(hist2, pan2) + do_right_panning(hist3, pan3)
			+ do_right_panning(hist4, pan4) + do_right_panning(hist5, pan5)
			+ imuldiv24(buf[i + 1], dryi);

		++i;
	}
	buf0->size = size, buf0->index = index;
	info->spt0 = spt0, info->spt1 = spt1, info->spt2 = spt2,
	info->spt3 = spt3, info->spt4 = spt4, info->spt5 = spt5,
	info->hist0 = hist0, info->hist1 = hist1, info->hist2 = hist2,
	info->hist3 = hist3, info->hist4 = hist4, info->hist5 = hist5;
}

/*! assign effect engine according to effect type. */
void convert_effect(EffectList *ef)
{
	ef->do_effect = NULL;
	switch(ef->type)
	{
	case EFFECT_NONE:
		break;
	case EFFECT_EQ2:
		ef->do_effect = do_eq2;
		break;
	case EFFECT_OVERDRIVE1:
		ef->do_effect = do_overdrive1;
		break;
	case EFFECT_DISTORTION1:
		ef->do_effect = do_distortion1;
		break;
	case EFFECT_HEXA_CHORUS:
		ef->do_effect = do_hexa_chorus;
		break;
	case EFFECT_OD1OD2:
		ef->do_effect = do_dual_od;
		break;
	default:
		break;
	}

	(*ef->do_effect)(NULL, MAGIC_INIT_EFFECT_INFO, ef);
}

struct delay_status_t delay_status;
struct reverb_status_t reverb_status;
struct chorus_status_t chorus_status;
struct chorus_param_t chorus_param;
struct eq_status_t eq_status;
