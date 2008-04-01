#!/bin/sh
bar="AUTHORS Makefile.am COPYING Makefile.in NEWS interface.h.in ChangeLog 
    README common.makefile.in ChangeLog.1  README.ja config.h.in ChangeLog.2 
    TODO INSTALL TiMidity-uj.ad configure INSTALL.ja TiMidity.ad 
    configure.in "
bar="$bar `ls libarc/*|grep libarc/` `ls libunimod/*|grep libunimod/`  `ls utils/*|grep utils/`
  `ls timidity/*|grep timidity/` `ls windrv/*|grep windrv/` `ls interface/*|grep interface/`
  `ls doc/* doc/*/* doc/*/*/*|grep doc` "
 
 bar2=`echo $bar|perl -pe 's/(\s)(\S*\:)/$1/g' -|perl -pe 's/(\s)(\S*CVS\S*)/$1/g' -`

for foo in $bar2  ; do
	echo $foo
	cat $foo| perl -pe 's/\n$/\r\n/g' - | perl -pe 's/\r\r\n$/\r\n/g' - >$foo.tmp
	mv $foo.tmp $foo
done
