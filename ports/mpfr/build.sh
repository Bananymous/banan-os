#!/bin/bash ../install.sh

NAME='mpfr'
VERSION='4.2.2'
DOWNLOAD_URL="https://ftp.gnu.org/gnu/mpfr/mpfr-$VERSION.tar.gz#826cbb24610bd193f36fde172233fb8c009f3f5c2ad99f644d0dea2e16a20e42"
DEPENDENCIES=('gmp')
CONFIGURE_OPTIONS=(
	"--target=$BANAN_TOOLCHAIN_TRIPLE"
	'--with-sysroot=/'
)
