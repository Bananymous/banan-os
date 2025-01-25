#!/bin/bash ../install.sh

NAME='binutils'
VERSION='2.39'
DOWNLOAD_URL="https://ftp.gnu.org/gnu/binutils/binutils-$VERSION.tar.gz#d12ea6f239f1ffe3533ea11ad6e224ffcb89eb5d01bbea589e9158780fa11f10"
DEPENDENCIES=('zlib')
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

build() {
	# This file is not even used. binutils just requires it to exist
	touch "$BANAN_SYSROOT/usr/include/memory.h"

	make -j$(nproc) || exit 1
	find . -type f -executable -exec strip --strip-unneeded {} + 2>/dev/null
}
