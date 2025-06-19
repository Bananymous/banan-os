#!/bin/bash
set -e

BINUTILS_VERSION="binutils-2.44"
BINUTILS_TAR="$BINUTILS_VERSION.tar.gz"
BINUTILS_URL="https://ftp.gnu.org/gnu/binutils/$BINUTILS_TAR"

GCC_VERSION="gcc-15.1.0"
GCC_TAR="$GCC_VERSION.tar.gz"
GCC_URL="https://ftp.gnu.org/gnu/gcc/$GCC_VERSION/$GCC_TAR"

GRUB_VERSION="grub-2.06"
GRUB_TAR="$GRUB_VERSION.tar.xz"
GRUB_URL="https://ftp.gnu.org/gnu/grub/$GRUB_TAR"

CMAKE_VERSION="cmake-3.26.6-linux-x86_64"
CMAKE_TAR="$CMAKE_VERSION.tar.gz"
CMAKE_URL="https://cmake.org/files/v3.26/$CMAKE_TAR"

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

if [[ -z $BANAN_TOOLCHAIN_TRIPLE ]]; then
	echo "You must set the BANAN_TOOLCHAIN_TRIPLE environment variable" >&2
	exit 1
fi

if [[ -z $BANAN_ARCH ]]; then
	echo  "You must set the BANAN_ARCH environment variable" >&2
	exit 1
fi

if [[ -z ${MAKE_JOBS:x} ]]; then
	MAKE_JOBS="-j$(nproc)"
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
		if [ ! -f $BINUTILS_TAR ]; then
			wget $BINUTILS_URL
		fi
		tar xf $BINUTILS_TAR
		patch -ruN -p1 -d "$BINUTILS_VERSION" < "$BANAN_TOOLCHAIN_DIR/$BINUTILS_VERSION.patch"
	fi

	cd $BANAN_BUILD_DIR/toolchain/$BINUTILS_VERSION
	enter_clean_build

	../configure \
		--target="$BANAN_TOOLCHAIN_TRIPLE" \
		--prefix="$BANAN_TOOLCHAIN_PREFIX" \
		--with-sysroot="$BANAN_SYSROOT" \
		--enable-initfini-array \
		--enable-shared \
		--enable-lto \
		--disable-nls \
		--disable-werror

	make $MAKE_JOBS
	make install
}

build_gcc () {
	echo "Building ${GCC_VERSION}"

	cd $BANAN_BUILD_DIR/toolchain

	if [ ! -d $GCC_VERSION ]; then
		if [ ! -f $GCC_TAR ]; then
			wget $GCC_URL
		fi
		tar xf $GCC_TAR
		patch -ruN -p1 -d "$GCC_VERSION" < "$BANAN_TOOLCHAIN_DIR/$GCC_VERSION.patch"
	fi

	cd $BANAN_BUILD_DIR/toolchain/$GCC_VERSION
	enter_clean_build

	../configure \
		--target="$BANAN_TOOLCHAIN_TRIPLE" \
		--prefix="$BANAN_TOOLCHAIN_PREFIX" \
		--with-sysroot="$BANAN_SYSROOT" \
		--enable-initfini-array \
		--enable-threads=posix \
		--enable-shared \
		--enable-lto \
		--disable-nls \
		--enable-languages=c,c++

	XCFLAGS=""
	if [ $BANAN_ARCH = "x86_64" ]; then
		XCFLAGS="-mcmodel=large -mno-red-zone"
	fi

	make $MAKE_JOBS all-gcc
	make $MAKE_JOBS all-target-libgcc CFLAGS_FOR_TARGET="$XCFLAGS"
	make install-gcc
	make install-target-libgcc
}

build_libstdcpp () {
	if ! [[ -d $BANAN_BUILD_DIR/toolchain/$GCC_VERSION/build-$BANAN_ARCH ]]; then
		echo "You have to build gcc first"
		exit 1
	fi

	cd $BANAN_BUILD_DIR/toolchain/$GCC_VERSION/build-$BANAN_ARCH
	make $MAKE_JOBS all-target-libstdc++-v3
	make install-target-libstdc++-v3
}

