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
#include "mt19937ar.h"
#include <math.h>
#include <stdlib.h>

#define SYS_EFFECT_PRE_LPF

/* #define SYS_EFFECT_CLIP */
#ifdef SYS_EFFECT_CLIP
#define CLIP_AMP_MAX (1L << (32 - GUARD_BITS))
#define CLIP_AMP_MIN (-1L << (32 - GUARD_BITS))
#endif /* SYS_EFFECT_CLIP */

static double REV_INP_LEV = 1.0;
#define MASTER_CHORUS_LEVEL 1.7
#define MASTER_DELAY_LEVEL 3.25

/*              */
/*  Dry Signal  */
/*              */
static int32 direct_buffer[AUDIO_BUFFER_SIZE * 2];
static int32 direct_bufsize = sizeof(direct_buffer);

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
    v_set_dry_signal(direct_buffer, buf, n);
  } else {
#endif
    register int32 i;
	register int32 *dbuf = direct_buffer;

    for(i = n - 1; i >= 0; i--)
    {
        dbuf[i] += buf[i];
    }
#if USE_ALTIVEC
  }
#endif
}
#endif

/* XG has "dry level". */
#if OPT_MODE != 0	/* fixed-point implementation */
#if _MSC_VER
void set_dry_signal_xg(int32 *buf, int32 count, int32 level)
{
	int32 *dbuf = direct_buffer;
	if(!level) {return;}
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
void set_dry_signal_xg(register int32 *sbuffer, int32 n, int32 level)
{
    register int32 i;
	int32 *buf = direct_buffer;
	if(!level) {return;}
	level = level * 65536 / 127;

	for(i = n - 1; i >= 0; i--) {buf[i] += imuldiv16(sbuffer[i], level);}
}
#endif	/* _MSC_VER */
#else	/* floating-point implementation */
void set_dry_signal_xg(register int32 *sbuffer, int32 n, int32 level)
{
    register int32 i;
    register int32 count = n;
	if(!level) {return;}
    FLOAT_T send_level = (FLOAT_T)level / 127.0;

    for(i = 0; i < count; i++)
    {
		direct_buffer[i] += sbuffer[i] * send_level;
    }
}
#endif /* OPT_MODE != 0 */

#ifdef SYS_EFFECT_CLIP
void mix_dry_signal(int32 *buf, int32 n)
{
	int32 i, x;
	for (i = 0; i < n; i++) {
		x = direct_buffer[i];
		buf[i] = (x > CLIP_AMP_MAX) ? CLIP_AMP_MAX
				: (x < CLIP_AMP_MIN) ? CLIP_AMP_MIN : x;
	}
	memset(direct_buffer, 0, sizeof(int32) * n);
}
#else /* SYS_EFFECT_CLIP */
void mix_dry_signal(int32 *buf, int32 n)
{
 	memcpy(buf, direct_buffer, sizeof(int32) * n);
	memset(direct_buffer, 0, sizeof(int32) * n);
}
#endif /* SYS_EFFECT_CLIP */

/*                    */
/*  Effect Utilities  */
/*                    */
/* general-purpose temporary variables for macros */
static int32 _output, _bufout, _temp1, _temp2, _temp3;

static inline int isprime(int val)
{
	int i;
	if (val == 2) {return 1;}
	if (val & 1) {
		for (i = 3; i < (int)sqrt((double)val) + 1; i += 2) {
			if ((val % i) == 0) {return 0;}
		}
		return 1; /* prime */
	} else {return 0;} /* even */
}

/*! delay */
static void free_delay(delay *delay)
{
	if(delay->buf != NULL) {
		free(delay->buf);
		delay->buf = NULL;
	}
}

static void set_delay(delay *delay, int32 size)
{
	if(size < 1) {size = 1;} 
	free_delay(delay);
	delay->buf = (int32 *)safe_malloc(sizeof(int32) * size);
	if(delay->buf == NULL) {return;}
	delay->index = 0;
	delay->size = size;
	memset(delay->buf, 0, sizeof(int32) * delay->size);
}

#define do_delay(_stream, _buf, _size, _index) \
{ \
	_output = _buf[_index];	\
	_buf[_index] = _stream;	\
	if(++_index >= _size) {	\
		_index = 0;	\
	}	\
	_stream = _output;	\
}

/*! LFO (low frequency oscillator) */
static void init_lfo(lfo *lfo, double freq, int type, double phase)
{
	int32 i, cycle, diff;

	lfo->count = 0;
	lfo->freq = freq;
	if (lfo->freq < 0.05f) {lfo->freq = 0.05f;}
	cycle = (double)play_mode->rate / lfo->freq;
	if (cycle < 1) {cycle = 1;}
	lfo->cycle = cycle;
	lfo->icycle = TIM_FSCALE((SINE_CYCLE_LENGTH - 1) / (double)cycle, 24) - 0.5;
	diff = SINE_CYCLE_LENGTH * phase / 360.0f;

	if(lfo->type != type) {	/* generate LFO waveform */
		switch(type) {
		case LFO_SINE:
			for(i = 0; i < SINE_CYCLE_LENGTH; i++)
				lfo->buf[i] = TIM_FSCALE((lookup_sine(i + diff) + 1.0) / 2.0, 16);
			break;
		case LFO_TRIANGULAR:
			for(i = 0; i < SINE_CYCLE_LENGTH; i++)
				lfo->buf[i] = TIM_FSCALE((lookup_triangular(i + diff) + 1.0) / 2.0, 16);
			break;
		default:
			for(i = 0; i < SINE_CYCLE_LENGTH; i++) {lfo->buf[i] = TIM_FSCALE(0.5, 16);}
			break;
		}
	}
	lfo->type = type;
}

/* returned value is between 0 and (1 << 16) */
inline int32 do_lfo(lfo *lfo)
{
	int32 val;
	val = lfo->buf[imuldiv24(lfo->count, lfo->icycle)];
	if(++lfo->count == lfo->cycle) {lfo->count = 0;}
	return val;
}

#if 0
/*! modulated delay with allpass interpolation (for Chorus Effect,...) */
static void free_mod_delay(mod_delay *delay)
{
	if(delay->buf != NULL) {
		free(delay->buf);
		delay->buf = NULL;
	}
}

static void set_mod_delay(mod_delay *delay, int32 ndelay, int32 depth)
{
	int32 size = ndelay + depth + 1;
	free_mod_delay(delay);
	delay->buf = (int32 *)safe_malloc(sizeof(int32) * size);
	if(delay->buf == NULL) {return;}
	delay->rindex = 0;
	delay->windex = 0;
	delay->hist = 0;
	delay->ndelay = ndelay;
	delay->depth = depth;
	delay->size = size;
	memset(delay->buf, 0, sizeof(int32) * delay->size);
}
#endif

#define do_mod_delay(_stream, _buf, _size, _rindex, _windex, _ndelay, _depth, _lfoval, _hist) \
{ \
	if(++_windex == _size) {_windex = 0;}	\
	_temp1 = _buf[_rindex];	\
	_temp2 = imuldiv24(_lfoval, _depth);	\
	_rindex = _windex - _ndelay - (_temp2 >> 8);	\
	if(_rindex < 0) {_rindex += _size;}	\
	_temp2 = 0xFF - (_temp2 & 0xFF);	\
	_hist = _temp1 + imuldiv8(_buf[_rindex] - _hist, _temp2);	\
	_buf[_windex] = _stream;	\
	_stream = _hist;	\
}

/*! modulated allpass filter with allpass interpolation (for Plate Reverberator,...) */
static void free_mod_allpass(mod_allpass *delay)
{
	if(delay->buf != NULL) {
		free(delay->buf);
		delay->buf = NULL;
	}
}

static void set_mod_allpass(mod_allpass *delay, int32 ndelay, int32 depth, double feedback)
{
	int32 size = ndelay + depth + 1;
	free_mod_allpass(delay);
	delay->buf = (int32 *)safe_malloc(sizeof(int32) * size);
	if(delay->buf == NULL) {return;}
	delay->rindex = 0;
	delay->windex = 0;
	delay->hist = 0;
	delay->ndelay = ndelay;
	delay->depth = depth;
	delay->size = size;
	delay->feedback = feedback;
	delay->feedbacki = TIM_FSCALE(feedback, 24);
	memset(delay->buf, 0, sizeof(int32) * delay->size);
}

#define do_mod_allpass(_stream, _buf, _size, _rindex, _windex, _ndelay, _depth, _lfoval, _hist, _feedback) \
{ \
	if(++_windex == _size) {_windex = 0;}	\
	_temp3 = _stream + imuldiv24(_hist, _feedback);	\
	_temp1 = _buf[_rindex];	\
	_temp2 = imuldiv24(_lfoval, _depth);	\
	_rindex = _windex - _ndelay - (_temp2 >> 8);	\
	if(_rindex < 0) {_rindex += _size;}	\
	_temp2 = 0xFF - (_temp2 & 0xFF);	\
	_hist = _temp1 + imuldiv8(_buf[_rindex] - _hist, _temp2);	\
	_buf[_windex] = _temp3;	\
	_stream = _hist - imuldiv24(_temp3, _feedback);	\
}

/* allpass filter */
static void free_allpass(allpass *allpass)
{
	if(allpass->buf != NULL) {
		free(allpass->buf);
		allpass->buf = NULL;
	}
}

static void set_allpass(allpass *allpass, int32 size, double feedback)
{
	if(allpass->buf != NULL) {
		free(allpass->buf);
		allpass->buf = NULL;
	}
	allpass->buf = (int32 *)safe_malloc(sizeof(int32) * size);
	if(allpass->buf == NULL) {return;}
	allpass->index = 0;
	allpass->size = size;
	allpass->feedback = feedback;
	allpass->feedbacki = TIM_FSCALE(feedback, 24);
	memset(allpass->buf, 0, sizeof(int32) * allpass->size);
}

#define do_allpass(_stream, _apbuf, _apsize, _apindex, _apfeedback) \
{ \
	_bufout = _apbuf[_apindex];	\
	_output = _stream - imuldiv24(_bufout, _apfeedback);	\
	_apbuf[_apindex] = _output;	\
	if(++_apindex >= _apsize) {	\
		_apindex = 0;	\
	}	\
	_stream = _bufout + imuldiv24(_output, _apfeedback);	\
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

/*! 1st order lowpass filter */
void init_filter_lowpass1(filter_lowpass1 *p)
{
	if (p->a > 1.0f) {p->a = 1.0f;}
	p->x1l = p->x1r = 0;
	p->ai = TIM_FSCALE(p->a, 24);
	p->iai = TIM_FSCALE(1.0 - p->a, 24);
}

#define do_filter_lowpass1(_stream, _x1, _a, _ia) \
{ \
	_stream = _x1 = imuldiv24(_x1, _ia) + imuldiv24(_stream, _a);	\
}

void do_filter_lowpass1_stereo(int32 *buf, int32 count, filter_lowpass1 *p)
{
	int32 i, a = p->ai, ia = p->iai, x1l = p->x1l, x1r = p->x1r;

	for(i = 0; i < count; i++) {
		/* left */
		do_filter_lowpass1(buf[i], x1l, a, ia);

		++i;

		/* right */
		do_filter_lowpass1(buf[i], x1r, a, ia);
	}
	x1l = p->x1l, x1r = p->x1r;
}

/*! shelving filter */
void calc_filter_shelving_low(filter_shelving *p)
{
	double a0, a1, a2, b0, b1, b2, omega, sn, cs, A, beta;

	A = pow(10, p->gain / 40);
	omega = 2.0 * M_PI * (double)p->freq / (double)play_mode->rate;
	sn = sin(omega);
	cs = cos(omega);
	if(p->q != 0) {beta = sqrt(A) / p->q;}
	else {beta = sqrt(A + A);}

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

	p->a1 = TIM_FSCALE(a1, 24);
	p->a2 = TIM_FSCALE(a2, 24);
	p->b0 = TIM_FSCALE(b0, 24);
	p->b1 = TIM_FSCALE(b1, 24);
	p->b2 = TIM_FSCALE(b2, 24);
}

void calc_filter_shelving_high(filter_shelving *p)
{
	double a0, a1, a2, b0, b1, b2, omega, sn, cs, A, beta;

	A = pow(10, p->gain / 40);
	omega = 2.0 * M_PI * (double)p->freq / (double)play_mode->rate;
	sn = sin(omega);
	cs = cos(omega);
	if(p->q != 0) {beta = sqrt(A) / p->q;}
	else {beta = sqrt(A + A);}

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

	p->a1 = TIM_FSCALE(a1, 24);
	p->a2 = TIM_FSCALE(a2, 24);
	p->b0 = TIM_FSCALE(b0, 24);
	p->b1 = TIM_FSCALE(b1, 24);
	p->b2 = TIM_FSCALE(b2, 24);
}

void init_filter_shelving(filter_shelving *p)
{
	p->x1l = 0, p->x2l = 0, p->y1l = 0, p->y2l = 0, p->x1r = 0,
		p->x2r = 0, p->y1r = 0, p->y2r = 0;
}

static void do_shelving_filter_stereo(int32* buf, int32 count, filter_shelving *p)
{
#if OPT_MODE != 0
	int32 i;
	int32 x1l = p->x1l, x2l = p->x2l, y1l = p->y1l, y2l = p->y2l,
		x1r = p->x1r, x2r = p->x2r, y1r = p->y1r, y2r = p->y2r, yout;
	int32 a1 = p->a1, a2 = p->a2, b0 = p->b0, b1 = p->b1, b2 = p->b2;

	for(i = 0; i < count; i++) {
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
	p->x1l = x1l, p->x2l = x2l, p->y1l = y1l, p->y2l = y2l,
		p->x1r = x1r, p->x2r = x2r, p->y1r = y1r, p->y2r = y2r;
#endif /* OPT_MODE != 0 */
}

/*! peaking filter */
void calc_filter_peaking(filter_peaking *p)
{
	double a0, ba1, a2, b0, b2, omega, sn, cs, A, alpha;

	A = pow(10, p->gain / 40);
	omega = 2.0 * M_PI * (double)p->freq / (double)play_mode->rate;
	sn = sin(omega);
	cs = cos(omega);
	if(p->q != 0) {alpha = sn / (2.0 * p->q);}
	else {
		p->b0 = TIM_FSCALE(1.0, 24);
		p->ba1 = p->b0 = p->a2 = p->b2 = 0;
		return;
	}

	a0 = 1.0 / (1.0 + alpha / A);
	ba1 = -2.0 * cs;
	a2 = 1.0 - alpha / A;
	b0 = 1.0 + alpha * A;
	b2 = 1.0 - alpha * A;

	ba1 *= a0;
	a2 *= a0;
	b0 *= a0;
	b2 *= a0;

	p->ba1 = TIM_FSCALE(ba1, 24);
	p->a2 = TIM_FSCALE(a2, 24);
	p->b0 = TIM_FSCALE(b0, 24);
	p->b2 = TIM_FSCALE(b2, 24);
}

void init_filter_peaking(filter_peaking *p)
{
	p->x1l = 0, p->x2l = 0, p->y1l = 0, p->y2l = 0, p->x1r = 0,
		p->x2r = 0, p->y1r = 0, p->y2r = 0;
}

static void do_peaking_filter_stereo(int32* buf, int32 count, filter_peaking *p)
{
#if OPT_MODE != 0
	int32 i;
	int32 x1l = p->x1l, x2l = p->x2l, y1l = p->y1l, y2l = p->y2l,
		x1r = p->x1r, x2r = p->x2r, y1r = p->y1r, y2r = p->y2r, yout;
	int32 ba1 = p->ba1, a2 = p->a2, b0 = p->b0, b2 = p->b2;

	for(i = 0; i < count; i++) {
		yout = imuldiv24(buf[i], b0) + imuldiv24(x1l - y1l, ba1) + imuldiv24(x2l, b2) - imuldiv24(y2l, a2);
		x2l = x1l;
		x1l = buf[i];
		y2l = y1l;
		y1l = yout;
		buf[i] = yout;

		yout = imuldiv24(buf[++i], b0) + imuldiv24(x1r - y1r, ba1) + imuldiv24(x2r, b2) - imuldiv24(y2r, a2);
		x2r = x1r;
		x1r = buf[i];
		y2r = y1r;
		y1r = yout;
		buf[i] = yout;
	}
	p->x1l = x1l, p->x2l = x2l, p->y1l = y1l, p->y2l = y2l,
		p->x1r = x1r, p->x2r = x2r, p->y1r = y1r, p->y2r = y2r;
#endif /* OPT_MODE != 0 */
}

void init_pink_noise(pink_noise *p)
{
	p->b0 = p->b1 = p->b2 = p->b3 = p->b4 = p->b5 = p->b6 = 0;
}

float get_pink_noise(pink_noise *p)
{
	float b0 = p->b0, b1 = p->b1, b2 = p->b2, b3 = p->b3,
	   b4 = p->b4, b5 = p->b5, b6 = p->b6, pink, white;

	white = genrand_real1() * 2.0 - 1.0;
	b0 = 0.99886 * b0 + white * 0.0555179;
	b1 = 0.99332 * b1 + white * 0.0750759;
	b2 = 0.96900 * b2 + white * 0.1538520;
	b3 = 0.86650 * b3 + white * 0.3104856;
	b4 = 0.55000 * b4 + white * 0.5329522;
	b5 = -0.7616 * b5 - white * 0.0168980;
	pink = b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362;
	b6 = white * 0.115926;
	pink *= 0.22;
	pink = (pink > 1.0) ? 1.0 : (pink < -1.0) ? -1.0 : pink;

	p->b0 = b0, p->b1 = b1, p->b2 = b2, p->b3 = b3,
		p->b4 = b4, p->b5 = b5, p->b6 = b6;

	return pink;
}

float get_pink_noise_light(pink_noise *p)
{
	float b0 = p->b0, b1 = p->b1, b2 = p->b2, pink, white;

	white = genrand_real1() * 2.0 - 1.0;
	b0 = 0.99765 * b0 + white * 0.0990460;
	b1 = 0.96300 * b1 + white * 0.2965164;
	b2 = 0.57000 * b2 + white * 1.0526913;
	pink = b0 + b1 + b2 + white * 0.1848;
	pink *= 0.22;
	pink = (pink > 1.0) ? 1.0 : (pink < -1.0) ? -1.0 : pink;

	p->b0 = b0, p->b1 = b1, p->b2 = b2;

	return pink;
}

/*                          */
/*  Standard Reverb Effect  */
/*                          */
#define REV_VAL0         5.3
#define REV_VAL1        10.5
#define REV_VAL2        44.12
#define REV_VAL3        21.0

static int32  reverb_effect_buffer[AUDIO_BUFFER_SIZE * 2];
static int32  reverb_effect_bufsize = sizeof(reverb_effect_buffer);

#if OPT_MODE != 0
#if _MSC_VER
void set_ch_reverb(int32 *buf, int32 count, int32 level)
{
	int32 *dbuf = reverb_effect_buffer;
	if(!level) {return;}
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
    int32 i, *dbuf = reverb_effect_buffer;
	if(!level) {return;}
    level = TIM_FSCALE(level / 127.0 * REV_INP_LEV, 24);

	for(i = count - 1; i >= 0; i--) {dbuf[i] += imuldiv24(buf[i], level);}
}
#endif	/* _MSC_VER */
#else
void set_ch_reverb(register int32 *sbuffer, int32 n, int32 level)
{
    register int32  i;
	if(!level) {return;}
    FLOAT_T send_level = (FLOAT_T)level / 127.0 * REV_INP_LEV;
	
	for(i = 0; i < n; i++)
    {
        reverb_effect_buffer[i] += sbuffer[i] * send_level;
    }
}
#endif /* OPT_MODE != 0 */

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

static void init_standard_reverb(InfoStandardReverb *info)
{
	double time;
	info->ta = info->tb = 0;
	info->HPFL = info->HPFR = info->LPFL = info->LPFR = info->EPFL = info->EPFR = 0;
	info->spt0 = info->spt1 = info->spt2 = info->spt3 = 0;
	time = reverb_time_table[reverb_status_gs.time] * gs_revchar_to_rt(reverb_status_gs.character) 
		/ reverb_time_table[64] * 0.8;
	info->rpt0 = REV_VAL0 * play_mode->rate / 1000.0f * time;
	info->rpt1 = REV_VAL1 * play_mode->rate / 1000.0f * time;
	info->rpt2 = REV_VAL2 * play_mode->rate / 1000.0f * time;
	info->rpt3 = REV_VAL3 * play_mode->rate / 1000.0f * time;
	while (!isprime(info->rpt0)) {info->rpt0++;}
	while (!isprime(info->rpt1)) {info->rpt1++;}
	while (!isprime(info->rpt2)) {info->rpt2++;}
	while (!isprime(info->rpt3)) {info->rpt3++;}
	set_delay(&(info->buf0_L), info->rpt0 + 1);
	set_delay(&(info->buf0_R), info->rpt0 + 1);
	set_delay(&(info->buf1_L), info->rpt1 + 1);
	set_delay(&(info->buf1_R), info->rpt1 + 1);
	set_delay(&(info->buf2_L), info->rpt2 + 1);
	set_delay(&(info->buf2_R), info->rpt2 + 1);
	set_delay(&(info->buf3_L), info->rpt3 + 1);
	set_delay(&(info->buf3_R), info->rpt3 + 1);
	info->fbklev = 0.12f;
	info->nmixlev = 0.7f;
	info->cmixlev = 0.9f;
	info->monolev = 0.7f;
	info->hpflev = 0.5f;
	info->lpflev = 0.45f;
	info->lpfinp = 0.55f;
	info->epflev = 0.4f;
	info->epfinp = 0.48f;
	info->width = 0.125f;
	info->wet = 2.0f * (double)reverb_status_gs.level / 127.0f * gs_revchar_to_level(reverb_status_gs.character);
	info->fbklevi = TIM_FSCALE(info->fbklev, 24);
	info->nmixlevi = TIM_FSCALE(info->nmixlev, 24);
	info->cmixlevi = TIM_FSCALE(info->cmixlev, 24);
	info->monolevi = TIM_FSCALE(info->monolev, 24);
	info->hpflevi = TIM_FSCALE(info->hpflev, 24);
	info->lpflevi = TIM_FSCALE(info->lpflev, 24);
	info->lpfinpi = TIM_FSCALE(info->lpfinp, 24);
	info->epflevi = TIM_FSCALE(info->epflev, 24);
	info->epfinpi = TIM_FSCALE(info->epfinp, 24);
	info->widthi = TIM_FSCALE(info->width, 24);
	info->weti = TIM_FSCALE(info->wet, 24);
}

static void free_standard_reverb(InfoStandardReverb *info)
{
	free_delay(&(info->buf0_L));
	free_delay(&(info->buf0_R));
	free_delay(&(info->buf1_L));
	free_delay(&(info->buf1_R));
	free_delay(&(info->buf2_L));
	free_delay(&(info->buf2_R));
	free_delay(&(info->buf3_L));
	free_delay(&(info->buf3_R));
}

/*! Standard Reverberator; this implementation is specialized for system effect. */
#if OPT_MODE != 0 /* fixed-point implementation */
static void do_ch_standard_reverb(int32 *buf, int32 count, InfoStandardReverb *info)
{
	int32 i, fixp, s, t;
	int32 spt0 = info->spt0, spt1 = info->spt1, spt2 = info->spt2, spt3 = info->spt3,
		ta = info->ta, tb = info->tb, HPFL = info->HPFL, HPFR = info->HPFR,
		LPFL = info->LPFL, LPFR = info->LPFR, EPFL = info->EPFL, EPFR = info->EPFR;
	int32 *buf0_L = info->buf0_L.buf, *buf0_R = info->buf0_R.buf,
		*buf1_L = info->buf1_L.buf, *buf1_R = info->buf1_R.buf,
		*buf2_L = info->buf2_L.buf, *buf2_R = info->buf2_R.buf,
		*buf3_L = info->buf3_L.buf, *buf3_R = info->buf3_R.buf;
	int32 fbklevi = info->fbklevi, cmixlevi = info->cmixlevi,
		hpflevi = info->hpflevi, lpflevi = info->lpflevi, lpfinpi = info->lpfinpi,
		epflevi = info->epflevi, epfinpi = info->epfinpi, widthi = info->widthi,
		rpt0 = info->rpt0, rpt1 = info->rpt1, rpt2 = info->rpt2, rpt3 = info->rpt3, weti = info->weti;

	if(count == MAGIC_INIT_EFFECT_INFO) {
		init_standard_reverb(info);
		return;
	} else if(count == MAGIC_FREE_EFFECT_INFO) {
		free_standard_reverb(info);
		return;
	}

	for (i = 0; i < count; i++)
	{
        /* L */
        fixp = reverb_effect_buffer[i];

        LPFL = imuldiv24(LPFL, lpflevi) + imuldiv24(buf2_L[spt2] + tb, lpfinpi) + imuldiv24(ta, widthi);
        ta = buf3_L[spt3];
        s  = buf3_L[spt3] = buf0_L[spt0];
        buf0_L[spt0] = -LPFL;

        t = imuldiv24(HPFL + fixp, hpflevi);
        HPFL = t - fixp;

        buf2_L[spt2] = imuldiv24(s - imuldiv24(fixp, fbklevi), cmixlevi);
        tb = buf1_L[spt1];
        buf1_L[spt1] = t;

        EPFL = imuldiv24(EPFL, epflevi) + imuldiv24(ta, epfinpi);
        buf[i] += imuldiv24(ta + EPFL, weti);

        /* R */
        fixp = reverb_effect_buffer[++i];

        LPFR = imuldiv24(LPFR, lpflevi) + imuldiv24(buf2_R[spt2] + tb, lpfinpi) + imuldiv24(ta, widthi);
        ta = buf3_R[spt3];
        s  = buf3_R[spt3] = buf0_R[spt0];
        buf0_R[spt0] = LPFR;

        t = imuldiv24(HPFR + fixp, hpflevi);
        HPFR = t - fixp;

        buf2_R[spt2] = imuldiv24(s - imuldiv24(fixp, fbklevi), cmixlevi);
        tb = buf1_R[spt1];
        buf1_R[spt1] = t;

        EPFR = imuldiv24(EPFR, epflevi) + imuldiv24(ta, epfinpi);
        buf[i] += imuldiv24(ta + EPFR, weti);

		if (++spt0 == rpt0) {spt0 = 0;}
		if (++spt1 == rpt1) {spt1 = 0;}
		if (++spt2 == rpt2) {spt2 = 0;}
		if (++spt3 == rpt3) {spt3 = 0;}
	}
	memset(reverb_effect_buffer, 0, sizeof(int32) * count);
	info->spt0 = spt0, info->spt1 = spt1, info->spt2 = spt2, info->spt3 = spt3,
	info->ta = ta, info->tb = tb, info->HPFL = HPFL, info->HPFR = HPFR,
	info->LPFL = LPFL, info->LPFR = LPFR, info->EPFL = EPFL, info->EPFR = EPFR;
}
#else /* floating-point implementation */
static void do_ch_standard_reverb(int32 *buf, int32 count, InfoStandardReverb *info)
{
	int32 i, fixp, s, t;
	int32 spt0 = info->spt0, spt1 = info->spt1, spt2 = info->spt2, spt3 = info->spt3,
		ta = info->ta, tb = info->tb, HPFL = info->HPFL, HPFR = info->HPFR,
		LPFL = info->LPFL, LPFR = info->LPFR, EPFL = info->EPFL, EPFR = info->EPFR;
	int32 *buf0_L = info->buf0_L.buf, *buf0_R = info->buf0_R.buf,
		*buf1_L = info->buf1_L.buf, *buf1_R = info->buf1_R.buf,
		*buf2_L = info->buf2_L.buf, *buf2_R = info->buf2_R.buf,
		*buf3_L = info->buf3_L.buf, *buf3_R = info->buf3_R.buf;
	FLOAT_T fbklev = info->fbklev, cmixlev = info->cmixlev,
		hpflev = info->hpflev, lpflev = info->lpflev, lpfinp = info->lpfinp,
		epflev = info->epflev, epfinp = info->epfinp, width = info->width,
		rpt0 = info->rpt0, rpt1 = info->rpt1, rpt2 = info->rpt2, rpt3 = info->rpt3, wet = info->wet;

	if(count == MAGIC_INIT_EFFECT_INFO) {
		init_standard_reverb(info);
		return;
	} else if(count == MAGIC_FREE_EFFECT_INFO) {
		free_standard_reverb(info);
		return;
	}

	for (i = 0; i < count; i++)
	{
        /* L */
        fixp = reverb_effect_buffer[i];

        LPFL = LPFL * lpflev + (buf2_L[spt2] + tb) * lpfinp + ta * width;
        ta = buf3_L[spt3];
        s  = buf3_L[spt3] = buf0_L[spt0];
        buf0_L[spt0] = -LPFL;

        t = (HPFL + fixp) * hpflev;
        HPFL = t - fixp;

        buf2_L[spt2] = (s - fixp * fbklev) * cmixlev;
        tb = buf1_L[spt1];
        buf1_L[spt1] = t;

        EPFL = EPFL * epflev + ta * epfinp;
        buf[i] += (ta + EPFL) * wet;

        /* R */
        fixp = reverb_effect_buffer[++i];

        LPFR = LPFR * lpflev + (buf2_R[spt2] + tb) * lpfinp + ta * width;
        ta = buf3_R[spt3];
        s  = buf3_R[spt3] = buf0_R[spt0];
        buf0_R[spt0] = LPFR;

        t = (HPFR + fixp) * hpflev;
        HPFR = t - fixp;

        buf2_R[spt2] = (s - fixp * fbklev) * cmixlev;
        tb = buf1_R[spt1];
        buf1_R[spt1] = t;

        EPFR = EPFR * epflev + ta * epfinp;
        buf[i] += (ta + EPFR) * wet;

		if (++spt0 == rpt0) {spt0 = 0;}
		if (++spt1 == rpt1) {spt1 = 0;}
		if (++spt2 == rpt2) {spt2 = 0;}
		if (++spt3 == rpt3) {spt3 = 0;}
	}
	memset(reverb_effect_buffer, 0, sizeof(int32) * count);
	info->spt0 = spt0, info->spt1 = spt1, info->spt2 = spt2, info->spt3 = spt3,
	info->ta = ta, info->tb = tb, info->HPFL = HPFL, info->HPFR = HPFR,
	info->LPFL = LPFL, info->LPFR = LPFR, info->EPFL = EPFL, info->EPFR = EPFR;
}
#endif /* OPT_MODE != 0 */

/*! Standard Monoral Reverberator; this implementation is specialized for system effect. */
static void do_ch_standard_reverb_mono(int32 *buf, int32 count, InfoStandardReverb *info)
{
	int32 i, fixp, s, t;
	int32 spt0 = info->spt0, spt1 = info->spt1, spt2 = info->spt2, spt3 = info->spt3,
		ta = info->ta, tb = info->tb, HPFL = info->HPFL, HPFR = info->HPFR,
		LPFL = info->LPFL, LPFR = info->LPFR, EPFL = info->EPFL, EPFR = info->EPFR;
	int32 *buf0_L = info->buf0_L.buf, *buf0_R = info->buf0_R.buf,
		*buf1_L = info->buf1_L.buf, *buf1_R = info->buf1_R.buf,
		*buf2_L = info->buf2_L.buf, *buf2_R = info->buf2_R.buf,
		*buf3_L = info->buf3_L.buf, *buf3_R = info->buf3_R.buf;
	FLOAT_T fbklev = info->fbklev, nmixlev = info->nmixlev, monolev = info->monolev,
		hpflev = info->hpflev, lpflev = info->lpflev, lpfinp = info->lpfinp,
		epflev = info->epflev, epfinp = info->epfinp, width = info->width,
		rpt0 = info->rpt0, rpt1 = info->rpt1, rpt2 = info->rpt2, rpt3 = info->rpt3, wet = info->wet;

	if(count == MAGIC_INIT_EFFECT_INFO) {
		init_standard_reverb(info);
		return;
	} else if(count == MAGIC_FREE_EFFECT_INFO) {
		free_standard_reverb(info);
		return;
	}

	for (i = 0; i < count; i++)
	{
        /* L */
        fixp = buf[i] * monolev;

        LPFL = LPFL * lpflev + (buf2_L[spt2] + tb) * lpfinp + ta * width;
        ta = buf3_L[spt3];
        s  = buf3_L[spt3] = buf0_L[spt0];
        buf0_L[spt0] = -LPFL;

        t = (HPFL + fixp) * hpflev;
        HPFL = t - fixp;

        buf2_L[spt2] = (s - fixp * fbklev) * nmixlev;
        tb = buf1_L[spt1];
        buf1_L[spt1] = t;

        /* R */
        LPFR = LPFR * lpflev + (buf2_R[spt2] + tb) * lpfinp + ta * width;
        ta = buf3_R[spt3];
        s  = buf3_R[spt3] = buf0_R[spt0];
        buf0_R[spt0] = LPFR;

        t = (HPFR + fixp) * hpflev;
        HPFR = t - fixp;

        buf2_R[spt2] = (s - fixp * fbklev) * nmixlev;
        tb = buf1_R[spt1];
        buf1_R[spt1] = t;

        EPFR = EPFR * epflev + ta * epfinp;
        buf[i] = (ta + EPFR) * wet + fixp;

		if (++spt0 == rpt0) {spt0 = 0;}
		if (++spt1 == rpt1) {spt1 = 0;}
		if (++spt2 == rpt2) {spt2 = 0;}
		if (++spt3 == rpt3) {spt3 = 0;}
	}
	memset(reverb_effect_buffer, 0, sizeof(int32) * count);
	info->spt0 = spt0, info->spt1 = spt1, info->spt2 = spt2, info->spt3 = spt3,
	info->ta = ta, info->tb = tb, info->HPFL = HPFL, info->HPFR = HPFR,
	info->LPFL = LPFL, info->LPFR = LPFR, info->EPFL = EPFL, info->EPFR = EPFR;
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

/*             */
/*  Freeverb   */
/*             */
static void set_freeverb_allpass(allpass *allpass, int32 size)
{
	if(allpass->buf != NULL) {
		free(allpass->buf);
		allpass->buf = NULL;
	}
	allpass->buf = (int32 *)safe_malloc(sizeof(int32) * size);
	if(allpass->buf == NULL) {return;}
	allpass->index = 0;
	allpass->size = size;
}

static void init_freeverb_allpass(allpass *allpass)
{
	memset(allpass->buf, 0, sizeof(int32) * allpass->size);
}

static void set_freeverb_comb(comb *comb, int32 size)
{
	if(comb->buf != NULL) {
		free(comb->buf);
		comb->buf = NULL;
	}
	comb->buf = (int32 *)safe_malloc(sizeof(int32) * size);
	if(comb->buf == NULL) {return;}
	comb->index = 0;
	comb->size = size;
	comb->filterstore = 0;
}

static void init_freeverb_comb(comb *comb)
{
	memset(comb->buf, 0, sizeof(int32) * comb->size);
}

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
#define combfbk 3.0f

static void realloc_freeverb_buf(InfoFreeverb *rev)
{
	int i;
	int32 tmpL, tmpR;
	double time, samplerate = play_mode->rate;

	time = reverb_time_table[reverb_status_gs.time] * gs_revchar_to_rt(reverb_status_gs.character) * combfbk
		/ (60 * combtunings[numcombs - 1] / (-20 * log10(rev->roomsize1) * 44100.0));

	for(i = 0; i < numcombs; i++)
	{
		tmpL = combtunings[i] * samplerate * time / 44100.0;
		tmpR = (combtunings[i] + stereospread) * samplerate * time / 44100.0;
		if(tmpL < 10) tmpL = 10;
		if(tmpR < 10) tmpR = 10;
		while(!isprime(tmpL)) tmpL++;
		while(!isprime(tmpR)) tmpR++;
		rev->combL[i].size = tmpL;
		rev->combR[i].size = tmpR;
		set_freeverb_comb(&rev->combL[i], rev->combL[i].size);
		set_freeverb_comb(&rev->combR[i], rev->combR[i].size);
	}

	for(i = 0; i < numallpasses; i++)
	{
		tmpL = allpasstunings[i] * samplerate * time / 44100.0;
		tmpR = (allpasstunings[i] + stereospread) * samplerate * time / 44100.0;
		if(tmpL < 10) tmpL = 10;
		if(tmpR < 10) tmpR = 10;
		while(!isprime(tmpL)) tmpL++;
		while(!isprime(tmpR)) tmpR++;
		rev->allpassL[i].size = tmpL;
		rev->allpassR[i].size = tmpR;
		set_freeverb_allpass(&rev->allpassL[i], rev->allpassL[i].size);
		set_freeverb_allpass(&rev->allpassR[i], rev->allpassR[i].size);
	}
}

static void update_freeverb(InfoFreeverb *rev)
{
	int i;
	double allpassfbk = 0.55, rtbase, rt;

	rev->wet = (double)reverb_status_gs.level / 127.0f * gs_revchar_to_level(reverb_status_gs.character) * fixedgain;
	rev->roomsize = gs_revchar_to_roomsize(reverb_status_gs.character) * scaleroom + offsetroom;
	rev->width = gs_revchar_to_width(reverb_status_gs.character);

	rev->wet1 = rev->width / 2.0f + 0.5f;
	rev->wet2 = (1.0f - rev->width) / 2.0f;
	rev->roomsize1 = rev->roomsize;
	rev->damp1 = rev->damp;

	realloc_freeverb_buf(rev);

	rtbase = 1.0 / (44100.0 * reverb_time_table[reverb_status_gs.time] * gs_revchar_to_rt(reverb_status_gs.character));

	for(i = 0; i < numcombs; i++)
	{
		rt = pow(10.0f, -combfbk * (double)combtunings[i] * rtbase);
		rev->combL[i].feedback = rt;
		rev->combR[i].feedback = rt;
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

	set_delay(&(rev->pdelay), (int32)((double)reverb_status_gs.pre_delay_time * play_mode->rate / 1000.0f));
}

static void init_freeverb(InfoFreeverb *rev)
{
	int i;
	for(i = 0; i < numcombs; i++) {
		init_freeverb_comb(&rev->combL[i]);
		init_freeverb_comb(&rev->combR[i]);
	}
	for(i = 0; i < numallpasses; i++) {
		init_freeverb_allpass(&rev->allpassL[i]);
		init_freeverb_allpass(&rev->allpassR[i]);
	}
}

static void alloc_freeverb_buf(InfoFreeverb *rev)
{
	int i;
	if(rev->alloc_flag) {return;}
	for (i = 0; i < numcombs; i++) {
		set_freeverb_comb(&rev->combL[i], combtunings[i]);
		set_freeverb_comb(&rev->combR[i], combtunings[i] + stereospread);
	}
	for (i = 0; i < numallpasses; i++) {
		set_freeverb_allpass(&rev->allpassL[i], allpasstunings[i]);
		set_freeverb_allpass(&rev->allpassR[i], allpasstunings[i] + stereospread);
		rev->allpassL[i].feedback = initialallpassfbk;
		rev->allpassR[i].feedback = initialallpassfbk;
	}

	rev->wet = initialwet * scalewet;
	rev->damp = initialdamp * scaledamp;
	rev->width = initialwidth;
	rev->roomsize = initialroom * scaleroom + offsetroom;

	rev->alloc_flag = 1;
}

static void free_freeverb_buf(InfoFreeverb *rev)
{
	int i;
	
	for(i = 0; i < numcombs; i++)
	{
		if(rev->combL[i].buf != NULL) {
			free(rev->combL[i].buf);
			rev->combL[i].buf = NULL;
		}
		if(rev->combR[i].buf != NULL) {
			free(rev->combR[i].buf);
			rev->combR[i].buf = NULL;
		}
	}
	for(i = 0; i < numallpasses; i++)
	{
		if(rev->allpassL[i].buf != NULL) {
			free(rev->allpassL[i].buf);
			rev->allpassL[i].buf = NULL;
		}
		if(rev->allpassR[i].buf != NULL) {
			free(rev->allpassR[i].buf);
			rev->allpassR[i].buf = NULL;
		}
	}
	free_delay(&(rev->pdelay));
}

#if OPT_MODE != 0	/* fixed-point implementation */
#define do_freeverb_allpass(_stream, _apbuf, _apsize, _apindex, _apfeedback) \
{ \
	_bufout = _apbuf[_apindex];	\
	_output = -_stream + _bufout;	\
	_apbuf[_apindex] = _stream + imuldiv24(_bufout, _apfeedback);	\
	if(++_apindex >= _apsize) {	\
		_apindex = 0;	\
	}	\
	_stream = _output;	\
}

#define do_freeverb_comb(_input, _ostream, _cbuf, _csize, _cindex, _cdamp1, _cdamp2, _cfs, _cfeedback)	\
{	\
	_output = _cbuf[_cindex];	\
	_cfs = imuldiv24(_output, _cdamp2) + imuldiv24(_cfs, _cdamp1);	\
	_cbuf[_cindex] = _input + imuldiv24(_cfs, _cfeedback);	\
	if(++_cindex >= _csize) {	\
		_cindex = 0;	\
	}	\
	_ostream += _output;	\
}

static void do_ch_freeverb(int32 *buf, int32 count, InfoFreeverb *rev)
{
	int32 i, k = 0;
	int32 outl, outr, input;
	comb *combL = rev->combL, *combR = rev->combR;
	allpass *allpassL = rev->allpassL, *allpassR = rev->allpassR;
	delay *pdelay = &(rev->pdelay);

	if(count == MAGIC_INIT_EFFECT_INFO) {
		alloc_freeverb_buf(rev);
		update_freeverb(rev);
		init_freeverb(rev);
		return;
	} else if(count == MAGIC_FREE_EFFECT_INFO) {
		free_freeverb_buf(rev);
		return;
	}

	for (k = 0; k < count; k++)
	{
		input = reverb_effect_buffer[k] + reverb_effect_buffer[k + 1];
		outl = outr = reverb_effect_buffer[k] = reverb_effect_buffer[k + 1] = 0;

		do_delay(input, pdelay->buf, pdelay->size, pdelay->index);

		for (i = 0; i < numcombs; i++) {
			do_freeverb_comb(input, outl, combL[i].buf, combL[i].size, combL[i].index,
				combL[i].damp1i, combL[i].damp2i, combL[i].filterstore, combL[i].feedbacki);
			do_freeverb_comb(input, outr, combR[i].buf, combR[i].size, combR[i].index,
				combR[i].damp1i, combR[i].damp2i, combR[i].filterstore, combR[i].feedbacki);
		}
		for (i = 0; i < numallpasses; i++) {
			do_freeverb_allpass(outl, allpassL[i].buf, allpassL[i].size, allpassL[i].index, allpassL[i].feedbacki);
			do_freeverb_allpass(outr, allpassR[i].buf, allpassR[i].size, allpassR[i].index, allpassR[i].feedbacki);
		}
		buf[k] += imuldiv24(outl, rev->wet1i) + imuldiv24(outr, rev->wet2i);
		buf[k + 1] += imuldiv24(outr, rev->wet1i) + imuldiv24(outl, rev->wet2i);
		++k;
	}
}
#else	/* floating-point implementation */
#define do_freeverb_allpass(_stream, _apbuf, _apsize, _apindex, _apfeedback) \
{ \
	_bufout = _apbuf[_apindex];	\
	_output = -_stream + _bufout;	\
	_apbuf[_apindex] = _stream + (_bufout * _apfeedback);	\
	if(++_apindex >= _apsize) {	\
		_apindex = 0;	\
	}	\
	_stream = _output;	\
}

#define do_freeverb_comb(_input, _ostream, _cbuf, _csize, _cindex, _cdamp1, _cdamp2, _cfs, _cfeedback)	\
{	\
	_output = _cbuf[_cindex];	\
	_cfs = (_output * _cdamp2) + (_cfs * _cdamp1);	\
	_cbuf[_cindex] = _input + (_cfs * _cfeedback);	\
	if(++_cindex >= _csize) {	\
		_cindex = 0;	\
	}	\
	_ostream += _output;	\
}

static void do_ch_freeverb(int32 *buf, int32 count, InfoFreeverb *rev)
{
	int32 i, k = 0;
	int32 outl, outr, input;
	delay *pdelay = &(rev->pdelay);

	if(count == MAGIC_INIT_EFFECT_INFO) {
		alloc_freeverb_buf(rev);
		update_freeverb(rev);
		init_freeverb(rev);
		return;
	} else if(count == MAGIC_FREE_EFFECT_INFO) {
		free_freeverb_buf(rev);
		return;
	}

	for (k = 0; k < count; k++)
	{
		input = reverb_effect_buffer[k] + reverb_effect_buffer[k + 1];
		outl = outr = reverb_effect_buffer[k] = reverb_effect_buffer[k + 1] = 0;

		do_delay(input, pdelay->buf, pdelay->size, pdelay->index);

		for (i = 0; i < numcombs; i++) {
			do_freeverb_comb(input, outl, rev->combL[i].buf, rev->combL[i].size, rev->combL[i].index,
				rev->combL[i].damp1, rev->combL[i].damp2, rev->combL[i].filterstore, rev->combL[i].feedback);
			do_freeverb_comb(input, outr, rev->combR[i].buf, rev->combR[i].size, rev->combR[i].index,
				rev->combR[i].damp1, rev->combR[i].damp2, rev->combR[i].filterstore, rev->combR[i].feedback);
		}
		for (i = 0; i < numallpasses; i++) {
			do_freeverb_allpass(outl, rev->allpassL[i].buf, rev->allpassL[i].size, rev->allpassL[i].index, rev->allpassL[i].feedback);
			do_freeverb_allpass(outr, rev->allpassR[i].buf, rev->allpassR[i].size, rev->allpassR[i].index, rev->allpassR[i].feedback);
		}
		buf[k] += outl * rev->wet1 + outr * rev->wet2;
		buf[k + 1] += outr * rev->wet1 + outl * rev->wet2;
		++k;
	}
}
#endif	/* OPT_MODE != 0 */

/*                                 */
/*  Reverb: Delay & Panning Delay  */
/*                                 */
/*! initialize Reverb: Delay Effect; this implementation is specialized for system effect. */
static void init_ch_reverb_delay(InfoDelay3 *info)
{
	int32 x;
	info->size[0] = (double)reverb_status_gs.time * 3.75f * play_mode->rate / 1000.0f;
	x = info->size[0] + 1;	/* allowance */
	set_delay(&(info->delayL), x);
	set_delay(&(info->delayR), x);
	info->index[0] = x - info->size[0];
	info->level[0] = (double)reverb_status_gs.level * 1.82f / 127.0f;
	info->feedback = sqrt((double)reverb_status_gs.delay_feedback / 127.0f) * 0.98f;
	info->leveli[0] = TIM_FSCALE(info->level[0], 24);
	info->feedbacki = TIM_FSCALE(info->feedback, 24);
}

static void free_ch_reverb_delay(InfoDelay3 *info)
{
	free_delay(&(info->delayL));
	free_delay(&(info->delayR));
}

/*! Reverb: Panning Delay Effect; this implementation is specialized for system effect. */
static void do_ch_reverb_panning_delay(int32 *buf, int32 count, InfoDelay3 *info)
{
	int32 i, l, r;
	delay *delayL = &(info->delayL), *delayR = &(info->delayR);
	int32 *bufL = delayL->buf, *bufR = delayR->buf;
	int32 buf_index = delayL->index, buf_size = delayL->size;
	int32 index0 = info->index[0], level0i = info->leveli[0],
		feedbacki = info->feedbacki;

	if(count == MAGIC_INIT_EFFECT_INFO) {
		init_ch_reverb_delay(info);
		return;
	} else if(count == MAGIC_FREE_EFFECT_INFO) {
		free_ch_reverb_delay(info);
		return;
	}

	for (i = 0; i < count; i++)
	{
		bufL[buf_index] = reverb_effect_buffer[i] + imuldiv24(bufR[index0], feedbacki);
		l = imuldiv24(bufL[index0], level0i);
		bufR[buf_index] = reverb_effect_buffer[i + 1] + imuldiv24(bufL[index0], feedbacki);
		r = imuldiv24(bufR[index0], level0i);

		buf[i] += r;
		buf[++i] += l;

		if (++index0 == buf_size) {index0 = 0;}
		if (++buf_index == buf_size) {buf_index = 0;}
	}
	memset(reverb_effect_buffer, 0, sizeof(int32) * count);
	info->index[0] = index0;
	delayL->index = delayR->index = buf_index;
}

/*! Reverb: Normal Delay Effect; this implementation is specialized for system effect. */
static void do_ch_reverb_normal_delay(int32 *buf, int32 count, InfoDelay3 *info)
{
	int32 i;
	delay *delayL = &(info->delayL), *delayR = &(info->delayR);
	int32 *bufL = delayL->buf, *bufR = delayR->buf;
	int32 buf_index = delayL->index, buf_size = delayL->size;
	int32 index0 = info->index[0], level0i = info->leveli[0],
		feedbacki = info->feedbacki;

	if(count == MAGIC_INIT_EFFECT_INFO) {
		init_ch_reverb_delay(info);
		return;
	} else if(count == MAGIC_FREE_EFFECT_INFO) {
		free_ch_reverb_delay(info);
		return;
	}

	for (i = 0; i < count; i++)
	{
		bufL[buf_index] = reverb_effect_buffer[i] + imuldiv24(bufL[index0], feedbacki);
		buf[i] += imuldiv24(bufL[index0], level0i);

		bufR[buf_index] = reverb_effect_buffer[++i] + imuldiv24(bufR[index0], feedbacki);
		buf[i] += imuldiv24(bufR[index0], level0i);

		if (++index0 == buf_size) {index0 = 0;}
		if (++buf_index == buf_size) {buf_index = 0;}
	}
	memset(reverb_effect_buffer, 0, sizeof(int32) * count);
	info->index[0] = index0;
	delayL->index = delayR->index = buf_index;
}

/*                      */
/*  Plate Reverberator  */
/*                      */
#define PLATE_SAMPLERATE 29761.0
#define PLATE_DECAY 0.50
#define PLATE_DECAY_DIFFUSION1 0.70
#define PLATE_DECAY_DIFFUSION2 0.50
#define PLATE_INPUT_DIFFUSION1 0.750
#define PLATE_INPUT_DIFFUSION2 0.625
#define PLATE_BANDWIDTH 0.9955
#define PLATE_DAMPING 0.0005
#define PLATE_WET 0.25

/*! calculate delay sample in current sample-rate */
static inline int32 get_plate_delay(double delay, double t)
{
	return (int32)(delay * play_mode->rate * t / PLATE_SAMPLERATE);
}

/*! Plate Reverberator; this implementation is specialized for system effect. */
static void do_ch_plate_reverb(int32 *buf, int32 count, InfoPlateReverb *info)
{
	int32 i;
	int32 x, xd, val, outl, outr, temp1, temp2, temp3;
	delay *pd = &(info->pd), *od1l = &(info->od1l), *od2l = &(info->od2l),
		*od3l = &(info->od3l), *od4l = &(info->od4l), *od5l = &(info->od5l),
		*od6l = &(info->od6l), *od1r = &(info->od1r), *od2r = &(info->od2r),
		*od3r = &(info->od3r), *od4r = &(info->od4r), *od5r = &(info->od5r),
		*od7r = &(info->od7r), *od7l = &(info->od7l), *od6r = &(info->od6r),
		*td1 = &(info->td1), *td2 = &(info->td2), *td1d = &(info->td1d), *td2d = &(info->td2d);
	allpass *ap1 = &(info->ap1), *ap2 = &(info->ap2), *ap3 = &(info->ap3),
		*ap4 = &(info->ap4), *ap6 = &(info->ap6), *ap6d = &(info->ap6d);
	mod_allpass *ap5 = &(info->ap5), *ap5d = &(info->ap5d);
	lfo *lfo1 = &(info->lfo1), *lfo1d = &(info->lfo1d);
	filter_lowpass1 *lpf1 = &(info->lpf1), *lpf2 = &(info->lpf2);
	int32 t1 = info->t1, t1d = info->t1d;
	int32 decayi = info->decayi, ddif1i = info->ddif1i, ddif2i = info->ddif2i,
		idif1i = info->idif1i, idif2i = info->idif2i;
	double t;

	if(count == MAGIC_INIT_EFFECT_INFO) {
		init_lfo(lfo1, 1.0f, LFO_SINE, 0);
		init_lfo(lfo1d, 1.0f, LFO_SINE, 0);
		t = reverb_time_table[reverb_status_gs.time] / reverb_time_table[64] - 1.0;
		t = 1.0 + t / 2;
		set_delay(pd, reverb_status_gs.pre_delay_time * play_mode->rate / 1000);
		set_delay(td1, get_plate_delay(4453, t)),
		set_delay(td1d, get_plate_delay(4217, t));
		set_delay(td2, get_plate_delay(3720, t));
		set_delay(td2d, get_plate_delay(3163, t));
		set_delay(od1l, get_plate_delay(266, t));
		set_delay(od2l, get_plate_delay(2974, t));
		set_delay(od3l, get_plate_delay(1913, t));
		set_delay(od4l, get_plate_delay(1996, t));
		set_delay(od5l, get_plate_delay(1990, t));
		set_delay(od6l, get_plate_delay(187, t));
		set_delay(od7l, get_plate_delay(1066, t));
		set_delay(od1r, get_plate_delay(353, t));
		set_delay(od2r, get_plate_delay(3627, t));
		set_delay(od3r, get_plate_delay(1228, t));
		set_delay(od4r, get_plate_delay(2673, t));
		set_delay(od5r, get_plate_delay(2111, t));
		set_delay(od6r, get_plate_delay(335, t));
		set_delay(od7r, get_plate_delay(121, t));
		set_allpass(ap1, get_plate_delay(142, t), PLATE_INPUT_DIFFUSION1);
		set_allpass(ap2, get_plate_delay(107, t), PLATE_INPUT_DIFFUSION1);
		set_allpass(ap3, get_plate_delay(379, t), PLATE_INPUT_DIFFUSION2);
		set_allpass(ap4, get_plate_delay(277, t), PLATE_INPUT_DIFFUSION2);
		set_allpass(ap6, get_plate_delay(1800, t), PLATE_DECAY_DIFFUSION2);
		set_allpass(ap6d, get_plate_delay(2656, t), PLATE_DECAY_DIFFUSION2);
		set_mod_allpass(ap5, get_plate_delay(672, t), get_plate_delay(16, t), PLATE_DECAY_DIFFUSION1);
		set_mod_allpass(ap5d, get_plate_delay(908, t), get_plate_delay(16, t), PLATE_DECAY_DIFFUSION1);
		lpf1->a = PLATE_BANDWIDTH, lpf2->a = 1.0 - PLATE_DAMPING;
		init_filter_lowpass1(lpf1);
		init_filter_lowpass1(lpf2);
		info->t1 = info->t1d = 0;
		info->decay = PLATE_DECAY;
		info->decayi = TIM_FSCALE(info->decay, 24);
		info->ddif1 = PLATE_DECAY_DIFFUSION1;
		info->ddif1i = TIM_FSCALE(info->ddif1, 24);
		info->ddif2 = PLATE_DECAY_DIFFUSION2;
		info->ddif2i = TIM_FSCALE(info->ddif2, 24);
		info->idif1 = PLATE_INPUT_DIFFUSION1;
		info->idif1i = TIM_FSCALE(info->idif1, 24);
		info->idif2 = PLATE_INPUT_DIFFUSION2;
		info->idif2i = TIM_FSCALE(info->idif2, 24);
		info->wet = PLATE_WET * (double)reverb_status_gs.level / 127.0;
		return;
	} else if(count == MAGIC_FREE_EFFECT_INFO) {
		free_delay(pd);	free_delay(td1); free_delay(td1d); free_delay(td2);
		free_delay(td2d); free_delay(od1l); free_delay(od2l); free_delay(od3l);
		free_delay(od4l); free_delay(od5l); free_delay(od6l); free_delay(od7l);
		free_delay(od1r); free_delay(od2r);	free_delay(od3r); free_delay(od4r);
		free_delay(od5r); free_delay(od6r);	free_delay(od7r); free_allpass(ap1);
		free_allpass(ap2); free_allpass(ap3); free_allpass(ap4); free_allpass(ap6);
		free_allpass(ap6d);	free_mod_allpass(ap5); free_mod_allpass(ap5d);
		return;
	}

	for (i = 0; i < count; i++)
	{
		outr = outl = 0;
		x = (reverb_effect_buffer[i] + reverb_effect_buffer[i + 1]) >> 1;
		reverb_effect_buffer[i] = reverb_effect_buffer[i + 1] = 0;

		do_delay(x, pd->buf, pd->size, pd->index);
		do_filter_lowpass1(x, lpf1->x1l, lpf1->ai, lpf1->iai);
		do_allpass(x, ap1->buf, ap1->size, ap1->index, idif1i);
		do_allpass(x, ap2->buf, ap2->size, ap2->index, idif1i);
		do_allpass(x, ap3->buf, ap3->size, ap3->index, idif2i);
		do_allpass(x, ap4->buf, ap4->size, ap4->index, idif2i);

		/* tank structure */
		xd = x;
		x += imuldiv24(t1d, decayi);
		val = do_lfo(lfo1);
		do_mod_allpass(x, ap5->buf, ap5->size, ap5->rindex, ap5->windex,
			ap5->ndelay, ap5->depth, val, ap5->hist, ddif1i);
		temp1 = temp2 = temp3 = x;	/* n_out_1 */
		do_delay(temp1, od5l->buf, od5l->size, od5l->index);
		outl -= temp1;	/* left output 5 */
		do_delay(temp2, od1r->buf, od1r->size, od1r->index);
		outr += temp2;	/* right output 1 */
		do_delay(temp3, od2r->buf, od2r->size, od2r->index);
		outr += temp3;	/* right output 2 */
		do_delay(x, td1->buf, td1->size, td1->index);
		do_filter_lowpass1(x, lpf2->x1l, lpf2->ai, lpf2->iai);
		temp1 = temp2 = x;	/* n_out_2 */
		do_delay(temp1, od6l->buf, od6l->size, od6l->index);
		outl -= temp1;	/* left output 6 */
		do_delay(temp2, od3r->buf, od3r->size, od3r->index);
		outr -= temp2;	/* right output 3 */
		x = imuldiv24(x, decayi);
		do_allpass(x, ap6->buf, ap6->size, ap6->index, ddif2i);
		temp1 = temp2 = x;	/* n_out_3 */
		do_delay(temp1, od7l->buf, od7l->size, od7l->index);
		outl -= temp1;	/* left output 7 */
		do_delay(temp2, od4r->buf, od4r->size, od4r->index);
		outr += temp2;	/* right output 4 */
		do_delay(x, td2->buf, td2->size, td2->index);
		t1 = x;

		xd += imuldiv24(t1, decayi);
		val = do_lfo(lfo1d);
		do_mod_allpass(x, ap5d->buf, ap5d->size, ap5d->rindex, ap5d->windex,
			ap5d->ndelay, ap5d->depth, val, ap5d->hist, ddif1i);
		temp1 = temp2 = temp3 = xd;	/* n_out_4 */
		do_delay(temp1, od1l->buf, od1l->size, od1l->index);
		outl += temp1;	/* left output 1 */
		do_delay(temp2, od2l->buf, od2l->size, od2l->index);
		outl += temp2;	/* left output 2 */
		do_delay(temp3, od6r->buf, od6r->size, od6r->index);
		outr -= temp3;	/* right output 6 */
		do_delay(xd, td1d->buf, td1d->size, td1d->index);
		do_filter_lowpass1(xd, lpf2->x1r, lpf2->ai, lpf2->iai);
		temp1 = temp2 = xd;	/* n_out_5 */
		do_delay(temp1, od3l->buf, od3l->size, od3l->index);
		outl -= temp1;	/* left output 3 */
		do_delay(temp2, od6r->buf, od6r->size, od6r->index);
		outr -= temp2;	/* right output 6 */
		xd = imuldiv24(xd, decayi);
		do_allpass(xd, ap6d->buf, ap6d->size, ap6d->index, ddif2i);
		temp1 = temp2 = xd;	/* n_out_6 */
		do_delay(temp1, od4l->buf, od4l->size, od4l->index);
		outl += temp1;	/* left output 4 */
		do_delay(temp2, od7r->buf, od7r->size, od7r->index);
		outr -= temp2;	/* right output 7 */
		do_delay(xd, td2d->buf, td2d->size, td2d->index);
		t1d = xd;

		buf[i] += outl;
		buf[i + 1] += outr;

		++i;
	}
	info->t1 = t1, info->t1d = t1d;
}

/*! initialize Reverb Effect */
void init_reverb(void)
{
	init_filter_lowpass1(&(reverb_status_gs.lpf));
	/* Only initialize freeverb if stereo output */
	/* Old non-freeverb must be initialized for mono reverb not to crash */
	if (! (play_mode->encoding & PE_MONO)
			&& (opt_reverb_control == 3 || opt_reverb_control == 4
			|| (opt_reverb_control < 0 && ! (opt_reverb_control & 0x100)))) {
		switch(reverb_status_gs.character) {	/* select reverb algorithm */
		case 5:	/* Plate Reverb */
			do_ch_plate_reverb(NULL, MAGIC_INIT_EFFECT_INFO, &(reverb_status_gs.info_plate_reverb));
			REV_INP_LEV = reverb_status_gs.info_plate_reverb.wet;
			break;
		case 6:	/* Delay */
			do_ch_reverb_normal_delay(NULL, MAGIC_INIT_EFFECT_INFO, &(reverb_status_gs.info_reverb_delay));
			REV_INP_LEV = 1.0;
			break;
		case 7: /* Panning Delay */
			do_ch_reverb_panning_delay(NULL, MAGIC_INIT_EFFECT_INFO, &(reverb_status_gs.info_reverb_delay));
			REV_INP_LEV = 1.0;
			break;
		default: /* Freeverb */
			do_ch_freeverb(NULL, MAGIC_INIT_EFFECT_INFO, &(reverb_status_gs.info_freeverb));
			REV_INP_LEV = reverb_status_gs.info_freeverb.wet;
			break;
		}
	} else {	/* Old Reverb */
		do_ch_standard_reverb(NULL, MAGIC_INIT_EFFECT_INFO, &(reverb_status_gs.info_standard_reverb));
		REV_INP_LEV = 1.0;
	}
	memset(reverb_effect_buffer, 0, reverb_effect_bufsize);
	memset(direct_buffer, 0, direct_bufsize);
}

void do_ch_reverb(int32 *buf, int32 count)
{
#ifdef SYS_EFFECT_PRE_LPF
	if ((opt_reverb_control == 3 || opt_reverb_control == 4
			|| (opt_reverb_control < 0 && ! (opt_reverb_control & 0x100))) && reverb_status_gs.pre_lpf)
		do_filter_lowpass1_stereo(reverb_effect_buffer, count, &(reverb_status_gs.lpf));
#endif /* SYS_EFFECT_PRE_LPF */
	if (opt_reverb_control == 3 || opt_reverb_control == 4
			|| (opt_reverb_control < 0 && ! (opt_reverb_control & 0x100))) {
		switch(reverb_status_gs.character) {	/* select reverb algorithm */
		case 5:	/* Plate Reverb */
			do_ch_plate_reverb(buf, count, &(reverb_status_gs.info_plate_reverb));
			REV_INP_LEV = reverb_status_gs.info_plate_reverb.wet;
			break;
		case 6:	/* Delay */
			do_ch_reverb_normal_delay(buf, count, &(reverb_status_gs.info_reverb_delay));
			REV_INP_LEV = 1.0;
			break;
		case 7: /* Panning Delay */
			do_ch_reverb_panning_delay(buf, count, &(reverb_status_gs.info_reverb_delay));
			REV_INP_LEV = 1.0;
			break;
		default: /* Freeverb */
			do_ch_freeverb(buf, count, &(reverb_status_gs.info_freeverb));
			REV_INP_LEV = reverb_status_gs.info_freeverb.wet;
			break;
		}
	} else {	/* Old Reverb */
		do_ch_standard_reverb(buf, count, &(reverb_status_gs.info_standard_reverb));
	}
}

void do_mono_reverb(int32 *buf, int32 count)
{
	do_ch_standard_reverb_mono(buf, count, &(reverb_status_gs.info_standard_reverb));
}

/*                   */
/*   Delay Effect    */
/*                   */
static int32 delay_effect_buffer[AUDIO_BUFFER_SIZE * 2];
static void do_ch_3tap_delay(int32 *, int32, InfoDelay3 *);
static void do_ch_cross_delay(int32 *, int32, InfoDelay3 *);
static void do_ch_normal_delay(int32 *, int32, InfoDelay3 *);

void init_ch_delay(void)
{
	memset(delay_effect_buffer, 0, sizeof(delay_effect_buffer));
	init_filter_lowpass1(&(delay_status_gs.lpf));
	do_ch_3tap_delay(NULL, MAGIC_INIT_EFFECT_INFO, &(delay_status_gs.info_delay));
}

void do_ch_delay(int32 *buf, int32 count)
{
#ifdef SYS_EFFECT_PRE_LPF
	if ((opt_reverb_control == 3 || opt_reverb_control == 4
			|| (opt_reverb_control < 0 && ! (opt_reverb_control & 0x100))) && delay_status_gs.pre_lpf)
		do_filter_lowpass1_stereo(delay_effect_buffer, count, &(delay_status_gs.lpf));
#endif /* SYS_EFFECT_PRE_LPF */
	switch (delay_status_gs.type) {
	case 1:
		do_ch_3tap_delay(buf, count, &(delay_status_gs.info_delay));
		break;
	case 2:
		do_ch_cross_delay(buf, count, &(delay_status_gs.info_delay));
		break;
	default:
		do_ch_normal_delay(buf, count, &(delay_status_gs.info_delay));
		break;
	}
}

#if OPT_MODE != 0
#if _MSC_VER
void set_ch_delay(int32 *buf, int32 count, int32 level)
{
	int32 *dbuf = delay_effect_buffer;
	if(!level) {return;}
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
	if(!level) {return;}
	level = level * 65536 / 127;

	for(i = n - 1; i >= 0; i--) {buf[i] += imuldiv16(sbuffer[i], level);}
}
#endif	/* _MSC_VER */
#else
void set_ch_delay(register int32 *sbuffer, int32 n, int32 level)
{
    register int32 i;
	if(!level) {return;}
    FLOAT_T send_level = (FLOAT_T)level / 127.0f;

    for(i = 0; i < n; i++)
    {
        delay_effect_buffer[i] += sbuffer[i] * send_level;
    }
}
#endif /* OPT_MODE != 0 */

/*! initialize Delay Effect; this implementation is specialized for system effect. */
static void init_ch_3tap_delay(InfoDelay3 *info)
{
	int32 i, x;

	info->size[0] = delay_status_gs.sample_c;
	info->size[1] = delay_status_gs.sample_l;
	info->size[2] = delay_status_gs.sample_r;
	x = info->size[0];	/* find maximum value */
	for (i = 1; i < 3; i++) {
		if (info->size[i] > x) {x = info->size[i];}
	}
	x += 1;	/* allowance */
	set_delay(&(info->delayL), x);
	set_delay(&(info->delayR), x);
	for (i = 0; i < 3; i++) {	/* set start-point */
		info->index[i] = x - info->size[i];
	}
	info->level[0] = delay_status_gs.level_ratio_c * MASTER_DELAY_LEVEL;
	info->level[1] = delay_status_gs.level_ratio_l * MASTER_DELAY_LEVEL;
	info->level[2] = delay_status_gs.level_ratio_r * MASTER_DELAY_LEVEL;
	info->feedback = delay_status_gs.feedback_ratio;
	info->send_reverb = delay_status_gs.send_reverb_ratio * REV_INP_LEV;
	for (i = 0; i < 3; i++) {
		info->leveli[i] = TIM_FSCALE(info->level[i], 24);
	}
	info->feedbacki = TIM_FSCALE(info->feedback, 24);
	info->send_reverbi = TIM_FSCALE(info->send_reverb, 24);
}

static void free_ch_3tap_delay(InfoDelay3 *info)
{
	free_delay(&(info->delayL));
	free_delay(&(info->delayR));
}

/*! 3-Tap Stereo Delay Effect; this implementation is specialized for system effect. */
static void do_ch_3tap_delay(int32 *buf, int32 count, InfoDelay3 *info)
{
	int32 i, x;
	delay *delayL = &(info->delayL), *delayR = &(info->delayR);
	int32 *bufL = delayL->buf, *bufR = delayR->buf;
	int32 buf_index = delayL->index, buf_size = delayL->size;
	int32 index0 = info->index[0], index1 = info->index[1], index2 = info->index[2];
	int32 level0i = info->leveli[0], level1i = info->leveli[1], level2i = info->leveli[2],
		feedbacki = info->feedbacki, send_reverbi = info->send_reverbi;

	if(count == MAGIC_INIT_EFFECT_INFO) {
		init_ch_3tap_delay(info);
		return;
	} else if(count == MAGIC_FREE_EFFECT_INFO) {
		free_ch_3tap_delay(info);
		return;
	}

	for (i = 0; i < count; i++)
	{
		bufL[buf_index] = delay_effect_buffer[i] + imuldiv24(bufL[index0], feedbacki);
		x = imuldiv24(bufL[index0], level0i) + imuldiv24(bufL[index1] + bufR[index1], level1i);
		buf[i] += x;
		reverb_effect_buffer[i] += imuldiv24(x, send_reverbi);

		bufR[buf_index] = delay_effect_buffer[++i] + imuldiv24(bufR[index0], feedbacki);
		x = imuldiv24(bufR[index0], level0i) + imuldiv24(bufL[index2] + bufR[index2], level2i);
		buf[i] += x;
		reverb_effect_buffer[i] += imuldiv24(x, send_reverbi);

		if (++index0 == buf_size) {index0 = 0;}
		if (++index1 == buf_size) {index1 = 0;}
		if (++index2 == buf_size) {index2 = 0;}
		if (++buf_index == buf_size) {buf_index = 0;}
	}
	memset(delay_effect_buffer, 0, sizeof(int32) * count);
	info->index[0] = index0, info->index[1] = index1, info->index[2] = index2;
	delayL->index = delayR->index = buf_index;
}

/*! Cross Delay Effect; this implementation is specialized for system effect. */
static void do_ch_cross_delay(int32 *buf, int32 count, InfoDelay3 *info)
{
	int32 i, l, r;
	delay *delayL = &(info->delayL), *delayR = &(info->delayR);
	int32 *bufL = delayL->buf, *bufR = delayR->buf;
	int32 buf_index = delayL->index, buf_size = delayL->size;
	int32 index0 = info->index[0], level0i = info->leveli[0],
		feedbacki = info->feedbacki, send_reverbi = info->send_reverbi;

	if(count == MAGIC_INIT_EFFECT_INFO) {
		init_ch_3tap_delay(info);
		return;
	} else if(count == MAGIC_FREE_EFFECT_INFO) {
		free_ch_3tap_delay(info);
		return;
	}

	for (i = 0; i < count; i++)
	{
		bufL[buf_index] = delay_effect_buffer[i] + imuldiv24(bufR[index0], feedbacki);
		l = imuldiv24(bufL[index0], level0i);
		bufR[buf_index] = delay_effect_buffer[i + 1] + imuldiv24(bufL[index0], feedbacki);
		r = imuldiv24(bufR[index0], level0i);

		buf[i] += r;
		reverb_effect_buffer[i] += imuldiv24(r, send_reverbi);
		buf[++i] += l;
		reverb_effect_buffer[i] += imuldiv24(l, send_reverbi);

		if (++index0 == buf_size) {index0 = 0;}
		if (++buf_index == buf_size) {buf_index = 0;}
	}
	memset(delay_effect_buffer, 0, sizeof(int32) * count);
	info->index[0] = index0;
	delayL->index = delayR->index = buf_index;
}

/*! Normal Delay Effect; this implementation is specialized for system effect. */
static void do_ch_normal_delay(int32 *buf, int32 count, InfoDelay3 *info)
{
	int32 i, x;
	delay *delayL = &(info->delayL), *delayR = &(info->delayR);
	int32 *bufL = delayL->buf, *bufR = delayR->buf;
	int32 buf_index = delayL->index, buf_size = delayL->size;
	int32 index0 = info->index[0], level0i = info->leveli[0],
		feedbacki = info->feedbacki, send_reverbi = info->send_reverbi;

	if(count == MAGIC_INIT_EFFECT_INFO) {
		init_ch_3tap_delay(info);
		return;
	} else if(count == MAGIC_FREE_EFFECT_INFO) {
		free_ch_3tap_delay(info);
		return;
	}

	for (i = 0; i < count; i++)
	{
		bufL[buf_index] = delay_effect_buffer[i] + imuldiv24(bufL[index0], feedbacki);
		x = imuldiv24(bufL[index0], level0i);
		buf[i] += x;
		reverb_effect_buffer[i] += imuldiv24(x, send_reverbi);

		bufR[buf_index] = delay_effect_buffer[++i] + imuldiv24(bufR[index0], feedbacki);
		x = imuldiv24(bufR[index0], level0i);
		buf[i] += x;
		reverb_effect_buffer[i] += imuldiv24(x, send_reverbi);

		if (++index0 == buf_size) {index0 = 0;}
		if (++buf_index == buf_size) {buf_index = 0;}
	}
	memset(delay_effect_buffer, 0, sizeof(int32) * count);
	info->index[0] = index0;
	delayL->index = delayR->index = buf_index;
}

/*                             */
/*        Chorus Effect        */
/*                             */
static int32 chorus_effect_buffer[AUDIO_BUFFER_SIZE * 2];

/*! Stereo Chorus; this implementation is specialized for system effect. */
static void do_ch_stereo_chorus(int32 *buf, int32 count, InfoStereoChorus *info)
{
	int32 i, output, f0, f1, v0, v1;
	int32 *bufL = info->delayL.buf, *bufR = info->delayR.buf,
		*lfobufL = info->lfoL.buf, *lfobufR = info->lfoR.buf,
		icycle = info->lfoL.icycle, cycle = info->lfoL.cycle,
		leveli = info->leveli, feedbacki = info->feedbacki,
		send_reverbi = info->send_reverbi, send_delayi = info->send_delayi,
		depth = info->depth, pdelay = info->pdelay, rpt0 = info->rpt0;
	int32 wpt0 = info->wpt0, spt0 = info->spt0, spt1 = info->spt1,
		hist0 = info->hist0, hist1 = info->hist1, lfocnt = info->lfoL.count;

	if(count == MAGIC_INIT_EFFECT_INFO) {
		init_lfo(&(info->lfoL), (double)chorus_status_gs.rate * 0.122f, LFO_SINE, 0);
		init_lfo(&(info->lfoR), (double)chorus_status_gs.rate * 0.122f, LFO_SINE, 90);
		info->pdelay = chorus_delay_time_table[chorus_status_gs.delay] * (double)play_mode->rate / 1000.0f;
		info->depth = (double)(chorus_status_gs.depth + 1) / 3.2f * (double)play_mode->rate / 1000.0f;
		info->pdelay -= info->depth / 2;	/* NOMINAL_DELAY to delay */
		if (info->pdelay < 1) {info->pdelay = 1;}
		info->rpt0 = info->pdelay + info->depth + 2;	/* allowance */
		set_delay(&(info->delayL), info->rpt0);
		set_delay(&(info->delayR), info->rpt0);
		info->feedback = (double)chorus_status_gs.feedback * 0.763f / 100.0f;
		info->level = (double)chorus_status_gs.level / 127.0f * MASTER_CHORUS_LEVEL;
		info->send_reverb = (double)chorus_status_gs.send_reverb * 0.787f / 100.0f * REV_INP_LEV;
		info->send_delay = (double)chorus_status_gs.send_delay * 0.787f / 100.0f;
		info->feedbacki = TIM_FSCALE(info->feedback, 24);
		info->leveli = TIM_FSCALE(info->level, 24);
		info->send_reverbi = TIM_FSCALE(info->send_reverb, 24);
		info->send_delayi = TIM_FSCALE(info->send_delay, 24);
		info->wpt0 = info->spt0 = info->spt1 = info->hist0 = info->hist1 = 0;
		return;
	} else if(count == MAGIC_FREE_EFFECT_INFO) {
		free_delay(&(info->delayL));
		free_delay(&(info->delayR));
		return;
	}

	/* LFO */
	f0 = imuldiv24(lfobufL[imuldiv24(lfocnt, icycle)], depth);
	spt0 = wpt0 - pdelay - (f0 >> 8);	/* integral part of delay */
	f0 = 0xFF - (f0 & 0xFF);	/* (1 - frac) * 256 */
	if(spt0 < 0) {spt0 += rpt0;}
	f1 = imuldiv24(lfobufR[imuldiv24(lfocnt, icycle)], depth);
	spt1 = wpt0 - pdelay - (f1 >> 8);	/* integral part of delay */
	f1 = 0xFF - (f1 & 0xFF);	/* (1 - frac) * 256 */
	if(spt1 < 0) {spt1 += rpt0;}
	
	for(i = 0; i < count; i++) {
		v0 = bufL[spt0];
		v1 = bufR[spt1];

		/* LFO */
		if(++wpt0 == rpt0) {wpt0 = 0;}
		f0 = imuldiv24(lfobufL[imuldiv24(lfocnt, icycle)], depth);
		spt0 = wpt0 - pdelay - (f0 >> 8);	/* integral part of delay */
		f0 = 0xFF - (f0 & 0xFF);	/* (1 - frac) * 256 */
		if(spt0 < 0) {spt0 += rpt0;}
		f1 = imuldiv24(lfobufR[imuldiv24(lfocnt, icycle)], depth);
		spt1 = wpt0 - pdelay - (f1 >> 8);	/* integral part of delay */
		f1 = 0xFF - (f1 & 0xFF);	/* (1 - frac) * 256 */
		if(spt1 < 0) {spt1 += rpt0;}
		if(++lfocnt == cycle) {lfocnt = 0;}

		/* left */
		/* delay with all-pass interpolation */
		output = hist0 = v0 + imuldiv8(bufL[spt0] - hist0, f0);
		bufL[wpt0] = chorus_effect_buffer[i] + imuldiv24(output, feedbacki);
		output = imuldiv24(output, leveli);
		buf[i] += output;
		/* send to other system effects (it's peculiar to GS) */
		reverb_effect_buffer[i] += imuldiv24(output, send_reverbi);
		delay_effect_buffer[i] += imuldiv24(output, send_delayi);

		/* right */
		/* delay with all-pass interpolation */
		output = hist1 = v1 + imuldiv8(bufR[spt1] - hist1, f1);
		bufR[wpt0] = chorus_effect_buffer[++i] + imuldiv24(output, feedbacki);
		output = imuldiv24(output, leveli);
		buf[i] += output;
		/* send to other system effects (it's peculiar to GS) */
		reverb_effect_buffer[i] += imuldiv24(output, send_reverbi);
		delay_effect_buffer[i] += imuldiv24(output, send_delayi);
	}
	memset(chorus_effect_buffer, 0, sizeof(int32) * count);
	info->wpt0 = wpt0, info->spt0 = spt0, info->spt1 = spt1,
		info->hist0 = hist0, info->hist1 = hist1;
	info->lfoL.count = info->lfoR.count = lfocnt;
}

void init_ch_chorus(void)
{
	/* clear delay-line of LPF */
	init_filter_lowpass1(&(chorus_status_gs.lpf));
	do_ch_stereo_chorus(NULL, MAGIC_INIT_EFFECT_INFO, &(chorus_status_gs.info_stereo_chorus));
	memset(chorus_effect_buffer, 0, sizeof(chorus_effect_buffer));
}

#if OPT_MODE != 0	/* fixed-point implementation */
#if _MSC_VER
void set_ch_chorus(int32 *buf, int32 count, int32 level)
{
	int32 *dbuf = chorus_effect_buffer;
	if(!level) {return;}
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
	if(!level) {return;}
	level = level * 65536 / 127;

	for(i = n - 1; i >= 0; i--) {buf[i] += imuldiv16(sbuffer[i], level);}
}
#endif	/* _MSC_VER */
#else	/* floating-point implementation */
void set_ch_chorus(register int32 *sbuffer,int32 n, int32 level)
{
    register int32 i;
    register int32 count = n;
	if(!level) {return;}
    FLOAT_T send_level = (FLOAT_T)level / 127.0;

    for(i = 0; i < count; i++)
    {
		chorus_effect_buffer[i] += sbuffer[i] * send_level;
    }
}
#endif /* OPT_MODE != 0 */

void do_ch_chorus(int32 *buf, int32 count)
{
#ifdef SYS_EFFECT_PRE_LPF
	if ((opt_reverb_control == 3 || opt_reverb_control == 4
			|| (opt_reverb_control < 0 && ! (opt_reverb_control & 0x100))) && chorus_status_gs.pre_lpf)
		do_filter_lowpass1_stereo(chorus_effect_buffer, count, &(chorus_status_gs.lpf));
#endif /* SYS_EFFECT_PRE_LPF */

	do_ch_stereo_chorus(buf, count, &(chorus_status_gs.info_stereo_chorus));
}

/*                             */
/*       EQ (Equalizer)        */
/*                             */
static int32 eq_buffer[AUDIO_BUFFER_SIZE * 2];

void init_eq_gs()
{
	memset(eq_buffer, 0, sizeof(eq_buffer));
	init_filter_shelving(&(eq_status_gs.lsf));
	init_filter_shelving(&(eq_status_gs.hsf));
}

void do_ch_eq_gs(int32* buf, int32 count)
{
	register int32 i;

	do_shelving_filter_stereo(eq_buffer, count, &(eq_status_gs.lsf));
	do_shelving_filter_stereo(eq_buffer, count, &(eq_status_gs.hsf));

	for(i = 0; i < count; i++) {
		buf[i] += eq_buffer[i];
		eq_buffer[i] = 0;
	}
}

void do_ch_eq_xg(int32* buf, int32 count, struct part_eq_xg *p)
{
	if(p->bass - 0x40 != 0) {
		do_shelving_filter_stereo(buf, count, &(p->basss));
	}
	if(p->treble - 0x40 != 0) {
		do_shelving_filter_stereo(buf, count, &(p->trebles));
	}
}

void do_multi_eq_xg(int32* buf, int32 count)
{
	if(multi_eq_xg.valid1) {
		if(multi_eq_xg.shape1) {	/* peaking */
			do_peaking_filter_stereo(buf, count, &(multi_eq_xg.eq1p));
		} else {	/* shelving */
			do_shelving_filter_stereo(buf, count, &(multi_eq_xg.eq1s));
		}
	}
	if(multi_eq_xg.valid2) {
		do_peaking_filter_stereo(buf, count, &(multi_eq_xg.eq2p));
	}
	if(multi_eq_xg.valid3) {
		do_peaking_filter_stereo(buf, count, &(multi_eq_xg.eq3p));
	}
	if(multi_eq_xg.valid4) {
		do_peaking_filter_stereo(buf, count, &(multi_eq_xg.eq4p));
	}
	if(multi_eq_xg.valid5) {
		if(multi_eq_xg.shape5) {	/* peaking */
			do_peaking_filter_stereo(buf, count, &(multi_eq_xg.eq5p));
		} else {	/* shelving */
			do_shelving_filter_stereo(buf, count, &(multi_eq_xg.eq5s));
		}
	}
}

#if OPT_MODE != 0
#if _MSC_VER
void set_ch_eq_gs(int32 *buf, int32 count)
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
void set_ch_eq_gs(register int32 *buf, int32 n)
{
    register int32 i;

    for(i = n-1; i >= 0; i--)
    {
        eq_buffer[i] += buf[i];
    }
}
#endif	/* _MSC_VER */
#else
void set_ch_eq_gs(register int32 *sbuffer, int32 n)
{
    register int32  i;
    
	for(i = 0; i < n; i++)
    {
        eq_buffer[i] += sbuffer[i];
    }
}
#endif /* OPT_MODE != 0 */


/*                                  */
/*  Insertion and Variation Effect  */
/*                                  */
void do_insertion_effect_gs(int32 *buf, int32 count)
{
	do_effect_list(buf, count, insertion_effect_gs.ef);
}

void do_insertion_effect_xg(int32 *buf, int32 count, struct effect_xg_t *st)
{
	do_effect_list(buf, count, st->ef);
}

void do_variation_effect1_xg(int32 *buf, int32 count)
{
	int32 i, x;
	int32 send_reverbi = TIM_FSCALE((double)variation_effect_xg[0].send_reverb * (0.787f / 100.0f * REV_INP_LEV), 24),
		send_chorusi = TIM_FSCALE((double)variation_effect_xg[0].send_chorus * (0.787f / 100.0f), 24);
	if (variation_effect_xg[0].connection == XG_CONN_SYSTEM) {
		do_effect_list(delay_effect_buffer, count, variation_effect_xg[0].ef);
		for (i = 0; i < count; i++) {
			x = delay_effect_buffer[i];
			buf[i] += x;
			reverb_effect_buffer[i] += imuldiv24(x, send_reverbi);
			chorus_effect_buffer[i] += imuldiv24(x, send_chorusi);
		}
	}
	memset(delay_effect_buffer, 0, sizeof(int32) * count);
}

void do_ch_chorus_xg(int32 *buf, int32 count)
{
	int32 i;
	int32 send_reverbi = TIM_FSCALE((double)chorus_status_xg.send_reverb * (0.787f / 100.0f * REV_INP_LEV), 24);

	do_effect_list(chorus_effect_buffer, count, chorus_status_xg.ef);
	for (i = 0; i < count; i++) {
		buf[i] += chorus_effect_buffer[i];
		reverb_effect_buffer[i] += imuldiv24(chorus_effect_buffer[i], send_reverbi);
	}
	memset(chorus_effect_buffer, 0, sizeof(int32) * count);
}

void do_ch_reverb_xg(int32 *buf, int32 count)
{
	int32 i;

	do_effect_list(reverb_effect_buffer, count, reverb_status_xg.ef);
	for (i = 0; i < count; i++) {
		buf[i] += reverb_effect_buffer[i];
	}
	memset(reverb_effect_buffer, 0, sizeof(int32) * count);
}

void init_ch_effect_xg(void)
{
	memset(reverb_effect_buffer, 0, sizeof(reverb_effect_buffer));
	memset(chorus_effect_buffer, 0, sizeof(chorus_effect_buffer));
	memset(delay_effect_buffer, 0, sizeof(delay_effect_buffer));
}

void alloc_effect(EffectList *ef)
{
	int i;

	ef->engine = NULL;
	for(i = 0; effect_engine[i].type != -1; i++) {
		if (effect_engine[i].type == ef->type) {
			ef->engine = &(effect_engine[i]);
			break;
		}
	}

	if (ef->info != NULL) {
		free(ef->info);
		ef->info = NULL;
	}
	ef->info = safe_malloc(ef->engine->info_size);

	/* initialize */
	(*ef->engine->do_effect)(NULL, MAGIC_INIT_EFFECT_INFO, ef);

	ctl->cmsg(CMSG_INFO, VERB_NOISY, "Effect Engine: %s", ef->engine->name);
}

/*! allocate new effect item and add it into the tail of effect list.
    EffectList *efc: pointer to the top of effect list.
    int8 type: type of new effect item.
    void *info: pointer to infomation of new effect item. */
EffectList *push_effect(EffectList *efc, int type)
{
	EffectList *eft, *efn;
	if(type == EFFECT_NONE) {return NULL;}
	efn = (EffectList *)safe_malloc(sizeof(EffectList));
	efn->type = type;
	efn->next_ef = NULL;
	alloc_effect(efn);

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
	while(efc != NULL && efc->engine->do_effect != NULL)
	{
		(*efc->engine->do_effect)(buf, count, efc);
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
			(*efc->engine->do_effect)(NULL, MAGIC_FREE_EFFECT_INFO, efc);
			free(efc->info);
			efc->info = NULL;
		}
		efc->engine = NULL;
		free(efc);
		efc = NULL;
	} while ((efc = efn) != NULL);
}

/*! 2-Band EQ */
void do_eq2(int32 *buf, int32 count, EffectList *ef)
{
	InfoEQ2 *eq = (InfoEQ2 *)ef->info;
	if(count == MAGIC_INIT_EFFECT_INFO) {
		eq->lsf.q = 0;
		eq->lsf.freq = eq->low_freq;
		eq->lsf.gain = eq->low_gain;
		calc_filter_shelving_low(&(eq->lsf));
		init_filter_shelving(&(eq->lsf));
		eq->hsf.q = 0;
		eq->hsf.freq = eq->high_freq;
		eq->hsf.gain = eq->high_gain;
		calc_filter_shelving_high(&(eq->hsf));
		init_filter_shelving(&(eq->hsf));
		return;
	} else if(count == MAGIC_FREE_EFFECT_INFO) {
		return;
	}
	if(eq->low_gain != 0) {
		do_shelving_filter_stereo(buf, count, &(eq->lsf));
	}
	if(eq->high_gain != 0) {
		do_shelving_filter_stereo(buf, count, &(eq->hsf));
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

#define OD_BITS 28
#define OD_MAX_NEG (1.0 / (double)(1L << OD_BITS))
#define OVERDRIVE_DIST 4.0
#define OVERDRIVE_RES 0.1
#define OVERDRIVE_LEVEL 1.0
#define OVERDRIVE_OFFSET 0
#define DISTORTION_DIST 40.0
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
		sig = (double)high * OD_MAX_NEG;
		ax1 = lastin;
		ay11 = ay1;
		ay31 = ay2;
		lastin = sig - tanh(kres * aout);
		ay1 = kp1h * (lastin + ax1) - kp * ay1;
		ay2 = kp1h * (ay1 + ay11) - kp * ay2;
		aout = kp1h * (ay2 + ay31) - kp * aout;
		sig = tanh(aout * value);
		high = TIM_FSCALE(sig, OD_BITS);
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
		sig = (double)high * OD_MAX_NEG;
		ax1 = lastin;
		ay11 = ay1;
		ay31 = ay2;
		lastin = sig - tanh(kres * aout);
		ay1 = kp1h * (lastin + ax1) - kp * ay1;
		ay2 = kp1h * (ay1 + ay11) - kp * ay2;
		aout = kp1h * (ay2 + ay31) - kp * aout;
		sig = tanh(aout * value);
		high = TIM_FSCALE(sig, OD_BITS);
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
		sig = (double)high * OD_MAX_NEG;
		ax1l = lastinl;
		ay11l = ay1l;
		ay31l = ay2l;
		lastinl = sig - tanh(kresl * aoutl);
		ay1l = kp1h * (lastinl + ax1l) - kp * ay1l;
		ay2l = kp1h * (ay1l + ay11l) - kp * ay2l;
		aoutl = kp1h * (ay2l + ay31l) - kp * aoutl;
		sig = tanh(aoutl * valuel);
		high = TIM_FSCALE(sig, OD_BITS);
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
		sig = (double)high * OD_MAX_NEG;
		ax1r = lastinr;
		ay11r = ay1r;
		ay31r = ay2r;
		lastinr = sig - tanh(kresr * aoutr);
		ay1r = kp1h * (lastinr + ax1r) - kp * ay1r;
		ay2r = kp1h * (ay1r + ay11r) - kp * ay2r;
		aoutr = kp1h * (ay2r + ay31r) - kp * aoutr;
		sig = tanh(aoutr * valuer);
		high = TIM_FSCALE(sig, OD_BITS);
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

#define HEXA_CHORUS_WET_LEVEL 0.2
#define HEXA_CHORUS_DEPTH_DEV (1.0 / (20.0 + 1.0))
#define HEXA_CHORUS_DELAY_DEV (1.0 / (20.0 * 3.0))

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
	int32 i, lfo_val,
		v0, v1, v2, v3, v4, v5, f0, f1, f2, f3, f4, f5;

	if(count == MAGIC_INIT_EFFECT_INFO) {
		set_delay(buf0, (int32)(9600.0f * play_mode->rate / 44100.0f));
		init_lfo(lfo, lfo->freq, LFO_SINE, 0);
		info->dryi = TIM_FSCALE(info->level * info->dry, 24);
		info->weti = TIM_FSCALE(info->level * info->wet * HEXA_CHORUS_WET_LEVEL, 24);
		v0 = info->depth * ((double)info->depth_dev * HEXA_CHORUS_DEPTH_DEV);
		info->depth0 = info->depth - v0;
		info->depth1 = info->depth;
		info->depth2 = info->depth + v0;
		info->depth3 = info->depth + v0;
		info->depth4 = info->depth;
		info->depth5 = info->depth - v0;
		v0 = info->pdelay * ((double)info->pdelay_dev * HEXA_CHORUS_DELAY_DEV);
		info->pdelay0 = info->pdelay + v0;
		info->pdelay1 = info->pdelay + v0 * 2;
		info->pdelay2 = info->pdelay + v0 * 3;
		info->pdelay3 = info->pdelay + v0 * 3;
		info->pdelay4 = info->pdelay + v0 * 2;
		info->pdelay5 = info->pdelay + v0;
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
		hist0 = v0 + imuldiv8(ebuf[spt0] - hist0, f0);
		hist1 = v1 + imuldiv8(ebuf[spt1] - hist1, f1);
		hist2 = v2 + imuldiv8(ebuf[spt2] - hist2, f2);
		hist3 = v3 + imuldiv8(ebuf[spt3] - hist3, f3);
		hist4 = v4 + imuldiv8(ebuf[spt4] - hist4, f4);
		hist5 = v5 + imuldiv8(ebuf[spt5] - hist5, f5);
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

static void free_effect_xg(struct effect_xg_t *st)
{
	free_effect_list(st->ef);
	st->ef = NULL;
}

void free_effect_buffers(void)
{
	int i;
	/* free GM/GS/GM2 effects */
	do_ch_standard_reverb(NULL, MAGIC_FREE_EFFECT_INFO, &(reverb_status_gs.info_standard_reverb));
	do_ch_freeverb(NULL, MAGIC_FREE_EFFECT_INFO, &(reverb_status_gs.info_freeverb));
	do_ch_plate_reverb(NULL, MAGIC_FREE_EFFECT_INFO, &(reverb_status_gs.info_plate_reverb));
	do_ch_reverb_normal_delay(NULL, MAGIC_FREE_EFFECT_INFO, &(reverb_status_gs.info_reverb_delay));
	do_ch_stereo_chorus(NULL, MAGIC_FREE_EFFECT_INFO, &(chorus_status_gs.info_stereo_chorus));
	do_ch_3tap_delay(NULL, MAGIC_FREE_EFFECT_INFO, &(delay_status_gs.info_delay));
	free_effect_list(insertion_effect_gs.ef);
	insertion_effect_gs.ef = NULL;
	/* free XG effects */
	free_effect_xg(&reverb_status_xg);
	free_effect_xg(&chorus_status_xg);
	for (i = 0; i < XG_VARIATION_EFFECT_NUM; i++) {
		free_effect_xg(&variation_effect_xg[i]);
	}
	for (i = 0; i < XG_INSERTION_EFFECT_NUM; i++) {
		free_effect_xg(&insertion_effect_xg[i]);
	}
}

/*! convert GS insertion effect parameters for internal 2-Band EQ. */
static void conv_gs_eq2(struct insertion_effect_gs_t *ieffect, EffectList *ef)
{
	InfoEQ2 *eq = (InfoEQ2 *)ef->info; 

	eq->high_freq = 4000;
	eq->high_gain = ieffect->parameter[16] - 0x40;
	eq->low_freq = 400;
	eq->low_gain = ieffect->parameter[17] - 0x40;
}

/*! convert GS insertion effect parameters for Overdrive1 / Distortion 1. */
static void conv_gs_overdrive1(struct insertion_effect_gs_t *ieffect, EffectList *ef)
{
	InfoOverdrive1 *od = (InfoOverdrive1 *)ef->info;
	
	od->level = (double)ieffect->parameter[19] / 127.0;
	od->drive = ieffect->parameter[0];
	od->pan = ieffect->parameter[18];
}

/*! convert GS insertion effect parameters for OD1 / OD2. */
static void conv_gs_dual_od(struct insertion_effect_gs_t *ieffect, EffectList *ef)
{
	InfoOD1OD2 *od = (InfoOD1OD2 *)ef->info;

	od->level = (double)ieffect->parameter[19] / 127.0;
	od->levell = (double)ieffect->parameter[16] / 127.0;
	od->levelr = (double)ieffect->parameter[18] / 127.0;
	od->drivel = ieffect->parameter[1];
	od->driver = ieffect->parameter[6];
	od->panl = ieffect->parameter[15];
	od->panr = ieffect->parameter[17];
	od->typel = ieffect->parameter[0];
	od->typer = ieffect->parameter[5];
}

/*! convert GS insertion effect parameters for Hexa-Chorus. */
static void conv_gs_hexa_chorus(struct insertion_effect_gs_t *ieffect, EffectList *ef)
{
	InfoHexaChorus *info = (InfoHexaChorus *)ef->info;
	
	info->level = (double)ieffect->parameter[19] / 127.0f;
	info->pdelay = pre_delay_time_table[ieffect->parameter[0]] * (double)play_mode->rate / 1000.0f;
	info->depth = (double)(ieffect->parameter[2] + 1) / 3.2f  * (double)play_mode->rate / 1000.0f;
	info->pdelay -= info->depth / 2;
	if(info->pdelay <= 1) {info->pdelay = 1;}
	info->lfo0.freq = rate1_table[ieffect->parameter[1]];
	info->pdelay_dev = ieffect->parameter[3];
	info->depth_dev = ieffect->parameter[4] - 64;
	info->pan_dev = ieffect->parameter[5];
	info->dry = (double)(127 - ieffect->parameter[19]) / 63.0;
	if(info->dry > 1.0) {info->dry = 1.0;}
	info->wet = (double)ieffect->parameter[19] / 64.0;
	if(info->wet > 1.0) {info->wet = 1.0;}
}

static double calc_dry_xg(int val, struct effect_xg_t *st)
{
	if (st->connection) {return 0.0f;}
	else {return ((double)(127 - val) / 127.0f);}
}

static double calc_wet_xg(int val, struct effect_xg_t *st)
{
	switch(st->connection) {
	case XG_CONN_SYSTEM:
		return ((double)st->ret / 127.0f);
	case XG_CONN_SYSTEM_CHORUS:
		return ((double)st->ret / 127.0f * MASTER_CHORUS_LEVEL);
	case XG_CONN_SYSTEM_REVERB:
		return ((double)st->ret / 127.0f);
	default:
		return ((double)val / 127.0f); 
	}
}

/*! 3-Band EQ */
static void do_eq3(int32 *buf, int32 count, EffectList *ef)
{
	InfoEQ3 *eq = (InfoEQ3 *)ef->info;
	if (count == MAGIC_INIT_EFFECT_INFO) {
		eq->lsf.q = 0;
		eq->lsf.freq = eq->low_freq;
		eq->lsf.gain = eq->low_gain;
		calc_filter_shelving_low(&(eq->lsf));
		init_filter_shelving(&(eq->lsf));
		eq->hsf.q = 0;
		eq->hsf.freq = eq->high_freq;
		eq->hsf.gain = eq->high_gain;
		calc_filter_shelving_high(&(eq->hsf));
		init_filter_shelving(&(eq->hsf));
		eq->peak.q = 1.0f / eq->mid_width;
		eq->peak.freq = eq->mid_freq;
		eq->peak.gain = eq->mid_gain;
		calc_filter_peaking(&(eq->peak));
		init_filter_peaking(&(eq->peak));
		return;
	} else if(count == MAGIC_FREE_EFFECT_INFO) {
		return;
	}
	if(eq->low_gain != 0) {
		do_shelving_filter_stereo(buf, count, &(eq->lsf));
	}
	if(eq->high_gain != 0) {
		do_shelving_filter_stereo(buf, count, &(eq->hsf));
	}
	if(eq->mid_gain != 0) {
		do_peaking_filter_stereo(buf, count, &(eq->peak));
	}
}

static void conv_xg_chorus_eq3(struct effect_xg_t *st, EffectList *ef)
{
	InfoEQ3 *info = (InfoEQ3 *)ef->info;
	int val;

	val = st->param_lsb[5];
	val = (val > 40) ? 40 : (val < 4) ? 4 : val;
	info->low_freq = eq_freq_table_xg[val];
	val = st->param_lsb[6] - 64;
	info->low_gain = (val > 12) ? 12 : (val < -12) ? -12 : val;
	val = st->param_lsb[7];
	val = (val > 58) ? 58 : (val < 28) ? 28 : val;
	info->high_freq = eq_freq_table_xg[val];
	val = st->param_lsb[8] - 64;
	info->high_gain = (val > 12) ? 12 : (val < -12) ? -12 : val;
	val = st->param_lsb[10];
	val = (val > 54) ? 54 : (val < 14) ? 14 : val;
	info->mid_freq = eq_freq_table_xg[val];
	val = st->param_lsb[11] - 64;
	info->mid_gain = (val > 12) ? 12 : (val < -12) ? -12 : val;
	val = st->param_lsb[12];
	val = (val > 120) ? 120 : (val < 10) ? 10 : val;
	info->mid_width = (double)val / 10.0f;
}

static void conv_xg_chorus(struct effect_xg_t *st, EffectList *ef)
{
	InfoChorus *info = (InfoChorus *)ef->info;

	info->rate = lfo_freq_table_xg[st->param_lsb[0]];
	info->depth_ms = (double)(st->param_lsb[1] + 1) / 3.2f;
	info->feedback = (double)(st->param_lsb[2] - 64) * (0.763f * 2.0f / 100.0f);
	info->pdelay_ms = mod_delay_offset_table_xg[st->param_lsb[3]];
	info->dry = calc_dry_xg(st->param_lsb[9], st);
	info->wet = calc_wet_xg(st->param_lsb[9], st);
	info->phase_diff = 0.0f;
}

static void conv_xg_flanger(struct effect_xg_t *st, EffectList *ef)
{
	InfoChorus *info = (InfoChorus *)ef->info;
	int val;

	info->rate = lfo_freq_table_xg[st->param_lsb[0]];
	info->depth_ms = (double)(st->param_lsb[1] + 1) / 3.2f;
	info->feedback = (double)(st->param_lsb[2] - 64) * (0.763f * 2.0f / 100.0f);
	info->pdelay_ms = mod_delay_offset_table_xg[st->param_lsb[2]];
	info->dry = calc_dry_xg(st->param_lsb[9], st);
	info->wet = calc_wet_xg(st->param_lsb[9], st);
	val = st->param_lsb[13];
	val = (val > 124) ? 124 : (val < 4) ? 4 : val;
	info->phase_diff = (double)(val - 64) * 3.0f;
}

static void conv_xg_symphonic(struct effect_xg_t *st, EffectList *ef)
{
	InfoChorus *info = (InfoChorus *)ef->info;

	info->rate = lfo_freq_table_xg[st->param_lsb[0]];
	info->depth_ms = (double)(st->param_lsb[1] + 1) / 3.2f;
	info->feedback = 0.0f;
	info->pdelay_ms = mod_delay_offset_table_xg[st->param_lsb[3]];
	info->dry = calc_dry_xg(st->param_lsb[9], st);
	info->wet = calc_wet_xg(st->param_lsb[9], st);
	info->phase_diff = 0.0f;
}

static void do_chorus(int32 *buf, int32 count, EffectList *ef)
{
	InfoChorus *info = (InfoChorus *)ef->info;
	int32 i, output, f0, f1, v0, v1;
	int32 *bufL = info->delayL.buf, *bufR = info->delayR.buf,
		*lfobufL = info->lfoL.buf, *lfobufR = info->lfoR.buf,
		icycle = info->lfoL.icycle, cycle = info->lfoL.cycle,
		dryi = info->dryi, weti = info->weti, feedbacki = info->feedbacki,
		depth = info->depth, pdelay = info->pdelay, rpt0 = info->rpt0;
	int32 wpt0 = info->wpt0, spt0 = info->spt0, spt1 = info->spt1,
		hist0 = info->hist0, hist1 = info->hist1, lfocnt = info->lfoL.count;

	if (count == MAGIC_INIT_EFFECT_INFO) {
		init_lfo(&(info->lfoL), info->rate, LFO_SINE, 0);
		init_lfo(&(info->lfoR), info->rate, LFO_SINE, info->phase_diff);
		info->pdelay = info->pdelay_ms * (double)play_mode->rate / 1000.0f;
		info->depth = info->depth_ms * (double)play_mode->rate / 1000.0f;
		info->pdelay -= info->depth / 2;	/* NOMINAL_DELAY to delay */
		if (info->pdelay < 1) {info->pdelay = 1;}
		info->rpt0 = info->pdelay + info->depth + 2;	/* allowance */
		set_delay(&(info->delayL), info->rpt0);
		set_delay(&(info->delayR), info->rpt0);
		info->feedbacki = TIM_FSCALE(info->feedback, 24);
		info->dryi = TIM_FSCALE(info->dry, 24);
		info->weti = TIM_FSCALE(info->wet, 24);
		info->wpt0 = info->spt0 = info->spt1 = info->hist0 = info->hist1 = 0;
		return;
	} else if (count == MAGIC_FREE_EFFECT_INFO) {
		free_delay(&(info->delayL));
		free_delay(&(info->delayR));
		return;
	}

	/* LFO */
	f0 = imuldiv24(lfobufL[imuldiv24(lfocnt, icycle)], depth);
	spt0 = wpt0 - pdelay - (f0 >> 8);	/* integral part of delay */
	f0 = 0xFF - (f0 & 0xFF);	/* (1 - frac) * 256 */
	if (spt0 < 0) {spt0 += rpt0;}
	f1 = imuldiv24(lfobufR[imuldiv24(lfocnt, icycle)], depth);
	spt1 = wpt0 - pdelay - (f1 >> 8);	/* integral part of delay */
	f1 = 0xFF - (f1 & 0xFF);	/* (1 - frac) * 256 */
	if (spt1 < 0) {spt1 += rpt0;}
	
	for (i = 0; i < count; i++) {
		v0 = bufL[spt0];
		v1 = bufR[spt1];

		/* LFO */
		if(++wpt0 == rpt0) {wpt0 = 0;}
		f0 = imuldiv24(lfobufL[imuldiv24(lfocnt, icycle)], depth);
		spt0 = wpt0 - pdelay - (f0 >> 8);	/* integral part of delay */
		f0 = 0xFF - (f0 & 0xFF);	/* (1 - frac) * 256 */
		if(spt0 < 0) {spt0 += rpt0;}
		f1 = imuldiv24(lfobufR[imuldiv24(lfocnt, icycle)], depth);
		spt1 = wpt0 - pdelay - (f1 >> 8);	/* integral part of delay */
		f1 = 0xFF - (f1 & 0xFF);	/* (1 - frac) * 256 */
		if(spt1 < 0) {spt1 += rpt0;}
		if(++lfocnt == cycle) {lfocnt = 0;}

		/* left */
		/* delay with all-pass interpolation */
		output = hist0 = v0 + imuldiv8(bufL[spt0] - hist0, f0);
		bufL[wpt0] = buf[i] + imuldiv24(output, feedbacki);
		buf[i] = imuldiv24(buf[i], dryi) + imuldiv24(output, weti);

		/* right */
		/* delay with all-pass interpolation */
		output = hist1 = v1 + imuldiv8(bufR[spt1] - hist1, f1);
		bufR[wpt0] = buf[++i] + imuldiv24(output, feedbacki);
		buf[i] = imuldiv24(buf[i], dryi) + imuldiv24(output, weti);
	}
	info->wpt0 = wpt0, info->spt0 = spt0, info->spt1 = spt1,
		info->hist0 = hist0, info->hist1 = hist1;
	info->lfoL.count = info->lfoR.count = lfocnt;
}

struct _EffectEngine effect_engine[] = {
	EFFECT_NONE, "None", NULL, NULL, NULL, 0,
	EFFECT_EQ2, "2-Band EQ", do_eq2, conv_gs_eq2, NULL, sizeof(InfoEQ2),
	EFFECT_OVERDRIVE1, "Overdrive", do_overdrive1, conv_gs_overdrive1, NULL, sizeof(InfoOverdrive1),
	EFFECT_DISTORTION1, "Distortion", do_distortion1, conv_gs_overdrive1, NULL, sizeof(InfoOverdrive1),
	EFFECT_OD1OD2, "OD1/OD2", do_dual_od, conv_gs_dual_od, NULL, sizeof(InfoOD1OD2),
	EFFECT_HEXA_CHORUS, "Hexa-Chorus", do_hexa_chorus, conv_gs_hexa_chorus, NULL, sizeof(InfoHexaChorus),
	EFFECT_CHORUS, "Chorus", do_chorus, NULL, conv_xg_chorus, sizeof(InfoChorus),
	EFFECT_FLANGER, "Flanger", do_chorus, NULL, conv_xg_flanger, sizeof(InfoChorus),
	EFFECT_SYMPHONIC, "Symphonic", do_chorus, NULL, conv_xg_symphonic, sizeof(InfoChorus),
	EFFECT_CHORUS_EQ3, "3-Band EQ (XG Chorus built-in)", do_eq3, NULL, conv_xg_chorus_eq3, sizeof(InfoEQ3),
	-1, "EOF", NULL, NULL, NULL, 0, 
};

struct effect_parameter_xg_t effect_parameter_xg[] = {
	0, 0, "NO EFFECT", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0x41, 0, "CHORUS 1", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	6, 54, 77, 106, 0, 28, 64, 46, 64, 64, 46, 64, 10, 0, 0, 0,
	-1, -1, "EOF", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
