#!/bin/bash ../install.sh

NAME='libiconv'
VERSION='1.18'
DOWNLOAD_URL="https://ftpmirror.gnu.org/gnu/libiconv/libiconv-$VERSION.tar.gz#3b08f5f4f9b4eb82f151a7040bfd6fe6c6fb922efe4b1659c66ea933276965e8"
CONFIG_SUB=('build-aux/config.sub' 'libcharset/build-aux/config.sub')
CONFIGURE_OPTIONS=(
	'--disable-nls'
	'CFLAGS=-std=c11'
)

pre_configure() {
	echo '#include_next <sys/types.h>' > srclib/sys_types.in.h
}
