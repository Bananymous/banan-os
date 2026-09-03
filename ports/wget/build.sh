#!/bin/bash ../install.sh

NAME='wget'
VERSION='1.25.0'
DOWNLOAD_URL="https://ftpmirror.gnu.org/gnu/wget/wget-$VERSION.tar.gz#766e48423e79359ea31e41db9e5c289675947a7fcf2efdcedb726ac9d0da3784"
DEPENDENCIES=('openssl' 'pcre2' 'zlib')
CONFIG_SUB=('build-aux/config.sub')
CONFIGURE_OPTIONS=(
	'--disable-nls'
	'--disable-ipv6'
	'--with-ssl=openssl'
	'CFLAGS=-Dpthread_self=pthread_self'
)

pre_configure() {
	echo '#include_next <sys/types.h>' > lib/sys_types.in.h
}
