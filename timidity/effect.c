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

    effect.c - To apply sound effects.
	      Mainly written by Masanao Izumo <mo@goice.co.jp>

 * Interfaces:
 * void init_effect(void);
 * do_effect(int32* buf, int32 count);
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#define USE_MT_RAND
#ifdef USE_MT_RAND
#include "mt19937ar.h"
#endif
#define RAND_MAX 0xFFFFFFFF

#include "timidity.h"
#include "instrum.h"
#include "playmidi.h"
#include "output.h"
#include "reverb.h"


static void effect_left_right_delay(int32* buff, int32 count);
static void init_ns_tap(void);
static void ns_shaping8(int32* buf, int32 count);
static void ns_shaping16(int32* buf, int32 count);

void do_effect(int32* buf, int32 count)
{
	/* reverb in mono */
    if(opt_reverb_control && (play_mode->encoding & PE_MONO)) {
		do_mono_reverb(buf, count);
	}

	/* for static reverb / chorus level */
	if(opt_reverb_control < 0 || opt_chorus_control < 0) {
		set_dry_signal(buf,2 * count);
		if(opt_chorus_control < 0) {
			set_ch_chorus(buf, 2 * count, -opt_chorus_control);
		}
		if(opt_reverb_control < 0) {
			set_ch_reverb(buf, 2 * count, -opt_reverb_control);
		}

		mix_dry_signal(buf,2 * count);
		if(opt_chorus_control < 0) {do_ch_chorus(buf, 2 * count);}
		if(opt_reverb_control < 0) {do_ch_reverb(buf, 2 * count);}
    }

	/* L/R Delay */
    effect_left_right_delay(buf, count);

    /* Noise shaping filter must apply at last */
    if(!(play_mode->encoding & (PE_16BIT|PE_ULAW|PE_ALAW)))
	ns_shaping8(buf, count);
	else if(play_mode->encoding & PE_16BIT)
	ns_shaping16(buf,count);
}


void init_effect(void)
{
    effect_left_right_delay(NULL, 0);
	init_ns_tap();
    init_reverb(play_mode->rate);
	init_ch_delay();
	init_ch_chorus();
	init_eq();
	init_insertion_effect();
}


/*
 * Left & Right Delay Effect
 */
