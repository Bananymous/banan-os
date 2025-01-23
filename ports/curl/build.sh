#!/bin/bash ../install.sh

NAME='curl'
VERSION='8.11.1'
DOWNLOAD_URL="https://curl.se/download/curl-$VERSION.tar.gz#a889ac9dbba3644271bd9d1302b5c22a088893719b72be3487bc3d401e5c4e80"
DEPENDENCIES=('ca-certificates' 'openssl' 'zlib' 'zstd')
CONFIGURE_OPTIONS=(
	'--disable-threaded-resolver'
	'--disable-ipv6'
	'--disable-docs'
	'--disable-ntlm'
	'--enable-optimize'
	'--with-openssl'
	'--with-zlib'
	'--with-zstd'
	'--without-libpsl'
	'--with-ca-bundle=/etc/ssl/certs/ca-certificates.crt'
	'--without-ca-path'
)

install() {
	make install DESTDIR="$BANAN_SYSROOT" || exit 1

	# remove libtool file
	rm -f $BANAN_SYSROOT/usr/lib/libcurl.la
}
