#!/bin/bash ../install.sh

NAME='gcc'
VERSION='12.2.0'
DOWNLOAD_URL="https://ftp.gnu.org/gnu/gcc/gcc-$VERSION/gcc-$VERSION.tar.gz#ac6b317eb4d25444d87cf29c0d141dedc1323a1833ec9995211b13e1a851261c"
DEPENDENCIES=('binutils' 'gmp' 'mpfr' 'mpc')
CONFIGURE_OPTIONS=(
	"--target=$BANAN_TOOLCHAIN_TRIPLE"
	'--with-sysroot=/'
	"--with-build-sysroot=$BANAN_SYSROOT"
	'--enable-initfini-array'
	'--enable-shared'
	'--enable-lto'
	'--disable-nls'
	'--enable-languages=c,c++'
)

build() {
	make -j$(nproc) all-gcc || exit 1
	make -j$(nproc) all-target-libgcc || exit 1
	make -j$(nproc) all-target-libstdc++-v3 || exit 1
	find . -type f -executable -exec strip --strip-unneeded {} + 2>/dev/null
}

install() {
	make install-gcc DESTDIR="$BANAN_SYSROOT" || exit 1
	make install-target-libgcc DESTDIR="$BANAN_SYSROOT" || exit 1
	make install-target-libstdc++-v3 DESTDIR="$BANAN_SYSROOT" || exit 1
}