static void effect_left_right_delay(int32* buff, int32 count)
{
    static int32 prev[AUDIO_BUFFER_SIZE * 2];
    int32 save[AUDIO_BUFFER_SIZE * 2];
    int32 pi, i, backoff;
    int b;

    if(buff == NULL)
    {
	memset(prev, 0, sizeof(prev));
	return;
    }

    if(play_mode->encoding & PE_MONO)
	return;
    if(effect_lr_mode == 0 || effect_lr_mode == 1 || effect_lr_mode == 2)
	b = effect_lr_mode;
    else
	return;

    count *= 2;
    backoff = 2 * (int)(play_mode->rate * effect_lr_delay_msec / 1000.0);
    if(backoff == 0)
	return;
    if(backoff > count)
	backoff = count;

    if(count < audio_buffer_size * 2)
    {
	memset(buff + count, 0, 4 * (audio_buffer_size * 2 - count));
	count = audio_buffer_size * 2;
    }

    memcpy(save, buff, 4 * count);
    pi = count - backoff;

    if(b == 2)
    {
	static int turn_counter = 0, tc;
	static int status;
	static double rate0, rate1, dr;
	int32 *p;

#define SIDE_CONTI_SEC 10
#define CHANGE_SEC     2.0

	if(turn_counter == 0)
	{
	    turn_counter = SIDE_CONTI_SEC * play_mode->rate;

	    /*  status: 0 -> 2 -> 3 -> 1 -> 4 -> 5 -> 0 -> ...
	     *  status left   right
	     *  0      -      +		(right)
	     *  1      +      -		(left)
	     *  2     -> +    +		(right -> center)
	     *  3      +     -> -	(center -> left)
	     *  4     -> -    -		(left -> center)
	     *  5      -     -> +	(center -> right)
	     */
	    status = 0;
	    tc = 0;
	}
	p = prev;
	for(i = 0; i < count; i += 2, pi += 2)
	{
	    if(i < backoff)
		p = prev;
	    else if(p == prev)
	    {
		pi = 0;
		p = save;
	    }

	    if(status < 2)
		buff[i+status] = p[pi+status];
	    else if(status < 4)
	    {
		int32 v, j;
		j = (status & 1);

		v = (int32)(rate0 * buff[i+j] + rate1 * p[pi+j]);
		buff[i+j] = v;
		rate0 += dr; rate1 -= dr;
	    }
	    else
	    {
		int32 v, j, k;
		j = (status & 1);
		k = !j;

		v = (int32)(rate0 * buff[i+j] + rate1 * p[pi+j]);
		buff[i+j] = v;
		buff[i+k] = p[pi+k];
		rate0 += dr; rate1 -= dr;
	    }

	    tc++;
	    if(tc == turn_counter)
	    {
		tc = 0;

		switch(status)
		{
		  case 0:
		    status = 2;
		    turn_counter = (CHANGE_SEC/2.0) * play_mode->rate;
		    rate0 = 0.0;
		    rate1 = 1.0;
		    dr = 1.0 / turn_counter;
		    break;

		  case 2:
		    status = 3;
		    rate0 = 1.0;
		    rate1 = 0.0;
		    dr = -1.0 / turn_counter;
		    break;

		  case 3:
		    status = 1;
		    turn_counter = SIDE_CONTI_SEC * play_mode->rate;
		    break;

		  case 1:
		    status = 4;
		    turn_counter = (CHANGE_SEC/2.0) * play_mode->rate;
		    rate0 = 1.0;
		    rate1 = 0.0;
		    dr = -1.0 / turn_counter;
		    break;

		  case 4:
		    status = 5;
		    turn_counter = (CHANGE_SEC/2.0) * play_mode->rate;
		    rate0 = 0.0;
		    rate1 = 1.0;
		    dr = 1.0 / turn_counter;
		    break;

		  case 5:
		    status = 0;
		    turn_counter = SIDE_CONTI_SEC * play_mode->rate;
		    break;
		}
	    }
	}
    }
    else
    {
	for(i = 0; i < backoff; i += 2, pi += 2)
	    buff[b+i] = prev[b+pi];
	for(pi = 0; i < count; i += 2, pi += 2)
	    buff[b+i] = save[b+pi];
    }

    memcpy(prev + count - backoff, save + count - backoff, 4 * backoff);
}

static int32 ns9_order;
static int32 ns9_histposl, ns9_histposr;
static int32 ns9_ehl[18];
static int32 ns9_ehr[18];
static uint32 ns9_r1l, ns9_r2l, ns9_r1r, ns9_r2r;
static double ns9_d = 1.0f / (double)(1U<<15) / RAND_MAX;
static float ns9_coef[9] = {2.412f, -3.370f, 3.937f, -4.174f, 3.353f, -2.205f, 1.281f, -0.569f, 0.0847f};
static int32 ns9_c[9];

static void init_ns_tap16(void)
{
	int i;
#ifdef USE_MT_RAND
	unsigned long init[4]={0x123, 0x234, 0x345, 0x456}, length=4;
    init_by_array(init, length);
#endif
	ns9_order = 9;
	for(i=0;i<ns9_order;i++) {
		ns9_c[i] = ns9_coef[i] * 0x10000;
	}
	memset(ns9_ehl, 0, sizeof(ns9_ehl));
	memset(ns9_ehr, 0, sizeof(ns9_ehr));
	ns9_histposl = ns9_histposr = 8;
	ns9_r1l = ns9_r2l = ns9_r1r = ns9_r2r = 0;
}

static inline int32 my_mod(int32 x, int32 n)
{
	if(x > n) x-=n;
	return x;
}

