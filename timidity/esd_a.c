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

    esd_a.c by Avatar <avatar@deva.net>
    based on linux_a.c

    Functions to play sound through EsounD
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>

#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <esd.h>


void *libesd = NULL;
int (*_esd_play_stream_fallback)(esd_format_t, int, const char *, const char *) = NULL;
void (*_esd_audio_flush)() = NULL;

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

#define dpm esd_play_mode

PlayMode dpm = {
    DEFAULT_RATE,
    PE_16BIT|PE_SIGNED,
    PF_PCM_STREAM/*|PF_CAN_TRACE*/,
    -1,
    {0}, /* default: get all the buffer fragments you can */
    "Enlightened sound daemon", 'e',
    "/dev/dsp",
    open_output,
    close_output,
    output_data,
    acntl
};


/*************************************************************************/
/* We currently only honor the PE_MONO bit, and the sample rate. */

static int open_output(void)
{
    int fd, tmp, i, warnings = 0;
    int include_enc, exclude_enc;
    esd_format_t esdformat;

    if(!libesd) {
	    libesd=dlopen("/usr/lib/libesd.so.0", RTLD_LAZY);
	    if(!libesd) {
		    fputs("Can't dlopen libesd", stderr);
		    return -1;
	    }
	    _esd_play_stream_fallback=dlsym(libesd, "esd_play_stream_fallback");
	    _esd_audio_flush=dlsym(libesd, "esd_audio_flush");
    }
    
    include_enc = 0;
    exclude_enc = PE_ULAW|PE_ALAW|PE_BYTESWAP; /* They can't mean these */
    if(dpm.encoding & PE_16BIT)
	include_enc |= PE_SIGNED;
    else
	exclude_enc |= PE_SIGNED;
    dpm.encoding = validate_encoding(dpm.encoding, include_enc, exclude_enc);

    /* Open the audio device */
    esdformat = (dpm.encoding & PE_16BIT) ? ESD_BITS16 : ESD_BITS8;
    esdformat |= (dpm.encoding & PE_MONO) ? ESD_MONO : ESD_STEREO;
    fd = _esd_play_stream_fallback(esdformat,dpm.rate,NULL,"timidity");
    if(fd < 0)
    {
	ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "%s: %s",
		  dpm.name, strerror(errno));
	return -1;
    }


    dpm.fd = fd;

    return warnings;
}

static int output_data(char *buf, int32 nbytes)
{
    int n;

    //    write(1, buf, nbytes);return 0;

    while(nbytes > 0)
    {
	if((n = write(dpm.fd, buf, nbytes)) == -1)
	{
	    ctl->cmsg(CMSG_WARNING, VERB_VERBOSE,
		      "%s: %s", dpm.name, strerror(errno));
	    if(errno == EWOULDBLOCK)
	    {
		/* It is possible to come here because of bug of the
		 * sound driver.
		 */
		continue;
	    }
	    return -1;
	}
	buf += n;
	nbytes -= n;
    }

    return 0;
}

static void close_output(void)
{
    if(libesd) {
	dlclose(libesd);
	libesd=NULL;
    }
    if(dpm.fd == -1)
	return;
    close(dpm.fd);
    dpm.fd = -1;
}

static int acntl(int request, void *arg)
{
    switch(request)
    {
      case PM_REQ_DISCARD:
        _esd_audio_flush();
	return 0;

      case PM_REQ_RATE: {
	  /* not implemented yet */
          break;
	}
    }
    return -1;
}
