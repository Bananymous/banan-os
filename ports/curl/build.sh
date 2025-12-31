#!/bin/bash ../install.sh

NAME='curl'
VERSION='8.17.0'
DOWNLOAD_URL="https://curl.se/download/curl-$VERSION.tar.xz#955f6e729ad6b3566260e8fef68620e76ba3c31acf0a18524416a185acf77992"
DEPENDENCIES=('ca-certificates' 'openssl' 'zlib' 'zstd')
CONFIG_SUB=('config.sub')
CONFIGURE_OPTIONS=(
	'--disable-threaded-resolver'
	'--disable-ipv6'
	'--disable-docs'
	'--disable-ntlm'
	'--disable-static'
	'--enable-optimize'
	'--with-openssl'
	'--with-zlib'
	'--with-zstd'
	'--without-libpsl'
	'--with-ca-bundle=/etc/ssl/certs/ca-certificates.crt'
	'--without-ca-path'
)