#ifdef USE_MT_RAND
#define frand() genrand_int32()
#else
static inline unsigned long frand()
{
	static unsigned long a = 0xDEADBEEF;

	a = a * 140359821 + 1;
	return a;
}
#endif

#define NS_AMP_MAX (int32)(0x0FFFFFFF)
#define NS_AMP_MIN (int32)(0x8FFFFFFF)

#if OPT_MODE != 0
static void ns_shaping16_9(int32* lp, int32 c)
{
	int32 i, l, sample, output;

	for(i=0;i<c;i++)
	{
		/* left channel */
		ns9_r2l = ns9_r1l;
		ns9_r1l = frand();
		if(lp[i] > NS_AMP_MAX) lp[i] = NS_AMP_MAX;
		else if(lp[i] < NS_AMP_MIN) lp[i] = NS_AMP_MIN;
		sample = lp[i] - imuldiv16(ns9_c[8],ns9_ehl[ns9_histposl+8])
			- imuldiv16(ns9_c[7],ns9_ehl[ns9_histposl+7]) - imuldiv16(ns9_c[6],ns9_ehl[ns9_histposl+6])
			- imuldiv16(ns9_c[5],ns9_ehl[ns9_histposl+5]) - imuldiv16(ns9_c[4],ns9_ehl[ns9_histposl+4])
			- imuldiv16(ns9_c[3],ns9_ehl[ns9_histposl+3]) - imuldiv16(ns9_c[2],ns9_ehl[ns9_histposl+1])
			- imuldiv16(ns9_c[1],ns9_ehl[ns9_histposl+1]) - imuldiv16(ns9_c[0],ns9_ehl[ns9_histposl]);
		l = sample >> (32-16-GUARD_BITS);
		output = l * (1U << (32-16-GUARD_BITS)) + ns9_d * (ns9_r1l - ns9_r2l);
		ns9_histposl = my_mod((ns9_histposl+8), ns9_order);
		ns9_ehl[ns9_histposl+9] = ns9_ehl[ns9_histposl] = output - sample;
		lp[i] = output;

		/* right channel */
		++i;
		ns9_r2r = ns9_r1r;
		ns9_r1r = frand();
		if(lp[i] > NS_AMP_MAX) lp[i] = NS_AMP_MAX;
		else if(lp[i] < NS_AMP_MIN) lp[i] = NS_AMP_MIN;
		sample = lp[i] - imuldiv16(ns9_c[8],ns9_ehr[ns9_histposr+8])
			- imuldiv16(ns9_c[7],ns9_ehr[ns9_histposr+7]) - imuldiv16(ns9_c[6],ns9_ehr[ns9_histposr+6])
			- imuldiv16(ns9_c[5],ns9_ehr[ns9_histposr+5]) - imuldiv16(ns9_c[4],ns9_ehr[ns9_histposr+4])
			- imuldiv16(ns9_c[3],ns9_ehr[ns9_histposr+3]) -	imuldiv16(ns9_c[2],ns9_ehr[ns9_histposr+1])
			- imuldiv16(ns9_c[1],ns9_ehr[ns9_histposr+1]) - imuldiv16(ns9_c[0],ns9_ehr[ns9_histposr]);
		l = sample >> (32-16-GUARD_BITS);
		output = l * (1U << (32-16-GUARD_BITS)) + ns9_d * (ns9_r1r - ns9_r2r);
		ns9_histposr = my_mod((ns9_histposr+8), ns9_order);
		ns9_ehr[ns9_histposr+9] = ns9_ehr[ns9_histposr] = output - sample;
		lp[i] = output;
	}
}
#else
static void ns_shaping16_9(int32* lp, int32 c)
{
	int32 i, l, sample, output;

	for(i=0;i<c;i++)
	{
		/* left channel */
		ns9_r2l = ns9_r1l;
		ns9_r1l = frand();
		if(lp[i] > NS_AMP_MAX) lp[i] = NS_AMP_MAX;
		else if(lp[i] < NS_AMP_MIN) lp[i] = NS_AMP_MIN;
		sample = lp[i] - ns9_coef[8]*ns9_ehl[ns9_histposl+8] - ns9_coef[7]*ns9_ehl[ns9_histposl+7] - ns9_coef[6]*ns9_ehl[ns9_histposl+6] -
				ns9_coef[5]*ns9_ehl[ns9_histposl+5] - ns9_coef[4]*ns9_ehl[ns9_histposl+4] - ns9_coef[3]*ns9_ehl[ns9_histposl+3] -
				ns9_coef[2]*ns9_ehl[ns9_histposl+1] - ns9_coef[1]*ns9_ehl[ns9_histposl+1] - ns9_coef[0]*ns9_ehl[ns9_histposl];
		l = sample >> (32-16-GUARD_BITS);
		output = l * (1U << (32-16-GUARD_BITS)) + ns9_d * (ns9_r1 - ns9_r2);
		ns9_histposl = my_mod((ns9_histposl+8), ns9_order);
		ns9_ehl[ns9_histposl+9] = ns9_ehl[ns9_histposl] = output - sample;
		lp[i] = output;

		/* right channel */
		++i;
		ns9_r2r = ns9_r1r;
		ns9_r1r = frand();
		if(lp[i] > NS_AMP_MAX) lp[i] = NS_AMP_MAX;
		else if(lp[i] < NS_AMP_MIN) lp[i] = NS_AMP_MIN;
		sample = lp[i] - ns9_coef[8]*ns9_ehr[ns9_histposr+8] - ns9_coef[7]*ns9_ehr[ns9_histposr+7] - ns9_coef[6]*ns9_ehr[ns9_histposr+6] -
				ns9_coef[5]*ns9_ehr[ns9_histposr+5] - ns9_coef[4]*ns9_ehr[ns9_histposr+4] - ns9_coef[3]*ns9_ehr[ns9_histposr+3] -
				ns9_coef[2]*ns9_ehr[ns9_histposr+1] - ns9_coef[1]*ns9_ehr[ns9_histposr+1] - ns9_coef[0]*ns9_ehr[ns9_histposr];
		l = sample >> (32-16-GUARD_BITS);
		output = l * (1U << (32-16-GUARD_BITS)) + ns9_d * (ns9_r1 - ns9_r2);
		ns9_histposr = my_mod((ns9_histposr+8), ns9_order);
		ns9_ehr[ns9_histposr+9] = ns9_ehr[ns9_histposr] = output - sample;
		lp[i] = output;
	}
}
#endif

