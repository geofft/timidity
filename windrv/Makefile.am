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

AM_FLAGS = $(CFLAGS)

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
	timiditydrv.c \
	timiwp_timidity.h \
	timiwp_timidity.c \
	timiditydrv.def \
	mmddk.h

SYSEXTRAS: timiditydrv.h

EXTRA_DIST = \
	 timiditydrv.inf  \
	 timiditydrv.idl



if MINGWGCC
libtimiditydrv_a_LIBADD = \
	$(top_builddir)/timidity/timidity.exe \
	$(top_builddir)/interface/libinterface.a \
	$(top_builddir)/libarc/libarc.a \
	$(top_builddir)/libunimod/libunimod.a \
	$(top_builddir)/utils/libutils.a 
libtimiditydrv_a_dep = \
	$(top_builddir)/timidity/timidity.exe \
	$(top_builddir)/interface/libinterface.a \
	$(top_builddir)/libarc/libarc.a \
	$(top_builddir)/libunimod/libunimod.a \
	$(top_builddir)/utils/libutils.a 
else
if DMC
libtimiditydrv_a_LIBADD = \
	interface.lib \
	arc.lib \
	unimod.lib \
	utils.lib \
	timidity.lib
libtimiditydrv_a_dep = \
	$(top_builddir)/interface/interface.lib \
	$(top_builddir)/libarc/arc.lib \
	$(top_builddir)/libunimod/unimod.lib \
	$(top_builddir)/utils/utils.lib \
	$(top_builddir)/timidity/timidity.lib
else
if POCC
libtimiditydrv_a_LIBADD = \
	$(top_builddir)/interface/interface.lib \
	$(top_builddir)/libarc/arc.lib \
	$(top_builddir)/libunimod/unimod.lib \
	$(top_builddir)/utils/utils.lib \
	$(top_builddir)/timidity/timidity.lib
libtimiditydrv_a_dep = \
	$(top_builddir)/interface/interface.lib \
	$(top_builddir)/libarc/arc.lib \
	$(top_builddir)/libunimod/unimod.lib \
	$(top_builddir)/utils/utils.lib \
	$(top_builddir)/timidity/timidity.lib
else
libtimiditydrv_a_LIBADD = \
	$(top_builddir)/interface/interface.lib \
	$(top_builddir)/libarc/arc.lib \
	$(top_builddir)/libunimod/unimod.lib \
	$(top_builddir)/utils/utils.lib \
	$(top_builddir)/timidity/timidity.exe
libtimiditydrv_a_dep = \
	$(top_builddir)/interface/interface.lib \
	$(top_builddir)/libarc/arc.lib \
	$(top_builddir)/libunimod/unimod.lib \
	$(top_builddir)/utils/utils.lib \
	$(top_builddir)/timidity/timidity.exe
endif
#   POCC
endif
#    DMC
endif
#    MINGWGCC



DRV_LIBS =  \
	kernel32.lib winspool.lib comdlg32.lib advapi32.lib oleaut32.lib \
	uuid.lib odbc32.lib wsock32.lib user32.lib shell32.lib winmm.lib wsock32.lib
#	kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib \
#	advapi32.lib shell32.lib ole32.lib oleaut32.lib odbc32.lib odbccp32.lib \
#	ksguid.lib ksuser.lib

if VCPP
VC_LINK = link.exe -dll -def:timiditydrv.def -implib:timiditydrv.lib -out:timiditydrv.dll

timiditydrv.dll: libtimiditydrv.a $(libtimiditydrv_a_OBJECTS) $(libtimiditydrv_a_DEPENDENCIES) $(libtimiditydrv_a_dep)
	@rm -f $@
	$(VC_LINK) $(VCPP_LDFLAGS) $(libtimiditydrv_a_OBJECTS) $(libtimiditydrv_a_LIBADD) $(LIBS)
else
if POCC
POCC_LINK = polink.exe -dll -def:timiditydrv.def -implib:timiditydrv.lib -out:timiditydrv.dll

timiditydrv.dll: libtimiditydrv.a $(libtimiditydrv_a_OBJECTS) $(libtimiditydrv_a_DEPENDENCIES) $(libtimiditydrv_a_dep)
	@rm -f $@
	$(POCC_LINK) $(POCC_LDFLAGS) $(libtimiditydrv_a_OBJECTS) $(libtimiditydrv_a_LIBADD) $(LIBS)
