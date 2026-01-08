#!/bin/bash ../install.sh

NAME='openssl'
VERSION='3.6.0'
DOWNLOAD_URL="https://github.com/openssl/openssl/releases/download/openssl-$VERSION/openssl-$VERSION.tar.gz#b6a5f44b7eb69e3fa35dbf15524405b44837a481d43d81daddde3ff21fcbb8e9"
DEPENDENCIES=('zlib')
MAKE_INSTALL_TARGETS=('install_sw' 'install_ssldirs')

configure() {
	./Configure --prefix=/usr --openssldir=/etc/ssl -DOPENSSL_USE_IPV6=0 no-asm no-tests banan_os-generic threads zlib
}
