#!/bin/sh
# TiMidity++ -- MIDI to WAVE converter and player
# Copyright (C) 1999-2003 Masanao Izumo <is@onicos.co.jp>
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
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
# USA

# bootstrap script: use me instead of autoreconf
if [ "x$1" = "x-f" -o "x$1" = "x--force" ]; then

    set -x;

    # Force re-construct the whole autotools.
    # May take much time than usual.

    # for Mac OS X + Fink:
    #aclocal -I /usr/share/aclocal -I /sw/share/aclocal -I /sw/share/aclocal-1.6 -I autoconf \
       aclocal -I /usr/share/aclocal -I autoconf \
    && autoheader \
    && for dir in \
	. \
	autoconf \
	config \
	doc \
	doc/C \
	doc/ja_JP.ujis \
	interface \
	interface/bitmaps \
	interface/motif_bitmaps \
	interface/pixmaps \
	libarc \
	libunimod \
	script \
	timidity \
	utils; \
       do \
	automake --gnu $dir/Makefile; \
       done \
    && autoconf;

else
    make -f autoconf/Makefile.boot
fi
