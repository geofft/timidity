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
#include "reverb.h"
#include <math.h>

/* delay buffers @65kHz */
#define REV_BUF0       344 * 2
#define REV_BUF1       684 * 2
#define REV_BUF2      2868 * 2
#define REV_BUF3      1368 * 2

#define REV_VAL0         5.3
#define REV_VAL1        10.5
#define REV_VAL2        44.12
#define REV_VAL3        21.0

FLOAT_T REV_INP_LEV = 0.55;

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


int isprime(int val)
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

void recompute_reverb_value(int32 output_rate)
{
	init_reverb(output_rate);
	rpt0 = def_rpt0 * reverb_status.time_ratio;
	rpt1 = def_rpt1 * reverb_status.time_ratio;
	rpt2 = def_rpt2 * reverb_status.time_ratio;
	rpt3 = def_rpt3 * reverb_status.time_ratio;
	while(!isprime(rpt0)) rpt0++;
	while(!isprime(rpt1)) rpt1++;
	while(!isprime(rpt2)) rpt2++;
	while(!isprime(rpt3)) rpt3++;
}


/* mixing dry signal */
void set_dry_signal(register int32 *buf, int32 n)
{
#if USE_ALTIVEC
  if(is_altivec_available()) {
    v_set_dry_signal(direct_buffer,buf,n);
  } else {
#endif
    register int32 i;
    register int32 count = n;
	register int32 *dbuf = direct_buffer;

    for(i=0;i<count;i++)
    {
        dbuf[i] += buf[i];
    }
#if USE_ALTIVEC
  }
#endif
}

#if OPT_MODE != 0
void mix_dry_signal(register int32 *buf, int32 n)
{
    register int32  i;
	register int32 count = n;
	register int32 *dbuf = direct_buffer;

    for(i=0;i<count;i++)
    {
 		buf[i] = dbuf[i];
		dbuf[i] = 0;
    }
}
#else
void mix_dry_signal(register int32 *buf, int32 n)
{
    register int32  i;
	register int32 count = n;
	register int32 *dbuf = direct_buffer;

    for(i=0;i<count;i++)
    {
 		buf[i] = dbuf[i];
		dbuf[i] = 0;
    }
}
#endif


void init_reverb(int32 output_rate)
{
    ta = 0; tb = 0;
    HPFL = 0; HPFR = 0;
    LPFL = 0; LPFR = 0;
    EPFL = 0; EPFR = 0;
    spt0 = 0; spt1 = 0;
    spt2 = 0; spt3 = 0;

    rev_memset(buf0_L); rev_memset(buf0_R);
    rev_memset(buf1_L); rev_memset(buf1_R);
    rev_memset(buf2_L); rev_memset(buf2_R);
    rev_memset(buf3_L); rev_memset(buf3_R);

    memset(effect_buffer, 0, effect_bufsize);
    memset(direct_buffer, 0, direct_bufsize);

    if(output_rate > 65000) output_rate=65000;
    else if(output_rate < 4000)  output_rate=4000;

    def_rpt0 = rpt0 = REV_VAL0 * output_rate / 1000;
    def_rpt1 = rpt1 = REV_VAL1 * output_rate / 1000;
    def_rpt2 = rpt2 = REV_VAL2 * output_rate / 1000;
    def_rpt3 = rpt3 = REV_VAL3 * output_rate / 1000;
}

#if OPT_MODE != 0
void set_ch_reverb(register int32 *sbuffer, int32 n, int32 level)
{
    register int32 i;
	register int32 count = n;
	register int32 *ebuf = effect_buffer;

    int32 send_level = (int32)((FLOAT_T)level / 127.0 * reverb_status.level_ratio * 0x100);

    for(i=0;i<count;i++)
    {
		ebuf[i] += imuldiv8(sbuffer[i],send_level);
    }
}
#else
void set_ch_reverb(register int32 *sbuffer, int32 n, int32 level)
{
    register int32  i;
    FLOAT_T send_level = (FLOAT_T)level / 127.0 * reverb_status.level_ratio;
	
	for(i = 0; i < n; i++)
    {
        effect_buffer[i] += sbuffer[i] * send_level;
    }
}
#endif /* OPT_MODE != 0 */

