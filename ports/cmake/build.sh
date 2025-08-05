#!/bin/bash ../install.sh

NAME='cmake'
VERSION='3.26.6' # NOTE: same version as used by our toolchain
DOWNLOAD_URL="https://github.com/Kitware/CMake/releases/download/v$VERSION/cmake-$VERSION.tar.gz#070b9a2422e666d2c1437e2dab239a236e8a63622d0a8d0ffe9e389613d2b76a"
DEPENDENCIES=('openssl' 'libuv' 'make' 'bash')

configure() {
	$BANAN_CMAKE \
		--toolchain="$BANAN_TOOLCHAIN_DIR/Toolchain.txt" \
		-B build -GNinja --fresh \
		-DCMAKE_INSTALL_PREFIX=/usr \
		-DCMAKE_USE_OPENSSL=ON \
		-DCMAKE_USE_SYSTEM_LIBUV=ON \
		-DOPENSSL_ROOT_DIR=/usr \
		-DBUILD_TESTING=OFF \
		. || exit 1
}

build() {
	$BANAN_CMAKE --build build || exit 1
}

install() {
	$BANAN_CMAKE --install build || exit 1
	cp $BANAN_TOOLCHAIN_DIR/cmake-platform/* $BANAN_SYSROOT/usr/share/cmake-3.26/Modules/Platform/
}