build_grub () {
	echo "Building ${GRUB_VERSION}"

	cd $BANAN_BUILD_DIR/toolchain

	if [ ! -f $GRUB_TAR ]; then
		wget $GRUB_URL
	fi

	if [ ! -d $GRUB_VERSION ]; then
		tar xvf $GRUB_TAR
	fi

	cd $GRUB_VERSION
	enter_clean_build

	../configure \
		--target="$BANAN_ARCH" \
		--prefix="$BANAN_TOOLCHAIN_PREFIX" \
		--with-platform="efi" \
		--disable-werror \
		CFLAGS=--std=gnu11

	make $MAKE_JOBS
	make install
}

build_cmake() {
	echo "Downloading ${CMAKE_VERSION}"

	cd $BANAN_BUILD_DIR/toolchain

	if [ ! -f $CMAKE_TAR ]; then
		wget $CMAKE_URL
	fi

	if [ ! -d $CMAKE_VERSION ]; then
		tar xvf $CMAKE_TAR
	fi

	cd $CMAKE_VERSION

	mkdir -p $BANAN_TOOLCHAIN_PREFIX/bin
	mkdir -p $BANAN_TOOLCHAIN_PREFIX/share

	cp -r ./bin/* $BANAN_TOOLCHAIN_PREFIX/bin/
	cp -r ./share/* $BANAN_TOOLCHAIN_PREFIX/share/
}

BUILD_BINUTILS=1
if [[ -f $BANAN_TOOLCHAIN_PREFIX/bin/$BANAN_TOOLCHAIN_TRIPLE-ld ]]; then
	echo "You already seem to have a binutils installed."
	read -e -p "Do you want to rebuild it [y/N]? " choice
	if ! [[ "$choice" == [Yy]* ]]; then
		BUILD_BINUTILS=0
	fi
fi

BUILD_GCC=1
if [[ -f $BANAN_TOOLCHAIN_PREFIX/bin/$BANAN_TOOLCHAIN_TRIPLE-gcc ]]; then
	echo "You already seem to have a gcc installed."
	read -e -p "Do you want to rebuild it [y/N]? " choice
	if ! [[ "$choice" == [Yy]* ]]; then
		BUILD_GCC=0
	fi
fi

BUILD_LIBSTDCPP=$BUILD_GCC
if ! (($BUILD_LIBSTDCPP)); then
	read -e -p "Do you want to rebuild libstdc++ [y/N]? " choice
	if [[ "$choice" == [Yy]* ]]; then
		BUILD_LIBSTDCPP=1
	fi
fi

BUILD_GRUB=1
if [[ -f $BANAN_TOOLCHAIN_PREFIX/bin/grub-mkstandalone ]]; then
	echo "You already seem to have a grub installed."
	read -e -p "Do you want to rebuild it [y/N]? " choice
	if ! [[ "$choice" == [Yy]* ]]; then
		BUILD_GRUB=0
	fi
fi

BUILD_CMAKE=1
if [[ -f $BANAN_TOOLCHAIN_PREFIX/bin/cmake ]]; then
	echo "You already seem to have a cmake installed."
	read -e -p "Do you want to rebuild it [y/N]? " choice
	if ! [[ "$choice" == [Yy]* ]]; then
		BUILD_CMAKE=0
	fi
fi

# delete everything but toolchain
mkdir -p $BANAN_BUILD_DIR
find $BANAN_BUILD_DIR -mindepth 1 -maxdepth 1 ! -name toolchain -exec rm -r {} +

# NOTE: we have to manually create initial sysroot with libc headers
#       since cmake cannot be invoked yet
mkdir -p $BANAN_SYSROOT/usr
cp -r $BANAN_ROOT_DIR/userspace/libraries/LibC/include $BANAN_SYSROOT/usr/include

mkdir -p $BANAN_BUILD_DIR/toolchain

if (($BUILD_BINUTILS)); then
	build_binutils
fi

if (($BUILD_GCC)); then
	build_gcc
fi

if (($BUILD_GRUB)); then
	build_grub
fi

if (($BUILD_CMAKE)); then
	build_cmake
fi

if (($BUILD_LIBSTDCPP)); then
	# delete sysroot and install libc
	rm -r $BANAN_SYSROOT
	$BANAN_SCRIPT_DIR/build.sh libc-static
	$BANAN_SCRIPT_DIR/build.sh libc-shared
	$BANAN_SCRIPT_DIR/build.sh install

	build_libstdcpp
fi

find "$BANAN_TOOLCHAIN_PREFIX" -type f -executable -exec strip --strip-unneeded {} + 2>/dev/null
