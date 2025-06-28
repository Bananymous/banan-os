#!/bin/bash ../install.sh

NAME='libtiff'
VERSION='4.7.0'
DOWNLOAD_URL="https://download.osgeo.org/libtiff/tiff-$VERSION.tar.gz#67160e3457365ab96c5b3286a0903aa6e78bdc44c4bc737d2e486bcecb6ba976"
TAR_CONTENT="tiff-$VERSION"
DEPENDENCIES=('zlib' 'zstd' 'libjpeg')

post_install() {
	# remove libtool files
	rm -f $BANAN_SYSROOT/usr/lib/libtiff.la
	rm -f $BANAN_SYSROOT/usr/lib/libtiffxx.la
}
