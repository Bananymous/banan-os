#!/bin/bash ../install.sh

NAME='libjpeg'
VERSION='9f'
DOWNLOAD_URL="https://www.ijg.org/files/jpegsrc.v9f.tar.gz#04705c110cb2469caa79fb71fba3d7bf834914706e9641a4589485c1f832565b"
TAR_CONTENT="jpeg-$VERSION"

install() {
	make install DESTDIR="$BANAN_SYSROOT" || exit 1

	# remove libtool files
	rm -f $BANAN_SYSROOT/usr/lib/libjpeg.la
}
