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
#
# This Makefile is known to work with GNU make.

SUBDIRS = \
	utils \
	libarc \
	interface \
	libunimod \
	timidity \
	doc \
	script

EXTRA_DIST = \
	autoconf \
	AUTHORS \
	COPYING \
	ChangeLog \
	ChangeLog.1 \
	INSTALL \
	INSTALL.jp \
	NEWS \
	README \
	README.jp \
	timidity.ide \
	timidity.mak \
	TiMidity-uj.ad \
	TiMidity.ad \
	bootstrap.sh \
	configs

TOOLSDIR = timidity-tools
UMPDIR = ump

dynamics:
	cd interface; $(MAKE) $@

install.man install.manj:
	cd doc; $(MAKE) $@

install.tk:
	cd interface; $(MAKE) $@

install.el:
	cd interface; $(MAKE) $@

install.bin:
	cd timidity; $(MAKE) install

dist-hook:
	CONFIG_HEADERS='' CONFIG_FILES=configs/msc-config.h $(SHELL) ./config.status
	cp -r configs $(distdir)
	chmod -R a+r $(distdir)
	$(AMTAR) chof - $(distdir) | bzip2 -9 - > $(distdir).tar.bz2

##
## Targets for timidity-tools
##
tools:: tools-targets
	cd $(TOOLSDIR); $(MAKE)

tools-targets: $(TOOLSDIR)/Makefile

install.tools:
	cd $(TOOLSDIR); $(MAKE) install

$(TOOLSDIR)/Makefile: $(TOOLSDIR)/Makefile.in config.status
	CONFIG_HEADERS='' CONFIG_FILES=$(TOOLSDIR)/Makefile $(SHELL) ./config.status

## for developers
#$(TOOLSDIR)/Makefile.in: $(TOOLSDIR)/Makefile.am
#	$(AUTOMAKE) --gnu $(TOOLSDIR)/Makefile


##
## Targets for UMP
##
ump: ump-targets
	cd $(UMPDIR); $(MAKE) ump

ump-targets: $(UMPDIR)/Makefile

$(UMPDIR)/Makefile: $(UMPDIR)/Makefile.in config.status
	CONFIG_HEADERS='' CONFIG_FILES=ump/Makefile $(SHELL) ./config.status

## for developers
#$(UMPDIR)/Makefile.in: $(UMPDIR)/Makefile.am
#	$(AUTOMAKE) --gnu $(UMPSDIR)/Makefile
