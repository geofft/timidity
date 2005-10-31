/*
    TiMidity++ wrapper dll
    Copyright (C) 2005 Keishi Suenaga <s_keishi@mutt.freemail.ne.jp>

    This program is free software; you can redistribute it and/or modify
    it under the terms timip_of the GNU General Public License as published by
    the Free Software Foundation; either version 2 timip_of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty timip_of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy timip_of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef __TIMIWP_TIMIDITY_H__
#define __TIMIWP_TIMIDITY_H__


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#include "config.h"
#include "sysdep.h"
	
int timiwp_main_ini(int argc, char **argv);
int timiwp_main_close(void);

int timiwp_play_main_ini(int nfiles, char **files);
int timiwp_play_main_close (void);

void rtsyn_init(void);
void rtsyn_stop_playing(void);
void rtsyn_close(void);
void rtsyn_reset(void);
int rtsyn_play_one_data (int port, int32 dwParam1, double event_time);
void rtsyn_play_one_sysex (char *sysexbuffer, int exlen, double event_time );
void rtsyn_play_calculate(void);
	
double get_current_calender_time(void);

#ifdef __cplusplus
}
#endif
/* __cplusplus */
	
#endif
/* __TIMIWP_TIMIDITY_H__ */

