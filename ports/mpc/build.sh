#!/bin/bash ../install.sh

NAME='mpc'
VERSION='1.3.1'
DOWNLOAD_URL="https://ftp.gnu.org/gnu/mpc/mpc-$VERSION.tar.gz#ab642492f5cf882b74aa0cb730cd410a81edcdbec895183ce930e706c1c759b8"
DEPENDENCIES=('gmp' 'mpfr')
CONFIGURE_OPTIONS=(
	"--target=$BANAN_TOOLCHAIN_TRIPLE"
	"--with-sysroot=$BANAN_SYSROOT"
)
