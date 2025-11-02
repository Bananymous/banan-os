#!/bin/bash ../install.sh

NAME='libogg'
VERSION='1.3.6'
DOWNLOAD_URL="https://github.com/xiph/ogg/releases/download/v$VERSION/libogg-$VERSION.tar.gz#83e6704730683d004d20e21b8f7f55dcb3383cdf84c0daedf30bde175f774638"
CONFIG_SUB=('config.sub')

post_install() {
	# remove libtool file
	rm -f $BANAN_SYSROOT/usr/lib/libogg.la
}
