# TiMidity++ -- MIDI to WAVE converter and player
# Copyright (C) 1999-2002 Masanao Izumo <mo@goice.co.jp>
# Copyright (C) 1995 Tuukka Toivonen <tt@cgs.fi>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

include $(top_srcdir)/common.makefile.in

INCLUDES = \
	-I$(top_srcdir)/libarc \
	-I$(top_srcdir)/timidity

noinst_LIBRARIES = libutils.a

libutils_a_SOURCES = \
	bitset.c \
	bitset.h \
	fft4g.c \
	fft4g.h \
	mblock.c \
	mblock.h \
	memb.c \
	memb.h \
	nkflib.c \
	nkflib.h \
	strtab.c \
	strtab.h \
	support.c \
	support.h \
	timer.c \
	timer.h

EXTRA_libutils_a_SOURCES = \
	fft.c \
	fft.h \
	support.c \
	net.c \
	net.h \
	mac_util.c \
	mac_util.h \
	mac_readdir.c \
	mac_readdir.h \
	readdir.h \
	readdir_win.c

if ENABLE_NETWORK
NET_OBJS = net.o
endif

if ENABLE_SOUND_SPEC
SOUND_SPEC_OBJS = fft.o
endif

if W32READDIR
W32READDIR_OBJS = readdir_win.o
endif

libutils_a_LIBADD = \
	$(NET_OBJS) \
	$(W32READDIR_OBJS) \
	$(SOUND_SPEC_OBJS)
