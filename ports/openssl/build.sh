#!/bin/bash ../install.sh

NAME='openssl'
VERSION='3.6.0'
DOWNLOAD_URL="https://github.com/openssl/openssl/releases/download/openssl-$VERSION/openssl-$VERSION.tar.gz#b6a5f44b7eb69e3fa35dbf15524405b44837a481d43d81daddde3ff21fcbb8e9"
DEPENDENCIES=('ca-certificates' 'zlib')
MAKE_INSTALL_TARGETS=('install_sw' 'install_ssldirs')

configure() {
	./Configure --prefix=/usr --openssldir=/etc/ssl -DOPENSSL_USE_IPV6=0 no-asm no-tests banan_os-generic threads zlib
}

post_install() {
	rm -f "$BANAN_SYSROOT/etc/ssl/certs"/*

	ln -svf "../cacert/cacert.pem" "$BANAN_SYSROOT/etc/ssl/cert.pem"
	ln -svf "../../cacert/cacert.pem" "$BANAN_SYSROOT/etc/ssl/certs/ca-certificates.crt"
	ln -svf "../../cacert/cacert.pem" "$BANAN_SYSROOT/etc/ssl/certs/ca-bundle.crt"

	openssl rehash "$BANAN_SYSROOT/etc/cacert/extracted"
	find "$BANAN_SYSROOT/etc/cacert/extracted" -type l -print0 |
	while IFS= read -r -d '' link; do
		ln -s "../../cacert/extracted/$(readlink "$link")" "$BANAN_SYSROOT/etc/ssl/certs/${link##*/}"
		rm "$link"
	done
}
