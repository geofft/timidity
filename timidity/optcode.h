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

#ifndef OPTCODE_H_INCLUDED
#define OPTCODE_H_INCLUDED 1

/* optimizing mode */
/* 0: none         */
/* 1: x86 asm      */
#define OPT_MODE 1

/* PowerPC's AltiVec enhancement */
/* 0: none                       */
/* 1: use altivec                */
/*    (need -faltivec option)    */
#ifndef USE_ALTIVEC
#define USE_ALTIVEC 0
#endif

/*****************************************************************************/

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif/* <sys/param.h> */
#ifdef HAVE_SYS_SYSCTL_H
#include <sys/sysctl.h>
#endif/* <sys/sysctl.h> */
#ifdef STDC_HEADERS
#include <string.h>
#elif HAVE_STRINGS_H
#include <strings.h>
#endif/* <string.h> */

#if __STDC_VERSION__ == 199901L
# include <stdbool.h>
#endif

/*****************************************************************************/
#if OPT_MODE == 1
#if defined(__GNUC__) && defined(__i386__)
static inline int32 imuldiv8(int32 a, int32 b)
{
    int32 result;
    __asm__("movl %1, %%eax\n\t"
	    "movl %2, %%edx\n\t"
	    "imull %%edx\n\t"
	    "shr $8, %%eax\n\t"
	    "shl $24, %%edx\n\t"
	    "or %%edx, %%eax\n\t"
	    "movl %%eax, %0\n\t"
	    : "=g"(result)
	    : "g"(a), "g"(b)
	    : "eax", "edx");
    return result;
}

static inline int32 imuldiv16(int32 a, int32 b)
{
    int32 result;
    __asm__("movl %1, %%eax\n\t"
	    "movl %2, %%edx\n\t"
	    "imull %%edx\n\t"
	    "shr $16, %%eax\n\t"
	    "shl $16, %%edx\n\t"
	    "or %%edx, %%eax\n\t"
	    "movl %%eax, %0\n\t"
	    : "=g"(result)
	    : "g"(a), "g"(b)
	    : "eax", "edx");
    return result;
}
#elif _MSC_VER
inline int32 imuldiv8(int32 a, int32 b) {
	_asm {
		mov eax, a
		mov edx, b
		imul edx
		shr eax, 8
		shl edx, 24
		or  eax, edx
	}
}

inline int32 imuldiv16(int32 a, int32 b) {
	_asm {
		mov eax, a
		mov edx, b
		imul edx
		shr eax, 16
		shl edx, 16
		or  eax, edx
	}
}
#else
/* generic */
int32 imuldiv8(int32 a, int32 b);
int32 imuldiv16(int32 a, int32 b);
#endif /* architectures */
#endif /* OPT_MODE != 0 */

/*****************************************************************************/
#if USE_ALTIVEC

#ifndef __bool_true_false_are_defined
#define bool _Bool
typedef enum { false = 0, true = 1 } bool;
#endif /* C99 Hack */

/* typedefs */
typedef vector signed int  vint32;
typedef vector signed char vint8;

/* prototypes */
void v_memset(void* dest, int c, size_t len);
void v_memzero(void* dest, size_t len);
void v_set_dry_signal(void* dest, const int32* buf, int32 n);

/* inline functions */
extern inline bool is_altivec_available(void)
{
  int sel[2] = { CTL_HW, HW_VECTORUNIT };
  int has_altivec = false;
  size_t len = sizeof(has_altivec);
  int error = sysctl(sel, 2, &has_altivec, &len, NULL, 0);
  if(!error) {
    return (bool)!!has_altivec;
  } else {
    return false;
  }
}

extern inline void libc_memset(void* destp, int c, size_t len)
{
    memset(destp,c,len);
}

static inline void* switch_memset(void* destp, int c, size_t len)
{
    void* keepdestp = destp;
    if(!is_altivec_available()) {
        libc_memset(destp,c,len);
    } else if (c) {
        v_memset(destp,c,len);
    } else {
        v_memzero(destp,len);
    }
    return keepdestp;
}

#define memset switch_memset
#endif /* altivec */

#endif /* OPTCODE_H_INCLUDED */
