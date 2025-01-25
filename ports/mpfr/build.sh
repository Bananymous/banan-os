#!/bin/bash ../install.sh

NAME='mpfr'
VERSION='4.2.1'
DOWNLOAD_URL="https://ftp.gnu.org/gnu/mpfr/mpfr-$VERSION.tar.gz#116715552bd966c85b417c424db1bbdf639f53836eb361549d1f8d6ded5cb4c6"
DEPENDENCIES=('gmp')
CONFIGURE_OPTIONS=(
	"--target=$BANAN_TOOLCHAIN_TRIPLE"
	'--with-sysroot=/'
)