/* Noise Shaping filter from
 * Kunihiko IMAI <imai@leo.ec.t.kanazawa-u.ac.jp>
 * (Modified by Masanao Izumo <mo@goice.co.jp>)
 */
static int32  ns_z0[4];
static int32  ns_z1[4];
int noise_sharp_type = 0;
static void init_ns_tap(void)
{
    memset(ns_z0, 0, sizeof(ns_z0));
    memset(ns_z1, 0, sizeof(ns_z0));
	if(play_mode->encoding & PE_16BIT) {init_ns_tap16();}
}

static void ns_shaping8(int32* lp, int32 c)
{
    int32 l, i, ll;
    int32 ns_tap_0, ns_tap_1, ns_tap_2, ns_tap_3;

    switch(noise_sharp_type)
    {
      default:
	return;
      case 1:
	ns_tap_0 = 1;
	ns_tap_1 = 0;
	ns_tap_2 = 0;
	ns_tap_3 = 0;
	break;
      case 2:
	ns_tap_0 = -2;
	ns_tap_1 = 1;
	ns_tap_2 = 0;
	ns_tap_3 = 0;
	break;
      case 3:
	ns_tap_0 = 3;
	ns_tap_1 = -3;
	ns_tap_2 = 1;
	ns_tap_3 = 0;
	break;
      case 4:
	ns_tap_0 = -4;
	ns_tap_1 = 6;
	ns_tap_2 = -4;
	ns_tap_3 = 1;
	break;
    }

    for(i = 0; i < c; i++)
    {
	/* applied noise-shaping filter */
	ll = lp[i] + ns_tap_0*ns_z0[0] +
		     ns_tap_1*ns_z0[1] +
		     ns_tap_2*ns_z0[2] +
		     ns_tap_3*ns_z0[3];
	l = ll>>(32-8-GUARD_BITS);

	if (l>127) l=127;
	else if (l<-128) l=-128;
	lp[i] = l<<(32-8-GUARD_BITS);

	ns_z0[3] = ns_z0[2]; ns_z0[2] = ns_z0[1]; ns_z0[1] = ns_z0[0];
	ns_z0[0] = ll - l*(1U<<(32-8-GUARD_BITS));

	if ( play_mode->encoding & PE_MONO ) continue;

	++i;
	ll = lp[i] + ns_tap_0*ns_z1[0] +
		     ns_tap_1*ns_z1[1] +
		     ns_tap_2*ns_z1[2] +
		     ns_tap_3*ns_z1[3];
	l=ll>>(32-8-GUARD_BITS);
	if (l>127) l=127;
	else if (l<-128) l=-128;
	lp[i] = l<<(32-8-GUARD_BITS);
	ns_z1[3] = ns_z1[2]; ns_z1[2] = ns_z1[1]; ns_z1[1] = ns_z1[0];
	ns_z1[0] = ll - l*(1U<<(32-8-GUARD_BITS));
    }
}

