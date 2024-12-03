#!/bin/bash ../install.sh

NAME='curl'
VERSION='8.8.0'
DOWNLOAD_URL="https://curl.se/download/curl-$VERSION.tar.gz#77c0e1cd35ab5b45b659645a93b46d660224d0024f1185e8a95cdb27ae3d787d"
DEPENDENCIES=('ca-certificates' 'openssl' 'zlib')
CONFIGURE_OPTIONS=(
	'--disable-threaded-resolver'
	'--disable-ipv6'
	'--disable-docs'
	'--disable-ntlm'
	'--with-openssl'
	'--with-zlib'
	'--with-ca-bundle=/etc/ssl/certs/ca-certificates.crt'
	'--without-ca-path'
)

install() {
	make install DESTDIR="$BANAN_SYSROOT" || exit 1
	rm -f $BANAN_SYSROOT/usr/lib/libcurl.la
}
