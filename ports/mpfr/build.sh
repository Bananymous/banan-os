#!/bin/bash ../install.sh

NAME='mpfr'
VERSION='4.2.2'
DOWNLOAD_URL="https://ftpmirror.gnu.org/gnu/mpfr/mpfr-$VERSION.tar.gz#826cbb24610bd193f36fde172233fb8c009f3f5c2ad99f644d0dea2e16a20e42"
DEPENDENCIES=('gmp')
CONFIG_SUB=('config.sub')
CONFIGURE_OPTIONS=(
	"--target=$BANAN_TOOLCHAIN_TRIPLE"
	'--with-sysroot=/'
)

post_install() {
	# remove libtool file
	rm -f $BANAN_SYSROOT/usr/lib/libmpfr.la
}
