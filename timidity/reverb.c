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

/* delay buffers @65kHz */
#define REV_BUF0       344
#define REV_BUF1       684
#define REV_BUF2      2868
#define REV_BUF3      1368

#define REV_VAL0         5.3
#define REV_VAL1        10.5
#define REV_VAL2        44.12
#define REV_VAL3        21.0

#define REV_INP_LEV      0.55
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
    if(output_rate < 4000)  output_rate=4000;

    def_rpt0 = rpt0 = REV_VAL0 * output_rate / 1000;
    def_rpt1 = rpt1 = REV_VAL1 * output_rate / 1000;
    def_rpt2 = rpt2 = REV_VAL2 * output_rate / 1000;
    def_rpt3 = rpt3 = REV_VAL3 * output_rate / 1000;
}


void set_ch_reverb(register int32 *sbuffer, int32 n, int level)
{
    register int32  i;

    FLOAT_T send_level = (FLOAT_T)level * (REV_INP_LEV/127.0);

    for(i = 0; i < n; i++)
    {
        direct_buffer[i] += sbuffer[i];
        effect_buffer[i] += sbuffer[i] * send_level;
    }
}


void do_ch_reverb(int32 *comp, int32 n)
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
        comp[i] = ta + EPFL + direct_buffer[i] * REV_INP_LEV;
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
        comp[i] = ta + EPFR + direct_buffer[i] * REV_INP_LEV;
        direct_buffer[i] = 0;

        rev_ptinc();
    }
}


void do_reverb(int32 *comp, int32 n)
{
    int32  fixp, s, t, i;

    for(i = 0; i < n; i++)
    {
        /* L */
        fixp = comp[i] * REV_INP_LEV;

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
        fixp = comp[++i] * REV_INP_LEV;

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


