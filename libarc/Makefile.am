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

include $(top_builddir)/common.makefile

INCLUDES = \
	-I$(top_srcdir)/timidity \
	-I$(top_srcdir)/utils \
	$(EXTRAINCS)


noinst_LIBRARIES = libarc.a

libarc_a_SOURCES = \
	arc.c \
	arc.h \
	arc_lzh.c \
	arc_mime.c \
	arc_tar.c \
	arc_zip.c \
	deflate.c \
	explode.c \
	explode.h \
	inflate.c \
	unlzh.c \
	unlzh.h \
	url.c \
	url.h \
	url_b64decode.c \
	url_buff.c \
	url_cache.c \
	url_dir.c \
	url_file.c \
	url_hqxdecode.c \
	url_inflate.c \
	url_mem.c \
	url_pipe.c \
	url_qsdecode.c \
	url_uudecode.c \
	zip.h

EXTRA_libarc_a_SOURCES = \
	url_http.c \
	url_ftp.c \
	url_news.c \
	url_newsgroup.c

if ENABLE_NETWORK
NET_OBJS = \
	url_http.o \
	url_ftp.o \
	url_news.o \
	url_newsgroup.o
endif

libarc_a_LIBADD = $(NET_OBJS)
