#!/bin/bash ../install.sh

NAME='gcc'
VERSION='15.2.0'
DOWNLOAD_URL="https://ftpmirror.gnu.org/gnu/gcc/gcc-$VERSION/gcc-$VERSION.tar.xz#438fd996826b0c82485a29da03a72d71d6e3541a83ec702df4271f6fe025d24e"
DEPENDENCIES=('binutils' 'gmp' 'mpfr' 'mpc')
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

build() {
	xcflags=""
	if [ $BANAN_ARCH = "x86_64" ]; then
		xcflags="-mcmodel=large -mno-red-zone"
	fi

	make -j$(nproc) all-gcc || exit 1
	make -j$(nproc) all-target-libgcc CFLAGS_FOR_TARGET="$xcflags" || exit 1
	make -j$(nproc) all-target-libstdc++-v3 || exit 1
}
