#!/bin/sh
#
# autogen.sh glue for timidity 2.12.x
# $Id: autogen.sh,v 1.11 2004/10/01 14:37:07 hmh Exp $
#
# Requires: automake, autoconf (newest versions), dpkg-dev
#
# Run with updateexec to update debian/executable.files
set -e

# Refresh GNU autotools toolchain.
echo "Running autotools..."
rm -rf autom4te.cache
aclocal-1.9 -I autoconf
autoheader
automake-1.9 --foreign --add-missing

# The automake package already links config.sub/guess to /usr/share/misc/
for i in config.guess config.sub missing install-sh mkinstalldirs depcomp ; do
	test -r /usr/share/automake-1.9/${i} && {
		rm -f "autoconf/${i}"
		cp -f "/usr/share/automake-1.9/${i}" autoconf/
	}
	chmod 755 autoconf/${i}
done

autoconf

# For the Debian build
test -d debian && {
	# link these in Debian builds
	rm -f autoconf/config.sub autoconf/config.guess
	ln -s /usr/share/misc/config.sub autoconf/config.sub
	ln -s /usr/share/misc/config.guess autoconf/config.guess

	# refresh list of executable scripts, to avoid possible breakage if
	# upstream tarball does not include the file or if it is mispackaged
	# for whatever reason.
	[ "$1" == "updateexec" ] && {
		echo Generating list of executable files...
		rm -f debian/executable.files
		find -type f -perm +111 ! -name '.*' -fprint debian/executable.files
	}

	# Remove any files in upstream tarball that we don't have in the Debian
	# package (because diff cannot remove files)
	version=`dpkg-parsechangelog | awk '/Version:/ { print $2 }' | sed -e 's/-[^-]\+$//'`
	source=`dpkg-parsechangelog | awk '/Source:/ { print $2 }' | tr -d ' '`
	if test -r ../${source}_${version}.orig.tar.gz ; then
		echo Generating list of files that should be removed...
		rm -f debian/deletable.files
		touch debian/deletable.files
		mkdir debian/tmp
		cd debian/tmp
		tar -zxf ../../../${source}_${version}.orig.tar.gz
		cd ../..
		for i in $(find debian/tmp/ -type f ! -name '.*' -print0 | awk -F '\000' '{ print "\"" $0 "\"" }') ; do
		  if test -r "${i}" ; then
		     filename=$(echo "${i}" | sed -e 's#.*debian/tmp/[^/]\+/##')
		     test -r "${filename}" || echo "${filename}" >>debian/deletable.files
		  fi
		done
		rm -fr debian/tmp
	else
		echo Emptying list of files that should be deleted...
		rm -f debian/deletable.files
		touch debian/deletable.files
	fi
}

exit 0
