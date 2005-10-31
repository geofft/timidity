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

AM_CXXFLAGS = $(CFLAGS)

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libarc \
	-I$(top_srcdir)/libunimod \
	-I$(top_srcdir)/interface \
	-I$(top_srcdir)/utils \
	-I$(top_srcdir)/timidity \
	$(EXTRAINCS)

if WINDRV
noinst_SCRIPTS = timiditydrv.dll
noinst_LIBRARIES = libtimiditydrv.a
endif

libtimiditydrv_a_SOURCES = \
	timiditydrv.idl \
	timiditydrv.cpp \
	timiwp_timidity.h \
	timiwp_timidity.c \
	timiditydrv.def 

SYSEXTRAS: timiditydrv.h

EXTRA_DIST = \
	 oemsetup.inf \
	 timiditydrv.inf \
	 dlldata.c \
	 timiditydrv_i.c \
	 timiditydrv_p.c \
	 timiditydrv.h

CLEANFILES = timiditydrv.tlb


libtimiditydrv_a_LIBADD = \
	$(top_builddir)/timidity/timidity.exe

libtimiditydrv_a_dep = \
	$(top_builddir)/timidity/timidity.exe 	


DRV_LIBS =  \
#	kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib \
#	advapi32.lib shell32.lib ole32.lib oleaut32.lib odbc32.lib odbccp32.lib \
#	ksguid.lib ksuser.lib

if VCPP
VC_LINK = link.exe -dll -def:timiditydrv.def -implib:timiditydrv.lib -out:timiditydrv.dll

timiditydrv.dll: libtimiditydrv.a $(libtimiditydrv_a_OBJECTS) $(libtimiditydrv_a_DEPENDENCIES) $(libtimiditydrv_a_dep)
	@rm -f libtimi_p_dll.a
	$(VC_LINK) $(VCPP_LDFLAGS) $(libtimiditydrv_a_OBJECTS) $(libtimiditydrv_a_LIBADD) $(LIBS) $(DRV_LIBS) 
else
if MINGWGCC
timiditydrv.dll: libtimiditydrv.a $(libtimiditydrv_a_OBJECTS) $(libtimiditydrv_a_DEPENDENCIES) $(libtimiditydrv_a_dep)
	@rm -f libtimi_p_dll.a
	gcc -shared -def-file timiditydrv.def -Wl,--dll -Wl,--enable-stdcall-fixup -Wl,--out-implib libtimiditydrv.a -o timiditydrv.dll $(libtimiditydrv_a_OBJECTS) $(libtimiditydrv_a_LIBADD) \
	$(top_builddir)/interface/libinterface.a $(top_builddir)/utils/libutils.a $(top_builddir)/libunimod/libunimod.a \
	$(top_builddir)/libarc/libarc.a $(LIBS) $(DRV_LIBS) 
endif
#    MINGWGCC
endif
#    VCPP

libtimiditydrv.a: $(libtimiditydrv_a_OBJECTS) $(libtimiditydrv_a_DEPENDENCIES) $(libtimiditydrv_a_dep)
	touch $@ 

if VCPP
timiditydrv_i.c timiditydrv_p.c timiditydrv.h:timiditydrv.idl
	midl -nologo timiditydrv.idl 
else
if BORLANDC
# BOLANDC not Work
#timiditydrv_i.c timiditydrv_p.c timiditydrv.h:timiditydrv.idl
#	midl -cpp_cmd cpp32.exe -cpp_opt "-P- -oCON" timiditydrv.idl
else
if MINGWGCC
# Mingw GCC not Work
#timiditydrv_i.c timiditydrv_p.c timiditydrv.h:timiditydrv.idl
#	midl -cpp_cmd cpp -cpp_opt --traditional timiditydrv.idl
endif
#     MINGWGCC
endif
#     BORLANDC
endif
#     VCPP

objs: $(libtimiditydrv_a_OBJECTS) $(libtimiditydrv_a_DEPENDENCIES) 
	@:

timiditydrv.obj: timiditydrv.cpp timiditydrv.h
        source='$<' object='$@' libtool=no \
        depfile='$(DEPDIR)/$*.Po' tmpdepfile='$(DEPDIR)/$*.TPo' \
        $(CXXDEPMODE) $(depcomp) \
        $(CXXCOMPILE) -c -o $@ `$(CYGPATH_W) '$<'`


uninstall-info-am:

clean:
	-rm -f *.o *.a *.dll *.obj *.RES *.idb *.pch *.lib

