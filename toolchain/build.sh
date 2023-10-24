#!/bin/bash
set -e

BINUTILS_VERSION="binutils-2.39"
GCC_VERSION="gcc-12.2.0"
GRUB_VERSION="grub-2.06"

if [[ -z $BANAN_SYSROOT ]]; then
	echo "You must set the BANAN_SYSROOT environment variable" >&2
	exit 1
fi

if [[ -z $BANAN_ROOT_DIR ]]; then
	echo "You must set the BANAN_ROOT_DIR environment variable" >&2
	exit 1
fi

if [[ -z $BANAN_BUILD_DIR ]]; then
	echo "You must set the BANAN_BUILD_DIR environment variable" >&2
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

enter_clean_build () {
	rm -rf build
	mkdir build
	cd build
}

build_binutils () {
	echo "Building ${BINUTILS_VERSION}"

	cd $BANAN_BUILD_DIR/toolchain

	if [ ! -f ${BINUTILS_VERSION}.tar.xz ]; then
		wget https://ftp.gnu.org/gnu/binutils/${BINUTILS_VERSION}.tar.xz
	fi

	if [ ! -d $BINUTILS_VERSION ]; then
		tar xvf ${BINUTILS_VERSION}.tar.xz
		patch -s -p0 < $BANAN_TOOLCHAIN_DIR/${BINUTILS_VERSION}.patch
	fi

	cd $BINUTILS_VERSION
	enter_clean_build

	../configure \
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

	cd $BANAN_BUILD_DIR/toolchain

	if [ ! -f ${GCC_VERSION}.tar.xz ]; then
		wget https://ftp.gnu.org/gnu/gcc/${GCC_VERSION}/${GCC_VERSION}.tar.xz
	fi

	if [ ! -d $GCC_VERSION ]; then
		tar xvf ${GCC_VERSION}.tar.xz
		patch -s -p0 < $BANAN_TOOLCHAIN_DIR/${GCC_VERSION}.patch
	fi

	cd ${GCC_VERSION}
	enter_clean_build

	../configure \
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

	cd $BANAN_BUILD_DIR/toolchain

	if [ ! -f ${GRUB_VERSION}.tar.xz ]; then
		wget https://ftp.gnu.org/gnu/grub/${GRUB_VERSION}.tar.xz
	fi

	if [ ! -d $GRUB_VERSION ]; then
		tar xvf ${GRUB_VERSION}.tar.xz
	fi

	cd $GRUB_VERSION
	enter_clean_build

	../configure \
		--target="$BANAN_ARCH" \
		--prefix="$BANAN_TOOLCHAIN_PREFIX" \
		--with-platform="efi" \
		--disable-werror
	
	make -j $(nproc)
	make install
}

build_libstdcpp () {
	if ! [[ -d $BANAN_BUILD_DIR/toolchain/$GCC_VERSION/build ]]; then
		echo "You have to build gcc first"
		exit 1
	fi

	cd $BANAN_BUILD_DIR/toolchain/$GCC_VERSION/build
	make -j $(nproc) all-target-libstdc++-v3
	make install-target-libstdc++-v3
}

if [[ $# -ge 1 ]]; then
	if [[ "$1" == "libstdc++" ]]; then
		build_libstdcpp
		exit 0
	fi

	echo "unrecognized arguments $@"
	exit 1
fi

# NOTE: we have to manually create initial sysroot with libc headers
#       since cmake cannot be invoked yet
echo "Syncing sysroot headers"
mkdir -p $BANAN_SYSROOT
sudo mkdir -p $BANAN_SYSROOT/usr/include
sudo rsync -a $BANAN_ROOT_DIR/libc/include/ $BANAN_SYSROOT/usr/include/

mkdir -p $BANAN_BUILD_DIR/toolchain

build_binutils
build_gcc
build_grub
