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

#include "timidity.h"
#include "instrum.h"
#include "playmidi.h"
#include "output.h"
#include "reverb.h"


static void effect_left_right_delay(int32* buff, int32 count);
static void init_ns_tap(void);
static void ns_shaping8(int32* buf, int32 count);

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

