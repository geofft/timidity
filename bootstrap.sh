#!/bin/sh
# bootstrap script: use me instead of autoreconf

set -x

# for Mac OS X + Fink:
#aclocal -I /usr/share/aclocal -I /sw/share/aclocal -I /sw/share/aclocal-1.6 -I autoconf
aclocal -I /usr/share/aclocal -I autoconf \
&& autoheader \
&& for l in . doc doc/C doc/ja_JP.ujis interface interface/bitmaps interface/motif_bitmaps interface/pixmaps libarc libunimod script timidity utils; do \
      automake --gnu $l/Makefile; \
done \
&& autoconf
