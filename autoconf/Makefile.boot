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

# $Id$
# $Author$
# Created at: Fri Jan 10 21:48:22 JST 2003

all: config.h.in automake configure

configure : configure.in aclocal.m4 config.h.in
	autoconf

aclocal.m4 : configure.in autoconf/alsa.m4 autoconf/arts.m4 autoconf/esd.m4 autoconf/gtk.m4 autoconf/ogg.m4 autoconf/utils.m4 autoconf/vorbis.m4
	aclocal -I ./autoconf

config.h.in : configure.in aclocal.m4
	autoheader

automake : aclocal.m4 configure.in Makefile.in autoconf/Makefile.in configs/Makefile.in doc/Makefile.in doc/C/Makefile.in doc/ja_JP.ujis/Makefile.in interface/Makefile.in interface/bitmaps/Makefile.in interface/motif_bitmaps/Makefile.in interface/pixmaps/Makefile.in libarc/Makefile.in libunimod/Makefile.in script/Makefile.in timidity/Makefile.in utils/Makefile.in

Makefile.in : Makefile.am
	automake Makefile

autoconf/Makefile.in : autoconf/Makefile.am
	automake autoconf/Makefile

configs/Makefile.in : configs/Makefile.am
	automake configs/Makefile

doc/Makefile.in : doc/Makefile.am
	automake doc/Makefile

doc/C/Makefile.in : doc/C/Makefile.am
	automake doc/C/Makefile

doc/ja_JP.ujis/Makefile.in : doc/ja_JP.ujis/Makefile.am
	automake doc/ja_JP.ujis/Makefile

interface/Makefile.in : interface/Makefile.am
	automake interface/Makefile

interface/bitmaps/Makefile.in : interface/bitmaps/Makefile.am
	automake interface/bitmaps/Makefile

interface/motif_bitmaps/Makefile.in : interface/motif_bitmaps/Makefile.am
	automake interface/motif_bitmaps/Makefile

interface/pixmaps/Makefile.in : interface/pixmaps/Makefile.am
	automake interface/pixmaps/Makefile

libarc/Makefile.in : libarc/Makefile.am
	automake libarc/Makefile

libunimod/Makefile.in : libunimod/Makefile.am
	automake libunimod/Makefile

script/Makefile.in : script/Makefile.am
	automake script/Makefile

timidity/Makefile.in : timidity/Makefile.am
	automake timidity/Makefile

utils/Makefile.in : utils/Makefile.am
	automake utils/Makefile
