#!/bin/bash ../install.sh

NAME='libsndfile'
VERSION='1.2.2'
DOWNLOAD_URL="https://github.com/libsndfile/libsndfile/releases/download/$VERSION/libsndfile-$VERSION.tar.xz#3799ca9924d3125038880367bf1468e53a1b7e3686a934f098b7e1d286cdb80e"
_DEPENDENCIES=('ca-certificates' 'openssl' 'zlib' 'zstd')
CONFIG_SUB=('build-aux/config.sub')
CONFIGURE_OPTIONS=(
	'CFLAGS=-std=c11'
)

post_install() {
	# remove libtool file
	rm -f $BANAN_SYSROOT/usr/lib/libsndfile.la
}
