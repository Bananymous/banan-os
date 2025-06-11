#!/bin/bash ../install.sh

NAME='libpng'
VERSION='1.6.48'
DOWNLOAD_URL="https://download.sourceforge.net/libpng/libpng-$VERSION.tar.gz#68f3d83a79d81dfcb0a439d62b411aa257bb4973d7c67cd1ff8bdf8d011538cd"
DEPENDENCIES=('zlib')

install() {
	make install DESTDIR="$BANAN_SYSROOT" || exit 1

	# remove libtool files
	rm -f $BANAN_SYSROOT/usr/lib/libpng.la
	rm -f $BANAN_SYSROOT/usr/lib/libpng16.la
}
