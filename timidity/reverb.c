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

void do_ch_reverb(int32* buf, int32 count)
{
	int i;
	if((opt_reverb_control == 3 || opt_effect_quality >= 1) && reverb_status.pre_lpf) {
		do_shelving_filter(effect_buffer, count, reverb_status.high_coef, reverb_status.high_val);
	}
	if(opt_reverb_control == 3 || opt_effect_quality >= 2) {
		do_freeverb(buf, count);
	} else {
		do_standard_reverb(buf, count);
	}
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
	memset(delay_buf0_L,0,sizeof(delay_buf0_L));
	memset(delay_buf0_R,0,sizeof(delay_buf0_R));
	memset(delay_effect_buffer,0,sizeof(delay_effect_buffer));
	memset(delay_status.high_val,0,sizeof(delay_status.high_val));

	delay_wpt0 = 0;
	delay_spt0 = 0;
	delay_spt1 = 0;
	delay_spt2 = 0;
#endif /* USE_DSP_EFFECT */
}

#ifdef USE_DSP_EFFECT
void do_ch_delay(int32* buf, int32 count)
{
	if((opt_reverb_control == 3 || opt_effect_quality >= 1) && delay_status.pre_lpf) {
		do_shelving_filter(delay_effect_buffer, count, delay_status.high_coef, delay_status.high_val);
	}

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
static int32 chorus_wpt0;
static int32 chorus_wpt1;
static int32 chorus_spt0;
static int32 chorus_spt1;
static int32 chorus_lfo0[SINE_CYCLE_LENGTH];
static int32 chorus_lfo1[SINE_CYCLE_LENGTH];
static int32 chorus_cyc0;
static int32 chorus_cnt0;
#endif /* USE_DSP_EFFECT */

void init_chorus_lfo(void)
{
#ifdef USE_DSP_EFFECT
	int32 i;

	chorus_cyc0 = chorus_param.cycle_in_sample;
	if(chorus_cyc0 == 0) {chorus_cyc0 = 1;}

	for(i=0;i<SINE_CYCLE_LENGTH;i++) {
		chorus_lfo0[i] = TIM_FSCALE((lookup_triangular(i) + 1.0) / 2, 24);
	}

	for(i=0;i<SINE_CYCLE_LENGTH;i++) {
		chorus_lfo1[i] = TIM_FSCALE((lookup_triangular(i + SINE_CYCLE_LENGTH / 4) + 1.0) / 2, 24);
	}
#endif /* USE_DSP_EFFECT */
}

void init_ch_chorus()
{
#ifdef USE_DSP_EFFECT
	memset(chorus_buf0_L,0,sizeof(chorus_buf0_L));
	memset(chorus_buf0_R,0,sizeof(chorus_buf0_R));
	memset(chorus_effect_buffer,0,sizeof(chorus_effect_buffer));
	memset(chorus_param.high_val,0,sizeof(chorus_param.high_val));

	chorus_cnt0 = 0;
	chorus_wpt0 = 0;
	chorus_wpt1 = 0;
	chorus_spt0 = 0;
	chorus_spt1 = 0;
#endif /* USE_DSP_EFFECT */
}

#ifdef USE_DSP_EFFECT
void do_stereo_chorus(int32* buf, register int32 count)
{
#if OPT_MODE != 0	/* fixed-point implementation */
	register int32 i;
	int32 level, feedback, send_reverb, send_delay, delay, depth, output, div, v1l, v1r, f0, f1;

	level = TIM_FSCALE(chorus_param.level_ratio * MASTER_CHORUS_LEVEL, 24);
	feedback = TIM_FSCALE(chorus_param.feedback_ratio, 24);
	send_reverb = TIM_FSCALE(chorus_param.send_reverb_ratio * REV_INP_LEV, 24);
	send_delay = TIM_FSCALE(chorus_param.send_delay_ratio, 24);
	depth = chorus_param.depth_in_sample;
	delay = chorus_param.delay_in_sample;
	div = TIM_FSCALE((SINE_CYCLE_LENGTH - 1) / (double)chorus_cyc0, 24) - 0.5;

	f0 = imuldiv16(chorus_lfo0[imuldiv24(chorus_cnt0, div)], depth);
	chorus_spt0 = chorus_wpt0 - delay - (f0 >> 8);
	f0 &= 0xFF;
	if(chorus_spt0 < 0) {chorus_spt0 += chorus_rpt0;}
	f1 = imuldiv16(chorus_lfo1[imuldiv24(chorus_cnt0, div)], depth);
	chorus_spt1 = chorus_wpt0 - delay - (f1 >> 8);
	f1 &= 0xFF;
	if(chorus_spt1 < 0) {chorus_spt1 += chorus_rpt0;}
	
	for(i=0;i<count;i++) {
		v1l = chorus_buf0_L[chorus_spt0];
		v1r = chorus_buf0_R[chorus_spt1];

		chorus_wpt1 = chorus_wpt0;
		if(++chorus_wpt0 == chorus_rpt0) {chorus_wpt0 = 0;}
		f0 = imuldiv16(chorus_lfo0[imuldiv24(chorus_cnt0, div)], depth);
		chorus_spt0 = chorus_wpt0 - delay - (f0 >> 8);
		f0 &= 0xFF;
		if(chorus_spt0 < 0) {chorus_spt0 += chorus_rpt0;}
		f1 = imuldiv16(chorus_lfo1[imuldiv24(chorus_cnt0, div)], depth);
		chorus_spt1 = chorus_wpt0 - delay - (f1 >> 8);
		f1 &= 0xFF;
		if(chorus_spt1 < 0) {chorus_spt1 += chorus_rpt0;}
		if(++chorus_cnt0 == chorus_cyc0) {chorus_cnt0 = 0;}

		output = v1l + imuldiv8(chorus_buf0_L[chorus_spt0] - v1l, f0);
		chorus_buf0_L[chorus_wpt1] = chorus_effect_buffer[i] + imuldiv24(output, feedback);
		output = imuldiv24(output, level);
		buf[i] += output;
		effect_buffer[i] += imuldiv24(output, send_reverb);
		delay_effect_buffer[i] += imuldiv24(output, send_delay);
		chorus_effect_buffer[i] = 0;

		output = v1r + imuldiv8(chorus_buf0_R[chorus_spt1] - v1r, f1);
		chorus_buf0_R[chorus_wpt1] = chorus_effect_buffer[++i] + imuldiv24(output, feedback);
		output = imuldiv24(output, level);
		buf[i] += output;
		effect_buffer[i] += imuldiv24(output, send_reverb);
		delay_effect_buffer[i] += imuldiv24(output, send_delay);
		chorus_effect_buffer[i] = 0;
	}
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

void do_ch_chorus(int32* buf, int32 count)
{
	if((opt_reverb_control == 3 || opt_effect_quality >= 1) && chorus_param.chorus_pre_lpf) {
		do_shelving_filter(chorus_effect_buffer, count, chorus_param.high_coef, chorus_param.high_val);
	}
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
static inline int32 do_left_panning(int32 sample, FLOAT_T pan)
{
	return (int32)(sample - sample * pan);
}

static inline int32 do_right_panning(int32 sample, FLOAT_T pan)
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

	volume = (FLOAT_T)gs_ieffect.parameter[19] / 127.0;
	level = (FLOAT_T)gs_ieffect.parameter[0] / 127.0;
	pan = (gs_ieffect.parameter[18] - 0x40) / 63.0;

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

	volume = (FLOAT_T)gs_ieffect.parameter[19] / 127.0;
	level = (FLOAT_T)gs_ieffect.parameter[0] / 127.0;
	pan = (gs_ieffect.parameter[18] - 0x40) / 63.0;

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

	volume = (FLOAT_T)gs_ieffect.parameter[19] / 127.0;
	volume1 = (FLOAT_T)gs_ieffect.parameter[16] / 127.0 * volume;
	volume2 = (FLOAT_T)gs_ieffect.parameter[18] / 127.0 * volume;
	level1 = (FLOAT_T)gs_ieffect.parameter[1] / 127.0;
	level2 = (FLOAT_T)gs_ieffect.parameter[6] / 127.0;
	pan1 = (gs_ieffect.parameter[15] - 0x40) / 63.0;
	pan2 = (gs_ieffect.parameter[17] - 0x40) / 63.0;

	type = gs_ieffect.parameter[0];
	if(type == 0) {od1 = do_overdrive;}
	else {od1 = do_distortion;}

	type = gs_ieffect.parameter[5];
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
	od_max_volume1 = 0;
	od_max_volume2 = 0;
}

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

static void realloc_allpass(allpass *allpass)
{
	if(allpass->buf != NULL) {free(allpass->buf);}
	allpass->buf = (int32 *)safe_malloc(sizeof(int32) * allpass->size);
	if(allpass->buf == NULL) {return;}
	allpass->index = 0;
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

static void realloc_comb(comb *comb)
{
	if(comb->buf != NULL) {free(comb->buf);}
	comb->buf = (int32 *)safe_malloc(sizeof(int32) * comb->size);
	if(comb->buf == NULL) {return;}
	comb->index = 0;
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
		realloc_comb(&rev->combL[i]);
		realloc_comb(&rev->combR[i]);
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
		realloc_allpass(&rev->allpassL[i]);
		realloc_allpass(&rev->allpassR[i]);
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
	memset(reverb_status.high_val, 0, sizeof(reverb_status.high_val));
	if(opt_reverb_control == 3 || opt_effect_quality >= 2) {
		alloc_revmodel();
		update_revmodel(revmodel);
		init_revmodel(revmodel);
		memset(effect_buffer, 0, effect_bufsize);
		memset(direct_buffer, 0, direct_bufsize);
		REV_INP_LEV = fixedgain * revmodel->wet;
	} else {
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

		REV_INP_LEV = 1.0;

		rpt0 = def_rpt0 * reverb_status.time_ratio;
		rpt1 = def_rpt1 * reverb_status.time_ratio;
		rpt2 = def_rpt2 * reverb_status.time_ratio;
		rpt3 = def_rpt3 * reverb_status.time_ratio;
		while(!isprime(rpt0)) rpt0++;
		while(!isprime(rpt1)) rpt1++;
		while(!isprime(rpt2)) rpt2++;
		while(!isprime(rpt3)) rpt3++;
	}
}

void free_effect_buffers(void)
{
	free_revmodel();
}

/*                                                        */
/* new implementation for insertion and variation effect. */
/*               (under construction...)                  */
/*                                                        */

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
		efc->do_effect = NULL;
		if(efc->info != NULL) {free(efc->info);}
		free(efc);
	} while ((efc = efn) != NULL);
}

/*! general purpose 2-band equalizer engine. */
void do_eq2(int32 *buf, int32 count, EffectList *ef)
{
	struct InfoEQ2 *eq = (struct InfoEQ2 *)ef->info;
	if(count == MAGIC_INIT_EFFECT_INFO) {
		calc_lowshelf_coefs(eq->low_coef, eq->low_freq, eq->low_gain, play_mode->rate);
		calc_highshelf_coefs(eq->high_coef, eq->high_freq, eq->high_gain, play_mode->rate);
		memset(eq->low_val, 0, sizeof(eq->low_val));
		memset(eq->high_val, 0, sizeof(eq->high_val));
		return;
	}
	if(eq->low_gain != 0) {
		do_shelving_filter(buf, count, eq->low_coef, eq->low_val);
	}
	if(eq->high_gain != 0) {
		do_shelving_filter(buf, count, eq->high_coef, eq->high_val);
	}
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
