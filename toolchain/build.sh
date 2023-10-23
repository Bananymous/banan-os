#!/bin/bash
set -e

BINUTILS_VERSION="binutils-2.39"
GCC_VERSION="gcc-12.2.0"
GRUB_VERSION="grub-2.06"

if [[ -z $BANAN_SYSROOT ]]; then
	echo "You must set the BANAN_SYSROOT environment variable" >&2
	exit 1
fi

if [[ -z $BANAN_TOOLCHAIN_DIR ]]; then
	echo "You must set the BANAN_TOOLCHAIN_DIR environment variable" >&2
	exit 1
fi

if [[ -z $BANAN_TOOLCHAIN_PREFIX ]]; then
	echo "You must set the BANAN_TOOLCHAIN_PREFIX environment variable" >&2
	exit 1
fi

if [[ -z $BANAN_TOOLCHAIN_TRIPLE_PREFIX ]]; then
	echo "You must set the BANAN_TOOLCHAIN_TRIPLE_PREFIX environment variable" >&2
	exit 1
fi

if [[ -z $BANAN_ARCH ]]; then
	echo  "You must set the BANAN_ARCH environment variable" >&2
	exit 1
fi

build_binutils () {
	echo "Building ${BINUTILS_VERSION}"

	cd $BANAN_TOOLCHAIN_DIR

	if [ ! -f ${BINUTILS_VERSION}.tar.xz ]; then
		wget https://ftp.gnu.org/gnu/binutils/${BINUTILS_VERSION}.tar.xz
	fi

	if [ ! -d $BINUTILS_VERSION ]; then
		tar xvf ${BINUTILS_VERSION}.tar.xz
		patch -s -p0 < ${BINUTILS_VERSION}.patch
	fi

	mkdir -p build/${BINUTILS_VERSION}/
	cd build/${BINUTILS_VERSION}/

	../../${BINUTILS_VERSION}/configure \
		--target="$BANAN_TOOLCHAIN_TRIPLE_PREFIX" \
		--prefix="$BANAN_TOOLCHAIN_PREFIX" \
		--with-sysroot="$BANAN_SYSROOT" \
		--disable-nls \
		--disable-werror

	make -j $(nproc)
	make install
}

build_gcc () {
	echo "Building ${GCC_VERSION}"

	cd $BANAN_TOOLCHAIN_DIR

	if [ ! -f ${GCC_VERSION}.tar.xz ]; then
		wget https://ftp.gnu.org/gnu/gcc/${GCC_VERSION}/${GCC_VERSION}.tar.xz
	fi

	if [ ! -d $GCC_VERSION ]; then
		tar xvf ${GCC_VERSION}.tar.xz
		patch -s -p0 < ${GCC_VERSION}.patch
	fi

	mkdir -p build/${GCC_VERSION}/
	cd build/${GCC_VERSION}/

	../../${GCC_VERSION}/configure \
		--target="$BANAN_TOOLCHAIN_TRIPLE_PREFIX" \
		--prefix="$BANAN_TOOLCHAIN_PREFIX" \
		--with-sysroot="$BANAN_SYSROOT" \
		--disable-nls \
		--enable-languages=c,c++

	make -j $(nproc) all-gcc 
	make -j $(nproc) all-target-libgcc CFLAGS_FOR_TARGET='-g -O2 -mcmodel=large -mno-red-zone'
	make install-gcc install-target-libgcc
}

build_grub () {
	echo "Building ${GRUB_VERSION}"

	cd $BANAN_TOOLCHAIN_DIR

	if [ ! -f ${GRUB_VERSION}.tar.xz ]; then
		wget https://ftp.gnu.org/gnu/grub/${GRUB_VERSION}.tar.xz
	fi

	if [ ! -d $GRUB_VERSION ]; then
		tar xvf ${GRUB_VERSION}.tar.xz
	fi

	mkdir -p build/${GRUB_VERSION}/
	cd build/${GRUB_VERSION}/

	../../${GRUB_VERSION}/configure \
		--target="$BANAN_ARCH" \
		--prefix="$BANAN_TOOLCHAIN_PREFIX" \
		--with-platform="efi" \
		--disable-werror
	
	make -j $(nproc)
	make install
}

build_libstdcpp () {
	cd build/${GCC_VERSION}/
	make -j $(nproc) all-target-libstdc++-v3
	make install-target-libstdc++-v3
}

if [[ "$1" == "libstdc++" ]]; then
	build_libstdcpp
	exit 0
fi

build_binutils
build_gcc
build_grub