#if OPT_MODE != 0
void do_standard_reverb(register int32 *comp, int32 n)
{
	register int32 i;
    int32  fixp, s, t;
	int32 lpf_lev,lpf_inp,width,hpf_lev,fbk_lev,cmix_lev,epf_lev,epf_inp;

	lpf_lev = REV_LPF_LEV * 0x10000;
	lpf_inp = REV_LPF_INP * 0x10000;
	width = REV_WIDTH * 0x10000;
	hpf_lev = REV_HPF_LEV * 0x10000;
	fbk_lev = REV_FBK_LEV * 0x10000;
	cmix_lev = REV_CMIX_LEV * 0x10000;
	epf_lev = REV_EPF_LEV * 0x10000;
	epf_inp = REV_EPF_INP * 0x10000;

    for(i = 0; i < n; i++)
    {
        /* L */
        fixp = effect_buffer[i];
        effect_buffer[i] = 0;

        LPFL = imuldiv16(LPFL,lpf_lev) + imuldiv16(buf2_L[spt2] + tb,lpf_inp) + imuldiv16(ta,width);
        ta = buf3_L[spt3];
        s  = buf3_L[spt3] = buf0_L[spt0];
        buf0_L[spt0] = -LPFL;

        t = imuldiv16(HPFL + fixp,hpf_lev);
        HPFL = t - fixp;

        buf2_L[spt2] = imuldiv16(s - imuldiv16(fixp,fbk_lev),cmix_lev);
        tb = buf1_L[spt1];
        buf1_L[spt1] = t;

        EPFL = imuldiv16(EPFL,epf_lev) + imuldiv16(ta,epf_inp);
        comp[i] += ta + EPFL;

        /* R */
        fixp = effect_buffer[++i];
        effect_buffer[i] = 0;

        LPFR = imuldiv16(LPFR,lpf_lev) + imuldiv16(buf2_R[spt2] + tb,lpf_inp) + imuldiv16(ta,width);
        ta = buf3_R[spt3];
        s  = buf3_R[spt3] = buf0_R[spt0];
        buf0_R[spt0] = LPFR;

        t = imuldiv16(HPFR + fixp,hpf_lev);
        HPFR = t - fixp;

        buf2_R[spt2] = imuldiv16(s - imuldiv16(fixp,fbk_lev),cmix_lev);
        tb = buf1_R[spt1];
        buf1_R[spt1] = t;

        EPFR = imuldiv16(EPFR,epf_lev) + imuldiv16(ta,epf_inp);
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

void do_ch_reverb(int32* buf, int32 count)
{
	do_standard_reverb(buf,count);
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

/* delay effect */
#ifdef NEW_CHORUS
static int32 delay_effect_buffer[AUDIO_BUFFER_SIZE * 2];
/*#define DELAY_PRE_LPF*/
static FLOAT_T DELAY_INP_LEV = 1.0;
#define DELAY_BUFFER_SIZE 48000
static int32 delay_buf0_L[DELAY_BUFFER_SIZE + 1];
static int32 delay_buf0_R[DELAY_BUFFER_SIZE + 1];
static int32 delay_rpt0 = DELAY_BUFFER_SIZE;
static int32 delay_wpt0;	/* write point 0 of ring buffer */
static int32 delay_spt0;	/* read point 0 of ring buffer */
static int32 delay_spt1;	/* read point 1 of ring buffer */
static int32 delay_spt2;	/* read point 2 of ring buffer */
#endif /* NEW_CHORUS */

struct delay_status_t delay_status;	/* delay options */
struct reverb_status_t reverb_status;	/* reverb options */
struct insertion_effect_t insertion_effect;	/* i/e options */

void do_basic_delay(int32* buf, int32 count);
void do_cross_delay(int32* buf, int32 count);
void do_3tap_delay(int32* buf, int32 count);

void init_ch_delay()
{
#ifdef NEW_CHORUS
	memset(delay_buf0_L,0,sizeof(delay_buf0_L));
	memset(delay_buf0_R,0,sizeof(delay_buf0_R));
	memset(delay_effect_buffer,0,sizeof(delay_effect_buffer));
	memset(delay_status.lpf_val,0,sizeof(delay_status.lpf_val));

	delay_wpt0 = 0;
	delay_spt0 = 0;
	delay_spt1 = 0;
	delay_spt2 = 0;
#endif /* NEW_CHORUS */
}

#ifdef NEW_CHORUS
void do_ch_delay(int32* buf, int32 count)
{
#ifdef DELAY_PRE_LPF
	do_lowpass_12db(delay_effect_buffer,count,delay_status.lpf_coef,delay_status.lpf_val);
#endif /* DELAY_PRE_LPF */

	switch(delay_status.type) {
	case 1: do_3tap_delay(buf,count);
			break;
	case 2: do_cross_delay(buf,count);
			break;
	default: do_basic_delay(buf,count);
			break;
	}
}

#if OPT_MODE != 0
void set_ch_delay(register int32 *sbuffer, int32 n, int32 level)
{
    register int32 i;
	register int32 count = n;
    int32 send_level = (int32)((FLOAT_T)level * DELAY_INP_LEV / 127.0 * 256);

    for(i=0;i<count;i++)
    {
        delay_effect_buffer[i] += imuldiv8(sbuffer[i],level);
    }
}
#else
void set_ch_delay(register int32 *sbuffer, int32 n, int32 level)
{
    register int32 i;
	register int32 count = n;
    FLOAT_T send_level = (FLOAT_T)level * DELAY_INP_LEV / 127.0;

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

	level = delay_status.level_ratio_c * 0x10000;
	feedback = delay_status.feedback_ratio * 0x10000;
	send_reverb = delay_status.send_reverb_ratio * 0x10000;

	delay_spt0 = delay_wpt0 - delay_status.sample_c;
	if(delay_spt0 < 0) {delay_spt0 += delay_rpt0;}

	for(i=0;i<n;i++) {
		delay_buf0_L[delay_wpt0] = delay_effect_buffer[i] + imuldiv16(delay_buf0_L[delay_spt0],feedback);
		output = imuldiv16(delay_buf0_L[delay_spt0],level);
		buf[i] += output;
		effect_buffer[i] += imuldiv16(output,send_reverb);
		delay_effect_buffer[i] = 0;

		delay_buf0_R[delay_wpt0] = delay_effect_buffer[++i] + imuldiv16(delay_buf0_R[delay_spt0],feedback);
		output = imuldiv16(delay_buf0_R[delay_spt0],level);
		buf[i] += output;
		effect_buffer[i] += imuldiv16(output,send_reverb);
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

	level = delay_status.level_ratio_c;
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

	feedback = delay_status.feedback_ratio * 0x10000;
	level_c = delay_status.level_ratio_c * 0x10000;
	level_l = delay_status.level_ratio_l * 0x10000;
	level_r = delay_status.level_ratio_r * 0x10000;
	send_reverb = delay_status.send_reverb_ratio * 0x10000;

	delay_spt0 = delay_wpt0 - delay_status.sample_c;
	if(delay_spt0 < 0) {delay_spt0 += delay_rpt0;}
	delay_spt1 = delay_wpt0 - delay_status.sample_l;
	if(delay_spt1 < 0) {delay_spt1 += delay_rpt0;}
	delay_spt2 = delay_wpt0 - delay_status.sample_r;
	if(delay_spt2 < 0) {delay_spt2 += delay_rpt0;}

	for(i=0;i<n;i++) {
		delay_buf0_L[delay_wpt0] = delay_effect_buffer[i] + imuldiv16(delay_buf0_R[delay_spt0],feedback);
		output = imuldiv16(delay_buf0_L[delay_spt0],level_c) + imuldiv16(delay_buf0_L[delay_spt1] + delay_buf0_R[delay_spt1],level_l);
		buf[i] += output;
		effect_buffer[i] += imuldiv16(output,send_reverb);
		delay_effect_buffer[i] = 0;

		delay_buf0_R[delay_wpt0] = delay_effect_buffer[++i] + imuldiv16(delay_buf0_L[delay_spt0],feedback);
		output = imuldiv16(delay_buf0_R[delay_spt0],level_c) + imuldiv16(delay_buf0_L[delay_spt2] + delay_buf0_R[delay_spt2],level_r);
		buf[i] += output;
		effect_buffer[i] += imuldiv16(output,send_reverb);
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
	level_c = delay_status.level_ratio_c;
	level_l = delay_status.level_ratio_l;
	level_r = delay_status.level_ratio_r;

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

	feedback = delay_status.feedback_ratio * 0x10000;
	level_c = delay_status.level_ratio_c * 0x10000;
	level_l = delay_status.level_ratio_l * 0x10000;
	level_r = delay_status.level_ratio_r * 0x10000;
	send_reverb = delay_status.send_reverb_ratio * 0x10000;

	delay_spt0 = delay_wpt0 - delay_status.sample_c;
	if(delay_spt0 < 0) {delay_spt0 += delay_rpt0;}
	delay_spt1 = delay_wpt0 - delay_status.sample_l;
	if(delay_spt1 < 0) {delay_spt1 += delay_rpt0;}
	delay_spt2 = delay_wpt0 - delay_status.sample_r;
	if(delay_spt2 < 0) {delay_spt2 += delay_rpt0;}

	for(i=0;i<n;i++) {
		delay_buf0_L[delay_wpt0] = delay_effect_buffer[i] + imuldiv16(delay_buf0_L[delay_spt0],feedback);
		output = imuldiv16(delay_buf0_L[delay_spt0],level_c) + imuldiv16(delay_buf0_L[delay_spt1] + delay_buf0_R[delay_spt1],level_l);
		buf[i] += output;
		effect_buffer[i] += imuldiv16(output,send_reverb);
		delay_effect_buffer[i] = 0;

		delay_buf0_R[delay_wpt0] = delay_effect_buffer[++i] + imuldiv16(delay_buf0_R[delay_spt0],feedback);
		output = imuldiv16(delay_buf0_R[delay_spt0],level_c) + imuldiv16(delay_buf0_L[delay_spt2] + delay_buf0_R[delay_spt2],level_r);
		buf[i] += output;
		effect_buffer[i] += imuldiv16(output,send_reverb);
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
	level_c = delay_status.level_ratio_c;
	level_l = delay_status.level_ratio_l;
	level_r = delay_status.level_ratio_r;

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

#endif /* NEW_CHORUS */

/* chorus effect */
#ifdef NEW_CHORUS
static int32 chorus_effect_buffer[AUDIO_BUFFER_SIZE * 2];
#define CHORUS_BUFFER_SIZE 9600
#define CHORUS_LFO_SIZE 1024
/*#define CHORUS_PRE_LPF*/
static FLOAT_T CHORUS_INP_LEV = 1.0;
static int32 chorus_buf0_L[CHORUS_BUFFER_SIZE + 1];
static int32 chorus_buf0_R[CHORUS_BUFFER_SIZE + 1];
static int32 chorus_rpt0 = CHORUS_BUFFER_SIZE;
static int32 chorus_wpt0;	/* write point 0 of ring buffer */
static int32 chorus_spt0;	/* read point 0 of ring buffer */
static int32 chorus_spt1;	/* read point 1 of ring buffer */
static int16 chorus_lfo0[CHORUS_LFO_SIZE];
static int16 chorus_lfo1[CHORUS_LFO_SIZE];
static int32 chorus_cyc0;	/* cycle of lfo0 */
static int32 chorus_cnt0;	/* counter for lfo0 */
static FLOAT_T chorus_div0;
#endif /* NEW_CHORUS */
struct chorus_status_t chorus_status;	/* chorus options */
struct chorus_param_t chorus_param;

void init_chorus_lfo()
{
#ifdef NEW_CHORUS
	register int32 i,j;
	int32 depth,delay,shift;
	FLOAT_T x = M_PI * 2.0 / (FLOAT_T)CHORUS_LFO_SIZE;
	chorus_cyc0 = chorus_param.cycle_in_sample;
	chorus_div0 = (FLOAT_T)CHORUS_LFO_SIZE / (FLOAT_T)chorus_cyc0;
	depth = chorus_param.depth_in_sample / 2;
	delay = chorus_param.delay_in_sample;
	shift = CHORUS_LFO_SIZE / 4;

	for(i=0;i<CHORUS_LFO_SIZE;i++) {
		chorus_lfo0[i] = delay + (sin(x * (FLOAT_T)i) + 1.0) * depth;
	}

	for(i=shift,j=0;i<CHORUS_LFO_SIZE;i++,j++) {
		chorus_lfo1[i] = chorus_lfo0[j];
	}

	for(i=0,j=CHORUS_LFO_SIZE - shift;i<shift;i++,j++) {
		chorus_lfo1[i] = chorus_lfo0[j];
	}

#endif /* NEW_CHORUS */
}

void init_ch_chorus()
{
#ifdef NEW_CHORUS
	memset(chorus_buf0_L,0,sizeof(chorus_buf0_L));
	memset(chorus_buf0_R,0,sizeof(chorus_buf0_R));
	memset(chorus_effect_buffer,0,sizeof(chorus_effect_buffer));
	memset(chorus_param.lpf_val,0,sizeof(chorus_param.lpf_val));

	chorus_cnt0 = 0;
	chorus_wpt0 = 0;
	chorus_spt0 = 0;
	chorus_spt1 = 0;
#endif /* NEW_CHORUS */
}


#ifdef NEW_CHORUS
#if OPT_MODE != 0
void do_stereo_chorus(int32* buf,int32 count)
{
	register int32 i;
	register int32 n = count;
	int32 lfo_cnt,lfo_div,level,feedback,send_reverb,send_delay,output;

	level = chorus_param.level_ratio * 0x10000;
	feedback = chorus_param.feedback_ratio * 0x10000;
	lfo_div = chorus_div0 * 0x10000;
	send_reverb = chorus_param.send_reverb_ratio * 0x10000;
	send_delay = chorus_param.send_delay_ratio * 0x10000;

	lfo_cnt = imuldiv16(chorus_cnt0,lfo_div);
	chorus_spt0 = chorus_wpt0 - chorus_lfo0[lfo_cnt];
	if(chorus_spt0 < 0) {chorus_spt0 += chorus_rpt0;}
	chorus_spt1 = chorus_wpt0 - chorus_lfo1[lfo_cnt];
	if(chorus_spt1 < 0) {chorus_spt1 += chorus_rpt0;}

	for(i=0;i<n;i++) {
		chorus_buf0_L[chorus_wpt0] = chorus_effect_buffer[i] + imuldiv16(chorus_buf0_L[chorus_spt0],feedback);
		output = imuldiv16(chorus_buf0_L[chorus_spt0],level);
		buf[i] += output;
		effect_buffer[i] += imuldiv16(output,send_reverb);
		delay_effect_buffer[i] += imuldiv16(output,send_delay);
		chorus_effect_buffer[i] = 0;

		chorus_buf0_R[chorus_wpt0] = chorus_effect_buffer[++i] + imuldiv16(chorus_buf0_R[chorus_spt1],feedback);
		output = imuldiv16(chorus_buf0_R[chorus_spt1],level);
		buf[i] += output;
		effect_buffer[i] += imuldiv16(output,send_reverb);
		delay_effect_buffer[i] += imuldiv16(output,send_delay);
		chorus_effect_buffer[i] = 0;

		if(++chorus_wpt0 == chorus_rpt0) {chorus_wpt0 = 0;}
		if(++chorus_cnt0 == chorus_cyc0) {chorus_cnt0 = 0;}
		lfo_cnt = imuldiv16(chorus_cnt0,lfo_div);
		chorus_spt0 = chorus_wpt0 - chorus_lfo0[lfo_cnt];
		if(chorus_spt0 < 0) {chorus_spt0 += chorus_rpt0;}
		chorus_spt1 = chorus_wpt0 - chorus_lfo1[lfo_cnt];
		if(chorus_spt1 < 0) {chorus_spt1 += chorus_rpt0;}
	}
}
#else
void do_stereo_chorus(int32* buf,int32 count)
{
	register int32 i;
	register int32 n = count;
	int32 lfo_cnt;
	FLOAT_T level,feedback;

	level = chorus_param.level_ratio;
	feedback = chorus_param.feedback_ratio;

	lfo_cnt = chorus_cnt0 * chorus_div0;
	chorus_spt0 = chorus_wpt0 - chorus_lfo0[lfo_cnt];
	if(chorus_spt0 < 0) {chorus_spt0 += chorus_rpt0;}
	chorus_spt1 = chorus_wpt0 - chorus_lfo1[lfo_cnt];
	if(chorus_spt1 < 0) {chorus_spt1 += chorus_rpt0;}

	for(i=0;i<n;i++) {
		chorus_buf0_L[chorus_wpt0] = chorus_effect_buffer[i] + chorus_buf0_L[chorus_spt0] * feedback;
		buf[i] += chorus_buf0_L[chorus_spt0] * level;
		chorus_effect_buffer[i] = 0;

		chorus_buf0_R[chorus_wpt0] = chorus_effect_buffer[++i] + chorus_buf0_R[chorus_spt1] * feedback;
		buf[i] += chorus_buf0_R[chorus_spt1] * level;
		chorus_effect_buffer[i] = 0;

		if(++chorus_wpt0 == chorus_rpt0) {chorus_wpt0 = 0;}
		if(++chorus_cnt0 == chorus_cyc0) {chorus_cnt0 = 0;}
		lfo_cnt = chorus_cnt0 * chorus_div0;
		chorus_spt0 = chorus_wpt0 - chorus_lfo0[lfo_cnt];
		if(chorus_spt0 < 0) {chorus_spt0 += chorus_rpt0;}
		chorus_spt1 = chorus_wpt0 - chorus_lfo1[lfo_cnt];
		if(chorus_spt1 < 0) {chorus_spt1 += chorus_rpt0;}
	}
}
#endif /* OPT_MODE != 0 */

#if OPT_MODE != 0
void set_ch_chorus(register int32 *sbuffer,int32 n, int32 level)
{
    register int32 i;
    register int32 count = n;
    int32 send_level = (int32)((FLOAT_T)level * CHORUS_INP_LEV / 127.0 * 256);

    for(i=0;i<count;i++)
    {
		chorus_effect_buffer[i] += imuldiv8(sbuffer[i],send_level);
    }
}
#else
void set_ch_chorus(register int32 *sbuffer,int32 n, int32 level)
{
    register int32 i;
    register int32 count = n;
    FLOAT_T send_level = (FLOAT_T)level * CHORUS_INP_LEV / 127.0;

    for(i=0;i<count;i++)
    {
		chorus_effect_buffer[i] += sbuffer[i] * send_level;
    }
}
#endif /* OPT_MODE != 0 */

void do_ch_chorus(int32* buf, int32 count)
{
#ifdef CHORUS_PRE_LPF
	do_lowpass_12db(chorus_effect_buffer,count,chorus_param.lpf_coef,chorus_param.lpf_val);
#endif /* CHORUS_PRE_LPF */
	do_stereo_chorus(buf,count);
}
#endif /* NEW_CHORUS */


/* EQ (equalizer) */
static int32 eq_buffer[AUDIO_BUFFER_SIZE*2];
struct eq_status_t eq_status;	/* eq options */

void init_eq()
{
	memset(eq_buffer,0,sizeof(eq_buffer));
	memset(eq_status.low_val,0,sizeof(eq_status.low_val));
	memset(eq_status.high_val,0,sizeof(eq_status.high_val));
}

void calc_lowshelf_coefs(int32* coef,int32 cutoff_freq,FLOAT_T dbGain,int32 rate)
{
	FLOAT_T a0,a1,a2,b0,b1,b2,omega,sn,cs,A,beta,S;
	FLOAT_T max_dbGain = 12;

	if(dbGain > max_dbGain) {dbGain = max_dbGain;}

	S = 1;
	A = pow(10, dbGain / 40);
	omega = 2.0 * M_PI * (FLOAT_T)cutoff_freq / (FLOAT_T)rate;
	sn = sin(omega);
	cs = cos(omega);
	beta = sqrt((A * A + 1.0) / S - (A - 1) * (A - 1));

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

	coef[0] = a1 * 0x10000;
	coef[1] = a2 * 0x10000;
	coef[2] = b0 * 0x10000;
	coef[3] = b1 * 0x10000;
	coef[4] = b2 * 0x10000;
}

void calc_highshelf_coefs(int32* coef,int32 cutoff_freq,FLOAT_T dbGain,int32 rate)
{
	FLOAT_T a0,a1,a2,b0,b1,b2,omega,sn,cs,A,beta,S;
	FLOAT_T max_dbGain = 12;

	if(dbGain > max_dbGain) {dbGain = max_dbGain;}

	S = 1;
	A = pow(10, dbGain / 40);
	omega = 2.0 * M_PI * (FLOAT_T)cutoff_freq / (FLOAT_T)rate;
	sn = sin(omega);
	cs = cos(omega);
	beta = sqrt((A * A + 1.0) / S - (A - 1) * (A - 1));

	a0 = 1.0 /((A + 1) - (A - 1) * cs + beta * sn);
	a1 = -2 * ((A - 1) - (A + 1) * cs);
	a2 = -((A + 1) - (A - 1) * cs - beta * sn);
	b0 = A * ((A + 1) + (A - 1) * cs + beta * sn);
	b1 = -2 * A * ((A - 1) + (A + 1) * cs);
	b2 = A * ((A + 1) + (A - 1) * cs - beta * sn);

	a1 *= a0;
	a2 *= a0;
	b1 *= a0;
	b2 *= a0;
	b0 *= a0;

	coef[0] = a1 * 0x10000;
	coef[1] = a2 * 0x10000;
	coef[2] = b0 * 0x10000;
	coef[3] = b1 * 0x10000;
	coef[4] = b2 * 0x10000;
}


void do_eq(register int32* buf,int32 count,int32* eq_coef,int32* eq_val)
{
#if OPT_MODE != 0
	register int32 i;
	int32 length;
	int32 x1l,x2l,y1l,y2l,x1r,x2r,y1r,y2r,yout;
	int32 a1,a2,b0,b1,b2;

	a1 = eq_coef[0];
	a2 = eq_coef[1];
	b0 = eq_coef[2];
	b1 = eq_coef[3];
	b2 = eq_coef[4];

	length = count;

	x1l = eq_val[0];
	x2l = eq_val[1];
	y1l = eq_val[2];
	y2l = eq_val[3];
	x1r = eq_val[4];
	x2r = eq_val[5];
	y1r = eq_val[6];
	y2r = eq_val[7];

	for(i=0;i<length;i++) {
		yout = imuldiv16(buf[i],b0) + imuldiv16(x1l,b1) + imuldiv16(x2l,b2) + imuldiv16(y1l,a1) + imuldiv16(y2l,a2);
		x2l = x1l;
		x1l = buf[i];
		y2l = y1l;
		y1l = yout;
		buf[i] = yout;

		yout = imuldiv16(buf[++i],b0) + imuldiv16(x1r,b1) + imuldiv16(x2r,b2) + imuldiv16(y1r,a1) + imuldiv16(y2r,a2);
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

	do_eq(eq_buffer,count,eq_status.low_coef,eq_status.low_val);
	do_eq(eq_buffer,count,eq_status.high_coef,eq_status.high_val);

	for(i=0;i<count;i++) {
		buf[i] += eq_buffer[i];
		eq_buffer[i] = 0;
	}
}

#if OPT_MODE != 0
void set_ch_eq(register int32 *buf, int32 n)
{
    register int32 i;
	register int32 count = n;

    for(i=0;i<count;i++)
    {
        eq_buffer[i] += buf[i];
    }
}
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

/* lowpass filters */
void do_lowpass_12db(register int32* buf,int32 count,int32* lpf_coef,int32* lpf_val)
{
#if OPT_MODE != 0
	register int32 i,length;
	int32 x1l,y1l,y2l,x1r,y1r,y2r,yout;
	int32 a1,a2,b0;

	a1 = lpf_coef[0];
	a2 = lpf_coef[1];
	b0 = lpf_coef[2];

	length = count;

	x1l = lpf_val[0];
	y1l = lpf_val[1];
	y2l = lpf_val[2];
	x1r = lpf_val[3];
	y1r = lpf_val[4];
	y2r = lpf_val[5];

	for(i=0;i<length;i++) {
		yout = imuldiv16(y1l,a1) + imuldiv16(y2l,a2) + imuldiv16(x1l,b0);
		x1l = buf[i];
		buf[i] = yout;
		y2l = y1l;
		y1l = yout;

		yout = imuldiv16(y1r,a1) + imuldiv16(y2r,a2) + imuldiv16(x1r,b0);
		x1r = buf[++i];
		buf[i] = yout;
		y2r = y1r;
		y1r = yout;
	}

	lpf_val[0] = x1l;
	lpf_val[1] = y1l;
	lpf_val[2] = y2l;
	lpf_val[3] = x1r;
	lpf_val[4] = y1r;
	lpf_val[5] = y2r;
#endif /* OPT_MODE != 0 */
}

void calc_lowpass_coefs_12db(int32* lpf_coef,int32 cutoff_freq,int16 resonance,int32 rate)
{
	FLOAT_T T,w0,k,a1,a2,b0;

	if(resonance >= 127) {resonance = 126;}

	T = 1.0 / (FLOAT_T)rate;
	w0 = 2.0 * M_PI * (FLOAT_T)cutoff_freq;
	k = 0.007874f * (127 - resonance);
	a1 = 2.0 * exp(-w0 * k / sqrt(1.0 - k * k) * T) * cos(w0 * T);
	a2 = -exp(-2.0 * w0 * k / sqrt(1.0 - k * k) * T);
	b0 = 1.0 - a1 - a2;

	lpf_coef[0] = a1 * 0x10000;
	lpf_coef[1] = a2 * 0x10000;
	lpf_coef[2] = b0 * 0x10000;
}

void calc_lowpass_coefs_24db(int32* lpf_coef,int32 cutoff_freq,int16 resonance,int32 rate)
{
	FLOAT_T c,a1,a2,a3,b1,b2,q,sqrt2 = 1.4142134;

	q = sqrt2 - (sqrt2 - 0.1) * (FLOAT_T)resonance / 127;

	c = 1.0 / tan(M_PI * (FLOAT_T)cutoff_freq / (FLOAT_T)rate); 

	a1 = 1.0 / (1.0 + q * c + c * c); 
	a2 = 2* a1; 
	a3 = a1; 
	b1 = -(2.0 * (1.0 - c * c) * a1); 
	b2 = -(1.0 - q * c + c * c) * a1; 

	lpf_coef[0] = a1 * 0x10000;
	lpf_coef[1] = a2 * 0x10000;
	lpf_coef[2] = a3 * 0x10000;
	lpf_coef[3] = b1 * 0x10000;
	lpf_coef[4] = b2 * 0x10000;
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
		yout = imuldiv16(buf[i] + x2l,a1) + imuldiv16(x1l,a2) + imuldiv16(y1l,b1) + imuldiv16(y2l,b2);
		x2l = x1l;
		x1l = buf[i];
		buf[i] = yout;
		y2l = y1l;
		y1l = yout;

		yout = imuldiv16(buf[++i] + x2r,a1) + imuldiv16(x1r,a2) + imuldiv16(y1r,b1) + imuldiv16(y2r,b2);
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

void calc_lowpass_coefs(int32* lpf_coef,int32 cutoff_freq,int16 resonance,int32 rate,int8 type)
{
	switch(type) {
	case 1:	calc_lowpass_coefs_12db(lpf_coef,cutoff_freq,resonance,rate);
		break;
	case 2: calc_lowpass_coefs_24db(lpf_coef,cutoff_freq,resonance,rate);
		break;
	default:
		break;
	}
}

void do_channel_lpf(register int32* buf,int32 count,int32* lpf_coef,int32* lpf_val,int8 type)
{
	switch(type) {
	case 1:	do_lowpass_12db(buf,count,lpf_coef,lpf_val);
		break;
	case 2: do_lowpass_24db(buf,count,lpf_coef,lpf_val);
		break;
	default:
		break;
	}
}


/*                             */
/* Insertion Effect (SC-88Pro) */
/*                             */

/* volume stat. for Overdrive / Distortion */
int32 od_max_volume1;	
int32 od_max_volume2;

/* general-purpose volume stat. */
inline void do_volume_stat(int32 sample,int32 *volume)
{
	*volume -= 200;
	if(sample < 0) {sample = -sample;}
	if(sample > *volume) {*volume = sample;}
}

/* general-purpose Panning */
/* pan = -1.0 ~ 1.0          */
inline int32 do_left_panning(int32 sample, FLOAT_T pan)
{
	return (int32)(sample - sample * pan);
}

inline int32 do_right_panning(int32 sample, FLOAT_T pan)
{
	return (int32)(sample + sample * pan);
}

/* general-purpose Distortion */
/* level = 0.0 ~ 1.0         */
/* volume = 0.0 ~ 1.0        */ 
inline int32 do_distortion(int32 sample,FLOAT_T level,FLOAT_T volume,int32 max_volume)
{
	int32 od_clip = max_volume >> 2;
	sample *= level;
	if(sample > od_clip) {sample = od_clip;}
	else if(sample < -od_clip) {sample = -od_clip;}
	sample *= volume;
	sample <<= 2;
	return sample;
}

/* general-purpose Overdrive */
/* level = 0.0 ~ 1.0        */
/* volume = 0.0 ~ 1.0       */
inline int32 do_overdrive(int32 sample,FLOAT_T level,FLOAT_T volume,int32 max_volume)
{
	int32 od_threshold = max_volume >> 1;

	if(sample < -od_threshold) {
		sample = (int32)(-pow((FLOAT_T)-sample / (FLOAT_T)max_volume, level) * volume * max_volume);
	} else if(sample > od_threshold) {
		sample = (int32)(pow((FLOAT_T)sample / (FLOAT_T)max_volume, level) * volume * max_volume);
	} else {
		sample *= volume;
	}

	return sample;
}

/* 0x0110: Overdrive */
void do_0110_overdrive(int32* buf, int32 count)
{
	register int32 i;
	int32 n = count,output;
	FLOAT_T pan;
	FLOAT_T level,volume;

	volume = (FLOAT_T)insertion_effect.parameter[19] / 127.0;
	level = (FLOAT_T)insertion_effect.parameter[0] / 127.0;
	pan = (insertion_effect.parameter[18] - 0x40) / 63.0;

	for(i=0;i<n;i++) {
		/* Left */
		output = buf[i];
		do_volume_stat(output,&od_max_volume1);
		output = do_overdrive(output,level,volume,od_max_volume1);
		buf[i] = do_left_panning(output,pan);

		/* Right */
		output = buf[++i];
		do_volume_stat(output,&od_max_volume2);
		output = do_overdrive(output,level,volume,od_max_volume2);
		buf[i] = do_right_panning(output,pan);
	}
}

/* 0x0111: Distortion */
void do_0111_distortion(int32* buf, int32 count)
{
	register int32 i;
	int32 n = count,output;
	FLOAT_T pan;
	FLOAT_T level,volume;

	volume = (FLOAT_T)insertion_effect.parameter[19] / 127.0;
	level = (FLOAT_T)insertion_effect.parameter[0] / 127.0;
	pan = (insertion_effect.parameter[18] - 0x40) / 63.0;

	for(i=0;i<n;i++) {
		/* Left */
		output = buf[i];
		do_volume_stat(output,&od_max_volume1);
		output = do_distortion(output,level,volume,od_max_volume1);
		buf[i] = do_left_panning(output,pan);

		/* Right */
		output = buf[++i];
		do_volume_stat(output,&od_max_volume2);
		output = do_distortion(output,level,volume,od_max_volume2);
		buf[i] = do_right_panning(output,pan);
	}
}

/* 0x1103: OD1 / OD2 */
void do_1103_dual_od(int32* buf, int32 count)
{
	register int32 i;
	int32 n = count,output1,output2,type;
	FLOAT_T pan1,pan2;
	FLOAT_T level1,level2,volume,volume1,volume2;
	int32 (*od1)(int32,FLOAT_T,FLOAT_T,int32),(*od2)(int32,FLOAT_T,FLOAT_T,int32);

	volume = (FLOAT_T)insertion_effect.parameter[19] / 127.0 * 0.5;
	volume1 = (FLOAT_T)insertion_effect.parameter[16] / 127.0 * volume;
	volume2 = (FLOAT_T)insertion_effect.parameter[18] / 127.0 * volume;
	level1 = (FLOAT_T)insertion_effect.parameter[1] / 127.0;
	level2 = (FLOAT_T)insertion_effect.parameter[6] / 127.0;
	pan1 = (insertion_effect.parameter[15] - 0x40) / 63.0;
	pan2 = (insertion_effect.parameter[17] - 0x40) / 63.0;

	type = insertion_effect.parameter[0];
	if(type == 0) {od1 = do_overdrive;}
	else {od1 = do_distortion;}

	type = insertion_effect.parameter[5];
	if(type == 0) {od2 = do_overdrive;}
	else {od2 = do_distortion;}

	for(i=0;i<n;i++) {
		/* Left */
		output1 = buf[i];
		do_volume_stat(output1,&od_max_volume1);
		output1 = (*od1)(output1,level1,volume1,od_max_volume1);

		/* Right */
		output2 = buf[++i];
		do_volume_stat(output2,&od_max_volume2);
		output2 = (*od2)(output2,level2,volume2,od_max_volume2);

		/* Mix */
		buf[i-1] = do_left_panning(output1,pan1) + do_left_panning(output2,pan2);
		buf[i] = do_right_panning(output1,pan1) + do_right_panning(output2,pan2);
	}
}

void init_insertion_effect()
{
	memset(insertion_effect.eq_low_val,0,sizeof(insertion_effect.eq_low_val));
	memset(insertion_effect.eq_high_val,0,sizeof(insertion_effect.eq_high_val));
	od_max_volume1 = 0;
	od_max_volume2 = 0;
}

void do_insertion_effect(int32 *buf,int32 count)
{
	if(insertion_effect.eq_low_gain != 0 || insertion_effect.eq_high_gain != 0) {
		do_eq(buf,count,insertion_effect.eq_low_coef,insertion_effect.eq_low_val);
		do_eq(buf,count,insertion_effect.eq_high_coef,insertion_effect.eq_high_val);
	}

	switch(insertion_effect.type) {
	case 0x0110: /* Overdrive */
		do_0110_overdrive(buf,count);
		break;
	case 0x0111: /* Distortion */
		do_0111_distortion(buf,count);
		break;
	case 0x1103: /* OD1 / OD2 */
		do_1103_dual_od(buf,count);
		break;
	default: break;
	}
}
