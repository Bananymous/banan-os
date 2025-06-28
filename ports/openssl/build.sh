#!/bin/bash ../install.sh

NAME='openssl'
VERSION='3.3.1'
DOWNLOAD_URL="https://github.com/openssl/openssl/releases/download/openssl-$VERSION/openssl-$VERSION.tar.gz#777cd596284c883375a2a7a11bf5d2786fc5413255efab20c50d6ffe6d020b7e"
DEPENDENCIES=('zlib')
MAKE_INSTALL_TARGETS=('install_sw' 'install_ssldirs')

configure() {
	./Configure --prefix=/usr --openssldir=/etc/ssl -DOPENSSL_USE_IPV6=0 no-asm no-tests banan_os-generic threads zlib
}
