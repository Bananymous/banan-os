#!/bin/bash
set -e

BINUTILS_VERSION="binutils-2.39"
GCC_VERSION="gcc-12.2.0"
GRUB_VERSION="grub-2.06"

cd $(dirname "$0")

if [[ -n $LIBSTDCPP ]]; then
	cd build/${GCC_VERSION}/
	make -j $(nproc) all-target-libstdc++-v3
	make install-target-libstdc++-v3
	exit 0
fi

if [[ -z $SYSROOT ]]; then
	echo "You must set the SYSROOT environment variable" >&2
	exit 1
fi

if [[ -z $TOOLCHAIN_PREFIX ]]; then
	echo "You must set the TOOLCHAIN_PREFIX environment variable" >&2
	exit 1
fi

if [[ -z $BANAN_ARCH ]]; then
	echo  "You must set the BANAN_ARCH environment variable" >&2
	exit 1
fi

TARGET="${BANAN_ARCH}-banan_os"

if [ ! -f ${TOOLCHAIN_PREFIX}/bin/${TARGET}-ld ]; then

	echo "Building ${BINUTILS_VERSION}"

	if [ ! -f ${BINUTILS_VERSION}.tar.xz ]; then
		wget https://ftp.gnu.org/gnu/binutils/${BINUTILS_VERSION}.tar.xz
	fi

	if [ ! -d $BINUTILS_VERSION ]; then
		tar xvf ${BINUTILS_VERSION}.tar.xz
		patch -s -p0 < ${BINUTILS_VERSION}.patch
	fi

	mkdir -p build/${BINUTILS_VERSION}/
	pushd build/${BINUTILS_VERSION}/

	../../${BINUTILS_VERSION}/configure \
		--target="$TARGET" \
		--prefix="$TOOLCHAIN_PREFIX" \
		--with-sysroot="$SYSROOT" \
		--disable-nls \
		--disable-werror

	make -j $(nproc)
	make install

	popd

fi

if [ ! -f ${TOOLCHAIN_PREFIX}/bin/${TARGET}-g++ ]; then

	echo "Building ${GCC_VERSION}"

	if [ ! -f ${GCC_VERSION}.tar.xz ]; then
		wget https://ftp.gnu.org/gnu/gcc/${GCC_VERSION}/${GCC_VERSION}.tar.xz
	fi

	if [ ! -d $GCC_VERSION ]; then
		tar xvf ${GCC_VERSION}.tar.xz
		patch -s -p0 < ${GCC_VERSION}.patch
	fi

	mkdir -p build/${GCC_VERSION}/
	pushd build/${GCC_VERSION}/

	../../${GCC_VERSION}/configure \
		--target="$TARGET" \
		--prefix="$TOOLCHAIN_PREFIX" \
		--with-sysroot="$SYSROOT" \
		--disable-nls \
		--enable-languages=c,c++

	make -j $(nproc) all-gcc 
	make -j $(nproc) all-target-libgcc CFLAGS_FOR_TARGET='-g -O2 -mcmodel=large -mno-red-zone'
	make install-gcc install-target-libgcc

	popd

fi

if [ ! -f ${TOOLCHAIN_PREFIX}/bin/grub-mkstandalone ]; then

	echo "Building ${GRUB_VERSION}"

	if [ ! -f ${GRUB_VERSION}.tar.xz ]; then
		wget https://ftp.gnu.org/gnu/grub/${GRUB_VERSION}.tar.xz
	fi

	if [ ! -d $GRUB_VERSION ]; then
		tar xvf ${GRUB_VERSION}.tar.xz
	fi

	mkdir -p build/${GRUB_VERSION}/
	pushd build/${GRUB_VERSION}/

	../../${GRUB_VERSION}/configure \
		--target="$BANAN_ARCH" \
		--prefix="$TOOLCHAIN_PREFIX" \
		--with-platform="efi" \
		--disable-werror
	
	make -j $(nproc)
	make install

	popd

fi
