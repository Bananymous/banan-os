#!/bin/bash ../install.sh

NAME='openssl'
VERSION='3.6.5'
DOWNLOAD_URL="https://github.com/openssl/openssl/releases/download/openssl-$VERSION/openssl-$VERSION.tar.gz#a2157c2830efdec3788939b00c9b0638306d3f0bbb76dc4832ee503bb397df98"
DEPENDENCIES=('ca-certificates' 'zlib' 'zstd')
MAKE_INSTALL_TARGETS=('install_sw' 'install_ssldirs')

configure() {
	./Configure --prefix=/usr --openssldir=/etc/ssl -DOPENSSL_USE_IPV6=0 \
		no-docs no-tests shared enable-zlib enable-zstd threads banan_os-generic
}

post_install() {
	rm -f "$DESTDIR/usr/lib/libcrypto.a" "$DESTDIR/usr/lib/libssl.a"

	chmod 0700 "$DESTDIR/etc/ssl/private"

	rm -f "$DESTDIR/etc/ssl/certs"/*

	ln -svf "../cacert/cacert.pem" "$DESTDIR/etc/ssl/cert.pem"
	ln -svf "../../cacert/cacert.pem" "$DESTDIR/etc/ssl/certs/ca-certificates.crt"
	ln -svf "../../cacert/cacert.pem" "$DESTDIR/etc/ssl/certs/ca-bundle.crt"

	openssl rehash "$BANAN_SYSROOT/etc/cacert/extracted"
	find "$BANAN_SYSROOT/etc/cacert/extracted" -type l -print0 |
	while IFS= read -r -d '' link; do
		ln -sf "../../cacert/extracted/$(readlink "$link")" "$DESTDIR/etc/ssl/certs/${link##*/}"
		rm "$link"
	done
}