else
if DMC
DMC_LINK = dmc.exe -WD
# -def:timiditydrv.def -implib:timiditydrv.lib -out:timiditydrv.dll

timiditydrv.dll: libtimiditydrv.a $(libtimiditydrv_a_OBJECTS) $(libtimiditydrv_a_dep)
	@rm -f $@ *.lib
	cp $(libtimiditydrv_a_dep) .
	$(DMC_LINK) $(DMC_LDFLAGS) $(libtimiditydrv_a_OBJECTS) $(libtimiditydrv_a_LIBADD) $(LIBS) timiditydrv.def -output $@
else
if BORLANDC
BORLANDC_LINK = ilink32.exe -Tpd
timiditydrv.dll: libtimiditydrv.a $(libtimiditydrv_a_OBJECTS) $(libtimiditydrv_a_DEPENDENCIES) $(libtimiditydrv_a_dep)
	@rm -f $@
	$(BORLANDC_LINK) C0D32.OBJ $(libtimiditydrv_a_OBJECTS) \
	$(top_builddir)\\interface\\interface.lib \
	$(top_builddir)\\libarc\\arc.lib \
	$(top_builddir)\\libunimod\\unimod.lib \
	$(top_builddir)\\utils\\utils.lib \
	$(top_builddir)\\timidity\\timidity.exe \
	$(DRV_LIBS),$@,,import32.lib cw32mti.lib,timiditydrv.def
else
if MINGWGCC
timiditydrv.dll: libtimiditydrv.a $(libtimiditydrv_a_OBJECTS) $(libtimiditydrv_a_DEPENDENCIES) $(libtimiditydrv_a_dep)
	@rm -f $@
	gcc -shared -def-file timiditydrv.def -Wl,--dll -Wl,--enable-stdcall-fixup -Wl,--out-implib libtimiditydrv.a -o $@ \
	$(libtimiditydrv_a_OBJECTS) $(libtimiditydrv_a_LIBADD) $(LIBS)
else
if WATCOM_C
WATCOM_LINK = link386.exe -dll -def:timiditydrv.def -implib:timiditydrv.lib -out:timiditydrv.dll
timiditydrv.dll: libtimiditydrv.a $(libtimiditydrv_a_OBJECTS) $(libtimiditydrv_a_DEPENDENCIES) $(libtimiditydrv_a_dep)
	@rm -f libtimi_p_dll.a
	$(WATCOM_LINK) $(libtimiditydrv_a_OBJECTS) $(libtimiditydrv_a_LIBADD) $(LIBS)
	
#WATCOM_LINK = wlink
#timiditydrv.dll: libtimiditydrv.a $(libtimiditydrv_a_OBJECTS) $(libtimiditydrv_a_DEPENDENCIES) $(libtimiditydrv_a_dep)
#	@rm -f $@
#	$(WATCOM_LINK) SYSTEM nt_dll initinstance terminstance \
#	`for foo in $(libtimiditydrv_a_OBJECTS) ; do echo "File $$foo "; done;` \
#	Name $@ \
#	Library $(top_builddir)\\interface\\interface.lib \
#	Library $(top_builddir)\\libarc\\arc.lib \
#	Library $(top_builddir)\\libunimod\\unimod.lib \
#	Library $(top_builddir)\\utils\\utils.lib \
#	Library $(top_builddir)\\timidity\\timidity.exe \
#	`for foo in  $(DRV_LIBS) ; do echo "Library $$foo"; done;` \
#	EXPort DllCanUnloadNow,DllGetClassObject,DllRegisterServer,DllUnregisterServer,DriverProc,modMessage
endif
#     WATCOM_C
endif
#    MINGWGCC
endif
#     BORLANDC
endif
#    DMC
endif
#    POCC
endif
#    VCPP

libtimiditydrv.a: $(libtimiditydrv_a_OBJECTS)  $(libtimiditydrv_a_dep)
	touch $@ 


objs: $(libtimiditydrv_a_OBJECTS) $(libtimiditydrv_a_DEPENDENCIES) 
	@:



uninstall-info-am:

clean:
	-rm -f *.o *.a *.dll *.obj *.RES  *.lib

