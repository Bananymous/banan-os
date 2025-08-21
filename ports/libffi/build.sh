#!/bin/bash ../install.sh

NAME='libffi'
VERSION='3.5.2'
DOWNLOAD_URL="https://github.com/libffi/libffi/releases/download/v$VERSION/libffi-$VERSION.tar.gz#f3a3082a23b37c293a4fcd1053147b371f2ff91fa7ea1b2a52e335676bac82dc"
CONFIG_SUB=('config.sub')

post_install() {
	# remove libtool file
	rm -f $BANAN_SYSROOT/usr/lib/libffi.la
}
