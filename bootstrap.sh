#!/bin/sh
# bootstrap script: use me instead of autoreconf
if [ "x$1" = "x-f" ]; then
    
    set -x;

    # Force re-construct the whole autotools.
    # May take much time than usual.

    # for Mac OS X + Fink:
    #aclocal -I /usr/share/aclocal -I /sw/share/aclocal -I /sw/share/aclocal-1.6 -I autoconf \
       aclocal -I /usr/share/aclocal -I autoconf \
    && autoheader \
    && for dir in \
	. \
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

    set -x;

    # just touch. You need it, don't you?
    for file in \
	./aclocal.m4 \
	./config.h.in \
	./Makefile.in \
	./doc/Makefile.in \
	./doc/C/Makefile.in \
	./doc/ja_JP.ujis/Makefile.in \
	./interface/Makefile.in \
	./interface/bitmaps/Makefile.in \
	./interface/motif_bitmaps/Makefile.in \
	./interface/pixmaps/Makefile.in \
	./libarc/Makefile.in \
	./libunimod/Makefile.in \
	./script/Makefile.in \
	./timidity/Makefile.in \
	./utils/Makefile.in \
	./configure; 
    do
        touch $file;
    done
  
fi
