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

    aRts_a.c by Peter L Jones <peter@drealm.org.uk>
    based on esd_a.c

    Functions to play sound through aRts
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <artsc.h>

#include "timidity.h"
#include "common.h"
#include "output.h"
#include "controls.h"
#include "timer.h"
#include "instrum.h"
#include "playmidi.h"
#include "miditrace.h"

static int open_output(void); /* 0=success, 1=warning, -1=fatal error */
static void close_output(void);
static int output_data(char *buf, int32 nbytes);
static int acntl(int request, void *arg);

/* export the playback mode */

#define dpm arts_play_mode

PlayMode dpm = {
    /*rate*/         DEFAULT_RATE,
    /*encoding*/     PE_16BIT|PE_SIGNED,
    /*flag*/         PF_PCM_STREAM/*|PF_BUFF_FRAGM_OPT/**/,
    /*fd*/           -1,
    /*extra_param*/  {0}, /* default: get all the buffer fragments you can */
    /*id*/           "aRts",
    /*id char*/      'R',
    /*name*/         "arts",
    open_output,
    close_output,
    output_data,
    acntl
};


/*************************************************************************/
/* We currently only honor the PE_MONO bit, and the sample rate. */

static int open_output(void)
{
    arts_stream_t fd = 0;
    int i, include_enc, exclude_enc;

    include_enc = 0;
    exclude_enc = PE_ULAW|PE_ALAW|PE_BYTESWAP; /* They can't mean these */
    if(dpm.encoding & PE_16BIT)
	include_enc |= PE_SIGNED;
    else
	exclude_enc |= PE_SIGNED;
    dpm.encoding = validate_encoding(dpm.encoding, include_enc, exclude_enc);

    /* Open the audio device */
    if((i = arts_init()) != 0)
    {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s",
		  dpm.name, arts_error_text(i));
	return -1;
    }
    fd = arts_play_stream(dpm.rate,
        (dpm.encoding & PE_16BIT) ? 16 : 8,
        (dpm.encoding & PE_MONO) ? 1 : 2,
        "timidity");
    dpm.fd = (int) fd;
    /* "this aRts function isnot yet implemented"
     *
    if (dpm.extra_param[0]) {
        i = arts_stream_set((arts_stream_t) dpm.fd,
            ARTS_P_PACKET_COUNT,
            dpm.extra_param[0]);
	if (i < 0) {
            ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s",
              dpm.name, arts_error_text(i));
	    return 1;
        }
    }
     *
     */
    return 0;
}

static int output_data(char *buf, int32 nbytes)
{
    int n;

    if (dpm.fd == -1) return -1;

    while(nbytes > 0)
    {
	if((n = arts_write((arts_stream_t) dpm.fd, buf, nbytes)) < 0)
	{
	    ctl->cmsg(CMSG_WARNING, VERB_VERBOSE,
		      "%s: %s", dpm.name, arts_error_text(n));
	    return -1;
	}
	buf += n;
	nbytes -= n;
    }

    return 0;
}

static void close_output(void)
{
    if(dpm.fd == -1)
	return;
    arts_close_stream((arts_stream_t) dpm.fd);
    arts_free();
    dpm.fd = -1;
}

static int acntl(int request, void *arg)
{
    int n;

    switch(request) {
      case PM_REQ_MIDI:
      case PM_REQ_INST_NAME:
      case PM_REQ_DISCARD:
      case PM_REQ_GETQSIZ:
      case PM_REQ_SETQSIZ:
      case PM_REQ_RATE:
      case PM_REQ_GETSAMPLES:
      case PM_REQ_GETFILLABLE:
      case PM_REQ_GETFILLED:
        return -1;
      case PM_REQ_FLUSH:
      case PM_REQ_PLAY_START:
      case PM_REQ_PLAY_END:
      case PM_REQ_OUTPUT_FINISH:
        break;
      case PM_REQ_GETFRAGSIZ:
	if (dpm.fd != -1) {
            n = arts_stream_get((arts_stream_t) dpm.fd, ARTS_P_PACKET_SIZE);
            if (n >= 0) {
                *(int *)arg = n;
                return 0;
            }
        }
        return -1;
    }
    return 0;
}