static void ns_shaping16_trad(int32* lp, int32 c)
{
    int32 l, i, ll;
    int32 ns_tap_0, ns_tap_1, ns_tap_2, ns_tap_3;

	ns_tap_0 = -4;
	ns_tap_1 = 6;
	ns_tap_2 = -4;
	ns_tap_3 = 1;

    for(i = 0; i < c; i++)
    {
	/* applied noise-shaping filter */
	if(lp[i] > NS_AMP_MAX) lp[i] = NS_AMP_MAX;
	else if(lp[i] < NS_AMP_MIN) lp[i] = NS_AMP_MIN;
	ll = lp[i] + ns_tap_0*ns_z0[0] +
		     ns_tap_1*ns_z0[1] +
		     ns_tap_2*ns_z0[2] +
		     ns_tap_3*ns_z0[3];
	l = ll>>(32-16-GUARD_BITS);
	lp[i] = l<<(32-16-GUARD_BITS);
	ns_z0[3] = ns_z0[2]; ns_z0[2] = ns_z0[1]; ns_z0[1] = ns_z0[0];
	ns_z0[0] = ll - l*(1U<<(32-16-GUARD_BITS));

	if ( play_mode->encoding & PE_MONO ) continue;

	++i;
	if(lp[i] > NS_AMP_MAX) lp[i] = NS_AMP_MAX;
	else if(lp[i] < NS_AMP_MIN) lp[i] = NS_AMP_MIN;
	ll = lp[i] + ns_tap_0*ns_z1[0] +
		     ns_tap_1*ns_z1[1] +
		     ns_tap_2*ns_z1[2] +
		     ns_tap_3*ns_z1[3];
	l=ll>>(32-16-GUARD_BITS);
	lp[i] = l<<(32-16-GUARD_BITS);
	ns_z1[3] = ns_z1[2]; ns_z1[2] = ns_z1[1]; ns_z1[1] = ns_z1[0];
	ns_z1[0] = ll - l*(1U<<(32-16-GUARD_BITS));
    }
}


static void ns_shaping16(int32* lp, int32 c)
{
    switch(noise_sharp_type)
    {
      default:
	return;
      case 1:
	ns_shaping16_trad(lp, c * 2);
	break;
      case 2:
	ns_shaping16_9(lp, c * 2);
	break;
    }
}
