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
	-I$(top_srcdir) \
	-I$(top_srcdir)/timidity \
	-I$(top_srcdir)/utils \
	-I$(top_srcdir)/libarc \
	$(EXTRAINCS)

if VCPP
noinst_DATA = unimod.lib
else
if BORLANDC
noinst_DATA = unimod.lib
else
noinst_LIBRARIES = libunimod.a
endif
endif

libunimod_a_SOURCES = \
	load_669.c \
	load_amf.c \
	load_dsm.c \
	load_far.c \
	load_gdm.c \
	load_imf.c \
	load_it.c \
	load_m15.c \
	load_med.c \
	load_mod.c \
	load_mtm.c \
	load_okt.c \
	load_s3m.c \
	load_stm.c \
	load_stx.c \
	load_ult.c \
	load_uni.c \
	load_xm.c \
	mloader.c \
	mlutil.c \
	mmsup.c \
	munitrk.c \
	unimod.h \
	unimod_priv.h


unimod_OBJS = \
	load_669.$(OBJEXT) \
	load_amf.$(OBJEXT) \
	load_dsm.$(OBJEXT) \
	load_far.$(OBJEXT) \
	load_gdm.$(OBJEXT) \
	load_imf.$(OBJEXT) \
	load_it.$(OBJEXT) \
	load_m15.$(OBJEXT) \
	load_med.$(OBJEXT) \
	load_mod.$(OBJEXT) \
	load_mtm.$(OBJEXT) \
	load_okt.$(OBJEXT) \
	load_s3m.$(OBJEXT) \
	load_stm.$(OBJEXT) \
	load_stx.$(OBJEXT) \
	load_ult.$(OBJEXT) \
	load_uni.$(OBJEXT) \
	load_xm.$(OBJEXT) \
	mloader.$(OBJEXT) \
	mlutil.$(OBJEXT) \
	mmsup.$(OBJEXT) \
	munitrk.$(OBJEXT) 

if VCPP	
unimod.lib: $(unimod_OBJS)
	link -lib $(unimod_OBJS) -out:$@
endif
if BORLANDC
unimod.lib: $(unimod_OBJS)
	for foo in  $(unimod_OBJS);do \
	  tlib $@ +$$foo; \
	done	
endif
clean:
	rm -f *.$(OBJEXT)
	rm -f *.$(so)
	rm -f *.a
	rm -f *.lib
