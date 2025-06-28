#!/bin/bash ../install.sh

NAME='libwebp'
VERSION='1.5.0'
DOWNLOAD_URL="https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-$VERSION.tar.gz#7d6fab70cf844bf6769077bd5d7a74893f8ffd4dfb42861745750c63c2a5c92c"
DEPENDENCIES=('libpng' 'libjpeg' 'libtiff')
CONFIGURE_OPTIONS=(
	"--with-pngincludedir=$BANAN_SYSROOT/usr/include"
	"--with-pnglibdir=$BANAN_SYSROOT/usr/lib"
)

post_install() {
	# remove libtool files
	rm -f $BANAN_SYSROOT/usr/lib/libwebp.la
	rm -f $BANAN_SYSROOT/usr/lib/libwebpdemux.la
	rm -f $BANAN_SYSROOT/usr/lib/libwebpmux.la
}
