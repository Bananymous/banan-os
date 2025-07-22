#!/bin/bash ../install.sh

NAME='gcc'
VERSION='15.1.0'
DOWNLOAD_URL="https://ftp.gnu.org/gnu/gcc/gcc-$VERSION/gcc-$VERSION.tar.gz#51b9919ea69c980d7a381db95d4be27edf73b21254eb13d752a08003b4d013b1"
DEPENDENCIES=('binutils' 'gmp' 'mpfr' 'mpc')
MAKE_BUILD_TARGETS=('all-gcc' 'all-target-libgcc' 'all-target-libstdc++-v3')
MAKE_INSTALL_TARGETS=('install-strip-gcc' 'install-strip-target-libgcc' 'install-strip-target-libstdc++-v3')
CONFIGURE_OPTIONS=(
	"--target=$BANAN_TOOLCHAIN_TRIPLE"
	'--with-sysroot=/'
	"--with-build-sysroot=$BANAN_SYSROOT"
	'--enable-initfini-array'
	'--enable-threads=posix'
	'--enable-shared'
	'--enable-lto'
	'--disable-nls'
	'--enable-languages=c,c++'
)

post_install() {
	# remove libtool files
	rm -f $BANAN_SYSROOT/usr/lib/libstdc++.la
	rm -f $BANAN_SYSROOT/usr/lib/libstdc++exp.la
	rm -f $BANAN_SYSROOT/usr/lib/libsupc++.la
}
