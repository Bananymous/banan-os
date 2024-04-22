#!/bin/bash
set -e

BINUTILS_VERSION="binutils-2.39"
BINUTILS_TAR="$BINUTILS_VERSION.tar.gz"
BINUTILS_URL="https://ftp.gnu.org/gnu/binutils/$BINUTILS_TAR"

GCC_VERSION="gcc-12.2.0"
GCC_TAR="$GCC_VERSION.tar.gz"
GCC_URL="https://ftp.gnu.org/gnu/gcc/$GCC_VERSION/$GCC_TAR"

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

if [[ -z $BANAN_SCRIPT_DIR ]]; then
	echo "You must set the BANAN_SCRIPT_DIR environment variable" >&2
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

if [[ -z ${MAKE_JOBS:x} ]]; then
	MAKE_JOBS="-j$(nproc)"
fi

if [ $BANAN_ARCH = "x86_64" ]; then
	XCFLAGS="-g -O2 -mcmodel=large -mno-red-zone"
else
	XCFLAGS="-g -O2"
fi

enter_clean_build () {
	rm -rf build-$BANAN_ARCH
	mkdir build-$BANAN_ARCH
	cd build-$BANAN_ARCH
}

build_binutils () {
	echo "Building ${BINUTILS_VERSION}"

	cd $BANAN_BUILD_DIR/toolchain

	if [ ! -d $BINUTILS_VERSION ]; then
		wget $BINUTILS_URL
		tar xf $BINUTILS_TAR
		cd $BINUTILS_VERSION
		patch -s -p1 < $BANAN_TOOLCHAIN_DIR/$BINUTILS_VERSION.patch
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

	make $MAKE_JOBS
	make install
}

build_gcc () {
	echo "Building ${GCC_VERSION}"

	cd $BANAN_BUILD_DIR/toolchain

	if [ ! -d $GCC_VERSION ]; then
		wget $GCC_URL
		tar xf $GCC_TAR
		cd $GCC_VERSION
		patch -s -p1 < $BANAN_TOOLCHAIN_DIR/$GCC_VERSION.patch
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

	make $MAKE_JOBS all-gcc
	make $MAKE_JOBS all-target-libgcc CFLAGS_FOR_TARGET="$XCFLAGS"
	make install-gcc
	make install-target-libgcc
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

	make $MAKE_JOBS
	make install
}

build_libstdcpp () {
	if ! [[ -d $BANAN_BUILD_DIR/toolchain/$GCC_VERSION/build-$BANAN_ARCH ]]; then
		echo "You have to build gcc first"
		exit 1
	fi

	cd $BANAN_BUILD_DIR/toolchain/$GCC_VERSION/build-$BANAN_ARCH
	make $MAKE_JOBS all-target-libstdc++-v3 CFLAGS_FOR_TARGET="$XCFLAGS"
	make install-target-libstdc++-v3
}

# delete everything but toolchain
mkdir -p $BANAN_BUILD_DIR
find $BANAN_BUILD_DIR -mindepth 1 -maxdepth 1 ! -name toolchain -exec rm -r {} +

# NOTE: we have to manually create initial sysroot with libc headers
#       since cmake cannot be invoked yet
echo "Creating dummy sysroot"
mkdir -p $BANAN_SYSROOT/usr
cp -r $BANAN_ROOT_DIR/libc/include $BANAN_SYSROOT/usr/include

mkdir -p $BANAN_BUILD_DIR/toolchain
build_binutils
build_gcc

# Grub is only needed for UEFI (x86_64)
if [ $BANAN_ARCH = "x86_64" ]; then
	build_grub
fi

# delete sysroot and install libc
rm -r $BANAN_SYSROOT
$BANAN_SCRIPT_DIR/build.sh libc-install

build_libstdcpp
