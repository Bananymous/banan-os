#!/bin/bash
set -e

BINUTILS_VERSION="binutils-2.39"
BINUTILS_GIT="https://sourceware.org/git/binutils-gdb.git"
BINUTILS_BRANCH="binutils-2_39"

GCC_VERSION="gcc-12.2.0"
GCC_GIT="https://gcc.gnu.org/git/gcc.git"
GCC_BRANCH="releases/$GCC_VERSION"

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

	if [ ! -d $BINUTILS_VERSION ]; then
		git clone --single-branch --branch $BINUTILS_BRANCH $BINUTILS_GIT $BINUTILS_VERSION
		cd $BINUTILS_VERSION
		git am $BANAN_TOOLCHAIN_DIR/$BINUTILS_VERSION.patch
	fi

	cd $BANAN_BUILD_DIR/toolchain/$BINUTILS_VERSION
	enter_clean_build

	../configure \
		--target="$BANAN_TOOLCHAIN_TRIPLE_PREFIX" \
		--prefix="$BANAN_TOOLCHAIN_PREFIX" \
		--with-sysroot="$BANAN_SYSROOT" \
		--disable-initfini-array \
		--disable-nls \
		--disable-werror

	make
	make install
}

build_gcc () {
	echo "Building ${GCC_VERSION}"

	cd $BANAN_BUILD_DIR/toolchain

	if [ ! -d $GCC_VERSION ]; then
		git clone --single-branch --branch $GCC_BRANCH $GCC_GIT $GCC_VERSION
		cd $GCC_VERSION
		git am $BANAN_TOOLCHAIN_DIR/$GCC_VERSION.patch
	fi

	cd $BANAN_BUILD_DIR/toolchain/$GCC_VERSION
	enter_clean_build

	../configure \
		--target="$BANAN_TOOLCHAIN_TRIPLE_PREFIX" \
		--prefix="$BANAN_TOOLCHAIN_PREFIX" \
		--with-sysroot="$BANAN_SYSROOT" \
		--disable-initfini-array \
		--disable-nls \
		--enable-languages=c,c++

	make all-gcc
	make all-target-libgcc CFLAGS_FOR_TARGET='-g -O2 -mcmodel=large -mno-red-zone'
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
	
	make
	make install
}

build_libstdcpp () {
	if ! [[ -d $BANAN_BUILD_DIR/toolchain/$GCC_VERSION/build ]]; then
		echo "You have to build gcc first"
		exit 1
	fi

	cd $BANAN_BUILD_DIR/toolchain/$GCC_VERSION/build
	make all-target-libstdc++-v3
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
echo "Creating dummy sysroot"
rm -rf $BANAN_SYSROOT
mkdir -p $BANAN_SYSROOT/usr
cp -r $BANAN_ROOT_DIR/libc/include $BANAN_SYSROOT/usr/include


# Cleanup all old files from toolchain prefix
rm -rf $BANAN_TOOLCHAIN_PREFIX

if [[ -z ${MAKEFLAGS:x} ]]; then
	export MAKEFLAGS="-j$(proc)"
fi

mkdir -p $BANAN_BUILD_DIR/toolchain
build_binutils
build_gcc
build_grub

rm -rf $BANAN_SYSROOT
