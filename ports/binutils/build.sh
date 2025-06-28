#!/bin/bash ../install.sh

NAME='binutils'
VERSION='2.44'
DOWNLOAD_URL="https://ftp.gnu.org/gnu/binutils/binutils-$VERSION.tar.gz#0cdd76777a0dfd3dd3a63f215f030208ddb91c2361d2bcc02acec0f1c16b6a2e"
DEPENDENCIES=('zlib')
MAKE_INSTALL_TARGETS=('install-strip')
CONFIGURE_OPTIONS=(
	"--target=$BANAN_TOOLCHAIN_TRIPLE"
	'--with-sysroot=/'
	"--with-build-sysroot=$BANAN_SYSROOT"
	'--enable-initfini-array'
	'--enable-shared'
	'--enable-lto'
	'--disable-nls'
	'--disable-werror'
)
