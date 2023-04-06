#!/bin/sh
set -e

BINUTILS_VERSION="binutils-2.39"
GCC_VERSION="gcc-12.2.0"

cd $(dirname "$0")

if [[ -z $SYSROOT ]]; then
	echo "You must set the SYSROOT environment variable" >&2
	exit 1
fi

if [[ -z $PREFIX ]]; then
	echo "You must set the PREFIX environment variable" >&2
	exit 1
fi

if [[ -z $ARCH ]]; then
	echo  "You must set the ARCH environment variable" >&2
	exit 1
fi

TARGET="${ARCH}-banan_os"

if [ ! -f ${PREFIX}/bin/${TARGET}-ld ]; then

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
		--prefix="$PREFIX" \
		--with-sysroot="$SYSROOT" \
		--disable-werror

	make -j $(nproc)
	make install

	popd

fi

if [ ! -f ${PREFIX}/bin/${TARGET}-g++ ]; then

	echo "Building ${GCC_VERSION}"

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
		--target="$TARGET" \
		--prefix="$PREFIX" \
		--with-sysroot="$SYSROOT" \
		--enable-languages=c,c++

	make -j $(nproc) all-gcc all-target-libgcc
	make install-gcc install-target-libgcc

fi
